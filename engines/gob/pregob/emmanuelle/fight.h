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

#ifndef GOB_PREGOB_EMMANUELLE_FIGHT_H
#define GOB_PREGOB_EMMANUELLE_FIGHT_H

#include "common/scummsys.h"

namespace Gob {
namespace Emmanuelle {

struct FightActorPlacement {
	byte spriteId;   ///< 1..20 within the selected RECUP half
	byte recupIndex; ///< 0..39 absolute RECUP root index
	int16 x, y;
	int16 cellX, cellY;
	int16 cellWidth, cellHeight;
	FightActorPlacement() {
		spriteId = 0;
		recupIndex = 0;

		x = 0;
		y = 0;

		cellX = 0;
		cellY = 70;
		cellWidth = 72;
		cellHeight = 80;
	}
};

byte fightFrameMax(bool rightActor, byte action);
byte fightSpriteId(bool rightActor, byte action, byte frame);
byte fightRecupIndex(bool rightActor, byte action, byte frame);
bool fightPlacement(bool rightActor, byte action, byte frame, int16 anchorX,
						  int16 spriteWidth, int16 spriteHeight, FightActorPlacement &out);

struct FightActor {
	byte action, frame, nextAction, nextFrame;
	int16 anchorX;
	FightActor() : action(1), frame(1), nextAction(1), nextFrame(1), anchorX(0) {}
};

struct FightState {
	FightActor left, right;
	int16 leftMeter, rightMeter;
	byte damageToLeft, damageToRight, pendingLeftDamage, pendingRightDamage;
	bool ended;
	byte result; ///< Script variable 39: 0 ongoing, 4 right defeated, 8 left defeated.
	FightState();
};

/** Platform fight input lifetime at and between action boundaries. */
class FightInputState {
public:
	explicit FightInputState(bool retainUntilBoundary) :
		_retainUntilBoundary(retainUntilBoundary) {
		_pendingKey = 0;
		_automatic = true;
	}

	void beginTick() {
		if (!_retainUntilBoundary)
			_pendingKey = 0;
	}
	void noteKey(int16 key) {
		if (key != 0) {
			_pendingKey = key;
			_automatic = false;
		}
	}
	int16 takeActionKey() {
		const int16 key = _pendingKey;
		_pendingKey = 0;
		return key;
	}
	int16 pendingKey() const { return _pendingKey; }
	bool automatic() const { return _automatic; }

private:
	int16 _pendingKey;
	bool _automatic;
	bool _retainUntilBoundary;
};

void initFightState(FightState &state, uint16 leftMeterBonus, uint16 rightMeterBonus,
						  uint16 leftDamageOption, uint16 rightDamageOption, uint16 leftBaseMeter, uint16 rightBaseMeter);
void applyFightLeftAction(FightState &state);
void applyFightRightAction(FightState &state);
bool setFightLeftInput(FightState &state, int key);
void commitFightNext(FightState &state);
void advanceFightFrame(FightState &state);
byte applyFightDamage(FightState &state); ///< returns 0,4,8
bool setFightConclusionFrame(FightState &state, byte result,
									   byte frame, byte randomFrame);
int16 fightSampleSlot(bool rightActor, byte action, byte frame);

class FightRandomSource {
public:
	virtual ~FightRandomSource() {}
	virtual uint16 fightRandom(uint16 upperExclusive) = 0;
};
byte chooseFightLeftAI(FightState &state, FightRandomSource &randomSource,
							 uint16 threshold, bool compareMeters);
byte chooseFightRightAI(FightState &state, FightRandomSource &randomSource, uint16 threshold);

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_FIGHT_H
