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

#include "gob/pregob/mewilo/mewilo.h"

#include "common/util.h"

#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/util.h"
#include "gob/video.h"

namespace Gob {
namespace Mewilo {

void PreGob_Mewilo::mousePressed(int16 x, int16 y, MouseButtons buttons) {
	// Retain coordinates at DOWN, not the eventual cursor position at UP.
	// Collect only while a script input is active, not across music/delays.
	// Bound queued user input; keep older presses when saturated.
	if (!_acceptMousePresses || _pauseDepth || _mousePresses.size() >= 16)
		return;
	MewiloInputEvent event;
	event.x = x + _vm->_video->_scrollOffsetX - _vm->_video->_screenDeltaX;
	event.y = y + _vm->_video->_scrollOffsetY - _vm->_video->_screenDeltaY;
	event.mouseButtons = (uint8)buttons;
	_mousePresses.push_back(event);
}

uint16 PreGob_Mewilo::randomModulo(uint16 count) {
	if (!count)
		return 0;
	return (uint16)(_vm->_rnd.getRandomNumber(0x7FFFFFFF) % count);
}

byte PreGob_Mewilo::translateInputKey(int16 key) const {
	if (key >= 32 && key <= 0x7F)
		return (byte)key;
	// Gob's keyboard translator returns DOS CP850 for these Atari/French
	// characters.  Convert the subset proven by the native Mewilo input table.
	switch ((uint16)key) {
	case 0x82:
		return 0x16; // e acute
	case 0x8A:
		return 0x17; // e grave
	case 0x85:
		return 0x18; // a grave
	case 0x97:
		return 0x19; // u grave
	case 0x87:
		return 0x1A; // c cedilla
	default:
		return 0;
	}
}

bool PreGob_Mewilo::waitInput(MewiloInputEvent &event) {
	presentFrame();
	_presentation.finishStep();
	debugC(4, kDebugGameFlow, "MEWILO_PRESENT input module=%s t=%u frame=%u", _moduleStem.c_str(),
		activeMillis(), _presentation.frame());
	event = MewiloInputEvent();
	_acceptMousePresses = true;
	while (!shouldQuit() && !hasError() && !_pendingRestore) {
		int16 mouseX = 0, mouseY = 0;
		MouseButtons buttons = kMouseButtonsNone;
		int16 key = 0;
		if (_pendingKey) {
			key = _pendingKey;
			_pendingKey = 0;
		} else {
			endFrame(true);
			// Do not use PreGob::checkInput: it clears held state, which the
			// FF03 input-release wait still needs. Presses have their own queue.
			_vm->_util->getMouseState(&mouseX, &mouseY, &buttons);
			key = _vm->_util->checkKey();
		}
		if (shouldQuit() || _pendingRestore)
			break;
		// Held-button state is not a press event: it both loses short clicks
		// and can repeat held ones. The shared event owner supplied each DOWN.
		buttons = kMouseButtonsNone;
		if (!key && !_mousePresses.empty()) {
			event = _mousePresses[0];
			_mousePresses.remove_at(0);
			_acceptMousePresses = false;
			return true;
		}
		if (!key && buttons == kMouseButtonsNone)
			continue;

		event.x = mouseX;
		event.y = mouseY;
		event.mouseButtons = (uint8)buttons;
		switch (key) {
		case kKeySpace:
			event.key = ' ';
			break;
		case kKeyEscape:
			event.key = kMewiloKeyEscape;
			break;
		case kKeyReturn:
			event.key = kMewiloKeyReturn;
			break;
		case kKeyBackspace:
			event.key = kMewiloKeyBackspace;
			break;
		case kKeyDelete:
			event.key = kMewiloKeyDelete;
			break;
		default:
			event.key = translateInputKey(key);
			break;
		}
		_acceptMousePresses = false;
		return true;
	}
	_acceptMousePresses = false;
	return false;
}

void PreGob_Mewilo::drawInputField(
	const MewiloTextField &field, const Common::Array<byte> &text, bool cursor) {
	if (!_vm->_draw->_backSurface) {
		renderFailure("missing input surface", 0);
		return;
	}
	const int16 x = field.xCell * 8;
	const int16 y = field.y;
	const int16 width = MAX<int16>(8, field.maxChars * 8);
	const int16 height = MAX<int16>(8, field.serializedHeight);
	_vm->_draw->_backSurface->fillRect(x, y, x + width - 1, y + height - 1, field.colorBase);
	for (uint i = 0; i < text.size() && i < field.maxChars; ++i)
		_font.drawChar(*_vm->_draw->_backSurface, text[i], x + i * 8, y, field.colorInput);
	if (cursor && text.size() < field.maxChars) {
		const int16 cx = x + text.size() * 8;
		const int16 cy = y + MIN<int16>(7, MAX<int16>(0, field.lineAdvance - 2));
		_vm->_draw->_backSurface->drawLine(cx, cy, cx + 7, cy, field.colorAlt);
	}
	markDirty(x, y, width, height);
}

bool PreGob_Mewilo::editTextField(const MewiloTextField &field, Common::Array<byte> &text) {
	text.clear();
	drawInputField(field, text, true);
	while (!shouldQuit() && !hasError() && !_pendingRestore) {
		MewiloInputEvent event;
		if (!waitInput(event))
			return false;
		if (event.key == kMewiloKeyEscape) {
			drawInputField(field, text, false);
			return false;
		}
		if (event.key == kMewiloKeyReturn) {
			drawInputField(field, text, false);
			return true;
		}
		if (event.key == kMewiloKeyBackspace || event.key == kMewiloKeyDelete) {
			if (!text.empty())
				text.pop_back();
			drawInputField(field, text, true);
			continue;
		}
		if (event.key > 0 && event.key < 0x100 && text.size() < field.maxChars) {
			text.push_back((byte)event.key);
			drawInputField(field, text, true);
		}
	}
	return false;
}

void PreGob_Mewilo::showRejectMessage(const byte *text, uint length) {
	if (!text || !length)
		return;
	if (!_vm->_draw->_backSurface) {
		renderFailure("missing rejection-message surface", 0);
		return;
	}
	const int16 y = 192;
	_vm->_draw->_backSurface->fillRect(0, y, 319, 199, 0);
	drawByteString(text, length, 0, y, 1, 8);
	markDirty(0, y, 320, 8);
	MewiloTimingPlan plan;
	plan.anchor = kMewiloVisibleHold;
	plan.rule = "reject-message";
	plan.region = Common::Rect(0, y, 320, 200);
	plan.milliseconds = _normalizeTiming ? mewiloReadingMillis(text, length) : 250;
	waitPresentation(plan, 0);
}

} // End of namespace Mewilo
} // End of namespace Gob
