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

#ifndef GOB_PREGOB_EMMANUELLE_OLDIXX_H
#define GOB_PREGOB_EMMANUELLE_OLDIXX_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/stream.h"

namespace Gob {
namespace Emmanuelle {

class OldIxxBank {
public:
	explicit OldIxxBank(Common::SeekableReadStream &stream);

	bool valid() const { return !_data.empty() && !_records.empty(); }
	uint recordCount() const { return _records.size(); }
	uint32 tailOffset() const { return _tailOffset; }
	uint32 tailSize() const { return _data.size() - _tailOffset; }

	/** Return the packed payload without its big-endian record size. */
	bool getPayload(uint index, Common::Array<byte> &payload) const;

private:
	struct Record {
		uint32 offset;
		uint16 size;
		Record(uint32 o, uint16 s) : offset(o), size(s) {}
	};

	Common::Array<byte> _data;
	Common::Array<Record> _records;
	uint32 _tailOffset;
};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_OLDIXX_H
