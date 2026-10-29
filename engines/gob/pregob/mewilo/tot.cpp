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
#include "common/stream.h"
#include "common/util.h"

#include "gob/pregob/mewilo/tot.h"

namespace Gob {
namespace Mewilo {

MewiloGraphStats::MewiloGraphStats() : reachableNodes(0), errors(0), requiredI00Records(0) {
	memset(opcodeCounts, 0, sizeof(opcodeCounts));
}

MewiloTotData::MewiloTotData(Common::SeekableReadStream &stream, MewiloPlatform platform) :
	_declaredSize(0), _rootOffset(0), _platformHeaderWord(0), _valid(false) {

	const int64 streamSize = stream.size();
	if (!isValidMewiloPlatform(platform) || streamSize < 16 || streamSize > 0xFFFF)
		return;

	_data.resize((uint32)streamSize);
	stream.seek(0);
	if (stream.read(_data.data(), _data.size()) != _data.size()) {
		_data.clear();
		return;
	}

	_declaredSize = uint16BEAt(0);
	_rootOffset = uint16BEAt(2);
	if (_declaredSize != _data.size() || _rootOffset < 16 || _rootOffset >= _data.size())
		return;

	if (platform == kMewiloPlatformAmstradCPC) {
		// CPC modules keep the direct size/root fields, but use the word at +6
		// (observed as 0x0B6B or 0x0C27) while the other reserved bytes remain
		// zero. Its semantic meaning is not yet established, so preserve it as
		// opaque platform metadata instead of assigning an invented role.
		if (_data[4] || _data[5])
			return;
		for (uint i = 8; i < 16; ++i) {
			if (_data[i] != 0)
				return;
		}
		_platformHeaderWord = uint16BEAt(6);
		if (!_platformHeaderWord)
			return;
	} else {
		// Atari ST, Amiga and Thomson all use the original LOAD-dialect header:
		// bytes +4..+15 are reserved and zero. Keep the guard because it also
		// prevents accidentally accepting unrelated old-TOT formats.
		for (uint i = 4; i < 16; ++i) {
			if (_data[i] != 0)
				return;
		}
	}

	_valid = true;
}

bool MewiloTotData::has(uint32 offset, uint32 length) const {
	return offset <= _data.size() && length <= (_data.size() - offset);
}

byte MewiloTotData::byteAt(uint32 offset) const {
	return has(offset) ? _data[offset] : 0;
}

uint16 MewiloTotData::uint16BEAt(uint32 offset) const {
	return has(offset, 2) ? READ_BE_UINT16(&_data[offset]) : 0;
}

MewiloBranch MewiloTotData::branchAt(uint32 offset) const {
	MewiloBranch branch;
	if (!has(offset, 3))
		return branch;
	branch.kind = _data[offset];
	branch.target = READ_BE_UINT16(&_data[offset + 1]);
	return branch;
}

const byte *MewiloTotData::dataAt(uint32 offset) const {
	return has(offset) ? &_data[offset] : nullptr;
}

byte MewiloTotData::inlineFFSize(byte subOpcode) {
	static const byte kSizes[12] = {
		0, 6, 2, 3, 3, 4, 4, 4, 4, 4, 4, 3};
	return subOpcode < ARRAYSIZE(kSizes) ? kSizes[subOpcode] : 0;
}

bool MewiloTotData::decodeSequence(uint16 sequenceOffset,
	Common::Array<MewiloSequenceItem> &items, uint16 &endOffset) const {
	items.clear();
	endOffset = sequenceOffset;
	if (!has(sequenceOffset))
		return false;

	const byte count = byteAt(sequenceOffset);
	uint32 p = sequenceOffset + 1;
	for (uint i = 0; i < count; ++i) {
		if (!has(p))
			return false;

		MewiloSequenceItem item;
		item.offset = (uint16)p;
		if (byteAt(p) == 0xFF) {
			if (!has(p, 2))
				return false;
			item.type = MewiloSequenceItem::kInlineFF;
			item.subOpcode = byteAt(p + 1);
			item.size = inlineFFSize(item.subOpcode);
			if (!item.size || !has(p, item.size))
				return false;
			p += item.size;
		} else {
			if (!has(p, 2))
				return false;
			item.type = MewiloSequenceItem::kTarget;
			item.target = uint16BEAt(p);
			item.size = 2;
			p += 2;
		}
		items.push_back(item);
	}

	if (p > 0xFFFF)
		return false;
	endOffset = (uint16)p;
	return true;
}

bool MewiloTotData::scanCString(uint32 offset, uint32 &endOffset) const {
	if (!has(offset))
		return false;
	for (uint32 p = offset; p < _data.size(); ++p) {
		if (_data[p] == 0) {
			endOffset = p + 1;
			return true;
		}
	}
	return false;
}

bool MewiloTotData::validateNode(uint16 offset, uint16 &nodeSize,
	Common::Array<uint16> &targets) const {
	nodeSize = 0;
	targets.clear();
	if (!has(offset) || offset == 0)
		return false;

	const byte op = byteAt(offset);
	uint32 p = offset;
	switch (op) {
	case 0x1C: {
		Common::Array<MewiloSequenceItem> items;
		uint16 end = 0;
		if (!decodeSequence(offset + 1, items, end))
			return false;
		for (uint i = 0; i < items.size(); ++i)
			if (items[i].type == MewiloSequenceItem::kTarget && items[i].target)
				targets.push_back(items[i].target);
		nodeSize = end - offset;
		return true;
	}
	case 0x1D: {
		if (!has(offset, 10))
			return false;
		uint32 end = 0;
		if (!scanCString(offset + 10, end) || end - offset > 0xFFFF)
			return false;
		nodeSize = (uint16)(end - offset);
		return true;
	}
	case 0x1B: {
		if (!has(offset, 4))
			return false;
		const byte count = byteAt(offset + 3);
		p = offset + 4 + (uint32)count * 4;
		if (!has(p, (uint32)(count + 1) * 3))
			return false;
		const uint16 escapeTarget = uint16BEAt(offset + 1);
		if (escapeTarget)
			targets.push_back(escapeTarget);
		for (uint i = 0; i <= count; ++i) {
			MewiloBranch branch = branchAt(p + i * 3);
			if (branch.target)
				targets.push_back(branch.target);
		}
		nodeSize = (uint16)(4 + count * 4 + (count + 1) * 3);
		return true;
	}
	case 0x20: {
		if (!has(offset, 2))
			return false;
		const byte count = byteAt(offset + 1);
		if (!count || !has(offset + 2, (uint32)count * 2))
			return false;
		for (uint i = 0; i < count; ++i) {
			uint16 target = uint16BEAt(offset + 2 + i * 2);
			if (target)
				targets.push_back(target);
		}
		nodeSize = 2 + count * 2;
		return true;
	}
	case 0x21: {
		if (!has(offset, 10))
			return false;
		MewiloBranch t = branchAt(offset + 4);
		MewiloBranch f = branchAt(offset + 7);
		if (t.target)
			targets.push_back(t.target);
		if (f.target)
			targets.push_back(f.target);
		nodeSize = 10;
		return byteAt(offset + 1) <= 3;
	}
	case 0x22: {
		if (!has(offset, 7))
			return false;
		const byte count = byteAt(offset + 6);
		if (!has(offset + 7, (uint32)count * 4))
			return false;
		uint16 target = uint16BEAt(offset + 1);
		if (target)
			targets.push_back(target);
		target = uint16BEAt(offset + 3);
		if (target)
			targets.push_back(target);
		for (uint i = 0; i < count; ++i) {
			MewiloBranch branch = branchAt(offset + 8 + i * 4);
			if (branch.target)
				targets.push_back(branch.target);
		}
		nodeSize = 7 + count * 4;
		return true;
	}
	case 0x12: {
		if (!has(offset, 15))
			return false;
		uint16 target = uint16BEAt(offset + 1);
		if (target)
			targets.push_back(target);
		MewiloBranch success = branchAt(offset + 8);
		MewiloBranch failure = branchAt(offset + 11);
		if (success.target)
			targets.push_back(success.target);
		if (failure.target)
			targets.push_back(failure.target);

		const byte rejectLength = byteAt(offset + 14);
		p = offset + 15;
		if (!has(p, rejectLength))
			return false;
		p += rejectLength;
		if (!has(p))
			return false;
		const byte fieldCount = byteAt(p++);
		for (uint i = 0; i < fieldCount; ++i) {
			if (!has(p, 10))
				return false;
			const byte answerLength = byteAt(p + 9);
			p += 10;
			if (!has(p, answerLength))
				return false;
			p += answerLength;
		}
		if (p - offset > 0xFFFF)
			return false;
		nodeSize = (uint16)(p - offset);
		return true;
	}
	case 0x41: {
		if (!has(offset, 7))
			return false;
		const uint16 size = uint16BEAt(offset + 1);
		if (size < 7 || !has(offset, size))
			return false;
		nodeSize = size;
		return true;
	}
	case 0x60:
		if (!has(offset, 8))
			return false;
		nodeSize = 8;
		return true;
	default:
		return false;
	}
}

bool MewiloTotData::validateReachableGraph(MewiloGraphStats &stats, Common::Array<uint16> *nodes) const {
	if (nodes)
		nodes->clear();
	stats = MewiloGraphStats();
	if (!valid()) {
		stats.errors = 1;
		return false;
	}

	Common::Array<MewiloSequenceItem> rootItems;
	uint16 rootEnd = 0;
	if (!decodeSequence(_rootOffset, rootItems, rootEnd)) {
		stats.errors = 1;
		return false;
	}

	Common::Array<uint16> queue;
	Common::Array<byte> visited;
	visited.resize(_data.size());
	memset(visited.data(), 0, visited.size());

	for (uint i = 0; i < rootItems.size(); ++i)
		if (rootItems[i].type == MewiloSequenceItem::kTarget && rootItems[i].target)
			queue.push_back(rootItems[i].target);

	for (uint q = 0; q < queue.size(); ++q) {
		const uint16 offset = queue[q];
		if (!offset)
			continue;
		if (offset < 16 || offset >= visited.size()) {
			++stats.errors;
			continue;
		}
		if (visited[offset])
			continue;
		visited[offset] = 1;

		uint16 nodeSize = 0;
		Common::Array<uint16> targets;
		if (!validateNode(offset, nodeSize, targets)) {
			++stats.errors;
			continue;
		}

		if (nodes)
			nodes->push_back(offset);
		++stats.reachableNodes;
		++stats.opcodeCounts[byteAt(offset)];
		if (byteAt(offset) == 0x60)
			stats.requiredI00Records = MAX<uint16>(stats.requiredI00Records, byteAt(offset + 7) + 1);
		// Queue every nonzero edge; the bounds check above must see malformed
		// nested edges too, rather than silently dropping them.
		for (uint i = 0; i < targets.size(); ++i)
			if (targets[i])
				queue.push_back(targets[i]);
	}

	return stats.errors == 0;
}

} // End of namespace Mewilo
} // End of namespace Gob
