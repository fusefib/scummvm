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

#include "gob/pregob/emmanuelle/oldtot.h"

#include "common/md5.h"
#include "common/memstream.h"

namespace Gob {
namespace Emmanuelle {

OldTotInline::OldTotInline() {
	subOpcode = 0;
	size = 0;
	arg = 0;

	for (uint i = 0; i < ARRAYSIZE(words); ++i)
		words[i] = 0;
}

OldTotScheduledCycle::OldTotScheduledCycle() {
	randomizeInitial = 0;
	period = 0;

	memset(reserved, 0, sizeof(reserved));
}

OldTotAnimationSetup::OldTotAnimationSetup() {
	slot = 0;
	flags = 0;
	frameDelay = 0;
	stepCount = 0;

	deltaX = 0;
	deltaY = 0;

	originX = 0;
	originY = 0;
	widthUnits = 0;
	height = 0;

	frameMax = 0;
	hasDescriptor = false;
	descriptorOffset = 0;
}

bool OldTotAnimationSetup::descriptorForFrame(byte frame, uint16 &offset) const {
	if (frame < 1 || frame > frameDescriptors.size())
		return false;
	offset = frameDescriptors[frame - 1];
	return offset != 0;
}

OldTotData::OldTotData(Common::SeekableReadStream &stream) {
	const int64 streamSize = stream.size();
	if (streamSize <= 0 || streamSize > 0xFFFF)
		return;

	_data.resize((uint32)streamSize);
	stream.seek(0);
	if (stream.read(&_data[0], _data.size()) != _data.size())
		_data.clear();
}

bool OldTotData::isCasinoWheelWait(OldTotPlatformTag platform,
		Common::Language language, uint16 offset) const {
	if (platform != kOldTotPlatformDOS && platform != kOldTotPlatformAtariST &&
		platform != kOldTotPlatformAmiga)
		return false;
	if ((language == Common::ES_ESP && platform == kOldTotPlatformAmiga) ||
		(language == Common::DE_DEU && platform != kOldTotPlatformAmiga))
		return false;
	static const struct {
		Common::Language language;
		uint16 sequence;
		uint16 message;
	} layouts[] = {
		{Common::EN_ANY, 0x4D12, 0x1B6F},
		{Common::ES_ESP, 0x4B8D, 0x1B2A},
		{Common::FR_FRA, 0x4CE6, 0x1BA4},
		{Common::DE_DEU, 0x4D55, 0x1B9E}
	};
	for (uint layout = 0; layout < ARRAYSIZE(layouts); ++layout) {
		if (layouts[layout].language != language)
			continue;
		const uint16 start = layouts[layout].sequence;
		if (!has(start, 80) || offset < start + 4 || offset >= start + 62 ||
			(offset - start - 4) % 5 != 0 ||
			_data[start] != 0x1C || _data[start + 1] != 29)
			return false;
		uint16 frames[3];
		for (uint i = 0; i < 12; ++i) {
			const uint16 record = start + 2 + 5 * i;
			const uint16 frame = uint16BEAt(record);
			if (_data[record + 2] != 0xFF || _data[record + 3] != 3 ||
				_data[record + 4] != 1)
				return false;
			if (i < 3) {
				OldTotDrawDescriptor descriptor;
				if (!decodeDrawDescriptor(frame, descriptor) || descriptor.flags != 0x43 ||
					descriptor.drawFlags != 0 || descriptor.x != 139 || descriptor.y != 153 ||
					descriptor.width != 48 || descriptor.height != 24)
					return false;
				frames[i] = frame;
			} else if (frame != frames[i % 3])
				return false;
		}
		const byte saveBubble[] = {0xFF, 1, 0, 80, 0, 24, 0, 136, 0, 24};
		OldTotDrawDescriptor bubble;
		return frames[0] != frames[1] && frames[0] != frames[2] && frames[1] != frames[2] &&
			memcmp(&_data[start + 62], saveBubble, sizeof(saveBubble)) == 0 &&
			decodeDrawDescriptor(uint16BEAt(start + 72), bubble) &&
			bubble.x == 80 && bubble.y == 24 && bubble.width == 136 && bubble.height == 24 &&
			uint16BEAt(start + 74) == layouts[layout].message &&
			uint16BEAt(start + 76) == 0x094F && uint16BEAt(start + 78) == 0xFF02;
	}
	return false;
}

bool OldTotData::computeMD5(byte digest[16]) const {
	if (!valid())
		return false;
	Common::MemoryReadStream stream(&_data[0], _data.size());
	return Common::computeStreamMD5(stream, digest);
}

bool OldTotData::has(uint32 offset, uint32 length) const {
	return offset <= _data.size() && length <= (_data.size() - offset);
}

byte OldTotData::byteAt(uint32 offset) const {
	return has(offset, 1) ? _data[offset] : 0;
}

uint16 OldTotData::uint16BEAt(uint32 offset) const {
	if (!has(offset, 2))
		return 0;
	return ((uint16)_data[offset] << 8) | _data[offset + 1];
}

bool OldTotData::getRootSequenceOffset(uint16 &offset) const {
	if (!has(2, 2))
		return false;

	const uint16 control = uint16BEAt(2);
	if (control == 0 || !has(control - 1, 2))
		return false;

	offset = control - 1;
	return true;
}

uint16 OldTotData::inlineSize(byte subOpcode) {
	switch (subOpcode) {
	case 0x01:
		return 10;
	case 0x02:
		return 2;
	case 0x03:
		return 3;
	case 0x04:
		return 3;
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x09:
	case 0x0A:
		return 6;
	case 0x0B:
	case 0x0C:
		return 3;
	default:
		return 0;
	}
}

bool OldTotData::decodeInline(uint16 offset, OldTotInline &out) const {
	if (!has(offset, 2) || byteAt(offset) != 0xFF)
		return false;

	out = OldTotInline();
	out.subOpcode = byteAt(offset + 1);
	out.size = inlineSize(out.subOpcode);
	if (out.size == 0 || !has(offset, out.size))
		return false;

	if (out.subOpcode == 0x01) {
		for (uint i = 0; i < 4; ++i)
			out.words[i] = uint16BEAt(offset + 2 + 2 * i);
	} else if (out.subOpcode >= 0x05 && out.subOpcode <= 0x0A) {
		out.words[0] = uint16BEAt(offset + 2);
		out.words[1] = uint16BEAt(offset + 4);
	} else if (out.size == 3) {
		out.arg = byteAt(offset + 2);
	}

	return true;
}

bool OldTotData::decodeScheduledCycle(uint16 offset, OldTotScheduledCycle &out) const {
	if (!has(offset, 11) || byteAt(offset) != 0x2A)
		return false;

	out = OldTotScheduledCycle();
	out.randomizeInitial = byteAt(offset + 1);
	out.period = byteAt(offset + 2);
	for (uint i = 0; i < 7; ++i)
		out.reserved[i] = byteAt(offset + 3 + i);

	const uint count = byteAt(offset + 10);
	if (!has(offset + 11, count * 2))
		return false;

	out.targets.resize(count);
	for (uint i = 0; i < count; ++i)
		out.targets[i] = uint16BEAt(offset + 11 + 2 * i);

	return true;
}

bool OldTotData::decodeAnimationSetup(uint16 offset, OldTotAnimationSetup &out) const {
	if (!has(offset, 16) || byteAt(offset) != 0x06)
		return false;

	out = OldTotAnimationSetup();
	out.slot = byteAt(offset + 1);
	out.flags = byteAt(offset + 2);
	out.frameDelay = byteAt(offset + 3);
	out.stepCount = byteAt(offset + 4);
	out.deltaX = (int8)byteAt(offset + 5);
	out.deltaY = (int8)byteAt(offset + 6);
	out.originX = uint16BEAt(offset + 7);
	out.originY = uint16BEAt(offset + 9);
	out.widthUnits = uint16BEAt(offset + 11);
	out.height = uint16BEAt(offset + 13);
	out.frameMax = byteAt(offset + 15);

	// Animation records index a big-endian descriptor table at byte 16.
	if (out.frameMax) {
		if (!has(offset + 16, (uint32)out.frameMax * 2))
			return false;
		out.frameDescriptors.resize(out.frameMax);
		for (uint i = 0; i < out.frameMax; ++i) {
			const uint16 d = uint16BEAt(offset + 16 + 2 * i);
			if (!d || !has(d, 12))
				return false;
			out.frameDescriptors[i] = d;
		}
		out.hasDescriptor = true;
		out.descriptorOffset = out.frameDescriptors[0];
	}

	return true;
}
bool OldTotData::decodeDrawDescriptor(uint16 offset, OldTotDrawDescriptor &out) const {
	if (!has(offset, 12) || byteAt(offset) <= 0x3F)
		return false;

	out = OldTotDrawDescriptor();
	out.flags = byteAt(offset);
	out.drawFlags = uint16BEAt(offset + 1);
	out.x = uint16BEAt(offset + 3);
	out.y = uint16BEAt(offset + 5);
	out.width = uint16BEAt(offset + 7);
	out.height = uint16BEAt(offset + 9);
	if (out.width == 0 || out.width > 320 || out.height == 0 || out.height > 200)
		return false;

	if (out.external())
		out.resourceIndex = byteAt(offset + 11);
	else if (out.flags & 0x10)
		out.payloadOffset = uint16BEAt(offset + 11);
	else
		out.payloadOffset = offset + 11;

	return out.external() || out.payloadOffset < _data.size();
}

bool OldTotData::decodeExternalDrawDescriptor(uint16 offset, OldTotDrawDescriptor &out) const {
	if (!decodeDrawDescriptor(offset, out))
		return false;
	return out.external();
}

int16 hitTestOldTotInputRegions(const OldTotData &data, uint16 nodeOffset,
								byte count, int16 physicalX, int16 physicalY,
								int16 xBias, int16 yBias) {
	if (uint32(nodeOffset) + 6u + 4u * uint32(count) > data.size())
		return -1;

	const int hitX = physicalX / 8 + xBias;
	const int hitY = physicalY + yBias;
	int16 selected = -1;
	for (byte i = 0; i < count; ++i) {
		const uint16 p = nodeOffset + 6 + 4 * i;
		const int x = data.byteAt(p);
		const int y = data.byteAt(p + 1);
		const int w = data.byteAt(p + 2);
		const int h = data.byteAt(p + 3);
		if (hitX >= x && hitX < x + w && hitY >= y && hitY < y + h)
			selected = i;
	}
	// The native loop retains the last matching region. Some script nodes
	// intentionally overlap, so returning early changes their branch choice.
	return selected;
}

bool OldTotData::getPackedPrefix(uint32 offset, uint32 expectedPixels, Common::Array<byte> &payload, bool amiga5Bit) const {
	payload.clear();
	uint32 pos = offset;
	uint32 pixels = 0;
	const byte shortFlag = amiga5Bit ? 0x04 : 0x08;
	const byte countMask = amiga5Bit ? 0x03 : 0x07;
	while (pixels < expectedPixels) {
		if (!has(pos, 1)) {
			payload.clear();
			return false;
		}
		const byte v = _data[pos++];
		payload.push_back(v);
		uint32 run;
		if (v & shortFlag) {
			run = (v & countMask) + 1;
		} else {
			if (!has(pos, 1)) {
				payload.clear();
				return false;
			}
			const byte lo = _data[pos++];
			payload.push_back(lo);
			run = ((((uint32)v & countMask) << 8) | lo) + 1;
		}
		// Packed rendering is destination-bounded; the final run may cross the
		// nominal pixel boundary.
		const uint32 remain = expectedPixels - pixels;
		pixels += run < remain ? run : remain;
	}
	return pixels == expectedPixels;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
