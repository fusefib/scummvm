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

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymapper.h"
#include "backends/keymapper/standard-actions.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/util.h"

namespace Gob {
namespace OnceUpon {

// Keymapper preserves this game-local tag on the generated key/mouse pair.
// Unbound physical controls arrive without it and must not bypass remapping.
static const Common::CustomEventType kMappedInput = 0x4F55494E;

static void addKey(Common::Keymap &map, const char *id,
		const Common::U32String &description, Common::KeyCode key, uint16 ascii,
		const char *keyboard, const char *secondary = nullptr) {
	Common::Action *action = new Common::Action(id, description);
	action->setKeyEvent(Common::KeyState(key, ascii));
	action->event.customType = kMappedInput;
	action->addDefaultInputMapping(keyboard);
	if (secondary)
		action->addDefaultInputMapping(secondary);
	map.addAction(action);
}

Common::KeymapArray createKeymaps() {
	using namespace Common;
	Keymap *common = new Keymap(Keymap::kKeymapTypeGame, "onceupon-common", _("Mouse controls"));
	Keymap *controls = new Keymap(Keymap::kKeymapTypeGame, "onceupon-controls", _("Game controls"));
	Keymap *pointer = new Keymap(Keymap::kKeymapTypeGame, "onceupon-pointer", _("Selection and exploration"));
	Keymap *forest = new Keymap(Keymap::kKeymapTypeGame, "onceupon-forest", _("Forest controls"));
	forest->setEnabled(false);

	Action *action = new Action(kStandardActionLeftClick, _("Left click"));
	action->setLeftClickEvent();
	action->event.customType = kMappedInput;
	action->addDefaultInputMapping("MOUSE_LEFT");
	action->addDefaultInputMapping("JOY_A");
	common->addAction(action);
	action = new Action(kStandardActionRightClick, _("Right click"));
	action->setRightClickEvent();
	action->event.customType = kMappedInput;
	action->addDefaultInputMapping("MOUSE_RIGHT");
	action->addDefaultInputMapping("JOY_B");
	common->addAction(action);

	addKey(*controls, "MENU", _("Game menu"), KEYCODE_ESCAPE, ASCII_ESCAPE, "ESCAPE", "F5");
	addKey(*controls, "CONFIRM", _("Confirm"), KEYCODE_RETURN, ASCII_RETURN, "RETURN", "KP_ENTER");
	addKey(*pointer, "CONTINUE", _("Continue"), KEYCODE_SPACE, ASCII_SPACE, "SPACE");
	addKey(*pointer, "UP", _("Move cursor up"), KEYCODE_UP, 0, "UP", "JOY_UP");
	addKey(*pointer, "DOWN", _("Move cursor down"), KEYCODE_DOWN, 0, "DOWN", "JOY_DOWN");
	addKey(*pointer, "LEFT", _("Move cursor left"), KEYCODE_LEFT, 0, "LEFT", "JOY_LEFT");
	addKey(*pointer, "RIGHT", _("Move cursor right"), KEYCODE_RIGHT, 0, "RIGHT", "JOY_RIGHT");

	addKey(*forest, "LEFT", _("Walk left"), KEYCODE_LEFT, 0, "LEFT", "JOY_LEFT");
	addKey(*forest, "RIGHT", _("Walk right"), KEYCODE_RIGHT, 0, "RIGHT", "JOY_RIGHT");
	addKey(*forest, "JUMP", _("Jump"), KEYCODE_UP, 0, "UP", "JOY_UP");
	addKey(*forest, "CROUCH", _("Crouch"), KEYCODE_DOWN, 0, "DOWN", "JOY_DOWN");
	addKey(*forest, "JUMP_LEFT", _("Jump left"), KEYCODE_HOME, 0, "HOME", "KP7");
	addKey(*forest, "JUMP_RIGHT", _("Jump right"), KEYCODE_PAGEUP, 0, "PAGEUP", "KP9");
	addKey(*forest, "CROUCH_LEFT", _("Crouch left"), KEYCODE_END, 0, "END", "KP1");
	addKey(*forest, "CROUCH_RIGHT", _("Crouch right"), KEYCODE_PAGEDOWN, 0, "PAGEDOWN", "KP3");
	addKey(*forest, "CAST", _("Cast a spell"), KEYCODE_SPACE, ASCII_SPACE, "SPACE", "JOY_X");
	addKey(*forest, "STOP", _("Stop"), KEYCODE_LCTRL, 0, "LCTRL", "RCTRL");

	// Ctrl+F5 and controller Start remain the global map's main-menu actions.
	KeymapArray maps;
	maps.push_back(common);
	maps.push_back(controls);
	maps.push_back(pointer);
	maps.push_back(forest);
	return maps;
}

KeyboardInput::KeyboardInput() : _registered(false), _context(kPointerInput), _focusLost(false) {
	memset(_pressed, 0, sizeof(_pressed));
	releaseActions();
}

KeyboardInput::~KeyboardInput() {
	end();
}

void KeyboardInput::begin() {
	end();
	memset(_pressed, 0, sizeof(_pressed));
	releaseActions();
	_context = kPointerInput;
	applyContext();
	// Dialogs can consume releases before they reach Gob's input loop.
	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, Common::EventManager::kEventRemapperPriority + 1, false);
	_registered = true;
}

void KeyboardInput::end() {
	if (_registered)
		g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);
	_registered = false;
}

void KeyboardInput::releaseActions() {
	memset(_mapped, 0, sizeof(_mapped));
}

void KeyboardInput::setContext(InputContext context) {
	if (context == _context)
		return;
	releaseActions();
	_context = context;
	applyContext();
}

void KeyboardInput::applyContext() {
	Common::Keymapper *mapper = g_system->getEventManager()->getKeymapper();
	mapper->setGameKeymapState("onceupon-controls", _context != kTextInput);
	mapper->setGameKeymapState("onceupon-pointer", _context == kPointerInput);
	mapper->setGameKeymapState("onceupon-forest", _context == kForestInput);
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

bool KeyboardInput::mappedControlsActive() const {
	const Common::Keymapper *mapper = g_system->getEventManager()->getKeymapper();
	const Common::Keymap *map = mapper->getKeymap("onceupon-common");
	return map && map->isEnabled() && mapper->isEnabled() &&
		mapper->enabledKeymapType() == Common::Keymap::kKeymapTypeGame;
}

bool KeyboardInput::isControlKey(Common::KeyCode key) {
	switch (key) {
	case Common::KEYCODE_UP: case Common::KEYCODE_DOWN:
	case Common::KEYCODE_LEFT: case Common::KEYCODE_RIGHT:
	case Common::KEYCODE_HOME: case Common::KEYCODE_KP7:
	case Common::KEYCODE_PAGEUP: case Common::KEYCODE_KP9:
	case Common::KEYCODE_END: case Common::KEYCODE_KP1:
	case Common::KEYCODE_PAGEDOWN: case Common::KEYCODE_KP3:
	case Common::KEYCODE_LCTRL: case Common::KEYCODE_RCTRL:
	case Common::KEYCODE_ESCAPE: case Common::KEYCODE_F5:
	case Common::KEYCODE_RETURN: case Common::KEYCODE_KP_ENTER:
	case Common::KEYCODE_SPACE:
		return true;
	default:
		return false;
	}
}

bool KeyboardInput::notifyEvent(const Common::Event &event) {
	if (event.type == Common::EVENT_FOCUS_LOST) {
		_focusLost = true;
		memset(_pressed, 0, sizeof(_pressed));
		releaseActions();
	} else if ((event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_KEYUP) &&
			event.kbd.keycode > Common::KEYCODE_INVALID && event.kbd.keycode < Common::KEYCODE_LAST) {
		if (mappedControlsActive() && event.customType == kMappedInput) {
			// Multiple bindings can hold one action; releasing one must not stop it.
			if (event.type == Common::EVENT_KEYUP) {
				if (_mapped[event.kbd.keycode])
					--_mapped[event.kbd.keycode];
			} else if (!event.kbdRepeat) {
				++_mapped[event.kbd.keycode];
			}
		} else if (mappedControlsActive() && _context != kTextInput && isControlKey(event.kbd.keycode)) {
			return true;
		} else if (event.type == Common::EVENT_KEYUP) {
			_pressed[event.kbd.keycode] = false;
		} else if (!event.kbdRepeat) {
			_pressed[event.kbd.keycode] = true;
		}
	} else if (mappedControlsActive() && event.customType != kMappedInput &&
			(event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_LBUTTONUP ||
			event.type == Common::EVENT_RBUTTONDOWN || event.type == Common::EVENT_RBUTTONUP)) {
		return true;
	}
	return false;
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
	const bool mapped = mappedControlsActive() && _context != kTextInput;
	for (uint i = 0; i < ARRAYSIZE(bindings); ++i)
		if (_mapped[bindings[i].key] || (_pressed[bindings[i].key] && (!mapped || !isControlKey(bindings[i].key))))
			result |= bindings[i].mask;
	return result;
}

uint8 KeyboardInput::confirmationKeys() const {
	return (_pressed[Common::KEYCODE_y] ? 1 : 0) | (_pressed[Common::KEYCODE_j] ? 2 : 0) |
		(_pressed[Common::KEYCODE_s] ? 4 : 0) | (_pressed[Common::KEYCODE_o] ? 8 : 0) | (_pressed[Common::KEYCODE_n] ? 16 : 0);
}

} // End of namespace OnceUpon
} // End of namespace Gob
