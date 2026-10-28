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

#include "common/endian.h"
#include "common/util.h"

#include "common/md5.h"
#include "common/memstream.h"
#include "gob/pregob/mewilo/graphics.h"
#include "gob/surface.h"

namespace Gob {
namespace Mewilo {

MewiloI00Bank::MewiloI00Bank() : _tailOffset(0) {
}

MewiloI00Bank::MewiloI00Bank(Common::SeekableReadStream &stream, bool cpcZeroPrefix) : _tailOffset(0) {
	load(stream, cpcZeroPrefix);
}

bool MewiloI00Bank::load(Common::SeekableReadStream &stream, bool cpcZeroPrefix) {
	_data.clear();
	_records.clear();
	_tailOffset = 0;

	const int64 streamSize = stream.size();
	if (streamSize <= 0 || streamSize > 0x7FFFFFFF)
		return false;

	_data.resize((uint32)streamSize);
	stream.seek(0);
	if (stream.read(_data.data(), _data.size()) != _data.size()) {
		_data.clear();
		return false;
	}

	uint32 p = 0;
	while (p + 2 <= _data.size()) {
		const uint16 framedSize = READ_BE_UINT16(&_data[p]);
		if (framedSize < 2 || p + framedSize > _data.size())
			break;
		_records.push_back(Record(p, framedSize));
		p += framedSize;
	}

	// One known CPC AMSOAD1.I00 has a 128-byte zero prefix where the other
	// extracted files carry an AMSDOS header. It is not an image record. If
	// ordinary framing found nothing, recognize that packaging quirk without
	// weakening the framed-stream parser itself.
	if (cpcZeroPrefix && _records.empty() && _data.size() > 130) {
		bool zeroPrefix = true;
		for (uint i = 0; i < 128; ++i) {
			if (_data[i]) {
				zeroPrefix = false;
				break;
			}
		}
		if (zeroPrefix) {
			p = 128;
			while (p + 2 <= _data.size()) {
				const uint16 framedSize = READ_BE_UINT16(&_data[p]);
				if (framedSize < 2 || p + framedSize > _data.size())
					break;
				_records.push_back(Record(p, framedSize));
				p += framedSize;
			}
		}
	}

	_tailOffset = p;
	return !_records.empty();
}

bool MewiloI00Bank::getRecord(uint index, uint16 &framedSize,
	const byte *&payload, uint32 &payloadSize) const {
	framedSize = 0;
	payload = nullptr;
	payloadSize = 0;
	if (index >= _records.size())
		return false;

	const Record &record = _records[index];
	framedSize = record.framedSize;
	payloadSize = record.framedSize - 2;
	// Empty framed records are representable, including at EOF. Never index
	// Array::operator[] at size(); a zero-length payload has no data pointer.
	if (payloadSize)
		payload = &_data[record.offset + 2];
	return true;
}

MewiloPackedDecodeResult::MewiloPackedDecodeResult() :
	pixelsWritten(0), bytesConsumed(0), runOverhang(0), complete(false), truncated(false) {
}

MewiloPackedDecodeResult drawMewiloPacked(const byte *payload, uint32 payloadSize,
	Surface &dest, int16 x, int16 y, uint16 width, uint16 height, byte scaleX) {
	MewiloPackedDecodeResult result;
	if (!payload || !width || !height || !scaleX)
		return result;

	const uint32 targetPixels = (uint32)width * height;
	uint32 p = 0;
	while (result.pixelsWritten < targetPixels) {
		if (p >= payloadSize) {
			result.truncated = true;
			break;
		}

		const byte code = payload[p++];
		const byte color = code >> 4;
		uint32 run = code & 7;
		if (!(code & 8)) {
			if (p >= payloadSize) {
				result.truncated = true;
				break;
			}
			run = (run << 8) | payload[p++];
		}
		++run;

		const uint32 remaining = targetPixels - result.pixelsWritten;
		const uint32 emit = MIN<uint32>(run, remaining);
		for (uint32 i = 0; i < emit; ++i) {
			const uint32 linear = result.pixelsWritten + i;
			const int16 px = x + (linear % width) * scaleX;
			const int16 py = y + (linear / width);
			for (byte sx = 0; sx < scaleX; ++sx) {
				const int16 outX = px + sx;
				if (outX >= 0 && py >= 0 && outX < dest.getWidth() && py < dest.getHeight())
					dest.putPixel(outX, py, color);
			}
		}

		result.pixelsWritten += emit;
		if (run > emit) {
			result.runOverhang = run - emit;
			break;
		}
	}

	result.bytesConsumed = p;
	result.complete = result.pixelsWritten == targetPixels;
	return result;
}

bool MewiloI00Bank::computeMD5(byte digest[16]) const {
	if (!valid())
		return false;
	Common::MemoryReadStream owned(_data.data(), _data.size());
	return Common::computeStreamMD5(owned, digest);
}

} // End of namespace Mewilo
} // End of namespace Gob
