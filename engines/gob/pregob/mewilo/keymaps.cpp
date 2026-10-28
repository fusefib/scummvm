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
 */

#include "gob/pregob/mewilo/keymaps.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/standard-actions.h"

#include "common/translation.h"

namespace Gob {
namespace Mewilo {

Common::KeymapArray createMewiloKeymaps() {
	using namespace Common;
	Keymap *map = new Keymap(Keymap::kKeymapTypeGame, "mewilo", "Mewilo");
	map->setPartialMatchAllowed(false);

	Action *action = new Action(kStandardActionLeftClick, _("Left click"));
	action->setLeftClickEvent();
	action->addDefaultInputMapping("MOUSE_LEFT");
	action->addDefaultInputMapping("JOY_A");
	map->addAction(action);
	action = new Action(kStandardActionRightClick, _("Right click"));
	action->setRightClickEvent();
	action->addDefaultInputMapping("MOUSE_RIGHT");
	action->addDefaultInputMapping("JOY_B");
	map->addAction(action);

	// EVENT_MAINMENU is handled by ScummVM's event manager, before Gob's
	// keyboard translator can turn F5 into Escape.
	action = new Action(kStandardActionOpenMainMenu, _("Game menu"));
	action->setEvent(EVENT_MAINMENU);
	action->addDefaultInputMapping("F5");
	action->addDefaultInputMapping("JOY_START");
	map->addAction(action);

	action = new Action("MEWILO_RETURN", _("Continue"));
	action->setKeyEvent(KeyState(KEYCODE_RETURN, ASCII_RETURN));
	action->addDefaultInputMapping("RETURN");
	action->addDefaultInputMapping("KP_ENTER");
	action->addDefaultInputMapping("JOY_X");
	map->addAction(action);
	action = new Action("MEWILO_ESCAPE", _("Cancel"));
	action->setKeyEvent(KeyState(KEYCODE_ESCAPE, ASCII_ESCAPE));
	action->addDefaultInputMapping("ESCAPE");
	action->addDefaultInputMapping("JOY_Y");
	map->addAction(action);

	// Unmapped printable keys (including Space and digits) pass through for
	// free text and numbered choices. No context switching or gameplay remap.
	KeymapArray maps;
	maps.push_back(map);
	return maps;
}

} // End of namespace Mewilo
} // End of namespace Gob
