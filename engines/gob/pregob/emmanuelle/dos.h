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

#ifndef GOB_PREGOB_EMMANUELLE_DOS_H
#define GOB_PREGOB_EMMANUELLE_DOS_H

#include "common/scummsys.h"
#include "common/rect.h"
#include "common/rendermode.h"
#include "common/str.h"

#include "gob/pregob/emmanuelle/interpreter.h"

namespace Gob {
namespace Emmanuelle {

uint16 dosRandomBounded(uint32 &state, uint16 upperExclusive);
void getDosSchedulerTiming(byte period,
						   uint16 &reload, uint16 &initialCountdown);

enum {
	kDosFightInitialHoldMilliseconds = 1000,
	kDosFightTickMinimumMilliseconds = 100
};
uint16 dosOpcode03DelayMilliseconds(byte argument);
/** Deterministic INIT-only cost of a native direct-video sprite write.
 * Calibrated against the fixed 1000-cycle CGA/EGA reference captures.
 */
uint16 dosIntroDrawDelayMilliseconds(byte videoMode, uint16 width,
									 uint16 height);
bool extendDosInitialFrameCoverage(Common::Rect &coverage, bool &hasCoverage,
								   int16 x, int16 y, uint16 width,
								   uint16 height);

byte dosEgaPaletteIndex(byte registerValue);
byte dosVideoModeForRenderMode(Common::RenderMode renderMode);
bool getDosPaletteRegisters(const Common::String &moduleName,
							byte registers[16]);
bool getDosCgaColorTables(const Common::String &moduleName,
						  byte evenTable[16], byte oddTable[16]);
byte dosCgaDitheredColor(byte logicalColor, uint16 x, uint16 y,
						 const byte evenTable[16],
						 const byte oddTable[16]);

/** Apply a DOS-native FF0B command. The caller rebuilds the video backend
 * when reinitializeVideo is set.
 */
bool applyDosNativeCommand(byte command, OldTotRuntimeState &runtime,
						   bool &reinitializeVideo,
						   bool forceFourColor = true);

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_DOS_H
