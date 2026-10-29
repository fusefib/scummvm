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

#include "gob/pregob/emmanuelle/input_keymaps.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/standard-actions.h"
#include "common/events.h"
#include "common/keyboard.h"
#include "common/translation.h"

namespace Gob {
namespace Emmanuelle {

enum EmmanuelleFightAction {
	kEmmanuelleFightBack = 0,
	kEmmanuelleFightForward,
	kEmmanuelleFightFirstDistraction,
	kEmmanuelleFightStrike,
	kEmmanuelleFightSecondDistraction,
	kEmmanuelleFightDodgeBlow,
	kEmmanuelleFightDodgePunch,
	kEmmanuelleFightDodgeHeadbutt,
	kEmmanuelleFightActionCount
};

static Common::Action *addKeyAction(Common::Keymap *keymap,
									 const char *id,
									 const Common::U32String &description,
									 Common::KeyCode keycode, uint16 ascii,
									 const char *keyboard,
									 const char *controller) {
	Common::Action *action = new Common::Action(id, description);
	action->setKeyEvent(Common::KeyState(keycode, ascii));
	if (keyboard)
		action->addDefaultInputMapping(keyboard);
	if (controller)
		action->addDefaultInputMapping(controller);
	keymap->addAction(action);
	return action;
}

static void addFightAction(Common::Keymap *keymap,
		const Common::U32String &description, EmmanuelleFightAction fightAction) {
	static const struct {
		const char *id;
		Common::KeyCode key;
		uint16 ascii;
		const char *keyboard;
		const char *controller;
		const char *secondary;
	} bindings[] = {
		{"EMMA_FIGHT_BACK", Common::KEYCODE_LEFT, '-', "LEFT", "JOY_LEFT", "x"},
		{"EMMA_FIGHT_FORWARD", Common::KEYCODE_RIGHT, '.', "RIGHT", "JOY_RIGHT", "c"},
		{"EMMA_FIGHT_ATTACK1", Common::KEYCODE_7, 'i', "7", "JOY_X", "KP7"},
		{"EMMA_FIGHT_ATTACK2", Common::KEYCODE_8, 'm', "8", "JOY_A", "KP8"},
		{"EMMA_FIGHT_ATTACK3", Common::KEYCODE_9, 'q', "9", "JOY_Y", "KP9"},
		{"EMMA_FIGHT_DEFEND1", Common::KEYCODE_1, 'g', "1", "JOY_LEFT_SHOULDER", "KP1"},
		{"EMMA_FIGHT_DEFEND2", Common::KEYCODE_2, 'k', "2", "JOY_DOWN", "KP2"},
		{"EMMA_FIGHT_DEFEND3", Common::KEYCODE_3, 'o', "3", "JOY_RIGHT_SHOULDER", "KP3"}
	};
	Common::Action *action = addKeyAction(keymap, bindings[fightAction].id, description,
		bindings[fightAction].key, bindings[fightAction].ascii,
		bindings[fightAction].keyboard, bindings[fightAction].controller);
	action->addDefaultInputMapping(bindings[fightAction].secondary);
}

Common::KeymapArray createEmmanuelleKeymaps(Common::Platform platform) {
	using namespace Common;

	Keymap *common = new Keymap(Keymap::kKeymapTypeGame,
		"emmanuelle-common", _("Default game keymappings"));
	Keymap *intro = new Keymap(Keymap::kKeymapTypeGame,
		emmanuelleInputKeymapId(kEmmanuelleInputIntro),
		_("Intro keymappings"));
	Keymap *choice = new Keymap(Keymap::kKeymapTypeGame,
		emmanuelleInputKeymapId(kEmmanuelleInputChoice),
		_("Choice selection keymappings"));
	Keymap *continuation = new Keymap(Keymap::kKeymapTypeGame,
		emmanuelleInputKeymapId(kEmmanuelleInputContinue),
		_("Continue keymappings"));
	Keymap *fight = new Keymap(Keymap::kKeymapTypeGame,
		emmanuelleInputKeymapId(kEmmanuelleInputFight),
		_("Fight keymappings"));
	common->setPartialMatchAllowed(false);
	intro->setPartialMatchAllowed(false);
	choice->setPartialMatchAllowed(false);
	continuation->setPartialMatchAllowed(false);
	fight->setPartialMatchAllowed(false);
	intro->setEnabled(false);
	choice->setEnabled(false);
	continuation->setEnabled(false);
	fight->setEnabled(false);

	Action *action = new Action(kStandardActionLeftClick, _("Left click"));
	action->setLeftClickEvent();
	action->addDefaultInputMapping("MOUSE_LEFT");
	common->addAction(action);
	action = new Action(kStandardActionRightClick, _("Right click"));
	action->setRightClickEvent();
	action->addDefaultInputMapping("MOUSE_RIGHT");
	common->addAction(action);
	action = addKeyAction(common, "EMMA_RETURN", _("Continue"), KEYCODE_RETURN,
		ASCII_RETURN, "RETURN", nullptr);
	action->addDefaultInputMapping("KP_ENTER");
	action->addDefaultInputMapping("MOUSE_MIDDLE");
	addKeyAction(common, "EMMA_ESCAPE", _("Cancel"), KEYCODE_ESCAPE,
		ASCII_ESCAPE, "ESCAPE", "JOY_B");
	action = new Action(kStandardActionOpenMainMenu, _("Game menu"));
	action->setCustomEngineActionEvent(
		(Common::CustomEventType)kEmmanuelleActionOpenMainMenu);
	action->addDefaultInputMapping("F5");
	action->addDefaultInputMapping("C+F5");
	action->addDefaultInputMapping("JOY_START");
	common->addAction(action);

	action = new Action(kStandardActionSkip, _("Skip intro"));
	action->setCustomEngineActionEvent(
		(Common::CustomEventType)kEmmanuelleActionSkipIntro);
	action->addDefaultInputMapping("SPACE");
	action->addDefaultInputMapping("JOY_BACK");
	intro->addAction(action);

	action = new Action("EMMA_SELECT", _("Select choice"));
	action->setEvent(EVENT_LBUTTONDOWN);
	action->addDefaultInputMapping("JOY_A");
	choice->addAction(action);
	if (platform != kPlatformAmiga) {
		addKeyAction(choice, "EMMA_CURSOR_UP", _("Move cursor up"),
			KEYCODE_UP, 0, "UP", "JOY_UP");
		addKeyAction(choice, "EMMA_CURSOR_DOWN", _("Move cursor down"),
			KEYCODE_DOWN, 0, "DOWN", "JOY_DOWN");
		addKeyAction(choice, "EMMA_CURSOR_LEFT", _("Move cursor left"),
			KEYCODE_LEFT, 0, "LEFT", "JOY_LEFT");
		addKeyAction(choice, "EMMA_CURSOR_RIGHT", _("Move cursor right"),
			KEYCODE_RIGHT, 0, "RIGHT", "JOY_RIGHT");
	}

	addKeyAction(continuation, "EMMA_CONTINUE", _("Continue"),
		KEYCODE_RETURN, ASCII_RETURN, nullptr,
		"JOY_A");

	// Native movement was X/C on DOS, Up/Down on Atari, and top-row 8/9 on
	// Amiga. Their action columns were keypad 9/6/3 and dodge columns 7/4/1.
	// Keep the shared fight loop's ASCII tokens when remapping these actions.
	addFightAction(fight, _("Step backward"),
		kEmmanuelleFightBack);
	addFightAction(fight, _("Step forward"),
		kEmmanuelleFightForward);
	addFightAction(fight,
		_("First distraction"),
		kEmmanuelleFightFirstDistraction);
	addFightAction(fight, _("Strike opponent"),
		kEmmanuelleFightStrike);
	addFightAction(fight,
		_("Second distraction"),
		kEmmanuelleFightSecondDistraction);
	addFightAction(fight, _("Dodge blow"),
		kEmmanuelleFightDodgeBlow);
	addFightAction(fight, _("Dodge punch"),
		kEmmanuelleFightDodgePunch);
	addFightAction(fight,
		_("Dodge headbutt"),
		kEmmanuelleFightDodgeHeadbutt);

	KeymapArray keymaps;
	keymaps.push_back(common);
	keymaps.push_back(intro);
	keymaps.push_back(choice);
	keymaps.push_back(continuation);
	keymaps.push_back(fight);
	return keymaps;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
