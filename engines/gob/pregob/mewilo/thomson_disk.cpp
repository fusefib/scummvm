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
#include "common/memstream.h"
#include "gob/pregob/mewilo/thomson_disk.h"

namespace Gob {
namespace Mewilo {

static bool diskName(const byte *bytes, uint size, Common::String &out) {
	while (size && bytes[size - 1] == ' ') --size;
	out.clear();
	for (uint i = 0; i < size; ++i) {
		const byte c = bytes[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return false;
		out += char(c);
	}
	return true;
}

bool MewiloThomsonDisk::load(Common::SeekableReadStream &source, Common::String &reason) {
	// Failed reloads must not leave an older disk silently usable.
	clear();
	reason = "Expected a raw 327680-byte, single-sided 80-track Thomson FD";
	if (source.size() != 80 * 16 * 256 || !source.seek(0)) return false;
	Common::Array<byte> disk;
	disk.resize(80 * 16 * 256);
	if (source.read(disk.data(), disk.size()) != disk.size() || source.err()) return false;
	const byte *fat = disk.data() + 20 * 4096 + 256;
	const byte *directory = fat + 256;
	bool allocated[160] = {};
	Common::Array<File> candidate;
	if (fat[0] != 0) { reason = "Invalid Thomson FAT header"; return false; }
	for (uint entry = 0; entry < 112; ++entry) {
		const byte *e = directory + entry * 32;
		if (e[0] == 0xFF) break;
		if (e[0] == 0) continue; // deleted entry, not end of directory
		File f;
		Common::String ext;
		reason = "Invalid or duplicate Thomson filename";
		if (!diskName(e, 8, f.name) || f.name.empty() || !diskName(e + 8, 3, ext)) return false;
		if (!ext.empty()) f.name += Common::String(".") + ext;
		for (uint i = 0; i < candidate.size(); ++i)
			if (candidate[i].name == f.name) return false;
		f.type = e[11]; f.format = e[12]; f.firstBlock = e[13];
		f.finalSectorBytes = READ_BE_UINT16(e + 14);
		uint payload = 0;
		if (f.type == 1 && f.format == 255 && (ext == "TOT" || ext == "I00" || ext == "I01"))
			payload = 256;
		else if ((f.type == 0 || f.type == 2) && f.format == 0)
			payload = 255;
		reason = Common::String::format("%s: unsupported sector contract or final length", f.name.c_str());
		if (!payload || !f.finalSectorBytes || f.finalSectorBytes > payload) return false;
		uint block = f.firstBlock;
		for (;;) {
			reason = Common::String::format("%s: invalid, overlapping or cyclic FAT chain", f.name.c_str());
			if (block >= 160 || block == 40 || block == 41 || allocated[block]) return false;
			allocated[block] = true;
			f.blocks.push_back(byte(block));
			const byte next = fat[block + 1];
			const bool last = next >= 0xC1 && next <= 0xC8;
			if (!last && next >= 160) return false;
			const uint sectors = last ? next - 0xC0 : 8;
			for (uint i = 0; i < sectors; ++i) {
				const uint size = last && i + 1 == sectors ? f.finalSectorBytes : payload;
				const uint offset = f.data.size();
				f.data.resize(offset + size);
				memcpy(f.data.data() + offset, disk.data() + block * 2048 + i * 256, size);
			}
			if (last) break;
			block = next;
		}
		candidate.push_back(f);
	}
	if (candidate.empty()) { reason = "Empty Thomson directory"; return false; }
	_files.swap(candidate);
	reason.clear();
	return true;
}

Common::SeekableReadStream *MewiloThomsonDisk::openFile(const Common::String &name) const {
	for (uint i = 0; i < _files.size(); ++i) {
		const File &f = _files[i];
		if (!f.name.equalsIgnoreCase(name)) continue;
		byte *copy = (byte *)malloc(f.data.size());
		if (!copy) return nullptr;
		memcpy(copy, f.data.data(), f.data.size());
		return new Common::MemoryReadStream(copy, f.data.size(), DisposeAfterUse::YES);
	}
	return nullptr;
}

} // namespace Mewilo
} // namespace Gob
