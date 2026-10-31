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

#include "wizardry/vm/console.h"

namespace Wizardry {

PCodeConsole::PCodeConsole(MachineHost &host, NativeRandom::Algorithm algorithm)
	: _host(host), _rngAlgorithm(algorithm), _portraitPolicy(host.nativeRevision() == 5 ? 1 : 0), _failed(false) {
	memset(&_state, 0, sizeof(_state));
	NativeRandom::reset(_rngAlgorithm, _state.rng);
	for (uint i = 0; i < 4; ++i)
		_state.monsterPicture[i] = 0xffff;
}

bool PCodeConsole::fail(const char *message) {
	_failed = true;
	_host.fail(Common::String("CONUNIT: ") + message);
	return false;
}

bool PCodeConsole::readWindow(uint16 address, Window &win) {
	win.address = address;
	win.next = _host.readWord(address);
	win.x = _host.readByte(address + 2);
	win.y = _host.readByte(address + 3);
	win.width = _host.readByte(address + 4);
	win.height = _host.readByte(address + 5);
	win.priority = _host.readByte(address + 8);
	if (win.width < 2 || win.height < 2 || uint(win.x) + win.width > kCols || uint(win.y) + win.height > kRows)
		return fail("window geometry is outside the 40 by 25 screen");
	return true;
}

uint16 PCodeConsole::windowCell(const Window &win, uint index) const {
	return _host.readWord(win.address + 10 + 2 * index);
}

uint16 PCodeConsole::visibleCell(uint index) const {
	if (index >= kCells)
		return 0;
	uint16 c = _state.cell[index];
	return ((_state.priority[index] & 0x3f) == 0 || c == 0x2020 || c == 0x0020) ? 0 : c;
}

void PCodeConsole::getRngState(uint16 state[4]) const {
	memcpy(state, _state.rng, sizeof(_state.rng));
}

void PCodeConsole::setRngState(const uint16 state[4]) {
	memcpy(_state.rng, state, sizeof(_state.rng));
	if (_rngAlgorithm == NativeRandom::kThreeWord)
		_state.rng[3] = 0;
}

void PCodeConsole::advanceRandom(uint16 buffer) {
	_host.writeWord(buffer, NativeRandom::next(_rngAlgorithm, _state.rng));
}

bool PCodeConsole::call(bool write, uint16 buffer, uint16 selector, uint16 z, uint16 w) {
	_failed = false;
	if (!write || selector != 1)
		_host.setIoResult(0);
	if (!write) {
		switch (selector) {
		case 10:
			advanceRandom(buffer);
			break;
		case 11: {
			bool available = _host.pollKeyboard();
			_host.writeWord(buffer, available ? 1 : 0);
			_state.idle = available ? 0 : (_state.idle < 0xffffffffU ? _state.idle + 1 : _state.idle);
			if (_state.idle > 300 && !_host.tracedInput())
				_host.yieldForInput();
			break;
		}
		case 12:
			_host.writeWord(buffer, 1);
			break;
		case 16:
			cacheList(buffer, z);
			break;
		case 21:
			_host.writeWord(buffer, _host.diskCapacity());
			break;
		default:
			_host.setIoResult(3);
			break;
		}
		return !_failed;
	}
	switch (selector) {
	case 1:
		init(buffer);
		break;
	case 2:
		layout(buffer);
		break;
	case 3:
		present(buffer);
		break;
	case 4:
		putChar(buffer, z);
		break;
	case 5:
		drawLine(buffer, z);
		break;
	case 13:
		if (_portraitPolicy)
			largePortraits(buffer, z);
		else
			portraits(buffer, z);
		break;
	case 14: {
		byte charset[kCharsetSize];
		const uint16 physical = _host.nativeRevision() == 5 ? 0xa1a2 : _host.nativeRevision() == 4 ? 0x872a
																								   : 0x85da;
		if (!_host.readDiskAt(z, sizeof(charset), charset, physical, _host.readWord(0xe0e2)))
			return fail("could not read the complete charset");
		memcpy(_state.charset, charset, sizeof(charset));
		break;
	}
	case 15:
		_host.writeWord(0xe8fe, 0);
		_state.initialized = false;
		break; // The backend owns display lifetime; no DOS text mode to restore.
	case 17:
		if (_portraitPolicy)
			backupPortrait(buffer, z);
		else
			clearAttributes(z);
		break;
	case 18:
		_state.monsterBlock = z;
		if (_portraitPolicy && _state.portraitOrientation)
			swapLargePortraits();
		for (uint i = 0; i < 4; ++i)
			_state.monsterPicture[i] = 0xffff;
		if (_portraitPolicy)
			syncPortraitCache();
		break;
	case 19:
		// INTERP:2305 doubles DX before waiting for BIOS clock ticks. Pass the
		// original parameter; the host maps it to a cooperative timed delay.
		_host.delay(z);
		break;
	case 22:
		break; // DOS disk-flush service; in-memory writes are already immediate.
	case 23:
		title(z);
		break;
	case 24:
		_host.sound(buffer, z, w);
		break;
	case 26:
		appendString(buffer, z);
		if (w)
			appendString(buffer, w);
		break;
	case 29:
		stringPosition(buffer, z, w);
		break;
	case 30:
		putString(buffer, z);
		break;
	case 32:
		loadHuffman(buffer);
		break;
	case 33:
		decodeHuffman(buffer, z);
		break;
	default:
		_host.fail(Common::String::format("CONUNIT: unsupported write selector %u", selector));
		_failed = true;
		break;
	}
	return !_failed;
}

void PCodeConsole::cacheList(uint16 buffer, uint16 key) {
	// URD16: I-III 23e4, IV 2508, V 25ec. Each node is key/next/previous;
	// the caller supplies head/tail words and moves the returned head's data.
	// Keep the original write order and rereads, including wrapped/aliased
	// guest addresses and the original empty/single-node edge behavior.
	uint16 node = _host.readWord(buffer);
	for (uint32 searched = 0; node && _host.readWord(node) != key; ++searched) {
		if (searched == 65535) {
			fail("Cyclic cache list in CONUNIT read selector 16");
			return;
		}
		node = _host.readWord(uint16(node + 2));
	}
	if (node) {
		if (node == _host.readWord(buffer))
			return;
		const uint16 next = _host.readWord(uint16(node + 2));
		const uint16 previous = _host.readWord(uint16(node + 4));
		_host.writeWord(uint16(previous + 2), next);
		if (node == _host.readWord(uint16(buffer + 2)))
			_host.writeWord(uint16(buffer + 2), previous);
		else
			_host.writeWord(uint16(_host.readWord(uint16(node + 2)) + 4), previous);
		_host.writeWord(uint16(node + 4), 0);
		const uint16 head = _host.readWord(buffer);
		_host.writeWord(uint16(node + 2), head);
		_host.writeWord(uint16(head + 4), node);
		_host.writeWord(buffer, node);
	} else {
		node = _host.readWord(uint16(buffer + 2));
		const uint16 previous = _host.readWord(uint16(node + 4));
		_host.writeWord(uint16(buffer + 2), previous);
		_host.writeWord(uint16(previous + 2), 0);
		_host.writeWord(uint16(node + 4), 0);
		const uint16 head = _host.readWord(buffer);
		_host.writeWord(uint16(node + 2), head);
		_host.writeWord(node, 0x8000);
		_host.writeWord(uint16(head + 4), node);
		_host.writeWord(buffer, node);
	}
}

void PCodeConsole::init(uint16 buffer) {
	const bool first = (_host.readWord(0xe8fe) & 255) == 0;
	if (!_host.graphicsInit(first)) {
		_failed = true;
		return;
	}
	_host.setIoResult(0);
	if (first) {
		_host.writeWord(0xe8fe, 0x00ff);
		_host.writeWord(0xe816, 0xe900);
		const byte revision = _host.nativeRevision();
		_host.writeWord(0xe81a, revision == 5 ? 0xa1a2 : revision == 4 ? 0x872a
																	   : 0x85da);
		memset(_state.priority, 0, sizeof(_state.priority));
		const uint16 blank = _host.nativeRevision() >= 4 ? 0x20 : 0;
		for (uint i = 0; i < kCells; ++i)
			_state.cell[i] = blank;
		memset(_state.vram, 0, sizeof(_state.vram));
	}
	// The IV/V routine sets ES=0040 but actually reads DS:006C.
	if (_rngAlgorithm == NativeRandom::kThreeWord)
		_state.rng[1] = uint16(_host.readWord(0x006c) + 2);
	_state.initialized = true;
	_host.writeWord(0xe822, _host.readWord(buffer));
	_host.writeWord(buffer, 0x31);
}

void PCodeConsole::putTile(uint cell, const byte *rows) {
	if (cell >= kCells) {
		fail("cell index is outside the screen");
		return;
	}
	uint base = (cell / kCols) * 320 + (cell % kCols) * 2;
	for (uint row = 0; row < 8; ++row) {
		uint off = base + (row & 1) * 0x2000 + (row >> 1) * 80;
		_state.vram[off] = rows[2 * row];
		_state.vram[off + 1] = rows[2 * row + 1];
	}
}

void PCodeConsole::blank(uint cell) {
	static const byte zeros[16] = {};
	_state.cell[cell] = _portraitPolicy ? 0x20 : 0;
	putTile(cell, zeros);
}

void PCodeConsole::draw(uint cell, uint16 word) {
	uint high = word >> 8, low = word & 255;
	if (high == 0) {
		if ((!_portraitPolicy && low == 0) || low == 0x20)
			blank(cell);
		else
			putTile(cell, _state.charset + low * 16);
	} else if (high == 0x20) {
		blank(cell);
	} else if (high >= 0x80) {
		byte solid[16];
		memset(solid, 255, sizeof(solid));
		putTile(cell, solid);
	} else if (_portraitPolicy) {
		if (high != 2 && high != 3) {
			fail("unsupported Wizardry V graphics pointer table entry");
			return;
		}
		const uint16 pointer = _host.readWord(0xe830 + 2 * (high - 2));
		if (pointer != 0x71a2 && pointer != 0x89a2) {
			fail("unsupported Wizardry V desired portrait pointer");
			return;
		}
		const uint group = (pointer == 0x89a2 ? 1 : 0) ^ _state.portraitOrientation;
		// Original V's 256-entry offset table maps 168..255 back to tile0.
		putTile(cell, _state.largePortrait[group] + (low < 168 ? low * 16 : 0));
	} else {
		int tile = int(high & 31) - 1;
		uint group = ((high >> 4) & 6) >> 1;
		if (tile < 0 || tile >= 32) {
			fail("invalid monster tile attribute");
			return;
		}
		putTile(cell, _state.monsterRecord[group] + tile * 16);
	}
}

void PCodeConsole::storeCell(uint cell, uint16 word) {
	uint16 old = _state.cell[cell];
	_state.cell[cell] = word;
	const uint16 blankValue = _portraitPolicy ? 0x20 : 0;
	uint16 a = (old == 0x2020 || old == 0x20) ? blankValue : old;
	uint16 b = (word == 0x2020 || word == 0x20) ? blankValue : word;
	if (a != b)
		draw(cell, word);
}

void PCodeConsole::borderCell(const Window &win, uint cell, uint16 glyph, bool layoutMode) {
	if (layoutMode) {
		byte previousPriority = _state.priority[cell] & 0x3f;
		if (win.priority < previousPriority)
			return;
		if (win.priority > previousPriority)
			_state.priority[cell] = win.priority;
		uint16 old = _state.cell[cell];
		if (old == glyph)
			return;
		_state.cell[cell] = glyph;
		uint high = old >> 8;
		_state.priority[cell] |= (high && high < 0x80) ? 0xc0 : 0x80;
	} else {
		if (win.priority < _state.priority[cell])
			return;
		_state.priority[cell] = win.priority;
		storeCell(cell, glyph);
	}
}

void PCodeConsole::drawBorder(const Window &win, bool layoutMode) {
	static const uint16 standard[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	static const uint16 emphasis[8] = {0x0d, 0x0a, 0x0e, 0x0b, 0x0c, 0x0e, 9, 0x0d};
	const uint16 *g = (layoutMode && win.priority == 0x3f) ? emphasis : standard;
	uint start = win.y * kCols + win.x;
	borderCell(win, start, g[0], layoutMode);
	for (uint i = 0; i < uint(win.width - 2); ++i)
		borderCell(win, start + 1 + i, g[1], layoutMode);
	borderCell(win, start + win.width - 1, g[2], layoutMode);
	for (uint i = 0; i < uint(win.height - 2); ++i) {
		borderCell(win, start + kCols * (1 + i), g[3], layoutMode);
		borderCell(win, start + kCols * (1 + i) + win.width - 1, g[4], layoutMode);
	}
	uint bottom = start + kCols * (win.height - 1);
	borderCell(win, bottom, g[5], layoutMode);
	for (uint i = 0; i < uint(win.width - 2); ++i)
		borderCell(win, bottom + 1 + i, g[6], layoutMode);
	borderCell(win, bottom + win.width - 1, g[7], layoutMode);
}

void PCodeConsole::interior(const Window &win) {
	uint first = win.y * kCols + win.x + kCols + 1;
	uint index = 0;
	for (uint row = 0; row < uint(win.height - 2); ++row) {
		for (uint col = 0; col < uint(win.width - 2); ++col) {
			uint cell = first + row * kCols + col;
			uint16 word = windowCell(win, index++);
			if (win.priority < (_state.priority[cell] & 0x3f))
				continue;
			_state.priority[cell] = win.priority;
			uint16 old = _state.cell[cell];
			if (word == old)
				continue;
			_state.cell[cell] = word;
			_state.priority[cell] |= 0x80;
			uint high = old >> 8;
			if (high && high < 0x80 && high != 0x20)
				_state.priority[cell] |= 0x40;
		}
	}
}

void PCodeConsole::flush() {
	for (uint cell = 0; cell < kCells && !_failed; ++cell) {
		byte p = _state.priority[cell];
		if (p == 0)
			blank(cell);
		else if (p & 0x80) {
			_state.priority[cell] = p & 0x3f;
			draw(cell, _state.cell[cell]);
		}
	}
}

void PCodeConsole::layout(uint16 buffer) {
	_state.head = buffer;
	memset(_state.priority, 0, sizeof(_state.priority));
	// A bitset detects cycles without imposing an arbitrary window-count limit.
	byte visited[8192] = {};
	uint16 address = buffer;
	while (address && !_failed) {
		uint index = address >> 3;
		byte mask = byte(1 << (address & 7));
		if (visited[index] & mask) {
			fail("cycle in the linked window list");
			return;
		}
		visited[index] |= mask;
		Window win;
		if (!readWindow(address, win))
			return;
		if (!(win.priority & 0x80)) {
			drawBorder(win, true);
			interior(win);
		}
		address = win.next;
	}
	if (!_failed)
		flush();
}

void PCodeConsole::present(uint16 buffer) {
	Window win;
	if (!readWindow(buffer, win) || (win.priority & 0x80))
		return;
	drawBorder(win, false);
	uint first = win.y * kCols + win.x + kCols + 1;
	uint index = 0;
	for (uint row = 0; row < uint(win.height - 2) && !_failed; ++row) {
		for (uint col = 0; col < uint(win.width - 2) && !_failed; ++col) {
			uint cell = first + row * kCols + col;
			uint16 word = windowCell(win, index++);
			if (win.priority < _state.priority[cell])
				continue;
			_state.priority[cell] = win.priority;
			storeCell(cell, word);
		}
	}
}

void PCodeConsole::putChar(uint16 address, uint16 character) {
	Window win;
	if (!readWindow(address, win))
		return;
	uint width = win.width - 2, height = win.height - 2;
	uint cx = _host.readByte(address + 6), cy = _host.readByte(address + 7);
	if (cx >= width || cy >= height) {
		fail("text cursor is outside its window");
		return;
	}
	_host.writeWord(address + 10 + 2 * (cy * width + cx), character);
	if (!(win.priority & 0x80)) {
		uint cell = (cy + win.y + 1) * kCols + (cx + win.x + 1);
		if (win.priority >= _state.priority[cell]) {
			_state.priority[cell] = win.priority;
			storeCell(cell, character);
		}
	}
	if (++cx == width) {
		cx = 0;
		if (++cy == height)
			cy = 0;
	}
	_host.writeByte(address + 6, cx);
	_host.writeByte(address + 7, cy);
}

void PCodeConsole::putString(uint16 address, uint16 source) {
	uint count = _host.readByte(source);
	for (uint i = 0; i < count && !_failed; ++i)
		putChar(address, _host.readByte(source + 1 + i));
}

void PCodeConsole::drawLine(uint16 buffer, uint16 address) {
	Window win;
	if (!readWindow(address, win))
		return;
	uint width = win.width - 2, height = win.height - 2;
	byte low = _host.readByte(buffer), high = _host.readByte(buffer + 2);
	byte col = _host.readByte(buffer + 4), row = _host.readByte(buffer + 6);
	byte dx = _host.readByte(buffer + 8), dy = _host.readByte(buffer + 10);
	uint16 count = _host.readWord(buffer + 12), glyph = _host.readWord(buffer + 14);
	uint16 cursor = address + 10 + 2 * (row * width) + 2 * _host.readWord(buffer + 4);
	int step = dy ? ((dy & 0x80) ? -int(width) : int(width)) : 0;
	step = (step + dx) & 255;
	if (step & 0x80)
		step -= 256;
	step *= 2;
	// A 16-bit zero count wraps, as it does in the original loop.
	for (uint32 iterations = 0; iterations < 65536; ++iterations) {
		if (low <= col && col <= high)
			_host.writeWord(cursor, glyph);
		cursor = uint16(cursor + step);
		col = byte(col + dx);
		if (col & 0x80)
			break;
		row = byte(row + dy);
		if (row & 0x80 || col == width || row == height)
			break;
		if (--count == 0)
			break;
	}
	_host.writeWord(buffer + 12, count);
	_host.writeWord(buffer + 10, uint16(step));
}

void PCodeConsole::clearAttributes(uint16 address) {
	for (uint i = 0; i < 0x2d0; ++i)
		_host.writeByte(address + 11 + 2 * i, 0);
}

void PCodeConsole::swapLargePortraits() {
	for (uint i = 0; i < kLargePortraitSize; ++i) {
		const byte value = _state.largePortrait[0][i];
		_state.largePortrait[0][i] = _state.largePortrait[1][i];
		_state.largePortrait[1][i] = value;
	}
	const uint16 picture = _state.monsterPicture[0];
	_state.monsterPicture[0] = _state.monsterPicture[1];
	_state.monsterPicture[1] = picture;
	_state.portraitOrientation ^= 1;
}

void PCodeConsole::syncPortraitCache() {
	for (uint group = 0; group < 2; ++group) {
		_host.writeWord(0xe838 + group * 4, _state.monsterPicture[group]);
		_host.writeWord(0xe83a + group * 4, (group ^ _state.portraitOrientation) ? 0x89a2 : 0x71a2);
	}
}

void PCodeConsole::largePortraits(uint16 buffer, uint16 address) {
	const byte first = _host.readByte(buffer);
	if (first == 255) {
		_host.writeWord(0xe834, _host.readWord(uint16(buffer + 2)));
		return;
	}
	const byte pictures[2] = {first, _host.readByte(uint16(buffer + 1))};
	const uint count = pictures[1] == 255 ? 1 : 2;
	for (uint group = 0; group < count; ++group) {
		const uint column = count == 1 ? 12 : group ? 20
													: 4;
		for (uint row = 0; row < 14; ++row)
			for (uint col = 0; col < 12; ++col)
				_host.writeWord(uint16(address + 10 + 2 * ((6 + row) * 36 + column + col)),
								uint16(((group + 2) << 8) | (row * 12 + col)));
	}
	for (uint group = 0; group < count; ++group) {
		uint cached = group;
		while (cached < 2 && _state.monsterPicture[cached] != pictures[group])
			++cached;
		if (cached < 2) {
			if (cached != group)
				swapLargePortraits();
		} else {
			const uint16 block = uint16(_state.monsterBlock + 6 * (int(pictures[group]) - 1));
			if (!_host.readDisk(block, kLargePortraitSize, _state.largePortrait[group])) {
				fail("could not read a complete Wizardry V portrait");
				return;
			}
			_state.monsterPicture[group] = pictures[group];
		}
		syncPortraitCache();
		_host.writeWord(0xe830 + group * 2, (group ^ _state.portraitOrientation) ? 0x89a2 : 0x71a2);
	}
	if (count == 1)
		_host.writeWord(0xe832, 0xffff);
	for (uint cell = 0; cell < kCells; ++cell) {
		const uint high = _state.cell[cell] >> 8;
		if (high && high < 0x80 && high != 0x20)
			_state.cell[cell] = (_state.cell[cell] & 0xff00) | 0x5f;
	}
}

void PCodeConsole::backupPortrait(uint16 buffer, uint16 address) {
	const bool restore = _host.readByte(buffer) != 0;
	const byte placement = _host.readByte(uint16(buffer + 1));
	const uint column = placement == 1 ? 12 : placement == 2 ? 20
															 : 4;
	const uint16 backup = uint16(_host.readWord(0xe834) + 0x557 + (placement == 2 ? 0x600 : 0));
	for (uint row = 0; row < 14; ++row) {
		for (uint col = 0; col < 12; ++col) {
			const uint16 cell = uint16(address + 10 + 2 * ((6 + row) * 36 + column + col));
			const uint16 saved = uint16(backup + row * 12 + col);
			if (restore)
				_host.writeWord(cell, _host.readByte(saved));
			else
				_host.writeByte(saved, _host.readByte(cell));
		}
	}
}

void PCodeConsole::portraits(uint16 buffer, uint16 address) {
	clearAttributes(address);
	byte pictures[4];
	uint count = 0;
	for (uint i = 0; i < 4; ++i) {
		byte picture = _host.readByte(buffer + i);
		if (picture != 0xff) {
			if (picture == 0) {
				fail("monster picture indices are one-based");
				return;
			}
			pictures[count++] = picture;
		}
	}
	static const byte columns[4][4] = {{15, 0, 0, 0}, {11, 19, 0, 0}, {7, 15, 23, 0}, {3, 11, 19, 27}};
	for (uint group = 0; group < count; ++group) {
		uint16 base = address + 10 + 2 * (324 + columns[count - 1][group]);
		byte attribute = byte(group << 5);
		for (uint row = 0; row < 5; ++row)
			for (uint col = 0; col < 6; ++col)
				_host.writeByte(base + 2 * (row * 36 + col) + 1, ++attribute);
	}
	for (uint i = 0; i < 4; ++i)
		_state.monsterPicture[i] = 0xffff;
	for (uint group = 0; group < count; ++group) {
		if (!_host.readDiskAt(uint32(_state.monsterBlock) + pictures[group] - 1, 512,
							  _state.monsterRecord[group], 0x7252 + group * 512, _host.readWord(0xe0e2))) {
			fail("could not read a complete monster portrait");
			return;
		}
		_state.monsterPicture[group] = pictures[group];
	}
	for (uint cell = 0; cell < kCells; ++cell) {
		uint high = _state.cell[cell] >> 8;
		if (high && high < 0x80 && high != 0x20)
			_state.cell[cell] = (_state.cell[cell] & 255) | 0x1f00;
	}
}

void PCodeConsole::title(uint16 block) {
	const uint base = 2 * 320 + 2 * 2;
	if (!block) {
		for (uint i = 0; i < 28; ++i)
			for (uint bank = 0; bank < 2; ++bank)
				memset(_state.vram + base + bank * 0x2000 + i * 80, 0, 70);
		return;
	}
	byte data[56 * 80 + 512];
	if (!_host.readDiskAt(block, sizeof(data), data, 0x7252, _host.readWord(0xe0e2))) {
		fail("could not read the complete title bitmap");
		return;
	}
	if (_portraitPolicy)
		memcpy(_state.largePortrait[_state.portraitOrientation], data, kLargePortraitSize);
	uint pos = 4;
	for (uint i = 0; i < 28; ++i) {
		for (uint bank = 0; bank < 2; ++bank) {
			memcpy(_state.vram + base + bank * 0x2000 + i * 80, data + pos, 70);
			pos += 80;
		}
	}
	for (uint i = 0; i < 4; ++i)
		_state.monsterPicture[i] = 0xffff;
	if (_portraitPolicy) {
		_host.writeWord(0xe838, 0xffff);
		_host.writeWord(0xe83c, 0xffff);
	}
}

void PCodeConsole::appendString(uint16 destination, uint16 source) {
	if (!source)
		return;
	uint n = _host.readByte(destination), m = _host.readByte(source);
	byte text[255];
	for (uint i = 0; i < m; ++i)
		text[i] = _host.readByte(source + 1 + i);
	for (uint i = 0; i < m; ++i)
		_host.writeByte(destination + 1 + n + i, text[i]);
	_host.writeByte(destination, byte(n + m));
}

void PCodeConsole::stringPosition(uint16 buffer, uint16 pattern, uint16 text) {
	uint patternSize = _host.readByte(pattern), textSize = _host.readByte(text);
	uint position = 0;
	if (patternSize && patternSize <= textSize) {
		for (uint i = 0; i + patternSize <= textSize; ++i) {
			uint matched = 0;
			while (matched < patternSize && _host.readByte(pattern + 1 + matched) == _host.readByte(text + 1 + i + matched))
				++matched;
			if (matched == patternSize) {
				position = i + 1;
				break;
			}
		}
	}
	_host.writeWord(buffer, position);
}

void PCodeConsole::loadHuffman(uint16 table) {
	byte bytes[HuffmanDecoder::kTableSize];
	for (uint i = 0; i < sizeof(bytes); ++i)
		bytes[i] = _host.readByte(uint16(table + i));
	Common::String message;
	if (!_huffman.load(bytes, sizeof(bytes), message))
		fail(message.c_str());
}

void PCodeConsole::decodeHuffman(uint16 source, uint16 destination) {
	Common::Array<byte> encoded(_huffman.maxEncodedSize()), decoded;
	for (uint i = 0; i < encoded.size(); ++i)
		encoded[i] = _host.readByte(uint16(source + i));
	Common::String message;
	uint32 bits = 0;
	if (!_huffman.decodePascal(encoded.data(), encoded.size(), decoded, bits, message)) {
		fail(message.c_str());
		return;
	}
	for (uint i = 0; i < decoded.size(); ++i)
		_host.writeByte(uint16(destination + i), decoded[i]);
}

} // namespace Wizardry
