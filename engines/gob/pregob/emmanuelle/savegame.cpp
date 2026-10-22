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

#include "gob/pregob/emmanuelle/emmanuelle.h"

#include "common/md5.h"
#include "common/translation.h"
#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/video.h"
#include "gob/pregob/emmanuelle/dos.h"
#include "gob/pregob/emmanuelle/oldixx.h"

namespace Gob {
namespace Emmanuelle {

struct EmmanuellePreparedRestore {
	EmmanuellePreparedRestore() {
		// Own staged resources; commitRestore adopts objects or swaps their state.
		tot = nullptr;
		localI00 = nullptr;
		vm = nullptr;
		executor = nullptr;
		active = nullptr;

		cursorDefined = false;
	}
	~EmmanuellePreparedRestore() {
		delete executor;
		delete vm;
		delete active;
		delete localI00;
		delete tot;
	}

	OldTotData *tot;
	OldIxxBank *localI00;
	OldTotVM *vm;
	OldTotExecutor *executor;
	ActiveSpriteLayer *active;
	OldTotRendererPreparedState renderer;
	SurfacePtr cursor;
	bool cursorDefined;
};

bool PreGob_Emmanuelle::moduleUsesLocalGraphics(
		ModuleId module) const {
	return module == kModuleLoadHot || module == kModuleLoadPla ||
		module == kModuleLoadCas || module == kModuleLoadAero;
}

bool PreGob_Emmanuelle::computeModuleIdentity(ModuleId module,
		uint32 &size, byte digest[16]) const {
	const char *stem = moduleFileStem(module);
	if (!stem || !digest)
		return false;
	const char *extension = ".TOT";
	Common::SeekableReadStream *stream = openLoose(
		Common::String(stem) + extension);
	if (!stream || stream->size() <= 0 || stream->size() > 0xFFFFFFFFU) {
		delete stream;
		return false;
	}
	size = (uint32)stream->size();
	const bool valid = Common::computeStreamMD5(*stream, digest);
	delete stream;
	return valid;
}

EmmanuelleSaveBoundaryState PreGob_Emmanuelle::saveBoundaryState() const {
	EmmanuelleSaveBoundaryState state;
	state.runtimeReady = _tot && _scriptVM && _executor;
	if (state.runtimeReady) {
		uint16 node = 0;
		state.activeInput = _executor->activeInputNode(node) && node != 0 &&
			node < _tot->size() && _tot->byteAt(node) == 0x1B;
		if (state.activeInput)
			state.activeInputNode = node;
		state.suspendedWait = state.activeInput && _executor->suspended() &&
			_executor->suspendedNode() == node;
	}

	state.paletteReady = true;

	state.inInit = _currentModule == kModuleInit;
	state.audioIdle = !isAudioPlaying(_platformTag, _audioActive,
		_vm && _vm->_sound && _vm->_sound->blasterPlayingSound());
	state.regionsCoherent = _scriptVM &&
		_renderer.savedRegionDepth() == _scriptVM->savedRegionDepth();
	state.fightIdle = !_fightActive;
	return state;
}

bool PreGob_Emmanuelle::safeSaveBoundary(Common::U32String *msg) const {
	const EmmanuelleSaveBoundaryState state = saveBoundaryState();
	if (state.canSave())
		return true;
	if (msg)
		*msg = _("Saving is available only at ordinary choices outside the introduction.");
	return false;
}

bool PreGob_Emmanuelle::canSaveGameState(Common::U32String *msg) const {
	return safeSaveBoundary(msg);
}

bool PreGob_Emmanuelle::canLoadGameState(Common::U32String *msg) const {
	const EmmanuelleSaveBoundaryState state = saveBoundaryState();
	if (state.canLoad())
		return true;
	if (msg)
		*msg = _("Loading is available only at ordinary choices.");
	return false;
}

bool PreGob_Emmanuelle::exportSaveState(
		EmmanuelleSaveState &state) const {
	if (!safeSaveBoundary(nullptr) || !_scriptVM || !_executor)
		return false;
	state.platform = _platformTag;
	state.profile = emmanuelleRuntimeProfile(_platformTag, _vm->_language);
	state.module = _currentModule;
	state.moduleFileStem = _moduleFileStem;
	if (!computeModuleIdentity(state.module, state.moduleSize,
			state.moduleMD5))
		return false;
	_scriptVM->storeVariables(state.variables, ARRAYSIZE(state.variables));
	state.runtime = _scriptVM->runtimeState();
	_scriptVM->exportTransientState(state.vmTransient);
	if (!_executor->exportSuspendedInputState(state.executor))
		return false;
	state.randomState = _randomState;
	state.commonRandomSeed = _vm->_rnd.getSeed();
	state.schedulerAccumulator = _schedulerAccumulator;
	state.schedulerDebt = _schedulerDebt;
	state.rememberedNode24Offset = _node24Offset;
	state.cursorDescriptor = _cursorDescriptor;
	state.cursorDefined = _cursorDefined;
	state.cursorVisible = isCursorVisible();
	if (!_renderer.exportState(state.renderer) ||
		!_active.exportState(state.renderer))
		return false;

	return true;
}

Common::Error PreGob_Emmanuelle::saveGameStream(
		Common::WriteStream &stream, bool isAutosave) const {
	const EmmanuelleSaveBoundaryState boundary = saveBoundaryState();
	EmmanuelleSaveState state;
	if (!exportSaveState(state) || !encodeEmmanuelleSave(state, stream)) {
		debugC(1, kDebugSaveLoad,
			"Emmanuelle: unable to serialize runtime state");
		return Common::kWritingFailed;
	}
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugSaveLoad,
			   "EMMA_TRACE event=save_export kind=%s normalized=%u "
			   "module=%s node=0x%04X",
			   isAutosave ? "autosave" : "manual",
			   boundary.normalizesExecutor() ? 1U : 0U,
			   state.moduleFileStem.c_str(), state.executor.suspendedNode);
	debugC(2, kDebugSaveLoad,
		"Emmanuelle: %s profile %u module %s at 0x%04X",
		isAutosave ? "autosaved" : "saved",
		(uint)state.profile, state.moduleFileStem.c_str(),
		state.executor.suspendedNode);
	return Common::kNoError;
}

bool PreGob_Emmanuelle::prepareRestore(const EmmanuelleSaveState &state,
		EmmanuellePreparedRestore &prepared, Common::String &reason) {
	if (state.platform != _platformTag ||
		state.profile != emmanuelleRuntimeProfile(
			_platformTag, _vm->_language)) {
		reason = "runtime profile mismatch";
		return false;
	}
	const char *stem = moduleFileStem(state.module);
	if (!stem || !state.moduleFileStem.equalsIgnoreCase(stem)) {
		reason = "invalid module";
		return false;
	}
	const char *extension = ".TOT";
	uint32 size = 0;
	byte digest[16];
	prepared.tot = loadScriptResource(Common::String(stem) + extension,
		reason, &size, digest);
	if (!prepared.tot)
		return false;
	if (size != state.moduleSize ||
		memcmp(digest, state.moduleMD5, sizeof(digest)) != 0) {
		reason = _s("This save uses a different version of the game scripts. "
			"Try an earlier saved game.");
		return false;
	}
	if (state.executor.suspendedNode >= prepared.tot->size() ||
		prepared.tot->byteAt(state.executor.suspendedNode) != 0x1B) {
		reason = "invalid suspended instruction";
		return false;
	}
	if (moduleUsesLocalGraphics(state.module)) {
		const char *ixxExtension = ".I00";
		Common::SeekableReadStream *stream = openLoose(
			Common::String(stem) + ixxExtension);
		if (!stream) {
			reason = "local graphics bank is unavailable";
			return false;
		}
		prepared.localI00 = new OldIxxBank(*stream);
		delete stream;
		if (!prepared.localI00->valid()) {
			reason = "invalid local graphics bank";
			return false;
		}
	}
	prepared.vm = new OldTotVM(*prepared.tot, *this);
	prepared.vm->loadVariables(state.variables, ARRAYSIZE(state.variables));
	prepared.vm->runtimeState() = state.runtime;
	if (!prepared.vm->importTransientState(state.vmTransient)) {
		reason = "invalid VM state";
		return false;
	}
	prepared.executor = new OldTotExecutor(
		*prepared.tot, *prepared.vm, *this);
	if (!prepared.executor->importState(state.executor)) {
		reason = "invalid executor state";
		return false;
	}
	if (state.renderer.savedRegions.size() !=
		state.vmTransient.savedRegionDepth) {
		reason = "invalid framebuffer or region state";
		return false;
	}
	OldTotRenderer candidateRenderer(_vm);
	candidateRenderer.setPlatform(_platformTag);
	candidateRenderer.setResourceBanks(prepared.localI00, _sharedI01);
	if (!candidateRenderer.prepareState(state.renderer,
			prepared.renderer)) {
		reason = "invalid renderer state";
		return false;
	}
	prepared.active = new ActiveSpriteLayer(_vm, 20);
	if (!prepared.active->importState(state.renderer)) {
		reason = "invalid active-object state";
		return false;
	}

	byte table[16];
	byte alternateTable[16];
	const bool paletteAvailable = state.runtime.videoMode <= 1
		? getDosCgaColorTables(stem, table, alternateTable)
		: getDosPaletteRegisters(stem, table);
	if (!paletteAvailable) {
		reason = "DOS presentation resources are unavailable";
		return false;
	}
	if (state.cursorDefined) {
		OldTotDrawDescriptor descriptor;
		if (!prepared.tot->decodeDrawDescriptor(
				state.cursorDescriptor, descriptor)) {
			reason = "invalid cursor descriptor";
			return false;
		}
		prepared.cursor = candidateRenderer.decodeSpriteSurface(
			*prepared.tot, descriptor);
		if (!prepared.cursor || prepared.cursor->getWidth() != 16 ||
			prepared.cursor->getHeight() != 16) {
			reason = "invalid cursor image";
			return false;
		}
		prepared.cursorDefined = true;
	}

	reason.clear();
	return true;
}

void PreGob_Emmanuelle::commitRestore(const EmmanuelleSaveState &state,
		EmmanuellePreparedRestore &prepared) {
	const char *stem = moduleFileStem(state.module);
	const bool usesLocalGraphics = moduleUsesLocalGraphics(state.module);
	if (_vm && _vm->_sound)
			_vm->_sound->speakerOff();
	clearModuleObjects();
	_tot = prepared.tot;
	prepared.tot = nullptr;
	_localI00 = prepared.localI00;
	prepared.localI00 = nullptr;
	_scriptVM = prepared.vm;
	prepared.vm = nullptr;
	_executor = prepared.executor;
	prepared.executor = nullptr;
	_moduleFileStem = stem;
	_currentModule = state.module;
	_currentUsesLocalGraphics = usesLocalGraphics;
	_renderer.setResourceBanks(_localI00, _sharedI01);
	_renderer.commitState(prepared.renderer);
	_active.swapState(*prepared.active);
	_runtimeState = state.runtime;
	for (uint i = 0; i < ARRAYSIZE(_variables); ++i)
		_variables[i] = state.variables[i];
	_randomState = state.randomState;
	_vm->_rnd.setSeed(state.commonRandomSeed);
	_schedulerAccumulator = state.schedulerAccumulator;
	_schedulerDebt = state.schedulerDebt;
	_node24Offset = state.rememberedNode24Offset;
	resetInitialStateAfterLoad();
	_cursorDefined = false;
	_cursorDescriptor = 0;
	if (prepared.cursorDefined) {
		setCursor(*prepared.cursor, 0, 0);
		_cursorDefined = true;
		_cursorDescriptor = state.cursorDescriptor;
	}
	if (state.cursorVisible && _cursorDefined)
		showCursor();
	else
		hideCursor();
	discardPendingInput();
	setInputContext(kEmmanuelleInputChoice);
	if (!_renderer.applyDosPalette(_moduleFileStem, _runtimeState.videoMode))
		warning("Emmanuelle: unable to restore DOS palette for %s",
			_moduleFileStem.c_str());
	presentFrame(false, true);
}

void PreGob_Emmanuelle::resetInitialStateAfterLoad() {
	_introSkip.finish();
	_initialInitPresentation = false;
	_initialTitleState = kInitialTitleNone;
	_initialTomahawkVisibleStart = 0;
	_dosInitialFrameCoverage = Common::Rect();
	_dosInitialFrameHasCoverage = false;
	_dosInitialFramePresented = false;
	_mainMenuActionThisPoll = false;
	_openMainMenuAfterSuspend = false;
	_audioKind = kOldTotAudioNone;
	_audioSlot = -1;
	_audioFrequency = 0;
	_audioRepeatCount = 0;
	_audioActive = false;
	_audioBlocking = false;
}

Common::Error PreGob_Emmanuelle::loadGameStream(
		Common::SeekableReadStream &stream) {
	if (_executor && !_executor->suspended())
		return Common::kReadingFailed;
	EmmanuelleSaveState state;
	EmmanuellePreparedRestore prepared;
	Common::String reason;
	if (!decodeEmmanuelleSave(stream, state, reason) ||
		!prepareRestore(state, prepared, reason)) {
		debugC(1, kDebugSaveLoad,
			"Emmanuelle: rejected save state: %s",
			reason.empty() ? "restore failed" : reason.c_str());
		return Common::Error(Common::kReadingFailed, reason);
	}
	commitRestore(state, prepared);
	debugC(2, kDebugSaveLoad,
		"Emmanuelle: restored profile %u module %s at 0x%04X",
		(uint)state.profile, state.moduleFileStem.c_str(),
		state.executor.suspendedNode);
	return Common::kNoError;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
