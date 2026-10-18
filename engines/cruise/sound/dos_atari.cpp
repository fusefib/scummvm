/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "audio/mixer.h"
#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "cruise/cruise.h"
#include "cruise/cruise_main.h"
#include "cruise/sound.h"
#include "cruise/sound/atari_players.h"
#include "cruise/sound/dos_driver.h"
#include "cruise/sound/resources.h"

namespace Cruise {

namespace {

class DosAtariSound : public PCSound {
public:
	DosAtariSound(Audio::Mixer *mixer, CruiseEngine *vm);
	~DosAtariSound() override;

	void loadMusic(const char *name) override;
	void playMusic() override;
	void stopMusic() override;
	void removeMusic() override;
	void fadeOutMusic() override { _music->fade(); }
	void fadeSong() override { _music->fade(); }
	bool songLoaded() const override { return _music->loaded(); }
	bool songPlayed() const override { return _music->started(); }
	const char *musicName() override { return _music->name(); }
	bool canReuseMusic(const char *name) override { return _music->canReuse(name); }
	uint8 numOrders() const override { return _music->moduleOrderCount(); }
	void setNumOrders(uint8 count) override { _music->setModuleOrderCount(count); }
	void setPattern(int offset, uint8 value) override { _music->setPattern(offset, value); }
	int16 musicLooping() const override { return _music->looping(); }
	void musicLoop(int16 value) override { _music->setLooping(value); }
	void prepareLoad() override;
	void doSync(Common::Serializer &s) override;
	void finalizeLoad() override;
	void pause(bool paused) override;
	void service() override { _music->service(); }
	void retireMusic() override { _music->retire(); }
	void toggleSound() override;
	void syncSounds() override;

private:
	void syncMusicVolume();

	Audio::Mixer *_mixer;
	CruiseEngine *_vm;
	AtariPsgStream *_music;
	Audio::SoundHandle _handle;
	bool _restartAfterLoad;
};

DosAtariSound::DosAtariSound(Audio::Mixer *mixer, CruiseEngine *vm)
	: PCSound(mixer, vm), _mixer(mixer), _vm(vm), _restartAfterLoad(false) {
	_music = new AtariPsgStream(mixer->getOutputRate(), readBundleSoundFile,
			MemoryFree);
	// The stream applies music gain itself; the DOS driver owns effects gain.
	_mixer->playStream(Audio::Mixer::kPlainSoundType, &_handle, _music, -1,
			Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);
	syncSounds();
}

DosAtariSound::~DosAtariSound() {
	_mixer->stopHandle(_handle);
	delete _music;
}

void DosAtariSound::loadMusic(const char *name) {
	char song[33];
	Common::strlcpy(song, name, sizeof(song));
	if (_music->started()) {
		_music->fade();
		int lastFade = _music->fadeLevel();
		uint32 lastProgress = _vm->presentationMillis();
		while (_music->started() && !_vm->soundWaitCancelled()) {
			if (!_vm->waitForSound(50))
				break;
			const int fade = _music->fadeLevel();
			const uint32 now = _vm->presentationMillis();
			if (fade != lastFade) {
				lastFade = fade;
				lastProgress = now;
			} else if (now - lastProgress >= 2000) {
				int checkedFade = fade;
				const SoundInternal::ReplacementWaitAction action =
						_music->recheckReplacementWait(lastFade,
								now - lastProgress, 2000, checkedFade);
				if (action == SoundInternal::kReplacementWaitContinue) {
					if (checkedFade != lastFade) {
						lastFade = checkedFade;
						lastProgress = now;
					}
					continue;
				}
				if (action == SoundInternal::kReplacementWaitForceStop) {
					warning("Cruise Atari music replacement stalled; stopping the old song");
					_music->stop();
				}
				break;
			}
		}
	}
	stopMusic();
	_music->unload();
	if (!_vm->soundWaitCancelled() && !_music->load(song))
		warning("Unable to load Cruise Atari music '%s'", song);
}

void DosAtariSound::playMusic() {
	_music->setOrderCount(_music->moduleOrderCount());
	_music->play();
}

void DosAtariSound::stopMusic() {
	_restartAfterLoad = false;
	_music->stop();
	// DOS StopSong also cleans up standalone effects, even without music.
	PCSound::stopMusic();
}

void DosAtariSound::removeMusic() {
	if (_music->started())
		stopMusic();
	_restartAfterLoad = false;
	_music->unload();
}

void DosAtariSound::prepareLoad() {
	stopMusic();
	_music->unload();
}

void DosAtariSound::doSync(Common::Serializer &s) {
	SoundInternal::MusicState state;
	_music->getSaveState(state);
	if (s.isLoading())
		prepareLoad();
	state.sync(s);
	s.syncAsSint16LE(_genVolume);
	if (s.isLoading()) {
		if (state.fading())
			state.retire();
		if (state.loaded && (!memchr(state.name, 0, sizeof(state.name)) || !_music->load(state.name))) {
			warning("Unable to restore Cruise Atari music");
			state.retire();
		}
		// Older DOS saves can mark playback active without the started flag.
		_restartAfterLoad = state.loaded && state.playing;
		_music->setLooping(state.looping);
		_music->restoreStoppedPosition(state.row, state.order);
	}
}

void DosAtariSound::finalizeLoad() {
	if (_restartAfterLoad && _music->loaded())
		playMusic();
	_restartAfterLoad = false;
	restoreEffects();
}

void DosAtariSound::pause(bool paused) {
	PCSound::pause(paused);
	_music->pause(paused);
}

void DosAtariSound::syncMusicVolume() {
	const bool muted = _soundDriver->soundDisabled() ||
			(ConfMan.hasKey("mute") && ConfMan.getBool("mute")) ||
			(ConfMan.hasKey("music_mute") && ConfMan.getBool("music_mute"));
	_music->syncVolume(muted ? 0 : CLIP(ConfMan.getInt("music_volume"), 0, 255));
}

void DosAtariSound::toggleSound() {
	PCSound::toggleSound();
	syncMusicVolume();
}

void DosAtariSound::syncSounds() {
	PCSound::syncSounds();
	syncMusicVolume();
}

} // End of anonymous namespace

Sound *createDosAtariSound(Audio::Mixer *mixer, CruiseEngine *vm) {
	return new DosAtariSound(mixer, vm);
}

} // End of namespace Cruise
