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

#include "gob/surface.h"
#include "gob/anifile.h"

#include "gob/pregob/onceupon/character_preview.h"

namespace Gob {
namespace OnceUpon {

namespace {

enum {
	kAnimWalkLeft  =  0,
	kAnimWalkRight =  1,
	kAnimJumpLeft  =  2,
	kAnimJumpRight =  3,
	kAnimTapFoot   = 14
};

} // End of anonymous namespace

CharGenChild::CharGenChild(const ANIFile &ani) : ANIObject(ani) {
	setPosition(260, 110);
	setAnimation(kAnimWalkLeft);
	setVisible(true);
	setPause(false);
}

CharGenChild::~CharGenChild() {
}

void CharGenChild::advance() {
	int16 x, y;
	getPosition(x, y);
	// Native transitions test the cycle's base position before its delta.
	if (lastFrame()) {
		int animation = -1;
		switch (getAnimation()) {
		case kAnimWalkLeft:
			if (x < 220) {
				animation = kAnimWalkRight;
				x = 160;
				y = 110;
			}
			break;
		case kAnimWalkRight:
			if (x > 200) {
				animation = kAnimJumpLeft;
				x = 260;
				y = 87;
			}
			break;
		case kAnimJumpLeft:
			animation = kAnimTapFoot;
			x = 155;
			y = 87;
			break;
		case kAnimTapFoot:
			animation = kAnimJumpRight;
			x = 155;
			y = 87;
			break;
		case kAnimJumpRight:
			animation = kAnimWalkLeft;
			x = 260;
			y = 110;
			break;
		default:
			break;
		}
		if (animation >= 0) {
			setAnimation(animation);
			setPosition(x, y);
			return;
		}
	}
	ANIObject::advance();
}

CharGenChild::Sound CharGenChild::shouldPlaySound() const {
	const uint16 anim  = getAnimation();
	const uint16 frame = getFrame();

	if (((anim == kAnimWalkLeft) || (anim == kAnimWalkRight)) && ((frame == 1) || (frame == 6)))
		return kSoundWalk;

	if (((anim == kAnimJumpLeft) || (anim == kAnimJumpRight)) &&  (frame == 0))
		return kSoundJump;

	return kSoundNone;
}

} // End of namespace OnceUpon

} // End of namespace Gob
