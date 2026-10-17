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
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/mutex.h"
#include "common/textconsole.h"

#include "cruise/sound/dos_driver.h"
#include "cruise/sound/internal.h"

#include "audio/fmopl.h"

namespace Cruise {

struct AdLibRegisterSoundInstrument {
	uint8 vibrato;
	uint8 attackDecay;
	uint8 sustainRelease;
	uint8 feedbackStrength;
	uint8 keyScaling;
	uint8 outputLevel;
	uint8 freqMod;
};

struct AdLibSoundInstrument {
	byte mode;
	byte channel;
	AdLibRegisterSoundInstrument regMod;
	AdLibRegisterSoundInstrument regCar;
	byte waveSelectMod;
	byte waveSelectCar;
	byte fixedNote;
};

struct VolumeEntry {
	int original;
	int adjusted;
};

class AdLibSoundDriver : public PCSoundDriver {
public:
	AdLibSoundDriver(Audio::Mixer *mixer, OPL::OPL *opl, bool startCallbacks);
	~AdLibSoundDriver() override;

	// PCSoundDriver interface
	void setupChannel(int channel, const byte *data, int instrument, int volume) override;
	void stopChannel(int channel) override;
	void stopAll() override;

	void initCard();
	void onTimer();
	void setupInstrument(const byte *data, int channel);
	void setupInstrument(const AdLibSoundInstrument *ins, int channel);
	void loadRegisterInstrument(const byte *data, AdLibRegisterSoundInstrument *reg);
	virtual void loadInstrument(const byte *data, AdLibSoundInstrument *asi) = 0;
	void syncSounds() override;

	int prepareMusicVolume(int volume, int fadeOut) const override;
	void adjustVolume(int channel, int volume);

protected:
	OPL::OPL *_opl;
	Audio::Mixer *_mixer;
	Common::Mutex _mutex;

	byte _vibrato;
	VolumeEntry _channelsVolumeTable[5];
	AdLibSoundInstrument _instrumentsTable[5];

	static const int _freqTable[];
	static const int _freqTableCount;
	static const int _operatorsTable[];
	static const int _operatorsTableCount;
	static const int _voiceOperatorsTable[];
	static const int _voiceOperatorsTableCount;
};

const int AdLibSoundDriver::_freqTable[] = {
	0x157, 0x16C, 0x181, 0x198, 0x1B1, 0x1CB,
	0x1E6, 0x203, 0x222, 0x243, 0x266, 0x28A
};

const int AdLibSoundDriver::_freqTableCount = ARRAYSIZE(_freqTable);

const int AdLibSoundDriver::_operatorsTable[] = {
	0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13,	16, 17, 18, 19, 20, 21
};

const int AdLibSoundDriver::_operatorsTableCount = ARRAYSIZE(_operatorsTable);

const int AdLibSoundDriver::_voiceOperatorsTable[] = {
	0, 3, 1, 4, 2, 5, 6, 9, 7, 10, 8, 11, 12, 15, 16, 16, 14, 14, 17, 17, 13, 13
};

const int AdLibSoundDriver::_voiceOperatorsTableCount = ARRAYSIZE(_voiceOperatorsTable);

class AdLibSoundDriverADL : public AdLibSoundDriver {
public:
	AdLibSoundDriverADL(Audio::Mixer *mixer, OPL::OPL *opl, bool startCallbacks)
		: AdLibSoundDriver(mixer, opl, startCallbacks) {}
	const char *getInstrumentExtension() const override { return ".ADL"; }
	void loadInstrument(const byte *data, AdLibSoundInstrument *asi) override;
	bool isInstrumentDataValid(const byte *data, uint32 size) const override {
		return SoundInternal::isAdLibInstrumentValid(data, size);
	}
	void setChannelFrequency(int channel, int frequency) override;
	void playSample(const byte *data, int size, int channel, int volume) override;
};


AdLibSoundDriver::AdLibSoundDriver(Audio::Mixer *mixer, OPL::OPL *opl,
		bool startCallbacks)
	: _opl(opl), _mixer(mixer) {
	if (!_opl || !_opl->init())
		error("Failed to create OPL");

	for (int i = 0; i < 5; ++i) {
		_channelsVolumeTable[i].original = 0;
		_channelsVolumeTable[i].adjusted = 0;
	}
	Common::fill(_instrumentsTable,
			_instrumentsTable + ARRAYSIZE(_instrumentsTable), AdLibSoundInstrument());
	initCard();

	_musicVolume = ConfMan.getBool("music_mute") ? 0 : MIN(255, ConfMan.getInt("music_volume"));
	_sfxVolume = ConfMan.getBool("sfx_mute") ? 0 : MIN(255, ConfMan.getInt("sfx_volume"));

	if (startCallbacks)
		_opl->start(new Common::Functor0Mem<void, AdLibSoundDriver>(this,
				&AdLibSoundDriver::onTimer), SoundInternal::kMusicTimerHz);
}

AdLibSoundDriver::~AdLibSoundDriver() {
	delete _opl;
}

void AdLibSoundDriver::syncSounds() {
	Common::StackLock lock(_mutex);
	PCSoundDriver::syncSounds();

	// Force all instruments to reload on the next playing point
	for (int i = 0; i < 5; ++i) {
		adjustVolume(i, _channelsVolumeTable[i].original);
		AdLibSoundInstrument *ins = &_instrumentsTable[i];
		setupInstrument(ins, i);
	}
}

int AdLibSoundDriver::prepareMusicVolume(int volume, int fadeOut) const {
	return SoundInternal::prepareAdLibMusicVolume(volume, fadeOut);
}

void AdLibSoundDriver::adjustVolume(int channel, int volume) {
	_channelsVolumeTable[channel].original = volume;

	volume = CLIP(volume, 0, 127);

	int volAdjust = (channel == 4) ? _sfxVolume : _musicVolume;
	volume = (volume * volAdjust) / 128;

	if (volume > 127)
		volume = 127;

	_channelsVolumeTable[channel].adjusted = volume;
}

void AdLibSoundDriver::setupChannel(int channel, const byte *data, int instrument, int volume) {
	Common::StackLock lock(_mutex);
	assert(channel < 5);
	if (data) {
		adjustVolume(channel, volume);
		setupInstrument(data, channel);
	}
}

void AdLibSoundDriver::stopChannel(int channel) {
	Common::StackLock lock(_mutex);
	assert(channel < 5);
	AdLibSoundInstrument *ins = &_instrumentsTable[channel];
	if (ins->mode != 0 && ins->channel == 6) {
		channel = 6;
	}
	if (ins->mode == 0 || channel == 6) {
		_opl->writeReg(0xB0 | channel, 0);
	}
	if (ins->mode != 0) {
		_vibrato &= ~(1 << (10 - ins->channel));
		_opl->writeReg(0xBD, _vibrato);
	}
}

void AdLibSoundDriver::stopAll() {
	Common::StackLock lock(_mutex);
	for (int i = 0; i < 18; ++i)
		_opl->writeReg(0x40 | _operatorsTable[i], 63);

	for (int i = 0; i < 9; ++i)
		_opl->writeReg(0xB0 | i, 0);

	_vibrato &= 0x20;
	_opl->writeReg(0xBD, _vibrato);
}

void AdLibSoundDriver::initCard() {
	_vibrato = 0x20;
	_opl->writeReg(0xBD, _vibrato);
	_opl->writeReg(0x08, 0x40);

	static const int oplRegs[] = { 0x40, 0x60, 0x80, 0x20, 0xE0 };

	for (int i = 0; i < 9; ++i) {
		_opl->writeReg(0xB0 | i, 0);
	}
	for (int i = 0; i < 9; ++i) {
		_opl->writeReg(0xC0 | i, 0);
	}

	for (int j = 0; j < 5; j++) {
		for (int i = 0; i < 18; ++i) {
			_opl->writeReg(oplRegs[j] | _operatorsTable[i], 0);
		}
	}

	_opl->writeReg(1, 0x20);
	_opl->writeReg(1, 0);
}

void AdLibSoundDriver::onTimer() {
	invokeUpdateCallback();
}

void AdLibSoundDriver::setupInstrument(const byte *data, int channel) {
	assert(channel < 5);
	AdLibSoundInstrument *ins = &_instrumentsTable[channel];
	loadInstrument(data, ins);

	setupInstrument(ins, channel);
}

void AdLibSoundDriver::setupInstrument(const AdLibSoundInstrument *ins, int channel) {
	int mod, car, tmp;
	const AdLibRegisterSoundInstrument *reg;

	if (ins->mode != 0)  {
		mod = _operatorsTable[_voiceOperatorsTable[2 * ins->channel + 0]];
		car = _operatorsTable[_voiceOperatorsTable[2 * ins->channel + 1]];
	} else {
		mod = _operatorsTable[_voiceOperatorsTable[2 * channel + 0]];
		car = _operatorsTable[_voiceOperatorsTable[2 * channel + 1]];
	}

	if (ins->mode == 0 || ins->channel == 6) {
		reg = &ins->regMod;
		_opl->writeReg(0x20 | mod, reg->vibrato);
		if (reg->freqMod) {
			tmp = reg->outputLevel & 0x3F;
		} else {
			tmp = (63 - (reg->outputLevel & 0x3F)) * _channelsVolumeTable[channel].adjusted;
			tmp = 63 - (2 * tmp + 127) / (2 * 127);
		}
		_opl->writeReg(0x40 | mod, tmp | (reg->keyScaling << 6));
		_opl->writeReg(0x60 | mod, reg->attackDecay);
		_opl->writeReg(0x80 | mod, reg->sustainRelease);
		if (ins->mode != 0) {
			_opl->writeReg(0xC0 | ins->channel, reg->feedbackStrength);
		} else {
			_opl->writeReg(0xC0 | channel, reg->feedbackStrength);
		}
		_opl->writeReg(0xE0 | mod, ins->waveSelectMod);
	}

	reg = &ins->regCar;
	_opl->writeReg(0x20 | car, reg->vibrato);
	tmp = (63 - (reg->outputLevel & 0x3F)) * _channelsVolumeTable[channel].adjusted;
	tmp = 63 - (2 * tmp + 127) / (2 * 127);
	_opl->writeReg(0x40 | car, tmp | (reg->keyScaling << 6));
	_opl->writeReg(0x60 | car, reg->attackDecay);
	_opl->writeReg(0x80 | car, reg->sustainRelease);
	_opl->writeReg(0xE0 | car, ins->waveSelectCar);
}

void AdLibSoundDriver::loadRegisterInstrument(const byte *data, AdLibRegisterSoundInstrument *reg) {
	reg->vibrato = 0;
	if (READ_LE_UINT16(data + 18)) { // amplitude vibrato
		reg->vibrato |= 0x80;
	}
	if (READ_LE_UINT16(data + 20)) { // frequency vibrato
		reg->vibrato |= 0x40;
	}
	if (READ_LE_UINT16(data + 10)) { // sustaining sound
		reg->vibrato |= 0x20;
	}
	if (READ_LE_UINT16(data + 22)) { // envelope scaling
		reg->vibrato |= 0x10;
	}
	reg->vibrato |= READ_LE_UINT16(data + 2) & 0xF; // frequency multiplier

	reg->attackDecay = READ_LE_UINT16(data + 6) << 4; // attack rate
	reg->attackDecay |= READ_LE_UINT16(data + 12) & 0xF; // decay rate

	reg->sustainRelease = READ_LE_UINT16(data + 8) << 4; // sustain level
	reg->sustainRelease |= READ_LE_UINT16(data + 14) & 0xF; // release rate

	reg->feedbackStrength = READ_LE_UINT16(data + 4) << 1; // feedback
	if (READ_LE_UINT16(data + 24) == 0) { // frequency modulation
		reg->feedbackStrength |= 1;
	}

	reg->keyScaling = READ_LE_UINT16(data);
	reg->outputLevel = READ_LE_UINT16(data + 16);
	reg->freqMod = READ_LE_UINT16(data + 24);
}

void AdLibSoundDriverADL::loadInstrument(const byte *data, AdLibSoundInstrument *asi) {
	asi->mode = *data++;
	asi->channel = *data++;
	asi->waveSelectMod = *data++ & 3;
	asi->waveSelectCar = *data++ & 3;
	asi->fixedNote = *data++;
	++data;
	loadRegisterInstrument(data, &asi->regMod); data += 26;
	loadRegisterInstrument(data, &asi->regCar);
}

void AdLibSoundDriverADL::setChannelFrequency(int channel, int frequency) {
	Common::StackLock lock(_mutex);
	assert(channel < 5);
	AdLibSoundInstrument *ins = &_instrumentsTable[channel];
	setupInstrument(ins, channel);
	const int rhythmIdentity = ins->channel;
	int frequencyChannel = channel;
	if (ins->mode != 0) {
		frequencyChannel = rhythmIdentity;
		if (frequencyChannel == 9) {
			frequencyChannel = 8;
		} else if (frequencyChannel == 10) {
			frequencyChannel = 7;
		}
	}
	int freq, note, oct;
	findNote(frequency, &note, &oct);

	note += oct * 12;
	if (ins->fixedNote) {
		note = ins->fixedNote;
	}
	if (note < 0) {
		note = 0;
	}

	freq = _freqTable[note % 12];
	_opl->writeReg(0xA0 | frequencyChannel, freq);
	freq = ((note / 12) << 2) | ((freq & 0x300) >> 8);
	if (ins->mode == 0) {
		freq |= 0x20;
	}
	_opl->writeReg(0xB0 | frequencyChannel, freq);
	if (ins->mode != 0) {
		_vibrato |= 1 << (10 - rhythmIdentity);
		_opl->writeReg(0xBD, _vibrato);
	}
}

void AdLibSoundDriverADL::playSample(const byte *data, int size, int channel, int volume) {
	Common::StackLock lock(_mutex);
	assert(channel < 5);
	if (size < 0 || !SoundInternal::isAdLibInstrumentValid(data, size)) {
		warning("Truncated Cruise ADL sound effect (%d bytes)", size);
		return;
	}
	const int logicalChannel = channel;
	AdLibSoundInstrument *ins = &_instrumentsTable[channel];
	loadInstrument(data, ins);
	const int rhythmIdentity = ins->channel;
	if (ins->mode == 0 || rhythmIdentity == 6)
		_opl->writeReg(0xB0 | logicalChannel, 0);
	if (ins->mode != 0) {
		_vibrato &= ~(1 << (10 - rhythmIdentity));
		_opl->writeReg(0xBD, _vibrato);
	}
	// Native DOS clears the old trigger before suppressing a new effect.
	if (soundDisabled())
		return;
	adjustVolume(channel, 0x7f);
	setupInstrument(ins, logicalChannel);

	int frequencyChannel = logicalChannel;
	if (ins->mode != 0) {
		frequencyChannel = rhythmIdentity;
		if (frequencyChannel == 9) {
			frequencyChannel = 8;
		} else if (frequencyChannel == 10) {
			frequencyChannel = 7;
		}
	}
	uint16 note = 48;
	if (ins->fixedNote) {
		note = ins->fixedNote;
	}
	int freq = _freqTable[note % 12];
	_opl->writeReg(0xA0 | frequencyChannel, freq);
	freq = ((note / 12) << 2) | ((freq & 0x300) >> 8);
	if (ins->mode == 0) {
		freq |= 0x20;
	}
	_opl->writeReg(0xB0 | frequencyChannel, freq);
	if (ins->mode != 0) {
		_vibrato |= 1 << (10 - rhythmIdentity);
		_opl->writeReg(0xBD, _vibrato);
	}
}


PCSoundDriver *createAdLibSoundDriver(Audio::Mixer *mixer) {
	return new AdLibSoundDriverADL(mixer, OPL::Config::create(), true);
}

PCSoundDriver *createAdLibSoundDriver(Audio::Mixer *mixer, OPL::OPL *opl) {
	return new AdLibSoundDriverADL(mixer, opl, false);
}

} // End of namespace Cruise
