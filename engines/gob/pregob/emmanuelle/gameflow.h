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

#ifndef GOB_PREGOB_EMMANUELLE_GAMEFLOW_H
#define GOB_PREGOB_EMMANUELLE_GAMEFLOW_H

#include "common/scummsys.h"

namespace Gob {
namespace Emmanuelle {

enum ModuleId {
	kModuleNone = 0,
	kModuleInit,
	kModuleLoadHot,
	kModuleLoadPla,
	kModuleLoadCas,
	kModuleLoadFav,
	kModuleLoadAero,
	kModuleLoadGar,
	kModuleLoadChut
};

struct ModuleTransition {
	ModuleTransition() {
		module = kModuleNone;

		shouldExit = false;
		resetForInit = false;
		usesLocalGraphics = false;
		reuseCurrent = false;
	}
	ModuleId module;
	bool shouldExit;
	bool resetForInit;
	bool usesLocalGraphics;
	bool reuseCurrent;
};

ModuleTransition moduleTransition(uint16 selector);
const char *moduleFileStem(ModuleId module);
bool moduleNeedsLocalGraphicsFile(ModuleId module,
									bool usesLocalGraphics,
									bool inlineAeroAndPla);
void resetInitVariables(uint16 *variables, uint count);
uint16 adjustModuleSelector(uint16 *variables, uint count);

class ModuleHost {
public:
	virtual ~ModuleHost() {}
	virtual bool shouldQuit() const { return false; }
	virtual bool loadModule(ModuleId module, bool usesLocalGraphics) = 0;
	virtual bool executeLoadedModule() = 0;
	virtual void unloadModule() = 0;
	/** Override the first module's next selector on a fresh game.
	 * Return true to skip the ordinary post-module selector adjustment. */
	virtual bool applyInitialModuleSelectorPolicy(ModuleId,
											 uint16 &) { return false; }
	virtual ModuleId loadedModule() const { return kModuleNone; }
	virtual bool loadedModuleUsesLocalGraphics() const { return false; }
};

enum ModuleLoopResult {
	kModuleLoopExit = 0,
	kModuleLoopUnmappedSelector,
	kModuleLoopLoadError,
	kModuleLoopExecutionError,
	kModuleLoopQuit,
	kModuleLoopLimit
};

/** maxModules limits module execution for tests. Zero disables the limit. */
ModuleLoopResult runModuleLoop(ModuleHost &host, uint16 *variables,
									 uint count, uint maxModules = 0);
ModuleLoopResult resumeModuleLoop(ModuleHost &host,
	uint16 *variables, uint count, ModuleId currentModule,
	bool currentUsesLocalGraphics, uint maxModules = 0);

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_GAMEFLOW_H
