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

#ifndef GOB_PREGOB_MEWILO_PLATFORM_H
#define GOB_PREGOB_MEWILO_PLATFORM_H

#include "common/scummsys.h"
#include "common/str.h"
#include "common/stream.h"

namespace Gob {
namespace Mewilo {

enum MewiloPlatform {
	kMewiloPlatformAtariST,
	kMewiloPlatformAmstradCPC,
	kMewiloPlatformAmiga,
	kMewiloPlatformThomsonTO
};

struct MewiloPlatformProfile {
	MewiloPlatform platform;
	const char *modulePrefix;
	byte packedPixelsPerCell;
	byte packedOutputScaleX;
	bool persistentI00Scratch;
	bool loadOptionalI01;
};

// Takes ownership even when the input has no valid AMSDOS wrapper.
Common::SeekableReadStream *normalizeMewiloCpcResource(Common::SeekableReadStream *stream);

bool isValidMewiloPlatform(MewiloPlatform platform);
const MewiloPlatformProfile &getMewiloPlatformProfile(MewiloPlatform platform);
void buildMewiloPalette(MewiloPlatform platform, byte palette[16 * 3]);
int mewiloModuleIndex(const Common::String &stem, const char *prefix);
Common::String mewiloModuleStem(uint index, const char *prefix);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_PLATFORM_H
