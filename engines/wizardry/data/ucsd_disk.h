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

#ifndef WIZARDRY_DATA_UCSD_DISK_H
#define WIZARDRY_DATA_UCSD_DISK_H

#include "common/array.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace Wizardry {

/** A linear, 512-byte-block DOS UCSD volume, not an Apple II sector image.
 *
 * The distribution WIZ1--4 images are physically shorter than the volume
 * size in their headers. Their last filler files extend beyond EOF. Keep
 * this metadata, but never return a partially available file as complete.
 * A failed load leaves the previously loaded disk unchanged.
 */
class UcsdDisk {
public:
	enum { kBlockSize = 512,
		   kDirectoryBlock = 2,
		   kEntrySize = 26 };

	struct Entry {
		Common::String name;
		uint16 firstBlock;
		uint16 endBlock;
		uint16 kind;
		uint16 lastBlockBytes;
		uint16 directoryIndex;
		bool available;

		Entry() : firstBlock(0), endBlock(0), kind(0), lastBlockBytes(0),
				  directoryIndex(0), available(false) {}
		uint32 size() const;
	};

	UcsdDisk() : _nominalBlocks(0) {}
	bool load(Common::SeekableReadStream &stream, Common::String &error);
	bool loadBytes(const Common::Array<byte> &bytes, Common::String &error);

	const Common::String &volumeName() const { return _name; }
	uint32 physicalBlocks() const { return _bytes.size() / kBlockSize; }
	uint16 nominalBlocks() const { return _nominalBlocks; }
	const Common::Array<Entry> &entries() const { return _entries; }
	const Entry *find(const Common::String &name) const;
	bool readFile(const Common::String &name, Common::Array<byte> &out,
				  Common::String &error) const;
	bool readFile(const Entry &entry, Common::Array<byte> &out,
				  Common::String &error) const;
	bool readBlocks(uint32 first, uint32 count, Common::Array<byte> &out,
					Common::String &error) const;

private:
	bool parse(Common::String &error);
	Common::Array<byte> _bytes;
	Common::Array<Entry> _entries;
	Common::String _name;
	uint16 _nominalBlocks;
};

} // namespace Wizardry

#endif
