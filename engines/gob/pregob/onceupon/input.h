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

#ifndef GOB_PREGOB_ONCEUPON_INPUT_H
#define GOB_PREGOB_ONCEUPON_INPUT_H

#include "backends/keymapper/keymap.h"
#include "common/events.h"

namespace Gob {
namespace OnceUpon {

enum InputContext { kPointerInput, kForestInput, kTextInput };
const Common::CustomEventType kSkipLevel = 1;

Common::KeymapArray createKeymaps();

/** Held controls and literal text input for Abracadabra and Baba Yaga. */
class KeyboardInput : public Common::EventObserver {
public:
	KeyboardInput();
	~KeyboardInput() override;
	void begin();
	void end();
	void setContext(InputContext context);
	static void clampActionPointer(int16 &x, int16 &y);
	static void moveActionPointer(int16 &x, int16 &y, uint32 heldKeys);
	void releaseActions();
	bool notifyEvent(const Common::Event &event) override;
	uint32 heldKeys() const;
	uint32 forestPress() const { return _forestPress; }
	void trackForestPress(bool track) { _trackForestPress = track; }
	void clearForestPress() { _forestPress = 0; }
	bool takeSkipLevel();
	bool takeFocusLost();
	void clearForestInput();
	uint8 confirmationKeys() const;

private:
	bool _pressed[Common::KEYCODE_LAST];
	uint _mapped[Common::KEYCODE_LAST];
	bool _registered;
	InputContext _context;
	uint32 _forestPress;
	bool _trackForestPress;
	bool _skipLevel;
	bool _focusLost;
	void applyContext();
	bool mappedControlsActive() const;
	static bool isControlKey(Common::KeyCode key);
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_INPUT_H
