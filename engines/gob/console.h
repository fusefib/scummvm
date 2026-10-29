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

#ifndef GOB_CONSOLE_H
#define GOB_CONSOLE_H

#include "common/array.h"
#include "gui/debugger.h"

namespace Gob {

class GobEngine;
class Cheater;

namespace Emmanuelle {
class OldTotDebugProvider;
}

class GobConsole : public GUI::Debugger {
public:
	GobConsole(GobEngine *vm);
	~GobConsole(void) override;

	void registerCheater(Cheater *cheater);
	void unregisterCheater();
	void registerEmmanuelleDebugger(Emmanuelle::OldTotDebugProvider *provider);
	void unregisterEmmanuelleDebugger();

private:
	GobEngine *_vm;

	Cheater *_cheater;
	Emmanuelle::OldTotDebugProvider *_emmanuelleDebugger;
	Common::Array<uint16> _emmanuellePreviousVariables;

	bool requireInterVariables();

	bool cmd_Help(int argc, const char **argv);
	void printEmmanuelleHelp();

	bool cmd_varSize(int argc, const char **argv);
	bool cmd_dumpVars(int argc, const char **argv);
	bool cmd_var8(int argc, const char **argv);
	bool cmd_var16(int argc, const char **argv);
	bool cmd_var32(int argc, const char **argv);
	bool cmd_varString(int argc, const char **argv);

	bool cmd_cheat(int argc, const char **argv);

	bool cmd_listArchives(int argc, const char **argv);

	bool cmd_gobStack(int argc, const char **argv);
	bool cmd_emmaState(int argc, const char **argv);
	bool cmd_emmaVars(int argc, const char **argv);
	bool cmd_emmaVarsSet(int argc, const char **argv);
	bool cmd_emmaStack(int argc, const char **argv);
	bool cmd_emmaTrace(int argc, const char **argv);
	bool cmd_cpSolve(int argc, const char **argv);
};

} // End of namespace Gob

#endif // GOB_CONSOLE_H
