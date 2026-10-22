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

#include "gob/pregob/emmanuelle/fight.h"

namespace Gob {
namespace Emmanuelle {

static const byte kLeftMax[15] =
	{0, 1, 4, 4, 3, 3, 3, 1, 1, 1, 3, 3, 3, 9, 9};
static const byte kRightMax[15] =
	{0, 1, 4, 4, 3, 3, 3, 1, 1, 1, 6, 3, 6, 9, 9};

static const byte kLeftSprite[14][9] = {
	{3, 3, 1, 1, 1, 6, 3, 6, 9},
	{9, 5, 0, 0, 0, 0, 0, 0, 0},
	{0, 2, 3, 4, 1, 0, 0, 0, 0},
	{0, 4, 3, 2, 1, 0, 0, 0, 0},
	{0, 6, 7, 7, 0, 0, 0, 0, 0},
	{0, 8, 9, 5, 0, 0, 0, 0, 0},
	{0, 10, 11, 5, 0, 0, 0, 0},
	{0, 12, 0, 0, 0, 0, 0, 0, 0},
	{0, 12, 0, 0, 0, 0, 0, 0, 0},
	{0, 12, 0, 0, 0, 0, 0, 0, 0},
	{0, 15, 15, 15, 15, 0, 0, 0, 0},
	{0, 16, 16, 16, 16, 0, 0, 0, 0},
	{0, 17, 18, 18, 18, 18, 18, 18, 18},
	{18, 19, 20, 20, 20, 20, 20, 20, 20}
};

static const byte kRightSprite[14][9] = {
	{20, 13, 14, 13, 14, 13, 14, 13, 14},
	{13, 5, 0, 0, 0, 0, 0, 0, 0},
	{0, 2, 1, 3, 4, 0, 0, 0, 0},
	{0, 3, 1, 2, 4, 0, 0, 0, 0},
	{0, 6, 7, 6, 0, 0, 0, 0, 0},
	{0, 10, 11, 10, 0, 0, 0, 0, 0},
	{0, 12, 13, 12, 0, 0, 0, 0, 0},
	{0, 20, 0, 0, 0, 0, 0, 0, 0},
	{0, 10, 0, 0, 0, 0, 0, 0, 0},
	{0, 10, 0, 0, 0, 0, 0, 0, 0},
	{0, 14, 14, 14, 14, 14, 14, 14, 14},
	{14, 16, 16, 16, 16, 0, 0, 0, 0},
	{0, 15, 15, 15, 15, 15, 15, 15, 15},
	{15, 17, 18, 17, 18, 17, 18, 17, 18}
};

byte fightFrameMax(bool rightActor, byte action) {
	return action < 15 ? (rightActor ? kRightMax[action] : kLeftMax[action]) : 0;
}

byte fightSpriteId(bool rightActor, byte action, byte frame) {
	if (action > 14)
		return 0;
	if (action == 14) {
		if (frame < 1 || frame > 9)
			return 0;
		if (rightActor)
			return (frame & 1) ? 19 : 20;
		return (frame & 1) ? 13 : 14;
	}
	if (frame < 9)
		return rightActor ? kRightSprite[action][frame] : kLeftSprite[action][frame];
	// 0x7995 treats both tables as flat arrays. Action13/frame9 crosses the
	// nominal row boundary and reads adjacent executable data.
	if (action == 13 && frame == 9)
		return rightActor ? 17 : 20;
	return 0;
}

byte fightRecupIndex(bool rightActor, byte action, byte frame) {
	const byte id = fightSpriteId(rightActor, action, frame);
	if (id < 1 || id > 20)
		return 0xFF;
	return rightActor ? (byte)(20 + id - 1) : (byte)(id - 1);
}

bool fightPlacement(bool rightActor, byte action, byte frame, int16 anchorX,
		int16 spriteWidth, int16 spriteHeight, FightActorPlacement &out) {
	const byte id = fightSpriteId(rightActor, action, frame);
	if (id < 1 || id > 20 || spriteWidth <= 0 || spriteHeight <= 0)
		return false;
	out.spriteId = id;
	out.recupIndex = rightActor ? (byte)(20 + id - 1) : (byte)(id - 1);
	out.cellX = anchorX - 16;
	out.cellY = 70;
	out.cellWidth = 72;
	out.cellHeight = 80;
	out.x = anchorX + 20 - spriteWidth / 2;
	out.y = (int16)((150 - spriteHeight) & ~1);
	return true;
}

FightState::FightState() {
	leftMeter = 0;
	rightMeter = 0;

	damageToLeft = 5;
	damageToRight = 5;
	pendingLeftDamage = 0;
	pendingRightDamage = 0;

	ended = false;
	result = 0;
}

void initFightState(FightState &state, uint16 leftMeterBonus, uint16 rightMeterBonus,
		uint16 leftDamageOption, uint16 rightDamageOption, uint16 leftBaseMeter, uint16 rightBaseMeter) {
	state = FightState();
	state.leftMeter = (int16)(leftBaseMeter + leftMeterBonus > 80 ? 80 : leftBaseMeter + leftMeterBonus);
	state.rightMeter = (int16)(rightBaseMeter + rightMeterBonus > 80 ? 80 : rightBaseMeter + rightMeterBonus);
	state.damageToLeft = leftDamageOption == 1 ? 10 : 5;
	state.damageToRight = rightDamageOption == 1 ? 10 : 5;
	state.left.anchorX = 96;
	state.right.anchorX = 256;
	state.left.action = 1;
	state.left.frame = 1;
	state.left.nextAction = 1;
	state.left.nextFrame = 1;
	state.right.action = 2;
	state.right.frame = 2;
	state.right.nextAction = 1;
	state.right.nextFrame = 1;
}

static int16 actorDistance(const FightState &state) {
	return state.right.anchorX - state.left.anchorX;
}

static void advanceActorFrame(FightActor &actor, byte frameMax) {
	if (actor.frame < frameMax)
		actor.nextFrame = actor.frame + 1;
	else {
		actor.nextAction = 1;
		actor.nextFrame = 1;
	}
}

static void setReaction(FightActor &actor, byte action) {
	// The native actor handlers write a hit reaction to both the current and
	// next actor state. The second actor handler can therefore advance a
	// reaction installed by the first handler during the same combat tick.
	actor.action = action;
	actor.frame = 1;
	actor.nextAction = action;
	actor.nextFrame = 1;
}

void applyFightLeftAction(FightState &state) {
	FightActor &actor = state.left;
	const byte action = actor.action;
	const byte frame = actor.frame;
	const byte frameMax = fightFrameMax(false, action);
	if (action < 1 || action > 13)
		return;
	actor.nextAction = action;
	actor.nextFrame = frame;
	if (action == 1) {
		actor.nextAction = actor.nextFrame = 1;
		return;
	}
	if (action == 2) {
		if (frame < frameMax) {
			actor.nextFrame = frame + 1;
			if (actorDistance(state) > 24)
				actor.anchorX += 8;
		} else
			actor.nextAction = actor.nextFrame = 1;
		return;
	}
	if (action == 3) {
		if (frame < frameMax) {
			actor.nextFrame = frame + 1;
			if (actor.anchorX > 50)
				actor.anchorX -= 8;
		} else
			actor.nextAction = actor.nextFrame = 1;
		return;
	}
	if (action >= 4 && action <= 6) {
		if (frame < frameMax) {
			actor.nextFrame = frame + 1;
			const byte nextFrame = actor.nextFrame;
			// Defense actions 7-9 block their corresponding attacks 4-6.
			if (action == 4 && state.right.action != 7 && nextFrame == frameMax && actorDistance(state) < 40) {
				setReaction(state.right, 10);
			} else if (action == 5 && state.right.action != 8 && nextFrame == frameMax && actorDistance(state) < 30) {
				setReaction(state.right, 11);
				state.pendingRightDamage = state.damageToRight;
			} else if (action == 6 && state.right.action != 9 && nextFrame == frameMax && actorDistance(state) < 30) {
				setReaction(state.right, 12);
			}
		} else {
			actor.nextAction = 1;
			actor.nextFrame = 1;
		}
		return;
	}
	advanceActorFrame(actor, frameMax);
}

void applyFightRightAction(FightState &state) {
	FightActor &actor = state.right;
	const byte action = actor.action;
	const byte frame = actor.frame;
	const byte frameMax = fightFrameMax(true, action);
	if (action < 1 || action > 13)
		return;
	actor.nextAction = action;
	actor.nextFrame = frame;
	if (action == 1) {
		actor.nextAction = actor.nextFrame = 1;
		return;
	}
	if (action == 2) {
		if (frame < frameMax) {
			actor.nextFrame = frame + 1;
			if (actorDistance(state) > 24)
				actor.anchorX -= 8;
		} else
			actor.nextAction = actor.nextFrame = 1;
		return;
	}
	if (action == 3) {
		if (frame < frameMax) {
			actor.nextFrame = frame + 1;
			if (actor.anchorX < 248)
				actor.anchorX += 8;
		} else
			actor.nextAction = actor.nextFrame = 1;
		return;
	}
	if (action >= 4 && action <= 6) {
		if (frame < frameMax) {
			actor.nextFrame = frame + 1;
			const byte nextFrame = actor.nextFrame;
			// Defense actions 7-9 block their corresponding attacks 4-6.
			if (action == 4 && state.left.action != 7 && nextFrame == frameMax && actorDistance(state) < 30) {
				setReaction(state.left, 10);
				state.pendingLeftDamage = state.damageToLeft;
			} else if (action == 5 && state.left.action != 8 && nextFrame == frameMax && actorDistance(state) < 30) {
				setReaction(state.left, 11);
				state.pendingLeftDamage = state.damageToLeft;
			} else if (action == 6 && state.left.action != 9 && nextFrame == frameMax && actorDistance(state) < 30) {
				setReaction(state.left, 12);
				state.pendingLeftDamage = state.damageToLeft;
			}
		} else {
			actor.nextAction = 1;
			actor.nextFrame = 1;
		}
		return;
	}
	advanceActorFrame(actor, frameMax);
}

bool setFightLeftInput(FightState &state, int key) {
	byte action = 0;
	switch (key) {
	case 0x2D:
		action = 3;
		break;
	case 0x2E:
		action = 2;
		break;
	case 0x49:
		action = 4;
		break;
	case 0x4D:
		action = 5;
		break;
	case 0x51:
		action = 6;
		break;
	case 0x47:
		action = 7;
		break;
	case 0x4B:
		action = 8;
		break;
	case 0x4F:
		action = 9;
		break;
	default:
		return false;
	}
	state.left.nextAction = action;
	state.left.nextFrame = 1;
	return true;
}

void commitFightNext(FightState &state) {
	state.left.action = state.left.nextAction;
	state.left.frame = state.left.nextFrame;
	state.right.action = state.right.nextAction;
	state.right.frame = state.right.nextFrame;
}

void advanceFightFrame(FightState &state) {
	// All three native loops commit the choices first, then derive the next
	// frame and any pending damage from the newly current actions.
	commitFightNext(state);
	applyFightLeftAction(state);
	applyFightRightAction(state);
}

byte applyFightDamage(FightState &state) {
	// DOS 0x7995 consumes both pending hits. 0x9AC1 then gives the left meter
	// depletion priority (v39=8) over right meter depletion (v39=4).
	if (state.pendingLeftDamage) {
		state.leftMeter -= state.pendingLeftDamage;
		state.pendingLeftDamage = 0;
		if (state.leftMeter < 3)
			state.leftMeter = 0;
	}
	if (state.pendingRightDamage) {
		state.rightMeter -= state.pendingRightDamage;
		state.pendingRightDamage = 0;
		if (state.rightMeter < 3)
			state.rightMeter = 0;
	}
	if (state.leftMeter == 0) {
		state.ended = true;
		state.result = 8;
	} else if (state.rightMeter == 0) {
		state.ended = true;
		state.result = 4;
	}
	return state.result;
}

bool setFightConclusionFrame(FightState &state, byte result,
									   byte frame, byte randomFrame) {
	if ((result != 4 && result != 8) || frame < 1 || frame > 9)
		return false;
	if (result == 8) {
		state.left.action = 13;
		state.left.frame = frame;
		state.right.action = 14;
		state.right.frame = frame;
	} else {
		state.left.action = 14;
		state.left.frame = frame;
		state.right.action = 13;
		if (randomFrame < 1)
			randomFrame = 1;
		else if (randomFrame > 8)
			randomFrame = 8;
		state.right.frame = randomFrame;
	}
	state.left.nextAction = state.left.action;
	state.left.nextFrame = state.left.frame;
	state.right.nextAction = state.right.action;
	state.right.nextFrame = state.right.frame;
	return true;
}

int16 fightSampleSlot(bool rightActor, byte action, byte frame) {
	if (!rightActor) {
		if (frame == 1 && action == 2)
			return 8;
		if (frame == 1 && action == 3)
			return 9;
		if (frame == 2 && action == 4)
			return 12;
		if (frame == 2 && action == 6)
			return 14;
		return -1;
	}
	if (frame == 1 && action == 2)
		return 10;
	if (frame == 1 && action == 3)
		return 11;
	if (frame == 2 && action >= 4 && action <= 6)
		return 17;
	if (frame == 1 && action == 10)
		return 15;
	if (frame == 1 && action == 11)
		return 16;
	return -1;
}

byte chooseFightLeftAI(FightState &state, FightRandomSource &randomSource,
							 uint16 threshold, bool compareMeters) {
	byte action = 1;
	const bool react = randomSource.fightRandom(10) < threshold ||
		(compareMeters && state.leftMeter > state.rightMeter);
	if (actorDistance(state) < 30) {
		if (react) {
			if (state.right.nextFrame == 2 && (state.right.nextAction >= 4 && state.right.nextAction <= 6))
				action = (byte)(state.right.nextAction + 3);
			else
				action = (byte)(randomSource.fightRandom(5) + 3);
		}
	} else if (react)
		action = 2;
	state.left.nextAction = action;
	state.left.nextFrame = 1;
	return action;
}

byte chooseFightRightAI(FightState &state, FightRandomSource &randomSource, uint16 threshold) {
	byte action = 1;
	const bool react = randomSource.fightRandom(10) < threshold;
	if (actorDistance(state) < 30) {
		if (react) {
			if (state.left.nextFrame == 2 && (state.left.nextAction >= 4 && state.left.nextAction <= 6))
				action = (byte)(state.left.nextAction + 3);
			else
				action = (byte)(randomSource.fightRandom(5) + 3);
		}
	} else if (react)
		action = 2;
	state.right.nextAction = action;
	state.right.nextFrame = 1;
	return action;
}

} // namespace Emmanuelle
} // namespace Gob
