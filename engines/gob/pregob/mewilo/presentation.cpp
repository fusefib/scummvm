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
 */

#include "gob/pregob/mewilo/presentation.h"

#include "common/util.h"

#include "gob/pregob/mewilo/tot.h"

namespace Gob {
namespace Mewilo {

MewiloTimingPlan::MewiloTimingPlan() :
	anchor(kMewiloElapsedWait), milliseconds(0), minimumDelay(0), minimumExposure(0), region(0, 0, 320, 200),
	rule("native-wait"), calibrated(false), animation(false), reading(false), textNode(0) {
}

uint32 mewiloScaleTime(uint32 milliseconds, uint speedFactor) {
	// Scale ONCE, round up so a nonzero visibility budget cannot vanish.
	const uint32 speed = MAX<uint32>(1, speedFactor);
	return milliseconds / speed + ((milliseconds % speed) != 0);
}

uint32 mewiloAnimationDelayMillis(uint32 frameMillis, byte operand) {
	if (!operand)
		return 0;
	// Story cues share one modern pace; operands over 100 retain longer beats.
	return MAX<uint32>(frameMillis, (uint64(frameMillis) * operand + 99) / 100);
}

uint32 mewiloReadingMillis(const byte *text, uint length) {
	uint characters = 0;
	for (uint i = 0; text && i < length && text[i]; ++i) {
		const byte c = text[i];
		if (c != '@' && c != '[' && c != ']' && c != '{' && c != '}' && c != ' ')
			++characters;
	}
	// Deliberate readability policy, NOT a recovered native clock constant.
	return MIN<uint32>(4000, MAX<uint32>(1200, 600 + MIN<uint>(characters, 100) * 45));
}

struct TimingSite {
	const char *module;
	Common::Language language;
	byte platforms;
	uint16 site, focus;
	byte operand, focusType;
	bool sceneAnimation;
	const char *name;
	uint16 exposureFocus; // Optional image shown beneath an older dialogue.
};

// Only reviewed story motion uses the shared frame pace. Navigation, scenery
// and repeated portrait reactions retain their authored waits. Local opcode
// guards keep wording/art replacements independent of these script cues.
static const TimingSite kTimingSites[] = {
	{"LOAD1", Common::DE_DEU, 5, 0x16E2, 0x224A, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x16E7, 0x24D3, 20, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x16EC, 0x251C, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x16F1, 0x29A2, 20, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x16F6, 0x2A13, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x16FB, 0x2E99, 20, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x1700, 0x2F0A, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x1705, 0x0629, 20, 0x1D, true, "dialogue-over-animation", 0x3299},
	{"LOAD1", Common::DE_DEU, 5, 0x1747, 0x34B0, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::DE_DEU, 5, 0x1751, 0x391B, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x1324, 0x09B2, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x1327, 0x09B2, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x149A, 0x34CC, 250, 0x41, true, "pig-motion", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x14A2, 0x358E, 250, 0x60, true, "pig-motion", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x159D, 0x35CE, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x15AA, 0x3603, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD2", Common::DE_DEU, 5, 0x15B7, 0x36B1, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD4", Common::DE_DEU, 5, 0x0F26, 0x0C7F, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD4", Common::DE_DEU, 5, 0x0F35, 0x0C47, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD4", Common::DE_DEU, 5, 0x0F42, 0x0CA2, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0552, 0x0091, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0561, 0x1D2B, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0566, 0x1F74, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x056B, 0x20FF, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0570, 0x2348, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0575, 0x20FF, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x057A, 0x2348, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x057F, 0x20FF, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0584, 0x2348, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x0589, 0x2534, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x05AD, 0x0057, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x05CD, 0x3A8F, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x05D2, 0x3B9D, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x05DF, 0x3CA0, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x05E4, 0x3DB2, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::DE_DEU, 5, 0x05E9, 0x3ED4, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x0256, 0x0326, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x025B, 0x0583, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x0260, 0x07B5, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x0267, 0x0112, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x028D, 0x149E, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x0292, 0x1636, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::DE_DEU, 5, 0x0297, 0x176A, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16B2, 0x221A, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16B7, 0x24A3, 20, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16BC, 0x24EC, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16C1, 0x2972, 20, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16C6, 0x29E3, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16CB, 0x2E69, 20, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16D0, 0x2EDA, 50, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x16D5, 0x0627, 20, 0x1D, true, "dialogue-over-animation", 0x3269},
	{"LOAD1", Common::FR_FRA, 5, 0x1717, 0x3480, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD1", Common::FR_FRA, 5, 0x1721, 0x38EB, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x130E, 0x0953, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x1311, 0x0953, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x1484, 0x34B6, 250, 0x41, true, "pig-motion", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x148C, 0x3578, 250, 0x60, true, "pig-motion", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x1587, 0x35B8, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x1594, 0x35ED, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD2", Common::FR_FRA, 5, 0x15A1, 0x369B, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD4", Common::FR_FRA, 5, 0x0FF9, 0x0D63, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD4", Common::FR_FRA, 5, 0x1008, 0x0D2B, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD4", Common::FR_FRA, 5, 0x1015, 0x0D83, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x056C, 0x008D, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x057B, 0x1D45, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x0580, 0x1F8E, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x0585, 0x2119, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x058A, 0x2362, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x058F, 0x2119, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x0594, 0x2362, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x0599, 0x2119, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x059E, 0x2362, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x05A3, 0x254E, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x05C7, 0x0051, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x05E7, 0x3AA9, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x05EC, 0x3BB7, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x05F9, 0x3CBA, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x05FE, 0x3DCC, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD7", Common::FR_FRA, 5, 0x0603, 0x3EEE, 10, 0x41, true, "snake-arrival", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x0237, 0x0305, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x023C, 0x0562, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x0241, 0x0794, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x0248, 0x0117, 200, 0x1D, false, "transient-text", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x026E, 0x147D, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x0273, 0x1615, 100, 0x41, true, "scene-animation", 0x0000},
	{"LOAD8", Common::FR_FRA, 5, 0x0278, 0x1749, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD1", Common::FR_FRA, 2, 0x16B8, 0x0627, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"AMSOAD1", Common::FR_FRA, 2, 0x16EF, 0x1EEA, 100, 0x60, true, "scene-animation", 0x0000},
	{"AMSOAD1", Common::FR_FRA, 2, 0x16F9, 0x1EFA, 100, 0x60, true, "scene-animation", 0x0000},
	{"AMSOAD2", Common::FR_FRA, 2, 0x1205, 0x0953, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"AMSOAD2", Common::FR_FRA, 2, 0x1208, 0x0953, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"AMSOAD4", Common::FR_FRA, 2, 0x0FF9, 0x0D63, 200, 0x1D, false, "transient-text", 0x0000},
	{"AMSOAD4", Common::FR_FRA, 2, 0x1008, 0x0D2B, 200, 0x1D, false, "transient-text", 0x0000},
	{"AMSOAD4", Common::FR_FRA, 2, 0x1015, 0x0D83, 200, 0x1D, false, "transient-text", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x056E, 0x008D, 200, 0x1D, false, "transient-text", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x057D, 0x0E8E, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x0582, 0x1035, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x0587, 0x115A, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x058C, 0x12FE, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x0591, 0x115A, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x0596, 0x12FE, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x059B, 0x115A, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x05A0, 0x12FE, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x05A5, 0x145B, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x05C9, 0x0051, 200, 0x1D, false, "transient-text", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x05E9, 0x1A40, 10, 0x41, true, "snake-arrival", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x05EE, 0x1B09, 10, 0x41, true, "snake-arrival", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x05FB, 0x1BCC, 10, 0x41, true, "snake-arrival", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x0600, 0x1C97, 10, 0x41, true, "snake-arrival", 0x0000},
	{"AMSOAD7", Common::FR_FRA, 2, 0x0605, 0x1D6A, 10, 0x41, true, "snake-arrival", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x0237, 0x0305, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x023C, 0x049D, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x0241, 0x05FA, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x0248, 0x0117, 200, 0x1D, false, "transient-text", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x026E, 0x0F8F, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x0273, 0x10F4, 100, 0x41, true, "scene-animation", 0x0000},
	{"AMSOAD8", Common::FR_FRA, 2, 0x0278, 0x11D6, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD1", Common::FR_FRA, 8, 0x16BE, 0x0627, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"THLOAD1", Common::FR_FRA, 8, 0x1700, 0x24F8, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD1", Common::FR_FRA, 8, 0x170A, 0x2963, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x1261, 0x0953, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x1264, 0x0953, 200, 0x1D, false, "dialogue-over-animation", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x13C9, 0x388C, 250, 0x41, true, "pig-motion", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x13D1, 0x394E, 250, 0x60, true, "pig-motion", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x14BB, 0x398E, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x14C8, 0x39C3, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD2", Common::FR_FRA, 8, 0x14D5, 0x3A71, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD4", Common::FR_FRA, 8, 0x0FF9, 0x0D63, 200, 0x1D, false, "transient-text", 0x0000},
	{"THLOAD4", Common::FR_FRA, 8, 0x1008, 0x0D2B, 200, 0x1D, false, "transient-text", 0x0000},
	{"THLOAD4", Common::FR_FRA, 8, 0x1015, 0x0D83, 200, 0x1D, false, "transient-text", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x056E, 0x008D, 200, 0x1D, false, "transient-text", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x057D, 0x1D47, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x0582, 0x1F90, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x0587, 0x211B, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x058C, 0x2364, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x0591, 0x211B, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x0596, 0x2364, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x059B, 0x211B, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x05A0, 0x2364, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x05A5, 0x2550, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x05C9, 0x0051, 200, 0x1D, false, "transient-text", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x05E9, 0x3AAB, 10, 0x41, true, "snake-arrival", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x05EE, 0x3BB9, 10, 0x41, true, "snake-arrival", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x05FB, 0x3CBC, 10, 0x41, true, "snake-arrival", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x0600, 0x3DCE, 10, 0x41, true, "snake-arrival", 0x0000},
	{"THLOAD7", Common::FR_FRA, 8, 0x0605, 0x3EF0, 10, 0x41, true, "snake-arrival", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x0237, 0x0305, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x023C, 0x0562, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x0241, 0x0794, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x0248, 0x0117, 200, 0x1D, false, "transient-text", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x026E, 0x147D, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x0273, 0x1615, 100, 0x41, true, "scene-animation", 0x0000},
	{"THLOAD8", Common::FR_FRA, 8, 0x0278, 0x1749, 100, 0x41, true, "scene-animation", 0x0000},
};

MewiloTimingPlan mewiloTimingPlan(const MewiloTotData &tot, const char *module,
	Common::Language language, MewiloPlatform platform, uint16 site, byte operand,
	bool readabilityHolds, uint32 animationFrameMillis) {
	MewiloTimingPlan native;
	native.milliseconds = mewiloNativeDelayMillis(platform, operand, native.calibrated);
	native.minimumDelay = native.milliseconds;
	if (!operand || !module || !isValidMewiloPlatform(platform) ||
		!tot.has(site, 3) || tot.byteAt(site) != 0xFF || tot.byteAt(site + 1) != 3 ||
		tot.byteAt(site + 2) != operand)
		return native;
	for (uint i = 0; i < ARRAYSIZE(kTimingSites); ++i) {
		const TimingSite &r = kTimingSites[i];
		const uint headerSize = r.focusType == 0x1D ? 10 : r.focusType == 0x60 ? 8 : 7;
		if (r.site != site || r.operand != operand || !(r.platforms & (1 << platform)) ||
			r.language != language || strcmp(r.module, module) ||
			(!r.sceneAnimation && !readabilityHolds) || !tot.has(r.focus, headerSize) ||
			tot.byteAt(r.focus) != r.focusType)
			continue;
		if (r.exposureFocus &&
			(!tot.has(r.exposureFocus, tot.byteAt(r.exposureFocus) == 0x60 ? 8 : 7) ||
				(tot.byteAt(r.exposureFocus) != 0x41 && tot.byteAt(r.exposureFocus) != 0x60)))
			continue;
		MewiloTimingPlan plan = native;
		plan.rule = r.name;
		// CPC's four-pixel source cells are doubled into these output cells.
		const uint coords = r.focusType == 0x1D ? 1 : 3;
		const int x = tot.byteAt(r.focus + coords) * 8;
		const int y = tot.byteAt(r.focus + coords + 1);
		const int width = tot.byteAt(r.focus + coords + 2) * 8;
		const int height = tot.byteAt(r.focus + coords + 3);
		if (!width || (!height && r.focusType != 0x1D))
			return native;
		plan.region = Common::Rect(x, y, x + width, y + MAX(8, height));
		plan.region.clip(Common::Rect(0, 0, 320, 200));
		if (plan.region.isEmpty())
			return native;
		if (r.sceneAnimation) {
			plan.animation = true;
			plan.milliseconds = mewiloAnimationDelayMillis(animationFrameMillis, operand);
			plan.minimumDelay = plan.milliseconds;
			plan.minimumExposure = plan.milliseconds;
			plan.calibrated = false;
			if (r.exposureFocus) {
				const uint16 focus = r.exposureFocus;
				const int ix = tot.byteAt(focus + 3) * 8, iy = tot.byteAt(focus + 4);
				plan.exposureRegion = Common::Rect(ix, iy,
					ix + tot.byteAt(focus + 5) * 8, iy + tot.byteAt(focus + 6));
				plan.exposureRegion.clip(Common::Rect(0, 0, 320, 200));
				if (plan.exposureRegion.isEmpty())
					return native;
				if (!readabilityHolds) {
					plan.region = plan.exposureRegion;
					plan.exposureRegion = Common::Rect();
				}
			}
		}
		if (readabilityHolds && r.focusType == 0x1D) {
			plan.reading = true;
			plan.anchor = kMewiloVisibleHold;
			plan.textNode = r.focus;
			plan.milliseconds = MAX<uint32>(plan.milliseconds,
				mewiloReadingMillis(tot.dataAt(r.focus + 10), tot.size() - r.focus - 10));
		}
		return plan;
	}
	return native;
}

MewiloPresentation::MewiloPresentation() {
	reset();
}

void MewiloPresentation::reset() {
	memset(_tiles, 0, sizeof(_tiles));
	memset(_pixels, 0, sizeof(_pixels));
	_dirty = Common::Rect();
	_frame = 0;
}

void MewiloPresentation::dirty(const Common::Rect &region) {
	Common::Rect clipped(region);
	clipped.clip(Common::Rect(0, 0, 320, 200));
	if (clipped.isEmpty())
		return;
	if (_dirty.isEmpty())
		_dirty = clipped;
	else
		_dirty.extend(clipped);
}

bool MewiloPresentation::submitted(
	const byte *pixels, uint pitch, uint32 activeTime, Common::Rect &changed) {
	changed = Common::Rect();
	if (!pixels || pitch < 320 || _dirty.isEmpty())
		return false;
	for (int ty = _dirty.top / 8; ty <= (_dirty.bottom - 1) / 8; ++ty) {
		for (int tx = _dirty.left / 8; tx <= (_dirty.right - 1) / 8; ++tx) {
			Tile &tile = _tiles[ty * kColumns + tx];
			bool different = !tile.known;
			for (int y = ty * 8; y < ty * 8 + 8; ++y) {
				const byte *row = pixels + y * pitch + tx * 8;
				byte *previous = _pixels + y * 320 + tx * 8;
				if (memcmp(previous, row, 8))
					different = true;
				memcpy(previous, row, 8);
			}
			if (different) {
				tile.since = activeTime;
				tile.known = true;
				const Common::Rect r(tx * 8, ty * 8, tx * 8 + 8, ty * 8 + 8);
				if (changed.isEmpty())
					changed = r;
				else
					changed.extend(r);
			}
		}
	}
	_dirty = Common::Rect();
	if (changed.isEmpty())
		return false;
	++_frame;
	return true;
}

uint32 MewiloPresentation::visibleAge(const Common::Rect &region, uint32 activeTime) const {
	Common::Rect r(region);
	r.clip(Common::Rect(0, 0, 320, 200));
	if (r.isEmpty())
		return 0;
	uint32 age = 0xFFFFFFFFU;
	for (int y = r.top / 8; y <= (r.bottom - 1) / 8; ++y)
		for (int x = r.left / 8; x <= (r.right - 1) / 8; ++x) {
			const Tile &tile = _tiles[y * kColumns + x];
			if (!tile.known)
				return 0;
			age = MIN(age, activeTime - tile.since); // wrap-safe active-time age
		}
	return age;
}

uint32 MewiloPresentation::waitRemaining(
	const MewiloTimingPlan &plan, uint speed, uint32 waitStart, uint32 activeTime) const {
	const uint32 age = plan.anchor == kMewiloVisibleHold ? visibleAge(plan.region, activeTime)
		: activeTime - waitStart;
	const uint32 budgetLeft = remaining(mewiloScaleTime(plan.milliseconds, speed), age);
	// These constraints run concurrently, never as two sequential sleeps. Even
	// an over-budget animation step gets a bounded opportunity to be displayed.
	const uint32 exposureLeft = plan.minimumExposure
		? remaining(mewiloScaleTime(plan.minimumExposure, speed),
			  visibleAge(
				  plan.exposureRegion.isEmpty() ? plan.region : plan.exposureRegion, activeTime))
		: 0;
	const uint32 delayLeft = remaining(
		mewiloScaleTime(plan.minimumDelay, speed), activeTime - waitStart);
	return MAX(MAX(budgetLeft, exposureLeft), delayLeft);
}

uint32 MewiloPresentation::remaining(uint32 budget, uint32 elapsed) {
	return elapsed >= budget ? 0 : budget - elapsed;
}

} // End of namespace Mewilo
} // End of namespace Gob
