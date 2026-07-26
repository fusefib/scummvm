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

#include "common/textconsole.h"
#include "mads/nebular/rsound_sections.h"

namespace MADS {
namespace RexNebular {

/*
 * RSOUND.001 contains 42 command entries. This matches the ASOUND.001 script
 * namespace, but not necessarily the same device-level channel operations.
 *
 * The patch-memory record at DS:0x0113 maps programs 32-45 to custom timbre
 * slots 0-13. Names are logged during bank upload and documented separately;
 * commands remain numeric because streams can select several programs.
 */
RSound1::RSound1(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.001", 3) {
}

byte RSound1::adjustedCommandParam() const {
	const byte value = (byte)_commandParam;
	return value > 0x40 ? value - 0x40 : 0;
}

void RSound1::startCommand111213() {
	if (isSequenceActive(0x1166))
		return;

	requestStopAll();
	startVoice(0, 0x1166);
	startVoice(1, 0x13bc);
	startVoice(2, 0x155c);
	startVoice(3, 0x15d8);
}

int RSound1::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);

	if (commandId < 0 || commandId > 41)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeCommonCommand(commandId, kCommonProfileSection1);

	switch (commandId) {
	case 9:
		startEffectVoice(0x0ad4);
		break;

	case 10:
		// Image 0x07E5: guard, then four fixed voices.
		if (!isSequenceActive(0x0ce4)) {
			startVoice(0, 0x0ce4);
			startVoice(1, 0x0d18);
			startVoice(2, 0x0e9c);
			startVoice(3, 0x0ee8);
		}
		break;

	case 11:
		startCommand111213();
		setVoiceVolume(0, 0x00);
		setVoiceVolume(1, 0x00);
		break;

	case 12:
		startCommand111213();
		setVoiceVolume(0, 0x50);
		setVoiceVolume(1, 0x00);
		break;

	case 13:
		startCommand111213();
		setVoiceVolume(0, 0x50);
		setVoiceVolume(1, 0x50);
		break;

	case 14:
		startEffectVoice(0x16c2);
		break;

	case 15:
		// Image 0x07C6: stop-all, then voices 5-7 only; the
		// parallel ASOUND command layers five OPL channels.
		if (!isSequenceActive(0x0f3a)) {
			requestStopAll();
			startVoice(4, 0x0f3a);
			startVoice(5, 0x102a);
			startVoice(6, 0x110e);
		}
		break;

	case 16:
		startEffectVoice(0x0ade);
		break;

	case 17:
		startEffectVoice(0x0ae8);
		break;

	case 18:
		startEffectVoice(0x0af2);
		break;

	case 19:
		requestStopAll();
		startEffectVoice(0x0b04);
		break;

	case 20:
		startEffectVoice(0x0b5e);
		break;

	case 21:
		startEffectVoice(0x0b4c);
		break;

	case 22:
		startEffectVoice(0x0b6e);
		break;

	case 23:
		// Image 0x0943 mutates byte +6 before allocation; this is
		// not ASOUND.001's resource-toggle mechanism.
		sequenceData(0x0b7a, 7)[6] ^= 0x1f;
		startEffectVoice(0x0b7a);
		break;

	case 24:
		startEffectVoice(0x0b84);
		break;

	case 25:
		startEffectVoice(0x0b8e);
		break;

	case 26:
	case 27: {
		const uint16 sequenceOffset = commandId == 26 ? 0x0cca : 0x0cbe;
		byte *data = sequenceData(sequenceOffset, 9);
		data[8] = (getRandomNumber() & 0x18) + 0x2d;
		data[5] = adjustedCommandParam() + 0x40;
		startVoice(7, sequenceOffset);
		break;
	}

	case 28:
		startEffectVoice(0x0b9e);
		break;

	case 29: {
		byte *data = sequenceData(0x0c6c, 12);
		data[11] = (adjustedCommandParam() >> 1) + 0x20;
		if (!isSequenceActive(0x0c6c))
			startEffectVoice(0x0c6c);
		break;
	}

	case 30: {
		byte *data = sequenceData(0x0c80, 12);
		data[11] = adjustedCommandParam() + 0x3f;
		if (!isSequenceActive(0x0c80))
			startAnyVoice(0x0c80);
		break;
	}

	case 31:
		startEffectVoice(0x0bbe);
		break;

	case 32: {
		const byte value = adjustedCommandParam() >> 1;
		byte *data = sequenceData(0x0c96, 30);
		data[11] = data[23] = value + 0x44;
		data[17] = data[29] = value + 0x14;
		if (!isSequenceActive(0x0c96))
			startAnyVoice(0x0c96);
		break;
	}

	case 33:
		startEffectVoice(0x0bd0);
		startEffectVoice(0x0bda);
		break;

	case 34: {
		byte *data = sequenceData(0x0be8, 17);
		data[9] = (getRandomNumber() & 0x0c) + 0x2d;
		data[16] = data[9] + 0x24;
		startEffectVoice(0x0be8);
		break;
	}

	case 35:
		startEffectVoice(0x0bfc);
		startEffectVoice(0x0c0e);
		startEffectVoice(0x0c20);
		break;

	case 36:
		startEffectVoice(0x0c3e);
		break;

	case 37: {
		const byte randomValue = getRandomNumber() & 0x0f;
		byte *data = sequenceData(0x0c4c, 7);
		data[6] = randomValue + 0x2a;
		data[3] = (byte)(0x3e - randomValue * 2);
		startEffectVoice(0x0c4c);
		break;
	}

	case 38:
		startAnyVoice(0x0c56);
		startEffectVoice(0x0c60);
		break;

	case 39:
		// Image 0x079C starts five fixed voices, including voice 9;
		// ASOUND.001 uses four OPL channels for this command ID.
		if (!isSequenceActive(0x1818)) {
			startVoice(4, 0x1818);
			startVoice(5, 0x1848);
			startVoice(6, 0x1874);
			startVoice(7, 0x18b4);
			startVoice(8, 0x18ce);
		}
		break;

	case 40:
		startEffectVoice(0x0c34);
		break;

	case 41:
		startEffectVoice(0x0cd6);
		break;
	}

	return 0;
}

/*
 * The June demo is an earlier build of section 1. It keeps the retail
 * command meanings through ID 40, but several streams, mutations and fixed
 * channel assignments differ. Its allocator at image 0x0628 covers all
 * melodic channels rather than the restricted retail effect pool.
 */
RSoundDemo1::RSoundDemo1(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.001", 0),
		_command23Toggle(false) {
}

byte RSoundDemo1::adjustedCommandParam() const {
	const byte value = (byte)_commandParam;
	return value > 0x40 ? value - 0x40 : 0;
}

void RSoundDemo1::startCommand111213() {
	if (isSequenceActive(0x1586))
		return;

	requestStopAll();
	startVoice(0, 0x1586);
	startVoice(1, 0x17dc);
	startVoice(2, 0x197c);
	startVoice(3, 0x19f8);
}

int RSoundDemo1::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);

	if (commandId < 0 || commandId > 40)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeCommonCommand(commandId, kCommonProfileSection1);

	switch (commandId) {
	case 9:
		startAnyVoice(0x0f34);
		break;

	case 10:
		if (!isSequenceActive(0x1104)) {
			requestStopAll();
			startVoice(4, 0x1104);
			startVoice(5, 0x1138);
			startVoice(6, 0x12bc);
			startVoice(7, 0x1308);
		}
		break;

	case 11:
		startCommand111213();
		setVoiceVolume(0, 0x00);
		setVoiceVolume(1, 0x00);
		break;

	case 12:
		startCommand111213();
		setVoiceVolume(0, 0x50);
		setVoiceVolume(1, 0x00);
		break;

	case 13:
		startCommand111213();
		setVoiceVolume(0, 0x50);
		setVoiceVolume(1, 0x50);
		break;

	case 14:
		startAnyVoice(0x1ae2);
		break;

	case 15:
		if (!isSequenceActive(0x135a)) {
			requestStopAll();
			startVoice(4, 0x135a);
			startVoice(5, 0x144a);
			startVoice(6, 0x152e);
		}
		break;

	case 16:
		startAnyVoice(0x0f3e);
		break;

	case 17:
		startAnyVoice(0x0f48);
		break;

	case 18:
		startAnyVoice(0x0f52);
		break;

	case 19:
		requestStopAll();
		startAnyVoice(0x0f64);
		break;

	case 20:
		startAnyVoice(0x0fbe);
		break;

	case 21:
		startAnyVoice(0x0fac);
		break;

	case 22: {
		byte *data = sequenceData(0x0fce, 7);
		data[6] = (getRandomNumber() & 0x07) + 0x73;
		startAnyVoice(0x0fce);
		break;
	}

	case 23:
		// The handler toggles a byte in its code segment, choosing 0x0FD8
		// on the first call and alternating thereafter.
		_command23Toggle = !_command23Toggle;
		startAnyVoice(_command23Toggle ? 0x0fd8 : 0x0fe0);
		break;

	case 24:
		startAnyVoice(0x0fe8);
		break;

	case 25:
		startAnyVoice(0x0ff2);
		break;

	case 26:
	case 27: {
		const uint16 sequenceOffset = commandId == 26 ? 0x10f8 : 0x10ec;
		byte *data = sequenceData(sequenceOffset, 9);
		data[8] = (getRandomNumber() & 0x18) + 0x2d;
		data[5] = adjustedCommandParam() + 0x40;
		startVoice(7, sequenceOffset);
		break;
	}

	case 28:
		startAnyVoice(0x1002);
		break;

	case 29: {
		byte *data = sequenceData(0x109a, 12);
		data[11] = (adjustedCommandParam() >> 1) + 0x20;
		if (!isSequenceActive(0x109a))
			startAnyVoice(0x109a);
		break;
	}

	case 30: {
		byte *data = sequenceData(0x10ae, 12);
		data[11] = adjustedCommandParam() + 0x3f;
		if (!isSequenceActive(0x10ae))
			startAnyVoice(0x10ae);
		break;
	}

	case 31:
		startAnyVoice(0x1022);
		break;

	case 32: {
		const byte value = adjustedCommandParam() >> 1;
		byte *data = sequenceData(0x10c4, 30);
		data[11] = data[23] = value + 0x44;
		data[17] = data[29] = value + 0x14;
		if (!isSequenceActive(0x10c4))
			startAnyVoice(0x10c4);
		break;
	}

	case 33:
		startAnyVoice(0x1034);
		startAnyVoice(0x103e);
		break;

	case 34: {
		byte *data = sequenceData(0x104c, 17);
		data[9] = (getRandomNumber() & 0x0c) + 0x2d;
		data[16] = data[9] + 0x24;
		startAnyVoice(0x104c);
		break;
	}

	case 35:
		startAnyVoice(0x1060);
		break;

	case 36:
		startAnyVoice(0x1078);
		break;

	case 37:
		startAnyVoice(0x1086);
		break;

	case 38:
		startAnyVoice(0x1090);
		break;

	case 39:
		if (!isSequenceActive(0x1c38)) {
			startVoice(4, 0x1c38);
			startVoice(5, 0x1c68);
			startVoice(6, 0x1c94);
			startVoice(7, 0x1cd4);
			startVoice(8, 0x1cee);
		}
		break;

	case 40:
		startAnyVoice(0x106e);
		break;
	}

	return 0;
}

/*
 * RSOUND.002 has 44 command entries and 17 custom timbres. Its patch-memory
 * record at DS:0x0123 maps programs 32-48 to custom slots 0-16.
 *
 * Commands 12, 18, 27, 28 and 30/31 are the principal stateful handlers:
 * a cycling stream byte, a verified 16-entry random table, allocated-voice
 * loop anchors, random stream mutation, and a shared mutable volume byte.
 */
RSound2::RSound2(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.002", 3),
		_command12Value(0x2f) {
}

int RSound2::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);

	if (commandId < 0 || commandId > 43)
		return 0;
	beginCommand(param);

	if (commandId <= 8) {
		// Images 0x0A37, 0x032E and 0x0331 restore the
		// self-modifying byte used by command 12.
		if (commandId == 0 || commandId == 1 || commandId == 5)
			_command12Value = 0x2f;
		return executeCommonCommand(commandId, kCommonProfileSection2);
	}

	switch (commandId) {
	case 9:
		// Image 0x07D5: channels 1, 2 and 9. The rhythm voice is
		// fixed even though its state is physically the second 0x22-byte slot.
		if (!isSequenceActive(0x103c)) {
			requestStopAll();
			startVoice(0, 0x103c);
			startVoice(1, 0x11b2);
			startVoice(8, 0x127e);
		}
		break;

	case 10:
		if (!isSequenceActive(0x132e)) {
			requestStopAll();
			startVoice(2, 0x132e);
			startVoice(8, 0x1384);
		}
		break;

	case 11:
		if (!isSequenceActive(0x1548)) {
			requestStopAll();
			startVoice(2, 0x1548);
			startVoice(8, 0x1648);
		}
		break;

	case 12: {
		// Image 0x08F2 modifies DS:0x0E55, the volume operand in a
		// program-33 (`RhoBounce`) stream.
		_command12Value = (_command12Value + 0x10) & 0x7f;
		sequenceData(0x0e52, 4)[3] = _command12Value;
		startEffectVoice(0x0e52);
		break;
	}

	case 13:
		startEffectVoice(0x0e5c);
		startEffectVoice(0x0e66);
		break;

	case 14:
		startEffectVoice(0x0e70);
		startEffectVoice(0x0e8a);
		break;

	case 15:
		if (!isSequenceActive(0x1dfc)) {
			requestStopAll();
			startAnyVoice(0x1dfc);
			startAnyVoice(0x222e);
			startAnyVoice(0x2648);
			startVoice(8, 0x26a2);
		}
		break;

	case 16:
		if (!isSequenceActive(0x3456)) {
			requestStopAll();
			startAnyVoice(0x3456);
			startAnyVoice(0x3572);
			startAnyVoice(0x367e);
			startAnyVoice(0x37c6);
			startAnyVoice(0x39ae);
			startAnyVoice(0x3a3a);
		}
		break;

	case 17:
		if (!isSequenceActive(0x3ac0)) {
			startEffectVoice(0x3ac0);
			startEffectVoice(0x3c70);
			startEffectVoice(0x3e16);
			startEffectVoice(0x3fbe);
		}
		break;

	case 18:
		// Image 0x07A3 checks MIDI channel 8 and indexes the exact
		// sixteen-word table stored at DS:0x0061.
		if (!voice(7).active()) {
			static const uint16 kSequences[16] = {
				0x3234, 0x3250, 0x326a, 0x3284,
				0x329e, 0x32d6, 0x3304, 0x333c,
				0x3352, 0x3378, 0x33b6, 0x33d0,
				0x33ea, 0x3404, 0x341e, 0x343e
			};
			startVoice(7, kSequences[(getRandomNumber() & 0x1e) >> 1]);
		}
		break;

	case 19:
		if (!isSequenceActive(0x2a64)) {
			requestStopAll();
			startAnyVoice(0x2a64);
			startAnyVoice(0x2be2);
			startAnyVoice(0x2dac);
			startAnyVoice(0x2ece);
			startAnyVoice(0x3026);
			startAnyVoice(0x30c2);
		}
		break;

	case 20:
		startEffectVoice(0x0f1e);
		startEffectVoice(0x0f1e);
		startEffectVoice(0x0f1e);
		startEffectVoice(0x0f1e);
		break;

	case 21:
		startEffectVoice(0x0f58);
		break;
	case 22:
		startEffectVoice(0x0f44);
		break;
	case 23:
		startEffectVoice(0x0eba);
		break;
	case 24:
		startEffectVoice(0x0eb0);
		break;
	case 25:
		startEffectVoice(0x0ea6);
		break;
	case 26:
		startEffectVoice(0x0f4e);
		break;

	case 27: {
		// Image 0x09C0 installs loop anchor 0x0FB8 on the first
		// two allocated voices before starting the anchor stream itself.
		int voiceIndex = startEffectVoice(0x0fac);
		if (voiceIndex >= 0)
			voice((uint)voiceIndex).innerLoopStart = 0x0fb8;
		voiceIndex = startEffectVoice(0x0fb2);
		if (voiceIndex >= 0)
			voice((uint)voiceIndex).innerLoopStart = 0x0fb8;
		startEffectVoice(0x0fb8);
		break;
	}

	case 28: {
		// Image 0x0957 writes gate, note and a second note twelve
		// semitones higher into the program-39 (`ChickCluck`) stream.
		const uint16 randomValue = getRandomNumber() & 0x7f;
		byte *data = sequenceData(0x0eda, 11);
		data[7] = (byte)randomValue;
		data[8] = (randomValue & 0x0f) + 0x43;
		data[10] = data[8] + 0x0c;
		startEffectVoice(0x0eda);
		break;
	}

	case 29:
		startAnyVoice(0x0f80);
		break;

	case 30:
		// Commands 30 and 31 share the volume operand at DS:0x0F0D.
		startEffectVoice(0x0f14);
		sequenceData(0x0f0a, 4)[3] = 0x28;
		startEffectVoice(0x0f0a);
		break;

	case 31:
		sequenceData(0x0f0a, 4)[3] = 0x18;
		startEffectVoice(0x0f0a);
		break;

	case 32:
		startEffectVoice(0x0ec4);
		break;
	case 33:
		startEffectVoice(0x0ed0);
		break;
	case 34:
		startEffectVoice(0x0ee8);
		break;
	case 35:
		startEffectVoice(0x0ef8);
		break;

	case 36:
		startEffectVoice(0x0ff4);
		startEffectVoice(0x1008);
		break;

	case 37:
		startEffectVoice(0x0fcc);
		break;

	case 38:
		if (!isSequenceActive(0x2b0e)) {
			requestStopAll();
			startAnyVoice(0x2b0e);
			startAnyVoice(0x2cd0);
			startAnyVoice(0x2e46);
			startAnyVoice(0x2f7c);
			startAnyVoice(0x3074);
			startAnyVoice(0x317e);
		}
		break;

	case 39:
		startEffectVoice(0x0fda);
		break;
	case 40:
		startEffectVoice(0x0fe6);
		break;
	case 41:
		startAnyVoice(0x0f62);
		break;
	case 42:
		startEffectVoice(0x0f92);
		break;

	case 43:
		// Both streams select program 35 (`BirdCaw`) and differ by one
		// semitone, matching the LEA operands at image 0x0926.
		startEffectVoice(0x1018);
		startEffectVoice(0x102a);
		break;
	}

	return 0;
}


RSound3::RSound3(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.003"),
		_command16Toggle(false),
		_command39Toggle(false) {
}

// The recovered table has 61 IDs. Commands 27/42 and 47/49 are
// handler aliases; 12, 52-56 and 58 use the shared no-op handler.
int RSound3::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);
	if (commandId < 0 || commandId > 60)
		return 0;
	beginCommand(param);

	switch (commandId) {
	case 0: case 1: case 2: case 3: case 4: case 6: case 7: case 8:
		return executeCommonCommand(commandId);
	case 5:
		if (!isSequenceActive(0x1ae6))
			executeCommonCommand(5);
		break;
	case 9:
		// Image 0x0743 calls command 1 and then writes the whole-update
		// divider. Zero disables voice processing; other values decimate it.
		requestStopAll();
		setUpdateDivider((byte)param);
		break;
	case 10:
		if (!isSequenceActive(0x14fe)) {
			requestStopAll();
			startVoice(0, 0x14fe); startVoice(1, 0x1630);
			startVoice(2, 0x186e); startVoice(3, 0x1a68);
			// Image 0x0940 tail-jumps to the channel-9 fixed loader.
			startVoice(8, 0x1aa6);
		}
		break;
	case 11:
		if (!isSequenceActive(0x1ae6)) {
			requestStopAll();
			// Image 0x09B6 calls helper 0x0947 with AL=0x64.
			sequenceData(0x204a, 2)[1] = 0x64;
			sequenceData(0x229c, 2)[1] = 0x64;
			sequenceData(0x2748, 2)[1] = 0x64;
			sequenceData(0x2c56, 2)[1] = 0x64;
			startVoice(0, 0x1ae6); startVoice(1, 0x1e00);
			startVoice(2, 0x1e66); startVoice(3, 0x204a);
			startVoice(4, 0x229c); startVoice(5, 0x2748);
			startVoice(6, 0x2c56);
		}
		break;
	case 12: case 52: case 53: case 54: case 55: case 56: case 58:
		break;
	case 13:
		// Image 0x0A1A always requests stop-all and allocates the same
		// program-45 (`CatAlien`) stream five times; there is no guard.
		requestStopAll();
		for (uint index = 0; index < 5; ++index)
			startAnyVoice(0x1364);
		break;
	case 14:
		startVoice(0, 0x32dc); startVoice(1, 0x32fc);
		startVoice(2, 0x331c); startVoice(3, 0x333e);
		startVoice(4, 0x335c); startVoice(5, 0x339e);
		startVoice(6, 0x33de); startVoice(7, 0x341e);
		break;
	case 15: {
		sequenceData(0x204a, 2)[1] = 0x3c;
		sequenceData(0x229c, 2)[1] = 0x3c;
		sequenceData(0x2748, 2)[1] = 0x3c;
		sequenceData(0x2c56, 2)[1] = 0x3c;

		// Helper 0x0A78 stores 0x3C only in channel 1's voice state,
		// but sends controller 7 on both channels 1 and 2.
		setVoiceVolume(0, 0x3c);
		sendUntrackedVoiceVolume(1, 0x3c);

		if (voice(3).active() && voice(3).identityOffset == 0x204a) {
			// Images 0x098E-0x09A7 set only pendingStop on channels
			// 3-8. Their identity fields deliberately remain intact.
			static const byte kFadeVoices[] = { 2, 3, 4, 5, 6, 7 };
			for (uint index = 0; index < ARRAYSIZE(kFadeVoices); ++index)
				voice(kFadeVoices[index]).pendingStop = 0xff;
			setUpdateDivider(1);
		} else {
			// Fallback at image 0x0964.
			requestStopAll();
			startVoice(0, 0x1ae6);
			startVoice(1, 0x1e00);
		}
		break;
	}
	case 16:
		// Image 0x0A94 XORs an embedded byte with 0xFF. The first
		// family overwrites four fixed voices without command 1; the
		// alternate family calls command 1 after its active guard.
		_command16Toggle = !_command16Toggle;
		if (_command16Toggle) {
			if (!isSequenceActive(0x345e)) {
				startVoice(0, 0x345e); startVoice(1, 0x364c);
				startVoice(2, 0x3806); startVoice(3, 0x399e);
			}
		} else if (!isSequenceActive(0x3b26)) {
			requestStopAll();
			startVoice(0, 0x3b26); startVoice(1, 0x3bd8);
			startVoice(2, 0x3cf8); startVoice(3, 0x3e46);
		}
		break;
	case 17:
		if (!isSequenceActive(0x3f5c)) {
			requestStopAll();
			startVoice(0, 0x3f5c); startVoice(1, 0x4022);
			startVoice(2, 0x41f0); startVoice(3, 0x42f8);
		}
		break;
	case 18:
		requestStopAll();
		startVoice(0, 0x4492); startVoice(1, 0x45a4);
		startVoice(2, 0x46a2); startVoice(3, 0x47dc);
		startVoice(4, 0x49c0); startVoice(5, 0x4a4e);
		break;
	case 19: if (!isSequenceActive(0x1ae6)) startEffectVoice(0x12b4); break;
	case 20: if (!isSequenceActive(0x1ae6)) startEffectVoice(0x1246); break;
	case 21:
		if (!isSequenceActive(0x1ae6)) { startEffectVoice(0x1232); startEffectVoice(0x123c); }
		break;
	case 22: startEffectVoice(0x126e); break;
	case 23:
		if (!isSequenceActive(0x1ae6)) { startEffectVoice(0x12d2); startEffectVoice(0x12e0); }
		break;
	case 24:
		if (!isSequenceActive(0x1ae6)) { startEffectVoice(0x11e2); startEffectVoice(0x120a); }
		break;
	case 25:
		// The dispatcher executes `xor ax,ax` immediately before the
		// indirect call, leaving ZF set for the `jz` at image 0x0C32.
		sequenceData(0x11a6, 6)[5] = 0x25;
		sequenceData(0x11c4, 6)[5] = 0x25;
		startEffectVoice(0x11a6); startEffectVoice(0x11c4);
		break;
	case 26: startEffectVoice(0x1252); break;
	case 27: case 42: startEffectVoice(0x14be); break;
	case 28:
		sequenceData(0x12ee, 4)[3] = 0x4d;
		startEffectVoice(0x12ee);
		break;
	case 29:
		// Handler 0x0B3C shares the command-28 tail with AL=0x7F.
		sequenceData(0x12ee, 4)[3] = 0x7f;
		startEffectVoice(0x12ee);
		break;
	case 30: startEffectVoice(0x14b4); startEffectVoice(0x14aa); break;
	case 31: startEffectVoice(0x12f8); startEffectVoice(0x130e); break;
	case 32: startEffectVoice(0x1472); break;
	case 33: startEffectVoice(0x147e); break;
	case 34: startEffectVoice(0x1488); break;
	case 35: startEffectVoice(0x1498); break;
	case 36: startEffectVoice(0x14da); startEffectVoice(0x14ee); break;
	case 37: startEffectVoice(0x14cc); break;
	case 38: startEffectVoice(0x133a); break;
	case 39:
	case 40: {
		byte *data = sequenceData(0x1346, 7);
		data[3] = commandId == 39 ? 0x4d : 0x2f;
		// Image 0x0B83 toggles embedded byte 0x0B77 by four and
		// stores 0x2C/0x28 in stream byte +6.
		_command39Toggle = !_command39Toggle;
		data[6] = _command39Toggle ? 0x2c : 0x28;
		startEffectVoice(0x1346);
		break;
	}
	case 41: startEffectVoice(0x1184); break;
	case 43: startEffectVoice(0x1350); startEffectVoice(0x135a); break;
	case 44: startEffectVoice(0x12a0); break;
	case 45: startEffectVoice(0x12aa); break;
	case 46: startEffectVoice(0x13aa); startEffectVoice(0x13c6); break;
	case 47: case 49: startEffectVoice(0x13e6); startEffectVoice(0x13fe); break;
	case 48: startEffectVoice(0x141e); break;
	case 50: startEffectVoice(0x1436); startEffectVoice(0x144c); break;
	case 51: startEffectVoice(0x125c); break;
	case 57: startEffectVoice(0x1466); break;
	case 59: startEffectVoice(0x1324); break;
	case 60: startEffectVoice(0x132e); break;
	}
	return 0;
}

RSound4::RSound4(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.004"),
		_callbackCounter(0), _callbackPeriod(0),
		_callbackAction(kCallbackNone) {
}

void RSound4::clearCallback() {
	// Command 0 clears DS:1182, DS:1184 and DS:1186.
	_callbackCounter = 0;
	_callbackPeriod = 0;
	_callbackAction = kCallbackNone;
}

void RSound4::resetSectionState() {
	clearCallback();
}

void RSound4::scheduleCallback(CallbackAction action) {
	// Commands 54-56 write only the callback pointer at DS:1186.
	_callbackAction = action;
}

void RSound4::runCallback(CallbackAction action) {
	switch (action) {
	case kCallbackCommand54:
		startAnyVoice(0x18b2);
		startAnyVoice(0x1904);
		break;
	case kCallbackCommand55:
		startAnyVoice(0x191e);
		break;
	case kCallbackCommand56:
		startAnyVoice(0x185c);
		break;
	default:
		break;
	}
}

void RSound4::sectionTimerTick() {
	if (!_callbackPeriod)
		return;

	// Image 0x091D: decrement, reload from DS:1184, then call DS:1186.
	_callbackCounter = (uint16)(_callbackCounter - 1);
	if (_callbackCounter)
		return;

	_callbackCounter = _callbackPeriod;

	// Each callback body clears DS:1186 before allocating its stream(s).
	const CallbackAction action = _callbackAction;
	_callbackAction = kCallbackNone;
	runCallback(action);
}

// Twenty-nine command IDs use the shared immediate-return handler.
// They remain explicit no-ops so the original 0-59 ABI is preserved.
int RSound4::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);
	if (commandId < 0 || commandId > 59)
		return 0;
	beginCommand(param);

	if (commandId <= 8) {
		if (commandId == 0)
			resetSectionState();
		return executeCommonCommand(commandId);
	}

	switch (commandId) {
	case 9:
		requestStopAll();
		setUpdateDivider((byte)param);
		break;
	case 10:
		// Image 0x0956 has no active guard: channels 4,9,1,2,3.
		requestStopAll();
		startVoice(3, 0x17de);
		startVoice(8, 0x181c);
		startVoice(0, 0x1274);
		startVoice(1, 0x13a6);
		startVoice(2, 0x15e4);
		break;
	case 12: {
		const byte value = (byte)(((uint16)param >> 1) + 0x24);
		sequenceData(0x1966, 2)[1] = value;
		sequenceData(0x1dc8, 2)[1] = value;
		sequenceData(0x1fba, 2)[1] = value;
		sequenceData(0x211e, 2)[1] = value;
		sequenceData(0x24a8, 2)[1] = value;

		if (voice(4).active() && voice(4).identityOffset == 0x1966)
			break;

		requestStopAll();
		startVoice(4, 0x1966);
		startVoice(5, 0x1dc8);
		startVoice(6, 0x1fba);
		startVoice(7, 0x211e);
		startVoice(8, 0x24a8);
		break;
	}
	case 19: startEffectVoice(0x1196); break;
	case 20: startEffectVoice(0x118a); break;
	case 21: startEffectVoice(0x1260); startEffectVoice(0x126a); break;
	case 27: startEffectVoice(0x1220); break;
	case 30: startEffectVoice(0x1216); startEffectVoice(0x120c); break;
	case 32: startEffectVoice(0x11d4); break;
	case 33: startEffectVoice(0x11e0); break;
	case 34: startEffectVoice(0x11ea); break;
	case 35: startEffectVoice(0x11fa); break;
	case 36: startEffectVoice(0x123c); startEffectVoice(0x1250); break;
	case 37: startEffectVoice(0x122e); break;
	case 52:
		voice(0).position = 0x1188;
		voice(1).position = 0x1188;
		voice(2).position = 0x1188;
		startVoice(4, 0x2a0c);
		break;
	case 53:
		requestStopAll();
		_callbackCounter = 0x0038;
		_callbackPeriod = 0x0038;
		startAnyVoice(0x1888);
		startAnyVoice(0x18de);
		break;
	case 54: scheduleCallback(kCallbackCommand54); break;
	case 55: scheduleCallback(kCallbackCommand55); break;
	case 56: scheduleCallback(kCallbackCommand56); break;
	case 57: startEffectVoice(0x11c8); break;
	case 58:
		voice(4).position = 0x1188;
		startVoice(0, 0x1274);
		startVoice(1, 0x13a6);
		startVoice(2, 0x15e4);
		break;
	case 59: startEffectVoice(0x11be); break;
	default:
		break;
	}
	return 0;
}

/*
 * RSOUND.005 has 42 command entries and reuses the byte-identical section
 * 3-5 MT-32 bank. Commands 11/24 and 12/25 are two distinct alias pairs.
 *
 * Commands 14/15 and 29/38/41 form two verified fixed-voice transition
 * families. Their channel assignments come from the fixed-loader calls, not
 * from the corresponding ASOUND channel layout.
 */
RSound5::RSound5(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.005") {
}

// Commands 11/24 and 12/25 are the two recovered handler aliases.
int RSound5::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);
	if (commandId < 0 || commandId > 41)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeCommonCommand(commandId);

	switch (commandId) {
	case 9: startEffectVoice(0x11c4); break;
	case 10: startEffectVoice(0x1238); break;
	case 11: case 24: startEffectVoice(0x1196); break;
	case 12: case 25: startEffectVoice(0x1242); break;
	case 13: startEffectVoice(0x125e); startEffectVoice(0x1268); break;
	case 14:
		// Image 0x09C1 starts the stream on fixed rhythm channel 9.
		startVoice(8, 0x1272);
		break;
	case 15:
		if (voice(8).identityOffset == 0x1272) {
			// Image 0x09C8 rewrites both loop anchors, then stores the
			// 16-bit value 1 at +00. That sets delay=1 and pitchStep=0.
			voice(8).innerLoopStart = 0x1288;
			voice(8).outerLoopStart = 0x1288;
			voice(8).delay = 1;
			voice(8).pitchStep = 0;
		}
		break;
	case 16: for (uint i = 0; i < 4; ++i) startEffectVoice(0x129a); break;
	case 17: startEffectVoice(0x11b4); break;
	case 18:
		startEffectVoice(0x12e4); startEffectVoice(0x12f6);
		startEffectVoice(0x1308); startEffectVoice(0x131a); break;
	case 19: startEffectVoice(0x132c); break;
	case 20: startEffectVoice(0x1368); break;
	case 21: startEffectVoice(0x1388); break;
	case 22: startEffectVoice(0x139a); break;
	case 23: for (uint i = 0; i < 4; ++i) startEffectVoice(0x13aa); break;
	case 26: startEffectVoice(0x13d6); break;
	case 27: startEffectVoice(0x13f0); break;
	case 28: startEffectVoice(0x121c); break;
	case 29:
		if (!isSequenceActive(0x1488)) {
			requestStopAll();
			startVoice(0, 0x1488); startVoice(1, 0x1534);
			startVoice(2, 0x15ea); startVoice(3, 0x1688);
			// Image 0x0941 tail-jumps to the channel-9 loader.
			startVoice(8, 0x1882);
		}
		break;
	case 30: startEffectVoice(0x1212); startEffectVoice(0x1208); break;
	case 31: startEffectVoice(0x140a); break;
	case 32: startEffectVoice(0x11d0); break;
	case 33: startEffectVoice(0x11dc); break;
	case 34: startEffectVoice(0x11e6); break;
	case 35: startEffectVoice(0x11f6); break;
	case 36: startEffectVoice(0x1464); startEffectVoice(0x1478); break;
	case 37:
		startEffectVoice(0x122a);
		break;
	case 38:
		// Image 0x0956 redirects channel 5 to the shared 00 00
		// termination pair, then enters command 29 at its channel-4 and
		// channel-9 loaders. There is no command-1 stop request here.
		voice(4).position = 0x1182;
		startVoice(3, 0x1688);
		startVoice(8, 0x1882);
		break;
	case 39:
		startEffectVoice(0x141e);
		startEffectVoice(0x1428);
		break;
	case 40:
		startEffectVoice(0x1432);
		break;
	case 41:
		// Image 0x0948 redirects rhythm channel 9 to the termination
		// pair and starts 0x1BB6 on fixed channel 4.
		voice(8).position = 0x1182;
		startVoice(3, 0x1bb6);
		break;
	}
	return 0;
}

/*
 * RSOUND.006 has 30 command entries and 27 unique handlers. Commands 13/14
 * are aliases; commands 26-28 enter the same one-byte RET handler.
 */
RSound6::RSound6(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.006"),
		_callbackCounter(0), _callbackPeriod(0),
		_callbackAction(kCallbackNone) {
}

void RSound6::clearCallback() {
	_callbackCounter = 0;
	_callbackPeriod = 0;
	_callbackAction = kCallbackNone;
}

void RSound6::resetSectionState() {
	clearCallback();
}

void RSound6::scheduleCallback(CallbackAction action) {
	// Images 0x0947 and 0x09A0 write only DS:10E8.
	_callbackAction = action;
}

void RSound6::startGroup130A() {
	// Callback body at image 0x0968.
	_callbackAction = kCallbackNone;
	requestStopAll();
	_callbackCounter = 0x0054;
	_callbackPeriod = 0x0054;
	startVoice(0, 0x130a); startVoice(1, 0x13b6);
	startVoice(2, 0x146c); startVoice(3, 0x150a);
	startVoice(8, 0x1704);
}

void RSound6::startGroup1A38() {
	// Callback body at image 0x09BA.
	_callbackAction = kCallbackNone;
	requestStopAll();
	_callbackCounter = 0x0054;
	_callbackPeriod = 0x0054;
	startVoice(0, 0x1a38); startVoice(1, 0x1bbe);
	startVoice(8, 0x1b90);
}

void RSound6::sectionTimerTick() {
	if (!_callbackPeriod)
		return;

	// Image 0x015B uses a 16-bit decrement/reload clock.
	_callbackCounter = (uint16)(_callbackCounter - 1);
	if (_callbackCounter)
		return;
	_callbackCounter = _callbackPeriod;

	switch (_callbackAction) {
	case kCallbackStartGroup130A:
		startGroup130A();
		break;
	case kCallbackStartGroup1A38:
		startGroup1A38();
		break;
	default:
		break;
	}
}

// Commands 13/14 are aliases. Commands 26-28 are intentional no-ops.
int RSound6::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);
	if (commandId < 0 || commandId > 29)
		return 0;
	beginCommand(param);

	if (commandId <= 8) {
		if (commandId == 0)
			resetSectionState();
		return executeCommonCommand(commandId);
	}

	switch (commandId) {
	case 9: startEffectVoice(0x111c); break;
	case 10: startEffectVoice(0x1190); break;
	case 11: for (uint i = 0; i < 4; ++i) startEffectVoice(0x11a4); break;
	case 12: startEffectVoice(0x11c8); break;
	case 13: case 14: startEffectVoice(0x11e8); break;
	case 15: for (uint i = 0; i < 4; ++i) startEffectVoice(0x11f2); break;
	case 16: startEffectVoice(0x121e); break;
	case 17: for (uint i = 0; i < 4; ++i) startEffectVoice(0x1228); break;
	case 18: startEffectVoice(0x1254); break;
	case 19: startEffectVoice(0x1266); break;
	case 20: startEffectVoice(0x1278); break;
	case 21:
		// Fixed channel 5 first; restricted allocations then fill 6-8.
		startVoice(4, 0x1282);
		startEffectVoice(0x1282); startEffectVoice(0x1282);
		startEffectVoice(0x12aa);
		break;
	case 22:
		// Image 0x09E4: fixed channels 5-8.
		startVoice(4, 0x12ce); startVoice(5, 0x12ce);
		startVoice(6, 0x12ce); startVoice(7, 0x12ce);
		break;
	case 23: startEffectVoice(0x1174); break;
	case 24:
		// Test the complete word at voice +00, not only the delay byte.
		if ((voice(0).delay || voice(0).pitchStep) &&
				voice(0).identityOffset == 0x130a)
			scheduleCallback(kCallbackStartGroup1A38);
		else
			startGroup1A38();
		break;
	case 25:
		// Image 0x0AD9 tail-jumps to fixed channel 5.
		startVoice(4, 0x12fe);
		break;
	case 26: case 27: case 28:
		// The entry byte at image 0x0946 is C3 (RET).
		break;
	case 29:
		if (isSequenceActive(0x130a))
			break;
		if ((voice(0).delay || voice(0).pitchStep) &&
				voice(0).identityOffset == 0x1a38)
			scheduleCallback(kCallbackStartGroup130A);
		else
			startGroup130A();
		break;
	}
	return 0;
}

/*
 * RSOUND.007 has 38 command entries and 30 unique handlers. Commands
 * 10-14, 26, 28, 29 and 31 share the one-byte RET handler.
 *
 * Its MT-32 bank is a verified section-7 variant: compared with the shared
 * section-3/4/5 bank, custom slots 8 and 12 are `SmoothNois` and `Monster`.
 */
RSound7::RSound7(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.007") {
}

// Commands 10-14, 26, 28, 29 and 31 use the shared no-op handler.
int RSound7::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);
	if (commandId < 0 || commandId > 37)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeCommonCommand(commandId);

	switch (commandId) {
	case 9:
		// Image 0x093A has no duplicate guard: command 1 followed by
		// fixed channels 1-4.
		requestStopAll();
		startVoice(0, 0x1c0e); startVoice(1, 0x1c88);
		startVoice(2, 0x1cd4); startVoice(3, 0x1d4e);
		break;
	case 10: case 11: case 12: case 13: case 14:
	case 26: case 28: case 29: case 31:
		break;
	case 15:
		startEffectVoice(0x125c);
		break;
	case 16:
		// RSOUND has a distinct rising stream; ASOUND aliases IDs 16/17.
		startEffectVoice(0x12de);
		break;
	case 17:
		// Distinct descending stream at handler image 0x09D7.
		startEffectVoice(0x12c2);
		break;
	case 18:
		// Image 0x09E5 uses the fixed channel-8 loader.
		startVoice(7, 0x12fc);
		break;
	case 19:
		if (voice(7).identityOffset == 0x12fc) {
			// Image 0x09EC rewrites both loop anchors, then performs
			// `mov word [bx],1`: delay=1 and pitchStep=0.
			voice(7).innerLoopStart = 0x1312;
			voice(7).outerLoopStart = 0x1312;
			voice(7).delay = 1;
			voice(7).pitchStep = 0;
		}
		break;
	case 20: startEffectVoice(0x1324); startEffectVoice(0x1324); break;
	case 21: startEffectVoice(0x1336); break;
	case 22: startEffectVoice(0x1340); break;
	case 23: startEffectVoice(0x12b4); break;
	case 24:
		// Image 0x0959 directly overwrites fixed channels 1-5.
		// There is no command-1 call and no active-sequence guard.
		startVoice(0, 0x137c); startVoice(1, 0x1406);
		startVoice(2, 0x1492); startVoice(3, 0x1516);
		startVoice(4, 0x1588);
		break;
	case 25:
		// Image 0x091B is command 1 followed by fixed channels 1-4.
		requestStopAll();
		startVoice(0, 0x1612); startVoice(1, 0x16c8);
		startVoice(2, 0x177e); startVoice(3, 0x1838);
		break;
	case 27:
		// Image 0x097C directly overwrites fixed channels 1-5. Unlike
		// ASOUND command 27, it does not call the first-group stop handler.
		startVoice(0, 0x1932); startVoice(1, 0x1986);
		startVoice(2, 0x19ec); startVoice(3, 0x1a66);
		startVoice(4, 0x1b3c);
		break;
	case 30: startEffectVoice(0x12aa); startEffectVoice(0x12a0); break;
	case 32: startEffectVoice(0x1268); break;
	case 33: startEffectVoice(0x1274); break;
	case 34: startEffectVoice(0x127e); break;
	case 35: startEffectVoice(0x128e); break;
	case 36: startEffectVoice(0x1358); startEffectVoice(0x136c); break;
	case 37: startEffectVoice(0x134a); break;
	}
	return 0;
}

/*
 * RSOUND.008 is the dense 38-entry numbered overlay: every command ID has a
 * distinct handler address. Commands 14 and 15 enter different instruction
 * addresses but share the mutation/playback tail at image 0x09E2.
 *
 * The bank changes patch memory and four custom timbres relative to the shared
 * section-3/4/5 bank. Names remain diagnostics rather than command labels.
 */
RSound8::RSound8(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.008") {
}

int RSound8::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);
	if (commandId < 0 || commandId > 37)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeCommonCommand(commandId);

	switch (commandId) {
	case 9:
		startEffectVoice(0x10ba);
		break;
	case 10:
		// Image 0x0991 directly overwrites fixed channels 1-4. There is
		// no command-1 stop request and no active-sequence guard.
		startVoice(0, 0x115a); startVoice(1, 0x115a);
		startVoice(2, 0x115a); startVoice(3, 0x1150); break;
	case 11: startEffectVoice(0x1194); break;
	case 12: startEffectVoice(0x11b2); break;
	case 13:
		for (uint i = 0; i < 4; ++i) startEffectVoice(0x11d0);
		break;
	case 14: case 15: {
		// Command 14 enters at image 0x09DE with AL=0x28/AH=0x01;
		// command 15 enters at 0x09D7 with AL=0x64/AH=0xFF.
		// The LEA at 0x09E2 addresses mutable sequence data. It is not
		// a fifth playback start; exactly four allocator calls follow.
		byte *data = sequenceData(0x1204, 10);
		data[3] = commandId == 14 ? 0x28 : 0x64;
		data[6] = data[9] = commandId == 14 ? 0x01 : 0xff;
		for (uint i = 0; i < 4; ++i)
			startEffectVoice(0x1204);
		break;
	}
	case 16: startEffectVoice(0x112e); startEffectVoice(0x112e); break;
	case 17: startEffectVoice(0x1234); break;
	case 18: startEffectVoice(0x1244); break;
	case 19: startEffectVoice(0x1254); break;
	case 20: startEffectVoice(0x125e); break;
	case 21: startEffectVoice(0x126e); break;
	case 22: startEffectVoice(0x1278); break;
	case 23:
		// Image 0x0A4C loads one stream on each fixed channel 1-4.
		// It does not call command 1 or test for an existing instance.
		startVoice(0, 0x128e); startVoice(1, 0x128e);
		startVoice(2, 0x128e); startVoice(3, 0x128e); break;
	case 24: startEffectVoice(0x12b4); break;
	case 25: startEffectVoice(0x12ca); break;
	case 26: startEffectVoice(0x12dc); break;
	case 27: startEffectVoice(0x1112); break;
	case 28:
		// Image 0x091B uses the stack-unwinding melodic-voice guard.
		// Rhythm channel 9 is intentionally excluded from that guard.
		if (!isSequenceActive(0x130a)) {
			requestStopAll();
			startVoice(0, 0x130a); startVoice(1, 0x1480);
			startVoice(8, 0x154c);
		}
		break;
	case 29:
		// Image 0x093A has the same guard/command-1 structure, then
		// starts fixed channel 3 and rhythm channel 9.
		if (!isSequenceActive(0x15fc)) {
			requestStopAll();
			startVoice(2, 0x15fc); startVoice(8, 0x1652);
		}
		break;
	case 30: startEffectVoice(0x1108); startEffectVoice(0x10fe); break;
	case 31: startEffectVoice(0x1140); break;
	case 32: startEffectVoice(0x10c6); break;
	case 33: startEffectVoice(0x10d2); break;
	case 34: startEffectVoice(0x10dc); break;
	case 35: startEffectVoice(0x10ec); break;
	case 36: startEffectVoice(0x12e6); startEffectVoice(0x12fa); break;
	case 37: startEffectVoice(0x1120); break;
	}
	return 0;
}

/*
 * RSOUND.009 is the opening/cinematic overlay. Unlike sections 1-8 it has
 * no whole-update divider, and its allocator at image 0x0605 uses only fixed
 * MIDI channels 6, 7 and 8. Rhythm channel 9 is never allocator-owned.
 */
RSound9::RSound9(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.009", 5, false),
		_callbackCounter(0), _callbackPeriod(0),
		_callbackAction(kCallbackNone) {
}

void RSound9::clearCallback() {
	// Exact translation of the three zero stores in command 0.
	_callbackCounter = 0;
	_callbackPeriod = 0;
	_callbackAction = kCallbackNone;
}

void RSound9::resetSectionState() {
	clearCallback();
}

void RSound9::scheduleCallback(CallbackAction action) {
	// The original handler writes a near code address to DS:0054 and returns;
	// it does not start the cue immediately and does not alter the countdown.
	_callbackAction = action;
}

void RSound9::startFixedCue(const uint16 *sequenceOffsets,
		uint sequenceCount) {
	if (sequenceCount > kVoiceCount)
		error("Too many fixed Rex RSOUND voices");

	// Do not add an active-sequence guard or an implicit stop here. The opening
	// callback bodies call start_voice_1, start_voice_2, ... directly and may
	// deliberately replace fixed cinematic layers.
	for (uint index = 0; index < sequenceCount; ++index)
		startVoice(index, sequenceOffsets[index]);
}

void RSound9::setOpeningBranchMarkers(byte value) {
	// These are self-modifying sequence operands at DS:6841, DS:7DCD and
	// DS:8791. Values 2 and 0 are written exactly by commands 34/51 and the
	// command-50 callback respectively.
	sequenceData(0x6840, 2)[1] = value;
	sequenceData(0x7dcc, 2)[1] = value;
	sequenceData(0x8790, 2)[1] = value;
}

void RSound9::runCallback(CallbackAction action) {
	switch (action) {
	case kCallbackCommand38: {
		static const uint16 kSequences[] = {
			0x1878, 0x1f64, 0x308e, 0x2a10,
			0x21c8, 0x2558, 0x1e9a
		};
		startFixedCue(kSequences, 7);
		break;
	}
	case kCallbackCommand39: {
		static const uint16 kSequences[] = {
			0x1a24, 0x1fd0, 0x318a, 0x2e4a,
			0x2380, 0x2642, 0x1e9c
		};
		startFixedCue(kSequences, 7);
		break;
	}
	case kCallbackCommand40: {
		static const uint16 kSequences[] = {
			0x4f00, 0x534a, 0x5cde, 0x6f8e, 0x8110
		};
		startFixedCue(kSequences, 5);
		break;
	}
	case kCallbackCommand41: {
		static const uint16 kSequences[] = {
			0x4f26, 0x53bc, 0x5dfe, 0x747e, 0x8340
		};
		startFixedCue(kSequences, 5);
		break;
	}
	case kCallbackCommand42: {
		static const uint16 kSequences[] = {
			0x4f30, 0x555c, 0x6582, 0x7a2e, 0x8480
		};
		startFixedCue(kSequences, 5);
		break;
	}
	case kCallbackCommand44_46: {
		static const uint16 kSequences[] = {
			0x3518, 0x3aa2, 0x403a, 0x4486
		};
		startFixedCue(kSequences, 4);
		break;
	}
	case kCallbackCommand45: {
		static const uint16 kSequences[] = {
			0x37ac, 0x3cf8, 0x41e6, 0x4630
		};
		startFixedCue(kSequences, 4);
		break;
	}
	case kCallbackCommand47: {
		static const uint16 kSequences[] = {
			0x47d6, 0x48fa, 0x4a20, 0x4bae
		};
		startFixedCue(kSequences, 4);
		break;
	}
	case kCallbackCommand50: {
		static const uint16 kSequences[] = {
			0x50b8, 0x7dce, 0x676a, 0x7d08, 0x85c4
		};
		setOpeningBranchMarkers(0);
		startFixedCue(kSequences, 5);
		break;
	}
	default:
		break;
	}
}

void RSound9::sectionTimerTick() {
	// The opening timer at image 0x0C89 invokes this clock directly before
	// update_all_voices. There is no numbered-overlay update divider.
	if (!_callbackPeriod)
		return;

	// Static translation of the timer body at image 0x0CA1: decrement DS:0050,
	// reload it from DS:0052 at zero, then call the one-shot pointer in DS:0054.
	// The arithmetic intentionally preserves 16-bit underflow. Remaining
	// uncertainty is limited to real-world DOS-versus-host timer jitter.
	_callbackCounter = (uint16)(_callbackCounter - 1);
	if (_callbackCounter)
		return;

	_callbackCounter = _callbackPeriod;

	const CallbackAction action = _callbackAction;
	_callbackAction = kCallbackNone;
	runCallback(action);
}

int RSound9::executeOpeningCommonCommand(int commandId) {
	switch (commandId) {
	case 0:
		// Image 0x0B62 clears the callback words, resets all voices and
		// channels, and re-sends the first DT1. The opening timer has no
		// update-divider state.
		resetSectionState();
		return executeCommonCommand(0);
	case 1:
		requestStopRange(0, 9);
		return 0;
	case 2:
		stopAndResetRange(0, 5);
		// The handler at image 0x0B59 re-sends the payload beginning at DS:006F
		// after resetting MIDI channels 2-6. This is the first embedded DT1
		// record, omitted by both earlier snapshots.
		sendFirstMt32Record();
		return 0;
	case 3:
		requestStopRange(0, 5);
		return 0;
	case 4:
		stopAndResetRange(5, 4);
		return 0;
	case 5:
		requestStopRange(5, 4);
		return 0;
	case 6:
	case 7:
	case 8:
		return executeCommonCommand(commandId);
	default:
		return 0;
	}
}

// The opening command table contains 52 IDs and 51 unique handlers;
// commands 44 and 46 intentionally share one handler.
int RSound9::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);

	if (commandId < 0 || commandId > 51)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeOpeningCommonCommand(commandId);

	switch (commandId) {
	case 9: {
		static const uint16 kSequences[] = {
			0x16e4, 0x1e9e, 0x2f9c, 0x2644,
			0x1ff4, 0x2382, 0x1aae
		};
		_callbackCounter = 0x0738;
		_callbackPeriod = 0x0054;
		startFixedCue(kSequences, 7);
		break;
	}
	case 10: {
		static const uint16 kSequences[] = {
			0x31e2, 0x31fa, 0x3212, 0x322c
		};
		startFixedCue(kSequences, 4);
		break;
	}
	case 11:
		startVoice(7, 0x33e2);
		break;
	case 12:
		startVoice(7, 0x342e);
		break;
	case 13:
		startVoice(7, 0x343a);
		break;
	case 14:
		startVoice(7, 0x3442);
		break;
	case 15:
		startVoice(7, 0x3462);
		break;
	case 16:
		startVoice(7, 0x347a);
		break;
	case 17:
		startVoice(7, 0x3470);
		break;
	case 18:
		// Handler 0x0A09 tail-jumps to allocator 0x0605: channels 6-8 only.
		startEffectVoice(0x3248);
		break;
	case 19:
		startEffectVoice(0x3262);
		break;
	case 20: {
		byte *data = sequenceData(0x3284, 7);
		data[6] = (byte)(((getRandomNumber() & 0x18) + 0x4d) & 0x7f);
		startEffectVoice(0x3284);
		break;
	}
	case 21: {
		// Commands 21/22 mutate byte +9 before the melodic-only
		// stack-unwinding guard at image 0x0332.
		sequenceData(0x3298, 10)[9] = 0x46;
		if (!isSequenceActive(0x3298))
			startEffectVoice(0x3298);
		break;
	}
	case 22: {
		sequenceData(0x3298, 10)[9] = 0x2d;
		if (!isSequenceActive(0x3298))
			startEffectVoice(0x3298);
		break;
	}
	case 23: {
		static const uint16 kSequences[] = {
			0x32b0, 0x32b6, 0x32c8
		};
		// 0x32CE is a loop anchor installed on each channels-6-8
		// allocation, not a fourth sequence start. DOS writes through BX
		// without a visible failure check; native code checks safely.
		for (uint index = 0; index < 3; ++index) {
			const int voiceIndex = startEffectVoice(kSequences[index]);
			if (voiceIndex >= 0)
				voice((uint)voiceIndex).innerLoopStart = 0x32ce;
		}
		break;
	}
	case 24:
		startEffectVoice(0x32e0);
		break;
	case 25:
		startEffectVoice(0x32f6);
		break;
	case 26:
		startEffectVoice(0x331a);
		break;
	case 27:
		startEffectVoice(0x3332);
		break;
	case 28: {
		byte *data = sequenceData(0x334a, 7);
		data[6] = (byte)(((getRandomNumber() & 0x1c) + 0x0f) & 0x7f);
		startVoice(7, 0x334a);
		break;
	}
	case 29: {
		byte *data = sequenceData(0x335e, 7);
		data[6] = (byte)(((getRandomNumber() & 0x0c) + 0x21) & 0x7f);
		startEffectVoice(0x335e);
		break;
	}
	case 30:
		startEffectVoice(0x3386);
		break;
	case 31:
		startEffectVoice(0x3396);
		startEffectVoice(0x33a4);
		startEffectVoice(0x33b2);
		break;
	case 32:
		startEffectVoice(0x33c0);
		break;
	case 33:
		// The command-table handler tail-jumps after 0x33CA. The adjacent
		// function at image 0x0B21 starts 0x33D8 but is not command 33.
		startEffectVoice(0x33ca);
		break;
	case 34: {
		static const uint16 kSequences[] = {
			0x4d2a, 0x51aa, 0x5634, 0x6844, 0x7dd0
		};
		requestStopAll();
		_callbackCounter = _callbackPeriod = 0x0060;
		setOpeningBranchMarkers(2);
		startFixedCue(kSequences, 5);
		break;
	}
	case 35:
		startEffectVoice(0x344c);
		break;
	case 36: {
		// All three starts use the opening allocator, so this command can
		// occupy only channels 6-8.
		startEffectVoice(0x334a);

		int voiceIndex = startEffectVoice(0x32c2);
		if (voiceIndex >= 0)
			voice((uint)voiceIndex).innerLoopStart = 0x3378;

		voiceIndex = startEffectVoice(0x32bc);
		if (voiceIndex >= 0)
			voice((uint)voiceIndex).innerLoopStart = 0x3368;
		break;
	}
	case 37: {
		byte *data = sequenceData(0x349c, 7);
		data[6] = (byte)(((getRandomNumber() & 0x02) + 0x48) & 0x7f);
		startEffectVoice(0x349c);
		break;
	}
	case 38:
		scheduleCallback(kCallbackCommand38);
		break;
	case 39:
		// The command table points to image 0x0782, which only installs callback
		// 0x079A. A separate routine at 0x078B reads a parameter and compares it
		// with DS:0054, but no command-table or direct caller has been confirmed.
		// Keep command 39 parameter-free unless a DOS trace proves that helper is
		// reachable through another game-facing path.
		scheduleCallback(kCallbackCommand39);
		break;
	case 40:
		scheduleCallback(kCallbackCommand40);
		break;
	case 41:
		scheduleCallback(kCallbackCommand41);
		break;
	case 42:
		scheduleCallback(kCallbackCommand42);
		break;
	case 43: {
		static const uint16 kSequences[] = {
			0x34be, 0x3a46, 0x3f52, 0x439a
		};
		_callbackCounter = _callbackPeriod = 0x0050;
		startFixedCue(kSequences, 4);
		break;
	}
	case 44:
	case 46:
		scheduleCallback(kCallbackCommand44_46);
		break;
	case 45:
		scheduleCallback(kCallbackCommand45);
		break;
	case 47:
		scheduleCallback(kCallbackCommand47);
		break;
	case 48: {
		byte *data = sequenceData(0x34b0, 11);
		// The original command mutates the loaded resource in place. Save/load
		// behavior across a live section still needs DOS comparison because the
		// ScummVM resource is reconstructed when the section driver is reloaded.
		data[6] ^= 0x1f;
		data[10] ^= 0x1f;
		startEffectVoice(0x34b0);
		break;
	}
	case 49: {
		static const uint16 kSequences[] = {
			0x12ca, 0x132a, 0x1384, 0x1666,
			0x1682, 0x16a0, 0x16be
		};
		startFixedCue(kSequences, 7);
		break;
	}
	case 50:
		scheduleCallback(kCallbackCommand50);
		break;
	case 51: {
		static const uint16 kSequences[] = {
			0x4d3c, 0x51ee, 0x5a62, 0x6986, 0x7e5c
		};
		requestStopAll();
		_callbackCounter = _callbackPeriod = 0x0060;
		setOpeningBranchMarkers(2);
		startFixedCue(kSequences, 5);
		break;
	}
	}

	return 0;
}

/*
 * The demo opening overlay predates the retail opening controller. It has no
 * deferred callback clock: commands 9 and 10 are no-ops, commands 34 and 39
 * are aliases, and every non-fixed allocation uses MIDI channels 6-8.
 */
RSoundDemo9::RSoundDemo9(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver) :
		RexRSound(mixer, opl, midiDriver, "RSOUND.009", 5, false) {
}

int RSoundDemo9::executeDemoCommonCommand(int commandId) {
	switch (commandId) {
	case 0:
		resetSectionState();
		return executeCommonCommand(0);
	case 1:
		requestStopRange(0, 9);
		return 0;
	case 2:
		stopAndResetRange(0, 5);
		sendFirstMt32Record();
		return 0;
	case 3:
		requestStopRange(0, 5);
		return 0;
	case 4:
		stopAndResetRange(5, 4);
		return 0;
	case 5:
		requestStopRange(5, 4);
		return 0;
	case 6:
	case 7:
	case 8:
		return executeCommonCommand(commandId);
	default:
		return 0;
	}
}

int RSoundDemo9::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);

	if (commandId < 0 || commandId > 39)
		return 0;
	beginCommand(param);
	if (commandId <= 8)
		return executeDemoCommonCommand(commandId);

	switch (commandId) {
	case 9:
	case 10:
		break;

	case 11:
		startVoice(7, 0x1454);
		break;
	case 12:
		startVoice(7, 0x14a0);
		break;
	case 13:
		startVoice(7, 0x14ac);
		break;
	case 14:
		startVoice(7, 0x14b4);
		break;
	case 15:
		startVoice(7, 0x14d4);
		break;
	case 16:
		startVoice(7, 0x14ec);
		break;
	case 17:
		startVoice(7, 0x14e2);
		break;

	case 18:
		startEffectVoice(0x12ba);
		break;
	case 19:
		startEffectVoice(0x12d4);
		break;
	case 20: {
		byte *data = sequenceData(0x12f6, 7);
		data[6] = (byte)(((getRandomNumber() & 0x38) + 0x4d) & 0x7f);
		startEffectVoice(0x12f6);
		break;
	}
	case 21:
	case 22: {
		byte *data = sequenceData(0x130a, 10);
		data[9] = commandId == 21 ? 0x46 : 0x2d;
		if (!isSequenceActive(0x130a))
			startEffectVoice(0x130a);
		break;
	}
	case 23: {
		static const uint16 kSequences[] = {
			0x1322, 0x1328, 0x133a
		};
		for (uint index = 0; index < ARRAYSIZE(kSequences); ++index) {
			const int voiceIndex = startEffectVoice(kSequences[index]);
			if (voiceIndex >= 0)
				voice((uint)voiceIndex).innerLoopStart = 0x1340;
		}
		break;
	}
	case 24:
		startEffectVoice(0x1352);
		break;
	case 25:
		startEffectVoice(0x1368);
		break;
	case 26:
		startEffectVoice(0x138c);
		break;
	case 27:
		startEffectVoice(0x13a4);
		break;
	case 28: {
		byte *data = sequenceData(0x13bc, 7);
		data[6] = (byte)(((getRandomNumber() & 0x1c) + 0x0f) & 0x7f);
		startEffectVoice(0x13bc);
		break;
	}
	case 29: {
		byte *data = sequenceData(0x13d0, 7);
		data[6] = (byte)(((getRandomNumber() & 0x0c) + 0x21) & 0x7f);
		startEffectVoice(0x13d0);
		break;
	}
	case 30:
		startEffectVoice(0x13f8);
		break;
	case 31:
		startEffectVoice(0x1408);
		startEffectVoice(0x1416);
		startEffectVoice(0x1424);
		break;
	case 32:
		startEffectVoice(0x1432);
		break;
	case 33:
		startEffectVoice(0x143c);
		break;
	case 34:
	case 39:
		startVoice(0, 0x1522);
		startVoice(1, 0x1700);
		startVoice(2, 0x1892);
		startVoice(3, 0x21f2);
		startVoice(4, 0x2e4a);
		break;
	case 35:
		startEffectVoice(0x14be);
		break;
	case 36: {
		startEffectVoice(0x13bc);

		int voiceIndex = startEffectVoice(0x1334);
		if (voiceIndex >= 0)
			voice((uint)voiceIndex).innerLoopStart = 0x13ea;

		voiceIndex = startEffectVoice(0x132e);
		if (voiceIndex >= 0)
			voice((uint)voiceIndex).innerLoopStart = 0x13da;
		break;
	}
	case 37: {
		byte *data = sequenceData(0x150e, 7);
		data[6] = (byte)(((getRandomNumber() & 0x02) + 0x48) & 0x7f);
		startEffectVoice(0x150e);
		break;
	}
	case 38:
		startVoice(0, 0x35c4);
		startVoice(1, 0x35ce);
		startVoice(2, 0x3612);
		startVoice(3, 0x3656);
		break;
	}

	return 0;
}

} // namespace RexNebular
} // namespace MADS
