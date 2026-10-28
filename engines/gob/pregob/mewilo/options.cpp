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

#include "gob/pregob/mewilo/options.h"

#include "common/config-manager.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "common/util.h"

#include "gui/ThemeEval.h"
#include "gui/widget.h"

namespace Gob {
namespace Mewilo {

MewiloOutputMode mewiloOutputMode(const Common::String &domain) {
	if (!ConfMan.hasKey("mewilo_output", domain))
		return kMewiloOriginal;
	const Common::String value = ConfMan.get("mewilo_output", domain);
	if (value == "corrected")
		return kMewiloCorrectedOriginal;
	if (value == "english")
		return kMewiloEnglish;
	if (value != "original")
		warning("Mewilo: unknown output choice '%s'; using original wording", value.c_str());
	return kMewiloOriginal;
}

MewiloPlatform mewiloPresentationPlatform(Common::Platform detected, bool thomsonTO) {
	const MewiloPlatform native = mewiloPlatformFromCommon(detected, thomsonTO);
	if (native != kMewiloPlatformAtariST && native != kMewiloPlatformAmiga)
		return native; // CPC/Thomson must keep their native geometry and resource paths.
	const Common::String value = ConfMan.hasKey("mewilo_presentation")
		? ConfMan.get("mewilo_presentation")
		: Common::String("detected");
	if (value == "atari")
		return kMewiloPlatformAtariST;
	if (value == "amiga")
		return kMewiloPlatformAmiga;
	if (value != "detected")
		warning("Mewilo: unknown presentation '%s'; using detected profile", value.c_str());
	return native;
}

class MewiloOptions : public GUI::OptionsContainerWidget {
public:
	MewiloOptions(GUI::GuiObject *boss, const Common::String &name, const Common::String &target) :
		OptionsContainerWidget(boss, name, "MewiloOptions", target), _wording(this),
		_presentation(this) {
		const Common::Platform p = Common::parsePlatform(ConfMan.get("platform", target));
		_full = p == Common::kPlatformAtariST || p == Common::kPlatformAmiga;
		new GUI::StaticTextWidget(widgetsBoss(), _dialogLayout + ".WordingLabel",
			_("Wording (takes effect next launch):"));
		new GUI::RadiobuttonWidget(widgetsBoss(), _dialogLayout + ".Original", &_wording,
			kMewiloOriginal, _("Original wording"));
		new GUI::RadiobuttonWidget(widgetsBoss(), _dialogLayout + ".Corrected", &_wording,
			kMewiloCorrectedOriginal, _("Corrected original wording"),
			_("Requires the matching mewilo-original.dat sidecar."));
		new GUI::RadiobuttonWidget(widgetsBoss(), _dialogLayout + ".English", &_wording,
			kMewiloEnglish, _("English"), _("Requires the matching mewilo-en.dat sidecar."));
		if (_full) {
			new GUI::StaticTextWidget(widgetsBoss(), _dialogLayout + ".ProfileLabel",
				_("Presentation (audio follows installed files):"));
			new GUI::RadiobuttonWidget(widgetsBoss(), _dialogLayout + ".Detected", &_presentation,
				0, _("Detected platform"));
			new GUI::RadiobuttonWidget(
				widgetsBoss(), _dialogLayout + ".Atari", &_presentation, 1, _("Atari ST"));
			new GUI::RadiobuttonWidget(
				widgetsBoss(), _dialogLayout + ".Amiga", &_presentation, 2, _("Amiga"));
		}
		_timing = new GUI::CheckboxWidget(widgetsBoss(), _dialogLayout + ".Timing",
			_("Improve scene readability"),
			_("Hold reviewed frames and text long enough to see them. Takes effect next launch; native delay conversions remain active."));
	}

	void load() override {
		_wording.setValue(mewiloOutputMode(_domain));
		const Common::String p = ConfMan.hasKey("mewilo_presentation", _domain)
			? ConfMan.get("mewilo_presentation", _domain)
			: Common::String();
		_presentation.setValue(p == "atari" ? 1 : p == "amiga" ? 2 : 0);
		_timing->setState(ConfMan.hasKey("mewilo_presentation_timing", _domain) &&
			ConfMan.getBool("mewilo_presentation_timing", _domain));
	}

	bool save() override {
		const char *const modes[] = {"original", "corrected", "english"};
		ConfMan.set("mewilo_output", modes[CLIP<int>(_wording.getValue(), 0, 2)], _domain);
		if (_full) {
			const char *const profiles[] = {"detected", "atari", "amiga"};
			ConfMan.set("mewilo_presentation", profiles[CLIP<int>(_presentation.getValue(), 0, 2)],
				_domain);
		}
		ConfMan.setBool("mewilo_presentation_timing", _timing->getState(), _domain);

		return true;
	}

	void setEnabled(bool enabled) override {
		_wording.setEnabled(enabled);
		_presentation.setEnabled(enabled && _full);
		_timing->setEnabled(enabled);
	}

protected:
	void defineLayout(GUI::ThemeEval &layout, const Common::String &name,
		const Common::String &overlay) const override {
		layout.addDialog(name, overlay)
			.addLayout(GUI::ThemeLayout::kLayoutVertical)
			.addPadding(0, 0, 0, 0);
		layout.addWidget("WordingLabel", "Checkbox");
		layout.addWidget("Original", "Checkbox")
			.addWidget("Corrected", "Checkbox")
			.addWidget("English", "Checkbox");
		if (_full) {
			layout.addSpace().addWidget("ProfileLabel", "Checkbox");
			layout.addWidget("Detected", "Checkbox")
				.addWidget("Atari", "Checkbox")
				.addWidget("Amiga", "Checkbox");
		}
		layout.addSpace().addWidget("Timing", "Checkbox").closeLayout().closeDialog();
	}

private:
	GUI::RadiobuttonGroup _wording, _presentation;
	GUI::CheckboxWidget *_timing;
	bool _full;
};

GUI::OptionsContainerWidget *createMewiloOptions(
	GUI::GuiObject *boss, const Common::String &name, const Common::String &target) {
	return new MewiloOptions(boss, name, target);
}
} // End of namespace Mewilo
} // End of namespace Gob
