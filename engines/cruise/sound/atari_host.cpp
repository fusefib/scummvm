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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "audio/mixer.h"

#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/str.h"
#include "common/system.h"

#include "cruise/cruise.h"
#include "cruise/cruise_main.h"
#include "cruise/sound.h"
#include "cruise/sound/atari_players.h"
#include "cruise/sound/internal.h"
#include "cruise/sound/resources.h"
#include "cruise/vars.h"

namespace Cruise {

namespace {

byte configuredVolume(const char *muteKey, const char *volumeKey) {
	if ((ConfMan.hasKey("mute") && ConfMan.getBool("mute")) ||
			(ConfMan.hasKey(muteKey) && ConfMan.getBool(muteKey)))
		return 0;
	return MIN(255, ConfMan.getInt(volumeKey));
}

} // End of anonymous namespace

namespace {

class AtariSound : public Sound {
public:
	AtariSound(Audio::Mixer *mixer, CruiseEngine *vm, AtariMidiSink *sink);
	~AtariSound() override;

	void loadMusic(const char *name) override;
	void playMusic() override;
	void stopMusic() override;
	void removeMusic() override;
	void fadeOutMusic() override;
	void playEffect(int sample, int channel, int period, int volume) override;
	void loopEffect(int sample, int channel, int period, int volume) override;
	void updateEffect(int sample, int channel, int period, int volume) override;
	void stopEffect(int channel) override;
	void restoreEffects() override;
	void finalizeLoad() override;
	void prepareLoad() override;
	void pause(bool paused) override;
	void service() override;
	void retireMusic() override;
	void doSync(Common::Serializer &s) override;
	const char *musicName() override;
	bool songLoaded() const override;
	bool songPlayed() const override;
	void fadeSong() override;
	int16 trackPosition() const override;
	uint8 numOrders() const override;
	void setNumOrders(uint8 v) override;
	void setPattern(int offset, uint8 value) override;
	int16 musicLooping() const override;
	void musicLoop(int16 v) override;
	void syncSounds() override;
	const char *soundEffectExtension() const override { return ".SPL"; }

private:
	bool backendLoaded() const;
	bool backendStarted() const;
	int backendFadeLevel() const;
	const char *backendName() const;
	void backendLoad(const char *name);
	void backendPlay();
	void backendStop();
	void backendUnload();
	void backendFade();
	void backendRestoreStoppedPosition(int rowOffset, int orderIndex);
	SoundInternal::ReplacementWaitAction backendRecheckReplacementWait(
			int previousFade, uint32 elapsed, uint32 stallTimeout,
			int &currentFade) const;

	Audio::Mixer *_mixer;
	CruiseEngine *_vm;
	AtariPsgStream *_psg;
	AtariMidiPlayer *_midi;
	Audio::SoundHandle _handle;
	bool _restartAfterLoad;
};

AtariSound::AtariSound(Audio::Mixer *mixer, CruiseEngine *vm, AtariMidiSink *sink)
	: _mixer(mixer), _vm(vm), _midi(nullptr), _restartAfterLoad(false) {
	_psg = new AtariPsgStream(_mixer->getOutputRate(), readBundleSoundFile,
			MemoryFree);
	if (sink)
		_midi = new AtariMidiPlayer(sink, readBundleSoundFile, MemoryFree);
	_mixer->playStream(Audio::Mixer::kPlainSoundType, &_handle, _psg, -1,
			Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);
	syncSounds();
}

AtariSound::~AtariSound() {
	if (_midi)
		delete _midi;
	_mixer->stopHandle(_handle);
	delete _psg;
}

bool AtariSound::backendLoaded() const {
	return _midi ? _midi->loaded() : _psg->loaded();
}

bool AtariSound::backendStarted() const {
	return _midi ? _midi->started() : _psg->started();
}

int AtariSound::backendFadeLevel() const {
	return _midi ? _midi->fadeLevel() : _psg->fadeLevel();
}

const char *AtariSound::backendName() const {
	return _midi ? _midi->name() : _psg->name();
}

void AtariSound::backendLoad(const char *name) {
	if (_midi) {
		if (!_midi->load(name))
			warning("Unable to load Atari Cruise MT-32 song '%s'", name);
	} else if (!_psg->load(name)) {
		warning("Unable to load Atari Cruise PSG song '%s'", name);
	}
}

void AtariSound::backendPlay() {
	if (_midi)
		_midi->play();
	else
		_psg->play();
}

void AtariSound::backendStop() {
	if (_midi)
		_midi->stop();
	else
		_psg->stop();
}

void AtariSound::backendUnload() {
	if (_midi)
		_midi->unload();
	else
		_psg->unload();
}

void AtariSound::backendFade() {
	if (_midi)
		_midi->fade();
	else
		_psg->fade();
}

void AtariSound::backendRestoreStoppedPosition(int rowOffset, int orderIndex) {
	if (_midi)
		_midi->restoreStoppedPosition(rowOffset, orderIndex);
	else
		_psg->restoreStoppedPosition(rowOffset, orderIndex);
}

SoundInternal::ReplacementWaitAction AtariSound::backendRecheckReplacementWait(
		int previousFade, uint32 elapsed, uint32 stallTimeout,
		int &currentFade) const {
	return _midi ? _midi->recheckReplacementWait(previousFade, elapsed,
			stallTimeout, currentFade) :
			_psg->recheckReplacementWait(previousFade, elapsed,
					stallTimeout, currentFade);
}

void AtariSound::loadMusic(const char *name) {
	char song[33];
	Common::strlcpy(song, name, sizeof(song));
	if (backendStarted()) {
		backendFade();
		int lastFade = backendFadeLevel();
		uint32 lastProgress = _vm->presentationMillis();
		while (backendStarted() && !_vm->soundWaitCancelled()) {
			if (!_vm->waitForSound(50))
				break;
			const int fade = backendFadeLevel();
			const uint32 now = _vm->presentationMillis();
			if (fade != lastFade) {
				lastFade = fade;
				lastProgress = now;
			} else if (now - lastProgress >= 2000) {
				int checkedFade = fade;
				const SoundInternal::ReplacementWaitAction action =
						backendRecheckReplacementWait(lastFade,
								now - lastProgress, 2000, checkedFade);
				if (action == SoundInternal::kReplacementWaitContinue) {
					if (checkedFade != lastFade) {
						lastFade = checkedFade;
						lastProgress = now;
					}
					continue;
				}
				if (action == SoundInternal::kReplacementWaitForceStop)
					backendStop();
				break;
			}
		}
	}
	if (_vm->soundWaitCancelled()) {
		backendStop();
		backendUnload();
		return;
	}
	backendStop();
	backendUnload();
	backendLoad(song);
}

void AtariSound::playMusic() {
	_psg->stopSample();
	backendPlay();
}

void AtariSound::stopMusic() {
	backendStop();
}

void AtariSound::removeMusic() {
	backendUnload();
}

void AtariSound::fadeOutMusic() {
	backendFade();
}

void AtariSound::playEffect(int sample, int channel, int period, int volume) {
	(void)channel;
	(void)period;
	(void)volume;
	if (sample < 0 || sample >= NUM_FILE_ENTRIES || backendStarted() ||
			!filesDatabase[sample].subData.ptr)
		return;
	_psg->startSample(filesDatabase[sample].subData.ptr,
			getSoundResourceSize(sample), false);
}

void AtariSound::loopEffect(int sample, int channel, int period, int volume) {
	if (sample < 0 || sample >= NUM_FILE_ENTRIES || channel < 0 || channel >= 4 ||
			!filesDatabase[sample].subData.ptr)
		return;
	SoundEntry &effect = soundList[channel];
	effect.frameNum = sample;
	effect.frequency = period;
	effect.volume = volume;
	if (!backendStarted())
		_psg->startSample(filesDatabase[sample].subData.ptr,
				getSoundResourceSize(sample), false);
}

void AtariSound::updateEffect(int sample, int channel, int period, int volume) {
	if (sample < 0 || sample >= NUM_FILE_ENTRIES || channel < 0 || channel >= 4 ||
			!filesDatabase[sample].subData.ptr)
		return;
	SoundEntry &effect = soundList[channel];
	if (period != -1)
		effect.frequency = period;
	if (volume != -1)
		effect.volume = volume;
}

void AtariSound::stopEffect(int channel) {
	if (channel == -1) {
		for (int i = 0; i < 4; ++i)
			soundList[i].frameNum = -1;
	} else if (channel >= 0 && channel < 4) {
		soundList[channel].frameNum = -1;
	} else {
		return;
	}
	_psg->stopSample();
}

void AtariSound::restoreEffects() {
	if (backendStarted())
		return;
	for (int channel = 0; channel < 4; ++channel) {
		const SoundEntry &effect = soundList[channel];
		if (effect.frameNum < 0 || effect.frameNum >= NUM_FILE_ENTRIES ||
				!filesDatabase[effect.frameNum].subData.ptr)
			continue;
		_psg->startSample(filesDatabase[effect.frameNum].subData.ptr,
				getSoundResourceSize(effect.frameNum), false);
	}
}

void AtariSound::finalizeLoad() {
	if (_restartAfterLoad && backendLoaded())
		backendPlay();
	_restartAfterLoad = false;
	restoreEffects();
}

void AtariSound::prepareLoad() {
	_restartAfterLoad = false;
	_psg->stopSample();
	backendStop();
	backendUnload();
}

void AtariSound::pause(bool paused) {
	_psg->pause(paused);
	if (_midi)
		_midi->pause(paused);
}

void AtariSound::service() {
	_psg->service();
}

void AtariSound::retireMusic() {
	if (_midi)
		_midi->retire();
	else
		_psg->retire();
}

void AtariSound::doSync(Common::Serializer &s) {
	SoundInternal::MusicState state;
	if (_midi)
		_midi->getSaveState(state);
	else
		_psg->getSaveState(state);
	if (s.isLoading())
		prepareLoad();
	state.sync(s);
	s.syncAsSint16LE(_genVolume);
	if (s.isLoading()) {
		if (state.fading())
			state.retire();
		if (state.loaded && memchr(state.name, 0, sizeof(state.name)))
			backendLoad(state.name);
		_restartAfterLoad = backendLoaded() && state.started && state.playing;
		musicLoop(state.looping);
		backendRestoreStoppedPosition(state.row, state.order);
	}
}

const char *AtariSound::musicName() {
	return backendName();
}

bool AtariSound::songLoaded() const {
	return backendLoaded();
}

bool AtariSound::songPlayed() const {
	return backendStarted();
}

void AtariSound::fadeSong() {
	backendFade();
}

uint8 AtariSound::numOrders() const {
	return _midi ? _midi->orderCount() : _psg->orderCount();
}

void AtariSound::setNumOrders(uint8 v) {
	if (_midi)
		_midi->setOrderCount(v);
	else
		_psg->setOrderCount(v);
}

void AtariSound::setPattern(int offset, uint8 value) {
	(void)offset;
	(void)value;
}

int16 AtariSound::trackPosition() const {
	// The native opcode reads the low word of a long overlapping SongLoop.
	return musicLooping();
}

int16 AtariSound::musicLooping() const {
	return _midi ? _midi->looping() : _psg->looping();
}

void AtariSound::musicLoop(int16 v) {
	if (_midi)
		_midi->setLooping(v);
	else
		_psg->setLooping(v);
}

void AtariSound::syncSounds() {
	const byte musicVolume = configuredVolume("music_mute", "music_volume");
	_psg->syncVolume(musicVolume);
	_psg->syncSfxVolume(configuredVolume("sfx_mute", "sfx_volume"));
	if (_midi)
		_midi->syncVolume(musicVolume);
}

} // End of anonymous namespace

Sound *createAtariSound(Audio::Mixer *mixer, CruiseEngine *vm, AtariMidiSink *sink) {
	return new AtariSound(mixer, vm, sink);
}

bool shouldUseAtariMidi(MusicType musicType, bool nativeMT32,
		bool explicitSelection) {
	return explicitSelection && (musicType == MT_MT32 ||
			(musicType == MT_GM && nativeMT32));
}

bool shouldCreateAtariMidi(MusicType musicType, bool nativeMT32,
		bool explicitSelection, bool available) {
	return available && shouldUseAtariMidi(musicType, nativeMT32,
			explicitSelection);
}

} // End of namespace Cruise
