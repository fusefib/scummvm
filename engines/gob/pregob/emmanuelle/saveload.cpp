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

#include "common/crc.h"
#include "common/memstream.h"

#include "gob/pregob/emmanuelle/saveload.h"


namespace Gob {
namespace Emmanuelle {

namespace {

const uint16 kSaveVersion = 1;
const uint32 kMaxPayloadSize = 8 * 1024 * 1024;
const uint kMaxSchedulerTargets = 255;
const uint kMaxSavedRegions = 10;
const uint kMaxActiveObjects = 20;

bool validSurfaceLayout(uint16 width, uint16 height, byte bytesPerPixel,
		uint32 size, bool allowEmpty) {
	if (width == 0 || height == 0)
		return allowEmpty && width == 0 && height == 0 &&
			bytesPerPixel == 0 && size == 0;
	return width <= 320 && height <= 200 &&
		bytesPerPixel >= 1 && bytesPerPixel <= 4 &&
		size == (uint32)width * height * bytesPerPixel;
}

bool validSurface(const EmmanuelleSurfaceState &surface, bool allowEmpty) {
	return validSurfaceLayout(surface.width, surface.height,
		surface.bytesPerPixel, surface.pixels.size(), allowEmpty);
}

// Check the stored structure here; restore validates it against game resources.
bool validateSaveState(const EmmanuelleSaveState &state) {
	const char *stem = moduleFileStem(state.module);
	if (!emmanuelleProfileMatchesPlatform(state.profile, state.platform) ||
		!stem || state.moduleFileStem.size() > 15 ||
		!state.moduleFileStem.equalsIgnoreCase(stem) ||
		state.moduleSize == 0 || state.moduleSize > 0xFFFF ||
		state.vmTransient.savedRegionDepth > kMaxSavedRegions ||
		state.vmTransient.schedulerTargets.size() > kMaxSchedulerTargets ||
		(state.vmTransient.schedulerTargets.empty()
			? state.vmTransient.schedulerIndex != 0
			: state.vmTransient.schedulerIndex >= state.vmTransient.schedulerTargets.size()) ||
		state.executor.frames.empty() ||
		state.executor.frames.size() > OldTotExecutor::kMaxFrames ||
		!state.executor.started || !state.executor.suspended)
		return false;
	for (uint i = 0; i < state.executor.frames.size(); ++i) {
		const OldTotExecFrame &frame = state.executor.frames[i];
		if (frame.kind < OldTotExecFrame::kDispatch ||
			frame.kind > OldTotExecFrame::kSequence ||
			frame.inputState < OldTotExecFrame::kInputInitial ||
			frame.inputState > OldTotExecFrame::kInputReenter)
			return false;
	}
	if (!validSurface(state.renderer.framebuffer, false) ||
		state.renderer.savedRegions.size() != state.vmTransient.savedRegionDepth ||
		state.renderer.activeObjects.size() > kMaxActiveObjects ||
		!validSurface(state.renderer.amigaPresentedFrame, true))
		return false;
	for (uint i = 0; i < state.renderer.savedRegions.size(); ++i) {
		if (!validSurface(state.renderer.savedRegions[i].surface, false))
			return false;
	}
	for (uint i = 0; i < state.renderer.activeObjects.size(); ++i) {
		const EmmanuelleActiveObjectState &object = state.renderer.activeObjects[i];
		if (!validSurface(object.background, !object.active) ||
			(object.active && (object.width != object.background.width ||
			 object.height != object.background.height)))
			return false;
	}
	if ((state.renderer.amigaPresentationMode != 0 &&
		 state.renderer.amigaPresentationMode != 43 &&
		 state.renderer.amigaPresentationMode != 44) ||
		(state.platform != kOldTotPlatformAmiga &&
		 state.renderer.amigaPresentationMode != 0) ||
		state.renderer.amigaPresentationVariant != 0 ||
		state.renderer.amigaPresentationPhase != 0 ||
		state.renderer.amigaPresentationPending ||
		state.renderer.amigaPresentationActive ||
		state.renderer.amigaPaletteTransitionPending)
		return false;
	// ST restore accepts either a complete or a wholly silent sample bank.
	if ((state.platform == kOldTotPlatformDOS &&
		 (state.nativeState.sampleBank != 0 || state.nativeState.sampleCount != 0)) ||
		(state.platform == kOldTotPlatformAtariST &&
		 (state.nativeState.sampleBank != 2 ||
		  (state.nativeState.sampleCount != 0 && state.nativeState.sampleCount != 19))) ||
		(state.platform == kOldTotPlatformAmiga &&
		 (state.nativeState.sampleBank != 2 || state.nativeState.sampleCount != 19)))
		return false;
	const EmmanuelleNativeState &native = state.nativeState;
	if (native.amigaSampleSlot == -1) {
		if (native.amigaSampleRepeatCount != 0)
			return false;
	} else if (state.platform != kOldTotPlatformAmiga || native.amigaSampleSlot < 0 ||
			native.amigaSampleSlot >= native.sampleCount ||
			(native.amigaSampleRepeatCount != 1 && native.amigaSampleRepeatCount != 2 &&
			 native.amigaSampleRepeatCount != -1)) {
		return false;
	}
	return state.platform == kOldTotPlatformAtariST
		? state.nativeState.effectCommand == 0 ||
		  state.nativeState.effectCommand == 43 || state.nativeState.effectCommand == 44
		: state.nativeState.effectCommand == 0;
}

void writeBool(Common::WriteStream &stream, bool value) {
	stream.writeByte(value ? 1 : 0);
}

bool readBool(Common::ReadStream &stream, bool &value) {
	const byte raw = stream.readByte();
	if (stream.err() || raw > 1)
		return false;
	value = raw != 0;
	return true;
}

void writeString(Common::WriteStream &stream, const Common::String &value) {
	stream.writeByte(value.size());
	stream.write(value.c_str(), value.size());
}

bool readString(Common::ReadStream &stream, Common::String &value,
				uint maxLength) {
	const uint length = stream.readByte();
	if (stream.err() || length > maxLength)
		return false;
	value.clear();
	for (uint i = 0; i < length; ++i)
		value += (char)stream.readByte();
	return !stream.err();
}

void writeSurface(Common::WriteStream &stream,
				  const EmmanuelleSurfaceState &surface) {
	stream.writeUint16LE(surface.width);
	stream.writeUint16LE(surface.height);
	stream.writeByte(surface.bytesPerPixel);
	stream.writeUint32LE(surface.pixels.size());
	if (!surface.pixels.empty())
		stream.write(&surface.pixels[0], surface.pixels.size());
}

bool readSurface(Common::ReadStream &stream,
				 EmmanuelleSurfaceState &surface, bool allowEmpty) {
	surface.width = stream.readUint16LE();
	surface.height = stream.readUint16LE();
	surface.bytesPerPixel = stream.readByte();
	const uint32 size = stream.readUint32LE();
	if (stream.err())
		return false;
	if (!validSurfaceLayout(surface.width, surface.height,
			surface.bytesPerPixel, size, allowEmpty))
		return false;
	if (size == 0)
		return true;
	surface.pixels.resize(size);
	return stream.read(&surface.pixels[0], size) == size && !stream.err();
}

void writePayload(const EmmanuelleSaveState &state,
				  Common::WriteStream &stream) {
	stream.writeByte((byte)state.platform);
	stream.writeByte((byte)state.profile);
	stream.writeByte((byte)state.module);
	stream.writeByte(0);
	writeString(stream, state.moduleFileStem);
	stream.writeUint32LE(state.moduleSize);
	stream.write(state.moduleMD5, sizeof(state.moduleMD5));
	for (uint i = 0; i < ARRAYSIZE(state.variables); ++i)
		stream.writeUint16LE(state.variables[i]);
	stream.writeByte(state.runtime.videoMode);
	stream.writeByte(state.runtime.savedVideoMode);
	stream.writeByte(state.runtime.videoInitMarker);

	stream.writeByte(state.vmTransient.savedRegionDepth);
	stream.writeByte(state.vmTransient.schedulerTargets.size());
	stream.writeByte(state.vmTransient.schedulerIndex);
	stream.writeUint16LE(state.vmTransient.schedulerReload);
	stream.writeUint16LE(state.vmTransient.schedulerCountdown);
	for (uint i = 0; i < state.vmTransient.schedulerTargets.size(); ++i)
		stream.writeUint16LE(state.vmTransient.schedulerTargets[i]);

	// A suspended executor is never empty; zero represents its full stack.
	// Earlier unreleased readers accepted at most 128 frames.
	stream.writeByte(state.executor.frames.size() == OldTotExecutor::kMaxFrames
		? 0 : state.executor.frames.size());
	stream.writeUint32LE(state.executor.steps);
	writeBool(stream, state.executor.started);
	writeBool(stream, state.executor.suspended);
	stream.writeUint16LE(state.executor.suspendedNode);
	for (uint i = 0; i < state.executor.frames.size(); ++i) {
		const OldTotExecFrame &frame = state.executor.frames[i];
		stream.writeByte((byte)frame.kind);
		stream.writeUint16LE(frame.current);
		stream.writeUint16LE(frame.cursor);
		stream.writeByte(frame.index);
		stream.writeByte(frame.count);
		stream.writeByte((byte)frame.inputState);
	}

	stream.writeUint32LE(state.randomState);
	stream.writeUint32LE(state.commonRandomSeed);
	stream.writeUint32LE(state.schedulerAccumulator);
	stream.writeUint32LE(state.schedulerDebt);
	stream.writeUint16LE(state.rememberedNode24Offset);
	// Use the formerly zero byte for slot+1 (bits 0..4) and once/twice/loop
	// (bits 5..6). Silent saves keep their version-1 layout and bytes.
	byte playback = 0;
	if (state.nativeState.amigaSampleSlot >= 0) {
		playback = state.nativeState.amigaSampleSlot + 1;
		if (state.nativeState.amigaSampleRepeatCount == 2)
			playback |= 0x20;
		else if (state.nativeState.amigaSampleRepeatCount == -1)
			playback |= 0x40;
	}
	stream.writeByte(playback);
	stream.writeUint16LE(state.cursorDescriptor);
	writeBool(stream, state.cursorDefined);
	writeBool(stream, state.cursorVisible);

	writeSurface(stream, state.renderer.framebuffer);
	stream.writeByte(state.renderer.savedRegions.size());
	for (uint i = 0; i < state.renderer.savedRegions.size(); ++i) {
		const EmmanuelleSavedRegionState &region =
			state.renderer.savedRegions[i];
		stream.writeSint16LE(region.x);
		stream.writeSint16LE(region.y);
		writeSurface(stream, region.surface);
	}
	stream.writeByte(state.renderer.activeObjects.size());
	for (uint i = 0; i < state.renderer.activeObjects.size(); ++i) {
		const EmmanuelleActiveObjectState &object =
			state.renderer.activeObjects[i];
		writeBool(stream, object.active);
		stream.writeSint16LE(object.x);
		stream.writeSint16LE(object.y);
		stream.writeSint16LE(object.width);
		stream.writeSint16LE(object.height);
		writeSurface(stream, object.background);
	}
	for (uint i = 0; i < ARRAYSIZE(state.renderer.atariPalette); ++i)
		stream.writeUint16LE(state.renderer.atariPalette[i]);
	for (uint i = 0; i < ARRAYSIZE(state.renderer.amigaPalette); ++i)
		stream.writeUint16LE(state.renderer.amigaPalette[i]);
	writeBool(stream, state.renderer.atariPaletteValid);
	writeBool(stream, state.renderer.atariBlack);
	writeBool(stream, state.renderer.amigaPaletteValid);
	writeBool(stream, state.renderer.amigaBlack);
	stream.writeUint32LE(state.renderer.amigaModuleGeneration);
	stream.writeUint32LE(state.renderer.amigaPresentedGeneration);
	stream.writeByte(state.renderer.amigaPresentationMode);
	stream.writeByte(state.renderer.amigaPresentationVariant);
	stream.writeByte(state.renderer.amigaPresentationPhase);
	writeBool(stream, state.renderer.amigaPresentationPending);
	writeBool(stream, state.renderer.amigaPresentationActive);
	writeBool(stream, state.renderer.amigaPaletteTransitionPending);
	writeSurface(stream, state.renderer.amigaPresentedFrame);

	stream.writeByte(state.nativeState.sampleBank);
	stream.writeByte(state.nativeState.sampleCount);
	writeBool(stream, state.nativeState.initialInitComplete);
	writeBool(stream, state.nativeState.skipIntermediateSequenceNodes);
	stream.writeByte(state.nativeState.effectCommand);
	writeBool(stream, state.nativeState.colorCycleEnabled);
	stream.writeByte(state.nativeState.colorCycleCounter);
}

bool readPayload(Common::SeekableReadStream &stream,
				 EmmanuelleSaveState &state) {
	state.platform = (OldTotPlatformTag)stream.readByte();
	state.profile = (EmmanuelleRuntimeProfile)stream.readByte();
	state.module = (ModuleId)stream.readByte();
	const byte reserved = stream.readByte();
	if (stream.err() || reserved != 0 ||
		!readString(stream, state.moduleFileStem, 15))
		return false;
	state.moduleSize = stream.readUint32LE();
	if (stream.read(state.moduleMD5, sizeof(state.moduleMD5)) !=
			sizeof(state.moduleMD5))
		return false;
	for (uint i = 0; i < ARRAYSIZE(state.variables); ++i)
		state.variables[i] = stream.readUint16LE();
	state.runtime.videoMode = stream.readByte();
	state.runtime.savedVideoMode = stream.readByte();
	state.runtime.videoInitMarker = stream.readByte();

	state.vmTransient.savedRegionDepth = stream.readByte();
	const uint schedulerCount = stream.readByte();
	state.vmTransient.schedulerIndex = stream.readByte();
	state.vmTransient.schedulerReload = stream.readUint16LE();
	state.vmTransient.schedulerCountdown = stream.readUint16LE();
	if (stream.err())
		return false;
	state.vmTransient.schedulerTargets.resize(schedulerCount);
	for (uint i = 0; i < schedulerCount; ++i)
		state.vmTransient.schedulerTargets[i] = stream.readUint16LE();

	const byte encodedFrameCount = stream.readByte();
	const uint frameCount = encodedFrameCount
		? encodedFrameCount : (uint)OldTotExecutor::kMaxFrames;
	state.executor.steps = stream.readUint32LE();
	if (!readBool(stream, state.executor.started) ||
		!readBool(stream, state.executor.suspended))
		return false;
	state.executor.suspendedNode = stream.readUint16LE();
	if (stream.err())
		return false;
	state.executor.frames.resize(frameCount);
	for (uint i = 0; i < frameCount; ++i) {
		OldTotExecFrame &frame = state.executor.frames[i];
		frame.kind = (OldTotExecFrame::Kind)stream.readByte();
		frame.current = stream.readUint16LE();
		frame.cursor = stream.readUint16LE();
		frame.index = stream.readByte();
		frame.count = stream.readByte();
		frame.inputState = (OldTotExecFrame::InputState)stream.readByte();
	}

	state.randomState = stream.readUint32LE();
	state.commonRandomSeed = stream.readUint32LE();
	state.schedulerAccumulator = stream.readUint32LE();
	state.schedulerDebt = stream.readUint32LE();
	state.rememberedNode24Offset = stream.readUint16LE();
	const byte playback = stream.readByte();
	state.cursorDescriptor = stream.readUint16LE();
	if (!readBool(stream, state.cursorDefined) ||
		!readBool(stream, state.cursorVisible) ||
		(playback & 0x80) != 0 || (playback & 0x60) == 0x60)
		return false;
	if (playback != 0) {
		static const int16 repeats[] = {1, 2, -1};
		state.nativeState.amigaSampleSlot = (playback & 0x1F) - 1;
		state.nativeState.amigaSampleRepeatCount = repeats[(playback >> 5) & 3];
	}

	if (!readSurface(stream, state.renderer.framebuffer, false))
		return false;
	const uint savedRegionCount = stream.readByte();
	if (stream.err() || savedRegionCount > kMaxSavedRegions)
		return false;
	state.renderer.savedRegions.resize(savedRegionCount);
	for (uint i = 0; i < savedRegionCount; ++i) {
		EmmanuelleSavedRegionState &region =
			state.renderer.savedRegions[i];
		region.x = stream.readSint16LE();
		region.y = stream.readSint16LE();
		if (!readSurface(stream, region.surface, false))
			return false;
	}
	const uint activeCount = stream.readByte();
	if (stream.err() || activeCount > kMaxActiveObjects)
		return false;
	state.renderer.activeObjects.resize(activeCount);
	for (uint i = 0; i < activeCount; ++i) {
		EmmanuelleActiveObjectState &object =
			state.renderer.activeObjects[i];
		if (!readBool(stream, object.active))
			return false;
		object.x = stream.readSint16LE();
		object.y = stream.readSint16LE();
		object.width = stream.readSint16LE();
		object.height = stream.readSint16LE();
		if (!readSurface(stream, object.background, !object.active))
			return false;
	}
	for (uint i = 0; i < ARRAYSIZE(state.renderer.atariPalette); ++i)
		state.renderer.atariPalette[i] = stream.readUint16LE();
	for (uint i = 0; i < ARRAYSIZE(state.renderer.amigaPalette); ++i)
		state.renderer.amigaPalette[i] = stream.readUint16LE();
	if (!readBool(stream, state.renderer.atariPaletteValid) ||
		!readBool(stream, state.renderer.atariBlack) ||
		!readBool(stream, state.renderer.amigaPaletteValid) ||
		!readBool(stream, state.renderer.amigaBlack))
		return false;
	state.renderer.amigaModuleGeneration = stream.readUint32LE();
	state.renderer.amigaPresentedGeneration = stream.readUint32LE();
	state.renderer.amigaPresentationMode = stream.readByte();
	state.renderer.amigaPresentationVariant = stream.readByte();
	state.renderer.amigaPresentationPhase = stream.readByte();
	if (!readBool(stream, state.renderer.amigaPresentationPending) ||
		!readBool(stream, state.renderer.amigaPresentationActive) ||
		!readBool(stream, state.renderer.amigaPaletteTransitionPending))
		return false;
	if (!readSurface(stream, state.renderer.amigaPresentedFrame, true))
		return false;

	state.nativeState.sampleBank = stream.readByte();
	state.nativeState.sampleCount = stream.readByte();
	if (!readBool(stream, state.nativeState.initialInitComplete) ||
		!readBool(stream, state.nativeState.skipIntermediateSequenceNodes))
		return false;
	state.nativeState.effectCommand = stream.readByte();
	if (!readBool(stream, state.nativeState.colorCycleEnabled))
		return false;
	state.nativeState.colorCycleCounter = stream.readByte();
	// Short scalar reads on memory streams set eos() without setting err().
	return !stream.err() && !stream.eos() && stream.pos() == stream.size() &&
		validateSaveState(state);
}

} // End of anonymous namespace

void exportOldTotSurface(const Surface &surface,
								  EmmanuelleSurfaceState &out) {
	out.width = surface.getWidth();
	out.height = surface.getHeight();
	out.bytesPerPixel = surface.getBPP();
	out.pixels.resize((uint32)out.width * out.height * out.bytesPerPixel);
	for (uint16 y = 0; y < out.height; ++y) {
		memcpy(&out.pixels[(uint32)y * out.width * out.bytesPerPixel],
			surface.getData(0, y), (uint32)out.width * out.bytesPerPixel);
	}
}

bool importOldTotSurface(const EmmanuelleSurfaceState &in,
								  SurfacePtr &out) {
	if (in.width == 0 || in.height == 0 || in.bytesPerPixel == 0 ||
		in.pixels.size() !=
			(uint32)in.width * in.height * in.bytesPerPixel)
		return false;
	SurfacePtr surface(new Surface(in.width, in.height, in.bytesPerPixel));
	for (uint16 y = 0; y < in.height; ++y) {
		memcpy(surface->getData(0, y),
			&in.pixels[(uint32)y * in.width * in.bytesPerPixel],
			(uint32)in.width * in.bytesPerPixel);
	}
	out = surface;
	return true;
}

EmmanuelleRendererState::EmmanuelleRendererState() {
	atariPaletteValid = false;
	atariBlack = false;
	amigaPaletteValid = false;
	amigaBlack = false;

	amigaModuleGeneration = 0;
	amigaPresentedGeneration = 0;

	amigaPresentationMode = 0;
	amigaPresentationVariant = 0;
	amigaPresentationPhase = 0;

	amigaPresentationPending = false;
	amigaPresentationActive = false;
	amigaPaletteTransitionPending = false;

	for (uint i = 0; i < ARRAYSIZE(atariPalette); ++i)
		atariPalette[i] = 0;
	for (uint i = 0; i < ARRAYSIZE(amigaPalette); ++i)
		amigaPalette[i] = 0;
}

EmmanuelleNativeState::EmmanuelleNativeState() {
	sampleBank = 0;
	sampleCount = 0;
	amigaSampleSlot = -1;
	amigaSampleRepeatCount = 0;

	initialInitComplete = false;
	skipIntermediateSequenceNodes = false;

	effectCommand = 0;
	colorCycleEnabled = false;
	colorCycleCounter = 0;
}

EmmanuelleSaveState::EmmanuelleSaveState() {
	platform = kOldTotPlatformUnknown;
	profile = kEmmanuelleProfileInvalid;
	module = kModuleNone;
	moduleSize = 0;

	randomState = 0;
	commonRandomSeed = 0;

	// Remainder and deferred work from the native scheduler.
	schedulerAccumulator = 0;
	schedulerDebt = 0;

	rememberedNode24Offset = 0;

	cursorDescriptor = 0;
	cursorDefined = false;
	cursorVisible = false;

	for (uint i = 0; i < ARRAYSIZE(moduleMD5); ++i)
		moduleMD5[i] = 0;
	for (uint i = 0; i < ARRAYSIZE(variables); ++i)
		variables[i] = 0;
}

bool validateEmmanuelleRendererState(const EmmanuelleRendererState &state,
		uint16 width, uint16 height, byte bytesPerPixel,
		byte atariPresentationMode) {
	if (!validSurface(state.framebuffer, false) ||
		state.framebuffer.width != width ||
		state.framebuffer.height != height ||
		state.framebuffer.bytesPerPixel != bytesPerPixel ||
		state.savedRegions.size() > 10 ||
		(atariPresentationMode != 0 && atariPresentationMode != 43 &&
		 atariPresentationMode != 44) ||
		(state.amigaPresentationMode != 0 &&
		 state.amigaPresentationMode != 43 &&
		 state.amigaPresentationMode != 44) ||
		state.amigaPresentationVariant != 0 ||
		state.amigaPresentationPhase != 0 ||
		state.amigaPresentationPending || state.amigaPresentationActive ||
		state.amigaPaletteTransitionPending)
		return false;
	for (uint i = 0; i < state.savedRegions.size(); ++i) {
		const EmmanuelleSavedRegionState &region = state.savedRegions[i];
		if (!validSurface(region.surface, false) || region.x < 0 ||
			region.y < 0 || region.x + region.surface.width > width ||
			region.y + region.surface.height > height)
			return false;
	}
	if (!state.amigaPresentedFrame.pixels.empty() &&
		(!validSurface(state.amigaPresentedFrame, false) ||
		 state.amigaPresentedFrame.width != width ||
		 state.amigaPresentedFrame.height != height ||
		 state.amigaPresentedFrame.bytesPerPixel != bytesPerPixel))
		return false;
	return true;
}

EmmanuelleRuntimeProfile emmanuelleRuntimeProfile(
		OldTotPlatformTag platform, Common::Language language) {
	if (platform == kOldTotPlatformDOS) {
		if (language == Common::ES_ESP)
			return kEmmanuelleProfileDosSpanish;
		if (language == Common::FR_FRA)
			return kEmmanuelleProfileDosFrench;
		return kEmmanuelleProfileDosEnglish;
	}
	if (platform == kOldTotPlatformAtariST) {
		if (language == Common::ES_ESP)
			return kEmmanuelleProfileAtariSpanish;
		if (language == Common::FR_FRA)
			return kEmmanuelleProfileAtariFrench;
		return kEmmanuelleProfileAtariEnglish;
	}
	if (platform == kOldTotPlatformAmiga) {
		if (language == Common::DE_DEU)
			return kEmmanuelleProfileAmigaGerman;
		if (language == Common::FR_FRA)
			return kEmmanuelleProfileAmigaFrench;
		return kEmmanuelleProfileAmigaEnglish;
	}
	return kEmmanuelleProfileInvalid;
}

const char *emmanuelleSaveBoundaryBlockerName(
		EmmanuelleSaveBoundaryBlocker blocker) {
	switch (blocker) {
	case kEmmanuelleSaveBoundaryReady:
		return "ready";
	case kEmmanuelleSaveBoundaryNoRuntime:
		return "no_runtime";
	case kEmmanuelleSaveBoundaryNoInput:
		return "no_input";
	case kEmmanuelleSaveBoundaryPresentation:
		return "presentation";
	case kEmmanuelleSaveBoundaryAudio:
		return "audio";
	case kEmmanuelleSaveBoundaryFight:
		return "fight";
	case kEmmanuelleSaveBoundaryRegions:
		return "regions";
	default:
		return "unknown";
	}
}

bool emmanuelleProfileMatchesPlatform(EmmanuelleRuntimeProfile profile,
									 OldTotPlatformTag platform) {
	switch (profile) {
	case kEmmanuelleProfileDosEnglish:
	case kEmmanuelleProfileDosSpanish:
	case kEmmanuelleProfileDosFrench:
		return platform == kOldTotPlatformDOS;
	case kEmmanuelleProfileAtariEnglish:
	case kEmmanuelleProfileAtariSpanish:
	case kEmmanuelleProfileAtariFrench:
		return platform == kOldTotPlatformAtariST;
	case kEmmanuelleProfileAmigaEnglish:
	case kEmmanuelleProfileAmigaGerman:
	case kEmmanuelleProfileAmigaFrench:
		return platform == kOldTotPlatformAmiga;
	default:
		return false;
	}
}

bool encodeEmmanuelleSave(const EmmanuelleSaveState &state,
						  Common::WriteStream &stream) {
	// Validate before byte fields can truncate values or the destination changes.
	if (!validateSaveState(state))
		return false;

	Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES);
	writePayload(state, payload);
	if (payload.err() || payload.size() == 0 ||
		payload.size() > kMaxPayloadSize)
		return false;
	Common::CRC32 crc;
	const uint32 checksum = crc.crcFast(payload.getData(), payload.size());
	stream.write("EMSV", 4);
	stream.writeUint16LE(kSaveVersion);
	stream.writeUint16LE(0);
	stream.writeUint32LE(payload.size());
	stream.writeUint32LE(checksum);
	stream.write(payload.getData(), payload.size());
	return !stream.err();
}

bool decodeEmmanuelleSave(Common::SeekableReadStream &stream,
						  EmmanuelleSaveState &state,
						  Common::String &reason) {
	byte magic[4];
	if (stream.read(magic, sizeof(magic)) != sizeof(magic) ||
		memcmp(magic, "EMSV", sizeof(magic)) != 0) {
		reason = "invalid magic";
		return false;
	}
	const uint16 version = stream.readUint16LE();
	const uint16 reserved = stream.readUint16LE();
	const uint32 payloadSize = stream.readUint32LE();
	const uint32 expectedCRC = stream.readUint32LE();
	if (stream.err() || version != kSaveVersion || reserved != 0 ||
		payloadSize == 0 || payloadSize > kMaxPayloadSize ||
		payloadSize > (uint32)(stream.size() - stream.pos())) {
		reason = version != kSaveVersion ? "unsupported version" :
			"invalid payload length";
		return false;
	}
	Common::Array<byte> payload;
	payload.resize(payloadSize);
	if (stream.read(&payload[0], payloadSize) != payloadSize) {
		reason = "truncated payload";
		return false;
	}
	Common::CRC32 crc;
	if (crc.crcFast(&payload[0], payload.size()) != expectedCRC) {
		reason = "payload checksum mismatch";
		return false;
	}
	Common::MemoryReadStream payloadStream(&payload[0], payload.size());
	EmmanuelleSaveState decoded;
	if (!readPayload(payloadStream, decoded)) {
		reason = "invalid payload state";
		return false;
	}
	state = decoded;
	reason.clear();
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
