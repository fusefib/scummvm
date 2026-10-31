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

#include "wizardry/data/ucsd_disk.h"
#include "common/endian.h"
#include "common/stream.h"

namespace Wizardry {

uint32 UcsdDisk::Entry::size() const {
	if (endBlock <= firstBlock)
		return 0;
	return uint32(endBlock - firstBlock - 1) * kBlockSize + lastBlockBytes;
}

static bool validDiskName(const byte *name, uint length) {
	for (uint i = 0; i < length; ++i) {
		if (name[i] < 32 || name[i] > 126 || name[i] == '/' || name[i] == '\\' || name[i] == ':')
			return false;
	}
	return length != 0;
}

bool UcsdDisk::load(Common::SeekableReadStream &stream, Common::String &error) {
	const int64 length = stream.size();
	if (length < 3 * kBlockSize || length % kBlockSize || length > int64(65535) * kBlockSize) {
		error = "UCSD image has an invalid or excessive size";
		return false;
	}
	Common::Array<byte> bytes;
	bytes.resize(uint32(length));
	if (!stream.seek(0) || stream.read(bytes.data(), bytes.size()) != bytes.size() || stream.err()) {
		error = "Cannot read the complete UCSD image";
		return false;
	}
	return loadBytes(bytes, error);
}

bool UcsdDisk::loadBytes(const Common::Array<byte> &bytes, Common::String &error) {
	UcsdDisk candidate;
	candidate._bytes = bytes;
	if (!candidate.parse(error))
		return false;
	*this = Common::move(candidate);
	error.clear();
	return true;
}

bool UcsdDisk::parse(Common::String &error) {
	if (_bytes.size() < 3 * kBlockSize || _bytes.size() % kBlockSize ||
		_bytes.size() > uint32(65535) * kBlockSize) {
		error = "UCSD image must contain complete 512-byte blocks and a directory";
		return false;
	}
	const byte *directory = _bytes.data() + kDirectoryBlock * kBlockSize;
	const uint16 directoryEnd = READ_LE_UINT16(directory + 2);
	_nominalBlocks = READ_LE_UINT16(directory + 14);
	const uint16 fileCount = READ_LE_UINT16(directory + 16);
	if (READ_LE_UINT16(directory) != 0 || READ_LE_UINT16(directory + 4) != 0 ||
		directoryEnd <= kDirectoryBlock || directoryEnd > physicalBlocks() ||
		_nominalBlocks < directoryEnd || directory[6] > 7 ||
		!validDiskName(directory + 7, directory[6])) {
		error = "Invalid UCSD volume header";
		return false;
	}
	const uint32 directoryBytes = uint32(directoryEnd - kDirectoryBlock) * kBlockSize;
	if (uint32(fileCount + 1) * kEntrySize > directoryBytes) {
		error = "UCSD file count exceeds the directory extent";
		return false;
	}
	_name = Common::String(reinterpret_cast<const char *>(directory + 7), directory[6]);
	for (uint i = 1; i <= fileCount; ++i) {
		const byte *record = directory + i * kEntrySize;
		Entry entry;
		entry.firstBlock = READ_LE_UINT16(record);
		entry.endBlock = READ_LE_UINT16(record + 2);
		entry.kind = READ_LE_UINT16(record + 4);
		entry.lastBlockBytes = READ_LE_UINT16(record + 22);
		entry.directoryIndex = i;
		if (record[6] > 15 || !validDiskName(record + 7, record[6]) ||
			entry.firstBlock < directoryEnd || entry.endBlock < entry.firstBlock ||
			entry.endBlock > _nominalBlocks || entry.lastBlockBytes > kBlockSize ||
			(entry.firstBlock == entry.endBlock && entry.lastBlockBytes != 0) ||
			(entry.kind & 15) > 8) {
			error = Common::String::format("Invalid UCSD directory entry %u", i);
			return false;
		}
		entry.name = Common::String(reinterpret_cast<const char *>(record + 7), record[6]);
		for (uint j = 0; j < _entries.size(); ++j) {
			const Entry &previous = _entries[j];
			if (entry.name.equalsIgnoreCase(previous.name)) {
				error = Common::String::format("Duplicate UCSD filename: %s", entry.name.c_str());
				return false;
			}
			if (entry.firstBlock < previous.endBlock && previous.firstBlock < entry.endBlock &&
				entry.firstBlock != entry.endBlock && previous.firstBlock != previous.endBlock) {
				error = Common::String::format("Overlapping UCSD extents: %s and %s",
											   previous.name.c_str(), entry.name.c_str());
				return false;
			}
		}
		const uint32 start = uint32(entry.firstBlock) * kBlockSize;
		entry.available = start <= _bytes.size() && entry.size() <= _bytes.size() - start;
		_entries.push_back(entry);
	}
	return true;
}

const UcsdDisk::Entry *UcsdDisk::find(const Common::String &name) const {
	for (uint i = 0; i < _entries.size(); ++i) {
		if (_entries[i].name.equalsIgnoreCase(name))
			return &_entries[i];
	}
	return nullptr;
}

bool UcsdDisk::readFile(const Common::String &name, Common::Array<byte> &out,
						Common::String &error) const {
	const Entry *entry = find(name);
	if (!entry) {
		error = Common::String::format("UCSD file not found: %s", name.c_str());
		return false;
	}
	return readFile(*entry, out, error);
}

bool UcsdDisk::readFile(const Entry &entry, Common::Array<byte> &out,
						Common::String &error) const {
	// Do not trust a caller-supplied or subsequently edited Entry object.
	const Entry *stored = find(entry.name);
	if (!stored || stored->directoryIndex != entry.directoryIndex ||
		stored->firstBlock != entry.firstBlock || stored->endBlock != entry.endBlock ||
		stored->lastBlockBytes != entry.lastBlockBytes) {
		error = "Entry does not belong to this UCSD volume";
		return false;
	}
	if (!stored->available) {
		error = Common::String::format("UCSD file extends beyond the physical image: %s", stored->name.c_str());
		return false;
	}
	Common::Array<byte> result;
	result.resize(stored->size());
	if (!result.empty())
		memcpy(result.data(), _bytes.data() + uint32(stored->firstBlock) * kBlockSize, result.size());
	out = Common::move(result);
	error.clear();
	return true;
}

bool UcsdDisk::readBlocks(uint32 first, uint32 count, Common::Array<byte> &out,
						  Common::String &error) const {
	if (first > physicalBlocks() || count > physicalBlocks() - first) {
		error = "UCSD block read exceeds the physical image";
		return false;
	}
	Common::Array<byte> result;
	result.resize(count * kBlockSize);
	if (!result.empty())
		memcpy(result.data(), _bytes.data() + first * kBlockSize, result.size());
	out = Common::move(result);
	error.clear();
	return true;
}

} // namespace Wizardry
