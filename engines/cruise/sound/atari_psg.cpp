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

#include "audio/chip.h"
#include "audio/ym2149.h"
#ifndef ATARI
#include "audio/softsynth/ym2149.h"
#endif

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/textconsole.h"

#include "cruise/sound/atari_players.h"

namespace Cruise {

namespace {

enum {
	kAtariMfpClock = 2457600,
	kSynthRate = 200,
	kSequencerPrescale = 200,
	kHardwareClockRate = 6400,
	kSampleRate = 6400
};

const uint16 kVolumeTable[16] = {
	0, 18, 35, 52, 69, 86, 103, 120,
	137, 154, 171, 188, 205, 222, 237, 256
};

const uint16 kYmSingleVoiceAmplitude[16] = {
	0, 120, 251, 385, 598, 840, 1258, 1725,
	2536, 3483, 5151, 7054, 10367, 14634, 22931, 34469
};

const byte kSampleDacTable[16][3] = {
	{ 14, 13, 10 }, { 14, 12, 11 }, { 14, 11, 11 }, { 13, 13, 9 },
	{ 12, 12, 12 }, { 12, 12, 11 }, { 12, 12, 9 }, { 13, 9, 8 },
	{ 12, 10, 8 }, { 11, 11, 6 }, { 10, 10, 8 }, { 11, 8, 1 },
	{ 9, 9, 2 }, { 7, 7, 6 }, { 5, 5, 4 }, { 0, 0, 0 }
};

const uint16 kSampleDacAmplitude[16] = {
	35499, 33479, 31211, 28332, 26557, 24097, 21599, 19011,
	16629, 14275, 11913, 9368, 7115, 4742, 2399, 0
};

int nearestAmplitude(const uint16 *amplitudes, int count, uint32 target) {
	int best = 0;
	uint32 bestDifference = ABS((int32)amplitudes[0] - (int32)target);
	for (int i = 1; i < count; ++i) {
		const uint32 difference =
				ABS((int32)amplitudes[i] - (int32)target);
		if (difference < bestDifference ||
				(difference == bestDifference &&
						amplitudes[i] < amplitudes[best])) {
			best = i;
			bestDifference = difference;
		}
	}
	return best;
}

void buildGainMap(const uint16 *amplitudes, int count, byte hostVolume,
		byte *gainMap) {
	for (int level = 0; level < count; ++level) {
		const uint32 target =
				((uint32)amplitudes[level] * hostVolume + 127) / 255;
		gainMap[level] = nearestAmplitude(amplitudes, count, target);
	}
}

class AtariYm2149Sink : public AtariPsgSink {
public:
	enum ClockState {
		kClockIdle,
		kClockRunning,
		kClockSuspended
	};

	AtariYm2149Sink(int outputRate) : _ym(YM2149::Config::create())
#ifndef ATARI
		, _emulated(nullptr)
#endif
		, _clockProc(nullptr), _clockRef(nullptr), _clockState(kClockIdle),
		  _clockWarningIssued(false)
	{
		if (!_ym || !_ym->init()) {
			warning("Unable to initialize Atari YM2149 output");
			delete _ym;
			_ym = nullptr;
		}
#ifndef ATARI
		if (_ym) {
			_emulated = static_cast<Audio::YM2149Emu *>(_ym);
			_emulated->setCallbackFrequency(outputRate);
		}
#else
		(void)outputRate;
#endif
	}
	~AtariYm2149Sink() override {
		stopClock();
		delete _ym;
	}
	void setReg(int reg, byte value) override {
		if (_ym)
			_ym->writeReg(reg, value);
	}
	int readBuffer(int16 *buffer, int count) override {
#ifndef ATARI
		if (_emulated)
			return _emulated->readBuffer(buffer, count);
#endif
		Common::fill(buffer, buffer + count, 0);
		return count;
	}
	bool hasHardwareClock() const override {
#ifdef ATARI
		return _ym != nullptr;
#else
		return false;
#endif
	}
	bool startClock(ClockProc proc, void *ref) override {
#ifdef ATARI
		if (!_ym)
			return false;
		if (_clockState == kClockRunning)
			return true;
		if (_clockState == kClockSuspended)
			stopClock();
		_clockProc = proc;
		_clockRef = ref;
		const YM2149::YM2149::PreciseCallbackResult result =
				_ym->startPreciseCallback(clockCallback, this,
						kHardwareClockRate);
		const bool started = result == YM2149::YM2149::kPreciseCallbackStarted;
		_clockState = started ? kClockRunning : kClockIdle;
		if (!started && !_clockWarningIssued) {
			warning("Unable to acquire precise Atari YM2149 timing");
			_clockWarningIssued = true;
		}
		if (!started) {
			_clockProc = nullptr;
			_clockRef = nullptr;
		}
		return started;
#else
		(void)proc;
		(void)ref;
		return false;
#endif
	}
	void suspendClock() override {
#ifdef ATARI
		if (_ym && _clockState == kClockRunning) {
			_ym->suspendPreciseCallback();
			_clockState = kClockSuspended;
		}
#endif
	}
	void stopClock() override {
#ifdef ATARI
		if (_ym && _clockState != kClockIdle)
			_ym->stopPreciseCallback();
#endif
		_clockState = kClockIdle;
		_clockProc = nullptr;
		_clockRef = nullptr;
	}
	void beginUpdate() override {
#ifdef ATARI
		if (_clockState == kClockRunning)
			_ym->beginPreciseUpdate();
#endif
	}
	void endUpdate() override {
#ifdef ATARI
		if (_clockState == kClockRunning)
			_ym->endPreciseUpdate();
#endif
	}

private:
	static void clockCallback(void *ref) {
		static_cast<AtariYm2149Sink *>(ref)->clockTick();
	}

	void clockTick() {
		if (_clockProc)
			(*_clockProc)(_clockRef);
	}

	YM2149::YM2149 *_ym;
#ifndef ATARI
	Audio::YM2149Emu *_emulated;
#endif
	ClockProc _clockProc;
	void *_clockRef;
	ClockState _clockState;
	bool _clockWarningIssued;
};

class AtariPsgLock {
public:
	AtariPsgLock(Common::Mutex &mutex, AtariPsgSink *sink)
		: _mutex(mutex), _sink(sink) {
		_mutex.lock();
		_sink->beginUpdate();
	}

	~AtariPsgLock() {
		_sink->endUpdate();
		_mutex.unlock();
	}

private:
	Common::Mutex &_mutex;
	AtariPsgSink *_sink;
};

// The synthesizer's ADD.L and NEG.L operations wrap at 32 bits.
int32 addWrapped(int32 left, int32 right) {
	return (int32)((uint32)left + (uint32)right);
}

int32 negateWrapped(int32 value) {
	return (int32)(0U - (uint32)value);
}

int32 advanceStage(int stage, int32 &value, int32 firstDelta,
		int32 firstTarget, int32 secondDelta, int32 secondTarget,
		int32 releaseDelta) {
	if (stage == 1) {
		value = addWrapped(value, firstDelta);
		if ((firstDelta < 0 && value <= firstTarget) ||
				(firstDelta >= 0 && value >= firstTarget)) {
			value = firstTarget;
			return 2;
		}
	} else if (stage == 2) {
		value = addWrapped(value, secondDelta);
		if ((secondDelta < 0 && value <= secondTarget) ||
				(secondDelta >= 0 && value >= secondTarget)) {
			value = secondTarget;
			return 3;
		}
	} else if (stage == 4) {
		value = addWrapped(value, releaseDelta);
		if ((releaseDelta < 0 && value <= 0) ||
				(releaseDelta >= 0 && value >= 0))
			value = 0;
	}
	return stage;
}

void advanceVibrato(int32 amplitude, int32 &step, int32 &value, uint16 &delay) {
	if (amplitude == 0)
		return;
	if (delay != 0) {
		--delay;
		return;
	}
	value = addWrapped(value, step);
	if (value >= amplitude) {
		value = amplitude;
		step = negateWrapped(step);
	} else if (value <= negateWrapped(amplitude)) {
		value = negateWrapped(amplitude);
		step = negateWrapped(step);
	}
}

} // End of anonymous namespace

AtariPsgStream::Voice::Voice()
	: active(false), timed(false), toneEnabled(false), duration(0), period(0),
	  noisePeriod(0), volumeIndex(0),
	  volumeStage(0), pitchStage(0), noiseStage(0), volumeEnvelope(0),
	  volumeVibrato(0), volumeVibratoStep(0), pitchEnvelope(0),
	  pitchVibrato(0), pitchVibratoStep(0), noiseEnvelope(0),
	  noiseVibrato(0), noiseVibratoStep(0), volumeDelay(0), pitchDelay(0),
	  noiseDelay(0),
	  outputVolume(0) {
	Common::fill(program, program + sizeof(program), 0);
}

AtariPsgStream::AtariPsgStream(int outputRate, ResourceLoader loader,
		ResourceFreer freer, AtariPsgSink *sink)
	: _outputRate(outputRate), _loader(loader), _freer(freer),
	  _sink(sink),
	  _started(false), _retirementPending(false), _looping(false),
	  _fadeRequested(false), _musicLevel(15), _fadeCadence(0),
	  _rowOffset(0), _orderIndex(0), _orderCount(0), _timerData(1),
	  _synthAccumulator(0), _sequencerAccumulator(0),
	  _hardwareSequencerAccumulator(0),
	  _hardwareSequencerThreshold(kHardwareClockRate * kSequencerPrescale),
	  _sequencerDivider(0),
	  _musicVolume(255), _sfxVolume(255),
	  _samplePosition(0), _sampleLowNibble(false), _sampleLooping(false),
	  _sampleFeederActive(false), _sampleDacOwned(false), _paused(false),
	  _sampleNibble(15), _sampleAccumulator(0), _hardwareClock(false),
	  _clockRetirePending(false), _hardwareSynthDivider(0) {
	_name[0] = '\0';
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
	Common::fill(_registers, _registers + ARRAYSIZE(_registers), 0);
	if (!_sink)
		_sink = new AtariYm2149Sink(outputRate);
	_hardwareClock = _sink->hasHardwareClock();
	buildGainMap(kYmSingleVoiceAmplitude, ARRAYSIZE(kYmSingleVoiceAmplitude),
			_musicVolume, _musicGainMap);
	buildGainMap(kSampleDacAmplitude, ARRAYSIZE(kSampleDacAmplitude),
			_sfxVolume, _sampleGainMap);
	writeRegister(7, 0x3f);
	for (int channel = 0; channel < kNumChannels; ++channel)
		writeVolume(channel, 0);
}

AtariPsgStream::~AtariPsgStream() {
	unload();
	_sink->stopClock();
	delete _sink;
}

int32 AtariPsgStream::readSint32(const byte *data) {
	return (int32)READ_BE_UINT32(data);
}

bool AtariPsgStream::loadInternal(const char *name) {
	uint32 size = 0;
	byte *data = (*_loader)(name, &size);
	if (!data)
		return false;

	bool valid = size >= kPatternDataOffset &&
			size >= kInstrumentDataOffset + kNumInstruments * kInstrumentProgramSize;
	const uint8 orderCount = valid ? data[kOrderCountOffset] : 0;
	valid = valid && orderCount > 0 && orderCount <= 128 &&
			kOrderTableOffset + orderCount <= kInstrumentDataOffset;
	uint32 requiredSize = kPatternDataOffset;
	if (valid) {
		for (int i = 0; i < orderCount; ++i)
			requiredSize = MAX(requiredSize, (uint32)kPatternDataOffset +
					(uint32)(data[kOrderTableOffset + i] + 1) * kPatternSize);
		valid = requiredSize <= size;
	}
	if (!valid) {
		warning("Malformed Atari Cruise CORP module '%s' (%u bytes)", name, size);
		(*_freer)(data);
		return false;
	}

	_song.resize(size);
	Common::copy(data, data + size, _song.begin());
	(*_freer)(data);
	Common::strlcpy(_name, name, sizeof(_name));
	_orderCount = orderCount;
	// The native PSG initializer subtracts from 250 in a byte register.
	const byte timerByte = (byte)(250 - _song[kTempoOffset]);
	_timerData = timerByte ? timerByte : 256;
	_hardwareSequencerThreshold =
			kHardwareClockRate * kSequencerPrescale * _timerData;
	return true;
}

bool AtariPsgStream::load(const char *name) {
	AtariPsgLock lock(_mutex, _sink);
	_sink->stopClock();
	stopInternal();
	unloadInternal();
	const bool loaded = loadInternal(name);
	updateHardwareClock();
	return loaded;
}

void AtariPsgStream::play() {
	AtariPsgLock lock(_mutex, _sink);
	if (_song.empty())
		return;
	_sink->stopClock();
	stopSampleInternal();
	for (int channel = 0; channel < kNumChannels; ++channel)
		stopVoice(channel);
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
	_rowOffset = 0;
	_orderIndex = 0;
	_synthAccumulator = 0;
	_sequencerAccumulator = 0;
	_hardwareSequencerAccumulator = 0;
	_sequencerDivider = 0;
	_hardwareSynthDivider = 0;
	_clockRetirePending = false;
	_fadeRequested = false;
	_fadeCadence = 0;
	_musicLevel = 15;
	_started = true;
	_retirementPending = false;
	updateHardwareClock();
}

void AtariPsgStream::stopInternal() {
	_retirementPending = false;
	if (_started) {
		for (int channel = 0; channel < kNumChannels; ++channel)
			stopVoice(channel);
	}
	_started = false;
	_fadeRequested = false;
	_fadeCadence = 0;
	_musicLevel = 15;
}

void AtariPsgStream::stop() {
	AtariPsgLock lock(_mutex, _sink);
	stopInternal();
	updateHardwareClock();
}

void AtariPsgStream::unloadInternal() {
	stopInternal();
	_song.clear();
	_name[0] = '\0';
	_orderCount = 0;
}

void AtariPsgStream::writeSampleNibble(byte nibble) {
	_sampleNibble = nibble & 0x0f;
	for (int channel = 0; channel < kNumChannels; ++channel)
		writeRegister(8 + channel, kSampleDacTable[_sampleNibble][channel]);
}

void AtariPsgStream::sampleTick() {
	if (!_sampleFeederActive)
		return;
	if (_sampleLowNibble) {
		writeSampleNibble(_sampleData[_samplePosition] & 0x0f);
		_sampleLowNibble = false;
		++_samplePosition;
		if (_samplePosition >= _sampleData.size()) {
			if (_sampleLooping)
				_samplePosition = 0;
			else
				_sampleFeederActive = false;
		}
	} else {
		writeSampleNibble(_sampleData[_samplePosition] >> 4);
		_sampleLowNibble = true;
	}
}

void AtariPsgStream::stopSampleInternal() {
	_sampleFeederActive = false;
	_sampleDacOwned = false;
	_sampleData.clear();
	_samplePosition = 0;
	_sampleLowNibble = false;
	_sampleLooping = false;
	_sampleAccumulator = 0;
	writeSampleNibble(15);
	refreshRenderedVolumes();
}

bool AtariPsgStream::startSample(const byte *data, uint32 size, bool loop) {
	if (!data || size == 0)
		return false;
	AtariPsgLock lock(_mutex, _sink);
	if (_started)
		return false;
	_sink->stopClock();
	_sampleData.resize(size);
	Common::copy(data, data + size, _sampleData.begin());
	_samplePosition = 0;
	_sampleLowNibble = false;
	_sampleLooping = loop;
	_sampleFeederActive = true;
	_sampleDacOwned = true;
	_sampleAccumulator = 0;
	writeRegister(7, 0x3f);
	writeRegister(13, 0);
	_clockRetirePending = false;
	return updateHardwareClock();
}

void AtariPsgStream::stopSample() {
	AtariPsgLock lock(_mutex, _sink);
	stopSampleInternal();
	if (_started) {
		updateMixer();
		refreshRenderedVolumes();
	}
	updateHardwareClock();
}

bool AtariPsgStream::sampleActive() const {
	AtariPsgLock lock(_mutex, _sink);
	return _sampleFeederActive;
}

bool AtariPsgStream::sampleDacOwned() const {
	AtariPsgLock lock(_mutex, _sink);
	return _sampleDacOwned;
}

void AtariPsgStream::pause(bool pausedState) {
	AtariPsgLock lock(_mutex, _sink);
	if (_paused == pausedState)
		return;
	_paused = pausedState;
	if (_paused) {
		_sink->stopClock();
		for (int channel = 0; channel < kNumChannels; ++channel)
			_sink->setReg(8 + channel, 0);
	} else {
		for (int reg = 0; reg < 14; ++reg)
			_sink->setReg(reg, physicalRegisterValue(reg, _registers[reg]));
		updateHardwareClock();
	}
}

void AtariPsgStream::service() {
	if (!_hardwareClock || !_clockRetirePending)
		return;
	AtariPsgLock lock(_mutex, _sink);
	if (_clockRetirePending) {
		if (_started || _sampleFeederActive)
			updateHardwareClock();
		else {
			_sink->stopClock();
			_clockRetirePending = false;
		}
	}
}

void AtariPsgStream::unload() {
	AtariPsgLock lock(_mutex, _sink);
	_sink->stopClock();
	unloadInternal();
	updateHardwareClock();
}

void AtariPsgStream::retire() {
	AtariPsgLock lock(_mutex, _sink);
	if (!_retirementPending)
		return;
	_sink->stopClock();
	unloadInternal();
	_rowOffset = _orderIndex = 0;
	updateHardwareClock();
}

void AtariPsgStream::fade() {
	AtariPsgLock lock(_mutex, _sink);
	if (_started)
		_fadeRequested = true;
}

const byte *AtariPsgStream::instrumentProgram(int instrument) const {
	if (instrument < 0 || instrument >= kNumInstruments || _song.empty())
		return nullptr;
	return _song.begin() + kInstrumentDataOffset + instrument * kInstrumentProgramSize;
}

void AtariPsgStream::writeRegister(int reg, byte value) {
	if (reg < 0 || reg >= (int)ARRAYSIZE(_registers))
		return;
	_registers[reg] = value;
	_sink->setReg(reg, physicalRegisterValue(reg, value));
}

byte AtariPsgStream::physicalRegisterValue(int reg, byte value) const {
	if (!_hardwareClock || reg < 8 || reg > 10)
		return value;
	if (_paused)
		return 0;
	if (_sampleDacOwned)
		return kSampleDacTable[_sampleGainMap[_sampleNibble]][reg - 8];
	return _musicGainMap[MIN(15, (int)value)];
}

void AtariPsgStream::refreshPhysicalVolumes() {
	if (!_hardwareClock)
		return;
	for (int reg = 8; reg <= 10; ++reg)
		_sink->setReg(reg, physicalRegisterValue(reg, _registers[reg]));
}

void AtariPsgStream::writeTone(int channel, uint16 period) {
	period &= 0x0fff;
	writeRegister(channel * 2, period & 0xff);
	writeRegister(channel * 2 + 1, period >> 8);
}

void AtariPsgStream::writeVolume(int channel, byte volume) {
	writeRegister(8 + channel, MIN(15, (int)volume));
}

void AtariPsgStream::refreshRenderedVolumes() {
	for (int channel = 0; channel < kNumChannels; ++channel)
		writeVolume(channel, _voices[channel].active ? _voices[channel].outputVolume : 0);
}

void AtariPsgStream::updateMixer() {
	byte mixer = 0;
	for (int channel = 0; channel < kNumChannels; ++channel) {
		const Voice &voice = _voices[channel];
		if (!voice.active || !voice.toneEnabled)
			mixer |= 1 << channel;
		if (!voice.active || voice.noisePeriod < 0)
			mixer |= 8 << channel;
	}
	writeRegister(7, mixer);
}

void AtariPsgStream::startVoice(int channel, const byte *program, uint16 period) {
	if (!program || channel < 0 || channel >= kNumChannels)
		return;
	Voice &voice = _voices[channel];
	voice.active = false;
	voice.outputVolume = 0;
	writeVolume(channel, 0);
	Common::copy(program, program + kInstrumentProgramSize, voice.program);
	voice.duration = READ_BE_INT16(program);
	voice.toneEnabled = READ_BE_INT16(program + 2) >= 0;
	voice.period = READ_BE_INT16(program + 2);
	voice.noisePeriod = READ_BE_INT16(program + 4);
	voice.volumeIndex = _musicLevel;
	voice.volumeStage = READ_BE_INT16(program + 8);
	voice.pitchStage = READ_BE_INT16(program + 36);
	voice.noiseStage = READ_BE_INT16(program + 80);
	voice.volumeEnvelope = voice.volumeStage == 0 ? 0xf0000 : 0;
	voice.volumeVibrato = 0;
	voice.volumeVibratoStep = readSint32(program + 30);
	voice.pitchEnvelope = 0;
	voice.pitchVibrato = 0;
	voice.pitchVibratoStep = readSint32(program + 62);
	voice.noiseEnvelope = 0;
	voice.noiseVibrato = 0;
	voice.noiseVibratoStep = readSint32(program + 106);
	voice.volumeDelay = READ_BE_UINT16(program + 34);
	voice.pitchDelay = READ_BE_UINT16(program + 78);
	voice.noiseDelay = READ_BE_UINT16(program + 110);
	voice.outputVolume = 0;
	voice.timed = (int16)period < 0;
	voice.active = voice.duration != 0;
	if (!voice.active)
		return;
	if (voice.toneEnabled) {
		voice.period = (int16)period;
		writeRegister(channel * 2, period & 0xff);
		// A negative argument queries the native register helper (0x22964).
		if (voice.period >= 0)
			writeRegister(channel * 2 + 1, (period >> 8) & 0x0f);
	}
	if (voice.noisePeriod >= 0)
		writeRegister(6, MIN(31, (int)voice.noisePeriod));
	updateMixer();
	if (voice.volumeStage == 0) {
		voice.outputVolume = MIN(15, (int)voice.volumeIndex);
		writeVolume(channel, voice.outputVolume);
	}
}

void AtariPsgStream::stopVoice(int channel) {
	if (channel < 0 || channel >= kNumChannels)
		return;
	Voice &voice = _voices[channel];
	voice.active = false;
	voice.outputVolume = 0;
	writeVolume(channel, 0);
	updateMixer();
}

bool AtariPsgStream::updateEnvelope(Voice &voice) {
	if (voice.volumeStage == 1) {
		voice.volumeEnvelope = addWrapped(voice.volumeEnvelope, readSint32(voice.program + 10));
		if (voice.volumeEnvelope > 0xeffff) {
			voice.volumeEnvelope = 0xf0000;
			voice.volumeStage = 2;
		}
	} else if (voice.volumeStage == 2) {
		voice.volumeEnvelope = addWrapped(voice.volumeEnvelope, readSint32(voice.program + 14));
		const int32 target = readSint32(voice.program + 18);
		if (voice.volumeEnvelope <= target) {
			voice.volumeEnvelope = target;
			voice.volumeStage = 3;
		}
	} else if (voice.volumeStage == 4) {
		voice.volumeEnvelope = addWrapped(voice.volumeEnvelope, readSint32(voice.program + 22));
		if (voice.volumeEnvelope < 1) {
			voice.volumeEnvelope = 0;
			voice.volumeStage = 0;
			voice.duration = 1;
		}
	}
	advanceVibrato(readSint32(voice.program + 26), voice.volumeVibratoStep,
			voice.volumeVibrato, voice.volumeDelay);
	// Native output gates use the updated stage and amplitude's high word.
	if (voice.volumeStage == 0 && READ_BE_UINT16(voice.program + 26) == 0)
		return false;
	const int32 total = addWrapped(voice.volumeEnvelope, voice.volumeVibrato);
	if (total < 0) {
		voice.outputVolume = 0;
	} else {
		const int32 scaled = kVolumeTable[MAX(0, MIN(15, (int)voice.volumeIndex))] *
				(int16)(total >> 8);
		voice.outputVolume = MAX(0, MIN(15, (int)(scaled >> 16)));
	}
	return true;
}

void AtariPsgStream::advancePitchVibrato(Voice &voice) {
	const int32 amplitude = readSint32(voice.program + 58);
	if (amplitude == 0)
		return;
	if (voice.pitchDelay != 0) {
		--voice.pitchDelay;
		return;
	}
	const uint32 oldValue = (uint32)voice.pitchVibrato;
	const uint32 delta = (uint32)voice.pitchVibratoStep;
	const uint32 sum = oldValue + delta;
	const bool carry = sum < oldValue;
	if (voice.pitchVibratoStep < 0) {
		const int32 lower = readSint32(voice.program + 70);
		if (!carry)
			voice.pitchVibratoStep = readSint32(voice.program + 74);
		if ((int32)sum <= lower) {
			voice.pitchVibratoStep = negateWrapped(voice.pitchVibratoStep);
			voice.pitchVibrato = lower;
		} else {
			voice.pitchVibrato = (int32)sum;
		}
	} else {
		if (carry)
			voice.pitchVibratoStep = readSint32(voice.program + 66);
		if ((int32)sum >= amplitude) {
			voice.pitchVibratoStep = negateWrapped(voice.pitchVibratoStep);
			voice.pitchVibrato = amplitude;
		} else {
			voice.pitchVibrato = (int32)sum;
		}
	}
}

void AtariPsgStream::updatePitch(Voice &voice, int channel) {
	voice.pitchStage = advanceStage(voice.pitchStage, voice.pitchEnvelope,
			readSint32(voice.program + 38), readSint32(voice.program + 42),
			readSint32(voice.program + 46), readSint32(voice.program + 50),
			readSint32(voice.program + 54));
	advancePitchVibrato(voice);
	if (!voice.toneEnabled ||
			(voice.pitchStage == 0 && READ_BE_UINT16(voice.program + 58) == 0))
		return;
	const int16 scale = (int16)(addWrapped(voice.pitchEnvelope, voice.pitchVibrato) >> 16);
	const int32 product = (int32)scale * (int16)voice.period;
	const uint32 shifted = (uint32)product << 4;
	const uint32 swapped = (shifted << 16) | (shifted >> 16);
	uint16 adjustmentWord = (uint16)swapped;
	if ((int32)swapped < 0)
		++adjustmentWord;
	const int16 adjustment = (int16)adjustmentWord;
	// Native ADD.W wraps before the signed clamp at 0x227AE.
	const int16 period = (int16)(uint16)(voice.period + adjustment);
	writeTone(channel, CLIP<int>(period, 0, 0x0fff));
}

void AtariPsgStream::updateNoise(Voice &voice) {
	voice.noiseStage = advanceStage(voice.noiseStage, voice.noiseEnvelope,
			readSint32(voice.program + 82), readSint32(voice.program + 86),
			readSint32(voice.program + 90), readSint32(voice.program + 94),
			readSint32(voice.program + 98));
	advanceVibrato(readSint32(voice.program + 102), voice.noiseVibratoStep,
			voice.noiseVibrato, voice.noiseDelay);
	if (voice.noisePeriod >= 0 &&
			(voice.noiseStage != 0 || READ_BE_UINT16(voice.program + 102) != 0)) {
		const int16 value = (int16)(uint16)(voice.noisePeriod +
				(addWrapped(voice.noiseEnvelope, voice.noiseVibrato) >> 16));
		// ADD.W/sign test precedes the native signed CMP.B against 31.
		const byte output = value < 0 ? 0 : ((int8)value > 31 ? 31 : (byte)value);
		writeRegister(6, output);
	}
}

void AtariPsgStream::synthTick() {
	for (int channel = kNumChannels - 1; channel >= 0; --channel) {
		Voice &voice = _voices[channel];
		if (!voice.active)
			continue;
		if (updateEnvelope(voice))
			writeVolume(channel, voice.outputVolume);
		updatePitch(voice, channel);
		updateNoise(voice);
		if (voice.timed && --voice.duration == 0) {
			if (voice.volumeStage == 0) {
				voice.outputVolume = 0;
				writeVolume(channel, 0);
				voice.active = false;
			} else {
				--voice.duration;
				voice.volumeStage = 4;
				if (voice.pitchStage != 0) {
					voice.pitchStage = 4;
					int32 release = readSint32(voice.program + 54);
					if ((release ^ voice.pitchEnvelope) >= 0)
						release = negateWrapped(release);
					WRITE_BE_UINT32(voice.program + 54, (uint32)release);
				}
				if (voice.noiseStage != 0) {
					voice.noiseStage = 4;
					int32 release = readSint32(voice.program + 98);
					if ((release ^ voice.noiseEnvelope) >= 0)
						release = negateWrapped(release);
					WRITE_BE_UINT32(voice.program + 98, (uint32)release);
				}
			}
		}
	}
}

void AtariPsgStream::processChannel(int channel, const byte *event) {
	const uint16 period = READ_BE_UINT16(event);
	const int instrument = (event[2] >> 4) - 1;
	if (instrument >= 0 && instrument < kNumInstruments)
		_channelInstrument[channel] = instrument;
	if (period != 0 && _channelInstrument[channel] >= 0)
		startVoice(channel, instrumentProgram(_channelInstrument[channel]), period);
}

void AtariPsgStream::trackerRow() {
	if (!_started || _song.empty() || _orderCount == 0 ||
			!validPosition(_rowOffset, _orderIndex))
		return;
	const uint32 pattern = _song[kOrderTableOffset + _orderIndex];
	const uint32 row = kPatternDataOffset + pattern * kPatternSize + _rowOffset;
	if (row + kRowSize > _song.size()) {
		stopInternal();
		return;
	}
	for (int channel = 0; channel < kNumChannels; ++channel)
		processChannel(channel, _song.begin() + row + channel * 4);
	_rowOffset += kRowSize;
	if (_rowOffset >= kPatternSize) {
		_rowOffset = 0;
		++_orderIndex;
		if (_orderIndex >= _orderCount)
			_orderIndex = 0;
	}
}

void AtariPsgStream::sequencerTick() {
	if (!_started)
		return;
	if (_fadeRequested && ++_fadeCadence > 31) {
		_fadeCadence = 0;
		if (--_musicLevel <= 0) {
			for (int channel = 0; channel < kNumChannels; ++channel)
				stopVoice(channel);
			_started = false;
			_musicLevel = 0;
			_retirementPending = true;
			return;
		}
	}
	if (++_sequencerDivider == 12) {
		_sequencerDivider = 0;
		trackerRow();
	}
}

int AtariPsgStream::readBuffer(int16 *buffer, const int numSamples) {
	if (_hardwareClock) {
		Common::fill(buffer, buffer + numSamples, 0);
		return numSamples;
	}
	AtariPsgLock lock(_mutex, _sink);

	int done = 0;
	while (done < numSamples) {
		if (_sampleFeederActive) {
			_sampleAccumulator += kSampleRate;
			while (_sampleAccumulator >= (uint64)_outputRate && _sampleFeederActive) {
				_sampleAccumulator -= _outputRate;
				sampleTick();
			}
		}
		if (_sampleDacOwned) {
			_synthAccumulator = 0;
		} else {
			_synthAccumulator += kSynthRate;
			while (_synthAccumulator >= (uint64)_outputRate) {
				_synthAccumulator -= _outputRate;
				synthTick();
			}
		}
		if (_started) {
			_sequencerAccumulator += kAtariMfpClock;
			const uint64 threshold = (uint64)_outputRate * kSequencerPrescale * _timerData;
			while (_sequencerAccumulator >= threshold) {
				_sequencerAccumulator -= threshold;
				sequencerTick();
			}
		}
		_sink->readBuffer(buffer + done, 1);
		const byte hostVolume = _sampleDacOwned ? _sfxVolume : _musicVolume;
		buffer[done] = (int16)((int32)buffer[done] * hostVolume / 255);
		++done;
	}
	return done;
}

void AtariPsgStream::hardwareClockCallback(void *ref) {
	static_cast<AtariPsgStream *>(ref)->hardwareTick();
}

void AtariPsgStream::hardwareTick() {
	if (_sampleFeederActive)
		sampleTick();

	if (!_started && !_sampleFeederActive) {
		_sink->suspendClock();
		_clockRetirePending = true;
		return;
	}

	if (!_sampleDacOwned &&
			++_hardwareSynthDivider == kHardwareClockRate / kSynthRate) {
		_hardwareSynthDivider = 0;
		synthTick();
	}

	if (_started) {
		_hardwareSequencerAccumulator += kAtariMfpClock;
		while (_hardwareSequencerAccumulator >= _hardwareSequencerThreshold) {
			_hardwareSequencerAccumulator -= _hardwareSequencerThreshold;
			sequencerTick();
		}
	}

	if (!_started && !_sampleFeederActive) {
		_sink->suspendClock();
		_clockRetirePending = true;
	}
}

bool AtariPsgStream::updateHardwareClock() {
	if (!_hardwareClock)
		return true;
	if (_paused || (!_started && !_sampleFeederActive)) {
		_sink->stopClock();
		_clockRetirePending = false;
		return true;
	}
	if (_sink->startClock(hardwareClockCallback, this)) {
		_clockRetirePending = false;
		return true;
	}
	handleClockStartFailure();
	return false;
}

void AtariPsgStream::handleClockStartFailure() {
	_started = false;
	_fadeRequested = false;
	_fadeCadence = 0;
	for (int channel = 0; channel < kNumChannels; ++channel) {
		_voices[channel].active = false;
		_voices[channel].outputVolume = 0;
		_registers[8 + channel] = 0;
		_sink->setReg(8 + channel, 0);
	}
	_sampleFeederActive = false;
	_sampleDacOwned = false;
	_sampleData.clear();
	_samplePosition = 0;
	_sampleLowNibble = false;
	_sampleLooping = false;
	_sampleAccumulator = 0;
	_sampleNibble = 15;
	_clockRetirePending = false;
	_sink->stopClock();
}

bool AtariPsgStream::validPosition(int rowOffset, int orderIndex) const {
	return rowOffset >= 0 && rowOffset < kPatternSize &&
			(rowOffset % kRowSize) == 0 && orderIndex >= 0 &&
			orderIndex < _orderCount;
}

bool AtariPsgStream::loaded() const {
	AtariPsgLock lock(_mutex, _sink);
	return !_song.empty();
}

bool AtariPsgStream::started() const {
	AtariPsgLock lock(_mutex, _sink);
	return _started;
}

int AtariPsgStream::fadeLevel() const {
	AtariPsgLock lock(_mutex, _sink);
	return _musicLevel;
}

int AtariPsgStream::rowOffset() const {
	AtariPsgLock lock(_mutex, _sink);
	return _rowOffset;
}

int AtariPsgStream::orderIndex() const {
	AtariPsgLock lock(_mutex, _sink);
	return _orderIndex;
}

uint8 AtariPsgStream::orderCount() const {
	AtariPsgLock lock(_mutex, _sink);
	return _orderCount;
}

void AtariPsgStream::setOrderCount(uint8 count) {
	AtariPsgLock lock(_mutex, _sink);
	if (!_song.empty() && count > 0 && count <= 128 &&
			kOrderTableOffset + count <= kInstrumentDataOffset)
		_orderCount = count;
}

uint8 AtariPsgStream::moduleOrderCount() const {
	AtariPsgLock lock(_mutex, _sink);
	return _song.empty() ? 0 : _song[kOrderCountOffset];
}

void AtariPsgStream::setModuleOrderCount(uint8 count) {
	AtariPsgLock lock(_mutex, _sink);
	if (_song.empty() || count == 0 ||
			kOrderTableOffset + count > kInstrumentDataOffset)
		return;
	for (int i = 0; i < count; ++i) {
		if (kPatternDataOffset + ((uint32)_song[kOrderTableOffset + i] + 1) *
				kPatternSize > _song.size())
			return;
	}
	_song[kOrderCountOffset] = count;
}

void AtariPsgStream::setPattern(int offset, uint8 pattern) {
	AtariPsgLock lock(_mutex, _sink);
	if (!_song.empty() && offset >= 0 &&
			offset < kInstrumentDataOffset - kOrderTableOffset &&
			kPatternDataOffset + ((uint32)pattern + 1) * kPatternSize <= _song.size())
		_song[kOrderTableOffset + offset] = pattern;
}

void AtariPsgStream::setLooping(int16 looping) {
	AtariPsgLock lock(_mutex, _sink);
	_looping = looping;
}

int16 AtariPsgStream::looping() const {
	AtariPsgLock lock(_mutex, _sink);
	return _looping;
}

const char *AtariPsgStream::name() const {
	return _name;
}

bool AtariPsgStream::canReuse(const char *name) const {
	AtariPsgLock lock(_mutex, _sink);
	return !_song.empty() && !strcmp(_name, name) && !_fadeRequested && !_retirementPending;
}

void AtariPsgStream::getSaveState(SoundInternal::MusicState &state) const {
	AtariPsgLock lock(_mutex, _sink);
	Common::strlcpy(state.name, _name, sizeof(state.name));
	state.loaded = !_song.empty();
	state.started = _started;
	state.playing = _started && !_fadeRequested;
	state.looping = _looping;
	state.row = _rowOffset;
	state.order = _orderIndex;
	if (_retirementPending)
		state.retire();
}

void AtariPsgStream::restoreStoppedPosition(int rowOffset, int orderIndex) {
	AtariPsgLock lock(_mutex, _sink);
	_sink->stopClock();
	_retirementPending = false;
	// WORKAROUND: Savegame version 1 lacks YM voice, envelope, sample and
	// timer phase state. Restore stopped; the host restarts active music from
	// the beginning after resource reconstruction instead of resuming
	// incomplete voices.
	_rowOffset = validPosition(rowOffset, orderIndex) ? rowOffset : 0;
	_orderIndex = validPosition(rowOffset, orderIndex) ? orderIndex : 0;
	_started = false;
	_fadeRequested = false;
	_fadeCadence = 0;
	_musicLevel = 15;
	_synthAccumulator = 0;
	_sequencerAccumulator = 0;
	_hardwareSequencerAccumulator = 0;
	_sequencerDivider = 0;
	_hardwareSynthDivider = 0;
	_clockRetirePending = false;
	for (int channel = 0; channel < kNumChannels; ++channel) {
		stopVoice(channel);
		_channelInstrument[channel] = -1;
	}
	updateHardwareClock();
}

SoundInternal::ReplacementWaitAction AtariPsgStream::recheckReplacementWait(
		int previousFade, uint32 elapsed, uint32 stallTimeout,
		int &currentFade) const {
	AtariPsgLock lock(_mutex, _sink);
	currentFade = _started ? _musicLevel : 0;
	return SoundInternal::checkReplacementWait(previousFade, currentFade,
			elapsed, stallTimeout);
}

void AtariPsgStream::syncVolume(byte musicVolume) {
	byte gainMap[ARRAYSIZE(_musicGainMap)];
	buildGainMap(kYmSingleVoiceAmplitude, ARRAYSIZE(kYmSingleVoiceAmplitude),
			musicVolume, gainMap);
	AtariPsgLock lock(_mutex, _sink);
	_musicVolume = musicVolume;
	Common::copy(gainMap, gainMap + ARRAYSIZE(gainMap), _musicGainMap);
	if (!_sampleDacOwned)
		refreshPhysicalVolumes();
}

void AtariPsgStream::syncSfxVolume(byte sfxVolume) {
	byte gainMap[ARRAYSIZE(_sampleGainMap)];
	buildGainMap(kSampleDacAmplitude, ARRAYSIZE(kSampleDacAmplitude), sfxVolume,
			gainMap);
	AtariPsgLock lock(_mutex, _sink);
	_sfxVolume = sfxVolume;
	Common::copy(gainMap, gainMap + ARRAYSIZE(gainMap), _sampleGainMap);
	if (_sampleDacOwned)
		refreshPhysicalVolumes();
}

byte AtariPsgStream::registerValue(int reg) const {
	AtariPsgLock lock(_mutex, _sink);
	return reg >= 0 && reg < (int)ARRAYSIZE(_registers) ? _registers[reg] : 0;
}

} // End of namespace Cruise
