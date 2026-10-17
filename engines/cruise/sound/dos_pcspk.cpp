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

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/mutex.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/timer.h"

#include "cruise/cruise_main.h"
#include "cruise/sound/dos_driver.h"
#include "cruise/sound/internal.h"
#include "cruise/sound/resources.h"
#include "cruise/sound/state.h"

#include "audio/mixer.h"
#include "audio/softsynth/pcspk.h"

namespace Cruise {

class PCSpeakerSoundDriverHP : public PCSoundDriver {
public:
	PCSpeakerSoundDriverHP(Audio::Mixer *mixer);
	~PCSpeakerSoundDriverHP() override;

	void setupChannel(int channel, const byte *data, int instrument, int volume) override;
	void setChannelFrequency(int channel, int frequency) override;
	void stopChannel(int channel) override;
	void playSample(const byte *data, int size, int channel, int volume) override;
	void stopAll() override;
	void setMusicPlaybackEnabled(bool enabled) override;
	void loadSong(const char *songName, const byte *moduleData) override;
	void unloadSong() override;
	bool usesInstrumentFiles() const override { return false; }
	bool isPCSpeaker() const override { return true; }
	const char *getSoundEffectExtension() const override { return ".HP"; }
	void syncSounds() override;

private:
	static void timerCallback(void *ref);
	void onTimer();
	void applyOutput();
	uint16 periodToDivisor(int frequency, byte mode) const;

	Audio::Mixer *_mixer;
	Audio::PCSpeakerStream *_stream;
	Common::Mutex _mutex;
	Audio::SoundHandle _handle;
	const byte *_songData;
	byte _ist[15];
	int _channelInstrument[4];
	SoundInternal::PCSpeakerState _state;
};


PCSpeakerSoundDriverHP::PCSpeakerSoundDriverHP(Audio::Mixer *mixer)
	: _mixer(mixer), _stream(nullptr), _mutex(), _songData(nullptr) {
	Common::fill(_ist, _ist + ARRAYSIZE(_ist), 0);
	for (int i = 0; i < 4; ++i)
		_channelInstrument[i] = -1;

	if (!_mixer || !_mixer->isReady())
		error("PC speaker output requires an initialized mixer");

	_stream = new Audio::PCSpeakerStream(_mixer->getOutputRate());
	_mixer->playStream(Audio::Mixer::kPlainSoundType, &_handle, _stream, -1,
			Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);

	syncSounds();
	if (!g_system->getTimerManager()->installTimerProc(timerCallback,
			1000000 / SoundInternal::kMusicTimerHz, this, "cruisePCSpeaker"))
		error("Unable to install Cruise PC speaker timer");
}

PCSpeakerSoundDriverHP::~PCSpeakerSoundDriverHP() {
	g_system->getTimerManager()->removeTimerProc(timerCallback);
	if (_mixer)
		_mixer->stopHandle(_handle);
	delete _stream;
	_stream = nullptr;
}

void PCSpeakerSoundDriverHP::timerCallback(void *ref) {
	static_cast<PCSpeakerSoundDriverHP *>(ref)->onTimer();
}

void PCSpeakerSoundDriverHP::applyOutput() {
	if (!_stream)
		return;
	const byte volume = _state.renderedVolume(_musicVolume, _sfxVolume);
	if (!_state.speakerGate() || _state.programmedDivisor() == 1 ||
			volume == 0) {
		_stream->stop();
		return;
	}

	// A zero PIT reload represents 65536 ticks.
	const int divisor = _state.programmedDivisor() ? _state.programmedDivisor() : 65536;
	const int frequency = 1193180 / divisor;
	if (frequency <= 0) {
		_stream->stop();
		return;
	}
	_stream->play(Audio::PCSpeaker::kWaveFormSquare, frequency, -1,
			volume);
}

void PCSpeakerSoundDriverHP::onTimer() {
	Common::StackLock callbackLock(_callbackMutex);
	if (_paused)
		return;
	bool invokeMusicCallback = false;
	{
		Common::StackLock lock(_mutex);
		if (_state.timerTick(invokeMusicCallback))
			applyOutput();
	}

	if (invokeMusicCallback && _upCb)
		(*_upCb)(_upRef);
}

void PCSpeakerSoundDriverHP::loadSong(const char *songName, const byte *moduleData) {
	Common::StackLock lock(_mutex);
	_songData = moduleData;
	Common::fill(_ist, _ist + ARRAYSIZE(_ist), 0);
	_state.resetMusic();
	for (int i = 0; i < 4; ++i)
		_channelInstrument[i] = -1;

	char istName[64];
	Common::strlcpy(istName, songName, sizeof(istName));
	char *dot = strrchr(istName, '.');
	if (dot)
		*dot = '\0';
	Common::strlcat(istName, ".IST", sizeof(istName));

	uint32 size = 0;
	byte *data = readBundleSoundFile(istName, &size);
	if (!data) {
		debug(2, "No PC-speaker IST table for song '%s'; using mode 0", songName);
		return;
	}

	const uint32 copySize = MIN<uint32>(ARRAYSIZE(_ist), size);
	Common::copy(data, data + copySize, _ist);
	MemFree(data);
}

void PCSpeakerSoundDriverHP::unloadSong() {
	Common::StackLock lock(_mutex);
	_songData = nullptr;
	Common::fill(_ist, _ist + ARRAYSIZE(_ist), 0);
	_state.resetMusic();
	for (int i = 0; i < 4; ++i)
		_channelInstrument[i] = -1;
}

void PCSpeakerSoundDriverHP::setupChannel(int channel, const byte *data, int instrument, int volume) {
	Common::StackLock lock(_mutex);
	(void)data;
	(void)volume;
	assert(channel >= 0 && channel < 4);
	_channelInstrument[channel] = instrument;

	if (_songData && instrument >= 0 && instrument < 15 && _songData[instrument] < 0x1e)
		_state.setChannelDivisor(channel, 1);
}

uint16 PCSpeakerSoundDriverHP::periodToDivisor(int frequency, byte mode) const {
	if (mode == 0)
		return SoundInternal::calculateSpeakerMode0Divisor(frequency);

	if (mode == 1) {
		int note;
		int octave;
		findNote(frequency, &note, &octave);
		(void)octave;
		return SoundInternal::calculateSpeakerMode1Divisor(note);
	}

	return 1;
}

void PCSpeakerSoundDriverHP::setChannelFrequency(int channel, int frequency) {
	Common::StackLock lock(_mutex);
	assert(channel >= 0 && channel < 4);
	const int instrument = _channelInstrument[channel];
	const byte mode = (instrument >= 0 && instrument < 15) ? _ist[instrument] : 0;
	_state.setChannelDivisor(channel, periodToDivisor(frequency, mode));
}

void PCSpeakerSoundDriverHP::stopChannel(int channel) {
	Common::StackLock lock(_mutex);
	if (channel >= 0 && channel < 4)
		_state.setChannelDivisor(channel, 1);
	else if (channel == 4) {
		_state.stopEffect();
		applyOutput();
	}
}

void PCSpeakerSoundDriverHP::playSample(const byte *data, int size, int channel, int volume) {
	Common::StackLock lock(_mutex);
	if (soundDisabled())
		return;
	(void)channel;
	(void)volume;
	if (size < 0 || !SoundInternal::isHPEffectValid(data, size)) {
		warning("Malformed Cruise HP effect (%d bytes)", size);
		return;
	}

	const uint16 count = READ_LE_UINT16(data);
	_state.startEffect(data, count);
	applyOutput();
}

void PCSpeakerSoundDriverHP::stopAll() {
	Common::StackLock lock(_mutex);
	_state.stopAll();
	applyOutput();
}

void PCSpeakerSoundDriverHP::setMusicPlaybackEnabled(bool enabled) {
	Common::StackLock lock(_mutex);
	_state.setMusicEnabled(enabled);
	applyOutput();
}

void PCSpeakerSoundDriverHP::syncSounds() {
	Common::StackLock lock(_mutex);
	PCSoundDriver::syncSounds();
	applyOutput();
}


PCSoundDriver *createPCSpeakerSoundDriver(Audio::Mixer *mixer) {
	return new PCSpeakerSoundDriverHP(mixer);
}

} // End of namespace Cruise
