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

#ifndef CRUISE_SOUND_ATARI_H
#define CRUISE_SOUND_ATARI_H

#include "audio/audiostream.h"
#include "audio/mididrv.h"

#include "common/array.h"
#include "common/mutex.h"
#include "common/scummsys.h"

#include "cruise/sound/state.h"

namespace Audio {
class Mixer;
}

namespace Cruise {

class CruiseEngine;
class Sound;

class AtariPsgSink {
public:
	typedef void (*ClockProc)(void *ref);

	virtual ~AtariPsgSink() {}
	virtual void setReg(int reg, byte value) = 0;
	virtual int readBuffer(int16 *buffer, int count) = 0;
	virtual bool hasHardwareClock() const { return false; }
	virtual bool startClock(ClockProc, void *) { return false; }
	virtual void suspendClock() {}
	virtual void stopClock() {}
	virtual void beginUpdate() {}
	virtual void endUpdate() {}
};

class AtariTimerSink {
public:
	typedef void (*TimerProc)(void *ref);
	virtual ~AtariTimerSink() {}
	virtual bool install(TimerProc proc, uint32 interval, void *ref,
			const char *name) = 0;
	virtual void remove(TimerProc proc) = 0;
};

class AtariMidiSink {
public:
	virtual ~AtariMidiSink() {}
	virtual void send(byte status, byte first, byte second) = 0;
	virtual void sysEx(const byte *data, uint16 size) = 0;
};

class AtariPsgStream : public Audio::AudioStream {
public:
	typedef byte *(*ResourceLoader)(const char *name, uint32 *size);
	typedef void (*ResourceFreer)(void *data);
	enum {
		kNumChannels = 3,
		kNumInstruments = 15,
		kInstrumentDataOffset = 600,
		kInstrumentProgramSize = 120,
		kPatternDataOffset = 2400,
		kPatternSize = 1024,
		kRowSize = 16,
		kOrderCountOffset = 470,
		kTempoOffset = 471,
		kOrderTableOffset = 472
	};

	AtariPsgStream(int outputRate, ResourceLoader loader, ResourceFreer freer,
			AtariPsgSink *sink = nullptr);
	~AtariPsgStream() override;

	bool load(const char *name);
	void play();
	void stop();
	void unload();
	void retire();
	void getSaveState(SoundInternal::MusicState &state) const;
	void fade();

	bool loaded() const;
	bool started() const;
	int fadeLevel() const;
	int rowOffset() const;
	int orderIndex() const;
	uint8 orderCount() const;
	void setOrderCount(uint8 count);
	// DOS scripts edit the module; PlaySong latches its count separately.
	uint8 moduleOrderCount() const;
	void setModuleOrderCount(uint8 count);
	void setPattern(int offset, uint8 pattern);
	void setLooping(int16 looping);
	int16 looping() const;
	const char *name() const;
	bool canReuse(const char *name) const;
	void restoreStoppedPosition(int rowOffset, int orderIndex);
	SoundInternal::ReplacementWaitAction recheckReplacementWait(int previousFade,
			uint32 elapsed, uint32 stallTimeout, int &currentFade) const;
	void syncVolume(byte musicVolume);
	void syncSfxVolume(byte sfxVolume);
	bool startSample(const byte *data, uint32 size, bool loop);
	void stopSample();
	bool sampleActive() const;
	bool sampleDacOwned() const;
	void pause(bool paused);
	void service();

	int readBuffer(int16 *buffer, const int numSamples) override;
	bool isStereo() const override { return false; }
	bool endOfData() const override { return false; }
	int getRate() const override { return _outputRate; }

	byte registerValue(int reg) const;

private:
	struct Voice {
		bool active;
		bool timed;
		bool toneEnabled;
		byte program[kInstrumentProgramSize];
		int16 duration;
		int16 period;
		int16 noisePeriod;
		int16 volumeIndex;
		int volumeStage;
		int pitchStage;
		int noiseStage;
		int32 volumeEnvelope;
		int32 volumeVibrato;
		int32 volumeVibratoStep;
		int32 pitchEnvelope;
		int32 pitchVibrato;
		int32 pitchVibratoStep;
		int32 noiseEnvelope;
		int32 noiseVibrato;
		int32 noiseVibratoStep;
		uint16 volumeDelay;
		uint16 pitchDelay;
		uint16 noiseDelay;
		byte outputVolume;

		Voice();
	};

	bool loadInternal(const char *name);
	void stopInternal();
	void unloadInternal();
	void synthTick();
	void sequencerTick();
	void trackerRow();
	void processChannel(int channel, const byte *event);
	void startVoice(int channel, const byte *program, uint16 period);
	void stopVoice(int channel);
	bool updateEnvelope(Voice &voice);
	void updatePitch(Voice &voice, int channel);
	void updateNoise(Voice &voice);
	void advancePitchVibrato(Voice &voice);
	void updateMixer();
	void writeTone(int channel, uint16 period);
	void writeVolume(int channel, byte volume);
	void writeRegister(int reg, byte value);
	void refreshRenderedVolumes();
	void refreshPhysicalVolumes();
	byte physicalRegisterValue(int reg, byte value) const;
	bool updateHardwareClock();
	void handleClockStartFailure();
	void hardwareTick();
	static void hardwareClockCallback(void *ref);
	void writeSampleNibble(byte nibble);
	void sampleTick();
	void stopSampleInternal();
	bool validPosition(int rowOffset, int orderIndex) const;
	const byte *instrumentProgram(int instrument) const;
	static int32 readSint32(const byte *data);

	int _outputRate;
	ResourceLoader _loader;
	ResourceFreer _freer;
	AtariPsgSink *_sink;
	mutable Common::Mutex _mutex;
	char _name[33];
	Common::Array<byte> _song;
	Voice _voices[kNumChannels];
	int _channelInstrument[kNumChannels];
	byte _registers[16];
	bool _started;
	bool _retirementPending;
	int16 _looping;
	bool _fadeRequested;
	int _musicLevel;
	int _fadeCadence;
	int _rowOffset;
	int _orderIndex;
	uint8 _orderCount;
	uint16 _timerData;
	uint64 _synthAccumulator;
	uint64 _sequencerAccumulator;
	uint32 _hardwareSequencerAccumulator;
	uint32 _hardwareSequencerThreshold;
	int _sequencerDivider;
	byte _musicVolume;
	byte _sfxVolume;
	byte _musicGainMap[16];
	byte _sampleGainMap[16];
	Common::Array<byte> _sampleData;
	uint32 _samplePosition;
	bool _sampleLowNibble;
	bool _sampleLooping;
	bool _sampleFeederActive;
	bool _sampleDacOwned;
	bool _paused;
	byte _sampleNibble;
	uint64 _sampleAccumulator;
	bool _hardwareClock;
	volatile bool _clockRetirePending;
	int _hardwareSynthDivider;
};

class AtariMidiPlayer {
public:
	typedef byte *(*ResourceLoader)(const char *name, uint32 *size);
	typedef void (*ResourceFreer)(void *data);

	enum {
		kNumChannels = 4,
		kNumInstruments = 15,
		kInstrumentNameOffset = 20,
		kInstrumentRecordSize = 30,
		kInstrumentNameSize = 22,
		kPatternDataOffset = 2400,
		kPatternSize = 1024,
		kRowSize = 16,
		kOrderCountOffset = 470,
		kTempoOffset = 471,
		kOrderTableOffset = 472,
		kH32SelectorOffset = 22,
		kH32CustomDataOffset = 23,
		kH32CustomDataSize = 246,
		kH32CustomResourceSize = 279
	};

	AtariMidiPlayer(AtariMidiSink *sink, ResourceLoader loader, ResourceFreer freer,
			AtariTimerSink *timer = nullptr);
	~AtariMidiPlayer();

	bool load(const char *name);
	void play();
	void stop();
	void unload();
	void retire();
	void getSaveState(SoundInternal::MusicState &state) const;
	void fade();
	void pause(bool paused);
	void syncVolume(byte musicVolume);

	bool loaded() const;
	bool started() const;
	int fadeLevel() const;
	uint8 orderCount() const;
	void setOrderCount(uint8 count);
	void setLooping(int16 looping);
	int16 looping() const;
	bool paused() const;
	const char *name() const;
	void restoreStoppedPosition(int rowOffset, int orderIndex);
	SoundInternal::ReplacementWaitAction recheckReplacementWait(int previousFade,
			uint32 elapsed, uint32 stallTimeout, int &currentFade) const;
	void timerTick();

private:
	static void timerCallback(void *ref);
	bool loadInternal(const char *name);
	void playInternal();
	void stopInternal();
	void unloadInternal();
	void installTimer();
	void removeTimer();
	void trackerRow();
	void processChannel(int channel, const byte *event);
	void stopChannel(int channel);
	void programChannel(int channel, int instrument);
	void sendDT1(byte address0, byte address1, byte address2,
			const byte *data, uint16 size);
	void uploadCustomTimbre(int instrument);
	void sendPatchTemporary(int channel, int group, int number, int volume);
	void refreshVolumes();
	bool validPosition(int rowOffset, int orderIndex) const;
	int periodToNote(uint16 period) const;
	int renderedVolume(int nativeVolume) const;

	AtariMidiSink *_sink;
	AtariTimerSink *_timer;
	ResourceLoader _loader;
	ResourceFreer _freer;
	mutable Common::Mutex _mutex;
	char _name[33];
	Common::Array<byte> _song;
	Common::Array<byte> _instruments[kNumInstruments];
	int _channelInstrument[kNumChannels];
	bool _timerInstalled;
	bool _paused;
	bool _started;
	bool _retirementPending;
	int16 _looping;
	int _fade;
	int _rowOffset;
	int _orderIndex;
	uint8 _orderCount;
	uint16 _timerData;
	int _timerDivider;
	byte _musicVolume;
};

Sound *createAtariSound(Audio::Mixer *mixer, CruiseEngine *vm, AtariMidiSink *sink);
bool shouldUseAtariMidi(MusicType musicType, bool nativeMT32,
		bool explicitSelection);
bool shouldCreateAtariMidi(MusicType musicType, bool nativeMT32,
		bool explicitSelection, bool available);

} // End of namespace Cruise

#endif
