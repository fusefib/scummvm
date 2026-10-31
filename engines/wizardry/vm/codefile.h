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

#ifndef WIZARDRY_VM_CODEFILE_H
#define WIZARDRY_VM_CODEFILE_H

#include "common/array.h"
#include "common/str.h"

namespace Wizardry {

struct PCodeProcedure {
	uint16 anchor = 0;
	uint16 entry = 0;
	uint16 exit = 0;
	uint16 localBytes = 0;
	uint16 parameterBytes = 0;
	byte level = 0;
};

struct PCodeSegment {
	byte number = 0;
	uint16 fileBlock = 0;
	Common::String name;
	Common::Array<byte> code;
	// Index zero is deliberately unused: Pascal procedures start at one.
	Common::Array<PCodeProcedure> procedures;
};

/** The supplied I-V linked UCSD codefiles, not a generic p-System loader. */
class PCodeFile {
public:
	bool load(const Common::Array<byte> &data, Common::String &error);
	const PCodeSegment *segment(byte number) const;
	const PCodeProcedure *procedure(byte segment, byte number) const;
	const Common::Array<PCodeSegment> &segments() const { return _segments; }
	uint32 checksum() const { return _checksum; }

private:
	Common::Array<PCodeSegment> _segments;
	uint32 _checksum = 0;
};

uint32 wizardryCrc32(const byte *data, uint32 size);

} // namespace Wizardry
#endif
