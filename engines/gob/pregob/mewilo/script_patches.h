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

#ifndef GOB_PREGOB_MEWILO_SCRIPT_PATCHES_H
#define GOB_PREGOB_MEWILO_SCRIPT_PATCHES_H

#include "common/array.h"
#include "common/language.h"
#include "common/stream.h"
#include "gob/pregob/mewilo/platform.h"

namespace Gob {
namespace Mewilo {

// Stable policy bits. Freeze at game start; do not change while a game is running.
// Original behavior is policy 0. Timing is deliberately NOT one of these flags.
enum MewiloScriptPatchPolicy {
	kMewiloPatchGermanText = 1 << 0,   // S04/S05/S07: wording, aliases and rejection text
	kMewiloPatchHenProtection = 1 << 1, // S02: refuse a hen when mongoose is held without a basket
	kMewiloKnownScriptPatchPolicies = 3
};

// Stable recipe IDs, not an indication that every recipe applies to every module.
enum MewiloScriptPatchId {
	kMewiloFixLetterWording = 1 << 0,
	kMewiloFixGermanAliases = 1 << 1,
	kMewiloFixGermanRejections = 1 << 2,
	kMewiloFixHenProtection = 1 << 3
};

/** Identity of the bytes actually prepared, NOT a later reopening of the file.
 *
 * These are full normalized-resource MD5s (not detection's 5000-byte window).
 * MD5 here is a release fingerprint, not an adversarial authenticity guarantee.
 * Future saves must additionally pin the run-wide policy and recipe revision;
 * appliedMask alone is insufficient (it describes only this resident module).
 */
struct MewiloScriptPatchReport {
	enum { kRecipeRevision = 2 };
	char sourceMd5[33];
	char effectiveMd5[33];
	uint32 sourceSize;
	uint32 effectiveSize;
	uint32 policy;
	uint32 appliedMask;
	uint32 changedSites;
	uint16 recipeRevision;
	bool recognized;

	MewiloScriptPatchReport();
};

/** Prepare a privately owned normalized TOT for the existing Mewilo parser.
 *
 * Caller must remove any AMSDOS wrapper first and supply the canonical uppercase
 * module stem (without .TOT). No file is written. Unknown identities/platforms/
 * languages pass through unchanged; a known recipe guard/validation failure
 * returns false, with output and report cleared (no partially patched result).
 *
 * Original and resulting graphs are validated. Original node/root offsets stay
 * fixed; the optional inventory/gate recipes append ordinary native conditions
 * and redirect one verified branch. No new VM opcode or serialized dialect.
 */
bool prepareMewiloScript(Common::SeekableReadStream &normalizedStream,
	const char *moduleStem, MewiloPlatform platform, Common::Language language,
	uint32 policy, Common::Array<byte> &output, MewiloScriptPatchReport &report);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_SCRIPT_PATCHES_H
