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

#include "gob/pregob/emmanuelle/dos.h"

namespace Gob {
namespace Emmanuelle {

namespace {

const byte kPaletteGroup1[16] = {
	0x00, 0x17, 0x01, 0x11, 0x13, 0x02, 0x12, 0x10,
	0x07, 0x14, 0x00, 0x15, 0x04, 0x06, 0x16, 0x17};

const byte kPaletteGroup2[16] = {
	0x00, 0x05, 0x01, 0x11, 0x07, 0x02, 0x12, 0x10,
	0x17, 0x14, 0x00, 0x15, 0x04, 0x06, 0x16, 0x17};

const byte kPaletteChut[16] = {
	0x00, 0x10, 0x01, 0x11, 0x13, 0x02, 0x12, 0x07,
	0x17, 0x14, 0x00, 0x03, 0x04, 0x06, 0x16, 0x17};

const byte kPaletteInit[16] = {
	0x00, 0x00, 0x01, 0x11, 0x10, 0x00, 0x12, 0x02,
	0x16, 0x04, 0x00, 0x14, 0x11, 0x17, 0x06, 0x16};

const byte kCgaGroup1Even[16] = {
	0, 3, 0, 0, 3, 1, 3, 0, 3, 2, 0, 2, 1, 2, 2, 3};

const byte kCgaGroup1Odd[16] = {
	0, 3, 0, 1, 0, 1, 1, 1, 0, 2, 0, 0, 2, 3, 3, 3};

const byte kCgaGroup2Even[16] = {
	0, 1, 0, 0, 3, 1, 3, 0, 3, 2, 0, 0, 1, 2, 2, 3};

const byte kCgaGroup2Odd[16] = {
	0, 2, 0, 1, 0, 1, 1, 1, 0, 2, 0, 2, 2, 3, 3, 3};

const byte kCgaChutEven[16] = {
	0, 0, 0, 0, 3, 1, 0, 0, 3, 2, 0, 0, 1, 2, 2, 3};

const byte kCgaChutOdd[16] = {
	0, 0, 0, 1, 0, 1, 1, 1, 0, 2, 0, 3, 2, 3, 3, 3};

const byte kCgaInitEven[16] = {
	0, 0, 0, 1, 3, 3, 3, 3, 2, 2, 3, 3, 1, 1, 1, 2};

const byte kCgaInitOdd[16] = {
	0, 3, 0, 0, 3, 0, 1, 0, 2, 2, 0, 3, 1, 1, 1, 2};

const byte *findDosPaletteRegisters(const Common::String &moduleName) {
	if (moduleName.equalsIgnoreCase("INIT") ||
		moduleName.equalsIgnoreCase("INIT.TOT"))
		return kPaletteInit;
	if (moduleName.equalsIgnoreCase("LOADCHUT") ||
		moduleName.equalsIgnoreCase("LOADCHUT.TOT"))
		return kPaletteChut;
	if (moduleName.equalsIgnoreCase("LOADAERO") ||
		moduleName.equalsIgnoreCase("LOADAERO.TOT") ||
		moduleName.equalsIgnoreCase("LOADGAR") ||
		moduleName.equalsIgnoreCase("LOADGAR.TOT"))
		return kPaletteGroup2;
	if (moduleName.equalsIgnoreCase("LOADHOT") ||
		moduleName.equalsIgnoreCase("LOADHOT.TOT") ||
		moduleName.equalsIgnoreCase("LOADPLA") ||
		moduleName.equalsIgnoreCase("LOADPLA.TOT") ||
		moduleName.equalsIgnoreCase("LOADCAS") ||
		moduleName.equalsIgnoreCase("LOADCAS.TOT") ||
		moduleName.equalsIgnoreCase("LOADFAV") ||
		moduleName.equalsIgnoreCase("LOADFAV.TOT"))
		return kPaletteGroup1;
	return nullptr;
}

bool findDosCgaColorTables(const Common::String &moduleName,
						   const byte *&evenTable, const byte *&oddTable) {
	evenTable = nullptr;
	oddTable = nullptr;
	if (moduleName.equalsIgnoreCase("INIT") ||
		moduleName.equalsIgnoreCase("INIT.TOT")) {
		evenTable = kCgaInitEven;
		oddTable = kCgaInitOdd;
	} else if (moduleName.equalsIgnoreCase("LOADCHUT") ||
			   moduleName.equalsIgnoreCase("LOADCHUT.TOT")) {
		evenTable = kCgaChutEven;
		oddTable = kCgaChutOdd;
	} else if (moduleName.equalsIgnoreCase("LOADAERO") ||
			   moduleName.equalsIgnoreCase("LOADAERO.TOT") ||
			   moduleName.equalsIgnoreCase("LOADGAR") ||
			   moduleName.equalsIgnoreCase("LOADGAR.TOT")) {
		evenTable = kCgaGroup2Even;
		oddTable = kCgaGroup2Odd;
	} else if (moduleName.equalsIgnoreCase("LOADHOT") ||
			   moduleName.equalsIgnoreCase("LOADHOT.TOT") ||
			   moduleName.equalsIgnoreCase("LOADPLA") ||
			   moduleName.equalsIgnoreCase("LOADPLA.TOT") ||
			   moduleName.equalsIgnoreCase("LOADCAS") ||
			   moduleName.equalsIgnoreCase("LOADCAS.TOT") ||
			   moduleName.equalsIgnoreCase("LOADFAV") ||
			   moduleName.equalsIgnoreCase("LOADFAV.TOT")) {
		evenTable = kCgaGroup1Even;
		oddTable = kCgaGroup1Odd;
	}
	return evenTable && oddTable;
}

} // End of anonymous namespace

uint16 dosRandomBounded(uint32 &state, uint16 upperExclusive) {
	if (upperExclusive == 0)
		return 0;

	state = state * 129U + 0x361962E9U;
	return (uint16)(((state >> 17) & 0x7FFFU) % upperExclusive);
}

void getDosSchedulerTiming(byte period,
						   uint16 &reload, uint16 &initialCountdown) {
	reload = (uint16)period * 100;
	if (period == 50 || period == 100 || period == 200)
		reload = (period == 50 ? 1500 : 2000) * 5;
	initialCountdown = 1;
}

uint16 dosOpcode03DelayMilliseconds(byte argument) {
	return (uint16)argument * 3;
}

uint16 dosIntroDrawDelayMilliseconds(byte videoMode, uint16 width,
									 uint16 height) {
	const uint32 pixels = (uint32)width * height;
	if (videoMode == 1)
		return (uint16)(48U + (pixels * 16U + 500U) / 1000U);
	return (uint16)(155U + (pixels * 35U + 500U) / 1000U);
}

bool extendDosInitialFrameCoverage(Common::Rect &coverage, bool &hasCoverage,
								   int16 x, int16 y, uint16 width,
								   uint16 height) {
	Common::Rect drawRect(x, y, x + width, y + height);
	drawRect.clip(Common::Rect(0, 0, 320, 200));
	if (drawRect.isEmpty())
		return false;

	if (hasCoverage)
		coverage.extend(drawRect);
	else {
		coverage = drawRect;
		hasCoverage = true;
	}
	return coverage.left == 0 && coverage.top == 0 &&
		coverage.right == 320 && coverage.bottom == 200;
}

byte dosEgaPaletteIndex(byte registerValue) {
	return (registerValue & 0x07) |
		   ((registerValue & 0x10) ? 0x08 : 0x00);
}

byte dosVideoModeForRenderMode(Common::RenderMode renderMode) {
	return renderMode == Common::kRenderCGA ? 1 : 2;
}

bool getDosPaletteRegisters(const Common::String &moduleName,
							byte registers[16]) {
	const byte *source = findDosPaletteRegisters(moduleName);
	if (!source || !registers)
		return false;

	for (uint i = 0; i < 16; ++i)
		registers[i] = source[i];
	return true;
}

bool getDosCgaColorTables(const Common::String &moduleName,
						  byte evenTable[16], byte oddTable[16]) {
	const byte *evenSource = nullptr;
	const byte *oddSource = nullptr;
	if (!evenTable || !oddTable ||
		!findDosCgaColorTables(moduleName, evenSource, oddSource))
		return false;
	for (uint i = 0; i < 16; ++i) {
		evenTable[i] = evenSource[i];
		oddTable[i] = oddSource[i];
	}
	return true;
}

byte dosCgaDitheredColor(byte logicalColor, uint16 x, uint16 y,
						 const byte evenTable[16],
						 const byte oddTable[16]) {
	if (!evenTable || !oddTable)
		return 0;
	const byte *table = ((x + y) & 1) ? oddTable : evenTable;
	return table[logicalColor & 0x0F];
}

bool applyDosNativeCommand(byte command, OldTotRuntimeState &runtime,
						   bool &reinitializeVideo, bool forceFourColor) {
	reinitializeVideo = false;
	if (command != 201)
		return false;

	runtime.savedVideoMode = runtime.videoMode;
	if (forceFourColor && runtime.videoMode > 1) {
		runtime.videoMode = 1;
		runtime.videoInitMarker = 0;
		reinitializeVideo = true;
	}
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
