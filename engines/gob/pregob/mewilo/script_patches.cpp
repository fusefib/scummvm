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

enum ResourceId { kUnknownResource, kGermanTown, kGermanHouse, kGermanHen, kGermanEnd };

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
		k68kPlatforms, Common::DE_DEU },
	{ kGermanHouse, "LOAD2", 14272, "9ce785b84f4412b91f666903b852e473",
		k68kPlatforms, Common::DE_DEU },
	{ kGermanHen, "LOAD5", 18760, "2d68722a92c25d9499f3fb7a553505db",
		k68kPlatforms, Common::DE_DEU },
	{ kGermanEnd, "LOAD8", 17114, "e8046f82c18ff92f5fce83c7e46f6f8b",
		k68kPlatforms, Common::DE_DEU },
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

struct InputCorrection {
	ResourceId resource;
	uint16 offset;
	uint16 nodeSize;
	const char *originalReject;
	uint16 originalRejectSize;
	const char *originalAnswers;
	uint16 originalAnswerSize;
	const char *alias; // Append; never remove a previously accepted alternative.
	const char *reject;
};

// S05/S07: shortening the translated rejection string makes room for aliases
// INSIDE the same node. Field-descriptor offsets change locally, but the input
// node's address, total size, branch destinations, attempts and controls do not.
// The four one-attempt messages are also translated, although their retry text
// is not normally displayed by the current executor.
static const InputCorrection kInputs[] = {
	{ kGermanTown, 0x1632, 87,
		"Non c'est pas \032\030\000", 17,
		"gombo/gombos/ein gombo/das gombos/das gombo\000", 44, "", "Falsch!" },
	{ kGermanTown, 0x1984, 74,
		"c'est pas \032\030\000", 13,
		"M\026wilo/MEWILO/m\026wilo/mewilo/Mewilo\000", 35, "", "Falsch!" },
	{ kGermanHouse, 0x133B, 53,
		"faux!\000", 6,
		"ARNAUD/arnaud/Arnaud\000", 21, "", "Nein!" },
	{ kGermanHen, 0x07CA, 83,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"die tuer/eine tuer/tueren\000", 26, "tuer", "Falsch!" },
	{ kGermanHen, 0x081D, 99,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"die glocke/die glocken/eine glocke/glocke\000", 42, "", "Falsch!" },
	{ kGermanHen, 0x0880, 91,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"die nadel/eine nadel/nadeln/nadel\000", 34, "", "Falsch!" },
	{ kGermanHen, 0x08DB, 87,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"der fuss/fuesse/ein fuss/fuss\000", 30, "", "Falsch!" },
	{ kGermanHen, 0x0932, 92,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"der schatten/ein schatten/schatten\000", 35, "", "Falsch!" },
	{ kGermanHen, 0x098E, 86,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"das auge/augen/ein auge/auge\000", 29, "", "Falsch!" },
	{ kGermanHen, 0x0A35, 90,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"der nagel/ein nagel/naegel/nagel\000", 33, "", "Falsch!" },
	{ kGermanHen, 0x0A8F, 97,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"die schere/ein schere/schere/die schere\000", 40, "", "Falsch!" },
	{ kGermanHen, 0x0AF0, 91,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"der fluss/ein fluss/fluesse/fluss\000", 34, "", "Falsch!" },
	{ kGermanHen, 0x0B4B, 99,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"die briefmarke/ein briefmarke/briefmarken\000", 42, "briefmarke", "Falsch!" },
	{ kGermanHen, 0x0BAE, 114,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"ein kartenspiel/das kartenspiel/kartenspiele/kartenspiel\000", 57, "", "Falsch!" },
	{ kGermanHen, 0x0C20, 95,
		"         C'est pas \032\030, pi\027ce !\000", 31,
		"eine pfeife/pfeife/pfeifen/die pfeife\000", 38, "", "Falsch!" },
	{ kGermanEnd, 0x02A9, 87,
		"       pas de chance\000", 21,
		"anselme und echevin/echevin und anselme\000", 40, "", "Falsch!" },
};

static bool correctInput(const Common::Array<byte> &source, Common::Array<byte> &target,
		const InputCorrection &r) {
	const uint32 at = r.offset;
	if (!contains(source, at, r.nodeSize) || source[at] != 0x12)
		return false;
	const uint32 oldRejectSize = source[at + 14];
	const uint32 fields = at + 15 + oldRejectSize;
	if (!contains(source, fields, 11) || source[fields] != 1)
		return false;
	const uint32 answers = fields + 11;
	const uint32 oldAnswerSize = source[fields + 10];
	if (!oldAnswerSize || answers + oldAnswerSize != at + r.nodeSize)
		return false;
	if (oldRejectSize != r.originalRejectSize || oldAnswerSize != r.originalAnswerSize ||
		!expect(source, at + 15, r.originalReject, oldRejectSize) ||
		!expect(source, answers, r.originalAnswers, oldAnswerSize))
		return false;

	// Stored byte strings may contain extra zero padding. Insert the alias before
	// the FIRST NUL, retaining the complete original span and all its padding.
	uint32 textEnd = 0;
	while (textEnd < oldAnswerSize && source[answers + textEnd])
		++textEnd;
	if (textEnd == oldAnswerSize)
		return false;
	const uint32 extra = r.alias[0] ? strlen(r.alias) + 1 : 0; // '/' + alias
	if (extra > oldRejectSize || strlen(r.reject) + 1 > oldRejectSize - extra ||
		oldAnswerSize + extra > 255)
		return false;

	const uint32 newRejectSize = oldRejectSize - extra;
	const uint32 newFields = at + 15 + newRejectSize;
	target[at + 14] = (byte)newRejectSize;
	memset(target.data() + at + 15, 0, newRejectSize);
	memcpy(target.data() + at + 15, r.reject, strlen(r.reject));
	memcpy(target.data() + newFields, source.data() + fields, 11);
	target[newFields + 10] = (byte)(oldAnswerSize + extra);
	const uint32 newAnswers = newFields + 11;
	memcpy(target.data() + newAnswers, source.data() + answers, textEnd);
	if (extra) {
		target[newAnswers + textEnd] = '/';
		memcpy(target.data() + newAnswers + textEnd + 1, r.alias, extra - 1);
	}
	memcpy(target.data() + newAnswers + textEnd + extra,
		source.data() + answers + textEnd, oldAnswerSize - textEnd);
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
		for (uint i = 0; ok && i < ARRAYSIZE(kInputs); ++i) {
			if (kInputs[i].resource != id)
				continue;
			ok = correctInput(source, candidate, kInputs[i]);
			prepared.appliedMask |= kMewiloFixGermanRejections;
			if (kInputs[i].alias[0])
				prepared.appliedMask |= kMewiloFixGermanAliases;
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
