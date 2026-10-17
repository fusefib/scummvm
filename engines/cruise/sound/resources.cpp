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

#include "cruise/cruise_main.h"
#include "cruise/sound/resources.h"
#include "cruise/volume.h"

namespace Cruise {

Common::String resolveSoundResourceName(const char *name, const char *effectExtension) {
	Common::String resolved(name);
	if (resolved.hasSuffixIgnoreCase(".SPL"))
		resolved = resolved.substr(0, resolved.size() - 4) + effectExtension;
	return resolved;
}

byte *readBundleSoundFile(const char *name, uint32 *size) {
	int fileIdx = findFileInDisks(name);
	if (fileIdx < 0)
		return nullptr;

	int unpackedSize = volumePtrToFileDescriptor[fileIdx].extSize + 2;
	if (size)
		*size = volumePtrToFileDescriptor[fileIdx].extSize;
	byte *data = (byte *)MemAlloc(unpackedSize);
	assert(data);

	if (volumePtrToFileDescriptor[fileIdx].size + 2 != unpackedSize) {
		uint8 *packedBuffer = (uint8 *)mallocAndZero(volumePtrToFileDescriptor[fileIdx].size + 2);

		loadPackedFileToMem(fileIdx, packedBuffer);
		delphineUnpack(data, packedBuffer, volumePtrToFileDescriptor[fileIdx].size);
		MemFree(packedBuffer);
	} else {
		loadPackedFileToMem(fileIdx, data);
	}

	return data;
}

} // End of namespace Cruise
