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

#include "wizardry/wizardry.h"
#include "common/algorithm.h"
#include "common/archive.h"
#include "common/savefile.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "gui/dialog.h"
#include "gui/gui-manager.h"
#include "gui/message.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"
#include "gui/widgets/groupedlist.h"
#include "wizardry/data/ucsd_disk.h"
#include "wizardry/transfer_import.h"
#include "wizardry/vm/codefile.h"

namespace Wizardry {

namespace {

const char *const kSourceNames[] = { "I", "II", "III" };
const char *const kSourceTitles[] = {
	"Wizardry I: Proving Grounds of the Mad Overlord (DOS/English)",
	"Wizardry II: The Knight of Diamonds (DOS/English)",
	"Wizardry III: Legacy of Llylgamyn (DOS/English)"
};

Common::U32String campaignLabel(const TransferCampaign &campaign) {
	return Common::U32String::format(_("Playthrough %u: Wizardry %s"), unsigned(campaign.identity.campaign) + 1,
		kSourceNames[campaign.identity.gameNumber - 1]);
}

Common::U32String campaignStorage(const TransferCampaign &campaign) {
	switch (campaign.identity.profile) {
	case CampaignStore::kCompatibility:
		return _("Single working disk");
	case CampaignStore::kOriginal:
		return _("Single working disk (original DOS memory)");
	case CampaignStore::kPaired:
		return _("Separate program and save disks");
	}
	return Common::U32String();
}

struct ImportChoice {
	enum Kind { kCampaign, kDisk };
	Kind kind = kCampaign;
	byte source = 0;
	TransferCampaign campaign;
	Common::ArchiveMemberPtr member;
	Common::U32StringArray locations;
	Common::U32String label;
	Common::U32String filename;
	Common::U32String location;
};

bool readDisk(const Common::ArchiveMemberPtr &member, Common::Array<byte> &bytes,
		Common::String &error) {
	Common::SeekableReadStream *in = member->createReadStream();
	bool ok = in && (in->size() == 640 * 512 || in->size() == 1280 * 512);
	if (ok) {
		bytes.resize(uint32(in->size()));
		ok = in->read(bytes.data(), bytes.size()) == bytes.size() && !in->err();
	}
	delete in;
	if (!ok)
		error = "The selected source disk could not be read completely";
	return ok;
}

struct DiskOrder {
	static int rank(const Common::String &name) {
		return name.hasPrefixIgnoreCase("SAVE") ? 0 : name.hasPrefixIgnoreCase("WIZ") ? 1 : 2;
	}
	bool operator()(const Common::ArchiveMemberDetails &a, const Common::ArchiveMemberDetails &b) const {
		const Common::String aName = a.arcMember->getFileName(), bName = b.arcMember->getFileName();
		if (rank(aName) != rank(bName))
			return rank(aName) < rank(bName);
		const int archive = a.arcName.compareToIgnoreCase(b.arcName);
		if (archive)
			return archive < 0;
		return a.arcMember->getPathInArchive().toString('/').compareToIgnoreCase(
			b.arcMember->getPathInArchive().toString('/')) < 0;
	}
};

Common::Array<ImportChoice> listSources(Common::SaveFileManager &manager, byte destination) {
	Common::Array<ImportChoice> choices;
	const byte maxSource = TransferDisks::sourceCount(destination);
	const Common::Array<TransferCampaign> campaigns = TransferImport::listCampaigns(manager, destination);
	for (uint i = 0; i < campaigns.size(); ++i) {
		ImportChoice choice;
		choice.source = campaigns[i].identity.gameNumber;
		choice.campaign = campaigns[i];
		choice.label = campaignLabel(choice.campaign);
		CampaignStore store(manager, choice.campaign.target, choice.campaign.identity);
		choice.filename = store.filename(0);
		choices.push_back(choice);
	}
	// Different mounted archives can contain different disks named SAVE1.DSK.
	Common::ArchiveMemberDetailsList members;
	SearchMan.listMatchingMembers(members, Common::Path("*.DSK"), true);
	Common::Array<Common::ArchiveMemberDetails> ordered;
	for (Common::ArchiveMemberDetailsList::const_iterator i = members.begin(); i != members.end(); ++i)
		ordered.push_back(*i);
	Common::sort(ordered.begin(), ordered.end(), DiskOrder());
	const uint firstDisk = choices.size();
	Common::Array<Common::Array<byte> > diskImages;
	for (uint i = 0; i < ordered.size(); ++i) {
		const Common::ArchiveMemberDetails &member = ordered[i];
		Common::Array<byte> bytes, program, normalized;
		Common::String error;
		UcsdDisk disk;
		if (!readDisk(member.arcMember, bytes, error) || !disk.loadBytes(bytes, error) ||
			!disk.readFile("SYSTEM.PASCAL", program, error))
			continue;
		const byte game = TransferDisks::scenario(wizardryCrc32(program.data(), program.size()));
		if (!game || game > maxSource || !TransferImport::prepareImage(game, bytes, normalized, error))
			continue;
		const Common::U32String location = Common::U32String::format("%s: %s", member.arcName.c_str(),
			member.arcMember->getPathInArchive().toString('/').c_str());
		uint duplicate = 0;
		while (duplicate < diskImages.size()) {
			if (choices[firstDisk + duplicate].source == game && diskImages[duplicate].size() == normalized.size() &&
				!memcmp(diskImages[duplicate].data(), normalized.data(), normalized.size()))
				break;
			++duplicate;
		}
		if (duplicate < diskImages.size()) {
			choices[firstDisk + duplicate].locations.push_back(location);
			continue;
		}
		ImportChoice choice;
		choice.kind = ImportChoice::kDisk;
		choice.source = game;
		choice.member = member.arcMember;
		choice.label = choice.filename = member.arcMember->getFileName();
		const Common::Path parent = member.arcMember->getPathInArchive().getParent();
		choice.location = Common::Path(member.arcName).join(parent).toString('/');
		choice.locations.push_back(location);
		choices.push_back(choice);
		diskImages.push_back(Common::move(normalized));
	}
	return choices;
}

Common::U32String sourceDetails(Common::SaveFileManager &manager, const ImportChoice &choice) {
	Common::U32String text = Common::U32String::format("Wizardry %s\n%S\n", kSourceNames[choice.source - 1], choice.label.c_str());
	if (choice.kind == ImportChoice::kCampaign) {
		const TransferCampaign &campaign = choice.campaign;
		CampaignStore store(manager, campaign.target, campaign.identity);
		text += Common::U32String::format(_("Target: %s\nPlaythrough: %u\nStorage: %S\n"),
			campaign.target.c_str(), unsigned(campaign.identity.campaign) + 1, campaignStorage(campaign).c_str());
		text += _("ScummVM keeps your progress in private working disk copies. Original game files are not changed.");
		text += Common::U32String("\n\n");
		text += _("Campaign files (alternating copies):");
		text += Common::U32String::format("\n%s\n%s\n", store.filename(0).c_str(), store.filename(1).c_str());
	} else {
		text += _("Game-folder disk files may contain defaults or older progress, rather than your current ScummVM playthrough.");
		text += Common::U32String("\n\n");
		text += _("File locations (identical import data):");
		for (uint i = 0; i < choice.locations.size(); ++i)
			text += "\n" + choice.locations[i];
	}
	return text;
}

enum { kImportCmd = 'impt', kDetailsCmd = 'dtls' };

class ImportDetailsDialog : public GUI::Dialog {
	Common::U32String _text;
	GUI::ListWidget *_list;
public:
	ImportDetailsDialog(const Common::U32String &text) : GUI::Dialog("WizardryImportDetails"), _text(text) {
		new GUI::StaticTextWidget(this, _name + ".Title", _("Source details"));
		_list = new GUI::ListWidget(this, _name + ".Text");
		_list->setEditable(false);
		_list->setNumberingMode(GUI::kListNumberingOff);
		new GUI::ButtonWidget(this, _name + ".Close", _("Close"), Common::U32String(), GUI::kCloseCmd);
	}
	void reflowLayout() override {
		GUI::ThemeEval &layout = *g_gui.xmlEval();
		const int line = layout.getVar("Globals.Line.Height");
		layout.addDialog(_name, "screen", -1, -1, 8)
			.addLayout(GUI::ThemeLayout::kLayoutVertical, 4).addPadding(4, 4, 4, 4)
				.addWidget("Title", "", -1, line)
				.addWidget("Text", "")
				.addWidget("Close", "Button")
			.closeLayout().closeDialog();
		GUI::Dialog::reflowLayout();
		Common::U32StringArray lines;
		g_gui.getFont().wordWrapText(_text, MAX(1, _list->getWidth() - layout.getVar("Globals.Scrollbar.Width") - 16), lines);
		for (uint i = 0; i < lines.size(); ++i)
			lines[i] = GUI::ListWidget::escapeString(lines[i]);
		_list->setList(lines);
	}
};

class TransferSourceDialog : public GUI::Dialog {
	const Common::Array<ImportChoice> &_choices;
	Common::SaveFileManager &_manager;
	GUI::GroupedListWidget *_list;
	GUI::StaticTextWidget *_info, *_location, *_filename;
	GUI::ButtonWidget *_import, *_details;
public:
	TransferSourceDialog(Common::SaveFileManager &manager, const Common::Array<ImportChoice> &choices)
		: GUI::Dialog("WizardryImport"), _choices(choices), _manager(manager) {
		new GUI::StaticTextWidget(this, _name + ".Title", _("Import characters"));
		new GUI::StaticTextWidget(this, _name + ".Heading", _("Choose a source to import characters."));
		_list = new GUI::GroupedListWidget(this, _name + ".List");
		_list->setEditable(false);
		_list->setNumberingMode(GUI::kListNumberingOff);
		_info = new GUI::StaticTextWidget(this, _name + ".Info", Common::U32String());
		_location = new GUI::StaticTextWidget(this, _name + ".Location", Common::U32String());
		_filename = new GUI::StaticTextWidget(this, _name + ".File", Common::U32String());
		_details = new GUI::ButtonWidget(this, _name + ".Details", _("Details"), Common::U32String(), kDetailsCmd);
		new GUI::ButtonWidget(this, _name + ".Cancel", _("Cancel"),
			_("Cancel the import and return to Utilities. Escape does the same."), GUI::kCloseCmd);
		_import = new GUI::ButtonWidget(this, _name + ".Import", _("Import"), Common::U32String(), kImportCmd);
		Common::U32StringArray labels, games;
		for (uint i = 0; i < _choices.size(); ++i) {
			labels.push_back(GUI::ListWidget::escapeString(_choices[i].label));
			games.push_back(Common::U32String(kSourceTitles[_choices[i].source - 1]));
		}
		_list->setList(labels);
		_list->setAttributeValues(games);
		_list->groupByAttribute();
		updateSelection();
	}
	int selectedChoice() const {
		const int selected = _list->getSelected();
		return selected >= 0 && uint(selected) < _choices.size() ? selected : -1;
	}
	void updateSelection() {
		const int selected = selectedChoice();
		_import->setEnabled(selected >= 0);
		_details->setEnabled(selected >= 0);
		Common::U32String info = _("Select a source to see its details.");
		Common::U32String location, filename;
		if (selected >= 0) {
			const ImportChoice &choice = _choices[selected];
			if (choice.kind == ImportChoice::kCampaign) {
				info = _("Info: Use this playthrough's saved roster.");
				location = Common::U32String::format(_("Launcher target id: %s"), choice.campaign.target.c_str());
			} else {
				info = _("Info: Not recommended. Default game file.");
				location = Common::U32String::format(_("Path: %S"), choice.location.c_str());
			}
			filename = Common::U32String::format(_("File: %S"), choice.filename.c_str());
		}
		_info->setLabel(info);
		_info->setTooltip(info);
		_location->setLabel(location);
		_location->setTooltip(location);
		_filename->setLabel(filename);
		_filename->setTooltip(filename);
	}
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override {
		if (cmd == GUI::kCloseCmd) {
			setResult(-1);
			close();
		} else if (cmd == GUI::kListSelectionChangedCmd) {
			updateSelection();
		} else if (cmd == kDetailsCmd && selectedChoice() >= 0) {
			ImportDetailsDialog details(sourceDetails(_manager, _choices[selectedChoice()]));
			details.runModal();
		} else if (cmd == kImportCmd || cmd == GUI::kListItemActivatedCmd || cmd == GUI::kListItemDoubleClickedCmd) {
			if (selectedChoice() >= 0) {
				setResult(selectedChoice());
				close();
			}
		} else {
			GUI::Dialog::handleCommand(sender, cmd, data);
		}
	}
	void reflowLayout() override {
		GUI::ThemeEval &layout = *g_gui.xmlEval();
		const int line = layout.getVar("Globals.Line.Height");
		layout.addDialog(_name, "screen", -1, -1, 4)
			.addLayout(GUI::ThemeLayout::kLayoutVertical, 4).addPadding(4, 4, 4, 4)
				.addWidget("Title", "", -1, line)
				.addWidget("Heading", "", -1, line)
				.addWidget("List", "")
				.addSpace(2)
				.addLayout(GUI::ThemeLayout::kLayoutVertical, 1).addPadding(0, 0, 0, 0)
					.addWidget("Info", "", -1, line)
					.addWidget("Location", "", -1, line)
					.addWidget("File", "", -1, line)
				.closeLayout()
				.addSpace(2)
				.addLayout(GUI::ThemeLayout::kLayoutHorizontal, 4).addPadding(0, 0, 0, 0)
					.addWidget("Details", "Button").addSpace()
					.addWidget("Cancel", "Button").addWidget("Import", "Button")
				.closeLayout()
			.closeLayout().closeDialog();
		GUI::Dialog::reflowLayout();
	}
};

bool chooseSource(Common::SaveFileManager &manager, byte destination, byte &source, Common::Array<byte> &image) {
	const Common::Array<ImportChoice> choices = listSources(manager, destination);
	if (choices.empty()) {
		GUI::MessageDialog(_("No compatible playthroughs or disk files were found. Returning to Utilities.")).runModal();
		return false;
	}
	TransferSourceDialog dialog(manager, choices);
	while (true) {
		const int selected = dialog.runModal();
		if (selected < 0 || uint(selected) >= choices.size())
			return false;
		const ImportChoice &choice = choices[selected];
		Common::Array<byte> candidate;
		Common::String error;
		bool ok;
		if (choice.kind == ImportChoice::kCampaign) {
			ok = TransferImport::loadCampaign(manager, choice.campaign, candidate, error);
		} else {
			Common::Array<byte> bytes;
			ok = readDisk(choice.member, bytes, error) &&
				TransferImport::prepareImage(choice.source, bytes, candidate, error);
		}
		if (!ok) {
			warning("Wizardry import: %s", error.c_str());
			GUI::MessageDialog(_("This source could not be read or is unsupported. Choose another source or cancel.")).runModal();
			continue;
		}
		const Common::U32String message =
			_("Import characters from this source? The source will not be changed. The game will ask which characters to transfer. Finish or cancel the import before saving.");
		GUI::MessageDialog confirm(message, _("Import"), _("Cancel"));
		if (confirm.runModal() != GUI::kMessageOK)
			continue;
		source = choice.source;
		image = Common::move(candidate);
		return true;
	}
}

} // namespace

MachineObserver::TransferSourceResult WizardryEngine::onTransferSourceRequest(byte destination, const TransferDisks &,
		byte &source, Common::Array<byte> &image) {
	// The VM cannot be replaced or suspended inside its source request callback.
	PauseToken pause = pauseEngine();
	_transferDialogOpen = true;
	present();
	const bool chosen = chooseSource(*_saveFileMan, destination, source, image);
	_transferDialogOpen = false;
	return chosen ? kTransferSelected : kTransferCancelled;
}

} // namespace Wizardry
