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

#include "common/endian.h"
#include "common/str.h"

#include "gob/pregob/emmanuelle/oldtot.h"

namespace Gob {
namespace Emmanuelle {

// A failed approach at Iguacu airport branches to Salvador's failure handler.
// It sets v[65] to 1, possibly replacing Salvador's successful state, while
// leaving Iguacu's v[67] unchanged. Use the existing Iguacu failure handler.
// For example, "I'd be all yours, for just one kiss..." with a low dialogue
// score can produce "Cut out the sweet-talk! You're getting on my nerves!"
// in English.
//
// Applies to: All versions
// Script: LOADAERO.TOT (DOS/ST), LOADAERO.TOA (Amiga)
bool OldTotData::applyScriptPatches(OldTotPlatformTag platform,
		Common::Language language, const Common::String &resourceName) {
	if (platform != kOldTotPlatformDOS && platform != kOldTotPlatformAtariST &&
		platform != kOldTotPlatformAmiga)
		return false;
	if (!resourceName.equalsIgnoreCase(platform == kOldTotPlatformAmiga
			? "LOADAERO.TOA" : "LOADAERO.TOT"))
		return false;

	if (language == Common::DE_DEU && platform != kOldTotPlatformAmiga)
		return false;

	struct IguacuAirportLayout {
		Common::Language language;
		uint16 condition;
		uint16 approachContinuation;
		uint16 iguacuFailure;
		uint16 salvadorFailure;
		uint16 ticketSelection;
	};
	static const IguacuAirportLayout layouts[] = {
		{Common::EN_ANY, 0x4345, 0x433F, 0x42FD, 0x44FB, 0x41FA},
		{Common::ES_ESP, 0x4137, 0x4131, 0x40EF, 0x42ED, 0x3FEC},
		{Common::FR_FRA, 0x421C, 0x4216, 0x41D4, 0x43D2, 0x40D1},
		{Common::DE_DEU, 0x436C, 0x4366, 0x4324, 0x4522, 0x4221}
	};
	const IguacuAirportLayout *layout = nullptr;
	for (uint i = 0; i < ARRAYSIZE(layouts); ++i) {
		if (layouts[i].language == language)
			layout = &layouts[i];
	}
	if (!layout)
		return false;

	const byte condition[] = {
		0x21, 1, 0, 12, 0, 0, // If v[12] == 0 (failed approach).
		1                     // False branch: continue the accepted approach.
	};
	const byte iguacuSet[] = {
		0x1C, 2,              // Sequence of two instructions.
		0xFF, 5, 0, 67, 0, 1  // Set v[67] = 1 (Iguacu failed), then select tickets.
	};
	const byte salvadorSet[] = {
		0x1C, 2,
		0xFF, 5, 0, 65, 0, 1  // Set v[65] = 1 (Salvador failed), then select tickets.
	};
	if (!has(layout->condition, 12) || !has(layout->iguacuFailure, 10) ||
		!has(layout->salvadorFailure, 10) ||
		memcmp(&_data[layout->condition], condition, sizeof(condition)) != 0 ||
		uint16BEAt(layout->condition + 7) != layout->approachContinuation ||
		_data[layout->condition + 9] != 1 ||
		uint16BEAt(layout->condition + 10) != layout->salvadorFailure ||
		memcmp(&_data[layout->iguacuFailure], iguacuSet, sizeof(iguacuSet)) != 0 ||
		uint16BEAt(layout->iguacuFailure + 8) != layout->ticketSelection ||
		memcmp(&_data[layout->salvadorFailure], salvadorSet, sizeof(salvadorSet)) != 0 ||
		uint16BEAt(layout->salvadorFailure + 8) != layout->ticketSelection)
		return false;

	// Replace only the true branch's destination in the owned script buffer.
	WRITE_BE_UINT16(&_data[layout->condition + 10], layout->iguacuFailure);
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
