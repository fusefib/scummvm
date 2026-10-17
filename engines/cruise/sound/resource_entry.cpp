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

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "cruise/cruise_main.h"
#include "cruise/dataLoader.h"

namespace Cruise {

int loadSPLSub(uint8 *ptr, uint32 decodedSize, const char *name,
		int sourceIdx, int destIdx, Common::Platform platform) {
	int fileIndex;
	const byte *sampleData = ptr;
	uint32 sampleSize = decodedSize;
	int sampleRate = 0;

	if (!ptr || !name || decodedSize == 0)
		return -1;

	if (platform == Common::kPlatformAmiga) {
		if (decodedSize < 10 || READ_BE_UINT32(ptr) != MKTAG('S', 'M', 'P', 0)) {
			warning("Malformed Amiga SMP resource '%s'", name);
			return -1;
		}
		sampleSize = READ_BE_UINT32(ptr + 4);
		if (sampleSize == 0 || (sampleSize & 1) != 0 ||
				sampleSize + 10 != decodedSize) {
			warning("Malformed Amiga SMP payload '%s' (%u of %u bytes)",
					name, sampleSize, decodedSize);
			return -1;
		}
		sampleRate = READ_BE_UINT16(ptr + 8);
		sampleData = ptr + 10;
	} else if (platform == Common::kPlatformAtariST) {
		if (decodedSize < 6 || READ_BE_UINT32(ptr) != MKTAG('S', 'P', 'L', 0)) {
			warning("Malformed Atari SPL resource '%s'", name);
			return -1;
		}
		sampleSize = READ_BE_UINT16(ptr + 4);
		if (sampleSize == 0 || sampleSize + 6 != decodedSize) {
			warning("Malformed Atari SPL payload '%s' (%u of %u bytes)",
					name, sampleSize, decodedSize);
			return -1;
		}
		sampleRate = 6400;
		sampleData = ptr + 6;
	}

	if (sampleSize > 0xffff) {
		warning("Oversized sound resource '%s' (%u bytes)", name, sampleSize);
		return -1;
	}

	if (destIdx == -1)
		fileIndex = createResFileEntry(sampleSize, 1, sampleSize, 1);
	else
		fileIndex = updateResFileEntry(sampleSize, 1, sampleSize, destIdx, 1);

	if (fileIndex < 0)
		error("Unable to load SPL resource");

	uint8 *destPtr = filesDatabase[fileIndex].subData.ptr;
	Common::copy(sampleData, sampleData + sampleSize, destPtr);
	filesDatabase[fileIndex].subData.resourceType = OBJ_TYPE_SOUND;
	filesDatabase[fileIndex].subData.index = sourceIdx;
	Common::strlcpy(filesDatabase[fileIndex].subData.name, name,
			sizeof(filesDatabase[fileIndex].subData.name));
	if (platform == Common::kPlatformAmiga || platform == Common::kPlatformAtariST)
		filesDatabase[fileIndex].subData.transparency = sampleRate;

	return 1;
}

uint32 getSoundResourceSize(int entryIdx) {
	if (entryIdx < 0 || entryIdx >= NUM_FILE_ENTRIES ||
			filesDatabase[entryIdx].subData.resourceType != OBJ_TYPE_SOUND)
		return 0;
	return filesDatabase[entryIdx].height;
}

} // End of namespace Cruise
