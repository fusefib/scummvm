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

#include "gob/pregob/emmanuelle/oldixx.h"

namespace Gob {
namespace Emmanuelle {

OldIxxBank::OldIxxBank(Common::SeekableReadStream &stream) {
	_tailOffset = 0;

	const int64 size = stream.size();
	if (size <= 0 || size > 0x7FFFFFFF)
		return;
	_data.resize((uint32)size);
	stream.seek(0);
	if (stream.read(&_data[0], _data.size()) != _data.size()) {
		_data.clear();
		return;
	}

	uint32 offset = 0;
	while (offset + 2 <= _data.size()) {
		const uint16 recordSize = ((uint16)_data[offset] << 8) | _data[offset + 1];
		if (recordSize < 2 || recordSize > _data.size() - offset)
			break;
		_records.push_back(Record(offset, recordSize));
		offset += recordSize;
	}
	_tailOffset = offset;
}

bool OldIxxBank::getPayload(uint index, Common::Array<byte> &payload) const {
	if (index >= _records.size())
		return false;
	const Record &r = _records[index];
	payload.resize(r.size - 2);
	if (!payload.empty())
		memcpy(&payload[0], &_data[r.offset + 2], payload.size());
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
