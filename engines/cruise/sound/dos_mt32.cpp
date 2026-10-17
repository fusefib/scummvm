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
#include "common/mutex.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/timer.h"

#include "cruise/sound/dos_driver.h"
#include "cruise/sound/internal.h"

#include "audio/mididrv.h"

namespace Cruise {

class MT32SoundDriverH32 : public PCSoundDriver {
public:
	MT32SoundDriverH32(MidiDriver *midi);
	~MT32SoundDriverH32() override;

	void setUpdateCallback(UpdateCallback upCb, void *ref) override;
	void setupChannel(int channel, const byte *data, int instrument, int volume) override;
	int prepareMusicVolume(int volume, int fadeOut) const override;
	void setChannelFrequency(int channel, int frequency) override;
	void stopChannel(int channel) override;
	void playSample(const byte *data, int size, int channel, int volume) override;
	void stopAll() override;
	void prepareInstrument(int instrument, const byte *data) override;
	bool isInstrumentDataValid(const byte *data, uint32 size) const override {
		return SoundInternal::isH32InstrumentValid(data, size);
	}
	const char *getInstrumentExtension() const override { return ".H32"; }
	const char *getSoundEffectExtension() const override { return ".H32"; }
	void syncSounds() override;
	void pause(bool paused) override;

private:
	static void timerCallback(void *ref);
	void sendDT1(byte address0, byte address1, byte address2, const byte *data, uint16 length);
	void sendPatchTemporary(int channel, int timbreGroup, int timbreNumber, int volume);
	void uploadCustomTimbre(int slot, const byte *data);
	void stopChannelUnlocked(int channel);
	int scaleUserVolume(int logicalVolume, bool sfx) const;

	MidiDriver *_midi;
	bool _timerInstalled;
	Common::Mutex _mutex;
	bool _channelValid[5];
	byte _channelTimbreGroup[5];
	byte _channelTimbreNumber[5];
	byte _channelLogicalVolume[5];
};


MT32SoundDriverH32::MT32SoundDriverH32(MidiDriver *midi)
	: _midi(midi), _timerInstalled(false), _mutex() {
	Common::fill(_channelValid, _channelValid + ARRAYSIZE(_channelValid), false);
	Common::fill(_channelTimbreGroup, _channelTimbreGroup + ARRAYSIZE(_channelTimbreGroup), 0);
	Common::fill(_channelTimbreNumber, _channelTimbreNumber + ARRAYSIZE(_channelTimbreNumber), 0);
	Common::fill(_channelLogicalVolume, _channelLogicalVolume + ARRAYSIZE(_channelLogicalVolume), 0);

	assert(_midi);
	_midi->sendMT32Reset();
	syncSounds();
}

MT32SoundDriverH32::~MT32SoundDriverH32() {
	setUpdateCallback(nullptr, nullptr);
	if (_midi) {
		stopAll();
		_midi->close();
		delete _midi;
		_midi = nullptr;
	}
}

void MT32SoundDriverH32::timerCallback(void *ref) {
	static_cast<MT32SoundDriverH32 *>(ref)->invokeUpdateCallback();
}

void MT32SoundDriverH32::pause(bool paused) {
	Common::StackLock lock(_callbackMutex);
	if (_paused == paused)
		return;
	_paused = paused;
	if (paused)
		stopAll();
}

void MT32SoundDriverH32::setUpdateCallback(UpdateCallback upCb, void *ref) {
	Common::TimerManager *timer = g_system->getTimerManager();
	assert(timer);

	if (!upCb) {
		PCSoundDriver::setUpdateCallback(nullptr, nullptr);
		if (_timerInstalled) {
			timer->removeTimerProc(timerCallback);
			_timerInstalled = false;
		}
		return;
	}

	PCSoundDriver::setUpdateCallback(upCb, ref);
	if (_timerInstalled)
		return;

	if (!timer->installTimerProc(timerCallback,
			1000000 / SoundInternal::kMusicTimerHz, this, "cruiseMT32")) {
		PCSoundDriver::setUpdateCallback(nullptr, nullptr);
		error("Unable to install Cruise MT-32 music timer");
	}
	_timerInstalled = true;
}

void MT32SoundDriverH32::sendDT1(byte address0, byte address1, byte address2, const byte *data, uint16 length) {
	byte message[SoundInternal::kH32TimbrePayloadSize + 8];
	SoundInternal::buildH32DT1(message, address0, address1, address2, data, length);
	_midi->sysEx(message, length + 8);
}

void MT32SoundDriverH32::uploadCustomTimbre(int slot, const byte *data) {
	assert(slot >= 0 && slot < 64);
	// The original uses address 08:(slot * 2):00 and 246 data bytes.
	sendDT1(0x08, (slot * 2) & 0x7f, 0x00, data,
			SoundInternal::kH32TimbrePayloadSize);
}

int MT32SoundDriverH32::scaleUserVolume(int logicalVolume, bool sfx) const {
	logicalVolume = CLIP(logicalVolume, 0, 100);
	const int userVolume = sfx ? _sfxVolume : _musicVolume;
	return (logicalVolume * userVolume + 127) / 255;
}

void MT32SoundDriverH32::sendPatchTemporary(int channel, int timbreGroup, int timbreNumber, int volume) {
	assert(channel >= 0 && channel < 5);
	byte patch[16] = {
		(byte)timbreGroup, (byte)timbreNumber,
		0x18, // Key shift
		0x32, // Fine tune
		0x0c, // Bender range
		0x03, // Assign mode
		0x01, // Reverb switch
		0x00,
		(byte)CLIP(volume, 0, 100),
		0x07, // Pan
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	// Patch Temporary starts at 03:00:(channel * 10h).
	sendDT1(0x03, 0x00, channel * 0x10, patch, sizeof(patch));
}

void MT32SoundDriverH32::prepareInstrument(int instrument, const byte *data) {
	Common::StackLock lock(_mutex);
	if (!data || instrument < 0 || instrument >= 15)
		return;

	const byte selector = data[22];
	if (selector >= 0x80 && selector < 0xc0)
		uploadCustomTimbre(instrument, data + 23);
}

int MT32SoundDriverH32::prepareMusicVolume(int volume, int fadeOut) const {
	volume -= fadeOut;
	return (volume >= 0 && volume <= 100) ? volume : 0;
}

void MT32SoundDriverH32::setupChannel(int channel, const byte *data, int instrument, int volume) {
	Common::StackLock lock(_mutex);
	assert(channel >= 0 && channel < 4);
	if (!data)
		return;

	const byte selector = data[22];
	int group;
	int number;
	if (selector < 0x80) {
		group = selector >> 6;
		number = selector & 0x3f;
	} else {
		group = 2;
		number = instrument;
	}

	_channelValid[channel] = true;
	_channelTimbreGroup[channel] = group;
	_channelTimbreNumber[channel] = number;
	_channelLogicalVolume[channel] = CLIP(volume, 0, 100);
	sendPatchTemporary(channel, group, number, scaleUserVolume(volume, false));
}

void MT32SoundDriverH32::setChannelFrequency(int channel, int frequency) {
	Common::StackLock lock(_mutex);
	assert(channel >= 0 && channel < 4);
	int note;
	int octave;
	findNote(frequency, &note, &octave);
	const int midiNote = CLIP(note + (octave + 1) * 12, 0, 127);
	// Tracker channels 0..3 map to MIDI channels 1..4.
	_midi->send(0x91 + channel, midiNote, 0x7f);
}

void MT32SoundDriverH32::stopChannel(int channel) {
	Common::StackLock lock(_mutex);
	stopChannelUnlocked(channel);
}

void MT32SoundDriverH32::stopChannelUnlocked(int channel) {
	assert(channel >= 0 && channel < 5);
	_midi->send(0xb1 + channel, MidiDriver::MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
}

void MT32SoundDriverH32::stopAll() {
	Common::StackLock lock(_mutex);
	if (_midi)
		_midi->stopAllNotes(true);
}

void MT32SoundDriverH32::playSample(const byte *data, int size, int channel, int volume) {
	Common::StackLock lock(_mutex);
	if (soundDisabled())
		return;
	assert(channel >= 0 && channel < 5);
	if (size < 0 || !SoundInternal::isH32InstrumentValid(data, size)) {
		warning("Truncated Cruise H32 sound effect (%d bytes)", size);
		return;
	}

	stopChannelUnlocked(channel);

	const byte selector = data[22];
	int group;
	int number;
	if (selector < 0x80) {
		group = selector >> 6;
		number = selector & 0x3f;
	} else {
		uploadCustomTimbre(channel, data + 23);
		group = 2;
		number = channel;
	}

	const int logicalVolume = CLIP((volume * 8) / 5, 0, 100);
	_channelValid[channel] = true;
	_channelTimbreGroup[channel] = group;
	_channelTimbreNumber[channel] = number;
	_channelLogicalVolume[channel] = logicalVolume;
	sendPatchTemporary(channel, group, number, scaleUserVolume(logicalVolume, true));
	_midi->send(0x91 + channel, 0x0c, 0x7f);
}

void MT32SoundDriverH32::syncSounds() {
	Common::StackLock lock(_mutex);
	PCSoundDriver::syncSounds();
	for (int channel = 0; channel < 5; ++channel) {
		if (!_channelValid[channel])
			continue;
		const bool sfx = channel == 4;
		sendPatchTemporary(channel, _channelTimbreGroup[channel], _channelTimbreNumber[channel],
				scaleUserVolume(_channelLogicalVolume[channel], sfx));
	}
}


PCSoundDriver *createMT32SoundDriver(MidiDriver *midi) {
	return new MT32SoundDriverH32(midi);
}

} // End of namespace Cruise
