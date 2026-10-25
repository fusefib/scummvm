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

#ifndef GOB_PREGOB_EMMANUELLE_OLDTOT_H
#define GOB_PREGOB_EMMANUELLE_OLDTOT_H

#include "common/array.h"
#include "common/language.h"
#include "common/scummsys.h"
#include "common/stream.h"

#include "gob/pregob/emmanuelle/platform.h"

namespace Gob {
namespace Emmanuelle {

struct OldTotInline {
	OldTotInline();

	byte subOpcode;
	uint16 size;
	uint16 words[4];
	byte arg;
};

struct OldTotScheduledCycle {
	OldTotScheduledCycle();

	byte randomizeInitial;
	byte period;
	byte reserved[7];
	Common::Array<uint16> targets;
};

struct OldTotDrawDescriptor {
	OldTotDrawDescriptor() {
		flags = 0;
		drawFlags = 0;

		x = 0;
		y = 0;
		width = 0;
		height = 0;

		resourceIndex = 0;
		payloadOffset = 0;
	}

	byte flags;
	uint16 drawFlags;
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
	byte resourceIndex;
	uint16 payloadOffset; ///< inline node+11 or BE16-linked packed stream; 0 for external

	bool external() const { return (flags & 0x20) != 0; }
	bool linkedOrInline() const { return (flags & 0x10) != 0; }
	byte bankSelector() const { return (flags & 0x0C) >> 2; }
	bool transparentZero() const { return (flags & 0x02) != 0; }
	bool horizontalFlip() const { return (drawFlags & 0x1000) != 0; }
	bool verticalFlip() const { return (drawFlags & 0x2000) != 0; }
	bool xIndirect() const { return (drawFlags & 0x8000) != 0; }
	bool yIndirect() const { return (drawFlags & 0x4000) != 0; }
};

enum OldTotAnimationFlags {
	kAnimRegister = 0x01,
	kAnimInitialDraw = 0x02,
	kAnimClear = 0x04,
	kAnimUnregister = 0x08,
	kAnimVariableFrame = 0x10,
	kAnimStepped = 0x20,
	kAnimLoopInput = 0x40,
	kAnimModifier80 = 0x80
};

struct OldTotAnimationSetup {
	OldTotAnimationSetup();

	byte slot;
	byte flags;
	byte frameDelay; ///< Multiplied by 10 on the delay path.
	byte stepCount;
	int8 deltaX;
	int8 deltaY;
	uint16 originX;
	uint16 originY;
	uint16 widthUnits;
	uint16 height;
	byte frameMax;
	bool hasDescriptor;
	uint16 descriptorOffset; ///< Alias for the first frame descriptor.
	Common::Array<uint16> frameDescriptors;
	bool descriptorForFrame(byte frame, uint16 &offset) const;
};

/** Reader for the big-endian, pointer-graph TOT format used by Emmanuelle.
 *
 * This is deliberately separate from Gob::TOTFile.  Emmanuelle predates the
 * later 128-byte x.y TOT header and its script word endianness is different.
 */
class OldTotData {
public:
	explicit OldTotData(Common::SeekableReadStream &stream);

	bool valid() const { return !_data.empty(); }
	uint32 size() const { return _data.size(); }
	bool computeMD5(byte digest[16]) const;
	/** Apply known corrections to owned bytes; return whether anything changed. */
	bool applyScriptPatches(OldTotPlatformTag platform, Common::Language language,
			const Common::String &resourceName);

	byte byteAt(uint32 offset) const;
	uint16 uint16BEAt(uint32 offset) const;

	/** Return the counted root sequence used by the native loaders. */
	bool getRootSequenceOffset(uint16 &offset) const;

	/** Decode an inline FF primitive.  Returns false if framing is invalid. */
	bool decodeInline(uint16 offset, OldTotInline &out) const;
	/** Recognize the twelve direct-draw waits in LOADCAS's roulette spin. */
	bool isCasinoWheelWait(OldTotPlatformTag platform, Common::Language language,
		uint16 offset) const;

	/** Decode opcode 0x2A's timer callback table. */
	bool decodeScheduledCycle(uint16 offset, OldTotScheduledCycle &out) const;

	/** Decode the 18-byte opcode 0x06 animation setup. */
	bool decodeAnimationSetup(uint16 offset, OldTotAnimationSetup &out) const;
	bool decodeDrawDescriptor(uint16 offset, OldTotDrawDescriptor &out) const;
	bool decodeExternalDrawDescriptor(uint16 offset, OldTotDrawDescriptor &out) const;
	/** Copy one unframed packed-RLE sprite, delimited by decoded pixel count. */
	bool getPackedPrefix(uint32 offset, uint32 expectedPixels, Common::Array<byte> &payload, bool amiga5Bit = false) const;

private:
	bool has(uint32 offset, uint32 length) const;
	bool patchIguacuAirport(Common::Language language);
	bool patchCasinoPrediction();
	bool patchCasinoEntryPause(OldTotPlatformTag platform, Common::Language language);
	bool patchRouletteTwenty(Common::Language language);
	bool patchMeterBand();
	bool patchCasinoReturn(Common::Language language);
	bool patchPartyHotspots(Common::Language language);
	bool patchHotelRoomState(Common::Language language);
	bool patchHotelHelicopter(Common::Language language);
	bool patchVendorAffordability(Common::Language language);
	bool patchLobbyGuestReset(OldTotPlatformTag platform, Common::Language language);
	bool patchAirportFallback(OldTotPlatformTag platform, Common::Language language);
	static uint16 inlineSize(byte subOpcode);

	Common::Array<byte> _data;
};

/** Hit-test an opcode 0x1B region table using a native cursor transform.
 * Region X/width bytes are 8-pixel units; Y/height bytes are pixels. The
 * xBias and yBias are applied after the X conversion. Bounds are lower
 * inclusive and upper exclusive. Returns -1 when no region contains the
 * transformed point.
 */
int16 hitTestOldTotInputRegions(const OldTotData &data, uint16 nodeOffset,
								byte count, int16 physicalX, int16 physicalY,
								int16 xBias, int16 yBias);

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_OLDTOT_H
