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
#include "gob/pregob/emmanuelle/amiga.h"
#include "gob/util.h"

namespace Gob {
namespace Emmanuelle {

PresentationState::PresentationState(OldTotPlatformTag platform) :
	_platform(platform) {
	_mode = 0;
	_variant = 0;
	_phase = 0;

	_pending = false;
	_active = false;
	_paletteTransitionPending = false;
}

void PresentationState::resetModule() {
	if (_platform != kOldTotPlatformAmiga)
		_mode = 0;
	_variant = 0;
	_phase = 0;
	_pending = false;
	_active = false;
	_paletteTransitionPending = false;
}

void PresentationState::arm(byte mode) {
	_mode = mode == 43 || mode == 44 ? mode : 0;
	_variant = 0;
	_phase = 0;
	_active = false;
}

void PresentationState::commitDirectFrame() {
	_pending = false;
	_active = false;
	_phase = 0;
}

bool PresentationState::nextStep(bool black, byte randomVariant,
		PresentationStep &step, bool sharedCadence) {
	step = PresentationStep();
	if (!_pending && !_active)
		return false;
	if (!_active) {
		_active = true;
		_phase = 0;
		_variant = _mode == 44 ? randomVariant : 0;
	}

	const bool atari = _platform == kOldTotPlatformAtariST;
	step.variant = _variant;
	step.phase = _phase;
	// ST restores black before any transition; Amiga mode 43 takes priority.
	if (black && (atari ||
		(_mode != 43 && (_paletteTransitionPending || _mode == 44)))) {
		step.operation = kPresentationFadeFromBlack;
		step.complete = true;
	} else if (_mode == 43) {
		step.operation = kPresentationMode43;
		step.complete = _phase == 15;
	} else if (_mode == 44) {
		step.operation = kPresentationMode44;
		const byte count = _variant == 0 ? 9 : (_variant < 5 ? 18 : 0);
		// Amiga presents stage zero before its counted waits.
		step.complete = _phase + (atari ? 1 : 0) == count;
	} else {
		step.operation = kPresentationFrame;
		// Gameplay shares Amiga's pre-publication and post-publication waits.
		step.complete = (!sharedCadence && _platform != kOldTotPlatformAmiga) || _phase == 1;
	}

	if (step.complete) {
		_pending = false;
		_active = false;
		_phase = 0;
		_variant = 0;
		_paletteTransitionPending = false;
		if (_mode == 44)
			_mode = 0;
	} else {
		++_phase;
	}
	return true;
}

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
	// Keep the press position when later events move or release the pointer.
	if (_mouseButtons == 0) {
		_mouseX = x;
		_mouseY = y;
	}
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

bool isAudioReadyForSave(OldTotPlatformTag platform, bool speakerActive,
		bool samplePlaying, bool blocking) {
	// Amiga samples can continue through ordinary choices, unlike DOS tones
	// and ST input-time playback. Save their restart state instead of waiting
	// for a finite effect to finish or blocking forever on a looping sample.
	return !isAudioPlaying(platform, speakerActive, samplePlaying) ||
		(platform == kOldTotPlatformAmiga && !blocking);
}

uint64 accumulateTimerQuanta(OldTotPlatformTag platform,
		uint32 &accumulator, uint32 ticks) {
	// DOS services 5000 calibrated units/s, ST one tick per VBL, and Amiga
	// 20 ticks/s. Logical service ticks stay at 50 Hz regardless of polling.
	const uint rate = platform == kOldTotPlatformDOS ? 5000 :
		(platform == kOldTotPlatformAtariST ? 50 : 20);
	const uint64 total = accumulator + (uint64)rate * ticks;
	const uint64 quanta = total / kHostFrameRate;
	accumulator = total % kHostFrameRate;
	return quanta;
}

uint32 minimumDurationRemaining(uint32 elapsed,
									  uint32 minimumDuration) {
	return elapsed < minimumDuration ? minimumDuration - elapsed : 0;
}

byte normalizeAnimationSpeed(int factor) {
	if (factor < 0)
		return 0;
	if (factor > 5)
		return 5;
	return (byte)factor;
}

bool isTitlePresentationVisible(OldTotPlatformTag platform,
									   bool black, bool framePending,
									   bool presentationActive,
									   bool paletteTransitionPending) {
	if (platform == kOldTotPlatformDOS)
		return true;
	if (platform == kOldTotPlatformAtariST)
		return !black && !framePending && !presentationActive;
	if (platform == kOldTotPlatformAmiga)
		return !black && !framePending && !presentationActive &&
			!paletteTransitionPending;
	return false;
}

ColorCycleResult advanceColorCycle(byte counter, bool enabled,
												uint16 selector) {
	ColorCycleResult result;
	result.counter = (byte)((counter + 1) % 6);
	result.rotatePalette =
		result.counter == 0 && enabled && selector == 10;
	return result;
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
	boundary.discardStale = true;
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
	return platform != kOldTotPlatformAmiga;
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
	// One host pass supplies one ST tick; zero is tested before decrementing.
	if (period == 50 || period == 100 || period == 200)
		reload = (period == 50 ? 1500 : 2000) / 20 - 1;
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

// Amiga policy.

namespace {

const uint16 kAmigaPalettes[5][32] = {
	{0x000, 0xFEE, 0xF00, 0x4F0, 0x211, 0x311, 0x511, 0x700,
	 0xD00, 0xF0C, 0xB08, 0x705, 0x041, 0x061, 0x191, 0x3B2,
	 0x420, 0x900, 0xC40, 0xE61, 0xE83, 0xFB4, 0xFD6, 0xFE8,
	 0x113, 0x309, 0x45F, 0x0CF, 0x334, 0x557, 0x889, 0xBBC},
	{0x000, 0xFFF, 0xF00, 0x4F0, 0x211, 0x9EF, 0xBFF, 0xDFF,
	 0xD00, 0xF0C, 0xB08, 0x705, 0x041, 0x061, 0x191, 0x3B2,
	 0x420, 0x900, 0xC40, 0xE71, 0xE83, 0xFB4, 0xFD6, 0xFE8,
	 0x113, 0x309, 0x45F, 0x0CF, 0x334, 0x557, 0x889, 0xBBC},
	{0x000, 0xFEE, 0x000, 0x000, 0x000, 0x446, 0x99B, 0xDDF,
	 0xD00, 0xF0C, 0x000, 0x223, 0x041, 0x061, 0x191, 0x3B2,
	 0x420, 0x900, 0xC40, 0xE61, 0xE83, 0xFB4, 0xFD6, 0xFE8,
	 0x113, 0x309, 0x45F, 0x0CF, 0x334, 0x557, 0x889, 0xBBC},
	{0x021, 0xFFF, 0x003, 0x004, 0x006, 0x009, 0x02B, 0x05D,
	 0x08F, 0x2AF, 0x7BF, 0x9CF, 0xADF, 0xDEF, 0x400, 0x600,
	 0x800, 0xB21, 0xE62, 0xE83, 0xFA4, 0xFC8, 0xF00, 0xF6A,
	 0xF9C, 0xF99, 0x072, 0x0A2, 0x0E4, 0x0F6, 0x777, 0x888},
	{0x000, 0x88A, 0xFF7, 0xFFF, 0x001, 0x004, 0x036, 0x306,
	 0xF00, 0xFF0, 0xFBF, 0xF0B, 0x031, 0x051, 0x181, 0x5F3,
	 0x310, 0x600, 0x200, 0x210, 0x421, 0x622, 0x652, 0xFF7,
	 0x004, 0x024, 0x306, 0x0BE, 0x003, 0x226, 0x447, 0x558}};

const uint16 kAmigaEffect45Sets[3][5] = {
	{0x8EF, 0x0AF, 0x03F, 0x008, 0x003},
	{0xFF9, 0xFB0, 0xF30, 0x800, 0x300},
	{0xFF7, 0x9F0, 0x0F0, 0x080, 0x030}};

const byte kAmigaMode43Order[16] = {
	0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};

} // End of anonymous namespace

bool getAmigaSampleCommand(byte command, AmigaSampleCommand &out) {
	out = AmigaSampleCommand();
	switch (command) {
	case 82:
		out.slot = 0;
		out.mixerRepeatCount = 2;
		out.preDelay = 50;
		out.postDelay = 100;
		out.stopAllFirst = true;
		return true;
	case 83:
		out.slot = 1;
		out.mixerRepeatCount = -1;
		return true;
	case 85:
		out.slot = 16;
		out.mixerRepeatCount = 2;
		return true;
	case 86:
		out.slot = 2;
		out.mixerRepeatCount = -1;
		return true;
	case 89:
		out.slot = 3;
		out.mixerRepeatCount = 2;
		out.postDelay = 100;
		return true;
	case 90:
		out.slot = 4;
		out.mixerRepeatCount = -1;
		return true;
	case 91:
		out.slot = 5;
		out.mixerRepeatCount = -1;
		return true;
	case 92:
		out.slot = 6;
		out.mixerRepeatCount = 2;
		out.postDelay = 100;
		return true;
	case 94:
		out.slot = 7;
		out.mixerRepeatCount = 2;
		out.postDelay = 100;
		return true;
	default:
		return false;
	}
}

uint16 amigaInitialModuleSelector(uint16 scriptSelector,
								 bool forceLoadHot) {
	return forceLoadHot ? 1 : scriptSelector;
}

uint16 amigaNativeRand(uint32 &state) {
	state = state * 0x41C64E6DU + 0x3039U;
	return (uint16)((state >> 16) & 0x7FFFU);
}

uint16 amigaRandomBounded(uint32 &state, uint16 upperExclusive) {
	const uint16 value = amigaNativeRand(state);
	return upperExclusive
			   ? (uint16)((value & 0x0FFFU) % upperExclusive)
			   : 0;
}

uint16 amigaNativeDelayMilliseconds(uint16 units) {
	return (uint16)(units / 30U) * 20U;
}

void getAmigaSchedulerTiming(byte period, uint16 &reload,
							 uint16 &initialCountdown) {
	reload = period == 200 ? 60 : 25;
	// Amiga timer quanta represent 50 ms; include the zero-test quantum.
	if (period == 50 || period == 100 || period == 200)
		reload = (period == 50 ? 1500 : 2000) / 50 - 1;
	initialCountdown = 0;
}

bool decodeAmigaPackedPixels(const Common::Array<byte> &payload,
							 uint16 width, uint16 height, Common::Array<byte> &pixels) {
	pixels.clear();
	if (width == 0 || height == 0)
		return false;
	const uint32 expected = (uint32)width * height;
	if (expected / width != height)
		return false;
	pixels.resize(expected);

	uint32 pixel = 0;
	uint32 position = 0;
	while (pixel < expected) {
		if (position >= payload.size()) {
			pixels.clear();
			return false;
		}
		const byte value = payload[position++];
		const byte color = value >> 3;
		uint32 run = 0;
		if (value & 0x04) {
			run = (value & 0x03) + 1;
		} else {
			if (position >= payload.size()) {
				pixels.clear();
				return false;
			}
			run = ((((uint32)value & 0x03) << 8) |
				   payload[position++]) +
				  1;
		}
		const uint32 remaining = expected - pixel;
		const uint32 count = run < remaining ? run : remaining;
		for (uint32 i = 0; i < count; ++i)
			pixels[pixel++] = color;
	}
	return true;
}

static byte amigaModulePaletteNumber(const Common::String &moduleName) {
	byte paletteNumber = 0;
	if (moduleName.equalsIgnoreCase("INIT") ||
		moduleName.equalsIgnoreCase("INIT.TOA")) {
		paletteNumber = 5;
	} else if (moduleName.equalsIgnoreCase("LOADCHUT") ||
			   moduleName.equalsIgnoreCase("LOADCHUT.TOA")) {
		paletteNumber = 3;
	} else if (moduleName.equalsIgnoreCase("LOADPLA") ||
			   moduleName.equalsIgnoreCase("LOADPLA.TOA") ||
			   moduleName.equalsIgnoreCase("LOADAERO") ||
			   moduleName.equalsIgnoreCase("LOADAERO.TOA") ||
			   moduleName.equalsIgnoreCase("LOADGAR") ||
			   moduleName.equalsIgnoreCase("LOADGAR.TOA")) {
		paletteNumber = 2;
	} else if (moduleName.equalsIgnoreCase("LOADHOT") ||
			   moduleName.equalsIgnoreCase("LOADHOT.TOA") ||
			   moduleName.equalsIgnoreCase("LOADCAS") ||
			   moduleName.equalsIgnoreCase("LOADCAS.TOA") ||
			   moduleName.equalsIgnoreCase("LOADFAV") ||
			   moduleName.equalsIgnoreCase("LOADFAV.TOA")) {
		paletteNumber = 1;
	}
	return paletteNumber;
}

bool getAmigaModulePalette(const Common::String &moduleName,
						   uint16 words[32]) {
	return getAmigaNumberedPalette(amigaModulePaletteNumber(moduleName), words);
}

bool getAmigaTextColorTable(const Common::String &moduleName, byte colors[16]) {
	// The Amiga loader selects logical text colors with each module. These
	// remain selected when a script temporarily changes the visible palette.
	static const byte tables[4][16] = {
		{0, 1, 25, 26, 26, 12, 13, 31, 1, 2, 0, 9, 17, 21, 22, 23},
		{0, 10, 25, 26, 26, 12, 13, 31, 1, 2, 0, 9, 17, 21, 22, 23},
		{0, 5, 25, 26, 26, 12, 13, 31, 1, 2, 0, 31, 17, 21, 22, 23},
		{0, 30, 25, 29, 31, 12, 13, 14, 2, 8, 0, 11, 26, 2, 22, 3}
	};
	const byte palette = amigaModulePaletteNumber(moduleName);
	if (palette == 0 || palette == 4)
		return false;
	memcpy(colors, tables[palette == 5 ? 3 : palette - 1], 16);
	return true;
}

bool getAmigaNumberedPalette(byte paletteNumber, uint16 words[32]) {
	if (!words || paletteNumber < 1 || paletteNumber > 5)
		return false;
	copyWords(kAmigaPalettes[paletteNumber - 1], 32, words);
	return true;
}

bool getAmigaEffect45(byte variant, uint16 words[5]) {
	if (!words || variant >= ARRAYSIZE(kAmigaEffect45Sets))
		return false;
	copyWords(kAmigaEffect45Sets[variant], 5, words);
	return true;
}

byte amigaPaletteComponent(uint16 word, byte shift) {
	const byte nibble = (byte)((word >> shift) & 0x0F);
	return (byte)((nibble << 2) | (nibble >> 2));
}

uint16 dimAmigaPaletteWord(uint16 word, byte steps) {
	byte red = (word >> 8) & 0x0F;
	byte green = (word >> 4) & 0x0F;
	byte blue = word & 0x0F;
	while (steps--) {
		if (red)
			--red;
		if (green)
			--green;
		if (blue)
			--blue;
	}
	return (uint16)((red << 8) | (green << 4) | blue);
}

byte prepareAmigaWaitColorCounter(byte counter) {
	counter %= 6;
	return counter < 4 ? 4 : counter;
}

byte amigaMode43PixelOffset(byte phase, uint32 wordOrdinal) {
	return kAmigaMode43Order[(phase + wordOrdinal) & 15];
}

byte amigaMode44WaitCount(byte variant) {
	return variant == 0 ? 9 : (variant < 5 ? 18 : 0);
}

bool getAmigaMode44Rect(byte variant, byte stage, AmigaMode44Rect &out) {
	const byte maxStage = amigaMode44WaitCount(variant);
	if (variant >= 5 || stage > maxStage)
		return false;
	if (variant == 0) {
		out.x = 144 - (int16)stage * 16;
		out.y = 90 - (int16)stage * 10;
		out.width = (uint16)(32 + stage * 32);
		out.height = (uint16)(20 + stage * 20);
	} else {
		out.width = (uint16)(32 + stage * 16);
		out.height = (uint16)(20 + stage * 10);
		out.x = variant == 1 || variant == 2
					? 288 - (int16)stage * 16
					: 0;
		out.y = variant == 2 || variant == 4
					? 180 - (int16)stage * 10
					: 0;
	}
	return true;
}

bool amigaSnapshotGenerationUsable(
	uint32 moduleGeneration, uint32 presentedGeneration, bool hasSnapshot) {
	if (!hasSnapshot || moduleGeneration == 0 || presentedGeneration == 0)
		return false;
	return presentedGeneration == moduleGeneration ||
		(moduleGeneration > 1 &&
		 presentedGeneration == moduleGeneration - 1);
}

} // namespace Emmanuelle
} // namespace Gob
