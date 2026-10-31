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

#include "common/endian.h"
#include "wizardry/vm/machine.h"

namespace Wizardry {

static const uint16 kHeapStart = 0x0732;
static const uint16 kArenaStart = 0xe01c;
static const uint16 kEvaluationStart = 0xf7fe;
static const uint16 kDictionaryTable = 0x0632;
static const uint16 kSavedArenaTable = 0x0672;
static const uint16 kReferenceTable = 0x06b2;
static const uint16 kCodeBaseTable = 0x06f2;

uint16 PCodeMachine::segmentBase(byte segment) const {
	return _originalMemory && segment < 32 ? readWord(kCodeBaseTable + 2 * segment) : 0;
}

uint16 PCodeMachine::segmentReferences(byte segment) const {
	return _originalMemory && segment < 32 ? readWord(kReferenceTable + 2 * segment) : 0;
}

uint16 PCodeMachine::originalStackPointer() const {
	return uint16(kEvaluationStart - 2 * _stack.size());
}

uint16 PCodeMachine::codeWord(byte segment, uint16 offset) const {
	return _originalMemory ? readWord(uint16(segmentBase(segment) + offset)) : READ_LE_UINT16(_code.segment(segment)->code.data() + offset);
}

void PCodeMachine::setArenaTop(uint16 value) {
	_np = value;
	if (_originalMemory)
		writeWord(0x0602, value);
}

void PCodeMachine::setHeapTop(uint16 value) {
	_heap = value;
	if (_originalMemory)
		writeWord(0x0604, value);
}

bool PCodeMachine::initializeOriginalMemory(const Common::Array<byte> &code, Common::String &error) {
	// Bootstrap reads the volume directory and then overwrites its first block
	// with the codefile directory. The surviving bytes can become locals later.
	memcpy(_memory + kHeapStart, _disk.data() + 1024, 2048);
	memcpy(_memory + kHeapStart, code.data(), 512);
	setHeapTop(kHeapStart);
	setArenaTop(kArenaStart);
	_literalNext = 0x0800;
	uint16 programBlock = 0;
	const uint count = READ_LE_UINT16(_disk.data() + 1040);
	for (uint i = 0; i < count; ++i) {
		const byte *entry = _disk.data() + 1024 + 26 * (i + 1);
		if (entry[6] == 13 && !memcmp(entry + 7, "SYSTEM.PASCAL", 13))
			programBlock = READ_LE_UINT16(entry);
	}
	if (!programBlock || uint32(programBlock) * 512 + 512 > _disk.size() ||
		memcmp(code.data(), _disk.data() + uint32(programBlock) * 512, 512)) {
		error = "Original memory boot requires the matching SYSTEM.PASCAL on the volume";
		return false;
	}
	for (uint i = 0; i < _code.segments().size(); ++i) {
		const PCodeSegment &s = _code.segments()[i];
		if (s.number >= 32 || s.code.size() >= kArenaStart - kHeapStart ||
			uint32(programBlock) + s.fileBlock > 0xffff) {
			error = "Code segment cannot be represented in the DOS memory arena";
			return false;
		}
		bool linked = false;
		for (uint slot = 0; slot < 16; ++slot) {
			if (READ_LE_UINT16(code.data() + 4 * slot) == s.fileBlock &&
				READ_LE_UINT16(code.data() + 4 * slot + 2) == s.code.size() &&
				code[0x100 + 2 * slot] == s.number)
				linked = true;
		}
		if (!linked) {
			error = "Codefile header and resident segment numbers disagree";
			return false;
		}
		const uint16 descriptor = 0xe13e + 6 * s.number;
		writeWord(descriptor, 4);
		writeWord(descriptor + 2, programBlock + s.fileBlock);
		writeWord(descriptor + 4, s.code.size());
	}
	writeWord(0xe0e2, 4);
	writeWord(0xe0e6, 0);
	setIoResult(0);
	return true;
}

void PCodeMachine::flushOriginalHeap() {
	const uint16 pending = readWord(0xe0e6);
	if (pending) {
		if (pending < kHeapStart || pending > _np) {
			fail("Invalid pending heap frontier");
			return;
		}
		setHeapTop(pending);
		writeWord(0xe0e6, 0);
	}
}

bool PCodeMachine::checkOriginalSpace(uint32 frontier) {
	if (frontier > kArenaStart) {
		fail("Arena address wrapped outside DOS memory");
		return false;
	}
	if (frontier <= _heap)
		flushOriginalHeap();
	if (_state == kFault)
		return false;
	if (frontier <= _heap) {
		fail("Heap/arena collision");
		return false;
	}
	return true;
}

bool PCodeMachine::loadSegment(byte sn) {
	const PCodeSegment *s = _code.segment(sn);
	if (!s || sn >= 32) {
		fail("Load of missing code segment");
		return false;
	}
	const uint16 refs = segmentReferences(sn);
	writeWord(0x060e, sn);
	if (refs) {
		if (refs == 0xffff) {
			fail("Segment reference count overflow");
			return false;
		}
		writeWord(kReferenceTable + 2 * sn, refs + 1);
		return true;
	}
	if (s->code.size() > _np || !checkOriginalSpace(uint32(_np) - s->code.size())) {
		if (_state != kFault)
			fail("Code segment exceeds available arena");
		return false;
	}
	const uint16 oldTop = _np;
	const uint16 base = oldTop - s->code.size();
	const uint16 descriptor = 0xe13e + 6 * sn;
	const uint32 offset = uint32(readWord(descriptor + 2)) * 512;
	if (readWord(descriptor + 4) != s->code.size() || offset > _disk.size() || s->code.size() > _disk.size() - offset) {
		fail("Invalid code segment disk descriptor");
		return false;
	}
	memcpy(_memory + base, _disk.data() + offset, s->code.size());
	writeWord(kSavedArenaTable + 2 * sn, oldTop);
	writeWord(kDictionaryTable + 2 * sn, oldTop - 2);
	writeWord(kCodeBaseTable + 2 * sn, base);
	writeWord(kReferenceTable + 2 * sn, 1);
	setArenaTop(base);
	setIoResult(0);
	return true;
}

bool PCodeMachine::releaseSegment(byte sn) {
	if (!sn)
		return true;
	const uint16 refs = segmentReferences(sn);
	if (!refs) {
		fail("Segment reference count underflow");
		return false;
	}
	if (refs == 1) {
		for (uint i = 0; i < _frames.size(); ++i) {
			if (_frames[i].segment == sn) {
				fail("Release of an active code segment");
				return false;
			}
		}
		const uint16 top = readWord(kSavedArenaTable + 2 * sn);
		if (top < _np || top > kArenaStart) {
			fail("Invalid released arena frontier");
			return false;
		}
		for (uint i = 0; i < _frames.size(); ++i) {
			if (_frames[i].base - 14 < top) {
				fail("Code release crosses an active frame");
				return false;
			}
		}
		for (uint i = 1; i < 32; ++i) {
			if (i != sn && segmentReferences(i) && segmentBase(i) < top && segmentBase(i) >= _np) {
				fail("Code segments released out of allocation order");
				return false;
			}
		}
		setArenaTop(top);
	}
	writeWord(kReferenceTable + 2 * sn, refs - 1);
	return true;
}

void PCodeMachine::syncOriginalRegisters() {
	if (_frames.empty())
		return;
	const Frame &f = _frames.back();
	PCodeProcedure p;
	if (!originalProcedure(f.segment, f.procedure, p)) {
		fail("Invalid resident procedure attributes");
		return;
	}
	const uint16 bp = f.base - 12;
	const uint16 dictionary = readWord(kDictionaryTable + 2 * f.segment);
	const uint16 anchor = segmentBase(f.segment) + p.anchor;
	writeWord(0x0606, dictionary);
	writeWord(0x0608, anchor);
	writeWord(0xe0ec, bp);
	writeWord(0xe0ee, anchor);
	writeWord(0xe0f0, dictionary);
}

bool PCodeMachine::originalProcedure(byte sn, byte pn, PCodeProcedure &result) const {
	const PCodeSegment *s = _code.segment(sn);
	if (!s || sn >= 32 || !pn || !segmentReferences(sn))
		return false;
	const uint32 base = segmentBase(sn), end = base + s->code.size();
	const uint32 dictionary = readWord(kDictionaryTable + 2 * sn);
	if (end > kArenaStart || dictionary != end - 2 || readByte(dictionary) != sn ||
		pn > readByte(dictionary + 1) || 2U * pn > dictionary - base)
		return false;
	const uint32 slot = dictionary - 2 * pn;
	const uint32 displacement = readWord(slot);
	if (displacement > slot)
		return false;
	const uint32 anchor = slot - displacement;
	if (anchor < base + 8 || anchor + 2 > end - 2 - 2U * readByte(dictionary + 1) || readByte(anchor) != pn)
		return false;
	const int32 entry = int32(anchor) - 2 - readWord(anchor - 2);
	const int32 exit = int32(anchor) - 4 - readWord(anchor - 4);
	result.anchor = anchor - base;
	result.localBytes = readWord(anchor - 8);
	result.parameterBytes = readWord(anchor - 6);
	result.level = readByte(anchor + 1);
	if (entry < int32(base) || entry >= int32(anchor) - 8 || exit < int32(base) || exit >= int32(anchor) - 8 ||
		((result.localBytes | result.parameterBytes) & 1) || uint32(result.localBytes) + result.parameterBytes > 0x9000)
		return false;
	result.entry = entry - base;
	result.exit = exit - base;
	return true;
}

void PCodeMachine::enterOriginal(byte sn, byte pn, StaticMode mode, bool external) {
	if (!_code.procedure(sn, pn) || sn >= 32) {
		fail("Call to missing procedure");
		return;
	}
	if (external && !loadSegment(sn))
		return;
	PCodeProcedure attributes;
	if (!originalProcedure(sn, pn, attributes)) {
		fail("Invalid resident procedure attributes");
		return;
	}
	const PCodeProcedure *p = &attributes;
	const uint bytes = uint(p->parameterBytes) + p->localBytes;
	if (bytes + 14 > _np || _frames.size() >= 512 ||
		!checkOriginalSpace(uint32(_np) - bytes - 12)) {
		if (_state != kFault)
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
	f.base = _np - bytes;
	const uint16 bp = f.base - 12;
	if (mode == kLocal)
		f.staticFrame = int32(_frames.size()) - 1;
	else if (mode == kGlobal)
		f.staticFrame = _frames.empty() ? -1 : 0;
	else if (mode == kSearch) {
		const int wanted = int(p->level) - 1;
		for (int i = int(_frames.size()) - 1; i >= 0; --i) {
			PCodeProcedure caller;
			if (originalProcedure(_frames[i].segment, _frames[i].procedure, caller) && caller.level == wanted) {
				f.staticFrame = i;
				break;
			}
		}
		if (f.staticFrame < 0) {
			fail("Unresolved original static link");
			return;
		}
	}
	writeWord(bp - 2, f.savedNP);
	writeWord(bp, mode == kRoot ? bp : (_frames[f.staticFrame].base - 12));
	writeWord(bp + 2, mode == kRoot ? bp : (_frames.back().base - 12));
	writeWord(bp + 4, mode == kRoot ? 0 : readWord(0x0608));
	writeWord(bp + 6, mode == kRoot ? 0 : readWord(0x0606));
	writeWord(bp + 8, mode == kRoot ? kHeapStart : uint16(segmentBase(_segment) + _ip));
	for (uint i = 0; i < p->parameterBytes / 2; ++i)
		writeWord(f.base + 2 * i, pop());
	f.stackBase = _stack.size();
	writeWord(bp + 10, originalStackPointer());
	writeWord(0x060c, pn);
	writeWord(0x060e, external || mode == kRoot ? sn : 0xffff);
	writeWord(0x0610, readWord(kDictionaryTable + 2 * sn));
	setArenaTop(bp - 2);
	_frames.push_back(f);
	_segment = sn;
	_ip = p->entry;
	syncOriginalRegisters();
}

void PCodeMachine::leaveOriginal(byte nret) {
	if (_frames.empty()) {
		fail("Return with no activation frame");
		return;
	}
	const Frame f = _frames.back();
	PCodeProcedure attributes;
	if (!originalProcedure(f.segment, f.procedure, attributes)) {
		fail("Invalid resident return procedure");
		return;
	}
	const PCodeProcedure *p = &attributes;
	const uint16 bp = f.base - 12;
	if (2U * nret > uint(p->parameterBytes) + p->localBytes || f.stackBase > _stack.size() ||
		readWord(bp + 10) != kEvaluationStart - 2 * f.stackBase || readWord(bp - 2) != f.savedNP) {
		fail("Invalid original procedure return");
		return;
	}
	_stack.resize(f.stackBase);
	for (int i = nret - 1; i >= 0; --i)
		push(readWord(f.base + 2 * i));
	setArenaTop(f.savedNP);
	_frames.pop_back();
	if (_frames.empty()) {
		_state = kHalted;
		return;
	}
	if (readWord(bp + 2) != _frames.back().base - 12) {
		fail("Invalid original dynamic link");
		return;
	}
	if (f.segment != f.returnSegment && !releaseSegment(f.segment))
		return;
	const uint16 returnAddress = readWord(bp + 8);
	const PCodeSegment *caller = _code.segment(f.returnSegment);
	const uint16 base = segmentBase(f.returnSegment);
	if (!caller || returnAddress < base || uint32(returnAddress) - base >= caller->code.size()) {
		fail("Invalid original return address");
		return;
	}
	_segment = f.returnSegment;
	_ip = returnAddress - base;
	syncOriginalRegisters();
}

} // namespace Wizardry
