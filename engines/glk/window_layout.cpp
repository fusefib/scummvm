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

#include "glk/windows.h"
#include "glk/conf.h"
#include "glk/window_graphics.h"
#include "glk/window_pair.h"
#include "glk/window_text_buffer.h"
#include "glk/window_text_grid.h"

namespace Glk {

struct PreparedWindowLayout {
	Window *_window;
	Rect _box;
	TextBufferWindow::ReflowState *_buffer;
	TextGridWindow::ResizeState *_grid;
	Graphics::ManagedSurface *_surface;
	PreparedWindowLayout(Window *window, const Rect &box) :
		_window(window), _box(box), _buffer(nullptr), _grid(nullptr), _surface(nullptr) {}
	~PreparedWindowLayout() {
		delete _buffer;
		delete _grid;
		delete _surface;
	}
};

Windows::PreparedLayout::~PreparedLayout() {
	for (uint i = 0; i < _windows.size(); ++i)
		delete _windows[i];
}

bool Windows::PreparedLayout::prepare(Window *window, const Rect &box,
		const Conf &settings, const Conf &before, const Conf &after, const PreparedFonts &fonts) {
	PreparedWindowLayout *entry = new PreparedWindowLayout(window, box);
	_windows.push_back(entry);
	switch (window->_type) {
	case wintype_Pair: {
		PairWindow *pair = static_cast<PairWindow *>(window);
		if (pair->_dir == winmethod_Arbitrary)
			return false;
		Rect first, second;
		pair->splitRectangles(box, settings, first, second);
		return prepare(pair->_children[pair->_backward ? 1 : 0], first, settings, before, after, fonts) &&
			prepare(pair->_children[pair->_backward ? 0 : 1], second, settings, before, after, fonts);
	}
	case wintype_TextBuffer: {
		TextBufferWindow *buffer = static_cast<TextBufferWindow *>(window);
		WindowStyle styles[style_NUMSTYLES];
		buffer->resolveStyles(before, after, styles, true);
		entry->_buffer = buffer->prepareReflow(box, settings, fonts, styles);
		return entry->_buffer != nullptr;
	}
	case wintype_TextGrid:
		entry->_grid = static_cast<TextGridWindow *>(window)->prepareResize(box, settings, true);
		return entry->_grid != nullptr;
	case wintype_Graphics:
		return static_cast<GraphicsWindow *>(window)->prepareResize(box, entry->_surface);
	default:
		return true;
	}
}

bool Windows::prepareLayout(Conf &settings, const Conf &before, const Conf &after,
		const PreparedFonts &fonts, PreparedLayout &layout) {
	assert(layout._windows.empty());
	return !_rootWin || layout.prepare(_rootWin, rootBounds(settings), settings, before, after, fonts);
}

void Windows::installLayout(PreparedLayout &layout) {
	for (uint i = 0; i < layout._windows.size(); ++i) {
		PreparedWindowLayout &entry = *layout._windows[i];
		if (entry._buffer) {
			static_cast<TextBufferWindow *>(entry._window)->installReflow(*entry._buffer);
		} else if (entry._grid) {
			static_cast<TextGridWindow *>(entry._window)->installResize(*entry._grid);
		} else if (entry._window->_type == wintype_Graphics) {
			static_cast<GraphicsWindow *>(entry._window)->installResize(entry._box, entry._surface);
			entry._surface = nullptr;
		} else {
			entry._window->_bbox = entry._box;
		}
	}
}

} // End of namespace Glk
