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

#ifndef GOB_PREGOB_EMMANUELLE_ANIMATION_H
#define GOB_PREGOB_EMMANUELLE_ANIMATION_H

#include "common/scummsys.h"

#include "gob/pregob/emmanuelle/oldtot.h"

namespace Gob {
namespace Emmanuelle {

struct AnimationFramePlan {
	int16 x, y, cellWidth, cellHeight;
	uint16 descriptorOffset; ///< frame-specific descriptor selected from node+16 table
	AnimationFramePlan() : x(0), y(0), cellWidth(0), cellHeight(0), descriptorOffset(0) {}
};

bool planAnimationFrame(const OldTotAnimationSetup &setup, int16 baseX, int16 baseY,
						  uint16 step, byte frame, AnimationFramePlan &out);

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_ANIMATION_H
