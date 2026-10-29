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

#include "gob/pregob/mewilo/mewilo.h"

#include "common/textconsole.h"
#include "common/util.h"

#include "gob/draw.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/pregob/mewilo/text.h"
#include "gob/video.h"

namespace Gob {
namespace Mewilo {

void PreGob_Mewilo::initPalette() {
	byte palette[(kCursorColor + 1) * 3];
	buildMewiloDisplayPalette(_presentationPlatform, palette);
	// Gob's palette fade accepts six-bit staging values, not RGB888.
	for (uint i = 0; i < 16 * 3; ++i)
		palette[i] >>= 2;
	// Keep the cursor's hue, using a nearby shade outside the scene's 16 inks.
	for (uint i = 0; i < 3; ++i) {
		const byte component = palette[15 * 3 + i];
		palette[kCursorColor * 3 + i] = component < 60 ? component + 4 : component - 4;
	}
	setPalette(palette, kCursorColor + 1);
	// A 16-color fade does not upload the spare cursor entry.
	_vm->_video->setPalElem(kCursorColor, palette[kCursorColor * 3],
		palette[kCursorColor * 3 + 1], palette[kCursorColor * 3 + 2],
		-1, _vm->_global->_videoMode);
}

void PreGob_Mewilo::markDirty(int16 x, int16 y, int16 width, int16 height) {
	if (width <= 0 || height <= 0)
		return;
	const int16 left = MAX<int16>(0, x);
	const int16 top = MAX<int16>(0, y);
	const int16 right = MIN<int16>(319, x + width - 1);
	const int16 bottom = MIN<int16>(199, y + height - 1);
	if (left <= right && top <= bottom) {
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
		_presentation.dirty(Common::Rect(left, top, right + 1, bottom + 1));
	}
}

void PreGob_Mewilo::drawByteString(
	const byte *text, uint length, int16 x, int16 y, byte color, byte lineAdvance) {
	if (!text)
		return;
	const int16 startX = x;
	for (uint i = 0; i < length; ++i) {
		const byte c = text[i];
		if (!c)
			break;
		if (c == '@') {
			x = startX;
			y += lineAdvance;
			continue;
		}
		_font.drawChar(*_vm->_draw->_backSurface, c, x, y, color,
			_profile.platform == kMewiloPlatformAmstradCPC);
		x += 8;
	}
}

void PreGob_Mewilo::drawTextNode(const MewiloTotData &tot, uint16 nodeOffset) {
	if (!tot.has(nodeOffset, 10) || !_vm->_draw->_backSurface) {
		hostFailure("invalid text header or missing surface", nodeOffset);
		return;
	}
	const int16 left = tot.byteAt(nodeOffset + 1) * 8;
	const int16 top = tot.byteAt(nodeOffset + 2);
	const byte widthCell = tot.byteAt(nodeOffset + 3);
	const int16 width = widthCell * 8;
	const int16 height = tot.byteAt(nodeOffset + 4);
	const byte lineAdvance = tot.byteAt(nodeOffset + 5);
	const byte background = tot.byteAt(nodeOffset + 6);
	const byte normal = tot.byteAt(nodeOffset + 7);
	const byte highlight = tot.byteAt(nodeOffset + 8);

	if (!widthCell)
		return;

	if (width > 0 && height > 0)
		_vm->_draw->_backSurface->fillRect(
			left, top, left + width - 1, top + height - 1, background);

	const uint32 textOffset = nodeOffset + 10;
	if (!tot.has(textOffset))
		return;

	const ReplacementEntry *replacement = replacementEntry(tot, nodeOffset, kReplacementText);
	const byte *text = replacement ? replacement->bytes.data() : tot.dataAt(textOffset);
	const uint32 textSize = replacement ? replacement->bytes.size() : tot.size() - textOffset;
	uint32 sourceOffset = 0;
	int16 y = top;
	int16 dirtyBottom = (height > 0) ? top + height - 1 : top - 1;
	byte color = normal;
	bool strike = false;
	bool finished = false;

	while (!finished && sourceOffset < textSize) {
		MewiloTextLine line;
		if (!extractMewiloTextLine(text + sourceOffset, textSize - sourceOffset, widthCell, line) ||
			!line.consumed) {
			hostFailure("malformed text stream", nodeOffset);
			break;
		}

		int16 x = left;
		for (uint i = 0; i < line.bytes.size(); ++i) {
			const byte c = line.bytes[i];
			switch (c) {
			case '[':
				color = highlight;
				continue;
			case ']':
				color = normal;
				continue;
			case '{':
				strike = true;
				continue;
			case '}':
				strike = false;
				continue;
			default:
				break;
			}

			_font.drawChar(*_vm->_draw->_backSurface, c, x, y, color,
				_profile.platform == kMewiloPlatformAmstradCPC);
			if (strike) {
				const int16 strikeY = y + MAX<int16>(0, ((int16)lineAdvance - 1) / 2);
				if (x >= 0 && x + 7 < 320 && strikeY >= 0 && strikeY < 200)
					_vm->_draw->_backSurface->drawLine(x, strikeY, x + 7, strikeY, color);
			}
			x += 8;
		}

		if (!line.bytes.empty())
			dirtyBottom = MAX<int16>(dirtyBottom, y + 7);

		sourceOffset += line.consumed;
		finished = line.endOfText;
		y += lineAdvance;
	}

	// Native text is not clipped to the serialized background-box height.
	// Include any wrapped glyph rows below the box in the Gob dirty region so
	// presentation matches what was actually written to the back surface.
	if (dirtyBottom >= top) {
		markDirty(left, top, width, dirtyBottom - top + 1);
		tracePresentationDraw(
			"text", nodeOffset, Common::Rect(left, top, left + width, dirtyBottom + 1));
	}
}

void PreGob_Mewilo::drawInlineImage(const MewiloTotData &tot, uint16 nodeOffset) {
	if (!tot.has(nodeOffset, 7) || !_vm->_draw->_backSurface) {
		hostFailure("invalid inline image header or missing surface", nodeOffset);
		return;
	}
	const uint16 recordSize = tot.uint16BEAt(nodeOffset + 1);
	if (recordSize < 7 || !tot.has(nodeOffset, recordSize)) {
		hostFailure("invalid inline image extent", nodeOffset);
		return;
	}
	const int16 x =
		tot.byteAt(nodeOffset + 3) * _profile.packedPixelsPerCell * _profile.packedOutputScaleX;
	const int16 y = tot.byteAt(nodeOffset + 4);
	const uint16 sourceWidth = tot.byteAt(nodeOffset + 5) * _profile.packedPixelsPerCell;
	const uint16 outputWidth = sourceWidth * _profile.packedOutputScaleX;
	const uint16 height = tot.byteAt(nodeOffset + 6);
	const MewiloPackedDecodeResult result =
		drawMewiloPacked(tot.dataAt(nodeOffset + 7), recordSize - 7, *_vm->_draw->_backSurface, x,
			y, sourceWidth, height, _profile.packedOutputScaleX);
	if (!result.complete) {
		hostFailure("incomplete inline image", nodeOffset);
		return;
	}
	// Native decoding (including persistent Atari scratch writes) comes first.
	const ReplacementEntry *replacement = replacementEntry(tot, nodeOffset, kReplacementImage);
	if (replacement && !drawReplacementImage(*replacement, *_vm->_draw->_backSurface, x, y))
		hostFailure("invalid replacement image", nodeOffset);
	markDirty(x, y, outputWidth, height);
	tracePresentationDraw("image", nodeOffset, Common::Rect(x, y, x + outputWidth, y + height));
}

void PreGob_Mewilo::drawExternalImage(const MewiloTotData &tot, uint16 nodeOffset) {
	if (!tot.has(nodeOffset, 8) || !_vm->_draw->_backSurface) {
		hostFailure("invalid external image header or missing surface", nodeOffset);
		return;
	}
	const byte masked = tot.byteAt(nodeOffset) & 0x3F;
	if (masked < 0x20)
		return;
	const byte bank = masked - 0x20;
	if (bank != 0)
		return; // Atari native code deliberately exits for nonzero banks.
	if (!_i00) {
		hostFailure("required I00 bank is missing", nodeOffset);
		return;
	}

	const uint16 descriptorSize = tot.uint16BEAt(nodeOffset + 1);
	const int16 x =
		tot.byteAt(nodeOffset + 3) * _profile.packedPixelsPerCell * _profile.packedOutputScaleX;
	const int16 y = tot.byteAt(nodeOffset + 4);
	const uint16 sourceWidth = tot.byteAt(nodeOffset + 5) * _profile.packedPixelsPerCell;
	const uint16 outputWidth = sourceWidth * _profile.packedOutputScaleX;
	const uint16 height = tot.byteAt(nodeOffset + 6);
	const byte recordIndex = tot.byteAt(nodeOffset + 7);

	uint16 framedSize = 0;
	const byte *payload = nullptr;
	uint32 payloadSize = 0;
	if (!_i00->getRecord(recordIndex, framedSize, payload, payloadSize)) {
		hostFailure("required I00 record is missing", nodeOffset);
		return;
	}
	if (descriptorSize != framedSize)
		warning("Mewilo: I00 size mismatch at 0x%04X (%u != %u)", nodeOffset, descriptorSize,
			framedSize);
	debugC(4, kDebugGraphics, "Mewilo: draw %s.I00[%u] at %d,%d (%ux%u, %u payload bytes)",
		_moduleStem.c_str(), recordIndex, x, y, outputWidth, height, payloadSize);

	const byte *decodePayload = payload;
	uint32 decodeSize = payloadSize;
	if (_profile.persistentI00Scratch) {
		if (payloadSize > _i00Scratch.size()) {
			hostFailure("I00 record exceeds native scratch capacity", nodeOffset);
			return;
		}
		// Atari native code overwrites only the freshly loaded prefix and leaves
		// the remainder of its reusable scratch buffer intact. Keep that behavior
		// for the French one-pixel-short compatibility case. CPC streams decode
		// exactly and use the record payload directly.
		if (payloadSize)
			memcpy(_i00Scratch.data(), payload, payloadSize);
		decodePayload = _i00Scratch.data();
		decodeSize = _i00Scratch.size();
	}
	const MewiloPackedDecodeResult result = drawMewiloPacked(decodePayload, decodeSize,
		*_vm->_draw->_backSurface, x, y, sourceWidth, height, _profile.packedOutputScaleX);
	if (!result.complete) {
		hostFailure("incomplete external image", nodeOffset);
		return;
	}
	// Native decoding (including persistent Atari scratch writes) comes first.
	const ReplacementEntry *replacement = replacementEntry(tot, nodeOffset, kReplacementImage);
	if (replacement && !drawReplacementImage(*replacement, *_vm->_draw->_backSurface, x, y))
		hostFailure("invalid replacement image", nodeOffset);
	markDirty(x, y, outputWidth, height);
	tracePresentationDraw("image", nodeOffset, Common::Rect(x, y, x + outputWidth, y + height));
}

void PreGob_Mewilo::saveRect(byte xCell, byte y, byte widthCell, byte height) {
	const int16 x = xCell * 8;
	const int16 width = widthCell * 8;
	if (width <= 0 || height <= 0)
		return;
	if (!_vm->_draw->_backSurface) {
		hostFailure("missing capture surface", 0);
		return;
	}
	SavedRect saved;
	saved.x = x;
	saved.y = y;
	saved.surface.reset(new Surface(width, height, _vm->_draw->_backSurface->getBPP()));
	// Clipped rows and columns are never read from the screen. Keep their
	// stored bytes deterministic rather than retaining allocation contents.
	saved.surface->clear();
	saved.surface->blit(*_vm->_draw->_backSurface, x, y, x + width - 1, y + height - 1, 0, 0);
	_savedRects.push_back(saved);
}

void PreGob_Mewilo::restoreRect() {
	if (_savedRects.empty())
		return;
	if (!_vm->_draw->_backSurface) {
		hostFailure("missing restore surface", 0);
		return;
	}
	SavedRect saved = _savedRects.back();
	_vm->_draw->_backSurface->blit(*saved.surface, saved.x, saved.y);
	_savedRects.pop_back();
	markDirty(saved.x, saved.y, saved.surface->getWidth(), saved.surface->getHeight());
}

void PreGob_Mewilo::fillScreen(byte color) {
	if (!_vm->_draw->_backSurface) {
		hostFailure("missing fill surface", 0);
		return;
	}
	_vm->_draw->_backSurface->fill(color);
	markDirty(0, 0, 320, 200);
}

void PreGob_Mewilo::drawScore(uint value, bool ending) {
	if (!ending) {
		drawScoreDigits(value, 37 * 8, 184, 0, 2);
		return;
	}
	// Amiga keeps pen 1 for the first ending counter; all others use pen 0.
	const byte firstPen = _profile.platform == kMewiloPlatformAmiga ? 1 : 0;
	drawScoreDigits(value, 28 * 8, 56, firstPen, 3);
	if (!hasError())
		drawScoreDigits(value, 19 * 8, 164, 0, 2);
}

void PreGob_Mewilo::drawScoreDigits(uint value, int16 x, int16 y, byte color, byte background) {
	if (!_vm->_draw->_backSurface) {
		hostFailure("missing score surface", 0);
		return;
	}
	_vm->_draw->_backSurface->fillRect(x, y, x + 15, y + 7, background);
	value = MIN<uint>(99, value);
	const byte digits[2] = {(byte)('0' + value / 10), (byte)('0' + value % 10)};
	drawByteString(digits, 2, x, y, color, 8);
	markDirty(x, y, 16, 8);
}

void PreGob_Mewilo::installCrossCursor() {
	if (!_crossCursor) {
		_crossCursor.reset(new Surface(16, 16, 1));
		_crossCursor->clear();
		_crossCursor->drawLine(7, 0, 7, 15, kCursorColor);
		_crossCursor->drawLine(0, 7, 15, 7, kCursorColor);
		_crossCursor->drawRect(5, 5, 9, 9, kCursorColor);
	}
	setCursor(*_crossCursor, 7, 7);
}

void PreGob_Mewilo::tracePresentationDraw(
	const char *kind, uint16 site, const Common::Rect &region) {
	debugC(4, kDebugGameFlow,
		"MEWILO_PRESENT draw module=%s site=%04X kind=%s t=%u frame=%u rect=%d,%d,%d,%d",
		_moduleStem.c_str(), site, kind, activeMillis(), _presentation.frame(), region.left,
		region.top, region.right, region.bottom);
}

void PreGob_Mewilo::presentFrame() {
	if (shouldQuit() || hasError())
		return;
	const SurfacePtr &surface = _vm->_draw->_backSurface;
	if (!surface || surface->getWidth() != 320 || surface->getHeight() != 200 ||
		surface->getBPP() != 1) {
		hostFailure("invalid presentation surface", 0);
		return;
	}
	_vm->_draw->blitInvalidated();
	_vm->_video->retrace();
	if (_presentation.pending()) {
		Common::Rect changed;
		const bool different =
			_presentation.submitted(surface->getData(), 320, activeMillis(), changed);
		debugC(4, kDebugGameFlow,
			"MEWILO_PRESENT submit module=%s t=%u frame=%u changed=%u rect=%d,%d,%d,%d",
			_moduleStem.c_str(), activeMillis(), _presentation.frame(), different, changed.left,
			changed.top, changed.right, changed.bottom);
	}
}

} // End of namespace Mewilo
} // End of namespace Gob
