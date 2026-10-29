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

struct SpriteBounds {
	SpriteBounds() : x(0), y(0), w(0), h(0) {}
	SpriteBounds(int16 px, int16 py, int16 pw, int16 ph) : x(px), y(py), w(pw), h(ph) {}
	int16 x, y, w, h;
};

struct ActiveCompositePlan {
	SpriteBounds oldRect;
	SpriteBounds newRect;
	SpriteBounds dirtyUnion;
	SpriteBounds alignedUnion;
};

ActiveCompositePlan planActiveComposite(int16 oldX, int16 oldY,
										int16 newX, int16 newY, int16 width, int16 height);

struct ActivePairCompositePlan {
	ActiveCompositePlan first;
	ActiveCompositePlan second;
	SpriteBounds dirtyUnion;
	SpriteBounds alignedUnion;
};

ActivePairCompositePlan planActivePairComposite(
	int16 firstOldX, int16 firstOldY, int16 firstNewX, int16 firstNewY,
	int16 firstWidth, int16 firstHeight, int16 secondOldX, int16 secondOldY,
	int16 secondNewX, int16 secondNewY, int16 secondWidth, int16 secondHeight);

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_GAMEFLOW_H
