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

#include "gob/anifile.h"
#include "gob/pregob/onceupon/forest_actor.h"

namespace Gob {
namespace OnceUpon {

const int16 ForestActor::kPlayerAnimations[60] = {
	0, 2, 0, 3, 1, 0, 0, 0, 14, 14, 6, 6, 0, 7, 7, 7, 0, 6, 4, 5,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 12, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0
};

ForestActor::ForestActor() : _ani(nullptr), _player(false) {
	_state.x = _state.y = 0;
	_state.animation = -1;
	_state.frame = 0;
	_state.nextAnimation = 8;
	_state.direction = 4;
	_state.status = kHidden;
	_state.delay = _state.delayCounter = 1;
	_state.frozen = false;
	_state.redraw = true;
}

void ForestActor::bind(const ANIFile &ani, bool player) {
	_ani = &ani;
	_player = player;
}

uint16 ForestActor::mappedAnimation() const {
	assert(_ani && _state.animation >= 0);
	return _player ? kPlayerAnimations[_state.animation] : _state.animation;
}

int16 ForestActor::groundedY(int16 animation, int16 frame, int16 bottom) const {
	uint16 mapped = _player ? kPlayerAnimations[animation] : animation;
	int16 x = 0, y = 0;
	_ani->getFrameOrigin(mapped, frame, x, y);
	return bottom - (_ani->getAnimationInfo(mapped).frameAreas[frame].bottom - y);
}

void ForestActor::place(int16 x, int16 bottom, int16 animation, int16 frame) {
	_state.x = x;
	_state.animation = animation;
	_state.frame = frame;
	_state.y = groundedY(animation, frame, bottom);
	_state.lastDrawnArea = Common::Rect(x, _state.y, x + 1, _state.y + 1);
	_state.delayCounter = 1;
	_state.frozen = false;
	_state.redraw = true;
}

void ForestActor::setAnimation(int16 animation) {
	_state.animation = animation;
	_state.frame = 0;
	_state.y = groundedY(animation, 0, 140);
}

bool ForestActor::lastFrame() const {
	return _state.frame == _ani->getAnimationInfo(mappedAnimation()).frameCount - 1;
}

bool ForestActor::canChangeAction() const {
	return _state.animation == 0 || _state.animation == 4 ||
		_state.animation == 8 || _state.animation == 9 ||
		((_state.animation == 1 || _state.animation == 3) && lastFrame());
}

void ForestActor::requestAction(int16 animation) {
	if (!canChangeAction())
		return;
	_state.x = _state.lastDrawnArea.left;
	setAnimation(animation);
}

ForestActor::Action ForestActor::decodeInput(uint32 heldKeys) {
	// Later directions take precedence in the native held-key scan.
	Action action = kStop;
	if (heldKeys & 0x8000) action = kJumpLeft;
	if (heldKeys & 0x0001) action = kJumpFacing;
	if (heldKeys & 0x10000) action = kJumpRight;
	if (heldKeys & 0x0008) action = kWalkLeft;
	if (heldKeys & 0x20000) action = kCrouchLeft;
	if (heldKeys & 0x0004) action = kWalkRight;
	if (heldKeys & 0x0002) action = kCrouchFacing;
	if (heldKeys & 0x40000) action = kCrouchRight;
	if (heldKeys & 0x0080)
		return kStop;
	if (!(heldKeys & 0x0020))
		return action;
	switch (action) {
	case kWalkLeft: case kJumpLeft: case kCrouchLeft: return kCastLeft;
	case kWalkRight: case kJumpRight: case kCrouchRight: return kCastRight;
	case kJumpFacing: return kQueueJump;
	case kStop: return kCastFacing;
	default: return kStop;
	}
}

void ForestActor::applyAction(Action action) {
	switch (action) {
	case kJumpLeft:
		requestAction(1);
		break;
	case kJumpRight:
		requestAction(3);
		break;
	case kCrouchLeft:
		requestAction(18);
		break;
	case kCrouchRight:
		requestAction(19);
		break;
	case kWalkLeft:
	case kWalkRight: {
		int16 animation = action == kWalkLeft ? 0 : 4;
		if (!canChangeAction() || _state.animation == animation)
			_state.nextAnimation = animation;
		else {
			_state.direction = 0;
			requestAction(animation);
		}
		break;
	}
	case kQueueJump:
		_state.nextAnimation = _state.direction == 0 ? 1 : 3;
		break;
	case kJumpFacing:
		requestAction(_state.direction == 0 ? 1 : 3);
		break;
	case kCrouchFacing:
		requestAction(_state.direction == 0 ? 18 : 19);
		break;
	case kCastFacing:
		requestAction(_state.direction == 0 ? 10 : 14);
		break;
	case kCastLeft:
		requestAction(10);
		break;
	case kCastRight:
		requestAction(14);
		break;
	case kStop:
		if (_state.animation == 0 || _state.animation == 4) {
			_state.x = _state.lastDrawnArea.left;
			_state.nextAnimation = _state.animation == 0 ? 8 : 9;
			setAnimation(_state.nextAnimation);
		} else
			_state.nextAnimation = _state.direction == 0 ? 8 : 9;
		break;
	}
}

bool ForestActor::keepsPosition() const {
	return _state.animation == 8 || _state.animation == 9 ||
		_state.animation == 18 || _state.animation == 19 ||
		(_state.animation >= 10 && _state.animation <= 17);
}

void ForestActor::stepPlayer(uint32 heldKeys) {
	assert(_player);
	// Native advancement samples the old animation before new input can replace it.
	const uint16 count = _ani->getAnimationInfo(mappedAnimation()).frameCount;
	applyAction(decodeInput(heldKeys));
	++_state.frame;
	switch (_state.animation) {
	case 0: case 1: case 7: case 10: case 11: case 17:
		_state.direction = 0;
		break;
	case 3: case 4: case 5: case 13: case 14: case 15:
		_state.direction = 4;
		break;
	}
	if ((_state.animation == 8 || _state.animation == 9) && _state.nextAnimation != _state.animation) {
		setAnimation(_state.nextAnimation);
	} else if (_state.frame == count) {
		const ANIFile::Animation &info = _ani->getAnimationInfo(mappedAnimation());
		int16 x = _state.x, y = _state.y;
		if (!keepsPosition()) {
			x = info.deltaX || info.deltaY ? x + info.deltaX : _state.lastDrawnArea.left;
			y = info.deltaX || info.deltaY ? y + info.deltaY : _state.lastDrawnArea.top;
		}
		bool change = _state.animation != _state.nextAnimation;
		setAnimation(_state.nextAnimation);
		_state.x = x;
		if (!change)
			_state.y = y;
	}
}

void ForestActor::advance() {
	if (_player || _state.status == kHidden)
		return;
	_state.redraw = _state.frozen;
	if (_state.frozen)
		return;
	if (_state.delayCounter < _state.delay)
		++_state.delayCounter;
	if (_state.delayCounter != _state.delay)
		return;
	_state.redraw = true;
	_state.delayCounter = 1;
	if (++_state.frame == _ani->getAnimationInfo(mappedAnimation()).frameCount) {
		_state.frame = 0;
		const ANIFile::Animation &info = _ani->getAnimationInfo(mappedAnimation());
		_state.x += info.deltaX;
		// PAPILHBG29 hovers in place. Its one-pixel loop offset eventually
		// sends it below the forest and erases a strip of the HUD.
		// Affected areas: room numbers follow play order.
		// Abracadabra DOS/ST: Ch. 2 room 2 (Advanced); Ch. 6 room 10
		// (Beginner), rooms 7, 9, 10 (Intermediate), and room 9 (Advanced).
		// Baba Yaga, all platforms: Ch. 2 room 6 and Ch. 6 room 7 (Intermediate).
		if (info.deltaY && info.name != "PAPILHBG29")
			_state.y += info.deltaY;
	}
}

Common::Rect ForestActor::frameArea() const {
	const ANIFile::FrameArea &area = _ani->getAnimationInfo(mappedAnimation()).frameAreas[_state.frame];
	return Common::Rect(_state.x + area.left, _state.y + area.top,
		_state.x + area.right + 1, _state.y + area.bottom + 1);
}

bool ForestActor::overlaps(const ForestActor &other) const {
	if (_state.status != kActive || other._state.status != kActive)
		return false;
	const Common::Rect &a = _state.lastDrawnArea, &b = other._state.lastDrawnArea;
	return a.left <= b.right - 11 && a.right - 1 >= b.left + 10 &&
		a.top <= b.bottom - 6 && a.bottom - 1 >= b.top + 5;
}

bool ForestActor::wandReaches(const ForestActor &other) const {
	if (_state.status != kActive || other._state.status != kActive)
		return false;
	const Common::Rect &a = _state.lastDrawnArea, &b = other._state.lastDrawnArea;
	int16 left = _state.direction == 4 ? a.left + 50 : a.right - 71;
	int16 right = left + 20;
	return b.left < right && left < b.right - 1 &&
		b.top < a.top + 25 && a.top + 15 < b.bottom - 1;
}

} // End of namespace OnceUpon
} // End of namespace Gob
