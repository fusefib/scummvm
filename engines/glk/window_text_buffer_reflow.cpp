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

#include "glk/window_text_buffer.h"
#include "glk/glk.h"
#include "common/ptr.h"

namespace Glk {

TextBufferWindow::ReflowState::ReflowState() : _copyBuf(nullptr),
	_width(0), _height(0), _yAdj(0), _scrollPos(0), _lastSeen(0), _scrollMax(0),
	_fence(0), _cursor(0), _leftWidth(0), _leftLines(0), _rightWidth(0),
	_rightLines(0), _hidden(false) {
}

TextBufferWindow::ReflowState::~ReflowState() {
	delete[] _copyBuf;
	for (uint i = 0; i < _rows.size(); ++i) {
		if (_rows[i]._lPic)
			_rows[i]._lPic->decrement();
		if (_rows[i]._rPic)
			_rows[i]._rPic->decrement();
	}
}

bool TextBufferWindow::ReflowState::nextLine(bool forced) {
	// Array growth may double capacity.
	if (_rows.size() >= kMaxRows)
		return false;
	_rows.back()._newLine = forced;
	if (_leftLines && !--_leftLines)
		_leftWidth = 0;
	if (_rightLines && !--_rightLines)
		_rightWidth = 0;
	TextBufferRow row;
	row._lm = _leftWidth;
	row._rm = _rightWidth;
	_rows.push_back(row);
	return true;
}

bool TextBufferWindow::ReflowState::append(uint32 ch, const Attributes &attr,
		const Conf &settings, const PreparedFonts &fonts, const WindowStyle *styles) {
	if (ch == '\n')
		return nextLine(true);
	if (_rows.back()._len == TBLINELEN - 1 && !nextLine(false))
		return false;

	TextBufferRow &row = _rows.back();
	row._chars[row._len] = ch;
	row._attrs[row._len++] = attr;
	const int available = MAX(1, (_box.width() - settings._tMarginX * 2 -
		settings._scrollWidth - 4) * GLI_SUBPIX - row._lm - row._rm);
	int len = row._len;
	while (len > 1 && row._chars[len - 1] == ' ' &&
			styles[row._attrs[len - 1].style].bg == settings._windowColor &&
			!styles[row._attrs[len - 1].style].reverse)
		--len;
	size_t width = 0;
	int start = 0;
	for (int i = 1; i <= len; ++i) {
		if (i == len || row._attrs[start] != row._attrs[i]) {
			width += g_vm->_screen->preparedTextWidth(fonts,
				row._attrs[start].attrFont(styles),
				Common::U32String((const char32_t *)row._chars + start, i - start));
			start = i;
		}
	}
	if (width < (uint)available)
		return true;

	int split = row._len > 1 ? row._len - 1 : row._len;
	for (int i = row._len - 1; i > 0; --i) {
		if (row._chars[i] == ' ') {
			split = i + 1;
			break;
		}
	}
	const int remaining = row._len - split;
	TextBufferRow tail;
	Common::copy(row._chars + split, row._chars + row._len, tail._chars);
	Common::copy(row._attrs + split, row._attrs + row._len, tail._attrs);
	row._len = split;
	if (!nextLine(false))
		return false;
	TextBufferRow &next = _rows.back();
	Common::copy(tail._chars, tail._chars + remaining, next._chars);
	Common::copy(tail._attrs, tail._attrs + remaining, next._attrs);
	next._len = remaining;
	return true;
}

TextBufferWindow::ReflowState *TextBufferWindow::prepareReflow(const Rect &box,
		const Conf &settings, const PreparedFonts &fonts, const WindowStyle *styles) const {
	if (settings._propInfo._cellW <= 0 || settings._propInfo._cellH <= 0 ||
			_scrollMax < 0 || (uint)_scrollMax >= _lines.size() ||
			(uint)_scrollMax >= ReflowState::kMaxRows)
		return nullptr;
	Common::ScopedPtr<ReflowState> state(new ReflowState());
	state->_box = box;
	state->_width = MAX(0, (box.width() - settings._tMarginX * 2 -
		settings._scrollWidth) / settings._propInfo._cellW);
	state->_height = MAX(0, (box.height() - settings._tMarginY * 2) /
		settings._propInfo._cellH);
	state->_yAdj = MAX(0, box.height() - state->_height * settings._propInfo._cellH -
		settings._tMarginY * 2);
	state->_box.top += state->_yAdj;
	state->_hidden = !state->_width || !state->_height;
	if (_inBuf && _height > 0 && (state->_hidden || box.width() -
			settings._tMarginX * 2 - settings._scrollWidth < settings._propInfo._cellW + 8))
		return nullptr;
	if (state->_hidden) {
		state->_rows = _lines;
		state->_rows[0]._len = _numChars;
		for (uint i = 0; i < state->_rows.size(); ++i) {
			if (state->_rows[i]._lPic)
				state->_rows[i]._lPic->increment();
			if (state->_rows[i]._rPic)
				state->_rows[i]._rPic->increment();
		}
		state->_scrollPos = _scrollPos;
		state->_lastSeen = _lastSeen;
		state->_scrollMax = _scrollMax;
		state->_fence = _inFence;
		state->_cursor = _inCurs;
		state->_leftWidth = _ladjw;
		state->_leftLines = _ladjn;
		state->_rightWidth = _radjw;
		state->_rightLines = _radjn;
		return state.release();
	}

	state->_rows.push_back(TextBufferRow());
	uint64 offset = 0, scrollAnchor = 0, seenAnchor = 0;
	const bool input = _lineRequest || _lineRequestUni;
	for (int i = _scrollMax; i >= 0; --i) {
		const TextBufferRow &old = _lines[i];
		if (i == _scrollPos)
			scrollAnchor = offset;
		if (i == _lastSeen)
			seenAnchor = offset;

		// Pictures retain their logical position and one reference per row.
		if ((old._lPic || old._rPic) && (state->_rows.back()._len ||
				(old._lPic && state->_rows.back()._lPic) ||
				(old._rPic && state->_rows.back()._rPic)) &&
				!state->nextLine(false))
			return nullptr;
		TextBufferRow &row = state->_rows.back();
		if (old._lPic) {
			row._lPic = old._lPic;
			row._lPic->increment();
			row._lHyper = old._lHyper;
			row._lm = state->_leftWidth = (old._lPic->w + settings._tMarginX) * GLI_SUBPIX;
			state->_leftLines = (old._lPic->h + settings._propInfo._cellH - 1) / settings._propInfo._cellH;
		}
		if (old._rPic) {
			row._rPic = old._rPic;
			row._rPic->increment();
			row._rHyper = old._rHyper;
			row._rm = state->_rightWidth = (old._rPic->w + settings._tMarginX) * GLI_SUBPIX;
			state->_rightLines = (old._rPic->h + settings._propInfo._cellH - 1) / settings._propInfo._cellH;
		}
		const int len = i ? old._len : input ? _inFence : _numChars;
		for (int j = 0; j < len; ++j) {
			if (!state->append(old._chars[j], old._attrs[j], settings, fonts, styles))
				return nullptr;
		}
		offset += len;
		if (old._newLine) {
			if (!state->nextLine(true))
				return nullptr;
			++offset;
		}
	}
	if (input) {
		const int len = _numChars - _inFence;
		if (state->_rows.back()._len + len >= TBLINELEN && !state->nextLine(false))
			return nullptr;
		TextBufferRow &row = state->_rows.back();
		state->_fence = row._len;
		state->_cursor = row._len + _inCurs - _inFence;
		Common::copy(_chars + _inFence, _chars + _numChars, row._chars + row._len);
		Common::copy(_attrs + _inFence, _attrs + _numChars, row._attrs + row._len);
		row._len += len;
	}

	const int count = state->_rows.size();
	state->_scrollMax = count - 1;
	offset = 0;
	for (int i = 0; i < count; ++i) {
		if (offset <= scrollAnchor)
			state->_scrollPos = count - i - 1;
		if (offset <= seenAnchor)
			state->_lastSeen = count - i - 1;
		offset += state->_rows[i]._len + state->_rows[i]._newLine;
	}
	if (!_scrollPos)
		state->_scrollPos = 0;
	state->_scrollPos = CLIP(state->_scrollPos, 0, MAX(0, count - state->_height));
	state->_lastSeen = MIN(state->_lastSeen, state->_scrollMax);
	for (int i = 0; i < count / 2; ++i)
		SWAP(state->_rows[i], state->_rows[count - i - 1]);
	const uint capacity = MAX((uint)SCROLLBACK, MAX((uint)count, (uint)state->_height));
	if (capacity > ReflowState::kMaxRows || (uint)state->_height > 0x7fffffffU / (TBLINELEN * sizeof(uint32)))
		return nullptr;
	state->_rows.resize(capacity);
	state->_copyBuf = new uint32[state->_height * TBLINELEN]();
	return state.release();
}

void TextBufferWindow::installReflow(ReflowState &state) {
	_lines.swap(state._rows);
	SWAP(_copyBuf, state._copyBuf);
	_bbox = state._box;
	_yAdj = state._yAdj;
	_width = state._width;
	_height = state._height;
	_scrollBack = _lines.size();
	_scrollMax = state._scrollMax;
	_scrollPos = state._scrollPos;
	_lastSeen = state._lastSeen;
	_inFence = state._fence;
	_inCurs = state._cursor;
	_ladjw = state._leftWidth;
	_ladjn = state._leftLines;
	_radjw = state._rightWidth;
	_radjn = state._rightLines;
	_numChars = _lines[0]._len;
	_chars = _lines[0]._chars;
	_attrs = _lines[0]._attrs;
	_copyPos = 0;
	_needsReflow = state._hidden;
	touchScroll();
}

} // End of namespace Glk
