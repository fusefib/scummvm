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

#ifndef GOB_PREGOB_EMMANUELLE_EMMANUELLE_H
#define GOB_PREGOB_EMMANUELLE_EMMANUELLE_H

#include "gob/pregob/pregob.h"

#include "gob/pregob/emmanuelle/active.h"
#include "gob/pregob/emmanuelle/debugger.h"
#include "gob/pregob/emmanuelle/executor.h"
#include "gob/pregob/emmanuelle/gameflow.h"
#include "gob/pregob/emmanuelle/interpreter.h"
#include "gob/pregob/emmanuelle/input_keymaps.h"
#include "gob/pregob/emmanuelle/fight.h"
#include "gob/pregob/emmanuelle/platform.h"
#include "gob/pregob/emmanuelle/renderer.h"
#include "gob/pregob/emmanuelle/runtime.h"
#include "gob/pregob/emmanuelle/saveload.h"

namespace Common {
class SeekableReadStream;
}

namespace Gob {

class Font;

namespace Emmanuelle {

class OldIxxBank;
class OldTotData;
struct EmmanuellePreparedRestore;

class PreGob_Emmanuelle : public PreGob,
						  public ModuleHost,
						  public OldTotExecHost,
						  public OldTotHost,
						  public FightRandomSource,
						  public OldTotDebugProvider {
public:
	PreGob_Emmanuelle(GobEngine *vm);
	~PreGob_Emmanuelle() override;

	void run() override;
	bool canSaveGameState(Common::U32String *msg = nullptr) const override;
	bool canLoadGameState(Common::U32String *msg = nullptr) const override;
	Common::Error saveGameStream(Common::WriteStream &stream,
		bool isAutosave = false) const override;
	Common::Error loadGameStream(Common::SeekableReadStream &stream) override;
	void handleCustomAction(Common::CustomEventType action) override;

	bool loadModule(ModuleId moduleId,
					bool usesLocalGraphics) override;
	bool executeLoadedModule() override;
	void unloadModule() override;
	bool shouldQuit() const override;
	ModuleId loadedModule() const override {
		return _currentModule;
	}
	bool loadedModuleUsesLocalGraphics() const override {
		return _currentUsesLocalGraphics;
	}

	uint16 randomBounded(uint16 upperExclusive) override;
	uint16 fightRandom(uint16 upperExclusive) override {
		return randomBounded(upperExclusive);
	}
	void schedulerTiming(byte period, bool timingVariant, uint16 &reload,
						 uint16 &initialCountdown) override;
	bool schedulerChecksZeroBeforeDecrement() const override {
		return Emmanuelle::schedulerChecksZeroBeforeDecrement(_platformTag);
	}
	int16 chooseInputRegion(uint16 nodeOffset, byte count,
							bool initialEntry, bool reentering) override;
	void defineCursor(uint16 descriptorOffset) override;
	void drawNode(uint16 nodeOffset) override;
	void textNode(uint16 nodeOffset) override;
	void animationNode(uint16 nodeOffset) override;
	void rememberNode24(uint16 nodeOffset) override { _node24Offset = nodeOffset; }
	bool shouldAbortExecution() const override;

	void saveScreenRect(uint16 x, uint16 y, uint16 width,
						uint16 height) override;
	void restoreScreenRect() override;
	void waitReturn() override;
	void waitInputRelease() override;
	void waitOpcode03(byte arg) override;
	// Sleep for the requested duration; event processing adds to elapsed time.
	void delay(uint16 milliseconds) override;
	void fillScreen(byte color) override;
	void dispatchScheduled(uint16 target) override;
	void runFight(uint16 leftMeterBonus, uint16 rightMeterBonus,
								 uint16 leftDamageOption, uint16 rightDamageOption,
								 uint16 leftBaseMeter, uint16 rightBaseMeter) override;
	bool handleNativeFF0B(byte command,
						  OldTotRuntimeState &runtimeState) override;
	void reinitializeVideoBackend(byte videoMode) override;

	void exportDiagnosticSnapshot(OldTotDiagnosticSnapshot &out) const override;
	bool queryProtectionHint(ProtectionChallenge &out) const override;
	void setDiagnosticTraceEnabled(bool enabled) override;
	bool diagnosticTraceEnabled() const override { return _diagnosticTraceEnabled; }

private:
	Common::SeekableReadStream *openLoose(const Common::String &name) const;
	OldTotData *loadScriptResource(const Common::String &name,
		Common::String &reason, uint32 *sourceSize = nullptr,
		byte *sourceMD5 = nullptr) const;
	bool loadSharedAssets();
	bool loadRecupAssets();
	void clearModuleObjects();

	void presentFrame(bool waitForFrame, bool force = false);
	void processGameEvents();
	// Count event-processing time toward the minimum elapsed duration.
	void waitForMinimumDuration(uint32 startTime, uint32 minimumMilliseconds);
	bool initialTomahawkPresentationVisible() const;
	void noteInitialTomahawkVisible();
	void ensureInitialTomahawkHandoff();
	void completeInitialTomahawkHandoff(ModuleId moduleId);
	void beginIntroFastForward();
	void finishIntro();
	int16 takePendingInput(int16 &mouseX, int16 &mouseY,
						  MouseButtons &mouseButtons);
	int16 pollGameInput(int16 &mouseX, int16 &mouseY,
						  MouseButtons &mouseButtons);
	int16 pollInterpreterInput(int16 &mouseX, int16 &mouseY,
							   MouseButtons &mouseButtons);
	void applyInputBoundary(const InputBoundary &boundary);
	void discardPendingInput();
	void serviceInterpreterTimer();
	bool inputIdle(int16 key, MouseButtons mouseButtons) const;
	bool handleProtectionTextNode(uint16 nodeOffset);
	void renderTextPlan(uint16 nodeOffset);
	bool renderAnimationFrame(const OldTotAnimationSetup &setup, uint16 step,
							  byte frame, int16 baseX, int16 baseY);
	bool buildFightActor(bool rightActor, const FightActor &actor,
						 SurfacePtr &cell, FightActorPlacement &placement);
	bool renderFightActors(const FightState &state);
	void playDosFightTone(uint16 frequency);
	void setAudioState(OldTotAudioKind kind, int16 slot,
							uint16 frequency, int16 repeatCount,
							bool blocking, bool active);
	void serviceWaitColorCycle();
	void setInputContext(EmmanuelleInputContext context);
	EmmanuelleSaveBoundaryState saveBoundaryState() const;
	bool safeSaveBoundary(Common::U32String *msg) const;
	bool computeModuleIdentity(ModuleId module, uint32 &size,
		byte digest[16]) const;
	bool exportSaveState(EmmanuelleSaveState &state) const;
	bool prepareRestore(const EmmanuelleSaveState &state,
		EmmanuellePreparedRestore &prepared, Common::String &reason);
	void commitRestore(const EmmanuelleSaveState &state,
		EmmanuellePreparedRestore &prepared);
	void resetInitialStateAfterLoad();
	bool moduleUsesLocalGraphics(ModuleId module) const;

	uint16 _variables[512];
	OldTotRuntimeState _runtimeState;
	OldTotData *_tot;
	OldTotData *_recupTot;
	OldIxxBank *_sharedI01;
	OldIxxBank *_localI00;
	OldTotVM *_scriptVM;
	OldTotExecutor *_executor;
	OldTotRenderer _renderer;
	ActiveSpriteLayer _active;
	Font *_fonts[2];
	Common::Array<uint16> _recupRoots;
	Common::String _moduleFileStem;
	ModuleId _currentModule;
	bool _currentUsesLocalGraphics;
	uint16 _node24Offset;
	uint32 _randomState;
	OldTotPlatformTag _platformTag;
	bool _enhancedDosFights;
	uint32 _schedulerAccumulator;
	uint32 _schedulerDebt;
	InputLatch _pendingInput;
	IntroSkipState _introSkip;
	// Remains active through protection and the title at the end of fresh INIT.
	bool _initialInitPresentation;
	enum InitialTitleState {
		kInitialTitleNone,
		kInitialTitleDrawn,
		kInitialTitleVisible
	};
	InitialTitleState _initialTitleState;
	uint32 _initialTomahawkVisibleStart;
	Common::Rect _dosInitialFrameCoverage;
	bool _dosInitialFrameHasCoverage;
	bool _dosInitialFramePresented;
	bool _cursorOwned;
	bool _cursorDefined;
	uint16 _cursorDescriptor;
	EmmanuelleInputContext _inputContext;
	bool _fightActive;
	bool _mainMenuActionThisPoll;
	bool _openMainMenuAfterSuspend;
	bool _diagnosticTraceEnabled;
	OldTotAudioKind _audioKind;
	int16 _audioSlot;
	uint16 _audioFrequency;
	int16 _audioRepeatCount;
	bool _audioActive;
	bool _audioBlocking;


};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_EMMANUELLE_H
