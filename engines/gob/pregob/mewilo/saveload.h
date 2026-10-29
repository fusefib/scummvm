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

#include "common/str.h"
#include "common/stream.h"

#include "gob/pregob/mewilo/executor.h"
#include "gob/pregob/mewilo/platform.h"
#include "gob/pregob/mewilo/script_patches.h"

namespace Gob {
namespace Mewilo {

struct MewiloSavedRegion {
	int16 x;
	int16 y;
	uint16 width;
	uint16 height;
	Common::Array<byte> pixels;

	MewiloSavedRegion() : x(0), y(0), width(0), height(0) {}
};

/** Current MEWS state, including its platform and complete resource identity. */
struct MewiloSaveState {
	byte platform;
	byte language; ///< 0: French, 1: German.
	byte module;   ///< Index into intro, map, 1..5, 7, 8.
	bool exactImages;
	bool cursorVisible;
	uint32 policy;
	uint32 randomSeed;
	uint16 recipeRevision;
	byte releaseMd5[16];
	char sourceMd5[33];
	char effectiveMd5[33];
	uint32 sourceSize;
	uint32 effectiveSize;
	MewiloExecState executor;
	Common::Array<byte> framebuffer; // 320 x 200 indexed pixels; destination palette remains native
	Common::Array<byte> scratch;     // native Atari persistent packed-image scratch, or empty
	Common::Array<MewiloSavedRegion> regions;

	MewiloSaveState();
};

/** Session identity from all nine prepared scripts, banks, font and replacements.
 * Resource reopening must not relabel bytes already loaded by the host.
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

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_SAVELOAD_H
