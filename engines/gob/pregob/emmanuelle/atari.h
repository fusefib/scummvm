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

#ifndef GOB_PREGOB_EMMANUELLE_ATARI_H
#define GOB_PREGOB_EMMANUELLE_ATARI_H

#include "common/scummsys.h"
#include "common/str.h"

namespace Gob {
namespace Emmanuelle {

enum AtariStAudioStopMode {
	kAtariStAudioStopNone = 0,
	kAtariStAudioStopIfPlaying,
	kAtariStAudioStopAlways
};

struct AtariStSampleCommand {
	AtariStSampleCommand() {
		slot = 0;
		mixerRepeatCount = 1;

		waitForCompletion = false;
		postDelay = 0;
		stopMode = kAtariStAudioStopNone;
	}

	byte slot;
	int16 mixerRepeatCount;
	bool waitForCompletion;
	uint16 postDelay;
	AtariStAudioStopMode stopMode;
};

enum AtariStAudioPolicy {
	kAtariStAudioRequired = 0,
	kAtariStAudioSilent
};

bool getAtariStSampleCommand(byte command, AtariStSampleCommand &out);
AtariStAudioPolicy atariStAudioPolicy(bool optionalAudio,
									 bool anySamplePresent);
const byte *getAtariStIntroComposition(uint &count);

uint16 atariStRandomBounded(uint32 nativeRandomValue,
							uint16 upperExclusive);
void getAtariStSchedulerTiming(byte period, uint16 &reload,
							   uint16 &initialCountdown);

bool getAtariStModulePalette(const Common::String &moduleName,
							 uint16 words[16]);
bool getAtariStNumberedPalette(byte paletteNumber, uint16 words[16]);
bool getAtariStEffect45(byte variant, uint16 words[4]);
byte atariStPaletteComponent(uint16 word, byte shift);
uint16 dimAtariStPaletteWord(uint16 word, byte steps);

byte prepareAtariStWaitColorCounter(byte counter);
uint16 atariStAnimationDelayChunk(uint16 remainingUnits);
bool atariStContinuationAcceptsKey(int16 key);

uint16 atariStMode43Mask(byte phase, uint32 wordOrdinal);
bool getAtariStRestoredDirtyBounds(uint16 x, uint16 width,
	uint16 screenWidth, uint16 &left, uint16 &right);

struct AtariStMode44Rect {
	AtariStMode44Rect() : x(0), y(0), width(0), height(0) {}

	int16 x;
	int16 y;
	uint16 width;
	uint16 height;
};

byte atariStMode44PhaseCount(byte variant);
bool getAtariStMode44Rect(byte variant, byte phase,
	AtariStMode44Rect &out);
byte atariStAnimationVblCount();

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_ATARI_H
