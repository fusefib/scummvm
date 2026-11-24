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
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 */

#include "gob/pregob/onceupon/input.h"
#include "common/system.h"
#include "common/util.h"

namespace Gob {
namespace OnceUpon {

KeyboardInput::KeyboardInput() : _registered(false), _focusLost(false) {
	memset(_pressed, 0, sizeof(_pressed));
}

KeyboardInput::~KeyboardInput() {
	end();
}

void KeyboardInput::begin() {
	end();
	memset(_pressed, 0, sizeof(_pressed));
	// Dialogs can consume releases before they reach Gob's input loop.
	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, Common::EventManager::kEventRemapperPriority + 1, false);
	_registered = true;
}

void KeyboardInput::end() {
	if (_registered)
		g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);
	_registered = false;
}

bool KeyboardInput::notifyEvent(const Common::Event &event) {
	if (event.type == Common::EVENT_FOCUS_LOST) {
		_focusLost = true;
		memset(_pressed, 0, sizeof(_pressed));
	} else if ((event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_KEYUP) &&
			event.kbd.keycode > Common::KEYCODE_INVALID && event.kbd.keycode < Common::KEYCODE_LAST) {
		if (event.type == Common::EVENT_KEYUP)
			_pressed[event.kbd.keycode] = false;
		else if (!event.kbdRepeat)
			_pressed[event.kbd.keycode] = true;
	}
	return false;
}

void KeyboardInput::clampActionPointer(int16 &x, int16 &y) {
	x = CLIP<int16>(x, 15, 288);
	y = CLIP<int16>(y, 49, 171);
}

void KeyboardInput::moveActionPointer(int16 &x, int16 &y, uint32 heldKeys) {
	if (heldKeys & 1) y -= 4;
	if (heldKeys & 2) y += 4;
	if (heldKeys & 4) x += 4;
	if (heldKeys & 8) x -= 4;
}

bool KeyboardInput::takeFocusLost() {
	const bool lost = _focusLost;
	_focusLost = false;
	return lost;
}

uint32 KeyboardInput::heldKeys() const {
	static const struct {
		Common::KeyCode key;
		uint32 mask;
	} bindings[] = {
		{Common::KEYCODE_UP, 0x0001}, {Common::KEYCODE_DOWN, 0x0002},
		{Common::KEYCODE_RIGHT, 0x0004}, {Common::KEYCODE_LEFT, 0x0008},
		{Common::KEYCODE_RETURN, 0x0010}, {Common::KEYCODE_KP_ENTER, 0x0010},
		{Common::KEYCODE_SPACE, 0x0020}, {Common::KEYCODE_ESCAPE, 0x0040},
		{Common::KEYCODE_LCTRL, 0x0080}, {Common::KEYCODE_RCTRL, 0x0080},
		{Common::KEYCODE_LSHIFT, 0x0100}, {Common::KEYCODE_RSHIFT, 0x0200},
		{Common::KEYCODE_LALT, 0x0400}, {Common::KEYCODE_RALT, 0x0400},
		{Common::KEYCODE_F1, 0x0800}, {Common::KEYCODE_F2, 0x1000},
		{Common::KEYCODE_F3, 0x2000}, {Common::KEYCODE_F4, 0x4000},
		{Common::KEYCODE_HOME, 0x8000}, {Common::KEYCODE_KP7, 0x8000},
		{Common::KEYCODE_PAGEUP, 0x10000}, {Common::KEYCODE_KP9, 0x10000},
		{Common::KEYCODE_END, 0x20000}, {Common::KEYCODE_KP1, 0x20000},
		{Common::KEYCODE_PAGEDOWN, 0x40000}, {Common::KEYCODE_KP3, 0x40000}
	};
	uint32 result = 0;
	for (uint i = 0; i < ARRAYSIZE(bindings); ++i)
		if (_pressed[bindings[i].key])
			result |= bindings[i].mask;
	return result;
}

uint8 KeyboardInput::confirmationKeys() const {
	return (_pressed[Common::KEYCODE_y] ? 1 : 0) | (_pressed[Common::KEYCODE_j] ? 2 : 0) |
		(_pressed[Common::KEYCODE_s] ? 4 : 0) | (_pressed[Common::KEYCODE_o] ? 8 : 0) | (_pressed[Common::KEYCODE_n] ? 16 : 0);
}

} // End of namespace OnceUpon
} // End of namespace Gob
