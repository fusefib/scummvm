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

#ifndef WIZARDRY_VM_MACHINE_H
#define WIZARDRY_VM_MACHINE_H

#include "wizardry/vm/codefile.h"
#include "wizardry/vm/host.h"

namespace Wizardry {

class PCodeConsole;

/** Host persistence and optional diagnostics, independent of the frontend. */
class MachineObserver {
public:
	virtual ~MachineObserver() {}
	virtual void beforeInput(byte) {}
	virtual void onDelay(uint16) {}
};

/** Cooperative p-machine for the Wizardry I-V DOS interpreters. */
class PCodeMachine : public MachineHost {
public:
	enum State { kRunning,
				 kWaitingForInput,
				 kDelayed,
				 kHalted,
				 kFault };
	PCodeMachine();
	~PCodeMachine() override;
	bool boot(const Common::Array<byte> &code, const Common::Array<byte> &disk, Common::String &error);
	State run(uint32 instructionBudget = 20000);
	void queueKey(byte key);
	void clearKeys();
	void setScriptedInput(bool scripted) { _scriptedInput = scripted; }
	void setObserver(MachineObserver *observer) { _observer = observer; }
	void setPreserveLocals(bool preserve) { _preserveLocals = preserve; }
	State state() const { return _state; }
	const Common::String &error() const { return _error; }
	uint64 instructions() const { return _instructions; }
	uint16 delayTicks() const { return _delayTicks; }
	byte currentSegment() const { return _segment; }
	uint32 currentIP() const { return _ip; }
	byte currentProcedure() const;
	PCodeConsole &console() { return *_console; }
	const PCodeConsole &console() const { return *_console; }
	const Common::Array<byte> &disk() const { return _disk; }
	const PCodeFile &codeFile() const { return _code; }
	bool canSuspend() const { return _state != kFault && _state != kHalted && !_frames.empty(); }

	byte readByte(uint16 address) const override { return _memory[address]; }
	uint16 readWord(uint16 address) const override;
	void writeByte(uint16 address, byte value) override { _memory[address] = value; }
	void writeWord(uint16 address, uint16 value) override;
	bool readDisk(uint32 block, uint32 size, byte *destination) override;
	bool keyAvailable() const override { return _keyPosition < _keys.size(); }
	void setIoResult(uint16 value) override { _ioResult = value; }
	void yieldForInput() override;
	void delay(uint16 ticks) override;
	void sound(uint16, uint16 pattern, uint16) override;
	void fail(const Common::String &message) override;

private:
	enum StaticMode {
		kLocal,
		kSearch,
		kGlobal,
		kRoot
	};
	struct Frame {
		byte segment = 0;
		byte procedure = 0;
		byte returnSegment = 0;
		uint16 base = 0;
		uint16 returnIP = 0;
		uint16 savedNP = 0;
		int32 staticFrame = -1;
		uint32 stackBase = 0;
	};
	struct Literal {
		byte segment = 0;
		uint16 offset = 0;
		uint16 length = 0;
		uint16 address = 0;
	};
	struct PendingInput {
		bool active = false;
		uint16 address = 0;
		uint16 count = 0;
		uint16 position = 0;
	};
	PCodeFile _code;
	Common::Array<byte> _disk;
	uint32 _sourceDiskCRC = 0;
	byte _memory[65536];
	Common::Array<uint16> _stack;
	Common::Array<Frame> _frames;
	Common::Array<Literal> _literals;
	Common::Array<byte> _keys;
	uint32 _keyPosition = 0;
	uint32 _ip = 0;
	uint16 _heap = 0x6000;
	uint16 _np = 0xf000;
	uint16 _literalNext = 0x0800;
	uint16 _ioResult = 0;
	uint16 _delayTicks = 0;
	byte _segment = 0;
	uint64 _instructions = 0;
	State _state = kHalted;
	PendingInput _input;
	bool _scriptedInput = false;
	bool _preserveLocals = false;
	bool _idleSnapshot = false;
	Common::String _error;
	PCodeConsole *_console = nullptr;
	MachineObserver *_observer = nullptr;

	byte fetchByte();
	uint16 fetchWord();
	uint16 big();
	uint16 pop();
	void push(uint16 value);
	bool requireStack(uint32 count);
	const PCodeProcedure *activeProcedure() const;
	void enter(byte segment, byte procedure, StaticMode mode);
	void leave(byte returnWords);
	int32 walk(byte levels);
	uint16 localAddress(int32 frame, uint16 word);
	uint16 literal(uint16 offset, uint16 length);
	void execute(byte opcode);
	void jump(bool take);
	void caseJump();
	void compare(byte opcode);
	void standardProcedure(byte number);
	void unitIO(bool write);
	void completeInput();
	void moveBytes(uint16 destination, uint16 source, uint32 size, bool backward = false);
	void moveWords(uint16 destination, uint16 source, uint16 count);
	void fillMemory(uint16 destination, byte value, uint32 size);
	Common::String pascalString(uint16 address) const;
};

} // namespace Wizardry
#endif
