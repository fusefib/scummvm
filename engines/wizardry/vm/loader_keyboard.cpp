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

#include "wizardry/vm/loader_keyboard.h"
#include "common/endian.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/util.h"

namespace Wizardry {

namespace {
struct LoaderTile {
	uint16 offset;
	byte pixels;
};
struct LoaderRule {
	byte revision;
	LoaderKeyboard::Action action;
	uint count;
	const LoaderTile *tiles;
};
#include "wizardry/vm/loader_patterns.h"

const uint kDataBase = 0x0584;

uint challengeDataSize(byte revision) {
	return revision == 4 ? 16998 : revision == 5 ? 12868
												 : 0;
}

bool digestEquals(const Common::Array<byte> &data, const char *expected) {
	Common::MemoryReadStream stream(data.data(), data.size());
	return Common::computeStreamMD5AsString(stream) == expected;
}

uint16 dataWord(const Common::Array<byte> &data, uint address) {
	assert(address >= kDataBase && address + 2 <= kDataBase + data.size());
	return READ_LE_UINT16(data.data() + address - kDataBase);
}

int matchGroup(const byte *vram, const Common::Array<byte> &data, uint targets, uint sources, uint count) {
	for (uint i = 0; i < count; ++i) {
		bool matched = true;
		for (uint glyph = 0; glyph < 4 && matched; ++glyph) {
			const uint source = dataWord(data, sources + 8 * i + 2 * glyph);
			const uint target = dataWord(data, targets + 4 * glyph + 2);
			assert(source >= kDataBase && source + 16 <= kDataBase + data.size());
			assert(target + 3 * 80 + 0x2002 <= 0x4000);
			const byte *pixels = data.data() + source - kDataBase;
			for (uint line = 0; line < 4; ++line)
				matched &= !memcmp(vram + target + line * 80, pixels + line * 4, 2) &&
						   !memcmp(vram + target + line * 80 + 0x2000, pixels + line * 4 + 2, 2);
		}
		if (matched)
			return i;
	}
	return -1;
}

void buildChallengeKeys(byte revision, const Common::Array<byte> &data, uint index, uint16 words[5]) {
	if (revision == 4) {
		uint value = dataWord(data, 0x0754 + 2 * index);
		for (int i = 3; i >= 0; --i) {
			const uint digit = value % 10;
			words[i] = uint16(((digit ? digit + 1 : 11) << 8) | ('0' + digit));
			value /= 10;
		}
	} else {
		for (uint i = 0; i < 4; ++i)
			words[i] = dataWord(data, 0x0744 + 8 * index + 2 * i);
	}
	words[4] = 0x1c0d;
}
} // namespace

LoaderKeyboard::Action LoaderKeyboard::match(byte revision, const byte *vram) {
	for (uint i = 0; i < ARRAYSIZE(kLoaderRules); ++i) {
		const LoaderRule &rule = kLoaderRules[i];
		if (rule.revision != revision)
			continue;
		bool matches = true;
		for (uint j = 0; j < rule.count && matches; ++j) {
			const LoaderTile &tile = rule.tiles[j];
			const byte *pixels = kLoaderPixels[tile.pixels];
			for (uint row = 0; row < 4; ++row) {
				const uint at = tile.offset + row * 80;
				matches &= !memcmp(vram + at, pixels + row * 4, 2) &&
						   !memcmp(vram + at + 0x2000, pixels + row * 4 + 2, 2);
			}
		}
		if (matches)
			return rule.action;
	}
	return kNone;
}

bool LoaderKeyboard::validQueue(const Common::Array<uint16> &keys) {
	if (keys.size() > 3)
		return false;
	for (uint i = 0; i < keys.size(); ++i)
		if (keys[i] != (keys.size() == 3 && !i ? 0x0332 : 0x1c0d))
			return false;
	return true;
}

bool LoaderKeyboard::validQueue(const Common::Array<uint16> &keys, byte revision, const Common::Array<byte> &data) {
	if (validQueue(keys))
		return true;
	if (keys.empty() || keys.size() > 5 || data.size() != challengeDataSize(revision) || !challengeDataSize(revision))
		return false;
	uint16 words[5];
	for (uint i = 0; i < (revision == 4 ? 8000U : 1296U); ++i) {
		// V selector rows 178/179 alias earlier rows 36/37.
		if (revision == 5 && (i % 216 == 178 || i % 216 == 179))
			continue;
		buildChallengeKeys(revision, data, i, words);
		bool equal = true;
		for (uint j = 0; j < keys.size(); ++j)
			equal &= keys[j] == words[5 - keys.size() + j];
		if (equal)
			return true;
	}
	return false;
}

bool LoaderKeyboard::extractData(byte revision, const Common::Array<byte> &com,
								 Common::Array<byte> &data, Common::String &error) {
	error.clear();
	if ((revision != 4 && revision != 5) || com.size() != (revision == 4 ? 18694U : 14742U) ||
		!digestEquals(com, revision == 4 ? "b4974832a38f94cc23c0667c7b842f9c" : "8a64f7ebb3781ac4ea6ca19cce3604f3")) {
		error = "Resident challenge data requires the matching supplied WIZ4.COM or WIZ5.COM";
		return false;
	}
	Common::Array<byte> candidate(com.data() + kDataBase - 0x100, challengeDataSize(revision));
	if (!validData(revision, candidate)) {
		error = "Resident challenge data differs from the supported loader";
		return false;
	}
	data = Common::move(candidate);
	return true;
}

bool LoaderKeyboard::validData(byte revision, const Common::Array<byte> &data) {
	return challengeDataSize(revision) && data.size() == challengeDataSize(revision) &&
		   digestEquals(data, revision == 4 ? "72477ebc27858cfc35f5693f1eaf6e27" : "632ac3bb1b85664274e2b45a6b05313a");
}

void LoaderKeyboard::challengeKeys(byte revision, const byte *vram,
								   const Common::Array<byte> &data, Common::Array<uint16> &keys) {
	assert(keys.empty());
	if (!challengeDataSize(revision) || data.size() != challengeDataSize(revision))
		return;
	uint index;
	if (revision == 4) {
		const int first = matchGroup(vram, data, 0x45d4, 0x460a, 20);
		if (first < 0)
			return;
		const int second = matchGroup(vram, data, 0x45e6, 0x46aa, 20);
		if (second < 0)
			return;
		const int third = matchGroup(vram, data, 0x45f8, 0x474a, 20);
		if (third < 0)
			return;
		index = first * 400 + second * 20 + third;
	} else {
		const int first = matchGroup(vram, data, 0x2fc4, 0x2fe8, 36);
		if (first < 0)
			return;
		const int second = matchGroup(vram, data, 0x2fd6, 0x3108, 216);
		if (second < 0)
			return;
		index = (first / 6) * 216 + second;
	}
	uint16 words[5];
	buildChallengeKeys(revision, data, index, words);
	for (uint i = 0; i < 5; ++i)
		keys.push_back(words[i]);
}

void LoaderKeyboard::enqueue(Action action, Common::Array<uint16> &keys) {
	assert(keys.empty());
	if (action == kStart) {
		keys.push_back(0x0332);
		keys.push_back(0x1c0d);
	}
	if (action == kStart || action == kReturn || action == kSelectSave)
		keys.push_back(0x1c0d);
}

} // namespace Wizardry
