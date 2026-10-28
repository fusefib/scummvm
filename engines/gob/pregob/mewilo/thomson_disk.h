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

#ifndef GOB_PREGOB_MEWILO_THOMSON_DISK_H
#define GOB_PREGOB_MEWILO_THOMSON_DISK_H

#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"

namespace Gob {
namespace Mewilo {

/** Immutable snapshot of one raw, single-sided, 80-track Thomson FD.
 *
 * DOS BASIC/BIN files carry 255 bytes per physical sector. Mewilo's
 * type-1/format-255 TOT/I00 files deliberately carry all 256 bytes.
 * No malformed-extraction fallback, disk writes, global SearchMan mount or
 * dependency on the source stream's lifetime is involved.
 */
class MewiloThomsonDisk {
public:
	struct File {
		Common::String name;
		byte type, format, firstBlock;
		uint16 finalSectorBytes;
		Common::Array<byte> blocks;
		Common::Array<byte> data;
	};

	bool load(Common::SeekableReadStream &source, Common::String &reason);
	void clear() { _files.clear(); }
	bool empty() const { return _files.empty(); }
	uint fileCount() const { return _files.size(); }
	const File &file(uint index) const { return _files[index]; }
	/** Caller owns the stream. It owns a copy and outlives clear()/destruction. */
	Common::SeekableReadStream *openFile(const Common::String &name) const;

private:
	Common::Array<File> _files;
};

} // namespace Mewilo
} // namespace Gob
#endif
