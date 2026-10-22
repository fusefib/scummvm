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

#ifndef GOB_PREGOB_EMMANUELLE_INPUT_KEYMAPS_H
#define GOB_PREGOB_EMMANUELLE_INPUT_KEYMAPS_H

#include "backends/keymapper/keymap.h"
#include "common/platform.h"

namespace Gob {
namespace Emmanuelle {

enum EmmanuelleInputContext {
	kEmmanuelleInputNone = 0,
	kEmmanuelleInputIntro,
	kEmmanuelleInputChoice,
	kEmmanuelleInputContinue,
	kEmmanuelleInputFight
};

enum EmmanuelleCustomAction {
	/** Skip the intro up to INIT's first choice, leaving protection active. */
	kEmmanuelleActionSkipIntro = 1,
	/** Open the game menu after the executor suspends at an input wait. */
	kEmmanuelleActionOpenMainMenu
};

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

inline const char *emmanuelleCommonKeymapId() {
	return "emmanuelle-common";
}

inline const char *emmanuelleInputKeymapId(
		EmmanuelleInputContext context) {
	switch (context) {
	case kEmmanuelleInputIntro:
		return "emmanuelle-intro";
	case kEmmanuelleInputChoice:
		return "emmanuelle-choice";
	case kEmmanuelleInputContinue:
		return "emmanuelle-continue";
	case kEmmanuelleInputFight:
		return "emmanuelle-fight";
	default:
		return nullptr;
	}
}

inline const char *emmanuelleSkipIntroKeyboardMapping() {
	return "SPACE";
}

inline const char *emmanuelleSkipIntroControllerMapping() {
	return "JOY_BACK";
}

inline bool emmanuelleHasKeyboardCursor(Common::Platform platform) {
	return true;
}

inline Common::EventType emmanuelleChoiceEventType() {
	return Common::EVENT_LBUTTONDOWN;
}

inline Common::KeyCode emmanuelleContinuationKeyCode() {
	return Common::KEYCODE_RETURN;
}

inline const char *emmanuelleChoiceControllerMapping() {
	return "JOY_A";
}

inline const char *emmanuelleContinuationControllerMapping() {
	return "JOY_A";
}

inline const char *emmanuelleEscapeControllerMapping() {
	return "JOY_B";
}

inline uint16 emmanuelleFightActionAscii(EmmanuelleFightAction action) {
	switch (action) {
	case kEmmanuelleFightBack:
		return '-';
	case kEmmanuelleFightForward:
		return '.';
	case kEmmanuelleFightFirstDistraction:
		return 'i';
	case kEmmanuelleFightStrike:
		return 'm';
	case kEmmanuelleFightSecondDistraction:
		return 'q';
	case kEmmanuelleFightDodgeBlow:
		return 'g';
	case kEmmanuelleFightDodgePunch:
		return 'k';
	case kEmmanuelleFightDodgeHeadbutt:
		return 'o';
	default:
		return 0;
	}
}

inline const char *emmanuelleFightActionId(EmmanuelleFightAction action) {
	const char *const ids[kEmmanuelleFightActionCount] = {
		"EMMA_FIGHT_BACK", "EMMA_FIGHT_FORWARD",
		"EMMA_FIGHT_ATTACK1", "EMMA_FIGHT_ATTACK2",
		"EMMA_FIGHT_ATTACK3", "EMMA_FIGHT_DEFEND1",
		"EMMA_FIGHT_DEFEND2", "EMMA_FIGHT_DEFEND3"
	};
	if (action >= kEmmanuelleFightActionCount)
		return nullptr;
	return ids[action];
}

inline Common::KeyCode emmanuelleFightActionKeyCode(
		EmmanuelleFightAction action) {
	if (action >= kEmmanuelleFightActionCount)
		return Common::KEYCODE_INVALID;
	const Common::KeyCode keys[kEmmanuelleFightActionCount] = {
		Common::KEYCODE_LEFT, Common::KEYCODE_RIGHT,
		Common::KEYCODE_7, Common::KEYCODE_8, Common::KEYCODE_9,
		Common::KEYCODE_1, Common::KEYCODE_2, Common::KEYCODE_3
	};
	return keys[action];
}

inline const char *emmanuelleFightKeyboardMapping(
		EmmanuelleFightAction action) {
	if (action >= kEmmanuelleFightActionCount)
		return nullptr;
	const char *const keys[kEmmanuelleFightActionCount] = {
		"LEFT", "RIGHT", "7", "8", "9", "1", "2", "3"
	};
	return keys[action];
}

inline const char *emmanuelleFightSecondaryKeyboardMapping(
		EmmanuelleFightAction action) {
	if (action >= kEmmanuelleFightActionCount)
		return nullptr;
	const char *const keys[kEmmanuelleFightActionCount] = {
		"x", "c", "KP7", "KP8", "KP9", "KP1", "KP2", "KP3"
	};
	return keys[action];
}

inline const char *emmanuelleFightControllerMapping(
		EmmanuelleFightAction action) {
	switch (action) {
	case kEmmanuelleFightBack:
		return "JOY_LEFT";
	case kEmmanuelleFightForward:
		return "JOY_RIGHT";
	case kEmmanuelleFightFirstDistraction:
		return "JOY_X";
	case kEmmanuelleFightStrike:
		return "JOY_A";
	case kEmmanuelleFightSecondDistraction:
		return "JOY_Y";
	case kEmmanuelleFightDodgeBlow:
		return "JOY_LEFT_SHOULDER";
	case kEmmanuelleFightDodgePunch:
		return "JOY_DOWN";
	case kEmmanuelleFightDodgeHeadbutt:
		return "JOY_RIGHT_SHOULDER";
	default:
		return nullptr;
	}
}

Common::KeymapArray createEmmanuelleKeymaps(Common::Platform platform);

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_INPUT_KEYMAPS_H
