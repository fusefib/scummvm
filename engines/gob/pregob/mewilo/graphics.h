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

#ifndef GOB_PREGOB_MEWILO_GRAPHICS_H
#define GOB_PREGOB_MEWILO_GRAPHICS_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/stream.h"

namespace Gob {

class Surface;

namespace Mewilo {

class MewiloI00Bank {
public:
	MewiloI00Bank();
	explicit MewiloI00Bank(Common::SeekableReadStream &stream);

	bool load(Common::SeekableReadStream &stream);
	bool valid() const { return !_records.empty(); }
	uint recordCount() const { return _records.size(); }
	uint32 tailOffset() const { return _tailOffset; }
	uint32 tailSize() const { return _data.size() - _tailOffset; }

	bool getRecord(uint index, uint16 &framedSize, const byte *&payload,
		uint32 &payloadSize) const;

private:
	struct Record {
		uint32 offset;
		uint16 framedSize;
		Record(uint32 o, uint16 s) : offset(o), framedSize(s) {}
	};

	Common::Array<byte> _data;
	Common::Array<Record> _records;
	uint32 _tailOffset;
};

struct MewiloPackedDecodeResult {
	uint32 pixelsWritten;
	uint32 bytesConsumed;
	uint32 runOverhang;
	bool complete;
	bool truncated;

	MewiloPackedDecodeResult();
};

/** Decode Mewilo's geometry-driven 4-bit packed RLE into a Gob surface.
 *
 * The byte grammar matches Gob's early packed renderer, but this bounded
 * implementation reports truncation instead of reading outside host memory.
 * scaleX only affects presentation; geometry and pixelsWritten remain in source
 * pixels so the same decoder can validate Atari (1x) and CPC (2x) resources.
 */
MewiloPackedDecodeResult drawMewiloPacked(const byte *payload, uint32 payloadSize,
	Surface &dest, int16 x, int16 y, uint16 width, uint16 height, byte scaleX = 1);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_GRAPHICS_H
