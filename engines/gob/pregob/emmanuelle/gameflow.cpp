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

#include "gob/pregob/emmanuelle/gameflow.h"

namespace Gob {
namespace Emmanuelle {

ModuleTransition moduleTransition(uint16 selector) {
	ModuleTransition t;
	if (selector == 40) {
		t.shouldExit = true;
		return t;
	}
	switch (selector) {
	case 0:
	case 13:
	case 14:
	case 15:
	case 16:
		t.module = kModuleInit;
		t.resetForInit = true;
		break;
	case 1:
	case 2:
		t.module = kModuleLoadHot;
		t.usesLocalGraphics = true;
		break;
	case 3:
	case 12:
		t.module = kModuleLoadPla;
		t.usesLocalGraphics = true;
		break;
	case 4:
		t.module = kModuleLoadCas;
		t.usesLocalGraphics = true;
		break;
	case 5:
		t.module = kModuleLoadFav;
		break;
	case 6:
	case 7:
		t.module = kModuleLoadAero;
		t.usesLocalGraphics = true;
		break;
	case 8:
	case 9:
		t.module = kModuleLoadGar;
		break;
	case 10:
		t.module = kModuleLoadChut;
		break;
	case 11:
		t.reuseCurrent = true;
		break;
	default:
		break;
	}
	return t;
}

const char *moduleFileStem(ModuleId module) {
	switch (module) {
	case kModuleInit:
		return "INIT";
	case kModuleLoadHot:
		return "LOADHOT";
	case kModuleLoadPla:
		return "LOADPLA";
	case kModuleLoadCas:
		return "LOADCAS";
	case kModuleLoadFav:
		return "LOADFAV";
	case kModuleLoadAero:
		return "LOADAERO";
	case kModuleLoadGar:
		return "LOADGAR";
	case kModuleLoadChut:
		return "LOADCHUT";
	default:
		return nullptr;
	}
}

bool moduleNeedsLocalGraphicsFile(ModuleId module,
									bool usesLocalGraphics,
									bool inlineAeroAndPla) {
	if (!usesLocalGraphics)
		return false;
	if (!inlineAeroAndPla)
		return true;
	return module != kModuleLoadAero && module != kModuleLoadPla;
}

void resetInitVariables(uint16 *variables, uint count) {
	if (!variables)
		return;
	for (uint i = 1; i <= 76 && i < count; ++i)
		variables[i] = 0;
	for (uint i = 78; i < 512 && i < count; ++i)
		variables[i] = 0;
}

uint16 adjustModuleSelector(uint16 *variables, uint count) {
	if (!variables || count == 0)
		return 40;
	uint16 selector = variables[0];
	if (selector != 40 && count > 29 && variables[29] == 0)
		selector = 13;
	if (selector != 40 && count > 21 && variables[21] == 0)
		selector = 16;
	variables[0] = selector;
	return selector;
}

static ModuleLoopResult runModuleLoop(ModuleHost &host,
		uint16 *variables, uint count, ModuleId currentModule,
		bool currentUsesLocalGraphics, bool moduleLoaded, bool freshLifecycle,
		uint maxModules) {
	if (!variables || count == 0)
		return kModuleLoopExecutionError;
	for (uint iteration = 0; !maxModules || iteration < maxModules;
		 ++iteration) {
		if (host.shouldQuit())
			return kModuleLoopQuit;
		if (!moduleLoaded) {
			ModuleTransition t = moduleTransition(variables[0]);
			if (t.shouldExit)
				return kModuleLoopExit;
			if (t.reuseCurrent) {
				if (currentModule == kModuleNone)
					return kModuleLoopUnmappedSelector;
				t.module = currentModule;
				t.usesLocalGraphics = currentUsesLocalGraphics;
			}
			const char *stem = moduleFileStem(t.module);
			if (!stem)
				return kModuleLoopUnmappedSelector;
			if (t.resetForInit)
				resetInitVariables(variables, count);
			if (!host.loadModule(t.module, t.usesLocalGraphics))
				return kModuleLoopLoadError;
			currentModule = t.module;
			currentUsesLocalGraphics = t.usesLocalGraphics;
		}
		moduleLoaded = false;
		if (host.shouldQuit()) {
			host.unloadModule();
			return kModuleLoopQuit;
		}
		const bool ok = host.executeLoadedModule();
		const ModuleId reportedModule = host.loadedModule();
		if (reportedModule != kModuleNone) {
			currentModule = reportedModule;
			currentUsesLocalGraphics =
				host.loadedModuleUsesLocalGraphics();
		}
		host.unloadModule();
		if (host.shouldQuit())
			return kModuleLoopQuit;
		if (!ok)
			return kModuleLoopExecutionError;
		if (!freshLifecycle ||
			!host.applyInitialModuleSelectorPolicy(currentModule, variables[0]))
			adjustModuleSelector(variables, count);
		freshLifecycle = false;
	}
	return kModuleLoopLimit;
}

ModuleLoopResult runModuleLoop(ModuleHost &host,
		uint16 *variables, uint count, uint maxModules) {
	return runModuleLoop(host, variables, count, kModuleNone, false, false,
		true, maxModules);
}

ModuleLoopResult resumeModuleLoop(ModuleHost &host,
		uint16 *variables, uint count, ModuleId currentModule,
		bool currentUsesLocalGraphics, uint maxModules) {
	if (currentModule == kModuleNone)
		return kModuleLoopUnmappedSelector;
	return runModuleLoop(host, variables, count, currentModule,
		currentUsesLocalGraphics, true, false, maxModules);
}

} // namespace Emmanuelle
} // namespace Gob
