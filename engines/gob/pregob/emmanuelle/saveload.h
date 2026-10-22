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

#ifndef GOB_PREGOB_EMMANUELLE_SAVELOAD_H
#define GOB_PREGOB_EMMANUELLE_SAVELOAD_H

#include "common/array.h"
#include "common/language.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "common/stream.h"

#include "gob/pregob/emmanuelle/executor.h"
#include "gob/pregob/emmanuelle/gameflow.h"
#include "gob/pregob/emmanuelle/interpreter.h"
#include "gob/pregob/emmanuelle/platform.h"
#include "gob/surface.h"

namespace Gob {
namespace Emmanuelle {

enum EmmanuelleRuntimeProfile : byte {
	kEmmanuelleProfileInvalid = 0,
	kEmmanuelleProfileDosEnglish = 1,
	kEmmanuelleProfileAtariEnglish = 4,
	kEmmanuelleProfileAtariSpanish = 5,
	kEmmanuelleProfileAmigaEnglish = 7
};

EmmanuelleRuntimeProfile emmanuelleRuntimeProfile(
	OldTotPlatformTag platform, Common::Language language);
bool emmanuelleProfileMatchesPlatform(EmmanuelleRuntimeProfile profile,
	OldTotPlatformTag platform);

enum EmmanuelleSaveBoundaryBlocker {
	kEmmanuelleSaveBoundaryReady = 0,
	kEmmanuelleSaveBoundaryNoRuntime,
	kEmmanuelleSaveBoundaryNoInput,
	kEmmanuelleSaveBoundaryPresentation,
	kEmmanuelleSaveBoundaryAudio,
	kEmmanuelleSaveBoundaryFight,
	kEmmanuelleSaveBoundaryRegions
};

const char *emmanuelleSaveBoundaryBlockerName(
	EmmanuelleSaveBoundaryBlocker blocker);

struct EmmanuelleSaveBoundaryState {
	EmmanuelleSaveBoundaryState() {
		activeInputNode = 0;
		runtimeReady = false;
		activeInput = false;
		suspendedWait = false;

		inInit = false;
		paletteReady = false;
		audioIdle = false;
		regionsCoherent = false;
		fightIdle = false;
	}

	bool stableChoice() const {
		return runtimeReady && activeInput && paletteReady && audioIdle &&
			regionsCoherent && fightIdle;
	}

	// Loading replaces the executor, so its input callback must have returned.
	bool canLoad() const { return stableChoice() && suspendedWait; }
	bool canSave() const { return stableChoice() && !inInit; }

	bool canHandoffMenu() const {
		return stableChoice() && !suspendedWait;
	}

	bool canOpenMenu() const {
		return canLoad();
	}

	bool normalizesExecutor() const {
		return canSave() && !suspendedWait;
	}

	EmmanuelleSaveBoundaryBlocker blocker() const {
		if (!runtimeReady)
			return kEmmanuelleSaveBoundaryNoRuntime;
		if (!activeInput)
			return kEmmanuelleSaveBoundaryNoInput;
		if (!paletteReady)
			return kEmmanuelleSaveBoundaryPresentation;
		if (!audioIdle)
			return kEmmanuelleSaveBoundaryAudio;
		if (!fightIdle)
			return kEmmanuelleSaveBoundaryFight;
		if (!regionsCoherent)
			return kEmmanuelleSaveBoundaryRegions;
		return kEmmanuelleSaveBoundaryReady;
	}

	uint16 activeInputNode;
	bool runtimeReady;
	bool activeInput;
	bool suspendedWait;
	bool inInit;
	bool paletteReady;
	bool audioIdle;
	bool regionsCoherent;
	bool fightIdle;
};

struct EmmanuelleSurfaceState {
	EmmanuelleSurfaceState() : width(0), height(0), bytesPerPixel(0) {}

	uint16 width;
	uint16 height;
	byte bytesPerPixel;
	Common::Array<byte> pixels;
};

void exportOldTotSurface(const Surface &surface, EmmanuelleSurfaceState &out);
bool importOldTotSurface(const EmmanuelleSurfaceState &in, SurfacePtr &out);

struct EmmanuelleSavedRegionState {
	EmmanuelleSavedRegionState() : x(0), y(0) {}

	int16 x;
	int16 y;
	EmmanuelleSurfaceState surface;
};

struct EmmanuelleActiveObjectState {
	EmmanuelleActiveObjectState() {
		active = false;

		x = 0;
		y = 0;
		width = 0;
		height = 0;
	}

	bool active;
	int16 x;
	int16 y;
	int16 width;
	int16 height;
	EmmanuelleSurfaceState background;
};

struct EmmanuelleRendererState {
	EmmanuelleRendererState();

	EmmanuelleSurfaceState framebuffer;
	Common::Array<EmmanuelleSavedRegionState> savedRegions;
	Common::Array<EmmanuelleActiveObjectState> activeObjects;
	uint16 atariPalette[16];
	uint16 amigaPalette[32];
	bool atariPaletteValid;
	bool atariBlack;
	bool amigaPaletteValid;
	bool amigaBlack;
	uint32 amigaModuleGeneration;
	uint32 amigaPresentedGeneration;
	byte amigaPresentationMode;
	byte amigaPresentationVariant;
	byte amigaPresentationPhase;
	bool amigaPresentationPending;
	bool amigaPresentationActive;
	bool amigaPaletteTransitionPending;
	EmmanuelleSurfaceState amigaPresentedFrame;
};

struct EmmanuelleNativeState {
	EmmanuelleNativeState();

	byte sampleBank;
	byte sampleCount;
	bool initialInitComplete;
	bool skipIntermediateSequenceNodes;
	byte effectCommand;
	bool colorCycleEnabled;
	byte colorCycleCounter;
};

struct EmmanuelleSaveState {
	EmmanuelleSaveState();

	OldTotPlatformTag platform;
	EmmanuelleRuntimeProfile profile;
	ModuleId module;
	Common::String moduleFileStem;
	uint32 moduleSize;
	byte moduleMD5[16];
	uint16 variables[512];
	OldTotRuntimeState runtime;
	OldTotVMTransientState vmTransient;
	OldTotExecutorState executor;
	uint32 randomState;
	uint32 commonRandomSeed;
	uint32 schedulerAccumulator;
	uint32 schedulerDebt;
	uint16 rememberedNode24Offset;
	uint16 cursorDescriptor;
	bool cursorDefined;
	bool cursorVisible;
	EmmanuelleRendererState renderer;
	EmmanuelleNativeState nativeState;
};

bool encodeEmmanuelleSave(const EmmanuelleSaveState &state,
	Common::WriteStream &stream);
bool decodeEmmanuelleSave(Common::SeekableReadStream &stream,
	EmmanuelleSaveState &state, Common::String &reason);
bool validateEmmanuelleRendererState(const EmmanuelleRendererState &state,
	uint16 width, uint16 height, byte bytesPerPixel,
	byte atariPresentationMode);
} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_SAVELOAD_H
