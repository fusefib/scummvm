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

#ifndef GOB_PREGOB_MEWILO_TOT_H
#define GOB_PREGOB_MEWILO_TOT_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/stream.h"

#include "gob/pregob/mewilo/platform.h"

namespace Gob {
namespace Mewilo {

struct MewiloBranch {
	byte kind;
	uint16 target;

	MewiloBranch() : kind(0), target(0) {}
};

struct MewiloSequenceItem {
	enum Type {
		kTarget,
		kInlineFF
	};

	Type type;
	uint16 offset;
	uint16 target;
	byte subOpcode;
	byte size;

	MewiloSequenceItem() : type(kTarget), offset(0), target(0), subOpcode(0), size(0) {}
};

struct MewiloGraphStats {
	uint32 reachableNodes;
	uint32 opcodeCounts[256];
	uint32 errors;
	uint16 requiredI00Records; ///< Highest reachable bank-0 record index plus one; zero if unused.

	MewiloGraphStats();
};

/** Reader for Mewilo's 16-byte, big-endian pointer-graph TOT dialect.
 *
 * This format predates Gob::TOTFile and is deliberately not represented by
 * Emmanuelle's later OldTotData structures.  In particular, the root pointer
 * at +2 is direct rather than biased by one.
 */
class MewiloTotData {
public:
	explicit MewiloTotData(Common::SeekableReadStream &stream,
		MewiloPlatform platform = kMewiloPlatformAtariST);

	bool valid() const { return _valid; }
	uint32 size() const { return _data.size(); }
	uint16 rootOffset() const { return _rootOffset; }
	uint16 declaredSize() const { return _declaredSize; }
	uint16 platformHeaderWord() const { return _platformHeaderWord; }

	bool has(uint32 offset, uint32 length = 1) const;
	byte byteAt(uint32 offset) const;
	uint16 uint16BEAt(uint32 offset) const;
	MewiloBranch branchAt(uint32 offset) const;

	const byte *dataAt(uint32 offset) const;

	/** Decode a root/0x1C sequence.  sequenceOffset points at the count byte. */
	bool decodeSequence(uint16 sequenceOffset, Common::Array<MewiloSequenceItem> &items,
		uint16 &endOffset) const;

	/** Return the byte size of an FF01..FF0B item, or zero if unknown. */
	static byte inlineFFSize(byte subOpcode);

	/** Validate reachable nodes and determine their required bank-0 records. */
	bool validateReachableGraph(MewiloGraphStats &stats, Common::Array<uint16> *nodes = nullptr) const;

private:
	bool validateNode(uint16 offset, uint16 &nodeSize, Common::Array<uint16> &targets) const;
	bool scanCString(uint32 offset, uint32 &endOffset) const;

	Common::Array<byte> _data;
	uint16 _declaredSize;
	uint16 _rootOffset;
	uint16 _platformHeaderWord;
	bool _valid;
};

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_TOT_H
