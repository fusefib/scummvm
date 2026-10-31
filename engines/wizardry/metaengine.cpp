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

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "base/plugins.h"
#include "common/translation.h"
#include "wizardry/wizardry.h"

class WizardryMetaEngine : public AdvancedMetaEngine<Wizardry::WizardryGameDescription> {
public:
	const char *getName() const override { return "wizardry"; }
	Common::KeymapArray initKeymaps(const char *) const override {
		using namespace Common;
		Keymap *map = new Keymap(Keymap::kKeymapTypeGame, "wizardry", _("Wizardry"));
		struct Binding {
			const char *id, *label;
			KeyCode key;
			uint16 ascii;
			const char *keyboard, *controller;
		};
		static const Binding bindings[] = {
			{"UP", _s("Up"), KEYCODE_UP, 0, "UP", "JOY_UP"},
			{"DOWN", _s("Down"), KEYCODE_DOWN, 0, "DOWN", "JOY_DOWN"},
			{"LEFT", _s("Left"), KEYCODE_LEFT, 0, "LEFT", "JOY_LEFT"},
			{"RIGHT", _s("Right"), KEYCODE_RIGHT, 0, "RIGHT", "JOY_RIGHT"},
			{"CONFIRM", _s("Confirm"), KEYCODE_RETURN, ASCII_RETURN, "RETURN", "JOY_A"},
			{"BACK", _s("Escape"), KEYCODE_ESCAPE, ASCII_ESCAPE, "ESCAPE", "JOY_B"},
			{"MENU", _s("Game menu"), KEYCODE_F5, 0, "F5", "JOY_START"}};
		for (uint i = 0; i < ARRAYSIZE(bindings); ++i) {
			const Binding &b = bindings[i];
			Action *action = new Action(b.id, _(b.label));
			action->setKeyEvent(KeyState(b.key, b.ascii));
			action->addDefaultInputMapping(b.keyboard);
			action->addDefaultInputMapping(b.controller);
			if (i < 4)
				action->allowKbdRepeats();
			map->addAction(action);
		}
		return Keymap::arrayOf(map);
	}
	Common::Error createInstance(OSystem *system, Engine **engine,
								 const Wizardry::WizardryGameDescription *description) const override {
		if (description->gameNumber < 1 || description->gameNumber > 5)
			return Common::kUnsupportedGameidError;
		*engine = new Wizardry::WizardryEngine(system, description);
		return Common::kNoError;
	}
};

#if PLUGIN_ENABLED_DYNAMIC(WIZARDRY)
REGISTER_PLUGIN_DYNAMIC(WIZARDRY, PLUGIN_TYPE_ENGINE, WizardryMetaEngine);
#else
REGISTER_PLUGIN_STATIC(WIZARDRY, PLUGIN_TYPE_ENGINE, WizardryMetaEngine);
#endif
