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

#ifndef MADS_DRAGONSPHERE_SOUND_GSOUND_H
#define MADS_DRAGONSPHERE_SOUND_GSOUND_H

#include "audio/mt32gm.h"
#include "common/util.h"
#include "mads/core/native_sound_timer.h"
#include "mads/core/sound_manager.h"

namespace MADS {
namespace Dragonsphere {
namespace Sound {

enum {
	GSOUND_CHANNEL_COUNT = 9,
	GSOUND_HELD_NOTE_COUNT = 4,
	GSOUND_SCRIPT_VARIABLE_COUNT = 32,
	GSOUND_EXPORT_COUNT = 11
};

struct GSoundDriverData {
	const char *filename;
	uint32 fileSize;
	const char *md5First8192;
	uint16 dataParagraph;
	uint16 dataSize;
	uint16 initializedDataSize;
	uint16 exports[GSOUND_EXPORT_COUNT];
	byte commandMax[5];
	byte section;
};

struct GSoundChannelRoot {
	byte channel;
	uint16 offset;
};

enum GSoundCommandMode {
	kGSoundNoOp,
	kGSoundMusic,
	kGSoundDirectMusic,
	kGSoundEffect78,
	kGSoundEffectAny,
	kGSoundEffectChannel8,
	kGSoundDirectChannels,
	kGSoundSpecial
};

enum GSoundCommandFlags {
	kGSoundCheckFirstRoot = 1 << 0,
	kGSoundDeferWhileActive = 1 << 1,
	kGSoundStopMusic = 1 << 2,
	kGSoundStopAll = 1 << 3,
	kGSoundSetMusicIndex = 1 << 4,
	kGSoundResetAll = 1 << 5
};

/**
 * One command recovered from an individual Dragonsphere GSOUND overlay.
 *
 * The tables deliberately retain channel numbers and data offsets rather
 * than translating them to a common MADS sound family. GSOUND's bytecode and
 * command ABI are not interchangeable with RSOUND or PSOUND.
 */
struct GSoundCommandSpec {
	byte command;
	GSoundCommandMode mode;
	byte flags;
	byte musicIndex;
	uint16 timerCounter;
	uint16 timerPeriod;
	byte guardCount;
	uint16 guards[3];
	byte rootCount;
	GSoundChannelRoot roots[9];
};

class GSound;

/** Native 0x28-byte GSOUND channel record represented in typed C++ state. */
class GSoundChannel {
public:
	GSound *_owner = nullptr;
	int _midiChannel = 0;

	int _activeCount = 0;
	int _pitchBendFadeStep = 0;
	int _volumeFadeStep = 0;
	int _panFadeStep = 0;
	int _note = 0;
	int _program = 0;
	int _velocity = 0;
	int _noteOffset = 0;
	int _keyOnDelayOverride = 0;
	int _keyOnDelay = 0;
	int _volumeFadeCounter = 0;
	int _volumeFadeReload = 0;
	int _pitchBendFadeCounter = 0;
	int _panFadeCounter = 0;
	int _panFadeReload = 0;
	int _pan = 0;
	int _volume = 0;
	int _pitchBend = 0;
	int _pitchBendFadeReload = 0;
	int _pitchBendFadeCount = 0;
	byte *_loopStartPtr = nullptr;
	byte *_pSrc = nullptr;
	byte *_innerLoopPtr = nullptr;
	byte *_outerLoopPtr = nullptr;
	uint16 _innerLoopCount = 0;
	uint16 _outerLoopCount = 0;
	byte *_soundData = nullptr;
	byte *_branchTarget = nullptr;
	int _field24 = 0;
	int _transpose = 0;
	int _pendingStop = 0;
	int _field27 = 0;

	void reset(byte *startPtr);
	void load(byte *startPtr);
	void enableFade(byte flag);
};

/**
 * Interpreter for Dragonsphere's native General MIDI `GSOUND.DR*` family.
 *
 * This class is intentionally independent of RSound. It shares only the
 * common native host timer and ScummVM's MIDI interface. The original DOS
 * MPU-401 probing and interrupt installation are replaced by MidiDriver.
 */
class GSound : public SoundDriver {
	friend class GSoundChannel;

private:
	const GSoundDriverData &_driverData;
	MidiDriver_MT32GM *_midiDriver = nullptr;
	uint32 _driverCallbackDelta = 0;
	NativeSoundTimer _hostTimer;
	uint16 _randomSeed = 1234;
	uint16 _stateChanged = 0;
	byte _heldNotes[GSOUND_CHANNEL_COUNT + 1][GSOUND_HELD_NOTE_COUNT];
	byte _scriptVariables[GSOUND_SCRIPT_VARIABLE_COUNT];
	byte _silenceStream[2];
	uint16 _callbackCounter = 0;
	uint16 _callbackPeriod = 0;
	int _deferredCommand = -1;
	uint16 _musicIndex = 0;
	int _fadeCounter = 0;
	int _fadePeriod = 0;
	// These fields are writable by the native stream grammar, but the audited
	// overlays contain no reader or service routine for them. Retain their
	// state without inventing behavior that the driver did not execute.
	int _clockUnknown = 0;
	int _clockCoarseTarget = 0;
	int _clockMediumTarget = 0;
	int _clockFine = 0;
	int _clockCoarse = 0;
	int _clockMedium = 0;
	bool _clockEnabled1 = false;
	bool _clockEnabled2 = false;
	int _masterVolume = 255;
	bool _isReady = false;

	void update();
	void pollChannel(GSoundChannel &channel);
	void tickDeferredCommand();
	void checkFades();
	void checkFade(GSoundChannel &channel);
	void flushHeldNotes(GSoundChannel &channel);
	void applyFades(GSoundChannel &channel);

	int8 readSignedByte(byte *&pSrc);
	byte readByte(byte *&pSrc);
	uint16 readWord(byte *&pSrc);
	byte *readRoot(byte *&pSrc);
	byte *dataAt(uint16 offset);
	bool contains(const byte *ptr, uint32 count = 1) const;

	void sendNoteOn(int channel, int note, int velocity);
	void sendProgramChange(int channel, int program);
	void sendControlChange(int channel, int controller, int value);
	void sendPitchBend(int channel, int value);
	void sendVolume(GSoundChannel &channel);
	int scaleVolume(int volume) const;
	void sendPan(GSoundChannel &channel);
	void setPitchBendSensitivity(int channel, int semitones);
	void resetMidiChannels();

	void resetChannels();
	void resetMusicChannels();
	void resetEffectChannels();
	void stopMusicChannels();
	void stopEffectChannels();
	bool anyChannelActive() const;
	bool isSoundActive(uint16 offset) const;
	GSoundChannel *playEffect78(uint16 offset);
	GSoundChannel *playEffectAny(uint16 offset);
	GSoundChannel *playEffectChannel8(uint16 offset);
	GSoundChannel *allocateChannel(uint16 offset, int first, int last);
	void loadRoots(const GSoundCommandSpec &spec);
	int executeSpec(const GSoundCommandSpec &spec, bool fromDeferred);
	void armTimer(uint16 counter, uint16 period);
	void deferCommand(int command);

	int command0();
	int command1();
	int command2();
	int command3();
	int command4();
	int command5();
	int command6();
	int command7();
	int command8();

	void onTimer();
	static void timerCallback(void *data);

protected:
	static int scaleMidiVolume(int volume, int masterVolume) {
		return CLIP(volume, 0, 127) * CLIP(masterVolume, 0, 255) / 255;
	}

	static byte applyArithmetic(byte opcode, byte lhs, byte rhs) {
		switch (opcode) {
		case 0xD5: case 0xD6: return lhs ^ rhs;
		case 0xD7: case 0xD8: return lhs | rhs;
		case 0xD9: case 0xDA: return lhs & rhs;
		case 0xDB: case 0xDC: return rhs ? lhs % rhs : lhs;
		case 0xDD: case 0xDE: return rhs ? lhs / rhs : lhs;
		case 0xDF: case 0xE0: return lhs * rhs;
		case 0xE1: case 0xE2: return lhs - rhs;
		case 0xE3: case 0xE4: return lhs + rhs;
		default: return lhs;
		}
	}

	GSoundChannel _channels[GSOUND_CHANNEL_COUNT];
	int _commandParam = 0;
	uint32 _frameCounter = 0;
	uint32 _tickCounter = 0;
	bool _isDisabled = false;
	int _pollResult = 0;

	virtual const GSoundCommandSpec *findCommandSpec(int command) const = 0;
	virtual bool executeSpecialCommand(int command, bool fromDeferred) = 0;
	virtual bool executeNativeCallback(uint16 targetOffset,
			GSoundChannel &channel) = 0;

	int executeCommand(int command, int param);
	void loadChannel(int channel, uint16 offset);
	void setDataByte(uint16 offset, byte value);
	byte getDataByte(uint16 offset) const;
	void setScriptVariable(byte index, byte value);
	byte getScriptVariable(byte index) const;
	void setMusicIndex(uint16 index) { _musicIndex = index; }
	void scheduleSpecial(int command, uint16 counter, uint16 period);
	bool soundActive(uint16 offset) const { return isSoundActive(offset); }
	bool channelsActive() const { return anyChannelActive(); }
	void stopMusic() { command3(); }
	void stopAll() { command1(); }
	void armNativeTimer(uint16 counter, uint16 period) {
		armTimer(counter, period);
	}
	void deferNativeCommand(int command) { deferCommand(command); }
	bool channelPlays(int channel, uint16 offset) const;
	void playNativeEffectAny(uint16 offset) { playEffectAny(offset); }

public:
	GSound(Audio::Mixer *mixer, const GSoundDriverData &driverData);
	~GSound() override;

	static bool validateOverlay(const GSoundDriverData &driverData);
	bool isReady() const { return _isReady; }

	int stop() override;
	int poll() override;
	void noise() override {}
	void setVolume(int volume) override;
};

} // namespace Sound
} // namespace Dragonsphere
} // namespace MADS

#endif // MADS_DRAGONSPHERE_SOUND_GSOUND_H
