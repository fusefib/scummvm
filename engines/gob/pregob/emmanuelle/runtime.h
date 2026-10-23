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

#ifndef GOB_PREGOB_EMMANUELLE_RUNTIME_H
#define GOB_PREGOB_EMMANUELLE_RUNTIME_H

#include "common/scummsys.h"
#include "gob/pregob/emmanuelle/platform.h"

namespace Gob {
namespace Emmanuelle {

enum OldTotAudioKind {
	kOldTotAudioNone = 0,
	kOldTotAudioDosSpeaker,
	kOldTotAudioAtariSample,
	kOldTotAudioAmigaSample
};

struct ProtectionPrompt {
	byte letter;
	uint16 box;
	byte color;
};
struct ProtectionChallenge {
	ProtectionPrompt first, second;
};

/** One-entry input latch used while native waits continue pumping events. */
class InputLatch {
public:
	InputLatch();

	void noteKey(int16 key);
	void noteLatestKey(int16 key);
	void noteMouse(int16 x, int16 y, byte buttons);
	void clear();
	// Inspect pending input without consuming it.
	int16 peek(int16 &x, int16 &y, byte &buttons) const;
	int16 take(int16 &x, int16 &y, byte &buttons);

private:
	int16 _key;
	int16 _mouseX;
	int16 _mouseY;
	byte _mouseButtons;
};

enum {
	kHostFrameRate = 50,
	kInitialTomahawkMinimumMilliseconds = 2500
};

struct ColorCycleResult {
	ColorCycleResult() : counter(0), rotatePalette(false) {}

	byte counter;
	bool rotatePalette;
};

struct InputBoundary {
	InputBoundary() : nativeDelayUnits(0), discardStale(false),
							showCursor(false) {}

	uint16 nativeDelayUnits;
	bool discardStale;
	bool showCursor;
};

enum PresentationOperation {
	kPresentationNone = 0,
	kPresentationFrame,
	kPresentationFadeFromBlack,
	kPresentationMode43,
	kPresentationMode44
};

struct PresentationStep {
	PresentationStep() : operation(kPresentationNone),
		variant(0), phase(0), complete(false) {}

	PresentationOperation operation;
	byte variant;
	byte phase;
	bool complete;
};

class PresentationState {
public:
	explicit PresentationState(OldTotPlatformTag platform);
	void resetModule();
	void arm(byte mode);
	void disarm() { arm(0); }
	void noteChange() { _pending = true; }
	void notePaletteTransition() { _paletteTransitionPending = true; }
	void commitDirectFrame();
	bool nextStep(bool black, byte randomVariant, PresentationStep &step);

	byte mode() const { return _mode; }
	byte variant() const { return _variant; }
	byte phase() const { return _phase; }
	bool pending() const { return _pending; }
	bool active() const { return _active; }
	bool paletteTransitionPending() const { return _paletteTransitionPending; }

private:
	OldTotPlatformTag _platform;
	byte _mode;
	byte _variant;
	byte _phase;
	bool _pending;
	bool _active;
	bool _paletteTransitionPending;
};

/** Intro skip state, cleared at INIT's first input choice. */
class IntroSkipState {
public:
	IntroSkipState();

	void begin();
	bool request();
	bool activate(bool quitRequested);
	void finish();

	bool available() const { return _state != kUnavailable; }
	bool fastForwarding() const { return _state == kSkipping; }

private:
	enum State { kUnavailable, kPlaying, kRequested, kSkipping };
	State _state;
};

bool isAudioPlaying(OldTotPlatformTag platform, bool speakerActive,
	bool samplePlaying);
bool isAudioReadyForSave(OldTotPlatformTag platform, bool speakerActive,
	bool samplePlaying, bool blocking);
/** Native timer quanta per 50 Hz input pass; Amiga retains a remainder. */
uint16 accumulateTimerQuanta(OldTotPlatformTag platform,
									 uint32 &accumulator);
/** Return only the time still needed to satisfy a minimum duration. */
uint32 minimumDurationRemaining(uint32 elapsed,
									  uint32 minimumDuration);
/** Whether the first complete title frame has reached a visible state. */
bool isTitlePresentationVisible(OldTotPlatformTag platform,
									   bool black, bool framePending,
									   bool presentationActive,
									   bool paletteTransitionPending);
ColorCycleResult advanceColorCycle(byte counter, bool enabled,
												uint16 selector);
uint16 opcode03DelayUnits(byte arg);
/** Delay before a fresh opcode-1B wait discards stale input. */
uint16 inputEntryDelay(OldTotPlatformTag platform);
InputBoundary choiceInputBoundary(OldTotPlatformTag platform,
										bool initialEntry, bool reentering);
/** Keyboard cursor motion performed by the native input service. */
byte keyboardCursorStep(OldTotPlatformTag platform);
/** Lowest cursor origin accepted by the native input service. */
int16 minimumCursorOrigin(OldTotPlatformTag platform);
bool recentersMouseOnModuleLoad(OldTotPlatformTag platform);
/** Native callback countdown ordering. */
bool schedulerChecksZeroBeforeDecrement(OldTotPlatformTag platform);
/** Host command 201 enables intermediate sequence suppression in selector 3. */
bool command201SkipsIntermediateNodes(OldTotPlatformTag platform,
									 uint16 moduleSelector);

bool decodeProtectionPrompt(const byte raw[4], ProtectionPrompt &out);
bool decodeProtectionRecord(const byte raw[8], ProtectionChallenge &out);
void installProtectionChallenge(const ProtectionChallenge &c, uint16 vars[7]);
bool buildProtectionHint(bool initModule, bool suspended, byte opcode,
						 byte regionCount, const uint16 vars[7],
						 ProtectionChallenge &out);
const char *protectionColorName(byte color);

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_RUNTIME_H
