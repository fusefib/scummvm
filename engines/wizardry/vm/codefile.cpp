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

#include "wizardry/vm/codefile.h"
#include "common/endian.h"
#include "common/util.h"

namespace Wizardry {

uint32 wizardryCrc32(const byte *data, uint32 size) {
	uint32 crc = 0xffffffffU;
	for (uint32 i = 0; i < size; ++i) {
		crc ^= data[i];
		for (uint bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
	}
	return crc ^ 0xffffffffU;
}

bool PCodeFile::load(const Common::Array<byte> &data, Common::String &error) {
	error.clear();
	if (data.size() < 512 || data.size() > 1024 * 1024) {
		error = "SYSTEM.PASCAL has an invalid size";
		return false;
	}
	Common::Array<PCodeSegment> result;
	uint32 starts[16] = {}, ends[16] = {};
	for (uint slot = 0; slot < 16; ++slot) {
		const uint32 start = uint32(READ_LE_UINT16(data.data() + 4 * slot)) * 512;
		const uint32 length = READ_LE_UINT16(data.data() + 4 * slot + 2);
		if (!length)
			continue;
		if (start < 512 || length < 4 || (length & 1) || start > data.size() || length > data.size() - start) {
			error = Common::String::format("Invalid code segment extent at directory slot %u", slot);
			return false;
		}
		for (uint earlier = 0; earlier < slot; ++earlier) {
			if (ends[earlier] && start < ends[earlier] && starts[earlier] < start + length) {
				error = "Overlapping code segments";
				return false;
			}
		}
		starts[slot] = start;
		ends[slot] = start + length;
		PCodeSegment s;
		for (uint j = 0; j < 8; ++j) {
			byte ch = data[0x40 + 8 * slot + j];
			if (!ch)
				break;
			if (ch < 0x20 || ch > 0x7e) {
				error = "Non-ASCII segment name";
				return false;
			}
			s.name += char(ch);
		}
		while (!s.name.empty() && s.name.lastChar() == ' ')
			s.name.deleteLastChar();
		while (!s.name.empty() && s.name[0] == ' ')
			s.name.deleteChar(0);
		s.code = Common::Array<byte>(data.data() + start, length);
		s.number = s.code[length - 2];
		const uint count = s.code[length - 1];
		if (!s.number || !count || 2 + 2 * count > length) {
			error = "Invalid segment procedure table";
			return false;
		}
		for (uint j = 0; j < result.size(); ++j) {
			if (result[j].number == s.number) {
				error = "Duplicate segment number";
				return false;
			}
		}
		s.procedures.resize(count + 1);
		const uint tableStart = length - 2 - 2 * count;
		for (uint number = 1; number <= count; ++number) {
			const int pointer = length - 2 - 2 * number;
			const int anchor = pointer - READ_LE_UINT16(s.code.data() + pointer);
			if (anchor < 8 || anchor + 2 > int(tableStart)) {
				error = Common::String::format("Invalid procedure anchor in %s:%u", s.name.c_str(), number);
				return false;
			}
			PCodeProcedure &p = s.procedures[number];
			p.anchor = anchor;
			p.localBytes = READ_LE_UINT16(s.code.data() + anchor - 8);
			p.parameterBytes = READ_LE_UINT16(s.code.data() + anchor - 6);
			const int entry = anchor - 2 - READ_LE_UINT16(s.code.data() + anchor - 2);
			const int exit = anchor - 4 - READ_LE_UINT16(s.code.data() + anchor - 4);
			if (entry < 0 || entry >= anchor - 8 || exit < 0 || exit >= anchor - 8 ||
				((p.localBytes | p.parameterBytes) & 1) ||
				uint32(p.localBytes) + p.parameterBytes > 0x9000 || s.code[anchor] != number) {
				error = Common::String::format("Invalid procedure attributes in %s:%u", s.name.c_str(), number);
				return false;
			}
			p.entry = entry;
			p.exit = exit;
			p.level = s.code[anchor + 1];
		}
		result.push_back(Common::move(s));
	}
	if (result.empty()) {
		error = "SYSTEM.PASCAL has no code segments";
		return false;
	}
	_segments = Common::move(result);
	_checksum = wizardryCrc32(data.data(), data.size());
	return true;
}

const PCodeSegment *PCodeFile::segment(byte number) const {
	for (uint i = 0; i < _segments.size(); ++i)
		if (_segments[i].number == number)
			return &_segments[i];
	return nullptr;
}

const PCodeProcedure *PCodeFile::procedure(byte sn, byte pn) const {
	const PCodeSegment *s = segment(sn);
	return s && pn && pn < s->procedures.size() ? &s->procedures[pn] : nullptr;
}

} // namespace Wizardry
