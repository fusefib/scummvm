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

#ifndef WIZARDRY_DATA_HUFFMAN_H
#define WIZARDRY_DATA_HUFFMAN_H

#include "common/array.h"
#include "common/str.h"

namespace Wizardry {

/** Shared CONUNIT WR32/WR33 decoder used by IV/V Huffman resources. */
class HuffmanDecoder {
public:
	enum { kTableSize = 768,
		   kNodeCount = 256 };
	HuffmanDecoder();
	// WR32 reads exactly 768 bytes. A containing resource may have a trailer.
	// Failed loads preserve the previous table. Unreachable entries are ignored.
	bool load(const byte *table, uint32 size, Common::String &error);
	bool load(const Common::Array<byte> &table, Common::String &error) {
		return load(table.data(), table.size(), error);
	}
	// Returns a Pascal length byte and up to 255 decoded bytes. Neither output
	// nor bitsConsumed changes on failure. Padding bits and unused trailing
	// bytes are allowed; no speculative read past the last required bit occurs.
	bool decodePascal(const byte *encoded, uint32 size, Common::Array<byte> &output,
					  uint32 &bitsConsumed, Common::String &error) const;
	bool isLoaded() const { return _loaded; }
	const byte *table() const { return _table; }
	uint16 maxCodeBits() const { return _maxCodeBits; }
	uint32 maxEncodedSize() const { return (256U * _maxCodeBits + 7) / 8; }

private:
	byte _table[kTableSize];
	uint16 _maxCodeBits;
	bool _loaded;
	bool symbol(const byte *encoded, uint32 size, uint32 &bit, byte &value) const;
};

} // namespace Wizardry
#endif
