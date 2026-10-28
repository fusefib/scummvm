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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/util.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "gob/pregob/mewilo/platform.h"

namespace Gob {
namespace Mewilo {

// CPC stores four source pixels per cell, then doubles them for presentation.
// The shared script coordinate space remains 40 cells wide on every platform.
static const MewiloPlatformProfile kProfiles[] = {
	{ kMewiloPlatformAtariST, "LOAD", 8, 1, true, true, kMewiloAudioAtariSpl },
	{ kMewiloPlatformAmstradCPC, "AMSOAD", 4, 2, false, false, kMewiloAudioCpcMus },
	{ kMewiloPlatformAmiga, "LOAD", 8, 1, false, false, kMewiloAudioAmigaSmp },
	{ kMewiloPlatformThomsonTO, "THLOAD", 8, 1, false, false, kMewiloAudioThomsonNote }
};

bool isValidMewiloPlatform(MewiloPlatform platform) {
	return (uint)platform < ARRAYSIZE(kProfiles);
}

MewiloPlatform mewiloPlatformFromCommon(Common::Platform platform, bool thomsonTO) {
	if (thomsonTO)
		return kMewiloPlatformThomsonTO;
	if (platform == Common::kPlatformAmstradCPC)
		return kMewiloPlatformAmstradCPC;
	if (platform == Common::kPlatformAmiga)
		return kMewiloPlatformAmiga;
	return kMewiloPlatformAtariST;
}

const MewiloPlatformProfile &getMewiloPlatformProfile(MewiloPlatform platform) {
	assert(isValidMewiloPlatform(platform));
	return kProfiles[platform];
}

void buildMewiloPalette(MewiloPlatform platform, byte palette[16 * 3]) {
	if (platform == kMewiloPlatformAmstradCPC) {
		// LOADER.COM programs firmware inks; colour numbers encode B+3R+9G.
		static const byte kInks[16] = { 0, 6, 18, 24, 2, 7, 11, 26, 13, 3, 19, 25, 1, 9, 15, 25 };
		static const byte kLevels[3] = { 0, 128, 255 };
		for (uint i = 0; i < 16; ++i) {
			palette[i * 3] = kLevels[(kInks[i] / 3) % 3];
			palette[i * 3 + 1] = kLevels[(kInks[i] / 9) % 3];
			palette[i * 3 + 2] = kLevels[kInks[i] % 3];
		}
		return;
	}
	if (platform == kMewiloPlatformThomsonTO) {
		// Native Thomson launch palette recovered from protected AUTO.BAT. The
		// launcher executes RESTORE 10000 and PALETTE I,ZZ for I=0..15 before
		// entering PARTIE4.BIN. Thomson BASIC encodes these 12-bit colours as
		// 0xBGR (low nibble red, middle green, high blue).
		static const uint16 kThomsonNativePalette[16] = {
			0x000, 0x00F, 0x0F0, 0x0FF,
			0xF00, 0x62F, 0xFF0, 0xFFF,
			0x777, 0x116, 0x060, 0x36C,
			0xA33, 0x030, 0x02B, 0x08C
		};
		for (uint i = 0; i < 16; ++i) {
			const uint16 color = kThomsonNativePalette[i];
			const byte red = color & 0x0F;
			const byte green = (color >> 4) & 0x0F;
			const byte blue = (color >> 8) & 0x0F;
			palette[i * 3 + 0] = red * 17;
			palette[i * 3 + 1] = green * 17;
			palette[i * 3 + 2] = blue * 17;
		}
		return;
	}

	static const uint16 kColors[16] = {
		0x000, 0x600, 0x260, 0x670, 0x027, 0x724, 0x076, 0x777,
		0x444, 0x511, 0x140, 0x753, 0x335, 0x030, 0x730, 0x541
	};
	for (uint i = 0; i < 16; ++i) {
		if (platform == kMewiloPlatformAmiga) {
			for (uint c = 0; c < 3; ++c)
				palette[i * 3 + c] = ((kColors[i] >> (8 - c * 4)) & 7) * 2 * 17;
			continue;
		}
		palette[i * 3] = (((kColors[i] >> 8) & 7) * 255 + 3) / 7;
		palette[i * 3 + 1] = (((kColors[i] >> 4) & 7) * 255 + 3) / 7;
		palette[i * 3 + 2] = ((kColors[i] & 7) * 255 + 3) / 7;
	}
}

void buildMewiloDisplayPalette(MewiloPlatform platform, byte palette[16 * 3]) {
	buildMewiloPalette(platform, palette);
	if (platform != kMewiloPlatformThomsonTO)
		return;

	static const byte kThomsonDisplayLevels[16] = {
		0, 96, 124, 143, 159, 172, 183, 194, 203, 212, 220, 228, 235, 242, 248, 255
	};
	for (uint i = 0; i < 16 * 3; ++i)
		palette[i] = kThomsonDisplayLevels[palette[i] / 17];
}

Common::SeekableReadStream *normalizeMewiloCpcResource(Common::SeekableReadStream *stream) {
	if (!stream)
		return nullptr;

	const int64 streamSize = stream->size();
	if (streamSize <= 0 || streamSize > 0x7FFFFFFF)
		return stream;

	byte *raw = (byte *)malloc((uint32)streamSize);
	if (!raw)
		return stream;
	stream->seek(0);
	const uint32 got = stream->read(raw, (uint32)streamSize);
	delete stream;
	if (got != (uint32)streamSize) {
		free(raw);
		return nullptr;
	}

	uint32 payloadOffset = 0;
	uint32 payloadSize = (uint32)streamSize;
	if (streamSize >= 128) {
		uint16 checksum = 0;
		for (uint i = 0; i < 67; ++i)
			checksum += raw[i];
		const uint16 storedChecksum = READ_LE_UINT16(raw + 67);
		const uint32 realLength = (uint32)raw[64] | ((uint32)raw[65] << 8) | ((uint32)raw[66] << 16);

		// A nonzero real length avoids treating the 128 zero bytes before the
		// known AMSOAD1.I00 stream as a checksum-valid AMSDOS header. Do not
		// require the embedded 8.3 name: the shipped AMSOAD7.TOT header names
		// itself AMSOAD5.TOT despite having a valid checksum and payload length.
		if (checksum == storedChecksum && realLength && realLength <= (uint32)streamSize - 128) {
			payloadOffset = 128;
			payloadSize = realLength;
		}
	}

	if (!payloadOffset)
		return new Common::MemoryReadStream(raw, payloadSize, DisposeAfterUse::YES);

	byte *payload = (byte *)malloc(payloadSize);
	if (!payload) {
		free(raw);
		return nullptr;
	}
	memcpy(payload, raw + payloadOffset, payloadSize);
	free(raw);
	return new Common::MemoryReadStream(payload, payloadSize, DisposeAfterUse::YES);
}

uint32 mewiloNativeDelayMillis(MewiloPlatform platform, byte operand, bool &calibrated) {
	calibrated = true;
	if (!operand)
		return 0; // FF03(0) is still an explicit input continuation.
	switch (platform) {
	case kMewiloPlatformAmiga:
		// Verified Hunk $2378 -> multiply by 6; $050A -> divide by 40;
		// $6882 -> dos.library Delay (50 ticks/sec). Integer truncation matters.
		return ((uint32)operand * 6 / 40) * 20;
	case kMewiloPlatformAmstradCPC:
		// LOADER.COM $481F multiplies the positive FF03 operand by 10,
		// then $4826 calls Turbo Pascal Delay ($021D), in nominal ms.
		// Preserve the requested interval, not Z80/Gate Array contention.
		return (uint32)operand * 10;
	case kMewiloPlatformThomsonTO:
		// Canonical PARTIE4 $8C9B: 150 iterations of LEAX -1,X / BNE,
		// plus PSHS A / LDX / PULS A / DECA / BNE = 1220 E cycles/unit.
		// 1 MHz E clock; exclude interrupts and the resource-read trampoline.
		return ((uint32)operand * 1220 + 999) / 1000;
	default:
		// The Atari busy loop is recovered, but its wall-clock rate is unproved.
		// Preserve the explicit compatibility estimate, never borrow audio units.
		calibrated = false;
		return operand;
	}
}

static const char *const kModuleSuffixes[9] = { "", "0", "1", "2", "3", "4", "5", "7", "8" };

int mewiloModuleIndex(const Common::String &stem, const char *prefix) {
	for (uint i = 0; i < ARRAYSIZE(kModuleSuffixes); ++i) {
		if (stem.equalsIgnoreCase(Common::String(prefix) + kModuleSuffixes[i]))
			return i;
	}
	return -1;
}

Common::String mewiloModuleStem(uint index, const char *prefix) {
	return index < ARRAYSIZE(kModuleSuffixes) ? Common::String(prefix) + kModuleSuffixes[index] : Common::String();
}

} // End of namespace Mewilo
} // End of namespace Gob
