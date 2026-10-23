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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "common/endian.h"
#include "common/str.h"

#include "gob/pregob/emmanuelle/oldtot.h"

namespace Gob {
namespace Emmanuelle {

bool OldTotData::applyScriptPatches(OldTotPlatformTag platform,
		Common::Language language, const Common::String &resourceName) {
	if (platform != kOldTotPlatformDOS && platform != kOldTotPlatformAtariST &&
		platform != kOldTotPlatformAmiga)
		return false;
	const bool amiga = platform == kOldTotPlatformAmiga;
	if (language == Common::DE_DEU && !amiga)
		return false;
	if (resourceName.equalsIgnoreCase(amiga ? "LOADCAS.TOA" : "LOADCAS.TOT")) {
		bool patched = patchCasinoEntryPause(platform, language);
		if (language == Common::EN_ANY)
			patched = patchCasinoPrediction() || patched;
		return patched;
	}
	if (!resourceName.equalsIgnoreCase(amiga ? "LOADAERO.TOA" : "LOADAERO.TOT"))
		return false;
	return patchIguacuAirport(language);
}

// A failed approach at Iguacu airport branches to Salvador's failure handler.
// It sets v[65] to 1, possibly replacing Salvador's successful state, while
// leaving Iguacu's v[67] unchanged. Use the existing Iguacu failure handler.
// For example, "I'd be all yours, for just one kiss..." with a low dialogue
// score can produce "Cut out the sweet-talk! You're getting on my nerves!"
// in English.
//
// Applies to: All versions
// Script: LOADAERO.TOT (DOS/ST), LOADAERO.TOA (Amiga)
bool OldTotData::patchIguacuAirport(Common::Language language) {
	struct IguacuAirportLayout {
		Common::Language language;
		uint16 condition;
		uint16 approachContinuation;
		uint16 iguacuFailure;
		uint16 salvadorFailure;
		uint16 ticketSelection;
	};
	static const IguacuAirportLayout layouts[] = {
		{Common::EN_ANY, 0x4345, 0x433F, 0x42FD, 0x44FB, 0x41FA},
		{Common::ES_ESP, 0x4137, 0x4131, 0x40EF, 0x42ED, 0x3FEC},
		{Common::FR_FRA, 0x421C, 0x4216, 0x41D4, 0x43D2, 0x40D1},
		{Common::DE_DEU, 0x436C, 0x4366, 0x4324, 0x4522, 0x4221}
	};
	const IguacuAirportLayout *layout = nullptr;
	for (uint i = 0; i < ARRAYSIZE(layouts); ++i) {
		if (layouts[i].language == language)
			layout = &layouts[i];
	}
	if (!layout)
		return false;

	const byte condition[] = {
		0x21, 1, 0, 12, 0, 0, // If v[12] == 0 (failed approach).
		1                     // False branch: continue the accepted approach.
	};
	const byte iguacuSet[] = {
		0x1C, 2,              // Sequence of two instructions.
		0xFF, 5, 0, 67, 0, 1  // Set v[67] = 1 (Iguacu failed), then select tickets.
	};
	const byte salvadorSet[] = {
		0x1C, 2,
		0xFF, 5, 0, 65, 0, 1  // Set v[65] = 1 (Salvador failed), then select tickets.
	};
	if (!has(layout->condition, 12) || !has(layout->iguacuFailure, 10) ||
		!has(layout->salvadorFailure, 10) ||
		memcmp(&_data[layout->condition], condition, sizeof(condition)) != 0 ||
		uint16BEAt(layout->condition + 7) != layout->approachContinuation ||
		_data[layout->condition + 9] != 1 ||
		uint16BEAt(layout->condition + 10) != layout->salvadorFailure ||
		memcmp(&_data[layout->iguacuFailure], iguacuSet, sizeof(iguacuSet)) != 0 ||
		uint16BEAt(layout->iguacuFailure + 8) != layout->ticketSelection ||
		memcmp(&_data[layout->salvadorFailure], salvadorSet, sizeof(salvadorSet)) != 0 ||
		uint16BEAt(layout->salvadorFailure + 8) != layout->ticketSelection)
		return false;

	// Replace only the true branch's destination in the owned script buffer.
	WRITE_BE_UINT16(&_data[layout->condition + 10], layout->iguacuFailure);
	return true;
}

// The English casino prediction draws its number over "the" at the end of
// this sentence. Its 24-pixel rectangle leaves the final 'e' visible ("28.e");
// the narrower "9." also leaves 'h'. Blank the word, use a colon and leave
// one character cell before the number. Record lengths stay unchanged.
//
// Applies to: English DOS, Atari ST and Amiga
// Script: LOADCAS.TOT / LOADCAS.TOA, text record 0x1CDF
bool OldTotData::patchCasinoPrediction() {
	const uint16 sentence = 0x1CDF;
	const uint16 strayWord = 0x1D26;
	const byte sentenceHeader[] = {0x1D, 0, 40, 0, 5, 1, 0, 0, 32, 8, 15, 0, 0, 0, 0};
	const char text[] = "@  Hey, before you go, put some@  money on this number; the    ";
	if (!has(sentence, sizeof(sentenceHeader) + sizeof(text)) ||
		memcmp(&_data[sentence], sentenceHeader, sizeof(sentenceHeader)) != 0 ||
		memcmp(&_data[sentence + sizeof(sentenceHeader)], text, sizeof(text)) != 0)
		return false;
	static const struct {
		uint16 offset;
		byte width;
		const char *text;
	} numbers[] = {
		{0x1B90, 16, "9."}, {0x1BA2, 24, "15."}, {0x1BB5, 24, "24."},
		{0x1BC8, 24, "7."}, {0x1BDA, 24, "11."}, {0x1BED, 24, "1."},
		{0x1BFF, 24, "33."}, {0x1C12, 24, "28."}, {0x1C25, 24, "12."},
		{0x1C38, 24, "26."}
	};
	for (uint i = 0; i < ARRAYSIZE(numbers); ++i) {
		const uint16 offset = numbers[i].offset;
		byte header[] = {0x1D, 0, 224, 0, 21, 0, 24, 0, 8, 8, 15, 0, 0, 0, 0};
		header[6] = numbers[i].width;
		const uint length = strlen(numbers[i].text) + 1;
		if (!has(offset, sizeof(header) + length) ||
			memcmp(&_data[offset], header, sizeof(header)) != 0 ||
			memcmp(&_data[offset + sizeof(header)], numbers[i].text, length) != 0)
			return false;
	}
	_data[strayWord] = ' ';
	_data[strayWord + 1] = ' ';
	_data[strayWord + 2] = ' ';
	_data[0x1D24] = ':';
	for (uint i = 0; i < ARRAYSIZE(numbers); ++i)
		WRITE_BE_UINT16(&_data[numbers[i].offset + 1], 232);
	return true;
}

// The opening casino caption uses FF03 1 and is covered almost immediately
// by the betting table. Use the other casino messages' FF03 250 pause here.
// This is a reading-time adjustment; the native scripts really request 1.
//
// Applies to: All versions
// Script: LOADCAS.TOT / LOADCAS.TOA
bool OldTotData::patchCasinoEntryPause(OldTotPlatformTag platform,
		Common::Language language) {
	static const struct {
		Common::Language language;
		uint16 entry, message, text, table;
		const char *caption;
	} layouts[] = {
		{Common::EN_ANY, 0x4E36, 0x47DC, 0x1B25, 0x47F7, " PLACE YOUR BETS."},
		{Common::ES_ESP, 0x4CB1, 0x4657, 0x1AE1, 0x4672, "  HAGAN JUEGO!"},
		{Common::FR_FRA, 0x4E0A, 0x47B0, 0x1B5A, 0x47CB, " FAITES VOS JEUX"},
		{Common::DE_DEU, 0x4E79, 0x481F, 0x1B54, 0x483A, " FAITES VOS JEUX"}
	};
	for (uint i = 0; i < ARRAYSIZE(layouts); ++i) {
		if (layouts[i].language != language)
			continue;
		byte entry[] = {
			0x1C, 7, 0xFF, 2, 0xFF, 2, // Restore the previous two screen regions.
			0xFF, 1, 0, 0, 0, 0, 1, 64, 0, 200, // Capture the full screen.
			0, 0,                     // Draw the opening caption.
			0xFF, 3, 1,               // The pause being replaced.
			0xFF, 11, 44, 0, 0        // Arm the transition and draw the table.
		};
		WRITE_BE_UINT16(entry + 16, layouts[i].message);
		WRITE_BE_UINT16(entry + 24, layouts[i].table);
		const byte caption[] = {0x1D, 0, 83, 0, 102, 0, 136, 0, 8, 8, 15, 0, 0, 0, 1};
		const uint length = strlen(layouts[i].caption) + 1;
		if (!has(layouts[i].entry, sizeof(entry)) || !has(layouts[i].message, 6) ||
			!has(layouts[i].table, 4) || !has(layouts[i].text, sizeof(caption) + length) ||
			memcmp(&_data[layouts[i].entry], entry, sizeof(entry)) != 0 ||
			_data[layouts[i].message] != 0x1C || _data[layouts[i].message + 1] != 2 ||
			uint16BEAt(layouts[i].message + 4) != layouts[i].text ||
			memcmp(&_data[layouts[i].text], caption, sizeof(caption)) != 0 ||
			memcmp(&_data[layouts[i].text + sizeof(caption)], layouts[i].caption, length) != 0 ||
			_data[layouts[i].table] != 0x1C || _data[layouts[i].table + 1] != 15)
			return false;
		const uint16 bubble = uint16BEAt(layouts[i].message + 2);
		const uint16 table = uint16BEAt(layouts[i].table + 2);
		byte bubbleDraw[] = {0x43, 0, 0, 0, 80, 0, 93, 0, 136, 0, 24, 0};
		bubbleDraw[11] = platform == kOldTotPlatformAmiga ? 0 : 13;
		const byte tableDraw[] = {0x60, 0, 0, 0, 0, 0, 0, 1, 64, 0, 200, 1};
		if (!has(bubble, sizeof(bubbleDraw)) || !has(table, sizeof(tableDraw)) ||
			memcmp(&_data[bubble], bubbleDraw, sizeof(bubbleDraw)) != 0 ||
			memcmp(&_data[table], tableDraw, sizeof(tableDraw)) != 0)
			return false;
		_data[layouts[i].entry + 20] = 250;
		return true;
	}
	return false;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
