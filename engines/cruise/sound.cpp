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
#include "common/system.h"
#include "common/textconsole.h"
#include "common/timer.h"

#include "cruise/cruise.h"
#include "cruise/cruise_main.h"
#include "cruise/sound.h"
#include "cruise/volume.h"
#include "cruise/vars.h"

#include "audio/fmopl.h"
#include "audio/mididrv.h"
#include "audio/softsynth/pcspk.h"

namespace Audio {
class Mixer;
}

namespace Cruise {

// The DOS executable programs PIT divisor 0x2E9C, approximately 100 Hz.
static const int kCruiseMusicTimerHz = 100;

class PCSoundDriver {
public:
	typedef void (*UpdateCallback)(void *);

	PCSoundDriver() :
		_upCb(nullptr),
		_upRef(nullptr),
		_musicVolume(0),
		_sfxVolume(0) {}
	virtual ~PCSoundDriver() {}

	virtual void setupChannel(int channel, const byte *data, int instrument, int volume) = 0;
	virtual int prepareMusicVolume(int volume, int fadeOut) const;
	virtual void setChannelFrequency(int channel, int frequency) = 0;
	virtual void stopChannel(int channel) = 0;
	virtual void playSample(const byte *data, int size, int channel, int volume) = 0;
	virtual void stopAll() = 0;
	virtual void loadSong(const char *songName, const byte *moduleData) {}
	virtual void unloadSong() {}
	virtual void prepareInstrument(int instrument, const byte *data) {}
	virtual bool usesInstrumentFiles() const { return true; }
	virtual const char *getInstrumentExtension() const { return ""; }
	virtual const char *getSoundEffectExtension() const { return getInstrumentExtension(); }
	virtual void syncSounds();

	void setUpdateCallback(UpdateCallback upCb, void *ref);
	void resetChannel(int channel);
	void findNote(int freq, int *note, int *oct) const;


protected:
	UpdateCallback _upCb;
	void *_upRef;
	uint8 _musicVolume;
	uint8 _sfxVolume;

	static const int _noteTable[];
	static const int _noteTableCount;
};

const int PCSoundDriver::_noteTable[] = {
	0xEEE, 0xE17, 0xD4D, 0xC8C, 0xBD9, 0xB2F, 0xA8E, 0x9F7,
	0x967, 0x8E0, 0x861, 0x7E8, 0x777, 0x70B, 0x6A6, 0x647,
	0x5EC, 0x597, 0x547, 0x4FB, 0x4B3, 0x470, 0x430, 0x3F4,
	0x3BB, 0x385, 0x353, 0x323, 0x2F6, 0x2CB, 0x2A3, 0x27D,
	0x259, 0x238, 0x218, 0x1FA, 0x1DD, 0x1C2, 0x1A9, 0x191,
	0x17B, 0x165, 0x151, 0x13E, 0x12C, 0x11C, 0x10C, 0x0FD,
	0x0EE, 0x0E1, 0x0D4, 0x0C8, 0x0BD, 0x0B2, 0x0A8, 0x09F,
	0x096, 0x08E, 0x086, 0x07E, 0x077, 0x070, 0x06A, 0x064,
	0x05E, 0x059, 0x054, 0x04F, 0x04B, 0x047, 0x043, 0x03F,
	0x03B, 0x038, 0x035, 0x032, 0x02F, 0x02C, 0x02A, 0x027,
	0x025, 0x023, 0x021, 0x01F, 0x01D, 0x01C, 0x01A, 0x019,
	0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x010, 0x00F
};

const int PCSoundDriver::_noteTableCount = ARRAYSIZE(_noteTable);

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
	byte amDepth;
};

struct VolumeEntry {
	int original;
	int adjusted;
};

class AdLibSoundDriver : public PCSoundDriver {
public:
	AdLibSoundDriver(Audio::Mixer *mixer);
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
	AdLibSoundDriverADL(Audio::Mixer *mixer) : AdLibSoundDriver(mixer) {}
	const char *getInstrumentExtension() const override { return ".ADL"; }
	void loadInstrument(const byte *data, AdLibSoundInstrument *asi) override;
	void setChannelFrequency(int channel, int frequency) override;
	void playSample(const byte *data, int size, int channel, int volume) override;
};

class MT32SoundDriverH32 : public PCSoundDriver {
public:
	MT32SoundDriverH32(MidiDriver *midi);
	~MT32SoundDriverH32() override;

	void setupChannel(int channel, const byte *data, int instrument, int volume) override;
	int prepareMusicVolume(int volume, int fadeOut) const override;
	void setChannelFrequency(int channel, int frequency) override;
	void stopChannel(int channel) override;
	void playSample(const byte *data, int size, int channel, int volume) override;
	void stopAll() override;
	void loadSong(const char *songName, const byte *moduleData) override;
	void prepareInstrument(int instrument, const byte *data) override;
	const char *getInstrumentExtension() const override { return ".H32"; }
	const char *getSoundEffectExtension() const override { return ".H32"; }
	void syncSounds() override;

private:
	static void timerCallback(void *ref);
	void onTimer();
	void sendDT1(byte address0, byte address1, byte address2, const byte *data, uint16 length);
	void sendPatchTemporary(int channel, int timbreGroup, int timbreNumber, int volume);
	void uploadCustomTimbre(int slot, const byte *data);
	int scaleUserVolume(int logicalVolume, bool sfx) const;

	MidiDriver *_midi;
	uint32 _timerAccumulator;
	uint32 _timerQuantum;
	bool _customTimbreLoaded[15];
	bool _channelValid[5];
	byte _channelTimbreGroup[5];
	byte _channelTimbreNumber[5];
	byte _channelLogicalVolume[5];
};

class PCSpeakerSoundDriverHP : public PCSoundDriver {
public:
	PCSpeakerSoundDriverHP(Audio::Mixer *mixer);
	~PCSpeakerSoundDriverHP() override;

	void setupChannel(int channel, const byte *data, int instrument, int volume) override;
	void setChannelFrequency(int channel, int frequency) override;
	void stopChannel(int channel) override;
	void playSample(const byte *data, int size, int channel, int volume) override;
	void stopAll() override;
	void loadSong(const char *songName, const byte *moduleData) override;
	void unloadSong() override;
	bool usesInstrumentFiles() const override { return false; }
	const char *getSoundEffectExtension() const override { return ".HP"; }
	void syncSounds() override;

private:
	struct HPEffectState {
		bool active;
		uint16 segmentsRemaining;
		uint16 ticksRemaining;
		int32 divisor;
		const byte *nextCommand;
		const byte *end;

		HPEffectState()
			: active(false), segmentsRemaining(0), ticksRemaining(0), divisor(1),
			  nextCommand(nullptr), end(nullptr) {}
	};

	static void timerCallback(void *ref);
	void onTimer();
	bool updateEffect();
	void outputDivisor(int32 divisor, byte volume);
	uint16 periodToDivisor(int frequency, byte mode) const;

	Audio::Mixer *_mixer;
	Audio::PCSpeakerStream *_stream;
	Audio::SoundHandle _handle;
	const byte *_songData;
	byte _ist[15];
	int _channelInstrument[4];
	uint16 _channelDivisor[4];
	byte _muxChannel;
	byte _timerParity;
	HPEffectState _effect;
};

class PCSoundFxPlayer {
private:
	enum {
		NUM_INSTRUMENTS = 15,
		NUM_CHANNELS = 4,

		VOLUME_TABLE_OFFSET = 0,
		INSTRUMENT_NAME_OFFSET = 20,
		INSTRUMENT_RECORD_SIZE = 30,
		INSTRUMENT_NAME_SIZE = 12,
		NUM_ORDERS_OFFSET = 470,
		TEMPO_OFFSET = 471,
		ORDER_TABLE_OFFSET = 472,
		AUX_DATA_OFFSET = 600,
		AUX_DATA_SIZE = 1800,
		PATTERN_DATA_OFFSET = AUX_DATA_OFFSET + AUX_DATA_SIZE,
		PATTERN_SIZE = 1024,
		ROW_SIZE = 16,
		CHANNEL_EVENT_SIZE = 4,
		EVENT_INSTRUMENT_OFFSET = 2
	};

	void update();
	void handleEvents();
	void handlePattern(int channel, const byte *patternData);

	char _musicName[33];
	bool _playing;
	bool _songPlayed;
	int _currentPos;
	int _currentOrder;
	int _numOrders;
	int _eventsDelay;
	bool _looping;
	int _fadeOutCounter;
	int _updateTicksCounter;
	int _instrumentsChannelTable[NUM_CHANNELS];
	byte *_sfxData;
	byte *_instrumentsData[NUM_INSTRUMENTS];
	PCSoundDriver *_driver;

public:
	PCSoundFxPlayer(PCSoundDriver *driver);
	~PCSoundFxPlayer();

	bool load(const char *song);
	void play();
	void stop();
	void unload();
	void fadeOut();
	void doSync(Common::Serializer &s);

	static void updateCallback(void *ref);

	bool songLoaded() const { return _sfxData != nullptr; }
	bool songPlayed() const { return _songPlayed; }
	bool playing() const { return _playing; }
	uint8 numOrders() const { assert(_sfxData); return _sfxData[NUM_ORDERS_OFFSET]; }
	void setNumOrders(uint8 v) { assert(_sfxData); _sfxData[NUM_ORDERS_OFFSET] = v; }
	void setPattern(int offset, uint8 value) { assert(_sfxData); _sfxData[ORDER_TABLE_OFFSET + offset] = value; }
	const char *musicName() { return _musicName; }

	// Note: Original game never actually uses looping variable. Songs are hardcoded to loop
	bool looping() const { return _looping; }
	void setLooping(bool v) { _looping = v; }
};

byte *readBundleSoundFile(const char *name) {
	// Load the correct file
	int fileIdx = findFileInDisks(name);
	if (fileIdx < 0) return nullptr;

	int unpackedSize = volumePtrToFileDescriptor[fileIdx].extSize + 2;
	byte *data = (byte *)MemAlloc(unpackedSize);
	assert(data);

	if (volumePtrToFileDescriptor[fileIdx].size + 2 != unpackedSize) {
		uint8 *packedBuffer = (uint8 *)mallocAndZero(volumePtrToFileDescriptor[fileIdx].size + 2);

		loadPackedFileToMem(fileIdx, packedBuffer);

		//uint32 realUnpackedSize = READ_BE_UINT32(packedBuffer + volumePtrToFileDescriptor[fileIdx].size - 4);

		delphineUnpack(data, packedBuffer, volumePtrToFileDescriptor[fileIdx].size);

		MemFree(packedBuffer);
	} else {
		loadPackedFileToMem(fileIdx, data);
	}

	return data;
}


int PCSoundDriver::prepareMusicVolume(int volume, int fadeOut) const {
	volume -= fadeOut;
	return volume > 0 ? volume : 0;
}

void PCSoundDriver::setUpdateCallback(UpdateCallback upCb, void *ref) {
	_upCb = upCb;
	_upRef = ref;
}

void PCSoundDriver::findNote(int freq, int *note, int *oct) const {
	*note = _noteTableCount - 1;
	for (int i = 0; i < _noteTableCount; ++i) {
		if (_noteTable[i] <= freq) {
			*note = i;
			break;
		}
	}

	*oct = *note / 12;
	*note %= 12;
}

void PCSoundDriver::resetChannel(int channel) {
	stopChannel(channel);
	stopAll();
}

void PCSoundDriver::syncSounds() {
	bool mute = false;
	if (ConfMan.hasKey("mute"))
		mute = ConfMan.getBool("mute");

	bool music_mute = mute;
	bool sfx_mute = mute;

	if (!mute) {
		music_mute = ConfMan.getBool("music_mute");
		sfx_mute = ConfMan.getBool("sfx_mute");
	}

	// Get the new music and sfx volumes
	_musicVolume = music_mute ? 0 : MIN(255, ConfMan.getInt("music_volume"));
	_sfxVolume = sfx_mute ? 0 : MIN(255, ConfMan.getInt("sfx_volume"));
}

AdLibSoundDriver::AdLibSoundDriver(Audio::Mixer *mixer)
	: _mixer(mixer) {
	_opl = OPL::Config::create();
	if (!_opl || !_opl->init())
		error("Failed to create OPL");

	for (int i = 0; i < 5; ++i) {
		_channelsVolumeTable[i].original = 0;
		_channelsVolumeTable[i].adjusted = 0;
	}
	memset(_instrumentsTable, 0, sizeof(_instrumentsTable));
	initCard();

	_musicVolume = ConfMan.getBool("music_mute") ? 0 : MIN(255, ConfMan.getInt("music_volume"));
	_sfxVolume = ConfMan.getBool("sfx_mute") ? 0 : MIN(255, ConfMan.getInt("sfx_volume"));

	_opl->start(new Common::Functor0Mem<void, AdLibSoundDriver>(this, &AdLibSoundDriver::onTimer), kCruiseMusicTimerHz);
}

AdLibSoundDriver::~AdLibSoundDriver() {
	delete _opl;
}

void AdLibSoundDriver::syncSounds() {
	PCSoundDriver::syncSounds();

	// Force all instruments to reload on the next playing point
	for (int i = 0; i < 5; ++i) {
		adjustVolume(i, _channelsVolumeTable[i].original);
		AdLibSoundInstrument *ins = &_instrumentsTable[i];
		setupInstrument(ins, i);
	}
}

int AdLibSoundDriver::prepareMusicVolume(int volume, int fadeOut) const {
	// The original AdLib driver floors non-zero tracker volume before fading,
	// then adds 25 percent and clamps to its seven-bit logical range.
	if (volume != 0 && volume < 0x50)
		volume = 0x50;

	volume -= fadeOut;
	if (volume < 0)
		volume = 0;

	volume += volume / 4;
	return MIN(volume, 0x7f);
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
	assert(channel < 5);
	if (data) {
		adjustVolume(channel, volume);
		setupInstrument(data, channel);
	}
}

void AdLibSoundDriver::stopChannel(int channel) {
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
	for (int i = 0; i < 18; ++i)
		_opl->writeReg(0x40 | _operatorsTable[i], 63);

	for (int i = 0; i < 9; ++i)
		_opl->writeReg(0xB0 | i, 0);

	_opl->writeReg(0xBD, 0);
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
	if (_upCb) {
		(*_upCb)(_upRef);
	}
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
	asi->amDepth = *data++;
	++data;
	loadRegisterInstrument(data, &asi->regMod); data += 26;
	loadRegisterInstrument(data, &asi->regCar); data += 26;
}

void AdLibSoundDriverADL::setChannelFrequency(int channel, int frequency) {
	assert(channel < 5);
	AdLibSoundInstrument *ins = &_instrumentsTable[channel];
	if (ins->mode != 0) {
		channel = ins->channel;
		if (channel == 9) {
			channel = 8;
		} else if (channel == 10) {
			channel = 7;
		}
	}
	int freq, note, oct;
	findNote(frequency, &note, &oct);

	note += oct * 12;
	if (ins->amDepth) {
		note = ins->amDepth;
	}
	if (note < 0) {
		note = 0;
	}

	freq = _freqTable[note % 12];
	_opl->writeReg(0xA0 | channel, freq);
	freq = ((note / 12) << 2) | ((freq & 0x300) >> 8);
	if (ins->mode == 0) {
		freq |= 0x20;
	}
	_opl->writeReg(0xB0 | channel, freq);
	if (ins->mode != 0) {
		_vibrato |= 1 << (10 - channel);
		_opl->writeReg(0xBD, _vibrato);
	}
}

void AdLibSoundDriverADL::playSample(const byte *data, int size, int channel, int volume) {
	assert(channel < 5);
	adjustVolume(channel, 0x7f);

	setupInstrument(data, channel);
	AdLibSoundInstrument *ins = &_instrumentsTable[channel];
	if (ins->mode != 0 && ins->channel == 6) {
		_opl->writeReg(0xB0 | channel, 0);
	}
	if (ins->mode != 0) {
		_vibrato &= ~(1 << (10 - ins->channel));
		_opl->writeReg(0xBD, _vibrato);
	}
	if (ins->mode != 0) {
		channel = ins->channel;
		if (channel == 9) {
			channel = 8;
		} else if (channel == 10) {
			channel = 7;
		}
	}
	uint16 note = 48;
	if (ins->amDepth) {
		note = ins->amDepth;
	}
	int freq = _freqTable[note % 12];
	_opl->writeReg(0xA0 | channel, freq);
	freq = ((note / 12) << 2) | ((freq & 0x300) >> 8);
	if (ins->mode == 0) {
		freq |= 0x20;
	}
	_opl->writeReg(0xB0 | channel, freq);
	if (ins->mode != 0) {
		_vibrato |= 1 << (10 - channel);
		_opl->writeReg(0xBD, _vibrato);
	}
}

MT32SoundDriverH32::MT32SoundDriverH32(MidiDriver *midi)
	: _midi(midi), _timerAccumulator(0), _timerQuantum(0) {
	Common::fill(_customTimbreLoaded, _customTimbreLoaded + ARRAYSIZE(_customTimbreLoaded), false);
	Common::fill(_channelValid, _channelValid + ARRAYSIZE(_channelValid), false);
	Common::fill(_channelTimbreGroup, _channelTimbreGroup + ARRAYSIZE(_channelTimbreGroup), 0);
	Common::fill(_channelTimbreNumber, _channelTimbreNumber + ARRAYSIZE(_channelTimbreNumber), 0);
	Common::fill(_channelLogicalVolume, _channelLogicalVolume + ARRAYSIZE(_channelLogicalVolume), 0);

	assert(_midi);
	_midi->sendMT32Reset();
	_timerQuantum = _midi->getBaseTempo();
	if (_timerQuantum == 0)
		_timerQuantum = 1000000 / kCruiseMusicTimerHz;
	_midi->setTimerCallback(this, timerCallback);

	syncSounds();
}

MT32SoundDriverH32::~MT32SoundDriverH32() {
	if (_midi) {
		_midi->setTimerCallback(nullptr, nullptr);
		stopAll();
		_midi->close();
		delete _midi;
		_midi = nullptr;
	}
}

void MT32SoundDriverH32::timerCallback(void *ref) {
	static_cast<MT32SoundDriverH32 *>(ref)->onTimer();
}

void MT32SoundDriverH32::onTimer() {
	_timerAccumulator += _timerQuantum;
	const uint32 cruiseTick = 1000000 / kCruiseMusicTimerHz;
	while (_timerAccumulator >= cruiseTick) {
		_timerAccumulator -= cruiseTick;
		if (_upCb)
			(*_upCb)(_upRef);
	}
}

void MT32SoundDriverH32::sendDT1(byte address0, byte address1, byte address2, const byte *data, uint16 length) {
	byte message[254];
	assert(length <= (uint16)(sizeof(message) - 8));
	message[0] = 0x41;
	message[1] = 0x10;
	message[2] = 0x16;
	message[3] = 0x12;
	message[4] = address0;
	message[5] = address1;
	message[6] = address2;
	Common::copy(data, data + length, message + 7);

	uint32 checksum = address0 + address1 + address2;
	for (uint16 i = 0; i < length; ++i)
		checksum += data[i];
	message[7 + length] = 0x80 - (checksum & 0x7f);
	_midi->sysEx(message, length + 8);
}

void MT32SoundDriverH32::uploadCustomTimbre(int slot, const byte *data) {
	assert(slot >= 0 && slot < 64);
	// The original uses address 08:(slot * 2):00 and 246 data bytes.
	sendDT1(0x08, (slot * 2) & 0x7f, 0x00, data, 0xf6);
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

void MT32SoundDriverH32::loadSong(const char *songName, const byte *moduleData) {
	(void)songName;
	(void)moduleData;
	Common::fill(_customTimbreLoaded, _customTimbreLoaded + ARRAYSIZE(_customTimbreLoaded), false);
}

void MT32SoundDriverH32::prepareInstrument(int instrument, const byte *data) {
	if (!data || instrument < 0 || instrument >= 15)
		return;

	const byte selector = data[22];
	if (selector >= 0x80 && !_customTimbreLoaded[instrument]) {
		uploadCustomTimbre(instrument, data + 23);
		_customTimbreLoaded[instrument] = true;
	}
}

int MT32SoundDriverH32::prepareMusicVolume(int volume, int fadeOut) const {
	volume -= fadeOut;
	return (volume >= 0 && volume <= 100) ? volume : 0;
}

void MT32SoundDriverH32::setupChannel(int channel, const byte *data, int instrument, int volume) {
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
		prepareInstrument(instrument, data);
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
	assert(channel >= 0 && channel < 4);
	int note;
	int octave;
	findNote(frequency, &note, &octave);
	const int midiNote = CLIP(note + (octave + 1) * 12, 0, 127);
	// Tracker channels 0..3 map to MIDI channels 1..4.
	_midi->send(0x91 + channel, midiNote, 0x7f);
}

void MT32SoundDriverH32::stopChannel(int channel) {
	assert(channel >= 0 && channel < 5);
	_midi->send(0xb1 + channel, MidiDriver::MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
}

void MT32SoundDriverH32::stopAll() {
	if (_midi)
		_midi->stopAllNotes(true);
}

void MT32SoundDriverH32::playSample(const byte *data, int size, int channel, int volume) {
	assert(channel >= 0 && channel < 5);
	if (!data || size < 23)
		return;

	stopChannel(channel);

	const byte selector = data[22];
	int group;
	int number;
	if (selector < 0x80) {
		group = selector >> 6;
		number = selector & 0x3f;
	} else {
		if (size < 23 + 0xf6) {
			warning("Truncated custom H32 sound effect (%d bytes)", size);
			return;
		}
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
	PCSoundDriver::syncSounds();
	for (int channel = 0; channel < 5; ++channel) {
		if (!_channelValid[channel])
			continue;
		const bool sfx = channel == 4;
		sendPatchTemporary(channel, _channelTimbreGroup[channel], _channelTimbreNumber[channel],
				scaleUserVolume(_channelLogicalVolume[channel], sfx));
	}
}

PCSpeakerSoundDriverHP::PCSpeakerSoundDriverHP(Audio::Mixer *mixer)
	: _mixer(mixer), _stream(nullptr), _songData(nullptr), _muxChannel(0), _timerParity(0) {
	Common::fill(_ist, _ist + ARRAYSIZE(_ist), 0);
	for (int i = 0; i < 4; ++i) {
		_channelInstrument[i] = -1;
		_channelDivisor[i] = 1;
	}

	if (!_mixer || !_mixer->isReady())
		error("PC speaker output requires an initialized mixer");

	_stream = new Audio::PCSpeakerStream(_mixer->getOutputRate());
	_mixer->playStream(Audio::Mixer::kPlainSoundType, &_handle, _stream, -1,
			Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);

	syncSounds();
	if (!g_system->getTimerManager()->installTimerProc(timerCallback,
			1000000 / kCruiseMusicTimerHz, this, "cruisePCSpeaker"))
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

void PCSpeakerSoundDriverHP::outputDivisor(int32 divisor, byte volume) {
	if (!_stream)
		return;
	if (divisor <= 1 || volume == 0) {
		_stream->stop();
		return;
	}

	const int frequency = 1193180 / divisor;
	if (frequency <= 0) {
		_stream->stop();
		return;
	}
	_stream->play(Audio::PCSpeaker::kWaveFormSquare, frequency, -1, volume);
}

bool PCSpeakerSoundDriverHP::updateEffect() {
	if (!_effect.active)
		return false;

	if (_effect.ticksRemaining > 0)
		--_effect.ticksRemaining;
	if (_effect.ticksRemaining != 0)
		return false;

	if (_effect.segmentsRemaining > 0)
		--_effect.segmentsRemaining;
	if (_effect.segmentsRemaining == 0) {
		_effect.active = false;
		return true;
	}

	if (!_effect.nextCommand || _effect.nextCommand + 4 > _effect.end) {
		warning("Truncated Cruise HP effect command stream");
		_effect.active = false;
		return true;
	}

	_effect.ticksRemaining = READ_LE_UINT16(_effect.nextCommand);
	if (_effect.ticksRemaining == 0)
		_effect.ticksRemaining = 1;
	_effect.divisor += (int16)READ_LE_UINT16(_effect.nextCommand + 2);
	_effect.nextCommand += 4;
	return true;
}

void PCSpeakerSoundDriverHP::onTimer() {
	// The original IRQ updates HP first and only changes its divisor when a
	// segment advances or finishes.
	if (updateEffect()) {
		if (_effect.active)
			outputDivisor(_effect.divisor, _sfxVolume);
		else if (_stream)
			_stream->stop();
	}

	// Four music voices are multiplexed on every other 100 Hz interrupt.
	_timerParity ^= 1;
	if (_timerParity & 1) {
		_muxChannel = (_muxChannel + 1) & 3;
		outputDivisor(_channelDivisor[_muxChannel], _musicVolume);
	}

	if (_upCb)
		(*_upCb)(_upRef);
}

void PCSpeakerSoundDriverHP::loadSong(const char *songName, const byte *moduleData) {
	_songData = moduleData;
	Common::fill(_ist, _ist + ARRAYSIZE(_ist), 0);
	for (int i = 0; i < 4; ++i) {
		_channelInstrument[i] = -1;
		_channelDivisor[i] = 1;
	}

	char istName[64];
	Common::strlcpy(istName, songName, sizeof(istName));
	char *dot = strrchr(istName, '.');
	if (dot)
		*dot = '\0';
	Common::strlcat(istName, ".IST", sizeof(istName));

	const int fileIdx = findFileInDisks(istName);
	if (fileIdx < 0) {
		debug(2, "No PC-speaker IST table for song '%s'; using mode 0", songName);
		return;
	}

	byte *data = readBundleSoundFile(istName);
	if (!data)
		return;
	const int copySize = MIN(15, (int)volumePtrToFileDescriptor[fileIdx].extSize);
	Common::copy(data, data + copySize, _ist);
	MemFree(data);
}

void PCSpeakerSoundDriverHP::unloadSong() {
	_songData = nullptr;
	Common::fill(_ist, _ist + ARRAYSIZE(_ist), 0);
	for (int i = 0; i < 4; ++i) {
		_channelInstrument[i] = -1;
		_channelDivisor[i] = 1;
	}
}

void PCSpeakerSoundDriverHP::setupChannel(int channel, const byte *data, int instrument, int volume) {
	(void)data;
	(void)volume;
	assert(channel >= 0 && channel < 4);
	_channelInstrument[channel] = instrument;

	if (_songData && instrument >= 0 && instrument < 15 && _songData[instrument] < 0x1e)
		_channelDivisor[channel] = 1;
}

uint16 PCSpeakerSoundDriverHP::periodToDivisor(int frequency, byte mode) const {
	if (mode == 0) {
		int period = frequency;
		if (period == 0)
			period = 0x78;
		if (period < 0)
			return 1;
		const uint32 intermediateHz = 249920U / (uint32)period;
		return intermediateHz ? (uint16)MIN((uint32)0xffff, 1193180U / intermediateHz) : 1;
	}

	if (mode == 1) {
		int note;
		int octave;
		findNote(frequency, &note, &octave);
		(void)octave;
		uint32 intermediateHz = (note + 0x10) * 2;
		if (intermediateHz == 0)
			intermediateHz = 0x7d0;
		return (uint16)MIN((uint32)0xffff, 1193180U / intermediateHz);
	}

	return 1;
}

void PCSpeakerSoundDriverHP::setChannelFrequency(int channel, int frequency) {
	assert(channel >= 0 && channel < 4);
	const int instrument = _channelInstrument[channel];
	const byte mode = (instrument >= 0 && instrument < 15) ? _ist[instrument] : 0;
	_channelDivisor[channel] = periodToDivisor(frequency, mode);
}

void PCSpeakerSoundDriverHP::stopChannel(int channel) {
	if (channel >= 0 && channel < 4)
		_channelDivisor[channel] = 1;
	else if (channel == 4)
		_effect.active = false;
}

void PCSpeakerSoundDriverHP::playSample(const byte *data, int size, int channel, int volume) {
	(void)channel;
	(void)volume;
	if (!data || size < 10)
		return;

	const uint16 count = READ_LE_UINT16(data);
	if (count == 0)
		return;
	if ((uint32)size < 6 + (uint32)count * 4) {
		warning("Truncated Cruise HP effect (%d bytes, %u segments)", size, count);
		return;
	}

	_effect.active = true;
	_effect.segmentsRemaining = count;
	_effect.divisor = READ_LE_UINT16(data + 2);
	_effect.ticksRemaining = READ_LE_UINT16(data + 6);
	if (_effect.ticksRemaining == 0)
		_effect.ticksRemaining = 1;
	_effect.divisor += (int16)READ_LE_UINT16(data + 8);
	_effect.nextCommand = data + 10;
	_effect.end = data + size;

	outputDivisor(_effect.divisor, _sfxVolume);
}

void PCSpeakerSoundDriverHP::stopAll() {
	for (int i = 0; i < 4; ++i)
		_channelDivisor[i] = 1;
	_effect.active = false;
	if (_stream)
		_stream->stop();
}

void PCSpeakerSoundDriverHP::syncSounds() {
	PCSoundDriver::syncSounds();
}

PCSoundFxPlayer::PCSoundFxPlayer(PCSoundDriver *driver)
	: _playing(false), _songPlayed(false), _driver(driver) {
	memset(_instrumentsData, 0, sizeof(_instrumentsData));
	_sfxData = nullptr;
	_fadeOutCounter = 0;
	_driver->setUpdateCallback(updateCallback, this);

	_currentPos = 0;
	_currentOrder = 0;
	_numOrders = 0;
	_eventsDelay = 0;
	_looping = false;
	_updateTicksCounter = 0;
}

PCSoundFxPlayer::~PCSoundFxPlayer() {
	_driver->setUpdateCallback(nullptr, nullptr);
	stop();
}

bool PCSoundFxPlayer::load(const char *song) {
	debug(9, "PCSoundFxPlayer::load('%s')", song);

	/* stop (w/ fade out) the previous song */
	while (_fadeOutCounter != 0 && _fadeOutCounter < 100) {
		g_system->delayMillis(50);
	}
	_fadeOutCounter = 0;

	if (_playing) {
		stop();
	}

	Common::strlcpy(_musicName, song, sizeof(_musicName));
	_songPlayed = false;
	_looping = false;
	_sfxData = readBundleSoundFile(song);
	if (!_sfxData) {
		warning("Unable to load soundfx module '%s'", song);
		return 0;
	}

	_driver->loadSong(song, _sfxData);

	for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
		_instrumentsData[i] = nullptr;
		if (!_driver->usesInstrumentFiles())
			continue;

		char instrument[64];
		Common::fill(instrument, instrument + ARRAYSIZE(instrument), 0);
		Common::copy(_sfxData + INSTRUMENT_NAME_OFFSET + i * INSTRUMENT_RECORD_SIZE,
				_sfxData + INSTRUMENT_NAME_OFFSET + i * INSTRUMENT_RECORD_SIZE + INSTRUMENT_NAME_SIZE,
				instrument);
		instrument[63] = '\0';

		if (strlen(instrument) != 0) {
			char *dot = strrchr(instrument, '.');
			if (dot) {
				*dot = '\0';
			}
			Common::strlcat(instrument, _driver->getInstrumentExtension(), sizeof(instrument));
			_instrumentsData[i] = readBundleSoundFile(instrument);
			if (!_instrumentsData[i]) {
				warning("Unable to load soundfx instrument '%s'", instrument);
			} else {
				_driver->prepareInstrument(i, _instrumentsData[i]);
			}
		}
	}
	return 1;
}

void PCSoundFxPlayer::play() {
	debug(9, "PCSoundFxPlayer::play()");
	if (_sfxData) {
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			_instrumentsChannelTable[i] = -1;
		}
		_currentPos = 0;
		_currentOrder = 0;
		_numOrders = _sfxData[NUM_ORDERS_OFFSET];
		_eventsDelay = (244 - _sfxData[TEMPO_OFFSET]) * 100 / 1060;
		_updateTicksCounter = 0;
		_playing = true;
	}
}

void PCSoundFxPlayer::stop() {
	if (_playing || _fadeOutCounter != 0) {
		_fadeOutCounter = 0;
		_playing = false;
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			_driver->stopChannel(i);
		}
		_driver->stopAll();
	}
	unload();
}

void PCSoundFxPlayer::fadeOut() {
	if (_playing) {
		_fadeOutCounter = 1;
		_playing = false;
	}
}

void PCSoundFxPlayer::updateCallback(void *ref) {
	((PCSoundFxPlayer *)ref)->update();
}

void PCSoundFxPlayer::update() {
	if (_playing || (_fadeOutCounter != 0 && _fadeOutCounter < 100)) {
		++_updateTicksCounter;
		if (_updateTicksCounter > _eventsDelay) {
			handleEvents();
			_updateTicksCounter = 0;
		}
	}
}

void PCSoundFxPlayer::handleEvents() {
	const byte *patternData = _sfxData + PATTERN_DATA_OFFSET;
	const byte *orderTable = _sfxData + ORDER_TABLE_OFFSET;
	uint16 patternNum = orderTable[_currentOrder] * PATTERN_SIZE;

	for (int i = 0; i < NUM_CHANNELS; ++i)
		handlePattern(i, patternData + patternNum + _currentPos + i * CHANNEL_EVENT_SIZE);

	if (_fadeOutCounter != 0 && _fadeOutCounter < 100) {
		_fadeOutCounter += 2;
	}
	if (_fadeOutCounter >= 100) {
		stop();
		return;
	}

	_currentPos += ROW_SIZE;
	if (_currentPos >= PATTERN_SIZE) {
		_currentPos = 0;
		++_currentOrder;
		if (_currentOrder == _numOrders) {
			_currentOrder = 0;
		}
	}
	debug(7, "_currentOrder=%d/%d _currentPos=%d", _currentOrder, _numOrders, _currentPos);
}

void PCSoundFxPlayer::handlePattern(int channel, const byte *patternData) {
	int instrument = patternData[EVENT_INSTRUMENT_OFFSET] >> 4;
	if (instrument != 0) {
		--instrument;
		if (_instrumentsChannelTable[channel] != instrument || _fadeOutCounter != 0) {
			_instrumentsChannelTable[channel] = instrument;
			const int volume = _driver->prepareMusicVolume(_sfxData[VOLUME_TABLE_OFFSET + instrument], _fadeOutCounter);
			_driver->setupChannel(channel, _instrumentsData[instrument], instrument, volume);
		}
	}
	int16 freq = (int16)READ_BE_UINT16(patternData);
	if (freq > 0) {
		_driver->stopChannel(channel);
		_driver->setChannelFrequency(channel, freq);
	}
}

void PCSoundFxPlayer::unload() {
	_driver->unloadSong();
	for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
		MemFree(_instrumentsData[i]);
		_instrumentsData[i] = nullptr;
	}
	MemFree(_sfxData);
	_sfxData = nullptr;
	_songPlayed = true;
}

void PCSoundFxPlayer::doSync(Common::Serializer &s) {
	s.syncBytes((byte *)_musicName, 33);
	uint16 v = (uint16)songLoaded();
	s.syncAsSint16LE(v);

	if (s.isLoading() && v) {
		load(_musicName);

		for (int i = 0; i < NUM_CHANNELS; ++i) {
			_instrumentsChannelTable[i] = -1;
		}

		_numOrders = _sfxData[NUM_ORDERS_OFFSET];
		_eventsDelay = (244 - _sfxData[TEMPO_OFFSET]) * 100 / 1060;
		_updateTicksCounter = 0;
	}

	s.syncAsSint16LE(_songPlayed);
	s.syncAsSint16LE(_looping);
	s.syncAsSint16LE(_currentPos);
	s.syncAsSint16LE(_currentOrder);
	s.syncAsSint16LE(_playing);
}

PCSound::PCSound(Audio::Mixer *mixer, CruiseEngine *vm) {
	_vm = vm;
	_mixer = mixer;
	_soundDriver = nullptr;

	if (_vm->getPlatform() == Common::kPlatformDOS) {
		const MidiDriver::DeviceHandle device = MidiDriver::detectDevice(
				MDT_PCSPK | MDT_ADLIB | MDT_MIDI | MDT_PREFER_MT32);
		const MusicType musicType = MidiDriver::getMusicType(device);

		switch (musicType) {
		case MT_PCSPK:
			debugC(1, kCruiseDebugSound, "Using original Cruise PC speaker backend");
			_soundDriver = new PCSpeakerSoundDriverHP(_mixer);
			break;
		case MT_MT32: {
			MidiDriver *midi = MidiDriver::createMidi(device);
			const int result = midi ? midi->open() : MidiDriver::MERR_DEVICE_NOT_AVAILABLE;
			if (result == 0) {
				debugC(1, kCruiseDebugSound, "Using original Cruise MT-32/H32 backend");
				_soundDriver = new MT32SoundDriverH32(midi);
			} else {
				warning("Unable to open MT-32 output (%s); falling back to AdLib",
						MidiDriver::getErrorName(result));
				delete midi;
			}
			break;
		}
		case MT_GM:
		case MT_GS:
			warning("Cruise H32 data requires MT-32 output; falling back to AdLib");
			break;
		case MT_ADLIB:
		default:
			break;
		}
	}

	if (!_soundDriver) {
		debugC(1, kCruiseDebugSound, "Using original Cruise AdLib/ADL backend");
		_soundDriver = new AdLibSoundDriverADL(_mixer);
	}

	_player = new PCSoundFxPlayer(_soundDriver);
	_genVolume = 0;
}

PCSound::~PCSound() {
	delete _player;
	delete _soundDriver;
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

void PCSound::removeMusic() {
	debugC(5, kCruiseDebugSound, "PCSound::removeMusic()");
	_player->unload();
}

void PCSound::fadeOutMusic() {
	debugC(5, kCruiseDebugSound, "PCSound::fadeOutMusic()");
	_player->fadeOut();
}

void PCSound::playSound(const uint8 *data, int size, int volume) {
	debugC(5, kCruiseDebugSound, "PCSound::playSound() channel %d size %d", 4, size);
	_soundDriver->playSample(data, size, 4, volume);
}

void PCSound::playEffect(int sample, int channel, int period, int volume) {
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
		filesDatabase[sample].width, 4, volume);
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
		_soundDriver->stopChannel(4);
		return;
	}

	if (channel < 0 || channel >= 4)
		return;

	soundList[channel].frameNum = -1;
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
			filesDatabase[effect.frameNum].width, 4, effect.volume);
	}
}

void PCSound::stopSound(int channel) {
	debugC(5, kCruiseDebugSound, "PCSound::stopSound() channel %d", channel);
	_soundDriver->resetChannel(channel);
}

void PCSound::stopChannel(int channel) {
	debugC(5, kCruiseDebugSound, "PCSound::stopChannel() channel %d", channel);
	_soundDriver->stopChannel(channel);
}

bool PCSound::isPlaying() const {
	return _player->playing();
}

bool PCSound::songLoaded() const {
	return _player->songLoaded();
}

bool PCSound::songPlayed() const {
	return _player->songPlayed();
}

void PCSound::fadeSong() {
	_player->fadeOut();
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

bool PCSound::musicLooping() const {
	return _player->looping();
}

void PCSound::musicLoop(bool v) {
	_player->setLooping(v);
}

void PCSound::doSync(Common::Serializer &s) {
	_player->doSync(s);
	s.syncAsSint16LE(_genVolume);
}

const char *PCSound::musicName() {
	return _player->musicName();
}

void PCSound::syncSounds() {
	_soundDriver->syncSounds();
}

const char *PCSound::soundEffectExtension() const {
	return _soundDriver->getSoundEffectExtension();
}

} // End of namespace Cruise
