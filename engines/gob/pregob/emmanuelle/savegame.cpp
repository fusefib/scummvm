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
	const char *extension = _platformTag == kOldTotPlatformAmiga
		? ".TOA" : ".TOT";
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
	state.nativeState.sampleBank = _sampleBank;
	state.nativeState.sampleCount = _sampleCount;
	state.nativeState.initialInitComplete =
		_platformTag == kOldTotPlatformDOS || _initialInitComplete;
	state.nativeState.skipIntermediateSequenceNodes = _skipIntermediateSequenceNodes;
	state.nativeState.colorCycleEnabled = _colorCycleEnabled;
	state.nativeState.colorCycleCounter = _colorCycleCounter;

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

} // End of namespace Emmanuelle
} // End of namespace Gob
