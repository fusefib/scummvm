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

#include "wizardry/vm/machine.h"
#include "common/endian.h"
#include "common/util.h"
#include "wizardry/vm/console.h"
#include "wizardry/vm/timing.h"

namespace Wizardry {

static uint16 packedFieldMask(uint16 width, byte revision) {
	if (width)
		return (1U << width) - 1;
	// Index zero aliases the STP jump displacement before the mask table.
	switch (revision) {
	case 1:
		return 0xced0;
	case 4:
		return 0xcdbd;
	case 5:
		return 0xccde;
	default:
		return 0;
	}
}

PCodeMachine::PCodeMachine() {
	memset(_memory, 0, sizeof(_memory));
	_console = new PCodeConsole(*this);
}

PCodeMachine::~PCodeMachine() {
	delete _console;
}

bool PCodeMachine::boot(const Common::Array<byte> &code, const Common::Array<byte> &disk, Common::String &error) {
	PCodeFile parsed;
	if (!parsed.load(code, error))
		return false;
	if (!parsed.procedure(1, 1) || disk.size() < 6 * 512 || disk.size() > 2 * 1024 * 1024 || (disk.size() % 512)) {
		error = "Invalid Wizardry program or linear disk image";
		return false;
	}
	_code = Common::move(parsed);
	_disk = disk;
	_sourceDiskCRC = wizardryCrc32(disk.data(), disk.size());
	memset(_memory, 0, sizeof(_memory));
	_stack.clear();
	_frames.clear();
	_literals.clear();
	clearKeys();
	_error.clear();
	_heap = 0x6000;
	_np = 0xf000;
	_literalNext = 0x0800;
	_ioResult = _delayTicks = 0;
	_waitRemainingUs = 0;
	_soundPattern = 0xffff;
	_instructions = 0;
	_input = PendingInput();
	_idleSnapshot = false;
	_state = kRunning;
	delete _console;
	_console = new PCodeConsole(*this);
	// HAS.CACHE activates the optional native RAM-disk cache, whose ABI is
	// not implemented. Mark it absent in this private working image only.
	uint count = READ_LE_UINT16(_disk.data() + 1024 + 16);
	if (count > 77) {
		error = "Invalid UCSD directory count";
		_state = kFault;
		return false;
	}
	for (uint i = 0; i < count; ++i) {
		uint off = 1024 + 26 * (i + 1);
		if (_disk[off + 6] == 9 && !memcmp(_disk.data() + off + 7, "HAS.CACHE", 9)) {
			_disk[off + 4] = _disk[off + 5] = 0;
		}
	}
	_segment = 1;
	_ip = 0;
	push(0);
	push(0);
	enter(1, 1, kRoot);
	if (_state == kFault) {
		error = _error;
		return false;
	}
	writeWord(_frames[0].base, 0x0700);
	writeWord(_frames[0].base + 2, 0);
	error.clear();
	return true;
}

uint16 PCodeMachine::readWord(uint16 address) const {
	return uint16(_memory[address] | (uint16(_memory[uint16(address + 1)]) << 8));
}

void PCodeMachine::writeWord(uint16 address, uint16 value) {
	_memory[address] = byte(value);
	_memory[uint16(address + 1)] = byte(value >> 8);
}

bool PCodeMachine::readDisk(uint32 block, uint32 size, byte *destination) {
	if (block > _disk.size() / 512 || size > _disk.size() - block * 512) {
		_ioResult = 10;
		return false;
	}
	if (size)
		memcpy(destination, _disk.data() + block * 512, size);
	return true;
}

void PCodeMachine::queueKey(byte key) {
	if (!key)
		return;
	if (_keyPosition == _keys.size())
		clearKeys();
	// Bound the stored queue as well as the unread suffix. Retaining consumed
	// prefixes across partial refills could otherwise exceed the snapshot's
	// 4096-byte limit even though at most 4096 keys remained pending.
	if (_keyPosition && _keys.size() >= 4096) {
		const uint32 unread = _keys.size() - _keyPosition;
		memmove(_keys.data(), _keys.data() + _keyPosition, unread);
		_keys.resize(unread);
		_keyPosition = 0;
	}
	if (_keys.size() - _keyPosition < 4096)
		_keys.push_back(key);
	_idleSnapshot = false;
}

void PCodeMachine::clearKeys() {
	_keys.clear();
	_keyPosition = 0;
}

void PCodeMachine::fail(const Common::String &message) {
	if (_state == kFault)
		return;
	_error = Common::String::format("%s (segment %u, procedure %u, IP 0x%04x)",
		message.c_str(), _segment, currentProcedure(), _ip);
	_state = kFault;
}

void PCodeMachine::yieldForInput() {
	if (_state != kFault) {
		_state = kWaitingForInput;
		if (!_idleSnapshot && _observer) {
			_observer->beforeInput(0);
			_idleSnapshot = true;
		}
	}
}

void PCodeMachine::delay(uint16 ticks) {
	if (_observer)
		_observer->onDelay(ticks);
	_delayTicks = ticks;
	if (_state != kFault) {
		_waitRemainingUs = NativeTiming::delayMicroseconds(ticks);
		_soundPattern = 0xffff;
		_state = kDelayed;
	}
}

void PCodeMachine::sound(uint16, uint16 pattern, uint16) {
	if (_realtime && _state != kFault && pattern <= 2 && !(_biosModifiers & 0x40)) {
		_soundPattern = pattern;
		_waitRemainingUs = NativeTiming::soundMicroseconds(pattern);
		_state = kPlayingSound;
	}
}

void PCodeMachine::advanceTime(uint32 milliseconds) {
	if (_state != kDelayed && _state != kPlayingSound)
		return;
	if (_state == kDelayed && (keyAvailable() || (_biosModifiers & 0x1f))) {
		_waitRemainingUs = 0;
		return;
	}
	const uint64 elapsed = uint64(milliseconds) * 1000;
	_waitRemainingUs = elapsed >= _waitRemainingUs ? 0 : _waitRemainingUs - uint32(elapsed);
}

uint32 PCodeMachine::soundElapsedUs() const {
	return _state == kPlayingSound ? NativeTiming::soundMicroseconds(_soundPattern) - _waitRemainingUs : 0;
}

byte PCodeMachine::currentProcedure() const {
	return _frames.empty() ? 0 : _frames.back().procedure;
}

const PCodeProcedure *PCodeMachine::activeProcedure() const {
	return _frames.empty() ? nullptr : _code.procedure(_frames.back().segment, _frames.back().procedure);
}

byte PCodeMachine::fetchByte() {
	const PCodeSegment *s = _code.segment(_segment);
	if (!s || _ip >= s->code.size()) {
		fail("Instruction fetch outside segment");
		return 0;
	}
	return s->code[_ip++];
}

uint16 PCodeMachine::fetchWord() {
	const uint lo = fetchByte();
	return lo | (uint16(fetchByte()) << 8);
}

uint16 PCodeMachine::big() {
	uint b = fetchByte();
	return (b & 0x80) ? ((b & 0x7f) << 8) | fetchByte() : b;
}

bool PCodeMachine::requireStack(uint32 count) {
	if (_stack.size() < count) {
		fail("Evaluation stack underflow");
		return false;
	}
	return true;
}

uint16 PCodeMachine::pop() {
	if (!requireStack(1))
		return 0;
	uint16 value = _stack.back();
	_stack.pop_back();
	return value;
}

void PCodeMachine::push(uint16 value) {
	if (_stack.size() >= 16384) {
		fail("Evaluation stack overflow");
		return;
	}
	_stack.push_back(value);
}

void PCodeMachine::enter(byte sn, byte pn, StaticMode mode) {
	const PCodeProcedure *p = _code.procedure(sn, pn);
	if (!p) {
		fail("Call to missing procedure");
		return;
	}
	const uint bytes = uint(p->parameterBytes) + p->localBytes;
	if (bytes > _np || _np - bytes < uint(_heap) + 64 || _frames.size() >= 512) {
		fail("Activation stack overflow");
		return;
	}
	if (!requireStack(p->parameterBytes / 2))
		return;
	Frame f;
	f.segment = sn;
	f.procedure = pn;
	f.returnSegment = _segment;
	f.returnIP = _ip;
	f.savedNP = _np;
	_np -= bytes;
	f.base = _np;
	for (uint i = 0; i < p->parameterBytes / 2; ++i)
		writeWord(f.base + 2 * i, pop());
	if (!_preserveLocals) {
		for (uint i = p->parameterBytes; i < bytes; ++i)
			_memory[uint16(f.base + i)] = 0;
	}
	f.stackBase = _stack.size();
	if (mode == kLocal)
		f.staticFrame = int32(_frames.size()) - 1;
	else if (mode == kGlobal)
		f.staticFrame = _frames.empty() ? -1 : 0;
	else if (mode == kSearch) {
		const int want = int(p->level) - 1;
		for (int i = int(_frames.size()) - 1; i >= 0; --i) {
			const PCodeProcedure *candidate = _code.procedure(_frames[i].segment, _frames[i].procedure);
			if (candidate && candidate->level == want) {
				f.staticFrame = i;
				break;
			}
		}
	}
	_frames.push_back(f);
	_segment = sn;
	_ip = p->entry;
}

void PCodeMachine::leave(byte nret) {
	if (_frames.empty()) {
		fail("Return with no activation frame");
		return;
	}
	const Frame f = _frames.back();
	const PCodeProcedure *p = activeProcedure();
	if (!p || 2U * nret > uint(p->parameterBytes) + p->localBytes || f.stackBase > _stack.size()) {
		fail("Invalid procedure return");
		return;
	}
	_stack.resize(f.stackBase);
	for (int i = nret - 1; i >= 0; --i)
		push(readWord(f.base + 2 * i));
	_np = f.savedNP;
	_frames.pop_back();
	_ip = f.returnIP;
	_segment = f.returnSegment;
	if (_frames.empty())
		_state = kHalted;
}

int32 PCodeMachine::walk(byte levels) {
	int32 f = int32(_frames.size()) - 1;
	while (levels-- && f >= 0)
		f = _frames[f].staticFrame;
	if (f < 0 || f >= int32(_frames.size())) {
		fail("Invalid static link");
		return -1;
	}
	return f;
}

uint16 PCodeMachine::localAddress(int32 frame, uint16 word) {
	if (frame < 0 || frame >= int32(_frames.size()) || !word) {
		fail("Invalid local/global address");
		return 0;
	}
	const Frame &f = _frames[frame];
	return uint16(f.base + 2 * (word - 1));
}

uint16 PCodeMachine::literal(uint16 offset, uint16 length) {
	for (uint i = 0; i < _literals.size(); ++i)
		if (_literals[i].segment == _segment && _literals[i].offset == offset)
			return _literals[i].address;
	const PCodeSegment *s = _code.segment(_segment);
	const uint32 next = uint32(_literalNext) + ((uint32(length) + 2) & ~1U);
	if (!s || offset > s->code.size() || length > s->code.size() - offset || next > 0x6000) {
		fail("Invalid or exhausted literal pool");
		return 0;
	}
	Literal lit;
	lit.segment = _segment;
	lit.offset = offset;
	lit.length = length;
	lit.address = _literalNext;
	memcpy(_memory + lit.address, s->code.data() + offset, length);
	_literalNext = next;
	_literals.push_back(lit);
	return lit.address;
}

void PCodeMachine::moveBytes(uint16 dst, uint16 src, uint32 size, bool backward) {
	if (size > 65536) {
		fail("Oversized memory move");
		return;
	}
	// Original MOVELEFT/SAS use REP MOVSB; MOVERIGHT sets DF first. A
	// temporary buffer (or memmove) loses the original overlap behavior.
	for (uint32 n = 0; n < size; ++n) {
		const uint32 i = backward ? size - n - 1 : n;
		writeByte(uint16(dst + i), readByte(uint16(src + i)));
	}
}

void PCodeMachine::moveWords(uint16 dst, uint16 src, uint16 count) {
	// MOV is REP MOVSW, not a sequence of byte copies. The distinction is
	// observable for odd overlapping addresses: each word is read first.
	for (uint32 i = 0; i < count; ++i)
		writeWord(uint16(dst + 2 * i), readWord(uint16(src + 2 * i)));
}

void PCodeMachine::fillMemory(uint16 dst, byte value, uint32 size) {
	if (size > 65536) {
		fail("Oversized memory fill");
		return;
	}
	for (uint32 i = 0; i < size; ++i)
		writeByte(uint16(dst + i), value);
}

Common::String PCodeMachine::pascalString(uint16 address) const {
	Common::String s;
	const uint size = readByte(address);
	for (uint i = 0; i < size; ++i)
		s += char(readByte(uint16(address + 1 + i)));
	return s;
}

PCodeMachine::State PCodeMachine::run(uint32 budget) {
	if (_state == kHalted || _state == kFault)
		return _state;
	if (_realtime && (_state == kDelayed || _state == kPlayingSound)) {
		advanceTime(0);
		if (_waitRemainingUs)
			return _state;
	}
	_state = kRunning;
	_delayTicks = 0;
	_waitRemainingUs = 0;
	_soundPattern = 0xffff;
	if (_input.active)
		completeInput();
	for (uint32 n = 0; n < budget && _state == kRunning; ++n) {
		const byte op = fetchByte();
		if (_state == kFault)
			break;
		++_instructions;
		if (op < 0x80)
			push(op);
		else if (op >= 0xd8 && op <= 0xe7)
			push(readWord(localAddress(int32(_frames.size()) - 1, op - 0xd8 + 1)));
		else if (op >= 0xe8 && op <= 0xf7)
			push(readWord(localAddress(0, op - 0xe8 + 1)));
		else if (op >= 0xf8)
			push(readWord(uint16(pop() + 2 * (op - 0xf8))));
		else
			execute(op);
	}
	return _state;
}

void PCodeMachine::execute(byte op) {
	uint16 a = 0, b = 0, c = 0, d = 0;
	int32 sa = 0, sb = 0;
	switch (op) {
	case 0x80:
		sa = int16(pop());
		push(sa < 0 ? -sa : sa);
		break; // ABI
	case 0x82:
		b = pop();
		a = pop();
		push(a + b);
		break;
	case 0x84:
		b = pop();
		a = pop();
		push(a & b);
		break;
	case 0x86: // DVI: truncate toward zero, including INT16_MIN / -1.
	case 0x8e: // MODI: signed remainder follows the dividend.
		sb = int16(pop());
		sa = int16(pop());
		if (!sb) {
			fail("Division by zero");
			break;
		}
		push(op == 0x86 ? sa / sb : sa % sb);
		break;
	case 0x88: // CHK keeps the index on the stack.
		c = pop();
		b = pop();
		a = pop();
		if (int16(a) < int16(b) || int16(a) > int16(c))
			fail("Range check failed");
		push(a);
		break;
	case 0x8b: // INN: count, reversed set words, then the member below them.
		b = pop();
		if (!requireStack(uint32(b) + 1))
			break;
		a = _stack[_stack.size() - b - 1];
		c = (a < 512 && a < uint32(b) * 16) ? ((_stack[_stack.size() - 1 - a / 16] >> (a & 15)) & 1) : 0;
		_stack.resize(_stack.size() - b - 1);
		push(c);
		break;
	case 0x8d:
		b = pop();
		a = pop();
		push(a | b);
		break;
	case 0x8f:
		sb = int16(pop());
		sa = int16(pop());
		push(sa * sb);
		break;
	case 0x91:
		sa = int16(pop());
		push(-sa);
		break;
	case 0x93:
		push(~pop());
		break;
	case 0x95:
		b = pop();
		a = pop();
		push(a - b);
		break;
	case 0x98:
		sa = int16(pop());
		push(sa * sa);
		break;
	case 0x9a:
		b = pop();
		a = pop();
		writeWord(a, b);
		break;
	case 0x9b:
		b = pop();
		a = pop();
		if (!b || b > 255 || b > readByte(a))
			fail("String index out of range");
		push(a);
		push(b);
		break;
	case 0x9e:
		standardProcedure(fetchByte());
		break;
	case 0x9f:
		push(0);
		break;
	case 0xa1:
		a = pop();
		jump(!(a & 1));
		break;
	case 0xa2:
		b = big();
		a = pop();
		push(a + 2 * b);
		break;
	case 0xa3:
		b = big();
		a = pop();
		push(readWord(uint16(a + 2 * b)));
		break;
	case 0xa4:
		c = big();
		sb = int16(pop());
		a = pop();
		push(a + 2 * int32(c) * sb);
		break;
	case 0xa5:
		push(localAddress(0, big()));
		break;
	case 0xa6: {
		uint32 offset = _ip;
		uint size = fetchByte();
		push(literal(offset, size + 1));
		_ip += size;
	} break;
	case 0xa8:
		c = big();
		b = pop();
		a = pop();
		moveWords(a, b, c);
		break;
	case 0xa9:
		push(readWord(localAddress(0, big())));
		break;
	case 0xaa:
		c = fetchByte();
		b = pop();
		a = pop();
		if (b < 256) {
			if (c < 1)
				fail("SAS character exceeds target capacity");
			writeByte(a, 1);
			writeByte(uint16(a + 1), b);
		} else {
			d = readByte(b);
			if (d > c) {
				fail("SAS string exceeds target capacity");
				break;
			}
			moveBytes(a, b, d + 1);
		}
		break;
	case 0xab:
		a = localAddress(0, big());
		writeWord(a, pop());
		break;
	case 0xac:
		caseJump();
		break;
	case 0xad:
		leave(fetchByte());
		break;
	case 0xae:
		enter(_segment, fetchByte(), kSearch);
		break;
	case 0xaf:
	case 0xb0:
	case 0xb1:
	case 0xb4:
	case 0xb5:
	case 0xb7:
		compare(op);
		break;
	case 0xb2:
	case 0xb6:
	case 0xb8: {
		int32 frame = walk(fetchByte());
		a = localAddress(frame, big());
		if (op == 0xb2)
			push(a);
		else if (op == 0xb6)
			push(readWord(a));
		else
			writeWord(a, pop());
	} break;
	case 0xb3: { // LDC: word-aligned literal words, pushed in file order.
		a = fetchByte();
		_ip = (_ip + 1) & ~1U;
		const PCodeSegment *segment = _code.segment(_segment);
		if (!a || !segment || _ip > segment->code.size() || 2U * a > segment->code.size() - _ip) {
			fail("Invalid multiword constant extent");
			break;
		}
		if (_stack.size() + a > 16384) {
			fail("Evaluation stack overflow");
			break;
		}
		for (uint i = 0; i < a; ++i)
			push(fetchWord());
		break;
	}
	case 0xb9:
		jump(true);
		break;
	case 0xba:
		c = pop();
		b = pop();
		a = pop();
		if (b > 16 || c > 15 || b + c > 16) {
			fail("Invalid packed field");
			break;
		}
		push((readWord(a) >> c) & packedFieldMask(b, 0));
		break;
	case 0xbb:
		d = pop();
		c = pop();
		b = pop();
		a = pop();
		if (b > 16 || c > 15 || b + c > 16) {
			fail("Invalid packed field");
			break;
		}
		{
			uint32 mask = uint32(packedFieldMask(b, 0)) << c;
			writeWord(a, (readWord(a) & ~mask) | ((uint32(d) << c) & mask));
		}
		break;
	case 0xbe:
		b = pop();
		a = pop();
		push(readByte(uint16(a + b)));
		break;
	case 0xbf:
		c = pop();
		b = pop();
		a = pop();
		writeByte(uint16(a + b), c);
		break;
	case 0xc0:
		c = fetchByte();
		d = fetchByte();
		b = pop();
		a = pop();
		if (!c || !d || d > 16 || c * d > 16) {
			fail("Invalid packed-array descriptor");
			break;
		}
		push(a + 2 * (b / c));
		push(d);
		push((b % c) * d);
		break;
	case 0xc1:
		// INTERP op_RBP restores the previous base context, then shares
		// RNP's return sequence. The supplied I-III programs use only RBP 0
		// in WIZARDRY:1, returning to the host rather than another Pascal
		// base program. Other base-return contexts need a separate model.
		a = fetchByte();
		if (_state == kFault)
			break;
		if (a || _frames.size() != 1 || _frames[0].segment != 1 || _frames[0].procedure != 1) {
			fail("Unsupported base-program return context");
			break;
		}
		leave(0);
		break;
	case 0xc3:
		b = pop();
		a = pop();
		push(a == b);
		break;
	case 0xc4:
		sb = int16(pop());
		sa = int16(pop());
		push(sa >= sb);
		break;
	case 0xc5:
		sb = int16(pop());
		sa = int16(pop());
		push(sa > sb);
		break;
	case 0xc6:
		push(localAddress(int32(_frames.size()) - 1, big()));
		break;
	case 0xc7:
		push(fetchWord());
		break;
	case 0xc8:
		sb = int16(pop());
		sa = int16(pop());
		push(sa <= sb);
		break;
	case 0xc9:
		sb = int16(pop());
		sa = int16(pop());
		push(sa < sb);
		break;
	case 0xca:
		push(readWord(localAddress(int32(_frames.size()) - 1, big())));
		break;
	case 0xcb:
		b = pop();
		a = pop();
		push(a != b);
		break;
	case 0xcc:
		a = localAddress(int32(_frames.size()) - 1, big());
		writeWord(a, pop());
		break;
	case 0xcd:
		a = fetchByte();
		b = fetchByte();
		enter(a ? a : _segment, b, kSearch);
		break;
	case 0xce:
		enter(_segment, fetchByte(), kLocal);
		break;
	case 0xcf:
		enter(_segment, fetchByte(), kGlobal);
		break;
	case 0xd2:
	case 0xd7:
		break;
	case 0xd6:
		_state = kHalted;
		break;
	default:
		fail(Common::String::format("Unimplemented opcode 0x%02x", op));
		break;
	}
}

void PCodeMachine::jump(bool take) {
	const byte displacement = fetchByte();
	if (!take)
		return;
	if (displacement < 0x80) {
		_ip += displacement;
	} else {
		const PCodeProcedure *p = activeProcedure();
		const PCodeSegment *s = _code.segment(_segment);
		const int slot = p ? int(p->anchor) + int(displacement) - 256 : -1;
		if (!s || slot < 0 || slot + 2 > int(s->code.size())) {
			fail("Invalid backward jump slot");
			return;
		}
		const int target = slot - READ_LE_UINT16(s->code.data() + slot);
		if (target < 0) {
			fail("Invalid backward jump target");
			return;
		}
		_ip = target;
	}
}

void PCodeMachine::caseJump() {
	if (_ip & 1)
		++_ip;
	const int32 low = int16(fetchWord());
	const int32 high = int16(fetchWord());
	const int32 value = int16(pop());
	if (value < low || value > high)
		return; // Execute the default UJP at the current IP.
	const PCodeSegment *s = _code.segment(_segment);
	const uint32 slot = _ip + 2 + 2 * (value - low);
	if (!s || slot + 2 > s->code.size()) {
		fail("Invalid case jump table");
		return;
	}
	const uint32 relative = READ_LE_UINT16(s->code.data() + slot);
	if (relative > slot) {
		fail("Invalid case jump target");
		return;
	}
	_ip = slot - relative;
}

void PCodeMachine::compare(byte opcode) {
	const byte type = fetchByte();
	int order = 0;
	if (type == 4) {
		const uint16 b = pop(), a = pop();
		Common::String left = a < 256 ? Common::String(char(a)) : pascalString(a);
		Common::String right = b < 256 ? Common::String(char(b)) : pascalString(b);
		const uint length = MIN(left.size(), right.size());
		for (uint i = 0; i < length && !order; ++i)
			order = int(byte(left[i])) - int(byte(right[i]));
		if (!order)
			order = int(left.size()) - int(right.size());
	} else if (type == 6) {
		const uint16 b = pop() & 1, a = pop() & 1;
		order = int(a) - int(b);
	} else if (type == 10 || type == 12) {
		const uint32 size = uint32(big()) * (type == 12 ? 2 : 1);
		const uint16 b = pop(), a = pop();
		if (size > 65536) {
			fail("Oversized array comparison");
			return;
		}
		if (type == 12) {
			// The original uses unsigned CMPSW ordering. Comparing the
			// little-endian byte encoding reverses e.g. 0100h versus 00ffh.
			for (uint32 i = 0; i < size && !order; i += 2)
				order = int(readWord(uint16(a + i))) - int(readWord(uint16(b + i)));
		} else {
			for (uint32 i = 0; i < size && !order; ++i)
				order = int(readByte(uint16(a + i))) - int(readByte(uint16(b + i)));
		}
	} else {
		fail(Common::String::format("Unimplemented comparison type %u", type));
		return;
	}
	switch (opcode) {
	case 0xaf:
		push(order == 0);
		break;
	case 0xb7:
		push(order != 0);
		break;
	case 0xb4:
		push(order <= 0);
		break;
	case 0xb5:
		push(order < 0);
		break;
	case 0xb0:
		push(order >= 0);
		break;
	case 0xb1:
		push(order > 0);
		break;
	default:
		break;
	}
}

void PCodeMachine::standardProcedure(byte n) {
	uint16 a = 0, b = 0, c = 0, d = 0, e = 0;
	switch (n) {
	case 1:
		b = pop();
		a = pop();
		if (uint32(_heap) + 2U * b + 64 > _np) {
			fail("Heap/stack collision in NEW");
			break;
		}
		writeWord(a, _heap);
		_heap += 2 * b;
		break;
	case 2:
	case 3:
		e = pop();
		d = pop();
		c = pop();
		b = pop();
		a = pop();
		moveBytes(uint16(c + d), uint16(a + b), e, n == 3);
		break;
	case 4: {
		b = pop();
		a = pop();
		int index = int(_frames.size()) - 1;
		while (index >= 0 && !(_frames[index].segment == a && _frames[index].procedure == b))
			--index;
		if (index < 0) {
			fail("EXIT target not on dynamic call chain");
			break;
		}
		while (int(_frames.size()) - 1 > index) {
			_np = _frames.back().savedNP;
			_segment = _frames.back().returnSegment;
			_frames.pop_back();
		}
		const PCodeProcedure *p = activeProcedure();
		if (p)
			_ip = p->exit;
		else
			fail("Missing EXIT procedure");
	} break;
	case 5:
		unitIO(false);
		break;
	case 6:
		unitIO(true);
		break;
	case 10:
		d = pop();
		c = pop();
		b = pop();
		a = pop();
		if (int16(c) > 0)
			fillMemory(uint16(a + b), byte(d), c);
		break;
	case 32:
		writeWord(pop(), _heap);
		break;
	case 33:
		a = readWord(pop());
		// WIZARDRY:18 also uses RELEASE to move the heap frontier forward.
		// It is a frontier assignment, not necessarily a monotonic free.
		if (a < 0x6000 || uint32(a) + 64 > _np) {
			fail("Invalid RELEASE mark");
			break;
		}
		_heap = a;
		break;
	case 34:
		push(_ioResult);
		break;
	case 38:
		a = pop();
		if (a == 2 && !_scriptedInput)
			clearKeys();
		break;
	case 40:
		push((_np - _heap) / 2);
		break;
	default:
		fail(Common::String::format("Unimplemented standard procedure %u", n));
		break;
	}
}

void PCodeMachine::unitIO(bool write) {
	if (!requireStack(6))
		return;
	const uint16 w = pop(), z = pop(), y = pop(), x = pop(), ptr = pop(), unit = pop();
	const uint16 address = ptr + x;
	if (unit == 13) {
		_console->call(write, address, y, z, w);
	} else if (unit == 4 || unit == 5 || (unit >= 9 && unit <= 12)) {
		// I-III report 632 accessible blocks even in the larger SAVE images.
		if (z >= 632 || y > _disk.size() || uint32(z) * 512 > _disk.size() - y ||
			uint32(z) * 512 + y > 632U * 512) {
			_ioResult = 10;
			return;
		}
		for (uint32 i = 0; i < y; ++i) {
			if (write)
				_disk[uint32(z) * 512 + i] = readByte(uint16(address + i));
			else
				writeByte(uint16(address + i), _disk[uint32(z) * 512 + i]);
		}
		_ioResult = 0;
	} else if (unit == 1 || unit == 2) {
		_ioResult = 0;
		if (!write) {
			_input.active = true;
			_input.address = address;
			_input.count = y;
			_input.position = 0;
			completeInput();
		}
	} else {
		_ioResult = 2;
	}
}

void PCodeMachine::completeInput() {
	while (_input.position < _input.count) {
		if (!keyAvailable()) {
			yieldForInput();
			return;
		}
		const byte key = _keys[_keyPosition++];
		if (_observer)
			_observer->beforeInput(key);
		writeByte(uint16(_input.address + _input.position++), key);
		_idleSnapshot = false;
	}
	_input.active = false;
	_ioResult = 0;
}

} // namespace Wizardry
