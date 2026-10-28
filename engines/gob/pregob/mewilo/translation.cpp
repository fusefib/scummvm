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

#include "gob/pregob/mewilo/translation.h"

#include "common/stream.h"

#include "gob/pregob/mewilo/executor.h"
#include "gob/pregob/mewilo/font.h"
#include "gob/pregob/mewilo/tot.h"
#include "gob/surface.h"

namespace Gob {
namespace Mewilo {

namespace {

int compare(const ReplacementKey &a, const ReplacementKey &b) {
	if (a.family != b.family)
		return int(a.family) - b.family;
	if (a.module != b.module)
		return int(a.module) - b.module;
	if (a.node != b.node)
		return int(a.node) - b.node;
	if (a.kind != b.kind)
		return int(a.kind) - b.kind;
	return int(a.field) - b.field;
}

bool printable(byte c) { return (c >= 32 && c < 127) || c == 0x11 || (c >= 0x16 && c <= 0x1F); }
bool validEntry(const ReplacementEntry &e) {
	if (e.key.kind == kReplacementGlyph)
		return e.key.family < 4 && e.key.module == 255 && !e.key.field && e.key.node < 128 &&
			!e.opcode && e.width == 8 && e.height == 8 && e.bytes.size() == 8;
	if (e.key.family > 3 || e.key.module > 8 || e.key.node < 16 || e.key.kind < 1 || e.key.kind > 4)
		return false;
	if (e.key.kind != kReplacementAnswers && e.key.field)
		return false;
	if (e.key.kind == kReplacementImage) {
		if ((e.opcode != 0x41 && e.opcode != 0x60) || !e.width || !e.height || e.height > 200 ||
			e.width > (e.key.family == kReplacementCpcFrench ? 160 : 320) || e.bytes.size() != uint32(e.width) * e.height)
			return false;
		for (uint i = 0; i < e.bytes.size(); ++i)
			if (e.bytes[i] > 15)
				return false;
		return true;
	}
	if (e.opcode != (e.key.kind == kReplacementText ? 0x1D : 0x12) || e.bytes.empty() || e.bytes.back() != 0)
		return false;
	if (e.key.kind == kReplacementReject) {
		if (e.width || e.height || e.bytes.size() > 41)
			return false;
	} else if (!e.width || e.width > 40 || e.height > 200)
		return false;
	uint partStart = 0;
	for (uint i = 0; i + 1 < e.bytes.size(); ++i) {
		if (!printable(e.bytes[i]))
			return false;
		if (e.key.kind == kReplacementAnswers && strchr("@[]{}", e.bytes[i]))
			return false;
		if (e.key.kind == kReplacementAnswers && e.bytes[i] == '/') {
			if (i == partStart || i - partStart > e.width)
				return false;
			partStart = i + 1;
		}
	}
	if (e.key.kind == kReplacementAnswers && (partStart + 1 >= e.bytes.size() || e.bytes.size() - partStart - 1 > e.width))
		return false;
	return true;
}
} // End of anonymous namespace

bool MewiloReplacements::load(Common::SeekableReadStream &input, Common::String &reason) {
	reason.clear();
	// A changing file must not enlarge the bound used for staged allocations.
	const int64 size = input.size();
	if (size < 12 || size > 4 * 1024 * 1024 || !input.seek(0)) {
		reason = "Invalid replacement pack size";
		return false;
	}
	Common::SeekableReadStream &s = input;
	char magic[4];
	if (s.read(magic, 4) != 4 || memcmp(magic, "MWEN", 4) || s.readUint16LE() != 2) {
		reason = "Unsupported replacement pack";
		return false;
	}
	const uint16 revision = s.readUint16LE(), count = s.readUint16LE(), flags = s.readUint16LE();
	if (!revision || !count || count > 8192 || flags > 1) {
		reason = "Invalid replacement pack header";
		return false;
	}
	Common::Array<ReplacementEntry> staged;
	for (uint i = 0; i < count; ++i) {
		const int64 position = s.pos();
		if (position < 0 || position > size || size - position < 16) {
			reason = "Truncated replacement record";
			return false;
		}
		ReplacementEntry e;
		e.key.family = s.readByte();
		e.key.module = s.readByte();
		e.key.kind = s.readByte();
		e.key.field = s.readByte();
		e.key.node = s.readUint16LE();
		e.opcode = s.readByte();
		const byte reserved = s.readByte();
		e.width = s.readUint16LE();
		e.height = s.readUint16LE();
		const uint32 length = s.readUint32LE();
		const int64 payloadOffset = s.pos();
		if (reserved || !length || payloadOffset < 0 || payloadOffset > size ||
			length > uint64(size - payloadOffset)) {
			reason = "Invalid replacement payload length";
			return false;
		}
		e.bytes.resize(length);
		if (s.read(e.bytes.data(), length) != length || !validEntry(e) ||
			(!staged.empty() && (staged[0].key.family != e.key.family || compare(staged.back().key, e.key) >= 0))) {
			reason = "Invalid or duplicate replacement entry";
			return false;
		}
		staged.push_back(e);
	}
	if (s.err() || s.pos() != size || s.size() != size) {
		reason = "Trailing or unreadable replacement payload";
		return false;
	}
	_entries.swap(staged);
	_revision = revision;
	_sparseOriginal = (flags == 1);
	return true;
}

const ReplacementEntry *MewiloReplacements::find(const ReplacementKey &key) const {
	uint first = 0, end = _entries.size();
	while (first < end) {
		const uint mid = first + (end - first) / 2;
		const int cmp = compare(_entries[mid].key, key);
		if (!cmp)
			return &_entries[mid];
		if (cmp < 0)
			first = mid + 1;
		else
			end = mid;
	}
	return nullptr;
}

const ReplacementEntry *MewiloReplacements::resolve(const MewiloTotData &tot, const ReplacementKey &key) const {
	if (key.kind == kReplacementGlyph)
		return nullptr; // glyphs are not script nodes
	const ReplacementEntry *e = find(key);
	if (!e || !tot.has(key.node) || tot.byteAt(key.node) != e->opcode)
		return nullptr;
	uint width = 0, height = 0;
	if (key.kind == kReplacementText) {
		if (!tot.has(key.node, 10))
			return nullptr;
		width = tot.byteAt(key.node + 3);
		height = tot.byteAt(key.node + 4);
	} else if (key.kind == kReplacementImage) {
		if (!tot.has(key.node, 7))
			return nullptr;
		width = tot.byteAt(key.node + 5) * (key.family == kReplacementCpcFrench ? 4 : 8);
		height = tot.byteAt(key.node + 6);
	} else {
		if (!tot.has(key.node, 15))
			return nullptr;
		uint32 p = key.node + 15 + tot.byteAt(key.node + 14);
		if (!tot.has(p))
			return nullptr;
		const uint count = tot.byteAt(p++);
		if (key.kind == kReplacementAnswers) {
			if (key.field >= count)
				return nullptr;
			for (uint field = 0; field <= key.field; ++field) {
				if (!tot.has(p, 10) || !tot.has(p + 10, tot.byteAt(p + 9)))
					return nullptr;
				if (field == key.field) {
					width = tot.byteAt(p + 2);
					height = tot.byteAt(p + 3);
				}
				p += 10 + tot.byteAt(p + 9);
			}
		}
	}
	return width == e->width && height == e->height ? e : nullptr;
}

bool MewiloReplacements::answerMatches(const MewiloTotData &tot, const ReplacementKey &key,
	const Common::Array<byte> &input, bool &handled) const {
	handled = false;
	if (key.kind != kReplacementAnswers)
		return false;
	const ReplacementEntry *e = resolve(tot, key);
	if (!e)
		return false;
	handled = true;
	Common::Array<byte> normalized = input, candidate;
	MewiloExecutor::normalizeAnswer(normalized);
	for (uint i = 0; i < e->bytes.size(); ++i) {
		const byte c = e->bytes[i];
		if (c == '/' || !c) {
			MewiloExecutor::normalizeAnswer(candidate);
			if (!candidate.empty() && normalized.size() == candidate.size() &&
				!memcmp(normalized.data(), candidate.data(), candidate.size()))
				return true;
			candidate.clear();
		} else
			candidate.push_back(c);
	}
	return false;
}

bool MewiloReplacements::validateModule(const MewiloTotData &tot, byte module, Common::String &reason) const {
	for (uint i = 0; i < _entries.size(); ++i) {
		const ReplacementEntry &e = _entries[i];
		if (e.key.module == module && !resolve(tot, e.key)) {
			reason = "replacement entry does not match its local node/geometry";
			return false;
		}
	}
	if (_sparseOriginal) {
		reason.clear();
		return true;
	}
	MewiloGraphStats stats;
	Common::Array<uint16> nodes;
	if (!tot.validateReachableGraph(stats, &nodes))
		return false;
	for (uint i = 0; i < nodes.size(); ++i) {
		const uint16 n = nodes[i];
		const byte op = tot.byteAt(n);
		ReplacementKey key = {family(), module, kReplacementText, 0, n};
		if (op == 0x1D && !resolve(tot, key)) {
			reason = "replacement pack is missing reachable text";
			return false;
		}
		if (op != 0x12)
			continue;
		key.kind = kReplacementReject;
		if (!resolve(tot, key)) {
			reason = "replacement pack is missing rejection text";
			return false;
		}
		const uint32 p = n + 15 + tot.byteAt(n + 14);
		if (!tot.has(p))
			return false;
		for (uint field = 0; field < tot.byteAt(p); ++field) {
			key.kind = kReplacementAnswers;
			key.field = field;
			if (!resolve(tot, key)) {
				reason = "replacement pack is missing an answer field";
				return false;
			}
		}
	}
	reason.clear();
	return true;
}

bool MewiloReplacements::applyGlyphs(MewiloFont &font) const {
	if (!font.valid())
		return false;
	for (uint i = 0; i < _entries.size(); ++i) {
		const ReplacementEntry &e = _entries[i];
		if (e.key.kind == kReplacementGlyph && !font.replaceGlyph(e.key.node, e.bytes.data(), e.bytes.size()))
			return false;
	}
	return true;
}

void MewiloReplacements::writeIdentity(Common::WriteStream &stream) const {
	if (_entries.empty())
		return;
	uint32 size = 12;
	for (uint i = 0; i < _entries.size(); ++i)
		size += 16 + _entries[i].bytes.size();
	stream.write("MWEN resident replacements", 26);
	stream.writeUint32LE(size);
	// Accepted packs have canonical ordering and zero reserved fields. Rebuild
	// the exact resident bytes instead of keeping a second copy of every image.
	stream.write("MWEN", 4);
	stream.writeUint16LE(2);
	stream.writeUint16LE(_revision);
	stream.writeUint16LE(_entries.size());
	stream.writeUint16LE(_sparseOriginal);
	for (uint i = 0; i < _entries.size(); ++i) {
		const ReplacementEntry &entry = _entries[i];
		stream.writeByte(entry.key.family);
		stream.writeByte(entry.key.module);
		stream.writeByte(entry.key.kind);
		stream.writeByte(entry.key.field);
		stream.writeUint16LE(entry.key.node);
		stream.writeByte(entry.opcode);
		stream.writeByte(0);
		stream.writeUint16LE(entry.width);
		stream.writeUint16LE(entry.height);
		stream.writeUint32LE(entry.bytes.size());
		stream.write(entry.bytes.data(), entry.bytes.size());
	}
}

bool drawReplacementImage(const ReplacementEntry &e, Surface &dest, int16 x, int16 y) {
	if (e.key.kind != kReplacementImage || !e.width || !e.height || e.bytes.size() != uint32(e.width) * e.height)
		return false;
	for (uint i = 0; i < e.bytes.size(); ++i)
		if (e.bytes[i] > 15)
			return false;
	const uint scale = e.key.family == kReplacementCpcFrench ? 2 : 1;
	for (uint sy = 0; sy < e.height; ++sy) {
		const int dy = int(y) + int(sy);
		if (dy < 0 || dy >= dest.getHeight())
			continue;
		for (uint sx = 0; sx < e.width; ++sx) {
			const byte pen = e.bytes[sy * e.width + sx];
			for (uint n = 0; n < scale; ++n) {
				const int dx = int(x) + int(sx * scale + n);
				if (dx >= 0 && dx < dest.getWidth())
					dest.putPixel(dx, dy, pen);
			}
		}
	}
	return true;
}

} // namespace Mewilo
} // namespace Gob
