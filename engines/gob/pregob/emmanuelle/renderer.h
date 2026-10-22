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

#ifndef GOB_PREGOB_EMMANUELLE_RENDERER_H
#define GOB_PREGOB_EMMANUELLE_RENDERER_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "common/stream.h"

#include "gob/surface.h"

#include "gob/pregob/emmanuelle/oldixx.h"
#include "gob/pregob/emmanuelle/oldtot.h"
#include "gob/pregob/emmanuelle/platform.h"
#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {

class Font;
class GobEngine;

namespace Emmanuelle {

struct EmmanuelleRendererState;

struct OldTotPreparedSavedRegion {
	OldTotPreparedSavedRegion() : x(0), y(0) {}
	OldTotPreparedSavedRegion(const SurfacePtr &savedSurface,
		int16 savedX, int16 savedY) : surface(savedSurface),
		x(savedX), y(savedY) {}

	SurfacePtr surface;
	int16 x;
	int16 y;
};

struct OldTotRendererPreparedState {
	OldTotRendererPreparedState();

	SurfacePtr framebuffer;
	Common::Array<OldTotPreparedSavedRegion> savedRegions;
	uint16 atariPalette[16];
	bool atariPaletteValid;
	bool atariBlack;
	byte atariPresentationMode;
};

bool prepareOldTotRendererState(const EmmanuelleRendererState &in,
	uint16 width, uint16 height, byte bytesPerPixel,
	byte atariPresentationMode, OldTotRendererPreparedState &out);

/** Adapter from old-TOT drawing operations to Gob surfaces and fonts. */
class OldTotRenderer {
public:
	explicit OldTotRenderer(GobEngine *vm);

	void setResourceBanks(const OldIxxBank *localBank, const OldIxxBank *sharedBank);
	void setPlatform(OldTotPlatformTag platform) {
		_platform = platform;
		_presentation = PresentationState(platform);
	}
	bool applyDosPalette(const Common::String &moduleName, byte videoMode);
	bool presentDosFrame(const Common::String &moduleName, byte videoMode);
	bool atariModulePaletteDiffers(const Common::String &moduleName) const;
	bool applyAtariModulePalette(const Common::String &moduleName);
	bool selectAtariPalette(byte paletteNumber, bool makeVisible);
	bool applyAtariPalette(byte paletteNumber);
	bool applyAtariEffectWords(const uint16 words[4]);
	bool rotateAtariEffectWords();
	bool rotateAtariColorCycleWords();
	/** Apply one of eight native palette phases without owning event pacing. */
	bool presentAtariFadeToBlackStep(byte phase);
	bool presentAtariFadeFromBlackStep(byte phase);
	bool restoreAtariPaletteImmediately();
	bool atariPaletteValid() const { return _atariPaletteValid; }
	bool atariBlack() const { return _atariBlack; }
	void armAtariTransition(byte mode);
	void disarmAtariTransition();
	void resetAtariModuleState();
	void notePresentationDirtyRect(int16 left, int16 top, int16 right,
		int16 bottom);
	void captureAtariPresentedFrame();
	bool nextPresentationStep(bool black, byte randomVariant,
		PresentationStep &step);
	bool beginAtariTransitionFrame();
	bool presentAtariMode43Step(byte phase);
	bool presentAtariMode44Step(byte variant, byte phase);
	byte presentationMode() const { return _presentation.mode(); }
	byte presentationVariant() const {
		return _presentation.variant();
	}
	byte presentationPhase() const { return _presentation.phase(); }
	bool presentationPending() const {
		return _presentation.pending();
	}
	bool presentationActive() const {
		return _presentation.active();
	}
	bool presentationQuiescent() const {
		return !_presentation.active() &&
			!_presentation.pending();
	}


	bool saveScreenRect(uint16 x, uint16 y, uint16 width, uint16 height);
	bool restoreScreenRect();
	void fillScreen(byte color);

	bool drawExternalSprite(const OldTotDrawDescriptor &desc, int16 x, int16 y);
	bool drawSprite(const OldTotData &tot, const OldTotDrawDescriptor &desc,
					int16 x, int16 y);
	SurfacePtr decodeSpriteSurface(const OldTotData &tot,
								   const OldTotDrawDescriptor &desc);
	SurfacePtr buildMaskedCell(const Surface &sprite, uint16 cellWidth,
							   uint16 cellHeight, int16 offsetX, int16 offsetY,
							   bool transparentZero) const;
	SurfacePtr buildFightCell(const Surface &sprite, uint16 cellWidth,
							  uint16 cellHeight, int16 offsetX,
							  int16 offsetY) const;
	bool drawPackedPayload(const Common::Array<byte> &payload, uint16 width,
						   uint16 height, int16 x, int16 y, bool transparentZero,
						   bool horizontalFlip, bool verticalFlip);

	Font *loadLetFont(Common::SeekableReadStream &stream) const;

	uint savedRegionDepth() const { return _savedRegions.size(); }

	bool exportState(EmmanuelleRendererState &out) const;
	bool prepareState(const EmmanuelleRendererState &in,
		byte atariPresentationMode,
		OldTotRendererPreparedState &out) const;
	void commitState(OldTotRendererPreparedState &in);

private:
	bool setAtariWords(const uint16 words[16], bool makeVisible);
	void showAtariWords(const uint16 words[16]);
	SurfacePtr clonePresentationFrame(const Surface &source) const;
	void copyPresentationRect(const Surface &target, int16 left, int16 top,
		int16 right, int16 bottom);
	void finishAtariTransitionFrame();
	void clearDirtyRect();

	GobEngine *_vm;
	OldTotPlatformTag _platform;
	const OldIxxBank *_localBank;
	const OldIxxBank *_sharedBank;
	Common::Array<OldTotPreparedSavedRegion> _savedRegions;
	uint16 _atariLogicalPalette[16];
	bool _atariPaletteValid;
	bool _atariBlack;
	PresentationState _presentation;
	SurfacePtr _atariPresentedSurface;
	SurfacePtr _atariTransitionTarget;
	bool _dirtyValid;
	int16 _dirtyLeft;
	int16 _dirtyTop;
	int16 _dirtyRight;
	int16 _dirtyBottom;
};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_RENDERER_H
