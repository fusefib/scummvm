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

#ifndef GOB_PREGOB_EMMANUELLE_AMIGA_H
#define GOB_PREGOB_EMMANUELLE_AMIGA_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {
namespace Emmanuelle {

struct AmigaSampleCommand {
	AmigaSampleCommand() {
		slot = 0;
		mixerRepeatCount = 1;

		preDelay = 0;
		postDelay = 0;
		stopAllFirst = false;
	}

	byte slot;
	int16 mixerRepeatCount;
	uint16 preDelay;
	uint16 postDelay;
	bool stopAllFirst;
};

bool getAmigaSampleCommand(byte command, AmigaSampleCommand &out);
bool amigaHandlesNativeCommand(byte command);

uint16 amigaNativeRand(uint32 &state);
uint16 amigaRandomBounded(uint32 &state, uint16 upperExclusive);
uint16 amigaNativeDelayMilliseconds(uint16 units);
void getAmigaSchedulerTiming(byte period, uint16 &reload,
							 uint16 &initialCountdown);
uint16 amigaInitialModuleSelector(uint16 scriptSelector,
								 bool forceLoadHot);

bool decodeAmigaPackedPixels(const Common::Array<byte> &payload,
							 uint16 width, uint16 height,
							 Common::Array<byte> &pixels);
bool getAmigaModulePalette(const Common::String &moduleName,
						   uint16 words[32]);
bool getAmigaTextColorTable(const Common::String &moduleName, byte colors[16]);
bool getAmigaNumberedPalette(byte paletteNumber, uint16 words[32]);
bool getAmigaEffect45(byte variant, uint16 words[5]);
byte amigaPaletteComponent(uint16 word, byte shift);
uint16 dimAmigaPaletteWord(uint16 word, byte steps);

byte prepareAmigaWaitColorCounter(byte counter);


byte amigaMode43PixelOffset(byte phase, uint32 wordOrdinal);

struct AmigaMode44Rect {
	AmigaMode44Rect() : x(0), y(0), width(0), height(0) {}

	int16 x;
	int16 y;
	uint16 width;
	uint16 height;
};

bool getAmigaMode44Rect(byte variant, byte stage, AmigaMode44Rect &out);
byte amigaMode44WaitCount(byte variant);
bool amigaSnapshotGenerationUsable(
	uint32 moduleGeneration, uint32 presentedGeneration, bool hasSnapshot);

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_AMIGA_H
