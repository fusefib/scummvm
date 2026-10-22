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

#include "gob/pregob/emmanuelle/runtime.h"
#include "gob/pregob/emmanuelle/atari.h"
#include "gob/util.h"

namespace Gob {
namespace Emmanuelle {

InputLatch::InputLatch() {
	_key = 0;
	_mouseX = 0;
	_mouseY = 0;
	_mouseButtons = 0;
}

IntroSkipState::IntroSkipState() {
	_state = kUnavailable;
}

void IntroSkipState::begin() {
	_state = kPlaying;
}

bool IntroSkipState::request() {
	if (_state != kPlaying)
		return false;
	_state = kRequested;
	return true;
}

bool IntroSkipState::activate(bool quitRequested) {
	if (_state != kRequested || quitRequested)
		return false;
	_state = kSkipping;
	return true;
}

void IntroSkipState::finish() {
	_state = kUnavailable;
}

void InputLatch::noteKey(int16 key) {
	if (_key == 0 && key != 0)
		_key = key;
}

void InputLatch::noteLatestKey(int16 key) {
	if (key != 0)
		_key = key;
}

void InputLatch::noteMouse(int16 x, int16 y, byte buttons) {
	_mouseX = x;
	_mouseY = y;
	_mouseButtons |= buttons;
}

void InputLatch::clear() {
	_key = 0;
	_mouseButtons = 0;
}

int16 InputLatch::peek(int16 &x, int16 &y, byte &buttons) const {
	x = _mouseX;
	y = _mouseY;
	buttons = _mouseButtons;
	return _key;
}

int16 InputLatch::take(int16 &x, int16 &y, byte &buttons) {
	const int16 key = _key;
	x = _mouseX;
	y = _mouseY;
	buttons = _mouseButtons;
	_key = 0;
	_mouseButtons = 0;
	return key;
}

bool isAudioPlaying(OldTotPlatformTag platform, bool speakerActive,
		bool samplePlaying) {
	return samplePlaying || (platform == kOldTotPlatformDOS && speakerActive);
}

uint16 accumulateTimerQuanta(OldTotPlatformTag platform,
									 uint32 &accumulator) {
	// DOS services 5000 calibrated units/s; ST services one tick per VBL.
	accumulator += platform == kOldTotPlatformDOS ? 5000 :
		50;
	const uint16 quanta = (uint16)(accumulator / kHostFrameRate);
	accumulator %= kHostFrameRate;
	return quanta;
}

uint32 minimumDurationRemaining(uint32 elapsed,
									  uint32 minimumDuration) {
	return elapsed < minimumDuration ? minimumDuration - elapsed : 0;
}

bool isTitlePresentationVisible(OldTotPlatformTag platform,
									   bool black, bool framePending,
									   bool presentationActive,
									   bool paletteTransitionPending) {
	if (platform == kOldTotPlatformDOS)
		return true;
	if (platform == kOldTotPlatformAtariST)
		return !black && !framePending && !presentationActive;
	return false;
}

uint16 opcode03DelayUnits(byte arg) {
	return arg == 0 || arg == 0xFF ? 0 : (uint16)arg * 5;
}

uint16 inputEntryDelay(OldTotPlatformTag platform) {
	return platform == kOldTotPlatformDOS ? 0 : 100;
}

InputBoundary choiceInputBoundary(OldTotPlatformTag platform,
										bool initialEntry, bool reentering) {
	InputBoundary boundary;
	const bool entering = initialEntry || reentering;
	if (!entering)
		return boundary;
	if (initialEntry)
		boundary.nativeDelayUnits = inputEntryDelay(platform);
	boundary.discardStale = platform != kOldTotPlatformDOS;
	boundary.showCursor = true;
	return boundary;
}

byte keyboardCursorStep(OldTotPlatformTag platform) {
	if (platform == kOldTotPlatformDOS)
		return 4;
	if (platform == kOldTotPlatformAtariST)
		return 1;
	return 0;
}

int16 minimumCursorOrigin(OldTotPlatformTag platform) {
	return platform == kOldTotPlatformDOS ? 4 : 0;
}

bool recentersMouseOnModuleLoad(OldTotPlatformTag platform) {
	return true;
}

bool schedulerChecksZeroBeforeDecrement(OldTotPlatformTag platform) {
	return platform != kOldTotPlatformDOS;
}

bool command201SkipsIntermediateNodes(OldTotPlatformTag platform,
									 uint16 moduleSelector) {
	return platform != kOldTotPlatformDOS && moduleSelector == 3;
}

bool decodeProtectionPrompt(const byte raw[4], ProtectionPrompt &out) {
	out.letter = (raw[0] & 0xC0) >> 6;
	const uint16 packed = ((uint16)raw[1] << 8) | raw[2];
	const int32 box = (int32)packed - 0x1C20 - ((int32)raw[1] << 5);
	out.box = box < 0 ? 0 : (uint16)box;
	out.color = (raw[3] & 0x70) >> 4;
	return out.letter < 4 && out.color < 8;
}

bool decodeProtectionRecord(const byte raw[8], ProtectionChallenge &out) {
	return decodeProtectionPrompt(raw, out.first) && decodeProtectionPrompt(raw + 4, out.second);
}

void installProtectionChallenge(const ProtectionChallenge &c, uint16 vars[7]) {
	vars[1] = c.first.letter;
	vars[2] = c.first.box;
	vars[3] = c.first.color;
	vars[4] = c.second.letter;
	vars[5] = c.second.box;
	vars[6] = c.second.color;
}

const char *protectionColorName(byte color) {
	switch (color) {
	case 0:
		return "white";
	case 1:
		return "black";
	case 2:
		return "yellow";
	case 3:
		return "green";
	case 4:
		return "blue";
	case 5:
		return "red";
	case 6:
		return "grey";
	case 7:
		return "brown";
	default:
		return nullptr;
	}
}

bool buildProtectionHint(bool initModule, bool suspended, byte opcode,
						 byte regionCount, const uint16 vars[7],
						 ProtectionChallenge &out) {
	if (!initModule || !suspended || opcode != 0x1B || regionCount != 8 ||
		!vars || vars[1] >= 4 || vars[3] >= 8 || vars[4] >= 4 ||
		vars[6] >= 8)
		return false;

	ProtectionChallenge challenge;
	challenge.first.letter = (byte)vars[1];
	challenge.first.box = vars[2];
	challenge.first.color = (byte)vars[3];
	challenge.second.letter = (byte)vars[4];
	challenge.second.box = vars[5];
	challenge.second.color = (byte)vars[6];
	out = challenge;
	return true;
}

// Atari ST policy.

namespace {

const byte kAtariStIntroSlots[363] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	1, 14, 9, 7, 2, 4, 12, 12, 1, 14, 9, 7, 2, 4, 12, 12,
	7, 16, 10, 4, 11, 7, 16, 16, 7, 7, 10, 4, 7, 7, 11, 16,
	7, 7, 11, 7, 3, 14, 3, 4, 3, 14, 6, 4, 3, 14, 3, 4,
	3, 14, 6, 7, 3, 14, 15, 4, 3, 14, 6, 4, 3, 14, 3, 4,
	3, 14, 7, 7, 11, 11, 7, 16, 16, 15, 13, 9, 5, 6, 13, 9,
	16, 15, 13, 9, 5, 6, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 1, 14, 7, 11, 9, 7, 7, 11, 2, 4,
	12, 12, 1, 14, 9, 7, 2, 4, 12, 12, 7, 11, 16, 10, 4, 7,
	11, 16, 7, 14, 11, 7, 7, 11, 7, 16, 11, 14, 14, 7, 11, 7,
	16, 10, 14, 6, 16, 10, 14, 6, 8, 10, 14, 6, 2, 10, 14, 6,
	14, 15, 13, 9, 5, 6, 13, 9, 7, 11, 13, 9, 5, 6, 13, 9,
	11, 9, 7, 11, 7, 9, 7, 11, 16, 7, 5, 2, 14, 15, 5, 2,
	14, 15, 8, 2, 14, 14, 14, 15, 16, 7, 11, 16, 6, 7, 7, 15,
	14, 11, 7, 16, 6, 7, 8, 11, 14, 7, 15, 15, 6, 10, 7, 10,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	1, 14, 9, 7, 2, 4, 12, 12, 1, 14, 9, 7, 2, 4, 12, 12,
	7, 16, 10, 4, 11, 7, 16, 16, 7, 7, 10, 4, 7, 7, 11, 16,
	7, 7, 11, 7, 3, 14, 3, 4, 3, 14, 6, 4, 3, 14, 3, 4,
	3, 14, 6, 7, 3, 14, 15, 4, 3, 14, 6, 4, 3, 14, 3, 4,
	3, 14, 7, 7, 11, 11, 7, 16, 16, 15, 13};

const uint16 kAtariStPalettes[5][16] = {
	{0x000, 0x777, 0x005, 0x127, 0x557, 0x030, 0x040, 0x444,
	 0x666, 0x700, 0x000, 0x726, 0x410, 0x632, 0x752, 0x774},
	{0x000, 0x504, 0x005, 0x127, 0x557, 0x030, 0x040, 0x444,
	 0x777, 0x700, 0x000, 0x726, 0x410, 0x632, 0x752, 0x774},
	{0x000, 0x233, 0x005, 0x127, 0x557, 0x030, 0x344, 0x444,
	 0x777, 0x700, 0x000, 0x566, 0x410, 0x632, 0x752, 0x774},
	{0x010, 0x777, 0x003, 0x004, 0x016, 0x047, 0x357, 0x567,
	 0x200, 0x400, 0x731, 0x752, 0x700, 0x735, 0x040, 0x072},
	{0x000, 0x223, 0x004, 0x115, 0x334, 0x020, 0x040, 0x232,
	 0x773, 0x722, 0x000, 0x726, 0x157, 0x775, 0x430, 0x777}};

const uint16 kAtariStEffect45Sets[3][4] = {
	{0x400, 0x700, 0x753, 0x765},
	{0x004, 0x006, 0x337, 0x567},
	{0x030, 0x060, 0x070, 0x576}};

const uint16 kAtariStMode43Masks[16] = {
	0x8000, 0x0800, 0x0080, 0x0008,
	0x4000, 0x0400, 0x0040, 0x0004,
	0x2000, 0x0200, 0x0020, 0x0002,
	0x1000, 0x0100, 0x0010, 0x0001};

void copyWords(const uint16 *source, uint count, uint16 *destination) {
	for (uint i = 0; i < count; ++i)
		destination[i] = source[i];
}

} // End of anonymous namespace

bool getAtariStSampleCommand(byte command, AtariStSampleCommand &out) {
	out = AtariStSampleCommand();
	switch (command) {
	case 82:
		out.slot = 0;
		out.stopMode = kAtariStAudioStopIfPlaying;
		out.postDelay = 50;
		return true;
	case 83:
		out.slot = 1;
		out.stopMode = kAtariStAudioStopAlways;
		out.mixerRepeatCount = -1;
		return true;
	case 85:
		out.slot = 16;
		out.stopMode = kAtariStAudioStopAlways;
		out.mixerRepeatCount = -1;
		return true;
	case 86:
		out.slot = 2;
		out.stopMode = kAtariStAudioStopAlways;
		out.mixerRepeatCount = -1;
		return true;
	case 89:
		out.slot = 3;
		out.stopMode = kAtariStAudioStopAlways;
		out.waitForCompletion = true;
		return true;
	case 90:
		out.slot = 4;
		out.stopMode = kAtariStAudioStopAlways;
		out.mixerRepeatCount = -1;
		return true;
	case 91:
		out.slot = 5;
		out.stopMode = kAtariStAudioStopAlways;
		out.mixerRepeatCount = -1;
		return true;
	case 92:
		out.slot = 6;
		out.stopMode = kAtariStAudioStopAlways;
		out.waitForCompletion = true;
		return true;
	case 94:
		out.slot = 7;
		out.stopMode = kAtariStAudioStopAlways;
		out.waitForCompletion = true;
		return true;
	default:
		return false;
	}
}

bool atariStHandlesNativeCommand(byte command) {
	switch (command) {
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 80:
	case 81:
	case 82:
	case 83:
	case 85:
	case 86:
	case 89:
	case 90:
	case 91:
	case 92:
	case 94:
	case 201:
		return true;
	default:
		return false;
	}
}

AtariStAudioPolicy atariStAudioPolicy(bool optionalAudio,
									 bool anySamplePresent) {
	if (optionalAudio && !anySamplePresent)
		return kAtariStAudioSilent;
	return kAtariStAudioRequired;
}

const byte *getAtariStIntroComposition(uint &count) {
	count = ARRAYSIZE(kAtariStIntroSlots);
	return kAtariStIntroSlots;
}

uint16 atariStRandomBounded(uint32 nativeRandomValue,
							uint16 upperExclusive) {
	if (upperExclusive == 0)
		return 0;
	return (uint16)((nativeRandomValue & 0x0FFFU) % upperExclusive);
}

void getAtariStSchedulerTiming(byte period, uint16 &reload,
							   uint16 &initialCountdown) {
	reload = period == 200 ? 170 : period;
	initialCountdown = 0;
}

bool getAtariStModulePalette(const Common::String &moduleName,
							 uint16 words[16]) {
	byte paletteNumber = 0;
	if (moduleName.equalsIgnoreCase("INIT") ||
		moduleName.equalsIgnoreCase("INIT.TOT")) {
		paletteNumber = 5;
	} else if (moduleName.equalsIgnoreCase("LOADCHUT") ||
			   moduleName.equalsIgnoreCase("LOADCHUT.TOT")) {
		paletteNumber = 3;
	} else if (moduleName.equalsIgnoreCase("LOADPLA") ||
			   moduleName.equalsIgnoreCase("LOADPLA.TOT") ||
			   moduleName.equalsIgnoreCase("LOADAERO") ||
			   moduleName.equalsIgnoreCase("LOADAERO.TOT") ||
			   moduleName.equalsIgnoreCase("LOADGAR") ||
			   moduleName.equalsIgnoreCase("LOADGAR.TOT")) {
		paletteNumber = 2;
	} else if (moduleName.equalsIgnoreCase("LOADHOT") ||
			   moduleName.equalsIgnoreCase("LOADHOT.TOT") ||
			   moduleName.equalsIgnoreCase("LOADCAS") ||
			   moduleName.equalsIgnoreCase("LOADCAS.TOT") ||
			   moduleName.equalsIgnoreCase("LOADFAV") ||
			   moduleName.equalsIgnoreCase("LOADFAV.TOT")) {
		paletteNumber = 1;
	}
	return getAtariStNumberedPalette(paletteNumber, words);
}

bool getAtariStNumberedPalette(byte paletteNumber, uint16 words[16]) {
	if (!words || paletteNumber < 1 || paletteNumber > 5)
		return false;
	copyWords(kAtariStPalettes[paletteNumber - 1], 16, words);
	return true;
}

bool getAtariStEffect45(byte variant, uint16 words[4]) {
	if (!words || variant >= ARRAYSIZE(kAtariStEffect45Sets))
		return false;
	copyWords(kAtariStEffect45Sets[variant], 4, words);
	return true;
}

byte atariStPaletteComponent(uint16 word, byte shift) {
	return (byte)(((word >> shift) & 7) * 9);
}

uint16 dimAtariStPaletteWord(uint16 word, byte steps) {
	byte red = (word >> 8) & 7;
	byte green = (word >> 4) & 7;
	byte blue = word & 7;
	red = red > steps ? red - steps : 0;
	green = green > steps ? green - steps : 0;
	blue = blue > steps ? blue - steps : 0;
	return (uint16)((red << 8) | (green << 4) | blue);
}

byte prepareAtariStWaitColorCounter(byte counter) {
	return counter < 4 ? 4 : counter;
}

uint16 atariStAnimationDelayChunk(uint16 remainingUnits) {
	return remainingUnits > 10 ? 10 : remainingUnits;
}

bool atariStContinuationAcceptsKey(int16 key) {
	return key != kKeyNone && key != kKeyLeft && key != kKeyRight &&
		   key != kKeyUp && key != kKeyDown;
}

uint16 atariStMode43Mask(byte phase, uint32 wordOrdinal) {
	return kAtariStMode43Masks[(phase + wordOrdinal) & 15];
}

bool getAtariStRestoredDirtyBounds(uint16 x, uint16 width,
	uint16 screenWidth, uint16 &left, uint16 &right) {
	if (width == 0 || screenWidth == 0 || x >= screenWidth)
		return false;
	left = (uint16)((x / 16U) * 16U);
	const uint32 endWord = ((uint32)x + width) / 16U + 1U;
	const uint32 end = MIN<uint32>(screenWidth, endWord * 16U);
	right = (uint16)(end - 1U);
	return true;
}

byte atariStMode44PhaseCount(byte variant) {
	return variant == 0 ? 9 : (variant < 5 ? 18 : 0);
}

bool getAtariStMode44Rect(byte variant, byte phase,
	AtariStMode44Rect &out) {
	const byte phaseCount = atariStMode44PhaseCount(variant);
	if (!phaseCount || phase >= phaseCount)
		return false;
	const byte stage = phase + 1;
	if (variant == 0) {
		out.x = 144 - (int16)stage * 16;
		out.y = 90 - (int16)stage * 10;
		out.width = (uint16)(32 + stage * 32);
		out.height = (uint16)(20 + stage * 20);
	} else {
		out.width = (uint16)(32 + stage * 16);
		out.height = (uint16)(20 + stage * 10);
		out.x = variant == 1 || variant == 2
			? 320 - (int16)out.width : 0;
		out.y = variant == 2 || variant == 4
			? 200 - (int16)out.height : 0;
	}
	return true;
}

byte atariStAnimationVblCount() {
	return 4;
}

} // namespace Emmanuelle
} // namespace Gob
