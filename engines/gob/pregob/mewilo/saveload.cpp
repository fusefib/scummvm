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

#include "common/memstream.h"
#include "common/md5.h"
#include "gob/pregob/mewilo/saveload.h"

namespace Gob {
namespace Mewilo {

static const uint32 kMaxPayload = 4 * 1024 * 1024;
static const byte kMagic[4] = { 'M', 'E', 'W', 'S' };
static const uint16 kVersion = 2;

MewiloSaveState::MewiloSaveState() : platform(0), language(0), module(0), exactImages(false),
	cursorVisible(true), policy(0), randomSeed(0), recipeRevision(MewiloScriptPatchReport::kRecipeRevision),
	sourceSize(0), effectiveSize(0) {
	memset(releaseMd5, 0, sizeof(releaseMd5));
	memset(sourceMd5, 0, sizeof(sourceMd5));
	memset(effectiveMd5, 0, sizeof(effectiveMd5));
}
MewiloReleaseIdentity::MewiloReleaseIdentity() : exactImages(true) {
	memset(digest, 0, sizeof(digest));
	memset(banks, 0, sizeof(banks));
}
static bool validHex(const char *hash) {
	for (uint i = 0; i < 32; ++i)
		if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return false;
	return hash[32] == 0;
}
bool validateMewiloSave(const MewiloSaveState &s) {
	if (s.platform > kMewiloPlatformThomsonTO || s.language > 1 || s.module > 8 ||
		(s.policy & ~kMewiloKnownScriptPatchPolicies) || s.recipeRevision != MewiloScriptPatchReport::kRecipeRevision ||
		!validHex(s.sourceMd5) || !validHex(s.effectiveMd5) || s.sourceSize < 16 || s.sourceSize > 65535 ||
		s.effectiveSize < 16 || s.effectiveSize > 65535 || s.framebuffer.size() != 64000 ||
		s.executor.frames.empty() || s.executor.frames.size() > 256 || s.regions.size() > 64) return false;
	if (s.platform == kMewiloPlatformAtariST ? s.scratch.size() != 32000 : !s.scratch.empty()) return false;
	const byte top = s.executor.frames.back().kind;
	if (top != MewiloExecFrame::kMouse && top != MewiloExecFrame::kNumbered && top != MewiloExecFrame::kContinue) return false;
	for (uint i = 0; i < s.executor.frames.size(); ++i) {
		const MewiloExecFrame &f = s.executor.frames[i];
		if (f.kind < MewiloExecFrame::kSequence || f.kind > MewiloExecFrame::kContinue ||
			f.offset < 16 || f.offset >= s.effectiveSize ||
			(f.kind == MewiloExecFrame::kSequence ? (!f.index || f.index > 255) : f.index != 0)) return false;
	}
	uint32 total = 0;
	for (uint i = 0; i < s.regions.size(); ++i) {
		const MewiloSavedRegion &r = s.regions[i];
		// Native save rectangles can extend past the screen; clipping is a host responsibility.
		if (r.x < 0 || r.x > 2040 || r.y < 0 || r.y > 255 || !r.width || r.width > 2040 ||
			!r.height || r.height > 255 || r.pixels.size() != uint32(r.width) * r.height) return false;
		total += r.pixels.size();
		if (total > 2 * 1024 * 1024) return false;
	}
	return true;
}
static void writeBlob(Common::WriteStream &out, const Common::Array<byte> &data) {
	out.writeUint32LE(data.size());
	if (!data.empty()) out.write(data.data(), data.size());
}
static bool readBlob(Common::SeekableReadStream &in, Common::Array<byte> &data, uint32 limit) {
	const uint32 size = in.readUint32LE();
	if (in.err() || in.eos() || size > limit || int64(size) > in.size() - in.pos()) return false;
	data.resize(size);
	return !size || in.read(data.data(), size) == size;
}
static bool writeMewiloSave(const MewiloSaveState &s, Common::WriteStream &out) {
	if (!validateMewiloSave(s)) return false;
	Common::MemoryWriteStreamDynamic p(DisposeAfterUse::YES);
	p.writeByte(s.platform); p.writeByte(s.language); p.writeByte(s.module);
	p.writeByte(s.exactImages); p.writeByte(s.cursorVisible);
	p.writeUint32LE(s.policy); p.writeUint32LE(s.randomSeed); p.writeUint16LE(s.recipeRevision);
	p.write(s.releaseMd5, 16); p.write(s.sourceMd5, 32); p.write(s.effectiveMd5, 32);
	p.writeUint32LE(s.sourceSize); p.writeUint32LE(s.effectiveSize);
	for (uint i = 0; i < kMewiloVariableCount; ++i) {
		p.writeSint16LE(s.executor.variables[i].word0); p.writeSint16LE(s.executor.variables[i].word1);
	}
	p.writeUint16LE(s.executor.frames.size());
	for (uint i = 0; i < s.executor.frames.size(); ++i) {
		const MewiloExecFrame &f = s.executor.frames[i];
		p.writeByte(f.kind); p.writeUint16LE(f.offset); p.writeUint16LE(f.index);
	}
	writeBlob(p, s.framebuffer); writeBlob(p, s.scratch);
	p.writeUint16LE(s.regions.size());
	for (uint i = 0; i < s.regions.size(); ++i) {
		const MewiloSavedRegion &r = s.regions[i];
		p.writeSint16LE(r.x); p.writeSint16LE(r.y); p.writeUint16LE(r.width); p.writeUint16LE(r.height);
		writeBlob(p, r.pixels);
	}
	if (p.err() || p.size() > kMaxPayload) return false;
	Common::MemoryReadStream input(p.getData(), p.size()); byte digest[16];
	if (!Common::computeStreamMD5(input, digest)) return false;
	out.write(kMagic, 4); out.writeUint16LE(kVersion); out.writeUint32LE(p.size()); out.write(digest, 16);
	out.write(p.getData(), p.size());
	return !out.err();
}
bool encodeMewiloSave(const MewiloSaveState &state, Common::WriteStream &out) {
	Common::MemoryWriteStreamDynamic staged(DisposeAfterUse::YES);
	if (!writeMewiloSave(state, staged)) return false;
	Common::MemoryReadStream input(staged.getData(), staged.size());
	MewiloSaveState decoded;
	Common::String reason;
	if (!decodeMewiloSave(input, decoded, reason)) return false;
	Common::MemoryWriteStreamDynamic roundtrip(DisposeAfterUse::YES);
	if (!writeMewiloSave(decoded, roundtrip) || staged.size() != roundtrip.size() ||
		memcmp(staged.getData(), roundtrip.getData(), staged.size())) return false;
	// Even a semantic encoding failure must leave the supplied stream untouched.
	return out.write(staged.getData(), staged.size()) == staged.size() && !out.err();
}

bool decodeMewiloSave(Common::SeekableReadStream &in, MewiloSaveState &out, Common::String &reason) {
	reason = "Invalid or unsupported Mewilo save";
	byte magic[4], expected[16];
	if (in.read(magic, 4) != 4 || memcmp(magic, kMagic, 4) || in.readUint16LE() != kVersion) return false;
	const uint32 size = in.readUint32LE();
	if (!size || size > kMaxPayload || in.read(expected, 16) != 16 || int64(size) > in.size() - in.pos()) return false;
	Common::Array<byte> bytes; bytes.resize(size);
	if (in.read(bytes.data(), size) != size) return false;
	Common::MemoryReadStream p(bytes.data(), size); byte digest[16];
	if (!Common::computeStreamMD5(p, digest) || memcmp(expected, digest, 16)) {
		reason = "Mewilo save checksum mismatch"; return false;
	}
	p.seek(0); MewiloSaveState s;
	s.platform = p.readByte(); s.language = p.readByte(); s.module = p.readByte();
	const byte exact = p.readByte(), cursor = p.readByte();
	if (exact > 1 || cursor > 1) return false;
	s.exactImages = exact; s.cursorVisible = cursor;
	s.policy = p.readUint32LE(); s.randomSeed = p.readUint32LE(); s.recipeRevision = p.readUint16LE();
	if (p.read(s.releaseMd5, 16) != 16 || p.read(s.sourceMd5, 32) != 32 || p.read(s.effectiveMd5, 32) != 32) return false;
	s.sourceSize = p.readUint32LE(); s.effectiveSize = p.readUint32LE();
	for (uint i = 0; i < kMewiloVariableCount; ++i) {
		s.executor.variables[i].word0 = p.readSint16LE(); s.executor.variables[i].word1 = p.readSint16LE();
	}
	const uint16 count = p.readUint16LE();
	if (!count || count > 256) return false;
	for (uint i = 0; i < count; ++i) {
		MewiloExecFrame f; f.kind = p.readByte(); f.offset = p.readUint16LE(); f.index = p.readUint16LE();
		s.executor.frames.push_back(f);
	}
	if (!readBlob(p, s.framebuffer, 64000) || !readBlob(p, s.scratch, 32000)) return false;
	const uint16 regions = p.readUint16LE(); if (regions > 64) return false;
	uint32 total = 0;
	for (uint i = 0; i < regions; ++i) {
		MewiloSavedRegion r; r.x = p.readSint16LE(); r.y = p.readSint16LE();
		r.width = p.readUint16LE(); r.height = p.readUint16LE();
		if (!readBlob(p, r.pixels, 2040 * 255)) return false;
		total += r.pixels.size(); if (total > 2 * 1024 * 1024) return false;
		s.regions.push_back(r);
	}
	if (p.err() || p.eos() || p.pos() != p.size() || !validateMewiloSave(s)) return false;
	out = s; reason.clear(); return true;
}
bool mewiloSaveCompatible(const MewiloSaveState &s, MewiloPlatform target,
		byte language, uint32 policy, const MewiloReleaseIdentity &release, Common::String &reason) {
	reason = "Save language, script policy or complete resource set does not match";
	if (!validateMewiloSave(s) || s.language != language || s.policy != policy ||
		memcmp(s.releaseMd5, release.digest, 16)) return false;
	if (s.platform != target) {
		const bool source68k = s.platform == kMewiloPlatformAtariST || s.platform == kMewiloPlatformAmiga;
		const bool target68k = target == kMewiloPlatformAtariST || target == kMewiloPlatformAmiga;
		reason = "Cross-platform restore requires identical Atari/Amiga resources and exact image geometry";
		if (!source68k || !target68k || !s.exactImages || !release.exactImages) return false;
	}
	const MewiloScriptPatchReport &r = release.scripts[s.module];
	if (strcmp(s.sourceMd5, r.sourceMd5) || strcmp(s.effectiveMd5, r.effectiveMd5) ||
		s.sourceSize != r.sourceSize || s.effectiveSize != r.effectiveSize) return false;
	reason.clear(); return true;
}
} // namespace Mewilo
} // namespace Gob
