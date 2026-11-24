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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "common/random.h"
#include "common/stream.h"

#include "gob/surface.h"
#include "gob/video.h"

#include "gob/pregob/gctfile.h"

namespace Gob {

GCTFile::Chunk::Chunk() : type(kChunkTypeNone) {
}


GCTFile::GCTFile(Common::SeekableReadStream &gct, Common::RandomSource &rnd) : _rnd(&rnd),
	_hasArea(false), _areaLeft(0), _areaTop(0), _areaRight(0), _areaBottom(0), _currentItem(0xFFFF),
	_nativeLayout(false), _nativeDone(false), _nativeNextLine(0) {

	load(gct);
}

GCTFile::~GCTFile() {
}

void GCTFile::load(Common::SeekableReadStream &gct) {
	gct.skip(4); // Required buffer size
	gct.skip(2); // Unknown

	// Read the selector and line counts for each item
	const uint16 itemCount = gct.readUint16LE();
	_items.resize(itemCount);

	for (auto &item : _items) {
		const uint16 selector  = gct.readUint16LE();
		const uint16 lineCount = gct.readUint16LE();

		item.selector = selector;
		item.lines.resize(lineCount);
	}

	// Read all item lines
	for (auto &item : _items) {
		for (auto &line : item.lines) {
			const uint16 lineSize = gct.readUint16LE();

			readLine(gct, line, lineSize);
		}
	}

	if (gct.err())
		error("GCTFile::load(): Failed reading GCT");
}

void GCTFile::readLine(Common::SeekableReadStream &gct, Line &line, uint16 lineSize) const {
	line.chunks.push_back(Chunk());

	while (lineSize > 0) {
		byte c = gct.readByte();
		lineSize--;

		if (c == 0) {
			// Command byte

			if (lineSize == 0)
				break;

			byte cmd = gct.readByte();
			lineSize--;

			// Line end command
			if (cmd == 0)
				break;

			// Item reference command
			if (cmd == 1) {
				if (lineSize < 2) {
					warning("GCTFile::readLine(): Item reference command is missing parameters");
					break;
				}

				const uint32 itemRef = gct.readUint16LE();
				lineSize -= 2;

				line.chunks.push_back(Chunk());
				line.chunks.back().type = kChunkTypeItem;
				line.chunks.back().item = itemRef;

				line.chunks.push_back(Chunk());
				continue;
			}

			warning("GCTFile::readLine(): Invalid command 0x%02X", cmd);
			break;
		}

		// Text
		line.chunks.back().type = kChunkTypeString;
		line.chunks.back().text += (char)c;
	}

	// Skip bytes we didn't read (because of errors)
	gct.skip(lineSize);

	// Remove empty chunks from the end of the list
	while (!line.chunks.empty() && (line.chunks.back().type == kChunkTypeNone))
		line.chunks.pop_back();
}

uint16 GCTFile::getLineCount(uint item) const {
	if (item >= _items.size())
		return 0;

	return _items[item].lines.size();
}

void GCTFile::selectLine(uint item, uint16 line) {
	if (item >= _items.size())
		return;

	_items[item].selector = line;
}

void GCTFile::setText(uint item, uint16 line, const Common::String &text) {
	if ((item >= _items.size()) || (line >= _items[item].lines.size()))
		return;

	_items[item].lines[line].chunks.clear();
	_items[item].lines[line].chunks.push_back(Chunk());

	_items[item].lines[line].chunks.back().type = kChunkTypeString;
	_items[item].lines[line].chunks.back().text = text;
}

void GCTFile::setText(uint item, const Common::String &text) {
	if (item >= _items.size())
		return;

	_items[item].selector = 0;

	_items[item].lines.resize(1);

	setText(item, 0, text);
}

void GCTFile::replaceTextChunk(uint item, uint16 line, const char *from, const char *to) {
	if (item >= _items.size() || line >= _items[item].lines.size())
		return;

	Chunks &chunks = _items[item].lines[line].chunks;
	for (Chunks::iterator chunk = chunks.begin(); chunk != chunks.end(); ++chunk) {
		if (chunk->type == kChunkTypeString && chunk->text == from) {
			chunk->text = to;
			return;
		}
	}
}

void GCTFile::trimEmptyTail(uint item, uint16 count) {
	if (item >= _items.size() || count > _items[item].lines.size())
		return;

	Lines &lines = _items[item].lines;
	const uint keep = lines.size() - count;
	for (uint i = keep; i < lines.size(); ++i)
		if (!lines[i].chunks.empty())
			return;
	lines.resize(keep);
}

void GCTFile::reset() {
	_currentItem = 0xFFFF;
	_currentText.clear();
	_nativeDone = false;
	_nativeNextLine = 0;
}

void GCTFile::enableNativeLayout() {
	_nativeLayout = true;
	reset();
}

Common::String GCTFile::getNativeLineText(const Line &line) {
	Common::String text;
	for (Chunks::const_iterator chunk = line.chunks.begin(); chunk != line.chunks.end(); ++chunk) {
		if (chunk->type == kChunkTypeString)
			text += chunk->text;
		else if (chunk->type == kChunkTypeItem)
			text += getNativeItemText(chunk->item);
	}
	return text;
}

Common::String GCTFile::getNativeItemText(uint item) {
	if (item >= _items.size() || _items[item].lines.empty())
		return Common::String();
	Item &source = _items[item];
	if (source.selector == kSelectorRandom)
		source.selector = _rnd->getRandomNumber(source.lines.size() - 1);
	const uint line = source.selector == kSelectorAll ? 0 : source.selector;
	if (line >= source.lines.size())
		return Common::String();
	return getNativeLineText(source.lines[line]);
}

bool GCTFile::nextNativeLine() {
	if (_nativeDone)
		return false;
	Item &source = _items[_currentItem];
	if (source.lines.empty()) {
		_nativeDone = true;
		return false;
	}
	if (source.selector == kSelectorRandom)
		source.selector = _rnd->getRandomNumber(source.lines.size() - 1);
	const uint line = source.selector == kSelectorAll ? _nativeNextLine++ : source.selector;
	_nativeDone = source.selector != kSelectorAll || _nativeNextLine >= source.lines.size();
	if (line >= source.lines.size()) {
		_nativeDone = true;
		return false;
	}
	_currentText.push_back(getNativeLineText(source.lines[line]));
	return true;
}

Common::String GCTFile::getLineText(const Line &line) const {
	Common::String text;

	// Go over all chunks in this line
	for (const auto &chunk : line.chunks) {
		// A chunk is either a direct string, or a reference to another item

		if (chunk.type == kChunkTypeItem) {
			Common::List<Common::String> lines;

			getItemText(chunk.item, lines);
			if (lines.empty())
				continue;

			if (lines.size() > 1)
				warning("GCTFile::getLineText(): Referenced item has multiple lines");

			text += lines.front();
		} else if (chunk.type == kChunkTypeString)
			text += chunk.text;
	}

	return text;
}

void GCTFile::getItemText(uint item, Common::List<Common::String> &text) const {
	text.clear();

	if ((item >= _items.size()) || _items[item].lines.empty())
		return;

	uint16 line = _items[item].selector;

	// Draw all lines
	if (line == kSelectorAll) {
		for (Lines::const_iterator l = _items[item].lines.begin(); l != _items[item].lines.end(); ++l)
			text.push_back(getLineText(*l));

		return;
	}

	// Draw random line
	if (line == kSelectorRandom)
		line = _rnd->getRandomNumber(_items[item].lines.size() - 1);

	if (line >= _items[item].lines.size())
		return;

	text.push_back(getLineText(_items[item].lines[line]));
}

void GCTFile::setArea(int16 left, int16 top, int16 right, int16 bottom) {
	trashBuffer();

	_hasArea = false;

	const int16 width  = right  - left + 1;
	const int16 height = bottom - top  + 1;
	if ((width <= 0) || (height <= 0))
		return;

	_areaLeft   = left;
	_areaTop    = top;
	_areaRight  = right;
	_areaBottom = bottom;

	_hasArea = true;

	resizeBuffer(width, height);
}

bool GCTFile::clear(Surface &dest, int16 &left, int16 &top, int16 &right, int16 &bottom) {
	return restoreScreen(dest, left, top, right, bottom);
}

bool GCTFile::fill(Surface &dest, uint8 color, int16 &left, int16 &top, int16 &right, int16 &bottom) {
	left   = _areaLeft;
	top    = _areaTop;
	right  = _areaRight;
	bottom = _areaBottom;

	if (!hasSavedBackground())
		saveScreen(dest, left, top, right, bottom);

	dest.fillRect(left, top, right, bottom, color);

	return true;
}

bool GCTFile::finished() const {
	return (_currentItem != 0xFFFF) && _currentText.empty() && (!_nativeLayout || _nativeDone);
}

bool GCTFile::draw(Surface &dest, uint16 item, const Font &font, uint8 color,
				   int16 &left, int16 &top, int16 &right, int16 &bottom, Common::String *drawnText) {
	if (drawnText)
		drawnText->clear();

	if ((item >= _items.size()) || !_hasArea)
		return false;

	left   = _areaLeft;
	top    = _areaTop;
	right  = _areaRight;
	bottom = _areaBottom;

	const int16 width  = right  - left + 1;
	const int16 height = bottom - top  + 1;
	if (_nativeLayout && (!font.getCharWidth() || !font.getCharHeight() || width < font.getCharWidth()))
		return false;

	const uint lineCount = height / font.getCharHeight();
	if (lineCount == 0)
		return false;

	if (!hasSavedBackground())
		saveScreen(dest, left, top, right, bottom);

	if (item != _currentItem) {
		_currentItem = item;
		if (_nativeLayout) {
			_nativeNextLine = 0;
			_nativeDone = false;
			_currentText.clear();
		} else {
			getItemText(_currentItem, _currentText);
		}
	}

	if (_currentText.empty() && (!_nativeLayout || !nextNativeLine()))
		return false;

	int16 y = top;
	for (uint i = 0; i < lineCount; i++, y += font.getCharHeight()) {
		if (_currentText.empty() && (!_nativeLayout || !nextNativeLine()))
			break;
		Common::String line = _currentText.front();
		if (_nativeLayout) {
			uint first = 0;
			while (first < line.size() && line[first] == ' ')
				++first;
			line.erase(0, first);
			const uint columns = width / font.getCharWidth();
			if (line.size() > columns) {
				uint end = columns;
				while (end && line[end] != ' ')
					--end;
				while (end && line[end] == ' ')
					--end;
				// A break after just the first character also uses the hard split.
				const uint count = end ? end + 1 : columns;
				_currentText.front() = line.substr(count);
				line = line.substr(0, count);
			} else {
				_currentText.pop_front();
			}
		} else {
			_currentText.pop_front();
		}
		uint centeredLength = line.size();
		if (_nativeLayout) {
			// Once Upon wraps as in the original, which also rounded x to a
			// whole cell. Center its visible row in pixels, ignoring end padding.
			while (centeredLength && line[centeredLength - 1] == ' ')
				--centeredLength;
		}
		const int16 x = left + (width - centeredLength * font.getCharWidth()) / 2;

		Common::String visible;
		font.drawString(line, x, y, color, 0, true, dest, drawnText ? &visible : nullptr);
		if (drawnText && !visible.empty()) {
			if (!drawnText->empty())
				*drawnText += '\n';
			*drawnText += visible;
		}
	}

	return true;
}

} // End of namespace Gob
