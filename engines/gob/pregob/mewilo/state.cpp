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

#include "common/md5.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "common/ustr.h"

#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/pregob/mewilo/tot.h"
#include "gob/util.h"

namespace Gob {
namespace Mewilo {

struct PreGob_Mewilo::PreparedRestore {
	PreparedModule module;
	MewiloSaveState state;
	MewiloExecutor executor;
	Common::Array<SavedRect> regions;
	Common::Array<byte> scratch;
	SurfacePtr framebuffer;

	explicit PreparedRestore(MewiloExecHost &host) : executor(host) {}
};

bool PreGob_Mewilo::canSaveGameState(Common::U32String *msg) const {
	bool ready = !hasError() && _saveReady && !_pendingRestore && _tot &&
		_executor.atSaveBoundary() && mewiloModuleIndex(_moduleStem, _profile.modulePrefix) >= 0 &&
		audioIdle() && _vm->_draw->_backSurface && _vm->_draw->_backSurface->getBPP() == 1 &&
		_vm->_draw->_backSurface->getWidth() == 320 &&
		_vm->_draw->_backSurface->getHeight() == 200 && _savedRects.size() <= 64;
	uint32 regionPixels = 0;
	for (uint i = 0; ready && i < _savedRects.size(); ++i) {
		const SurfacePtr &s = _savedRects[i].surface;
		ready = s && s->getBPP() == 1;
		if (ready) {
			regionPixels += uint32(s->getWidth()) * s->getHeight();
			ready = regionPixels <= 2 * 1024 * 1024;
		}
	}
	if (!ready && msg)
		*msg = _(
			"Save at a scene choice or continue prompt after audio ends, outside a text-entry dialog.");
	return ready;
}

bool PreGob_Mewilo::canLoadGameState(Common::U32String *msg) const {
	if (_startupLoad)
		return !hasError() && _saveReady && !_pendingRestore && _vm->_draw->_backSurface;
	// The explicit continuation covers ordinary choices and FF03(0), including intro prompts.
	const bool ready = !hasError() && _saveReady && !_pendingRestore && _tot &&
		_executor.atSaveBoundary() && audioIdle() && _vm->_draw->_backSurface &&
		_vm->_draw->_backSurface->getBPP() == 1 && _vm->_draw->_backSurface->getWidth() == 320 &&
		_vm->_draw->_backSurface->getHeight() == 200;
	if (!ready && msg)
		*msg =
			_("Load at a choice or continue prompt after audio ends, outside a text-entry dialog.");
	return ready;
}

bool PreGob_Mewilo::buildReleaseIdentity(MewiloReleaseIdentity &out) const {
	MewiloReleaseIdentity candidate;
	Common::MemoryWriteStreamDynamic manifest(DisposeAfterUse::YES);
	const char magic[] = "MEWILO resource identity 1";
	manifest.write(magic, sizeof(magic));
	// Hash owned decoded font bytes, then compare them with the resident font.
	// A second read must never relabel the bitmap already used by this session.
	Common::SeekableReadStream *fontStream = openResource("LETTRES.BIN");
	MewiloFont font;
	byte digest[16];
	const bool fontOK = fontStream &&
		font.load(*fontStream, _profile.platform == kMewiloPlatformThomsonTO) &&
		font.computeMD5(digest) && !memcmp(digest, _fontMd5, 16);
	delete fontStream;
	if (!fontOK)
		return false;
	manifest.write(digest, 16);
	// Atari's optional I01 bank is never drawn. Only resources that can affect
	// script execution or rendered pixels participate in this manifest.
	Surface scratch(320, 200, 1);
	for (uint i = 0; i < 9; ++i) {
		PreparedModule p;
		if (!prepareModule(mewiloModuleStem(i, _profile.modulePrefix), p))
			return false;
		candidate.scripts[i] = p.scriptPatchReport;
		memcpy(candidate.banks[i], p.bankMd5, 16);
		manifest.writeUint32LE(p.scriptPatchReport.sourceSize);
		manifest.write(p.scriptPatchReport.sourceMd5, 32);
		manifest.writeUint32LE(p.scriptPatchReport.effectiveSize);
		manifest.write(p.scriptPatchReport.effectiveMd5, 32);
		manifest.write(p.bankMd5, 16);
		MewiloGraphStats stats;
		Common::Array<uint16> nodes;
		if (!p.tot->validateReachableGraph(stats, &nodes))
			return false;
		for (uint j = 0; j < nodes.size(); ++j) {
			const uint16 n = nodes[j];
			if (p.tot->byteAt(n) != 0x60)
				continue;
			uint16 framed = 0;
			uint32 size = 0;
			const byte *data = nullptr;
			if (!p.i00 || !p.i00->getRecord(p.tot->byteAt(n + 7), framed, data, size))
				return false;
			const MewiloPackedDecodeResult decoded = drawMewiloPacked(data, size, scratch, 0, 0,
				p.tot->byteAt(n + 5) * _profile.packedPixelsPerCell, p.tot->byteAt(n + 6));
			// Only exact payloads are transferable from Amiga to the Atari scratch path.
			if (!decoded.complete || decoded.truncated || decoded.runOverhang ||
				decoded.bytesConsumed != size || framed != p.tot->uint16BEAt(n + 1) || size > 32000)
				candidate.exactImages = false;
		}
	}
	_replacements.writeIdentity(manifest);
	if (manifest.err())
		return false;
	Common::MemoryReadStream input(manifest.getData(), manifest.size());
	if (!Common::computeStreamMD5(input, candidate.digest))
		return false;
	out = candidate;
	return true;
}

bool PreGob_Mewilo::exportSaveState(MewiloSaveState &out) const {
	if (!canSaveGameState())
		return false;
	MewiloSaveState s;
	s.platform = _profile.platform;
	if (_vm->_language != Common::FR_FRA && _vm->_language != Common::DE_DEU)
		return false;
	s.language = _vm->_language == Common::DE_DEU ? 1 : 0;
	s.module = mewiloModuleIndex(_moduleStem, _profile.modulePrefix);
	s.exactImages = _releaseIdentity.exactImages;
	s.cursorVisible = isCursorVisible();
	s.policy = _scriptPatchPolicy;
	s.randomSeed = _vm->_rnd.getSeed();
	memcpy(s.releaseMd5, _releaseIdentity.digest, 16);
	memcpy(s.sourceMd5, _scriptPatchReport.sourceMd5, 33);
	memcpy(s.effectiveMd5, _scriptPatchReport.effectiveMd5, 33);
	s.sourceSize = _scriptPatchReport.sourceSize;
	s.effectiveSize = _scriptPatchReport.effectiveSize;
	if (!_executor.exportState(s.executor))
		return false;
	const Surface &screen = *_vm->_draw->_backSurface;
	if (screen.getWidth() != 320 || screen.getHeight() != 200 || screen.getBPP() != 1)
		return false;
	s.framebuffer.resize(64000);
	for (uint y = 0; y < 200; ++y)
		memcpy(s.framebuffer.data() + y * 320, screen.getData(0, y), 320);
	s.scratch = _i00Scratch;
	for (uint i = 0; i < _savedRects.size(); ++i) {
		if (!_savedRects[i].surface || _savedRects[i].surface->getBPP() != 1)
			return false;
		const Surface &surface = *_savedRects[i].surface;
		MewiloSavedRegion r;
		r.x = _savedRects[i].x;
		r.y = _savedRects[i].y;
		r.width = surface.getWidth();
		r.height = surface.getHeight();
		r.pixels.resize(uint32(r.width) * r.height);
		for (uint y = 0; y < r.height; ++y)
			memcpy(r.pixels.data() + y * r.width, surface.getData(0, y), r.width);
		s.regions.push_back(r);
	}
	if (!validateMewiloSave(s))
		return false;
	out = s;
	return true;
}

Common::Error PreGob_Mewilo::saveGameStream(Common::WriteStream &stream, bool isAutosave) const {
	MewiloSaveState s;
	return exportSaveState(s) && encodeMewiloSave(s, stream) ? Common::kNoError
		: Common::kWritingFailed;
}

Common::Error PreGob_Mewilo::loadGameStream(Common::SeekableReadStream &stream) {
	if (!canLoadGameState())
		return Common::kReadingFailed;
	PreparedRestore *p = new PreparedRestore(*this);
	Common::String reason;
	MewiloReleaseIdentity fresh;
	bool ok = decodeMewiloSave(stream, p->state, reason);
	if (ok) {
		reason = "Installed game resources changed or are incomplete";
		ok = buildReleaseIdentity(fresh) && !memcmp(fresh.digest, _releaseIdentity.digest, 16);
	}
	if (ok)
		ok = mewiloSaveCompatible(p->state, _profile.platform,
			_vm->_language == Common::DE_DEU ? 1 : 0, _scriptPatchPolicy, fresh, reason);
	if (ok) {
		reason = "Saved module, graphics bank or continuation is invalid";
		ok = prepareModule(mewiloModuleStem(p->state.module, _profile.modulePrefix), p->module);
	}
	// Verify the actual detached bytes too, not just an earlier reopening.
	if (ok)
		ok = !strcmp(p->module.scriptPatchReport.sourceMd5, p->state.sourceMd5) &&
			!strcmp(p->module.scriptPatchReport.effectiveMd5, p->state.effectiveMd5) &&
			!memcmp(p->module.bankMd5, fresh.banks[p->state.module], 16) &&
			p->executor.importState(*p->module.tot, p->state.executor);
	if (!ok) {
		warning("Mewilo: restore rejected (%s)", reason.c_str());
		delete p;
		return Common::Error(Common::kReadingFailed, reason);
	}
	p->framebuffer.reset(new Surface(320, 200, 1));
	for (uint y = 0; y < 200; ++y)
		memcpy(p->framebuffer->getData(0, y), p->state.framebuffer.data() + y * 320, 320);
	for (uint i = 0; i < p->state.regions.size(); ++i) {
		const MewiloSavedRegion &r = p->state.regions[i];
		SavedRect rect;
		rect.x = r.x;
		rect.y = r.y;
		rect.surface.reset(new Surface(r.width, r.height, 1));
		for (uint y = 0; y < r.height; ++y)
			memcpy(rect.surface->getData(0, y), r.pixels.data() + y * r.width, r.width);
		p->regions.push_back(rect);
	}
	if (_profile.persistentI00Scratch) {
		p->scratch.resize(32000);
		memset(p->scratch.data(), 0, p->scratch.size());
		if (p->state.platform == kMewiloPlatformAtariST)
			memcpy(p->scratch.data(), p->state.scratch.data(), p->scratch.size());
	}
	_pendingRestore = p; // causes waitInput to unwind without dispatching another branch
	return Common::kNoError;
}

void PreGob_Mewilo::discardRestore() {
	delete _pendingRestore;
	_pendingRestore = nullptr;
}

void PreGob_Mewilo::commitRestore() {
	PreparedRestore *p = _pendingRestore;
	if (!p)
		return;
	stopMewiloAudio();
	commitModule(p->module);
	_executor.adoptState(p->executor);
	_savedRects.swap(p->regions);
	_i00Scratch.swap(p->scratch);
	_vm->_draw->_backSurface->blit(*p->framebuffer);
	_vm->_rnd.setSeed(p->state.randomSeed);
	_pendingKey = 0;
	_mousePresses.clear();
	_acceptMousePresses = false;
	_vm->_util->clearKeyBuf();
	_vm->_util->forceMouseUp();
	if (p->state.cursorVisible)
		showCursor();
	else
		hideCursor();
	markDirty(0, 0, 320, 200);
	discardRestore();
}

} // End of namespace Mewilo
} // End of namespace Gob
