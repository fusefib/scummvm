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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/textconsole.h"

#include "cruise/cruise.h"
#include "cruise/cruise_main.h"
#include "cruise/sound.h"
#include "cruise/sound/dos_driver.h"
#include "cruise/sound/dos_player.h"
#include "cruise/sound/resources.h"
#include "cruise/sound/state.h"

#include "audio/mididrv.h"

namespace Cruise {

namespace {

class HostedPCSoundFxPlayer : public PCSoundFxPlayer {
public:
	HostedPCSoundFxPlayer(PCSoundDriver *driver, CruiseEngine *vm)
		: PCSoundFxPlayer(driver, readBundleSoundFile, MemoryFree), _vm(vm) {}

protected:
	bool replacementShouldQuit() const override { return _vm->soundWaitCancelled(); }
	uint32 replacementMillis() const override { return _vm->presentationMillis(); }
	bool replacementDelay(uint32 delayMs) override { return _vm->waitForSound(delayMs); }

private:
	CruiseEngine *_vm;
};

} // End of anonymous namespace

PCSound::PCSound(Audio::Mixer *mixer, CruiseEngine *vm) {
	_vm = vm;
	_mixer = mixer;
	_soundDriver = nullptr;

	if (_vm->getPlatform() == Common::kPlatformDOS) {
		const MidiDriver::DeviceHandle device = MidiDriver::detectDevice(
				MDT_PCSPK | MDT_ADLIB);
		const MusicType musicType = MidiDriver::getMusicType(device);

		switch (musicType) {
		case MT_PCSPK:
			debugC(1, kCruiseDebugSound, "Using original Cruise PC speaker backend");
			_soundDriver = createPCSpeakerSoundDriver(_mixer);
			break;
		case MT_MT32:
		case MT_GM:
		case MT_GS:
		case MT_ADLIB:
		default:
			break;
		}
	}

	if (!_soundDriver) {
		debugC(1, kCruiseDebugSound, "Using original Cruise AdLib/ADL backend");
		_soundDriver = createAdLibSoundDriver(_mixer);
	}

	_player = new HostedPCSoundFxPlayer(_soundDriver, vm);
	_genVolume = 0;
}

bool Sound::isMusicAvailable(const char *name) const {
	return isResourceAvailable(name);
}

bool Sound::canReuseMusic(const char *name) {
	return !strcmp(musicName(), name);
}

PCSound::~PCSound() {
	delete _player;
	delete _soundDriver;
}

Sound *createSound(Audio::Mixer *mixer, CruiseEngine *vm) {
	return new PCSound(mixer, vm);
}

void PCSound::loadMusic(const char *name) {
	debugC(5, kCruiseDebugSound, "PCSound::loadMusic('%s')", name);
	_player->load(name);
}

void PCSound::playMusic() {
	debugC(5, kCruiseDebugSound, "PCSound::playMusic()");
	_player->play();
}

void PCSound::stopMusic() {
	debugC(5, kCruiseDebugSound, "PCSound::stopMusic()");
	_player->stop();
}

void PCSound::toggleSound() {
	_soundDriver->toggleSound();
}

void PCSound::pause(bool paused) {
	_soundDriver->pause(paused);
}

void PCSound::removeMusic() {
	debugC(5, kCruiseDebugSound, "PCSound::removeMusic()");
	_player->unload();
}

void PCSound::fadeOutMusic() {
	debugC(5, kCruiseDebugSound, "PCSound::fadeOutMusic()");
	_player->fadeOut();
}

void PCSound::playEffect(int sample, int channel, int period, int volume) {
	(void)channel;
	(void)period;
	if (sample < 0 || sample >= NUM_FILE_ENTRIES)
		return;
	if (!filesDatabase[sample].subData.ptr)
		return;

	_soundDriver->playSample(filesDatabase[sample].subData.ptr,
			getSoundResourceSize(sample), 4, volume);
}

void PCSound::loopEffect(int sample, int channel, int period, int volume) {
	if (channel < 0 || channel >= 4 || sample < 0 || sample >= NUM_FILE_ENTRIES)
		return;
	if (!filesDatabase[sample].subData.ptr)
		return;

	SoundEntry &effect = soundList[channel];
	effect.frameNum = sample;
	effect.frequency = period;
	effect.volume = volume;

	// Cruise retains four logical effect slots but dispatches all of them to
	// physical backend channel 4.
	_soundDriver->playSample(filesDatabase[sample].subData.ptr,
		getSoundResourceSize(sample), 4, volume);
}

void PCSound::updateEffect(int sample, int channel, int period, int volume) {
	if (channel < 0 || channel >= 4 || sample < 0 || sample >= NUM_FILE_ENTRIES)
		return;
	if (!filesDatabase[sample].subData.ptr)
		return;

	SoundEntry &effect = soundList[channel];

	// The sample is only a validity guard. The existing frameNum is retained.
	if (volume != -1)
		effect.volume = volume;
	if (period != -1)
		effect.frequency = period;

	// The original handler next calls a bare RETF, so there is no immediate
	// backend operation. The state is used by save/load reconstruction.
}

void PCSound::stopEffect(int channel) {
	if (channel == -1) {
		for (int i = 0; i < 4; ++i)
			soundList[i].frameNum = -1;
		if (!songPlayed())
			_soundDriver->stopChannel(4);
		return;
	}

	if (channel < 0 || channel >= 4)
		return;

	soundList[channel].frameNum = -1;
	if (!songPlayed())
		_soundDriver->stopChannel(4);
}

void PCSound::restoreEffects() {
	for (int channel = 0; channel < 4; ++channel) {
		const SoundEntry &effect = soundList[channel];
		if (effect.frameNum < 0 || effect.frameNum >= NUM_FILE_ENTRIES)
			continue;
		if (!filesDatabase[effect.frameNum].subData.ptr)
			continue;

		// The original restoration loop replays each logical slot through the
		// same fixed hardware effect channel. Its backends ignore the saved
		// frequency; H32 alone consumes the restored volume.
		_soundDriver->playSample(filesDatabase[effect.frameNum].subData.ptr,
			getSoundResourceSize(effect.frameNum), 4, effect.volume);
	}
}

void PCSound::finalizeLoad() {
	_player->finalizeLoad();
	restoreEffects();
}

bool PCSound::songLoaded() const {
	return _player->songLoaded();
}

bool PCSound::songPlayed() const {
	return _player->songPlayed();
}

void PCSound::fadeSong() {
	_player->requestFade();
}

uint8 PCSound::numOrders() const {
	return _player->numOrders();
}

void PCSound::setNumOrders(uint8 v) {
	_player->setNumOrders(v);
}

void PCSound::setPattern(int offset, uint8 value) {
	_player->setPattern(offset, value);
}

int16 PCSound::musicLooping() const {
	return _player->looping();
}

void PCSound::musicLoop(int16 v) {
	_player->setLooping(v);
}

void PCSound::doSync(Common::Serializer &s) {
	_player->doSync(s);
	s.syncAsSint16LE(_genVolume);
}

void PCSound::retireMusic() {
	_player->retire();
}

const char *PCSound::musicName() {
	return _player->musicName();
}

void PCSound::syncSounds() {
	_player->syncSounds();
}

const char *PCSound::soundEffectExtension() const {
	return _soundDriver->getSoundEffectExtension();
}

} // End of namespace Cruise
