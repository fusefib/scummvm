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
#include "common/md5.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "gob/pregob/mewilo/script_patches.h"
#include "gob/pregob/mewilo/tot.h"

namespace Gob {
namespace Mewilo {

MewiloScriptPatchReport::MewiloScriptPatchReport() {
	memset(this, 0, sizeof(*this));
	recipeRevision = kRecipeRevision;
}

namespace {

enum ResourceId { kUnknownResource, kGermanTown };

struct ResourceIdentity {
	ResourceId id;
	const char *stem;
	uint32 size;
	const char *md5;
	uint32 platforms;
	Common::Language language;
};

// Full French/German scripts are the retained Amiga bytes. Atari is permitted
// ONLY for byte-identical normalized resources; this does not repair detection.
static const uint32 k68kPlatforms = (1U << kMewiloPlatformAmiga) |
	(1U << kMewiloPlatformAtariST);

static const ResourceIdentity kResources[] = {
	{ kGermanTown, "LOAD1", 18657, "92145781364cb2fc96b5634e2e9b2405",
		k68kPlatforms, Common::DE_DEU }
};

static bool contains(const Common::Array<byte> &data, uint32 at, uint32 count) {
	return at <= data.size() && count <= data.size() - at;
}

static bool expect(const Common::Array<byte> &data, uint32 at, const void *bytes, uint32 count) {
	return contains(data, at, count) && memcmp(data.data() + at, bytes, count) == 0;
}

static bool digest(const Common::Array<byte> &data, char out[33]) {
	Common::MemoryReadStream stream(data.data(), data.size(), DisposeAfterUse::NO);
	byte bytes[16];
	if (!Common::computeStreamMD5(stream, bytes))
		return false;
	static const char hex[] = "0123456789abcdef";
	for (uint i = 0; i < 16; ++i) {
		out[2 * i] = hex[bytes[i] >> 4];
		out[2 * i + 1] = hex[bytes[i] & 15];
	}
	out[32] = 0;
	return true;
}

static bool validate(const Common::Array<byte> &data, MewiloPlatform platform) {
	Common::MemoryReadStream stream(data.data(), data.size(), DisposeAfterUse::NO);
	MewiloTotData tot(stream, platform);
	MewiloGraphStats stats;
	return tot.valid() && tot.validateReachableGraph(stats);
}

static ResourceId identify(const char *stem, uint32 size, const char *md5,
		MewiloPlatform platform, Common::Language language) {
	if ((uint)platform > (uint)kMewiloPlatformThomsonTO)
		return kUnknownResource;
	for (uint i = 0; i < ARRAYSIZE(kResources); ++i) {
		const ResourceIdentity &r = kResources[i];
		if (r.size == size && (r.platforms & (1U << platform)) &&
			r.language == language && !strcmp(r.stem, stem) && !strcmp(r.md5, md5))
			return r.id;
	}
	return kUnknownResource;
}

// S04: same text-node header, four rows, <=30 cells per row, unchanged NUL
// position. Correct the relation "named BY Arnaud", not the MEWILO answer.
static const byte kLetterOriginal[] = "\035\005\010\036 \010\000\003\000\000 DAMIT ICH IHN DIR GEBE,MUSST@DU ERST DEN NAMEN EINES OPFERS@NENNEN,DAS ALS ARNAUD DE RONAN@        BEKANNT IST.\000";
static const char kLetterCorrected[] = "NENNE DEN NAMEN DES OPFERS, @DAS ARNAUD DE RONAN IN      @SEINEM BRIEF NENNT. DANN    @BEKOMMST DU DEN BRIEF.   ";



static bool correctLetter(const Common::Array<byte> &source, Common::Array<byte> &target) {
	if (sizeof(kLetterOriginal) - 1 != 10 + sizeof(kLetterCorrected) ||
		!expect(source, 0x11BC, kLetterOriginal, sizeof(kLetterOriginal) - 1))
		return false;
	memcpy(target.data() + 0x11C6, kLetterCorrected, sizeof(kLetterCorrected));
	return true;
}

} // End of anonymous namespace

bool prepareMewiloScript(Common::SeekableReadStream &normalizedStream,
		const char *moduleStem, MewiloPlatform platform, Common::Language language,
		uint32 policy, Common::Array<byte> &output, MewiloScriptPatchReport &report) {
	output.clear();
	report = MewiloScriptPatchReport();
	if (!moduleStem || (policy & ~kMewiloKnownScriptPatchPolicies))
		return false;
	const int64 size = normalizedStream.size();
	if (size < 16 || size > 0xFFFF || !normalizedStream.seek(0))
		return false;
	Common::Array<byte> source;
	source.resize((uint32)size);
	if (normalizedStream.read(source.data(), source.size()) != source.size() || normalizedStream.err() ||
		!validate(source, platform))
		return false;

	MewiloScriptPatchReport prepared;
	prepared.policy = policy;
	prepared.sourceSize = source.size();
	if (!digest(source, prepared.sourceMd5))
		return false;
	const ResourceId id = identify(moduleStem, source.size(), prepared.sourceMd5, platform, language);
	prepared.recognized = id != kUnknownResource;
	Common::Array<byte> candidate = source;
	bool ok = true;

	if (policy & kMewiloPatchGermanText) {
		if (id == kGermanTown) {
			ok = correctLetter(source, candidate);
			prepared.appliedMask |= kMewiloFixLetterWording;
			++prepared.changedSites;
		}
	}
	if (!ok || (prepared.appliedMask && !validate(candidate, platform))) {
		warning("Mewilo: refusing failed script correction in %s.TOT", moduleStem);
		return false;
	}
	prepared.effectiveSize = candidate.size();
	if (!digest(candidate, prepared.effectiveMd5))
		return false;
	output = candidate;
	report = prepared;
	return true;
}

} // End of namespace Mewilo
} // End of namespace Gob
