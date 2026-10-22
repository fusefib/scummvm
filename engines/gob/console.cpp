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
#include "gob/gob.h"
#include "gob/game.h"
#include "gob/inter.h"
#include "gob/dataio.h"
#include "gob/cheater.h"
#include "gob/pregob/emmanuelle/diagnostics.h"
#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {

namespace {

const char *emmanuellePlatformName(Emmanuelle::OldTotPlatformTag platform) {
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

bool emmanuelleVariableUsed(uint index) {
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

Common::String emmanuelleVariableDescription(uint index, uint16 value,
		const Common::String &module) {
	if (index >= 64 && index <= 67) {
		static const char *const cities[] = {"Rio", "Salvador", "Manaus", "Iguacu"};
		static const char *const states[] = {"not attempted", "failed", "successful"};
		return Common::String::format("%s airport encounter: %s", cities[index - 64],
			value < ARRAYSIZE(states) ? states[value] : "unknown state");
	}
	if (module.equalsIgnoreCase("INIT") && index >= 1 && index <= 6) {
		static const char *const fields[] = {"letter", "box", "color"};
		return Common::String::format("Protection prompt %u %s", (index - 1) / 3 + 1,
			fields[(index - 1) % 3]);
	}
	// Dialogue scratch variables have other roles outside the airport module.
	if (module.equalsIgnoreCase("LOADAERO")) {
		if (index >= 80 && index <= 89)
			return Common::String::format("Airport topic %u weight", index - 79);
		if (index >= 90 && index <= 95) {
			static const char *const names[] = {
				"Selected topic weight", "Dialogue random roll (0-9)",
				"Dialogue score", "Dialogue/travel option",
				"Offered dialogue topic A", "Offered dialogue topic B"
			};
			return names[index - 90];
		}
	}
	if (module.equalsIgnoreCase("LOADCAS")) {
		if (index >= 120 && index <= 122)
			return Common::String::format("Roulette wager %u code", index - 119);
		if (index >= 217 && index <= 219)
			return Common::String::format("Roulette wager %u stake", index - 216);
		if (index == 127)
			return "Roulette result";
	}
	switch (index) {
	case 0:
		return "Module selector";
	case 21:
		return "Bankroll";
	case 44:
		return "Location selector";
	default:
		return Common::String();
	}
}

} // End of anonymous namespace

GobConsole::GobConsole(GobEngine *vm) : GUI::Debugger(), _vm(vm),
		_cheater(nullptr), _emmanuelleDebugger(nullptr),
		_emmanuelleHasPreviousVariables(false) {
	registerCmd("continue",     WRAP_METHOD(GobConsole, cmdExit));
	registerCmd("help",      	WRAP_METHOD(GobConsole, cmd_Help));
	registerCmd("varSize",      WRAP_METHOD(GobConsole, cmd_varSize));
	registerCmd("dumpVars",     WRAP_METHOD(GobConsole, cmd_dumpVars));
	registerCmd("var8",         WRAP_METHOD(GobConsole, cmd_var8));
	registerCmd("var16",        WRAP_METHOD(GobConsole, cmd_var16));
	registerCmd("var32",        WRAP_METHOD(GobConsole, cmd_var32));
	registerCmd("varString",    WRAP_METHOD(GobConsole, cmd_varString));
	registerCmd("cheat",        WRAP_METHOD(GobConsole, cmd_cheat));
	registerCmd("listArchives", WRAP_METHOD(GobConsole, cmd_listArchives));
	registerCmd("gobStack",     WRAP_METHOD(GobConsole, cmd_gobStack));
	registerCmd("emmaState",    WRAP_METHOD(GobConsole, cmd_emmaState));
	registerCmd("emmaVars",     WRAP_METHOD(GobConsole, cmd_emmaVars));
	registerCmd("emmaStack",    WRAP_METHOD(GobConsole, cmd_emmaStack));
	registerCmd("emmaTrace",    WRAP_METHOD(GobConsole, cmd_emmaTrace));
}

GobConsole::~GobConsole() {
}

void GobConsole::registerCheater(Cheater *cheater) {
	_cheater = cheater;
}

void GobConsole::unregisterCheater() {
	_cheater = nullptr;
}

void GobConsole::registerEmmanuelleDebugger(
	Emmanuelle::OldTotDebugProvider *provider) {
	_emmanuelleDebugger = provider;
	_emmanuellePreviousVariables.clear();
	_emmanuelleHasPreviousVariables = false;
}

void GobConsole::unregisterEmmanuelleDebugger() {
	_emmanuelleDebugger = nullptr;
	_emmanuellePreviousVariables.clear();
	_emmanuelleHasPreviousVariables = false;
}

bool GobConsole::hasInterVariables() const {
	return _vm->_inter && _vm->_inter->_variables;
}

bool GobConsole::requireInterVariables() {
	if (hasInterVariables())
		return true;
	debugPrintf("This command requires a standard Gob interpreter.\n");
	return false;
}

bool GobConsole::cmd_Help(int, const char **) {
	debugPrintf("Debug\n");
	debugPrintf("-----\n");
	debugPrintf(" debugflag_list - Lists the available debug flags and their status\n");
	debugPrintf(" debugflag_enable - Enables a debug flag\n");
	debugPrintf(" debugflag_disable - Disables a debug flag\n");
	debugPrintf(" debuglevel - Sets debug level\n");
	debugPrintf("\n");
	debugPrintf("Commands\n");
	debugPrintf("--------\n");
	debugPrintf(" continue - returns back to the game\n");
	debugPrintf(" listArchives - shows which Archives are currently being used\n");
	debugPrintf(" cheat - enables Cheats for Geisha\n");
	debugPrintf("\n");
	debugPrintf("Variables\n");
	debugPrintf("---------\n");
	debugPrintf(" varSize - shows the size of a variable in bytes\n");
	debugPrintf(" dumpVars - dumps the variables to variables.dmp\n");
	debugPrintf(" var8 - manipulates 8-bit variables; usage: var8 <var offset> (<value>)\n");
	debugPrintf(" var16 - manipulates 16-bit variables; usage: var16 <var offset> (<value>)\n");
	debugPrintf(" var32 - manipulates 32-bit variables; usage: var32 <var offset> (<value>)\n");
	debugPrintf(" varString - manipulates string references; usage: varString <var offset> (<value>)\n");
	debugPrintf("\n");
	debugPrintf("Scripts\n");
	debugPrintf("------\n");
	debugPrintf(" gobStack - prints the TOT scripts call stack (requires Gameflow debug flag)\n");
	debugPrintf("\n");
	if (_emmanuelleDebugger) {
		debugPrintf("Emmanuelle\n");
		debugPrintf("----------\n");
		debugPrintf(" emmaState - prints old-TOT runtime state\n");
		debugPrintf(" emmaVars - prints used/nonzero variables; usage: emmaVars (changed | start (count))\n");
		debugPrintf(" emmaStack - prints the explicit old-TOT executor stack\n");
		debugPrintf(" emmaTrace - controls parseable tracing; usage: emmaTrace (on | off)\n");
		debugPrintf(" cpsolve - prints the active copy-protection answers\n");
		debugPrintf("\n");
	}
	return true;
}

bool GobConsole::cmd_varSize(int argc, const char **argv) {
	if (!requireInterVariables())
		return true;
	debugPrintf("Size of the variable space: %d bytes\n", _vm->_inter->_variables->getSize());
	return true;
}

bool GobConsole::cmd_dumpVars(int argc, const char **argv) {
	if (!requireInterVariables())
		return true;

	Common::DumpFile file;

	const char *outFile = "variables.dmp";
	if (!file.open(outFile))
		return true;

	file.write(_vm->_inter->_variables->getAddressOff8(0), _vm->_inter->_variables->getSize());

	file.flush();
	file.close();

	debugPrintf("Dumped %s successfully to ScummVM directory\n", outFile);
	return true;
}

bool GobConsole::cmd_var8(int argc, const char **argv) {
	if (!requireInterVariables())
		return true;
	if (argc == 1) {
		debugPrintf("Usage: var8 <var offset> (<value>)\n");
		return true;
	}

	uint32 varNum = atoi(argv[1]);

	if (varNum >= _vm->_inter->_variables->getSize()) {
		debugPrintf("Variable offset out of range\n");
		return true;
	}

	if (argc > 2) {
		uint32 varVal = atoi(argv[2]);
		_vm->_inter->_variables->writeOff8(varNum, varVal);
	}

	debugPrintf("var8_%d = %d\n", varNum, _vm->_inter->_variables->readOff8(varNum));

	return true;
}

bool GobConsole::cmd_var16(int argc, const char **argv) {
	if (!requireInterVariables())
		return true;
	if (argc == 1) {
		debugPrintf("Usage: var16 <var offset> (<value>)\n");
		return true;
	}

	uint32 varNum = atoi(argv[1]);

	if ((varNum + 1) >= _vm->_inter->_variables->getSize()) {
		debugPrintf("Variable offset out of range\n");
		return true;
	}

	if (argc > 2) {
		uint32 varVal = atoi(argv[2]);
		_vm->_inter->_variables->writeOff16(varNum, varVal);
	}

	debugPrintf("var16_%d = %d\n", varNum, _vm->_inter->_variables->readOff16(varNum));

	return true;
}

bool GobConsole::cmd_var32(int argc, const char **argv) {
	if (!requireInterVariables())
		return true;
	if (argc == 1) {
		debugPrintf("Usage: var32 <var offset> (<value>)\n");
		return true;
	}

	uint32 varNum = atoi(argv[1]);

	if ((varNum + 3) >= _vm->_inter->_variables->getSize()) {
		debugPrintf("Variable offset out of range\n");
		return true;
	}

	if (argc > 2) {
		uint32 varVal = atoi(argv[2]);
		_vm->_inter->_variables->writeOff32(varNum, varVal);
	}

	debugPrintf("var32_%d = %d\n", varNum, _vm->_inter->_variables->readOff32(varNum));

	return true;
}

bool GobConsole::cmd_varString(int argc, const char **argv) {
	if (!requireInterVariables())
		return true;
	if (argc == 1) {
		debugPrintf("Usage: varString <var offset> (<value>)\n");
		return true;
	}

	uint32 varNum = atoi(argv[1]);

	if (varNum >= _vm->_inter->_variables->getSize()) {
		debugPrintf("Variable offset out of range\n");
		return true;
	}

	if (argc > 2) {
		uint32 maxLength = _vm->_inter->_variables->getSize() - varNum;

		Common::strlcpy(_vm->_inter->_variables->getAddressOffString(varNum), argv[2], maxLength);
	}

	debugPrintf("varString_%d = \"%s\"\n", varNum, _vm->_inter->_variables->getAddressOffString(varNum));

	return true;
}

bool GobConsole::cmd_cheat(int argc, const char **argv) {
	if (_cheater)
		return _cheater->cheat(*this);

	return true;
}

bool GobConsole::cmd_listArchives(int argc, const char **argv) {
	Common::Array<ArchiveInfo> info;

	_vm->_dataIO->getArchiveInfo(info);

	debugPrintf("   Archive    | Base | FileCount\n");
	debugPrintf("--------------------------------\n");
	for (Common::Array<ArchiveInfo>::const_iterator it = info.begin(); it != info.end(); ++it)
		if (!it->name.empty())
		debugPrintf("%13s |   %d  | %d\n", it->name.c_str(), it->base, it->fileCount);

	return true;
}

bool GobConsole::cmd_gobStack(int argc, const char **argv) {
	if (!_vm->_inter) {
		debugPrintf("This command requires a standard Gob interpreter.\n");
		return true;
	}
	debugPrintf("%s", _vm->_game->getGobStack().c_str());
	return true;
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
	debugPrintf("native bank=%u sounds=%u audio=%u kind=%u slot=%d "
				 "frequency=%u repeat=%d blocking=%u init=%u "
				 "skipIntermediateSequenceNodes=%u presentation=%u/%u/%u "
				 "framePending=%u presentationActive=%u "
				 "paletteTransition=%u black=%u "
				 "active=%u saved=%u "
				 "cycle=%u/%u trace=%u\n",
				 (uint)state.native.sampleBank, (uint)state.native.sampleCount,
				 state.native.audioActive ? 1U : 0U,
				 (uint)state.native.audioKind, (int)state.native.audioSlot,
				 (uint)state.native.audioFrequency,
				 (int)state.native.audioRepeatCount,
				 state.native.audioBlocking ? 1U : 0U,
				 state.native.initialInitComplete ? 1U : 0U,
				 state.native.skipIntermediateSequenceNodes ? 1U : 0U,
				 (uint)state.native.presentationMode,
				 (uint)state.native.presentationVariant,
				 (uint)state.native.presentationPhase,
				 state.native.presentationDirty ? 1U : 0U,
				 state.native.presentationActive ? 1U : 0U,
				 state.native.paletteTransition ? 1U : 0U,
				 state.native.paletteBlack ? 1U : 0U,
				 state.native.activeObjectCount,
				 state.native.savedRegionDepth,
				 state.native.colorCycleEnabled ? 1U : 0U,
				 (uint)state.native.colorCounter, state.traceEnabled ? 1U : 0U);
	debugPrintf("amiga moduleGeneration=%u snapshot=%u "
				 "snapshotGeneration=%u\n",
				 (uint)state.native.moduleGeneration,
				 state.native.presentedSnapshot ? 1U : 0U,
				 (uint)state.native.presentedGeneration);
	return true;
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
				_emmanuelleHasPreviousVariables &&
				 i < _emmanuellePreviousVariables.size()
					? _emmanuellePreviousVariables[i]
					: 0;
			if (previous != state.variables[i]) {
				const Common::String description = emmanuelleVariableDescription(
					i, state.variables[i], state.module);
				debugPrintf("v[%u]: %u -> %u%s%s\n", i, (uint)previous,
					(uint)state.variables[i], description.empty() ? "" : "  // ",
					description.c_str());
				++changes;
			}
		}
		debugPrintf("%u changed variables\n", changes);
		_emmanuellePreviousVariables = state.variables;
		_emmanuelleHasPreviousVariables = true;
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
		const Common::String description = emmanuelleVariableDescription(
			i, state.variables[i], state.module);
		debugPrintf("v[%u]=%u%s%s\n", i, (uint)state.variables[i],
			description.empty() ? "" : "  // ", description.c_str());
	}
	_emmanuellePreviousVariables = state.variables;
	_emmanuelleHasPreviousVariables = true;
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

} // End of namespace Gob
