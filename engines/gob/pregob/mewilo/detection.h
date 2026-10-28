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

#ifndef GOB_PREGOB_MEWILO_DETECTION_H
#define GOB_PREGOB_MEWILO_DETECTION_H

#include "common/fs.h"
#include "engines/game.h"

namespace Gob {

// Advisory audio evidence chooses between the already matched 68k profiles.
// Both/absent markers retain both choices. Does not change resource detection.
void preferMewiloNativeProfile(const Common::FSList &files, DetectedGames &games);

} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_DETECTION_H
