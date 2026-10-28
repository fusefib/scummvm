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

#ifndef GOB_PREGOB_MEWILO_SAVELOAD_H
#define GOB_PREGOB_MEWILO_SAVELOAD_H

#include "common/stream.h"
#include "common/str.h"
#include "gob/pregob/mewilo/executor.h"
#include "gob/pregob/mewilo/script_patches.h"

namespace Gob {
namespace Mewilo {

struct MewiloSavedRegion {
	int16 x, y;
	uint16 width, height;
	Common::Array<byte> pixels;
	MewiloSavedRegion() : x(0), y(0), width(0), height(0) {}
};

/** One forward-only wire format; explicit platform and resource-set identity.
 * No serialized C++ layout, filenames supplied by the save, or raw pointers.
 */
struct MewiloSaveState {
	byte platform, language, module; // language: 0 French, 1 German; module: indices into intro,map,1..5,7,8; intro allowed at explicit continue prompts
	bool exactImages, cursorVisible;
	uint32 policy, randomSeed;
	uint16 recipeRevision;
	byte releaseMd5[16];
	char sourceMd5[33], effectiveMd5[33];
	uint32 sourceSize, effectiveSize;
	MewiloExecState executor;
	Common::Array<byte> framebuffer; // 320 x 200 indexed pixels; destination palette remains native
	Common::Array<byte> scratch;     // native Atari persistent packed-image scratch, or empty
	Common::Array<MewiloSavedRegion> regions;
	MewiloSaveState();
};

/** Run-wide identity, computed from all nine effective scripts, banks and font.
 * These reports describe resident/prepared bytes, never a later substitute.
 */
struct MewiloReleaseIdentity {
	byte digest[16];
	MewiloScriptPatchReport scripts[9]; // intro, map, 1..5,7,8
	byte banks[9][16];
	bool exactImages;
	MewiloReleaseIdentity();
};

bool validateMewiloSave(const MewiloSaveState &state);
bool encodeMewiloSave(const MewiloSaveState &state, Common::WriteStream &stream);
bool decodeMewiloSave(Common::SeekableReadStream &stream, MewiloSaveState &state, Common::String &reason);
bool mewiloSaveCompatible(const MewiloSaveState &state, MewiloPlatform platform,
	byte language, uint32 policy, const MewiloReleaseIdentity &release, Common::String &reason);

} // namespace Mewilo
} // namespace Gob
#endif
