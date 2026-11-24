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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#ifndef GOB_PREGOB_ONCEUPON_FOREST_ACTOR_H
#define GOB_PREGOB_ONCEUPON_FOREST_ACTOR_H

#include "common/rect.h"

namespace Gob {

class ANIFile;

namespace OnceUpon {

class Forest;

/** Animation and collision state used by the three forest sections. */
class ForestActor {
public:
	enum Status {
		kActive = 0,
		kHidden = 1,
		kErase = 3
	};

	struct State {
		int16 x, y;
		int16 animation, frame, nextAnimation;
		int16 direction;
		// Native aligned bounds from the previous draw, with room-entry adjustments.
		Common::Rect lastDrawnArea;
		Status status;
		byte delay, delayCounter;
		bool frozen, redraw;
	};

	ForestActor();
	void bind(const ANIFile &ani, bool player = false);
	void place(int16 x, int16 bottom, int16 animation, int16 frame = 0);
	void setAnimation(int16 animation);
	void advance();
	void stepPlayer(uint32 heldKeys);

	bool lastFrame() const;
	bool canChangeAction() const;
	bool overlaps(const ForestActor &other) const;
	bool wandReaches(const ForestActor &other) const;
	Common::Rect frameArea() const;

private:
	friend class Forest;
	static const int16 kPlayerAnimations[60];
	const ANIFile *_ani;
	bool _player;
	State _state;

	uint16 mappedAnimation() const;
	int16 groundedY(int16 animation, int16 frame, int16 bottom) const;
	void requestAction(int16 animation);
	enum Action {
		kStop, kWalkLeft, kWalkRight, kJumpLeft, kJumpRight, kJumpFacing,
		kQueueJump, kCrouchLeft, kCrouchRight, kCrouchFacing,
		kCastLeft, kCastRight, kCastFacing
	};
	static Action decodeInput(uint32 heldKeys);
	void applyAction(Action action);
	bool keepsPosition() const;
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_FOREST_ACTOR_H
