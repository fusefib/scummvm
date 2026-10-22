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
	SurfacePtr amigaPresentedFrame;
	uint16 atariPalette[16];
	uint16 amigaPalette[32];
	bool atariPaletteValid;
	bool atariBlack;
	bool amigaPaletteValid;
	bool amigaBlack;
	uint32 amigaModuleGeneration;
	uint32 amigaPresentedGeneration;
	byte atariPresentationMode;
	byte amigaPresentationMode;
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
	bool amigaModulePaletteDiffers(const Common::String &moduleName) const;
	bool applyAmigaModulePalette(const Common::String &moduleName);
	bool applyAmigaPalette(byte paletteNumber, bool makeVisible = true);
	bool applyAmigaEffect45(const uint16 words[5]);
	bool rotateAmigaEffect46();
	bool rotateAmigaColorCycle();
	/** Apply one of sixteen native palette phases without owning events. */
	bool presentAmigaFadeToBlackStep(byte phase);
	bool presentAmigaFadeFromBlackStep(byte phase);
	bool restoreAmigaPaletteImmediately();
	bool amigaPaletteValid() const { return _amigaPaletteValid; }
	bool amigaBlack() const { return _amigaBlack; }
	bool amigaPaletteTransitionPending() const {
		return _presentation.paletteTransitionPending();
	}

	void armAmigaTransition(byte mode);
	void disarmAmigaTransition();
	void noteAmigaPaletteTransition();
	void beginAmigaModuleState();
	// Module changes discard transient backing stores without restoring them.
	// The displayed snapshot remains the source for a native transition.
	void resetAmigaModuleState();
	void captureAmigaPresentedFrame();
	void commitAmigaDirectFrame();

	bool beginAmigaTransitionFrame();
	bool presentAmigaMode43Step(byte phase);
	bool presentAmigaMode44Step(byte variant, byte phase);

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
	void drawLetText(const Font &font, const Common::String &text, int16 x,
					 int16 y, byte foreground, byte background,
					 bool transparentBackground);

	uint savedRegionDepth() const { return _savedRegions.size(); }
	void discardSavedRegions() { _savedRegions.clear(); }

	bool amigaPresentedSnapshotValid() const {
		return (bool)_amigaPresentedSurface;
	}
	uint32 amigaModuleGeneration() const { return _amigaModuleGeneration; }
	uint32 amigaPresentedGeneration() const {
		return _amigaPresentedGeneration;
	}
	bool exportState(EmmanuelleRendererState &out) const;
	bool prepareState(const EmmanuelleRendererState &in,
		byte atariPresentationMode,
		OldTotRendererPreparedState &out) const;
	void commitState(OldTotRendererPreparedState &in);

private:
	bool setAtariWords(const uint16 words[16], bool makeVisible);
	void showAtariWords(const uint16 words[16]);
	bool setAmigaWords(const uint16 words[32], bool makeVisible);
	void showAmigaWords(const uint16 words[32]);
	bool decodeAmigaPackedPayload(const Common::Array<byte> &payload,
								  uint16 width, uint16 height,
								  Surface &out) const;
	SurfacePtr clonePresentationFrame(const Surface &source) const;
	void copyPresentationRect(const Surface &target, int16 left, int16 top,
		int16 right, int16 bottom);
	void finishAtariTransitionFrame();
	void clearDirtyRect();
	void finishAmigaTransitionFrame();

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
	uint16 _amigaLogicalPalette[32];
	bool _amigaPaletteValid;
	bool _amigaBlack;
	SurfacePtr _amigaPresentedSurface;
	SurfacePtr _amigaTransitionTarget;
	uint32 _amigaModuleGeneration;
	uint32 _amigaPresentedGeneration;
};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_RENDERER_H
