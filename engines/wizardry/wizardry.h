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
 */

#ifndef WIZARDRY_WIZARDRY_H
#define WIZARDRY_WIZARDRY_H

#include "audio/mixer.h"
#include "engines/engine.h"
#include "wizardry/campaign.h"
#include "wizardry/detection.h"
#include "wizardry/vm/machine.h"

namespace Audio {
class QueuingAudioStream;
}

namespace Wizardry {

class WizardryEngine : public Engine, public MachineObserver {
public:
	WizardryEngine(OSystem *system, const WizardryGameDescription *description);
	~WizardryEngine() override;
	Common::Error run() override;
	bool hasFeature(EngineFeature feature) const override;
	bool canSaveGameStateCurrently(Common::U32String *message = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *message = nullptr) override;
	bool canSaveAutosaveCurrently() override;
	Common::String getSaveStateName(int slot) const override;
	Common::Error loadGameState(int slot) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	void pauseEngineIntern(bool pause) override;

private:
	const WizardryGameDescription *_description;
	PCodeMachine _machine;
	byte _pixels[640 * 200];
	uint16 _screenWidth = 320;
	uint32 _lastClock = 0;
	bool _ready = false;
	bool _loadingAutosave = false;
	bool _paused = false;
	bool _soundActive = false;
	uint64 _soundInstruction = 0;
	Audio::SoundHandle _speakerHandle;
	// Engine-owned; the mixer borrows it until stopSound detaches the handle.
	Audio::QueuingAudioStream *_speakerStream = nullptr;
	CampaignStore *_campaign = nullptr;
	bool _campaignDirty = false, _campaignWarned = false;
	bool _campaignRecoveryPending = false;
	bool _campaignSelectionPending = false;
	uint32 _nextCampaignRetry = 0;
	Common::Array<byte> _scenarioBackup;
	Common::String _scenarioError;
	bool _scenarioFailed = false;
	bool _scenarioStarted = false;
	void present();
	void processEvents();
	void updateTime();
	void startSound();
	void stopSound();
	void onDiskWrite() override { _campaignDirty = true; }
	bool onStateLoad(const PCodeMachine &candidate, Common::String &error) override;
	bool canCreateScenarioDisk() const override { return !_scenarioBackup.empty(); }
	bool onScenarioDiskCreated(const Common::Array<byte> &master,
							   const Common::Array<byte> &save, Common::String &error) override;
	bool preserveScenarioSession(Common::String &error);
	void restoreScenarioSession();
	CampaignStore::Identity campaignIdentity(const PCodeMachine &machine) const;
	bool configureCampaign(Common::String &error);
	bool flushCampaign(Common::String &error, bool force = false);
	void campaignWarning(const Common::String &message);
};

} // namespace Wizardry

#endif
