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

namespace {
struct HotelArrivalLayout {
	Common::Language language;
	uint16 arrival, roomSelection, helicopter;
};

static const HotelArrivalLayout kHotelArrivals[] = {
	{Common::EN_ANY, 0x69B3, 0x77AB, 0x6C02},
	{Common::ES_ESP, 0x667D, 0x7475, 0x68CC},
	{Common::FR_FRA, 0x695B, 0x7753, 0x6BAA},
	{Common::DE_DEU, 0x6A31, 0x7829, 0x6C80}
};
} // End of anonymous namespace

bool OldTotData::applyScriptPatches(OldTotPlatformTag platform,
		Common::Language language, const Common::String &resourceName) {
	if (platform != kOldTotPlatformDOS && platform != kOldTotPlatformAtariST &&
		platform != kOldTotPlatformAmiga)
		return false;
	const bool amiga = platform == kOldTotPlatformAmiga;
	if (language != Common::EN_ANY && language != Common::ES_ESP &&
		language != Common::FR_FRA && language != Common::DE_DEU)
		return false;
	if (language == Common::DE_DEU && !amiga)
		return false;
	bool patched = false;
	if (resourceName.equalsIgnoreCase(amiga ? "LOADHOT.TOA" : "LOADHOT.TOT") ||
		resourceName.equalsIgnoreCase(amiga ? "LOADAERO.TOA" : "LOADAERO.TOT") ||
		resourceName.equalsIgnoreCase(amiga ? "LOADCAS.TOA" : "LOADCAS.TOT") ||
		resourceName.equalsIgnoreCase(amiga ? "LOADFAV.TOA" : "LOADFAV.TOT") ||
		resourceName.equalsIgnoreCase(amiga ? "LOADGAR.TOA" : "LOADGAR.TOT") ||
		resourceName.equalsIgnoreCase(amiga ? "LOADPLA.TOA" : "LOADPLA.TOT"))
		patched = patchMeterBand();
	if (resourceName.equalsIgnoreCase(amiga ? "LOADCAS.TOA" : "LOADCAS.TOT")) {
		patched = patchCasinoEntryPause(platform, language) || patched;
		patched = patchRouletteTwenty(language) || patched;
		patched = patchRouletteConfirmation(platform, language) || patched;
		patched = patchCasinoReturn(language) || patched;
		if (language == Common::EN_ANY)
			patched = patchCasinoPrediction() || patched;
		patched = patchCasinoCaptions(platform, language) || patched;
		return patched;
	}
	if (resourceName.equalsIgnoreCase(amiga ? "LOADGAR.TOA" : "LOADGAR.TOT")) {
		patched = patchPartyHotspots(language) || patched;
		if (language == Common::ES_ESP)
			patched = patchSpanishReplies() || patched;
		return patched;
	}
	if (resourceName.equalsIgnoreCase(amiga ? "LOADFAV.TOA" : "LOADFAV.TOT"))
		return patchVendorAffordability(language) || patched;
	if (resourceName.equalsIgnoreCase(amiga ? "LOADHOT.TOA" : "LOADHOT.TOT")) {
		patched = patchHotelRoomState(language) || patched;
		patched = patchHotelHelicopter(language) || patched;
		return patchLobbyGuestReset(platform, language) || patched;
	}
	if (!resourceName.equalsIgnoreCase(amiga ? "LOADAERO.TOA" : "LOADAERO.TOT"))
		return patched;
	patched = patchIguacuAirport(language) || patched;
	return patchAirportFallback(platform, language) || patched;
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

// LOADCAS's opening and closed-bets announcements repaint the bubble's bevel,
// and the English full stops reach its right edge. Keep the existing bubble
// background and give the text blocks margins and the stakes lines a gap.
// German Amiga also retains French captions. Translate these in place using
// the original fonts' lowercase a-umlaut (0x84).
//
// Applies to: All supported versions
// Script: LOADCAS.TOT / LOADCAS.TOA
bool OldTotData::patchCasinoCaptions(OldTotPlatformTag platform,
		Common::Language language) {
	static const struct {
		Common::Language language;
		uint16 text[3]; // Opening, stakes placed, bets closed.
		uint16 bubbles[2], amigaBubbles[2]; // Lower and upper bubble.
		byte x[3]; // Includes any leading spaces.
		const char *captions[3];
	} layouts[] = {
		{Common::EN_ANY, {0x1B25, 0x1B46, 0x1B6F},
			{0x6352, 0x6FB9}, {0x63E0, 0x7318}, {78, 92, 78},
			{" PLACE YOUR BETS.", "  THE STAKES@   ARE LAID.", " BETS ARE CLOSED."}},
		{Common::ES_ESP, {0x1AE1, 0x1AFF, 0x1B2A},
			{0x61CD, 0x6E34}, {0x625B, 0x7193}, {86, 92, 80},
			{"  HAGAN JUEGO!", " LAS APUESTAS@ ESTAN HECHAS", "    NO VA MAS"}},
		{Common::FR_FRA, {0x1B5A, 0x1B7A, 0x1BA4},
			{0x6326, 0x6F8D}, {0x63B4, 0x72EC}, {80, 84, 80},
			{" FAITES VOS JEUX", "    LES JEUX@   SONT FAITS", " RIEN NE VA PLUS"}},
		{Common::DE_DEU, {0x1B54, 0x1B74, 0x1B9E},
			{0, 0}, {0x6423, 0x735B}, {97, 96, 84},
			{" FAITES VOS JEUX", "    LES JEUX@   SONT FAITS", " RIEN NE VA PLUS"}}
	};
	static const char *const germanCaptions[] = {
		"Bitte setzen!", "  Eins\x84tze@  platziert", "Nichts geht mehr"
	};
	for (uint i = 0; i < ARRAYSIZE(layouts); ++i) {
		if (layouts[i].language != language)
			continue;
		const byte headers[][15] = {
			{0x1D, 0, 83, 0, 102, 0, 136, 0, 8, 8, 15, 0, 0, 0, 1},
			{0x1D, 0, 91, 0, 28, 0, 112, 0, 16, 8, 15, 0, 0, 0, 1},
			{0x1D, 0, 83, 0, 32, 0, 136, 0, 8, 8, 15, 0, 0, 0, 1}
		};
		// Check every caption and both bubble descriptors before changing any text.
		for (uint caption = 0; caption < ARRAYSIZE(headers); ++caption) {
			const uint16 offset = layouts[i].text[caption];
			const uint length = strlen(layouts[i].captions[caption]) + 1;
			if (!has(offset, sizeof(headers[caption]) + length) ||
				(language == Common::DE_DEU && strlen(germanCaptions[caption]) >= length) ||
				memcmp(&_data[offset], headers[caption], sizeof(headers[caption])) != 0 ||
				memcmp(&_data[offset + sizeof(headers[caption])], layouts[i].captions[caption], length) != 0)
				return false;
		}
		const uint16 *bubbles = platform == kOldTotPlatformAmiga ? layouts[i].amigaBubbles : layouts[i].bubbles;
		for (uint bubble = 0; bubble < 2; ++bubble) {
			byte descriptor[] = {0x43, 0, 0, 0, 80, 0, 93, 0, 136, 0, 24, 0, 0};
			if (bubble) {
				descriptor[0] = 0x53;
				descriptor[6] = 24;
				WRITE_BE_UINT16(descriptor + 11, bubbles[0] + 11);
			}
			const uint length = bubble ? 13 : 11;
			if (!has(bubbles[bubble], length) || memcmp(&_data[bubbles[bubble]], descriptor, length) != 0)
				return false;
		}

		for (uint caption = 0; caption < ARRAYSIZE(headers); ++caption) {
			WRITE_BE_UINT16(&_data[layouts[i].text[caption] + 1], layouts[i].x[caption]);
			_data[layouts[i].text[caption] + 10] = 16; // Transparent: retain the bubble artwork.
			if (language == Common::DE_DEU) {
				// Pad to the existing terminator so later records keep their offsets.
				byte *text = &_data[layouts[i].text[caption] + 15];
				memset(text, ' ', strlen(layouts[i].captions[caption]));
				memcpy(text, germanCaptions[caption], strlen(germanCaptions[caption]));
			}
		}
		WRITE_BE_UINT16(&_data[layouts[i].text[0] + 3], 101);
		WRITE_BE_UINT16(&_data[layouts[i].text[1] + 3], 26);
		WRITE_BE_UINT16(&_data[layouts[i].text[1] + 7], 20);
		_data[layouts[i].text[1] + 9] = 10;
		return true;
	}
	return false;
}

// Roulette's random-result table assigns 21 to both slots 20 and 21, so a
// normal spin cannot select 20. Correct the existing assignment; selection,
// the lucky-number path and settlement retain their original behavior.
//
// Applies to: All versions
// Script: LOADCAS.TOT / LOADCAS.TOA
bool OldTotData::patchRouletteTwenty(Common::Language language) {
	static const struct {
		Common::Language language;
		uint16 table, firstResult;
	} layouts[] = {
		{Common::EN_ANY, 0x4CC5, 0x4B9D},
		{Common::ES_ESP, 0x4B40, 0x4A18},
		{Common::FR_FRA, 0x4C99, 0x4B71},
		{Common::DE_DEU, 0x4D08, 0x4BE0}
	};
	for (uint layout = 0; layout < ARRAYSIZE(layouts); ++layout) {
		if (layouts[layout].language != language)
			continue;
		const uint16 table = layouts[layout].table;
		const uint16 firstResult = layouts[layout].firstResult;
		const byte random[] = {0x20, 1, 37}; // Select one of 37 result handlers.
		if (!has(table, 77) || !has(firstResult, 37 * 8) ||
			memcmp(&_data[table], random, sizeof(random)) != 0)
			return false;
		for (uint result = 0; result < 37; ++result) {
			byte assignment[] = {0x1C, 1, 0xFF, 5, 0, 127, 0, 0};
			assignment[7] = result == 20 ? 21 : result; // Set v[127].
			const uint16 handler = firstResult + 8 * result;
			if (uint16BEAt(table + 3 + 2 * result) != handler ||
				memcmp(&_data[handler], assignment, sizeof(assignment)) != 0)
				return false;
		}
		WRITE_BE_UINT16(&_data[firstResult + 20 * 8 + 6], 20);
		return true;
	}
	return false;
}

// The lipstick meter's 25..49 branch repeats the low band. Its intermediate
// graphic already exists in helper 0x08AD; redirect that branch to it.
//
// Applies to: All versions, shared meter in LOADHOT, LOADAERO, LOADCAS,
// LOADFAV, LOADGAR and LOADPLA (.TOT / .TOA)
bool OldTotData::patchMeterBand() {
	const uint16 conditions[] = {0x08A1, 0x08B1, 0x08C1, 0x08D1};
	const byte comparisons[][12] = {
		{0x21, 0, 0, 30, 0, 75, 1, 0x08, 0x99, 1, 0x08, 0x9D}, // v[30] < 75
		{0x21, 0, 0, 30, 0, 50, 1, 0x08, 0xA1, 1, 0x08, 0xBD}, // v[30] < 50
		{0x21, 0, 0, 30, 0, 25, 1, 0x08, 0xB1, 1, 0x08, 0xBD}, // v[30] < 25
		{0x21, 1, 0, 30, 0, 0, 1, 0x08, 0xC1, 1, 0x08, 0xCD}  // v[30] == 0
	};
	if (!has(0x08A1, 60))
		return false;
	for (uint i = 0; i < ARRAYSIZE(conditions); ++i) {
		if (memcmp(&_data[conditions[i]], comparisons[i], sizeof(comparisons[i])) != 0)
			return false;
	}
	if (uint16BEAt(0x08AD) != 0x1C01) // Sequence containing one draw.
		return false;
	const uint16 draw = uint16BEAt(0x08AF);
	const byte descriptor[] = {0x65, 0, 0, 0, 0, 0, 162, 0, 32, 0, 22, 53};
	if (!has(draw, sizeof(descriptor)) ||
		memcmp(&_data[draw], descriptor, sizeof(descriptor)) != 0)
		return false;
	WRITE_BE_UINT16(&_data[0x08BB], 0x08AD);
	return true;
}

// The French/German Rio casino-bar encounter returns to hotel location 22
// (Manaus). Use location 2 (Rio), as in the English and Spanish scripts.
//
// Applies to: French DOS/ST/Amiga and German Amiga
// Script: LOADCAS.TOT / LOADCAS.TOA
bool OldTotData::patchCasinoReturn(Common::Language language) {
	const uint16 entry = language == Common::FR_FRA ? 0x4EB6 :
		(language == Common::DE_DEU ? 0x4F25 : 0);
	const byte sequence[] = {
		0x1C, 3, 0x09, 0x45, // Advance time.
		0xFF, 5, 0, 44, 0, 22, // Set hotel location v[44] = 22.
		0xFF, 5, 0, 0, 0, 2   // Return through the hotel module selector.
	};
	if (!entry || !has(entry, sizeof(sequence)) ||
		memcmp(&_data[entry], sequence, sizeof(sequence)) != 0)
		return false;
	WRITE_BE_UINT16(&_data[entry + 8], 2);
	return true;
}

// At the Manaus garden party, the two guest hotspots test each other's
// availability flags. A hidden guest can therefore remain clickable. Make
// each hotspot use the same condition as its guest's existing draw branch.
//
// Applies to: All versions
// Script: LOADGAR.TOT / LOADGAR.TOA
bool OldTotData::patchPartyHotspots(Common::Language language) {
	static const struct {
		Common::Language language;
		uint16 clickCondition, drawCondition, choice;
		uint16 firstGuest, secondGuest, firstDraw, secondDraw;
	} layouts[] = {
		{Common::EN_ANY, 0x38B2, 0x3928, 0x38D6, 0x386D, 0x3884, 0x3910, 0x3914},
		{Common::ES_ESP, 0x37B4, 0x382A, 0x37D8, 0x376F, 0x3786, 0x3812, 0x3816},
		{Common::FR_FRA, 0x38EE, 0x3964, 0x3912, 0x38A9, 0x38C0, 0x394C, 0x3950},
		{Common::DE_DEU, 0x3969, 0x39DF, 0x398D, 0x3924, 0x393B, 0x39C7, 0x39CB}
	};
	for (uint layout = 0; layout < ARRAYSIZE(layouts); ++layout) {
		if (layouts[layout].language != language)
			continue;
		const uint16 click = layouts[layout].clickCondition;
		const uint16 draw = layouts[layout].drawCondition;
		if (!has(click, 24) || !has(draw, 24))
			return false;
		const uint16 offsets[] = {click, uint16(click + 12), draw, uint16(draw + 12)};
		const uint16 no[] = {layouts[layout].firstGuest, layouts[layout].choice, 0, layouts[layout].secondDraw};
		const uint16 yes[] = {layouts[layout].choice, layouts[layout].secondGuest, layouts[layout].firstDraw, 0};
		byte tests[][12] = {
			{0x21, 1, 0, 70, 0, 2, 1, 0, 0, 1, 0, 0}, // First hotspot: v[70] == 2.
			{0x21, 1, 0, 68, 0, 0, 1, 0, 0, 1, 0, 0}, // Second hotspot: v[68] == 0.
			{0x21, 1, 0, 68, 0, 0, 1, 0, 0, 1, 0, 0}, // First guest is drawn if v[68] == 0.
			{0x21, 1, 0, 70, 0, 2, 1, 0, 0, 1, 0, 0}  // Second guest is drawn if v[70] != 2.
		};
		for (uint i = 0; i < ARRAYSIZE(offsets); ++i) {
			WRITE_BE_UINT16(tests[i] + 7, no[i]);
			WRITE_BE_UINT16(tests[i] + 10, yes[i]);
			if (memcmp(&_data[offsets[i]], tests[i], sizeof(tests[i])) != 0)
				return false;
		}
		WRITE_BE_UINT16(&_data[click + 2], 68);
		WRITE_BE_UINT16(&_data[click + 4], 0);
		WRITE_BE_UINT16(&_data[click + 7], layouts[layout].choice);
		WRITE_BE_UINT16(&_data[click + 10], layouts[layout].firstGuest);
		WRITE_BE_UINT16(&_data[click + 14], 70);
		WRITE_BE_UINT16(&_data[click + 16], 2);
		WRITE_BE_UINT16(&_data[click + 19], layouts[layout].secondGuest);
		WRITE_BE_UINT16(&_data[click + 22], layouts[layout].choice);
		return true;
	}
	return false;
}

// Rio/Iguacu hotel arrivals select the room state, then erase it with the
// temporary-variable reset. Reset first and select afterwards, retaining
// the preceding time advance and the rest of the arrival sequence.
//
// Applies to: All versions
// Script: LOADHOT.TOT / LOADHOT.TOA
bool OldTotData::patchHotelRoomState(Common::Language language) {
	if (!has(0x0044, 122) || uint16BEAt(0x0044) != 0x1C14)
		return false;
	for (uint i = 0; i < 20; ++i) {
		byte clear[] = {0xFF, 5, 0, 0, 0, 0};
		clear[3] = i < 10 ? 10 + i : 90 + i - 10;
		if (memcmp(&_data[0x0046 + 6 * i], clear, sizeof(clear)) != 0)
			return false;
	}
	for (uint layout = 0; layout < ARRAYSIZE(kHotelArrivals); ++layout) {
		if (kHotelArrivals[layout].language != language)
			continue;
		const uint16 arrival = kHotelArrivals[layout].arrival;
		if (!has(arrival, 76))
			return false;
		for (uint city = 0; city < 2; ++city) {
			byte entry[] = {0x1C, 13, 0xFF, 5, 0, 44, 0, 0,
				0x09, 0x45, 0, 0, 0, 0x44}; // Time, room selection, reset.
			entry[7] = city == 0 ? 2 : 32;
			WRITE_BE_UINT16(entry + 10, kHotelArrivals[layout].roomSelection);
			if (memcmp(&_data[arrival + 38 * city], entry, sizeof(entry)) != 0)
				return false;
		}
		for (uint city = 0; city < 2; ++city) {
			WRITE_BE_UINT16(&_data[arrival + 38 * city + 10], 0x0044);
			WRITE_BE_UINT16(&_data[arrival + 38 * city + 12], kHotelArrivals[layout].roomSelection);
		}
		return true;
	}
	return false;
}

// Rio/Iguacu arrivals draw the helicopter without setting v[11], so it
// cannot be boarded. Reuse the existing draw-and-enable helpers, after
// patchHotelRoomState has put the temporary reset before room selection.
//
// Applies to: All versions
// Script: LOADHOT.TOT / LOADHOT.TOA
bool OldTotData::patchHotelHelicopter(Common::Language language) {
	for (uint layout = 0; layout < ARRAYSIZE(kHotelArrivals); ++layout) {
		const HotelArrivalLayout &site = kHotelArrivals[layout];
		if (site.language != language)
			continue;
		if (!has(site.arrival, 76) || !has(site.helicopter, 26) ||
			uint16BEAt(site.helicopter) != 0x1C02)
			return false;
		for (uint city = 0; city < 2; ++city) {
			byte entry[] = {0x1C, 13, 0xFF, 5, 0, 44, 0, 0,
				0x09, 0x45, 0, 0x44, 0, 0}; // Time, reset, room selection.
			entry[7] = city == 0 ? 2 : 32;
			WRITE_BE_UINT16(entry + 12, site.roomSelection);
			const uint16 arrival = site.arrival + 38 * city;
			const uint16 helper = site.helicopter + (city == 0 ? 16 : 6);
			byte enabled[] = {0x1C, 2, 0, 0, 0xFF, 5, 0, 11, 0, 0};
			WRITE_BE_UINT16(enabled + 2, site.helicopter);
			enabled[9] = city == 0 ? 1 : 2;
			if (memcmp(&_data[arrival], entry, sizeof(entry)) != 0 ||
				uint16BEAt(arrival + 31) != site.helicopter ||
				memcmp(&_data[helper], enabled, sizeof(enabled)) != 0)
				return false;
		}
		WRITE_BE_UINT16(&_data[site.arrival + 31], site.helicopter + 16);
		WRITE_BE_UINT16(&_data[site.arrival + 38 + 31], site.helicopter + 6);
		return true;
	}
	return false;
}

// The favela vendor debits the price before testing for negative cash.
// Subtraction saturates at zero, so an unaffordable offer is accepted.
// Compare first and debit only affordable offers. Each shortened offer leaves
// six bytes for cleanup before the existing rejection scene; the old refund
// records hold the paid path. Record sizes and following offsets stay unchanged.
//
// Applies to: All versions
// Script: LOADFAV.TOT / LOADFAV.TOA
bool OldTotData::patchVendorAffordability(Common::Language language) {
	static const struct {
		Common::Language language;
		uint16 firstOffer, rejected, paidOutcome;
	} layouts[] = {
		{Common::EN_ANY, 0x1505, 0x15F9, 0x149E},
		{Common::ES_ESP, 0x150D, 0x1601, 0x14A6},
		{Common::FR_FRA, 0x14F9, 0x15ED, 0x1492},
		{Common::DE_DEU, 0x152B, 0x161F, 0x14C4}
	};
	const uint16 prices[] = {50, 10, 1};
	for (uint layout = 0; layout < ARRAYSIZE(layouts); ++layout) {
		if (layouts[layout].language != language)
			continue;
		const uint16 first = layouts[layout].firstOffer;
		if (!has(first - 72, 114))
			return false;
		for (uint i = 0; i < ARRAYSIZE(prices); ++i) {
			const uint16 offer = first + 14 * i;
			const uint16 condition = first - 12 * (i + 1);
			const uint16 refund = first - 48 - 12 * i;
			byte debitFirst[] = {0x1C, 4, 0xFF, 2, 0xFF, 2, 0xFF, 9, 0, 21, 0, 0, 0, 0};
			byte negative[] = {0x21, 0, 0, 21, 0, 0, 1, 0, 0, 1, 0, 0};
			byte refundThenReject[] = {0x1C, 3, 0xFF, 7, 0, 21, 0, 0, 0xFF, 2, 0, 0};
			WRITE_BE_UINT16(debitFirst + 10, prices[i]);
			WRITE_BE_UINT16(debitFirst + 12, condition);
			WRITE_BE_UINT16(negative + 7, layouts[layout].paidOutcome);
			WRITE_BE_UINT16(negative + 10, refund);
			// The original refund records list the prices in reverse order.
			WRITE_BE_UINT16(refundThenReject + 6, prices[2 - i]);
			WRITE_BE_UINT16(refundThenReject + 10, layouts[layout].rejected);
			if (memcmp(&_data[offer], debitFirst, sizeof(debitFirst)) != 0 ||
				memcmp(&_data[condition], negative, sizeof(negative)) != 0 ||
				memcmp(&_data[refund], refundThenReject, sizeof(refundThenReject)) != 0)
				return false;
		}
		for (uint i = 0; i < ARRAYSIZE(prices); ++i) {
			const uint16 offer = first + 14 * i;
			const uint16 condition = first - 12 * (i + 1);
			const uint16 debit = first - 48 - 12 * i;
			byte restoreThenCheck[] = {0x1C, 3, 0xFF, 2, 0xFF, 2, 0, 0,
				0x1C, 2, 0xFF, 2, 0, 0}; // Rejection cleanup follows the offer.
			byte debitThenContinue[] = {0x1C, 2, 0xFF, 9, 0, 21, 0, 0, 0, 0, 0, 0};
			WRITE_BE_UINT16(restoreThenCheck + 6, condition);
			WRITE_BE_UINT16(restoreThenCheck + 12, layouts[layout].rejected);
			WRITE_BE_UINT16(debitThenContinue + 6, prices[i]);
			WRITE_BE_UINT16(debitThenContinue + 8, layouts[layout].paidOutcome);
			memcpy(&_data[offer], restoreThenCheck, sizeof(restoreThenCheck));
			memcpy(&_data[debit], debitThenContinue, sizeof(debitThenContinue));
			WRITE_BE_UINT16(&_data[condition + 4], prices[i]);
			WRITE_BE_UINT16(&_data[condition + 7], debit);
			WRITE_BE_UINT16(&_data[condition + 10], offer + 8);
		}
		return true;
	}
	return false;
}

// The French/German lobby omits the guest reset used in English/Spanish.
// After a completed encounter, selection can leave the old guest clickable
// even though that guest is no longer drawn. Clear v[13] before selecting.
//
// Applies to: French DOS/ST/Amiga and German Amiga
// Script: LOADHOT.TOT / LOADHOT.TOA
bool OldTotData::patchLobbyGuestReset(OldTotPlatformTag platform,
		Common::Language language) {
	if (language != Common::FR_FRA && language != Common::DE_DEU)
		return false;
	const uint16 lobby = language == Common::DE_DEU ? 0x7840 : 0x776A;
	const byte frenchST[] = {0x1E, 13, 0xFF, 11, 44, 0xD1, 0xF7, 0xAA, 0x61,
		0xAA, 0x79, 0xAA, 0x6D, 1, 0xCA, 9, 0x59, 0x5C, 0x5D, 0x68, 0xDB,
		0x77, 0x3C, 0x72, 0xAA, 0x76, 0xE5, 0x76, 0xC0};
	const byte frenchAmiga[] = {0x1E, 13, 0xFF, 11, 44, 0xE1, 0x56, 0xB1, 0x86,
		0xB1, 0x9E, 0xB1, 0x92, 1, 0xCA, 9, 0x59, 0x5C, 0x5D, 0x68, 0xDB,
		0x77, 0x3C, 0x72, 0xAA, 0x76, 0xE5, 0x76, 0xC0};
	const byte germanAmiga[] = {0x1E, 13, 0xFF, 11, 44, 0xE2, 0x2C, 0xB2, 0x5C,
		0xB2, 0x74, 0xB2, 0x68, 1, 0xCA, 9, 0x59, 0x5D, 0x33, 0x69, 0xB1,
		0x78, 0x12, 0x73, 0x80, 0x77, 0xBB, 0x77, 0x96};
	const byte *original = language == Common::DE_DEU ? germanAmiga :
		platform == kOldTotPlatformAmiga ? frenchAmiga : frenchST;
	if (_data.size() >= 0xFF00 || !has(lobby, 29) ||
		memcmp(&_data[lobby], original, 29) != 0)
		return false;
	const uint16 guest = uint16BEAt(lobby + 27);
	byte selector[] = {8, 0, 20, 3, 6, 8, 12, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
	WRITE_BE_UINT16(selector + 8, guest - 7);
	for (uint i = 0; i < 3; ++i)
		WRITE_BE_UINT16(selector + 11 + 3 * i, guest - 36);
	if (!has(guest, sizeof(selector)) ||
		memcmp(&_data[guest], selector, sizeof(selector)) != 0)
		return false;

	// Keep every original call in place; replace only the final selection call.
	const uint16 helper = _data.size();
	const byte reset[] = {0x1C, 2, 0xFF, 5, 0, 13, 0, 0};
	_data.resize(helper + 10);
	memcpy(&_data[helper], reset, sizeof(reset));
	WRITE_BE_UINT16(&_data[helper + 8], guest);
	WRITE_BE_UINT16(&_data[lobby + 27], helper);
	return true;
}

// At Iguacu/Manaus's special hours, these Amiga scripts use Rio's score
// test. Below 75 this selects Rio's character/visit flag in the wrong city.
// Use the existing local tests; their successful ending is already the same.
//
// Applies to: French and German Amiga
// Script: LOADAERO.TOA
bool OldTotData::patchAirportFallback(OldTotPlatformTag platform,
		Common::Language language) {
	if (platform != kOldTotPlatformAmiga)
		return false;
	static const struct {
		Common::Language language;
		uint16 first, rio, rioOrdinary, ending, ordinary[2];
	} layouts[] = {
		{Common::FR_FRA, 0x4706, 0x46DF, 0x46D9, 0x3B2A, {0x4693, 0x46AD}},
		{Common::DE_DEU, 0x4856, 0x482F, 0x4829, 0x3C7A, {0x47E3, 0x47FD}}
	};
	for (uint layout = 0; layout < ARRAYSIZE(layouts); ++layout) {
		if (layouts[layout].language != language)
			continue;
		byte rio[] = {0x21, 0, 0, 30, 0, 75, 1, 0, 0, 1, 0, 0};
		WRITE_BE_UINT16(rio + 7, layouts[layout].ending);
		WRITE_BE_UINT16(rio + 10, layouts[layout].rioOrdinary);
		if (!has(layouts[layout].rio, sizeof(rio)) ||
			memcmp(&_data[layouts[layout].rio], rio, sizeof(rio)) != 0)
			return false;
		for (uint city = 0; city < 2; ++city) {
			const uint16 local = layouts[layout].first - 12 + 23 * city;
			byte condition[] = {0x21, 0, 0, 30, 0, 75, 1, 0, 0, 1, 0, 0,
				8, 0, 20, 1, 0, 1, 0, 0, 1, 0, 0};
			condition[16] = city == 0 ? 11 : 20;
			WRITE_BE_UINT16(condition + 7, layouts[layout].ending);
			WRITE_BE_UINT16(condition + 10, layouts[layout].ordinary[city]);
			WRITE_BE_UINT16(condition + 18, layouts[layout].ordinary[city]);
			WRITE_BE_UINT16(condition + 21, layouts[layout].rio);
			if (!has(local, sizeof(condition)) ||
				memcmp(&_data[local], condition, sizeof(condition)) != 0)
				return false;
		}
		for (uint city = 0; city < 2; ++city)
			WRITE_BE_UINT16(&_data[layouts[layout].first + 9 + 23 * city],
				layouts[layout].first - 12 + 23 * city);
		return true;
	}
	return false;
}

// Roulette result 18 settles and immediately clears its display, unlike the
// other 36 results. Keep its six drawing calls and settlement, then wait for input.
// Append the replacement so that all existing record addresses stay valid.
//
// Applies to: All versions
// Script: LOADCAS.TOT / LOADCAS.TOA
bool OldTotData::patchRouletteConfirmation(OldTotPlatformTag platform,
		Common::Language language) {
	static const struct {
		Common::Language language;
		bool amiga;
		uint16 result, draws[6];
	} layouts[] = {
		{Common::EN_ANY, false, 0x499B, {0x6460, 0x66FE, 0x6AAF, 0x6B6D, 0x4847, 0x66B6}},
		{Common::ES_ESP, false, 0x4816, {0x62DB, 0x6579, 0x692A, 0x69E8, 0x46C2, 0x6531}},
		{Common::FR_FRA, false, 0x496F, {0x6434, 0x66D2, 0x6A83, 0x6B41, 0x481B, 0x668A}},
		{Common::EN_ANY, true, 0x499B, {0x6536, 0x6845, 0x6CAF, 0x6D6E, 0x4847, 0x67F8}},
		{Common::ES_ESP, true, 0x4816, {0x63B1, 0x66C0, 0x6B2A, 0x6BE9, 0x46C2, 0x6673}},
		{Common::FR_FRA, true, 0x496F, {0x650A, 0x6819, 0x6C83, 0x6D42, 0x481B, 0x67CC}},
		{Common::DE_DEU, true, 0x49DE, {0x6579, 0x6888, 0x6CF2, 0x6DB1, 0x488A, 0x683B}}
	};
	for (uint i = 0; i < ARRAYSIZE(layouts); ++i) {
		if (layouts[i].language != language ||
				layouts[i].amiga != (platform == kOldTotPlatformAmiga))
			continue;
		byte original[] = {0x1C, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 11, 101};
		for (uint draw = 0; draw < 6; ++draw)
			WRITE_BE_UINT16(original + 2 + 2 * draw, layouts[i].draws[draw]);
		if (_data.size() >= 0xFF00 || !has(layouts[i].result, sizeof(original)) ||
				memcmp(&_data[layouts[i].result], original, sizeof(original)) != 0)
			return false;

		// Addresses starting with FF are reserved for inline instructions.
		const uint16 replacement = _data.size();
		_data.resize(replacement + 20);
		memcpy(&_data[replacement], original, sizeof(original));
		_data[replacement + 1] = 8;
		_data[replacement + 17] = 0xFF;
		_data[replacement + 18] = 3;
		_data[replacement + 19] = 0;
		_data[layouts[i].result + 1] = 1;
		WRITE_BE_UINT16(&_data[layouts[i].result + 2], replacement);
		return true;
	}
	return false;
}

// Three Spanish LOADGAR replies start with a blank line that pushes their
// final refusal out of the text rectangle. Replace that first break with a
// space, retaining every record boundary and the continuation waits.
// Applies to: All Spanish versions.
bool OldTotData::patchSpanishReplies() {
	static const struct {
		uint16 offset, x, y, width, height;
		const char *text;
	} replies[] = {
		{0x1BA0, 104, 19, 128, 16, "@     OH NO!@    ESO NO!"},
		{0x1BC8, 104, 19, 128, 16, "@   NO ERES MUY@      HABIL!"},
		{0x2A1E, 40, 5, 256, 32, "@   SE\xA4OR, NO TENGO EL GUSTO DE@    CONCERLE. QUIEN ES PARA@         COLARSE EN MI@        PALCO? MARCHESE!"}
	};
	for (uint i = 0; i < ARRAYSIZE(replies); ++i) {
		byte header[] = {0x1D, 0, 0, 0, 0, 0, 0, 0, 0, 8, 15, 0, 0, 0, 0};
		WRITE_BE_UINT16(header + 1, replies[i].x);
		WRITE_BE_UINT16(header + 3, replies[i].y);
		WRITE_BE_UINT16(header + 5, replies[i].width);
		WRITE_BE_UINT16(header + 7, replies[i].height);
		const uint length = strlen(replies[i].text) + 1;
		if (!has(replies[i].offset, sizeof(header) + length) ||
			memcmp(&_data[replies[i].offset], header, sizeof(header)) != 0 ||
			memcmp(&_data[replies[i].offset + sizeof(header)], replies[i].text, length) != 0)
			return false;
	}
	for (uint i = 0; i < ARRAYSIZE(replies); ++i)
		_data[replies[i].offset + 15] = ' ';
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
