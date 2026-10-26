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

#include "engines/advancedDetector.h"
#include "engines/dialogs.h"

#include "backends/keymapper/keymap.h"
#include "common/config-manager.h"
#include "common/hashmap.h"
#include "common/platform.h"

#include "common/translation.h"

#include "gui/ThemeEval.h"
#include "gui/widget.h"

#include "gob/gameidtotype.h"
#include "gob/gob.h"
#include "gob/pregob/emmanuelle/input_keymaps.h"
#include "gob/pregob/emmanuelle/runtime.h"

// For struct GOBGameDescription.
#include "gob/detection/detection.h"

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_COPY_PROTECTION,
                {
                        _s("Enable copy protection"),
                        _s("Enable any copy protection that would otherwise be bypassed by default."),
                        "copy_protection",
                        false,
                        0,
                        0
                },
        },
	{
		GAMEOPTION_EMMANUELLE_ENHANCED_FIGHTS,
		{
			_s("Use selected graphics mode for fights"),
			_s("Keep the selected DOS graphics mode during fight scenes instead "
			   "of using the original four-color mode."),
			"emmanuelle_enhanced_fights",
			false,
			0,
			0
		}
	},
#ifdef USE_TTS
	{
		GAMEOPTION_TTS,
		{
			_s("Enable Text to Speech"),
			_s("Use TTS to read text in the game (if TTS is available)"),
			"tts_enabled",
			false,
			0,
			0
		}
	},
#endif

	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

class EmmanuelleOptionsWidget : public GUI::ExtraGuiOptionsWidget {
	enum {
		kSpeedChangedCmd = 'ESCH'
	};

public:
	EmmanuelleOptionsWidget(GUI::GuiObject *boss, const Common::String &name,
			const Common::String &domain, const ExtraGuiOptions &options) :
		GUI::ExtraGuiOptionsWidget(boss, name, domain, options),
		_optionCount(options.size()) {
		// The options container owns its child widgets.
		_speedLabel = nullptr;
		_speedSlider = nullptr;

		_speedLabel = new GUI::StaticTextWidget(widgetsBoss(),
			_dialogLayout + ".AnimationSpeedLabel", _("Animation speed: Default"));
		_speedSlider = new GUI::SliderWidget(widgetsBoss(),
			_dialogLayout + ".AnimationSpeed",
			_("Adjusts gameplay steps and timed pauses. The intro and conversation choices keep their timing."),
			kSpeedChangedCmd);
		// Larger factors add longer waits; the slider runs from slow to fast.
		_speedSlider->setMinValue(0);
		_speedSlider->setMaxValue(5);
		_speedSlider->setValue(3);
	}

	void load() override {
		GUI::ExtraGuiOptionsWidget::load();
		_speedSlider->setValue(5 - Gob::Emmanuelle::normalizeAnimationSpeed(
			ConfMan.getInt("emmanuelle_animation_speed", _domain)));
		updateSpeedLabel();
	}

	bool save() override {
		GUI::ExtraGuiOptionsWidget::save();
		ConfMan.setInt("emmanuelle_animation_speed", 5 - _speedSlider->getValue(), _domain);
		return true;
	}

	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == kSpeedChangedCmd)
			updateSpeedLabel();
		else
			GUI::ExtraGuiOptionsWidget::handleCommand(sender, cmd, data);
	}

protected:
	void defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName,
			const Common::String &overlayedLayout) const override {
		layouts.addDialog(layoutName, overlayedLayout)
			.addLayout(GUI::ThemeLayout::kLayoutVertical)
				.addPadding(0, 0, 0, 0);
		for (uint i = 0; i < _optionCount; ++i)
			layouts.addWidget(Common::String::format("customOption%uCheckbox", i + 1), "Checkbox");
		layouts.addSpace(layouts.getVar("Globals.Layout.Spacing"))
				.addWidget("AnimationSpeedLabel", "", -1, layouts.getVar("Globals.Line.Height"))
				.addWidget("AnimationSpeed", "Slider")
			.closeLayout()
		.closeDialog();
	}

private:
	void updateSpeedLabel() {
		static const char *const names[] = {
			_s("Fastest"), _s("Fast"), _s("Default"),
			_s("Relaxed"), _s("Slow"), _s("Slowest")
		};
		_speedLabel->setLabel(Common::U32String::format(_("Animation speed: %S"),
			_(names[5 - _speedSlider->getValue()]).c_str()));
	}

	const uint _optionCount;
	GUI::StaticTextWidget *_speedLabel;
	GUI::SliderWidget *_speedSlider;
};

class GobMetaEngine : public AdvancedMetaEngine<Gob::GOBGameDescription> {
public:
	const char *getName() const override {
		return "gob";
	}

	bool hasFeature(MetaEngineFeature f) const override;
	void registerDefaultSettings(const Common::String &target) const override;
	GUI::OptionsContainerWidget *buildEngineOptionsWidget(GUI::GuiObject *boss,
		const Common::String &name, const Common::String &target) const override;
	Common::KeymapArray initKeymaps(const char *target) const override;
	SaveStateList listSaves(const char *target) const override;
	bool removeSaveState(const char *target, int slot) const override;
	SaveStateDescriptor querySaveMetaInfos(const char *target,
		int slot) const override;
	Common::String getSavegameFile(int saveGameIdx,
		const char *target = nullptr) const override;
	int getAutosaveSlot() const override { return 0; }

	Common::Error createInstance(OSystem *syst, Engine **engine, const Gob::GOBGameDescription *desc) const override;

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return optionsList;
	}
};

void GobMetaEngine::registerDefaultSettings(const Common::String &target) const {
	MetaEngine::registerDefaultSettings(target);
	ConfMan.registerDefault("emmanuelle_animation_speed", 2);
}

GUI::OptionsContainerWidget *GobMetaEngine::buildEngineOptionsWidget(
		GUI::GuiObject *boss, const Common::String &name, const Common::String &target) const {
	if (!target.empty() && getGameId(target.c_str()) == "emmanuelle")
		return new EmmanuelleOptionsWidget(boss, name, target, getExtraGuiOptions(target));
	return MetaEngine::buildEngineOptionsWidget(boss, name, target);
}

bool GobMetaEngine::hasFeature(MetaEngineFeature f) const {
	return f == kSupportsListSaves ||
		f == kSupportsLoadingDuringStartup ||
		f == kSupportsDeleteSave ||
		f == kSavesSupportMetaInfo ||
		f == kSavesSupportThumbnail ||
		f == kSavesSupportCreationDate ||
		f == kSavesSupportPlayTime ||
		f == kSimpleSavesNames ||
		f == kSavesUseExtendedFormat;
}

SaveStateList GobMetaEngine::listSaves(const char *target) const {
	if (!target || getGameId(target) != "emmanuelle")
		return SaveStateList();
	return MetaEngine::listSaves(target);
}

bool GobMetaEngine::removeSaveState(const char *target, int slot) const {
	return target && getGameId(target) == "emmanuelle" &&
		MetaEngine::removeSaveState(target, slot);
}

SaveStateDescriptor GobMetaEngine::querySaveMetaInfos(
		const char *target, int slot) const {
	if (!target || getGameId(target) != "emmanuelle")
		return SaveStateDescriptor();
	return MetaEngine::querySaveMetaInfos(target, slot);
}

Common::String GobMetaEngine::getSavegameFile(int saveGameIdx,
		const char *target) const {
	if (target && getGameId(target) == "emmanuelle")
		return MetaEngine::getSavegameFile(saveGameIdx, target);
	if (!target)
		target = getName();
	return saveGameIdx == kSavegameFilePattern
		? Common::String::format("%s.s##", target)
		: Common::String::format("%s.s%02d", target, saveGameIdx);
}

Common::KeymapArray GobMetaEngine::initKeymaps(const char *target) const {
	if (!target || getGameId(target) != "emmanuelle")
		return MetaEngine::initKeymaps(target);

	const Common::Platform platform = Common::parsePlatform(
		ConfMan.get("platform", target));
	return Gob::Emmanuelle::createEmmanuelleKeymaps(platform);
}

bool Gob::GobEngine::hasFeature(EngineFeature f) const {
	// Only Woodruff reads the subtitles setting (Init_v4::updateConfig)
	return
		(f == kSupportsReturnToLauncher) ||
		(f == kSupportsSubtitleOptions && _gameType == kGameTypeWoodruff) ||
		((f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime) && _gameType == kGameTypeEmmanuelle);
}

Common::Error GobMetaEngine::createInstance(OSystem *syst, Engine **engine, const Gob::GOBGameDescription *gd) const {
	Gob::GobEngine *gobEngine = new Gob::GobEngine(syst);
	*engine = gobEngine;
	gobEngine->initGame(gd);
	return Common::kNoError;
}


#if PLUGIN_ENABLED_DYNAMIC(GOB)
	REGISTER_PLUGIN_DYNAMIC(GOB, PLUGIN_TYPE_ENGINE, GobMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(GOB, PLUGIN_TYPE_ENGINE, GobMetaEngine);
#endif

namespace Gob {

GameType GobEngine::getGameType(const char *gameId) const {
	const GameIdToType *gameInfo = gameIdToType;

	while (gameInfo->gameId != nullptr) {
		if (!strcmp(gameId, gameInfo->gameId))
			return gameInfo->gameType;
		gameInfo++;
	}

	error("Unknown game ID: %s", gameId);
}

bool GobEngine::gameTypeHasAddOns() const {
	return  getGameType() == kGameTypeAdibou1 ||
			getGameType() == kGameTypeAdibou2 ||
			getGameType() == kGameTypeAdi2 ||
			getGameType() == kGameTypeAdi4;
}


// Accelerator, to discard some directories we know have no chance to be add-ons
bool GobEngine::dirCanBeGameAddOn(const Common::FSDirectory &dir) const {
	if (getGameType() == kGameTypeAdibou2)
		return dir.hasFile("intro_ap.stk");

	return true;
}

// To display a warning if a directory likely to be an add-on does not match anything
bool GobEngine::dirMustBeGameAddOn(const Common::FSDirectory &dir) const {
	if (getGameType() == kGameTypeAdibou2)
		return dir.hasFile("intro_ap.stk");

	return false;
}

void GobEngine::initGame(const GOBGameDescription *gd) {
	if (gd->startTotBase == nullptr)
		_startTot = "intro.tot";
	else
		_startTot = gd->startTotBase;

	if (gd->startStkBase == nullptr)
		_startStk = "intro.stk";
	else
		_startStk = gd->startStkBase;

	_demoIndex = gd->demoIndex;

	_gameType = getGameType(gd->desc.gameId);
	_features = gd->features;
	_language = gd->desc.language;
	_platform = gd->desc.platform;
	_extra = gd->desc.extra;

	_enableAdibou2FreeBananasWorkaround = gd->desc.flags & GF_ENABLE_ADIBOU2_FREE_BANANAS_WORKAROUND;
	_enableAdibou2FlowersInfiniteLoopWorkaround = gd->desc.flags & GF_ENABLE_ADIBOU2_FLOWERS_INFINITE_LOOP_WORKAROUND;
}

} // End of namespace Gob
