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

#ifndef GOB_PREGOB_EMMANUELLE_DEBUGGER_H
#define GOB_PREGOB_EMMANUELLE_DEBUGGER_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "gob/pregob/emmanuelle/executor.h"
#include "gob/pregob/emmanuelle/interpreter.h"
#include "gob/pregob/emmanuelle/platform.h"
#include "gob/pregob/emmanuelle/runtime.h"
#include "gob/pregob/emmanuelle/saveload.h"

namespace Gob {
namespace Emmanuelle {

struct ProtectionChallenge;

bool buildProtectionHint(bool initModule, bool suspended, byte opcode,
						 byte regionCount, const uint16 vars[7],
						 ProtectionChallenge &out);
const char *protectionColorName(byte color);

struct OldTotInputDiagnosticState {
	OldTotInputDiagnosticState() {
		key = 0;
		mouseX = 0;
		mouseY = 0;
		mouseButtons = 0;
	}

	int16 key;
	int16 mouseX;
	int16 mouseY;
	byte mouseButtons;
};

struct OldTotNativeDiagnosticState {
	OldTotNativeDiagnosticState() {
		sampleBank = 0;
		sampleCount = 0;

		presentationMode = 0;
		presentationVariant = 0;
		presentationPhase = 0;
		colorCounter = 0;

		audioKind = kOldTotAudioNone;
		audioSlot = -1;
		audioFrequency = 0;
		audioRepeatCount = 0;
		audioActive = false;
		audioBlocking = false;

		activeObjectCount = 0;
		savedRegionDepth = 0;

		moduleGeneration = 0;
		presentedGeneration = 0;
		presentedSnapshot = false;

		initialInitComplete = false;
		skipIntermediateSequenceNodes = false;
		colorCycleEnabled = false;

		presentationDirty = false;
		presentationActive = false;
		paletteTransition = false;
		paletteBlack = false;
	}

	byte sampleBank;
	byte sampleCount;
	byte presentationMode;
	byte presentationVariant;
	byte presentationPhase;
	byte colorCounter;
	OldTotAudioKind audioKind;
	int16 audioSlot;
	uint16 audioFrequency;
	int16 audioRepeatCount;
	uint activeObjectCount;
	uint savedRegionDepth;
	uint32 moduleGeneration;
	uint32 presentedGeneration;
	bool audioActive;
	bool audioBlocking;
	bool initialInitComplete;
	bool skipIntermediateSequenceNodes;
	bool colorCycleEnabled;
	bool presentationDirty;
	bool presentationActive;
	bool paletteTransition;
	bool paletteBlack;
	bool presentedSnapshot;
};

struct OldTotDiagnosticSnapshot {
	OldTotDiagnosticSnapshot() {
		platform = kOldTotPlatformUnknown;
		selector = 0;
		postPolicySelector = 0;
		schedulerAccumulator = 0;
		schedulerDebt = 0;

		hasVM = false;
		hasExecutor = false;
		cursorVisible = false;
		traceEnabled = false;
	}

	OldTotPlatformTag platform;
	Common::String module;
	Common::String nextModule;
	uint16 selector;
	uint16 postPolicySelector;
	uint32 schedulerAccumulator;
	uint32 schedulerDebt;
	Common::Array<uint16> variables;
	OldTotVMTransientState vm;
	OldTotExecutorState executor;
	OldTotInputDiagnosticState input;
	OldTotNativeDiagnosticState native;
	EmmanuelleSaveBoundaryState saveBoundary;
	bool hasVM;
	bool hasExecutor;
	bool cursorVisible;
	bool traceEnabled;
};

class OldTotDebugProvider {
public:
	virtual ~OldTotDebugProvider() {}

	virtual void exportDiagnosticSnapshot(OldTotDiagnosticSnapshot &out) const = 0;
	virtual bool queryProtectionHint(ProtectionChallenge &out) const = 0;
	virtual void setDiagnosticTraceEnabled(bool enabled) = 0;
	virtual bool diagnosticTraceEnabled() const = 0;
	virtual bool setDiagnosticVariable(uint16 index, uint16 value, uint16 &previousValue) = 0;
};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_DEBUGGER_H
