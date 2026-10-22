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

#include "gob/console.h"
#include "gob/pregob/emmanuelle/debugger.h"
#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {

void GobConsole::registerEmmanuelleDebugger(
	Emmanuelle::OldTotDebugProvider *provider) {
	_emmanuelleDebugger = provider;
	_emmanuellePreviousVariables.clear();
}

void GobConsole::unregisterEmmanuelleDebugger() {
	_emmanuelleDebugger = nullptr;
	_emmanuellePreviousVariables.clear();
}

void GobConsole::printEmmanuelleHelp() {
	debugPrintf("Emmanuelle\n");
	debugPrintf("----------\n");
	debugPrintf(" emmaState - prints old-TOT runtime state\n");
	debugPrintf(" emmaVars - prints variables; usage: emmaVars (changed | start (count))\n");
	debugPrintf(" emmaStack - prints the explicit old-TOT executor stack\n");
	debugPrintf(" emmaTrace - controls parseable tracing; usage: emmaTrace (on | off)\n");
	debugPrintf(" cpsolve - prints the active copy-protection answers\n");
	debugPrintf("\n");
}

static const char *emmanuellePlatformName(Emmanuelle::OldTotPlatformTag platform) {
	switch (platform) {
	case Emmanuelle::kOldTotPlatformDOS:
		return "DOS";
	case Emmanuelle::kOldTotPlatformAtariST:
		return "Atari ST";
	case Emmanuelle::kOldTotPlatformAmiga:
		return "Amiga";
	default:
		return "unknown";
	}
}

bool GobConsole::cmd_emmaState(int argc, const char **argv) {
	if (!_emmanuelleDebugger) {
		debugPrintf("No Emmanuelle runtime is active.\n");
		return true;
	}
	Emmanuelle::OldTotDiagnosticSnapshot state;
	_emmanuelleDebugger->exportDiagnosticSnapshot(state);
	debugPrintf("platform=%s module=%s selector=%u postSelector=%u next=%s\n",
				 emmanuellePlatformName(state.platform), state.module.c_str(),
				 (uint)state.selector, (uint)state.postPolicySelector,
				 state.nextModule.c_str());
	debugPrintf("vm=%u executor=%u steps=%u suspended=%u node=0x%04X depth=%u\n",
				 state.hasVM ? 1U : 0U, state.hasExecutor ? 1U : 0U,
				 (uint)state.executor.steps, state.executor.suspended ? 1U : 0U,
				 state.executor.suspendedNode, (uint)state.executor.frames.size());
	debugPrintf("scheduler remainder=%u debt=%u\n",
				 (uint)state.schedulerAccumulator, (uint)state.schedulerDebt);
	debugPrintf("scheduler targets=%u index=%u reload=%u countdown=%u savedRegions=%u\n",
				 (uint)state.vm.schedulerTargets.size(), state.vm.schedulerIndex,
				 (uint)state.vm.schedulerReload, (uint)state.vm.schedulerCountdown,
				 state.vm.savedRegionDepth);
	debugPrintf("input pending key=%d mouse=(%d,%d) buttons=%u cursor=%u\n",
				 (int)state.input.key,
				 (int)state.input.mouseX, (int)state.input.mouseY,
				 (uint)state.input.mouseButtons, state.cursorVisible ? 1U : 0U);
	debugPrintf("save activeInput=%u activeNode=0x%04X suspendedWait=%u "
				 "normalizesExecutor=%u runtime=%u stableChoice=%u inInit=%u "
				 "palette=%u audio=%u regions=%u fight=%u loadSafe=%u "
				 "saveSafe=%u blocker=%s\n",
				 state.saveBoundary.activeInput ? 1U : 0U,
				 state.saveBoundary.activeInputNode,
				 state.saveBoundary.suspendedWait ? 1U : 0U,
				 state.saveBoundary.normalizesExecutor() ? 1U : 0U,
				 state.saveBoundary.runtimeReady ? 1U : 0U,
				 state.saveBoundary.stableChoice() ? 1U : 0U,
				 state.saveBoundary.inInit ? 1U : 0U,
				 state.saveBoundary.paletteReady ? 1U : 0U,
				 state.saveBoundary.audioIdle ? 1U : 0U,
				 state.saveBoundary.regionsCoherent ? 1U : 0U,
				 state.saveBoundary.fightIdle ? 1U : 0U,
				 state.saveBoundary.canLoad() ? 1U : 0U,
				 state.saveBoundary.canSave() ? 1U : 0U,
				 Emmanuelle::emmanuelleSaveBoundaryBlockerName(
					 state.saveBoundary.blocker()));
	debugPrintf("native audio=%u kind=%u slot=%d frequency=%u repeat=%d blocking=%u active=%u saved=%u trace=%u\n",
				state.native.audioActive ? 1U : 0U,
				(uint)state.native.audioKind,
				(int)state.native.audioSlot,
				(uint)state.native.audioFrequency,
				(int)state.native.audioRepeatCount,
				state.native.audioBlocking ? 1U : 0U,
				state.native.activeObjectCount,
				state.native.savedRegionDepth,
				state.traceEnabled ? 1U : 0U);
	return true;
}

static bool emmanuelleVariableUsed(uint index) {
	// These ranges cover the seven known DOS, ST and Amiga script sets.
	static const byte ranges[][2] = {
		{0, 6}, {10, 33}, {35, 73}, {75, 77}, {80, 99},
		{120, 124}, {126, 127}, {129, 130}, {210, 215}, {217, 219}
	};
	for (uint i = 0; i < ARRAYSIZE(ranges); ++i) {
		if (index >= ranges[i][0] && index <= ranges[i][1])
			return true;
	}
	return false;
}

bool GobConsole::cmd_emmaVars(int argc, const char **argv) {
	if (!_emmanuelleDebugger) {
		debugPrintf("No Emmanuelle runtime is active.\n");
		return true;
	}
	Emmanuelle::OldTotDiagnosticSnapshot state;
	_emmanuelleDebugger->exportDiagnosticSnapshot(state);
	if (argc > 1 && Common::String(argv[1]).equalsIgnoreCase("changed")) {
		uint changes = 0;
		for (uint i = 0; i < state.variables.size(); ++i) {
			const uint16 previous =
				i < _emmanuellePreviousVariables.size()
					? _emmanuellePreviousVariables[i]
					: 0;
			if (previous != state.variables[i]) {
				debugPrintf("v[%u]: %u -> %u\n", i, (uint)previous,
					(uint)state.variables[i]);
				++changes;
			}
		}
		debugPrintf("%u changed variables\n", changes);
		_emmanuellePreviousVariables = state.variables;
		return true;
	}

	int start = argc > 1 ? atoi(argv[1]) : 0;
	int count = argc > 2 ? atoi(argv[2]) :
		(argc > 1 ? 64 : (int)state.variables.size());
	if (start < 0 || count < 1 || (uint)start >= state.variables.size()) {
		debugPrintf("Usage: emmaVars (changed | start (count))\n");
		return true;
	}
	const uint end = MIN<uint>((uint)start + (uint)count,
							 state.variables.size());
	if (argc == 1)
		debugPrintf("Showing known used slots and nonzero values; use emmaVars 0 %u for all slots.\n",
			(uint)state.variables.size());
	for (uint i = (uint)start; i < end; ++i) {
		// Keep unexpected nonzero state visible even outside the known ranges.
		if (argc == 1 && !emmanuelleVariableUsed(i) && state.variables[i] == 0)
			continue;
		debugPrintf("v[%u]=%u\n", i, (uint)state.variables[i]);
	}
	_emmanuellePreviousVariables = state.variables;
	return true;
}

bool GobConsole::cmd_emmaStack(int argc, const char **argv) {
	if (!_emmanuelleDebugger) {
		debugPrintf("No Emmanuelle runtime is active.\n");
		return true;
	}
	Emmanuelle::OldTotDiagnosticSnapshot state;
	_emmanuelleDebugger->exportDiagnosticSnapshot(state);
	debugPrintf("steps=%u suspended=%u node=0x%04X frames=%u\n",
				 (uint)state.executor.steps,
				 state.executor.suspended ? 1U : 0U,
				 state.executor.suspendedNode,
				 (uint)state.executor.frames.size());
	for (uint i = 0; i < state.executor.frames.size(); ++i) {
		const Emmanuelle::OldTotExecFrame &frame = state.executor.frames[i];
		debugPrintf("#%u kind=%s current=0x%04X cursor=0x%04X index=%u count=%u\n",
					 i, frame.kind == Emmanuelle::OldTotExecFrame::kDispatch
						? "dispatch" : "sequence",
					 frame.current, frame.cursor, (uint)frame.index,
					 (uint)frame.count);
	}
	return true;
}

bool GobConsole::cmd_emmaTrace(int argc, const char **argv) {
	if (!_emmanuelleDebugger) {
		debugPrintf("No Emmanuelle runtime is active.\n");
		return true;
	}
	if (argc == 1) {
		debugPrintf("Emmanuelle trace is %s.\n",
					 _emmanuelleDebugger->diagnosticTraceEnabled()
						? "enabled" : "disabled");
		debugPrintf("Usage: emmaTrace on | off. Events go to the log through enabled debug channels.\n");
		debugPrintf("For example: debuglevel 3, then debugflag_enable Gameflow.\n");
		return true;
	}
	const Common::String value(argv[1]);
	if (value.equalsIgnoreCase("on")) {
		_emmanuelleDebugger->setDiagnosticTraceEnabled(true);
		debugPrintf("Emmanuelle trace enabled.\n");
		return cmd_emmaState(0, nullptr);
	} else if (value.equalsIgnoreCase("off")) {
		_emmanuelleDebugger->setDiagnosticTraceEnabled(false);
		debugPrintf("Emmanuelle trace disabled.\n");
	} else {
		debugPrintf("Usage: emmaTrace (on | off)\n");
	}
	return true;
}

bool GobConsole::cmd_cpSolve(int argc, const char **argv) {
	if (!_emmanuelleDebugger) {
		debugPrintf("No Emmanuelle runtime is active.\n");
		return true;
	}
	if (argc != 1) {
		debugPrintf("Usage: cpsolve\n");
		return true;
	}

	Emmanuelle::ProtectionChallenge challenge;
	if (!_emmanuelleDebugger->queryProtectionHint(challenge)) {
		debugPrintf("No active copy-protection challenge\n");
		return true;
	}

	debugPrintf("First: %c%u -> %s (region %u)\n",
		(char)('A' + challenge.first.letter), (uint)challenge.first.box,
		Emmanuelle::protectionColorName(challenge.first.color),
		(uint)challenge.first.color);
	debugPrintf("Second: %c%u -> %s (region %u)\n",
		(char)('A' + challenge.second.letter), (uint)challenge.second.box,
		Emmanuelle::protectionColorName(challenge.second.color),
		(uint)challenge.second.color);
	return true;
}

} // End of namespace Gob
