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

PCodeConsole::PCodeConsole(MachineHost &host) : _host(host), _failed(false) {
	memset(&_state, 0, sizeof(_state));
	const uint16 initial[4] = {0x5bab, 0xd02b, 0x7e15, 0x7351};
	setRngState(initial);
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
}

void PCodeConsole::advanceRandom(uint16 buffer) {
	static const uint16 inc[4] = {0x3619, 0xff8b, 0x0183, 0x7fc9};
	static const uint16 mul[4] = {0x6a2d, 0xfff1, 0xffaf, 0xffd9};
	for (uint i = 0; i < 4; ++i)
		_state.rng[i] = uint16(uint32(_state.rng[i]) * mul[i] + inc[i]);
	uint16 swapped = uint16((_state.rng[3] << 8) | (_state.rng[3] >> 8));
	// The shipped interpreter stores AX, discarding the four-word mix in BX.
	_host.writeWord(buffer, swapped & 0x700f);
}

bool PCodeConsole::call(bool write, uint16 buffer, uint16 selector, uint16 z, uint16 w) {
	_failed = false;
	_host.setIoResult(0);
	if (!write) {
		switch (selector) {
		case 10:
			advanceRandom(buffer);
			break;
		case 11: {
			bool available = _host.keyAvailable();
			_host.writeWord(buffer, available ? 1 : 0);
			_state.idle = available ? 0 : (_state.idle < 0xffffffffU ? _state.idle + 1 : _state.idle);
			if (_state.idle > 300)
				_host.yieldForInput();
			break;
		}
		case 12:
			_host.writeWord(buffer, 1);
			break;
		case 21:
			_host.writeWord(buffer, 0x278);
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
		portraits(buffer, z);
		break;
	case 14: {
		byte charset[kCharsetSize];
		if (!_host.readDisk(z, sizeof(charset), charset))
			return fail("could not read the complete charset");
		memcpy(_state.charset, charset, sizeof(charset));
		break;
	}
	case 15:
		break; // The backend owns display lifetime; no DOS video mode to restore.
	case 17:
		clearAttributes(z);
		break;
	case 18:
		_state.monsterBlock = z;
		for (uint i = 0; i < 4; ++i)
			_state.monsterPicture[i] = 0xffff;
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

void PCodeConsole::init(uint16 buffer) {
	_state.initialized = true;
	memset(_state.priority, 0, sizeof(_state.priority));
	memset(_state.cell, 0, sizeof(_state.cell));
	memset(_state.vram, 0, sizeof(_state.vram));
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
	_state.cell[cell] = 0;
	putTile(cell, zeros);
}

void PCodeConsole::draw(uint cell, uint16 word) {
	uint high = word >> 8, low = word & 255;
	if (high == 0) {
		if (low == 0 || low == 0x20)
			blank(cell);
		else
			putTile(cell, _state.charset + low * 16);
	} else if (high == 0x20) {
		blank(cell);
	} else if (high >= 0x80) {
		byte solid[16];
		memset(solid, 255, sizeof(solid));
		putTile(cell, solid);
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
	uint16 a = (old == 0x2020 || old == 0x20) ? 0 : old;
	uint16 b = (word == 0x2020 || word == 0x20) ? 0 : word;
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
		if (!_host.readDisk(uint32(_state.monsterBlock) + pictures[group] - 1, 512, _state.monsterRecord[group])) {
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
	if (!_host.readDisk(block, sizeof(data), data)) {
		fail("could not read the complete title bitmap");
		return;
	}
	uint pos = 4;
	for (uint i = 0; i < 28; ++i) {
		for (uint bank = 0; bank < 2; ++bank) {
			memcpy(_state.vram + base + bank * 0x2000 + i * 80, data + pos, 70);
			pos += 80;
		}
	}
	for (uint i = 0; i < 4; ++i)
		_state.monsterPicture[i] = 0xffff;
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
