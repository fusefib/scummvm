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
 */

#ifndef WIZARDRY_VM_TRANSFER_H
#define WIZARDRY_VM_TRANSFER_H

#include "common/array.h"
#include "common/str.h"

namespace Wizardry {

/** Private earlier-scenario SAVE images used by the original Move Characters. */
struct TransferDisks {
	Common::Array<byte> images[3];
	uint32 sourceCRC[3] = {0, 0, 0};
	byte active = 0;
	bool choosing = false;
	bool transaction = false;

	static byte scenario(uint32 programCRC);
	static byte sourceCount(byte target) { return target == 5 ? 3 : target == 3 ? 2
																: target == 2   ? 1
																				: 0; }
	static bool validImage(byte source, const Common::Array<byte> &image, Common::String &error);
	bool attach(byte target, byte source, const Common::Array<byte> &image, Common::String &error);
	bool present(byte source) const { return source >= 1 && source <= 3 && !images[source - 1].empty(); }
	bool empty() const { return images[0].empty() && images[1].empty() && images[2].empty() && !sourceCRC[0] && !sourceCRC[1] && !sourceCRC[2] && !active && !choosing && !transaction; }
};

} // namespace Wizardry
#endif
