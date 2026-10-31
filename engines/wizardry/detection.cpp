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

#include "wizardry/detection.h"
#include "base/plugins.h"

static const PlainGameDescriptor kWizardryGames[] = {
	{"wizardry1", "Wizardry I: Proving Grounds of the Mad Overlord"},
	{"wizardry2", "Wizardry II: The Knight of Diamonds"},
	{"wizardry3", "Wizardry III: Legacy of Llylgamyn"},
	{"wizardry4", "Wizardry IV: The Return of Werdna"},
	{"wizardry5", "Wizardry V: Heart of the Maelstrom"},
	{nullptr, nullptr}
};

static const Wizardry::WizardryGameDescription kWizardryDescriptions[] = {
	{{"wizardry1", "",
	  AD_ENTRY1s("WIZ1.DSK", "c52895175e3aac4d9ac56f00e25c229c", 327680),
	  Common::EN_ANY, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO3(GUIO_NOMUSIC, GUIO_NOSPEECH, GAMEOPTION_ORIGINAL_SAVE_DISK)},
	 1},
	{{"wizardry2", "",
	  AD_ENTRY1s("WIZ2.DSK", "a0e0a2a211355cc5bb841177506f216f", 327680),
	  Common::EN_ANY, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO3(GUIO_NOMUSIC, GUIO_NOSPEECH, GAMEOPTION_ORIGINAL_SAVE_DISK)},
	 2},
	{{"wizardry3", "",
	  AD_ENTRY1s("WIZ3.DSK", "cf5ac7f4e5ccbf26155d8cf2ebdd3611", 327680),
	  Common::EN_ANY, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO3(GUIO_NOMUSIC, GUIO_NOSPEECH, GAMEOPTION_ORIGINAL_SAVE_DISK)},
	 3},
	{{"wizardry4", "",
	  AD_ENTRY1s("WIZ4.DSK", "37ef0d3ba2fd641b170148b2f4c19f6a", 327680),
	  Common::EN_ANY, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO2(GUIO_NOMUSIC, GUIO_NOSPEECH)},
	 4},
	{{"wizardry5", "",
	  AD_ENTRY1s("WIZ5.DSK", "932277b230b5f35fdd9497415823ba96", 655360),
	  Common::EN_ANY, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO2(GUIO_NOMUSIC, GUIO_NOSPEECH)},
	 5},
	{AD_TABLE_END_MARKER, 0}
};

class WizardryMetaEngineDetection : public AdvancedMetaEngineDetection<Wizardry::WizardryGameDescription> {
public:
	WizardryMetaEngineDetection() : AdvancedMetaEngineDetection(kWizardryDescriptions, kWizardryGames) {
		_maxScanDepth = 2;
	}
	const char *getName() const override { return "wizardry"; }
	const char *getEngineName() const override { return "Wizardry I-V"; }
	const char *getOriginalCopyright() const override { return "Wizardry (C) 1981-1988 Sir-Tech Software"; }
};

REGISTER_PLUGIN_STATIC(WIZARDRY_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, WizardryMetaEngineDetection);
