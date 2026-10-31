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

#include "wizardry/data/huffman.h"
#include "common/util.h"

namespace Wizardry {

HuffmanDecoder::HuffmanDecoder() : _maxCodeBits(0), _loaded(false) {
	memset(_table, 0, sizeof(_table));
}

bool HuffmanDecoder::load(const byte *table, uint32 size, Common::String &error) {
	error.clear();
	if (!table || size < kTableSize) {
		error = "Truncated CONUNIT Huffman table (768 bytes required)";
		return false;
	}
	byte candidate[kTableSize];
	memcpy(candidate, table, sizeof(candidate));
	for (uint i = 0; i < kNodeCount; ++i)
		candidate[i] &= 3;
	// Iterative post-order graph validation permits shared acyclic subtrees.
	// Native branch indices are bytes; cycles are the only unbounded traversal.
	byte color[kNodeCount] = {}, nodes[kNodeCount] = {}, phase[kNodeCount] = {};
	uint16 depths[kNodeCount] = {};
	uint top = 0;
	color[0] = 1;
	for (;;) {
		const uint node = nodes[top];
		if (phase[top] < 2) {
			const uint branch = phase[top]++;
			const byte mask = branch ? 1 : 2;
			if (candidate[node] & mask)
				continue;
			const byte child = candidate[(branch ? 256 : 512) + node];
			if (color[child] == 1) {
				error = "CONUNIT Huffman table contains a reachable cycle";
				return false;
			}
			if (!color[child]) {
				if (top + 1 >= kNodeCount) {
					error = "CONUNIT Huffman tree exceeds its 256-node limit";
					return false;
				}
				++top;
				nodes[top] = child;
				phase[top] = 0;
				color[child] = 1;
			}
		} else {
			const uint zeroDepth = (candidate[node] & 2) ? 0 : depths[candidate[512 + node]];
			const uint oneDepth = (candidate[node] & 1) ? 0 : depths[candidate[256 + node]];
			depths[node] = 1 + MAX(zeroDepth, oneDepth);
			color[node] = 2;
			if (!top)
				break;
			--top;
		}
	}
	memcpy(_table, candidate, sizeof(_table));
	_maxCodeBits = depths[0];
	_loaded = true;
	return true;
}

bool HuffmanDecoder::symbol(const byte *encoded, uint32 size, uint32 &bit, byte &value) const {
	uint node = 0;
	for (uint depth = 0; depth < _maxCodeBits; ++depth) {
		if (bit / 8 >= size)
			return false;
		const bool one = (encoded[bit / 8] & (1U << (bit & 7))) != 0;
		++bit;
		const uint next = _table[(one ? 256 : 512) + node];
		if (_table[node] & (one ? 1 : 2)) {
			value = next;
			return true;
		}
		node = next;
	}
	return false;
}

bool HuffmanDecoder::decodePascal(const byte *encoded, uint32 size, Common::Array<byte> &output,
								  uint32 &bitsConsumed, Common::String &error) const {
	error.clear();
	if (!_loaded) {
		error = "CONUNIT Huffman decoding requested before loading a table";
		return false;
	}
	if (!encoded || !size) {
		error = "Truncated Huffman Pascal string";
		return false;
	}
	uint32 bit = 0;
	byte length;
	if (!symbol(encoded, size, bit, length)) {
		error = "Truncated Huffman string length";
		return false;
	}
	Common::Array<byte> result(uint(length) + 1);
	result[0] = length;
	for (uint i = 1; i <= length; ++i) {
		if (!symbol(encoded, size, bit, result[i])) {
			error = "Truncated Huffman string data";
			return false;
		}
	}
	output = Common::move(result);
	bitsConsumed = bit;
	return true;
}

} // namespace Wizardry
