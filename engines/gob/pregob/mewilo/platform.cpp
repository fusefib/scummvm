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

#include "common/util.h"
#include "gob/pregob/mewilo/platform.h"

namespace Gob {
namespace Mewilo {

// CPC stores four source pixels per cell, then doubles them for presentation.
// The shared script coordinate space remains 40 cells wide on every platform.
static const MewiloPlatformProfile kProfiles[] = {
	{ kMewiloPlatformAtariST, "LOAD", 8, 1, true, true },
	{ kMewiloPlatformAmstradCPC, "AMSOAD", 4, 2, false, false },
	{ kMewiloPlatformAmiga, "LOAD", 8, 1, false, false },
	{ kMewiloPlatformThomsonTO, "THLOAD", 8, 1, false, false }
};

bool isValidMewiloPlatform(MewiloPlatform platform) {
	return (uint)platform < ARRAYSIZE(kProfiles);
}

const MewiloPlatformProfile &getMewiloPlatformProfile(MewiloPlatform platform) {
	assert(isValidMewiloPlatform(platform));
	return kProfiles[platform];
}

} // End of namespace Mewilo
} // End of namespace Gob
