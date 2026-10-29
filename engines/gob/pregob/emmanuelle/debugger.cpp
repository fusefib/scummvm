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
#include "gob/sound/sound.h"
#include "gob/pregob/emmanuelle/emmanuelle.h"
#include "graphics/cursorman.h"
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
	debugPrintf(" emmaVars - prints variables; usage: emmaVars (details | changed | start (count))\n");
	debugPrintf(" emmaVarsSet - changes a variable; usage: emmaVarsSet INDEX VALUE\n");
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
				 state.saveBoundary.audioReady ? 1U : 0U,
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

enum EmmanuelleVariableGroup {
	kEmmaGameStatus,
	kEmmaInventory,
	kEmmaEncounters,
	kEmmaScene,
	kEmmaConversation,
	kEmmaHotelReplies,
	kEmmaRoulette,
	kEmmaProtection,
	kEmmaOtherNonzero,
};

static EmmanuelleVariableGroup emmanuelleVariableGroup(uint index) {
	struct VariableRange {
		byte first;
		byte last;
		EmmanuelleVariableGroup group;
	};

	// One fixed partition defines both the known slots and their display order.
	static const VariableRange ranges[] = {
		{0, 0, kEmmaGameStatus},
		{1, 6, kEmmaProtection},
		{10, 19, kEmmaScene},
		{20, 21, kEmmaGameStatus},
		{22, 28, kEmmaInventory},
		{29, 30, kEmmaGameStatus},
		{31, 33, kEmmaInventory},
		{35, 35, kEmmaEncounters},
		{36, 36, kEmmaInventory},
		{37, 37, kEmmaEncounters},
		{38, 38, kEmmaInventory},
		{39, 42, kEmmaScene},
		{43, 43, kEmmaEncounters},
		{44, 44, kEmmaGameStatus},
		{45, 45, kEmmaEncounters},
		{46, 48, kEmmaInventory},
		{49, 49, kEmmaScene},
		{50, 50, kEmmaInventory},
		{51, 56, kEmmaHotelReplies},
		{57, 62, kEmmaEncounters},
		{63, 63, kEmmaGameStatus},
		{64, 70, kEmmaEncounters},
		{71, 72, kEmmaHotelReplies},
		{73, 73, kEmmaInventory},
		{75, 75, kEmmaInventory},
		{76, 76, kEmmaScene},
		{77, 77, kEmmaEncounters},
		{80, 98, kEmmaConversation},
		{99, 99, kEmmaScene},
		{120, 124, kEmmaRoulette},
		{126, 127, kEmmaRoulette},
		{129, 130, kEmmaRoulette},
		{210, 215, kEmmaRoulette},
		{217, 219, kEmmaRoulette},
	};
	for (uint i = 0; i < ARRAYSIZE(ranges); ++i) {
		if (index >= ranges[i].first && index <= ranges[i].last)
			return ranges[i].group;
	}
	return kEmmaOtherNonzero;
}

static bool emmanuelleVariableUsed(uint index) {
	return emmanuelleVariableGroup(index) != kEmmaOtherNonzero;
}

static Common::String emmanuelleVariableDescription(uint index, uint16 value,
		const Common::String &module) {
	struct VariableState {
		uint16 value;
		const char *description;
	};

	static const VariableState heldStates[] = {
		{0, "not held"}, {1, "held"}, {0, nullptr}
	};
	static const VariableState boughtStates[] = {
		{0, "not bought"}, {1, "bought"}, {0, nullptr}
	};
	static const VariableState receivedStates[] = {
		{0, "not received"}, {1, "received"}, {0, nullptr}
	};
	static const VariableState shownStates[] = {
		{0, "not shown"}, {1, "shown"}, {0, nullptr}
	};
	static const VariableState completedStates[] = {
		{0, "not completed"}, {1, "completed"}, {0, nullptr}
	};
	static const VariableState acceptedStates[] = {
		{0, "not accepted"}, {1, "accepted"}, {0, nullptr}
	};
	static const VariableState enabledStates[] = {
		{0, "disabled"}, {1, "enabled"}, {0, nullptr}
	};
	static const VariableState availableStates[] = {
		{0, "unavailable"}, {1, "available"}, {0, nullptr}
	};
	static const VariableState helicopterStates[] = {
		{0, "unavailable"}, {1, "available"}, {2, "available"}, {0, nullptr}
	};
	static const VariableState collectedStates[] = {
		{0, "not collected"}, {1, "already collected"}, {0, nullptr}
	};
	static const VariableState encounterStates[] = {
		{0, "not attempted"}, {1, "unfinished"}, {2, "completed"}, {0, nullptr}
	};
	static const VariableState airportStates[] = {
		{0, "not attempted"}, {1, "failed"}, {2, "successful"}, {0, nullptr}
	};
	// A completed encounter need not earn its law.
	static const VariableState asymmetryStates[] = {
		{0, "not attempted"}, {1, "unfinished"}, {2, "completed without Asymmetry"},
		{3, "Asymmetry achieved"}, {0, nullptr}
	};
	static const VariableState unusualStates[] = {
		{0, "not attempted"}, {1, "unfinished"}, {2, "completed without Unusual"},
		{3, "Unusual achieved"}, {0, nullptr}
	};
	static const VariableState numberStates[] = {
		{0, "not completed"}, {1, "completed without Number"}, {2, "Number achieved"}, {0, nullptr}
	};
	static const VariableState invitationStates[] = {
		{0, "not invited"}, {1, "invited or unfinished"}, {2, "completed"}, {0, nullptr}
	};
	static const VariableState protectorStates[] = {
		{0, "not attempted"}, {1, "protector appeared"}, {2, "completed"}, {0, nullptr}
	};
	static const VariableState aphrodisiacStates[] = {
		{0, "consumed"}, {1, "full"}, {65535, "not held"}, {0, nullptr}
	};
	// Callbacks change the displayed reply before the player accepts it.
	static const VariableState replyStates[] = {
		{0, "unset"}, {1, "refuse payment"}, {2, "agree to pay"}, {3, "payment not requested"},
		{4, "accept intimacy"}, {5, "decline intimacy"}, {0, nullptr}
	};
	static const VariableState referralStates[] = {
		{0, "not pending"}, {1, "pending"}, {0, nullptr}
	};
	static const VariableState sequenceStates[] = {
		{0, "full"}, {2, "short"}, {0, nullptr}
	};

	static const VariableState setupStates[] = {
		{0, "pending"}, {1, "initialized"}, {0, nullptr}
	};
	static const VariableState giftStates[] = {
		{0, "none"}, {1, "cigar"}, {2, "whisky"}, {0, nullptr}
	};

	Common::String description;
	const VariableState *states = nullptr;
	bool contextual = false;
	const bool hasDialogue = module.equalsIgnoreCase("LOADAERO") ||
		module.equalsIgnoreCase("LOADCAS") || module.equalsIgnoreCase("LOADGAR") ||
		module.equalsIgnoreCase("LOADHOT") || module.equalsIgnoreCase("LOADPLA");
	if (index >= 64 && index <= 67) {
		static const char *const cities[] = {"Rio", "Salvador", "Manaus", "Iguacu"};
		description = Common::String::format("%s airport encounter", cities[index - 64]);
		states = airportStates;
	} else if (index >= 1 && index <= 6) {
		static const char *const fields[] = {"letter", "box", "color"};
		description = Common::String::format("Protection prompt %u %s", (index - 1) / 3 + 1,
			fields[(index - 1) % 3]);
	} else if (hasDialogue && index == 12) {
		description = "Approach";
		states = acceptedStates;
		contextual = true;
	} else if (index >= 80 && index <= 89) {
		description = Common::String::format("Dialogue topic %u weight", index - 79);
	} else if (index >= 90 && index <= 98) {
		static const char *const names[] = {
			"Displayed topic weight", "Dialogue random roll (0-9)", "Dialogue score",
			"Dialogue/travel option", "Offered dialogue topic A", "Offered dialogue topic B",
			"Offered dialogue topic C", "Displayed reply", "Payment prompt"
		};
		description = names[index - 90];
		if (index == 97)
			states = replyStates;
		if (index == 98)
			states = enabledStates;
	} else if (index >= 120 && index <= 122) {
		description = Common::String::format("Roulette wager %u code", index - 119);
	} else if (index >= 217 && index <= 219) {
		description = Common::String::format("Roulette wager %u stake", index - 216);
	} else {
		struct VariableDescription {
			uint16 index;
			const char *module;
			const char *description;
			const VariableState *states;
		};
		// LOADHOT stores Emmanuelle's or Mario's room in v15/v16. The selected
		// door matches when the current floor (v17) equals v15 and v10 equals v16.
		static const VariableDescription descriptions[] = {
			{0, nullptr, "Module selector", nullptr},
			{10, "LOADFAV", "Encounter sequence", sequenceStates},
			{11, "LOADHOT", "Helicopter", helicopterStates},
			{13, nullptr, "Encounter character", nullptr},
			{14, "LOADCAS", "Offered gift", giftStates},
			{14, "LOADGAR", "Conversation topic", nullptr},
			{15, "LOADCAS", "Casino man response stage", nullptr},
			{15, "LOADHOT", "Special room floor", nullptr},
			{16, "LOADCAS", "Casino woman response stage", nullptr},
			{16, "LOADHOT", "Special room door", nullptr},
			{17, nullptr, "Current hotel floor", nullptr},
			{18, nullptr, "Hotel tip contact", availableStates},
			{19, "LOADCAS", "Predicted roulette number", nullptr},
			{19, "LOADFAV", "Market warning", shownStates},
			{19, "LOADGAR", "Reply alternation", nullptr},
			{19, "LOADHOT", "Hotel information contact", availableStates},
			{20, nullptr, "Game hour", nullptr},
			{21, nullptr, "Cash (hundreds)", nullptr},
			{22, nullptr, "Pandeiro", heldStates},
			{23, nullptr, "Rio statuette", heldStates},
			{24, nullptr, "Waterfall statuette", heldStates},
			{25, nullptr, "Opera statuette", heldStates},
			{26, nullptr, "Carnival mask", heldStates},
			{27, nullptr, "Toucan", boughtStates},
			{28, nullptr, "Opera glasses", heldStates},
			{29, nullptr, "Energy", nullptr},
			{30, nullptr, "Erotic potential", nullptr},
			{31, nullptr, "Aphrodisiac", aphrodisiacStates},
			{32, nullptr, "Eroticism lesson", receivedStates},
			{33, nullptr, "Waterfall statuette pickup", collectedStates},
			{35, nullptr, "Salvador beach encounter", encounterStates},
			{36, nullptr, "Casino number tip", receivedStates},
			{37, nullptr, "Casino encounter", encounterStates},
			{38, nullptr, "Casino contact referral", referralStates},
			{39, nullptr, "Energy budget or fight result", nullptr},
			{40, nullptr, "Energy gauge remainder", nullptr},
			{41, nullptr, "Encounter animation variant", nullptr},
			{42, nullptr, "Beach contact remark", nullptr},
			{43, nullptr, "Helicopter pilot encounter", completedStates},
			{44, nullptr, "Location selector", nullptr},
			{45, nullptr, "Rio beach encounter", asymmetryStates},
			{46, nullptr, "Waterfall harness", receivedStates},
			{47, nullptr, "Boatman introduction", shownStates},
			{48, nullptr, "Governor introduction", shownStates},
			{49, nullptr, "Favela penalty bypass", enabledStates},
			{50, nullptr, "Fortifying potion", receivedStates},
			{51, nullptr, "Rio hotel information reply", nullptr},
			{52, nullptr, "Salvador hotel information reply", nullptr},
			{53, nullptr, "Manaus hotel information reply", nullptr},
			{54, nullptr, "Iguacu hotel information reply", nullptr},
			{55, nullptr, "Rio hotel gossip reply", nullptr},
			{56, nullptr, "Salvador hotel gossip reply", nullptr},
			{57, nullptr, "Salvador lower-town left encounter", encounterStates},
			{58, nullptr, "Salvador lower-town middle encounter", protectorStates},
			{59, nullptr, "Salvador lower-town right encounter", unusualStates},
			{60, nullptr, "Rio lobby encounter", encounterStates},
			{61, nullptr, "Salvador lobby encounter", encounterStates},
			{62, nullptr, "Garden-party companion state", invitationStates},
			{63, nullptr, "Initial LOADHOT setup", setupStates},
			{68, nullptr, "Garden-party middle encounter", numberStates},
			{69, nullptr, "Iguacu lobby encounter", numberStates},
			{70, nullptr, "Garden-party right encounter", encounterStates},
			{71, nullptr, "Manaus hotel gossip reply", nullptr},
			{72, nullptr, "Iguacu hotel gossip reply", nullptr},
			{73, nullptr, "Manaus hotel introduction", shownStates},
			{75, nullptr, "Aphrodisiac mission", acceptedStates},
			{76, nullptr, "Beach contact return screen", nullptr},
			{77, nullptr, "Encounter result count", nullptr},
			{99, nullptr, "Wrong-room keyhole views", nullptr},
			{123, nullptr, "Roulette chip X", nullptr},
			{124, nullptr, "Roulette chip Y", nullptr},
			{126, nullptr, "Selected chip value", nullptr},
			{127, nullptr, "Roulette result", nullptr},
			{129, nullptr, "Roulette ball X", nullptr},
			{130, nullptr, "Roulette ball Y", nullptr},
			{210, nullptr, "Ten-unit chips", nullptr},
			{211, nullptr, "Chip setup remainder input", nullptr},
			{212, nullptr, "Five-unit chips", nullptr},
			{213, nullptr, "One-unit chips", nullptr},
			{214, nullptr, "Selected roulette wager code", nullptr},
			{215, nullptr, "Next roulette wager slot", nullptr},
		};
		for (uint i = 0; i < ARRAYSIZE(descriptions); ++i) {
			if (index == descriptions[i].index &&
				(!descriptions[i].module || module.equalsIgnoreCase(descriptions[i].module))) {
				description = descriptions[i].description;
				states = descriptions[i].states;
				contextual = descriptions[i].module != nullptr;
				break;
			}
		}
	}
	if (description.empty() && ((index >= 10 && index <= 12) ||
		(index >= 14 && index <= 16) || index == 19))
		description = "Shared scene value";
	if (states) {
		const char *meaning = "unknown state";
		for (uint i = 0; states[i].description; ++i) {
			if (value == states[i].value) {
				meaning = states[i].description;
				break;
			}
		}
		description = Common::String::format("%s: %s", description.c_str(), meaning);
	}
	if (contextual) {
		if ((index >= 14 && index <= 16) || index == 19)
			return Common::String::format("Shared scene value\n          // Currently: %s / %s",
				description.c_str(), module.c_str());
		return Common::String::format("Shared scene value\n          // Currently: %s",
			description.c_str());
	}
	return description;
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
		return true;
	}

	static const char usage[] = "Usage: emmaVars [details | changed | start [count]]\n";
	const bool compactView = argc == 1;
	const bool detailsView = argc == 2 && Common::String(argv[1]).equalsIgnoreCase("details");
	if (!compactView && !detailsView && (argv[1][0] < '0' || argv[1][0] > '9')) {
		debugPrintf("%s", usage);
		return true;
	}
	int start = compactView || detailsView ? 0 : atoi(argv[1]);
	int count = compactView || detailsView ? (int)state.variables.size() :
		(argc > 2 ? atoi(argv[2]) : 64);
	if (start < 0 || count < 1 || (uint)start >= state.variables.size()) {
		debugPrintf("%s", usage);
		return true;
	}
	const uint end = MIN<uint>((uint)start + (uint)count,
							 state.variables.size());
	if (compactView) {
		debugPrintf("Use emmaVars details for grouped descriptions and current states.\n");
		debugPrintf("Module: %s\n", state.module.c_str());
		uint columns = 0;
		for (uint i = 0; i < state.variables.size(); ++i) {
			if (!emmanuelleVariableUsed(i) && state.variables[i] == 0)
				continue;
			const Common::String cell = Common::String::format("v[%u]=%u", i, (uint)state.variables[i]);
			debugPrintf("%-13s", cell.c_str());
			if (++columns == 6) {
				debugPrintf("\n");
				columns = 0;
			}
		}
		if (columns)
			debugPrintf("\n");
		_emmanuellePreviousVariables = state.variables;
		return true;
	}
	if (detailsView) {
		debugPrintf("Module: %s\n", state.module.c_str());
		debugPrintf("Stored values may be inactive or left over from another scene.\n");
		static const char *const groupNames[] = {
			"Game status and setup", "Inventory and story progress", "Encounter progress",
			"Scene and shared script state", "Conversation state", "Hotel information and gossip",
			"Roulette", "Protection", "Other nonzero values"
		};
		for (uint category = 0; category < ARRAYSIZE(groupNames); ++category) {
			debugPrintf("\n%s:\n", groupNames[category]);
			bool hasValues = false;
			for (uint i = 0; i < state.variables.size(); ++i) {
				if ((uint)emmanuelleVariableGroup(i) != category ||
					(category == kEmmaOtherNonzero && state.variables[i] == 0))
					continue;
				const Common::String description = emmanuelleVariableDescription(
					i, state.variables[i], state.module);
				debugPrintf("v[%u]=%u%s%s\n", i, (uint)state.variables[i],
					description.empty() ? "" : "  // ", description.c_str());
				hasValues = true;
			}
			if (!hasValues)
				debugPrintf("(none)\n");
		}
		_emmanuellePreviousVariables = state.variables;
		return true;
	}
	for (uint i = (uint)start; i < end; ++i) {
		const Common::String description = emmanuelleVariableDescription(
			i, state.variables[i], state.module);
		debugPrintf("v[%u]=%u%s%s\n", i, (uint)state.variables[i],
			description.empty() ? "" : "  // ", description.c_str());
	}
	_emmanuellePreviousVariables = state.variables;
	return true;
}

static bool parseEmmanuelleVariableNumber(const char *argument, uint32 maximum, uint16 &number) {
	if (!*argument)
		return false;
	uint32 value = 0;
	for (; *argument; ++argument) {
		if (*argument < '0' || *argument > '9')
			return false;
		const uint digit = *argument - '0';
		if (value > (maximum - digit) / 10)
			return false;
		value = value * 10 + digit;
	}
	number = value;
	return true;
}

bool GobConsole::cmd_emmaVarsSet(int argc, const char **argv) {
	if (!_emmanuelleDebugger) {
		debugPrintf("No Emmanuelle runtime is active.\n");
		return true;
	}
	uint16 index, value, previousValue;
	if (argc != 3 || !parseEmmanuelleVariableNumber(argv[1], 511, index) ||
		!parseEmmanuelleVariableNumber(argv[2], 65535, value)) {
		debugPrintf("Usage: emmaVarsSet INDEX VALUE (index 0-511, value 0-65535)\n");
	} else if (!_emmanuelleDebugger->setDiagnosticVariable(index, value, previousValue)) {
		debugPrintf("No Emmanuelle VM is active.\n");
	} else {
		debugPrintf("v[%u]: %u -> %u\n", (uint)index, (uint)previousValue, (uint)value);
	}
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

namespace Emmanuelle {

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


void PreGob_Emmanuelle::exportDiagnosticSnapshot(
	OldTotDiagnosticSnapshot &out) const {
	out = OldTotDiagnosticSnapshot();
	out.platform = _platformTag;
	out.module = _moduleFileStem;
	out.input.key = _pendingInput.peek(out.input.mouseX, out.input.mouseY,
		out.input.mouseButtons);
	out.cursorVisible = CursorMan.isVisible();
	out.saveBoundary = saveBoundaryState();
	out.traceEnabled = _diagnosticTraceEnabled;
	out.schedulerAccumulator = _schedulerAccumulator;
	out.schedulerDebt = _schedulerDebt;
	out.variables.resize(ARRAYSIZE(_variables));
	if (_scriptVM) {
		out.hasVM = true;
		_scriptVM->exportTransientState(out.vm);
		_scriptVM->storeVariables(out.variables.data(), out.variables.size());
	} else {
		for (uint i = 0; i < ARRAYSIZE(_variables); ++i)
			out.variables[i] = _variables[i];
	}
	if (_executor) {
		out.hasExecutor = true;
		_executor->exportState(out.executor);
	}
	// Selector resolution updates variables, so use a copy of the snapshot.
	uint16 policyVariables[ARRAYSIZE(_variables)];
	out.selector = out.variables[0];
	for (uint i = 0; i < out.variables.size(); ++i)
		policyVariables[i] = out.variables[i];
	out.postPolicySelector = adjustModuleSelector(
		policyVariables, ARRAYSIZE(policyVariables));
	const ModuleTransition transition =
		moduleTransition(out.postPolicySelector);
	out.nextModule = moduleFileStem(transition.module);

	out.native.audioKind = _audioKind;
	out.native.audioSlot = _audioSlot;
	out.native.audioFrequency = _audioFrequency;
	out.native.audioRepeatCount = _audioRepeatCount;
	out.native.audioActive = isAudioPlaying(_platformTag, _audioActive,
		_vm->_sound->blasterPlayingSound());
	out.native.audioBlocking = _audioBlocking && out.native.audioActive;
	out.native.sampleBank = _sampleBank;
	out.native.sampleCount = _sampleCount;
	out.native.initialInitComplete = _initialInitComplete;
	out.native.skipIntermediateSequenceNodes = _skipIntermediateSequenceNodes;
	out.native.colorCycleEnabled = _colorCycleEnabled;
	out.native.colorCounter = _colorCycleCounter;
	out.native.activeObjectCount = _active.activeCount();
	out.native.savedRegionDepth = _renderer.savedRegionDepth();
	if (_platformTag == kOldTotPlatformAtariST ||
		_platformTag == kOldTotPlatformAmiga) {
		out.native.presentationMode = _renderer.presentationMode();
		out.native.presentationVariant = _renderer.presentationVariant();
		out.native.presentationPhase = _renderer.presentationPhase();
		out.native.presentationDirty = _renderer.presentationPending();
		out.native.presentationActive = _renderer.presentationActive();
	}
	if (_platformTag == kOldTotPlatformAtariST)
		out.native.paletteBlack = _renderer.atariBlack();
	else if (_platformTag == kOldTotPlatformAmiga) {
		out.native.paletteTransition = _renderer.amigaPaletteTransitionPending();
		out.native.paletteBlack = _renderer.amigaBlack();
		out.native.presentedSnapshot = _renderer.amigaPresentedSnapshotValid();
		out.native.moduleGeneration = _renderer.amigaModuleGeneration();
		out.native.presentedGeneration = _renderer.amigaPresentedGeneration();
	}
}

bool PreGob_Emmanuelle::queryProtectionHint(
	ProtectionChallenge &out) const {
	uint16 node = 0;
	if (!_tot || !_scriptVM || !_executor ||
		!_executor->activeInputNode(node) || uint32(node) + 6U > _tot->size())
		return false;

	uint16 variables[7];
	for (uint i = 0; i < ARRAYSIZE(variables); ++i)
		variables[i] = _scriptVM->readVariable(i);
	return buildProtectionHint(_moduleFileStem.equalsIgnoreCase("INIT"), true,
		_tot->byteAt(node), _tot->byteAt(node + 5), variables, out);
}

void PreGob_Emmanuelle::setDiagnosticTraceEnabled(bool enabled) {
	_diagnosticTraceEnabled = enabled;
	debugC(1, kDebugGameFlow, "Emmanuelle: diagnostic trace %s",
		   enabled ? "enabled" : "disabled");
}

bool PreGob_Emmanuelle::setDiagnosticVariable(uint16 index, uint16 value, uint16 &previousValue) {
	if (!_scriptVM || index >= ARRAYSIZE(_variables))
		return false;
	previousValue = _scriptVM->readVariable(index);
	_scriptVM->writeVariable(index, value);
	return true;
}

} // End of namespace Emmanuelle

} // End of namespace Gob
