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
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/savefile.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "gui/chooser.h"
#include "gui/message.h"
#include "gui/widget.h"
#include "wizardry/data/ucsd_disk.h"
#include "wizardry/transfer_import.h"
#include "wizardry/vm/codefile.h"

namespace Wizardry {

namespace {

const char *const kSourceNames[] = { "I", "II", "III" };

class TransferSourceDialog : public GUI::ChooserDialog {
public:
	TransferSourceDialog() : GUI::ChooserDialog(_("Import characters: select a campaign or disk")) {
		GUI::ButtonWidget *cancel = static_cast<GUI::ButtonWidget *>(findWidget("Browser.Cancel"));
		cancel->setTooltip(_("Cancel the import and return to Utilities. Escape does the same."));
	}
};

Common::U32String campaignLabel(const TransferCampaign &campaign) {
	Common::String description = campaign.target;
	const Common::ConfigManager::Domain *domain = ConfMan.getDomain(campaign.target);
	if (domain && domain->contains("description") && !(*domain)["description"].empty())
		description = (*domain)["description"];
	Common::U32String mode;
	switch (campaign.identity.profile) {
	case CampaignStore::kCompatibility:
		mode = _("Compatibility disk");
		break;
	case CampaignStore::kOriginal:
		mode = _("Original memory, single disk");
		break;
	case CampaignStore::kPaired:
		mode = _("Original save disk");
		break;
	}
	return Common::U32String::format(_("Wizardry %s: campaign %u, %s [%s; %S]"),
		kSourceNames[campaign.identity.gameNumber - 1], unsigned(campaign.identity.campaign),
		description.c_str(), campaign.target.c_str(), mode.c_str());
}

struct ImportChoice {
	enum Kind { kCurrent, kCampaign, kDisk };
	Kind kind = kCurrent;
	byte source = 0;
	TransferCampaign campaign;
	Common::ArchiveMemberPtr member;
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

bool chooseSource(Common::SaveFileManager &manager, byte destination,
		const TransferDisks &current, byte &source, Common::Array<byte> &image) {
	Common::Array<ImportChoice> choices;
	Common::U32StringArray labels;
	const byte maxSource = TransferDisks::sourceCount(destination);
	for (byte game = 1; game <= maxSource; ++game) {
		if (!current.present(game))
			continue;
		ImportChoice choice;
		choice.source = game;
		choices.push_back(choice);
		labels.push_back(Common::U32String::format(_("Wizardry %s: current transfer disk"), kSourceNames[game - 1]));
	}
	const Common::Array<TransferCampaign> campaigns = TransferImport::listCampaigns(manager, destination);
	for (uint i = 0; i < campaigns.size(); ++i) {
		ImportChoice choice;
		choice.kind = ImportChoice::kCampaign;
		choice.source = campaigns[i].identity.gameNumber;
		choice.campaign = campaigns[i];
		choices.push_back(choice);
		Common::U32String label = campaignLabel(choice.campaign);
		for (uint j = 0; j < campaigns.size(); ++j) {
			if (i != j && campaignLabel(campaigns[j]) == label) {
				label += Common::U32String::format(" [%08x-%08x]",
					choice.campaign.identity.masterCRC, choice.campaign.identity.saveCRC);
				break;
			}
		}
		labels.push_back(label);
	}
	// Different mounted archives can contain different disks named SAVE1.DSK.
	Common::ArchiveMemberDetailsList members;
	SearchMan.listMatchingMembers(members, Common::Path("*.DSK"), true);
	for (Common::ArchiveMemberDetailsList::const_iterator i = members.begin(); i != members.end(); ++i) {
		Common::Array<byte> bytes, program, normalized;
		Common::String error;
		UcsdDisk disk;
		if (!readDisk(i->arcMember, bytes, error) || !disk.loadBytes(bytes, error) ||
			!disk.readFile("SYSTEM.PASCAL", program, error))
			continue;
		const byte game = TransferDisks::scenario(wizardryCrc32(program.data(), program.size()));
		if (!game || game > maxSource || !TransferImport::prepareImage(game, bytes, normalized, error))
			continue;
		ImportChoice choice;
		choice.kind = ImportChoice::kDisk;
		choice.source = game;
		choice.member = i->arcMember;
		choices.push_back(choice);
		labels.push_back(Common::U32String::format(_("Wizardry %s: disk %s [%s]"), kSourceNames[game - 1],
			i->arcMember->getPathInArchive().toString('/').c_str(), i->arcName.c_str()));
	}
	if (choices.empty()) {
		GUI::MessageDialog(_("No supported Wizardry source campaign or disk was found. Returning to Utilities.")).runModal();
		return false;
	}
	while (true) {
		TransferSourceDialog dialog;
		dialog.setList(labels);
		const int selected = dialog.runModal();
		if (selected < 0 || uint(selected) >= choices.size())
			return false;
		const ImportChoice &choice = choices[selected];
		if (choice.kind == ImportChoice::kCurrent) {
			source = choice.source;
			image.clear();
			return true;
		}
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
			GUI::MessageDialog(_("Unable to import this source. Its disk data is missing, unreadable or unsupported. Please choose another campaign or disk.")).runModal();
			continue;
		}
		const Common::U32String message = current.present(choice.source) ?
			_("Import a private copy? The source will not be changed. This replaces the current transfer disk, so previously moved characters may become available again.") :
			_("Import a private copy? The source will not be changed. The game will ask you which characters to transfer.");
		GUI::MessageDialog confirm(message, _("Import"), _("Cancel"));
		if (confirm.runModal() != GUI::kMessageOK)
			continue;
		source = choice.source;
		image = Common::move(candidate);
		return true;
	}
}

} // namespace

MachineObserver::TransferSourceResult WizardryEngine::onTransferSourceRequest(byte destination, const TransferDisks &current,
		byte &source, Common::Array<byte> &image) {
	// The VM cannot be replaced or suspended inside its source request callback.
	PauseToken pause = pauseEngine();
	_transferDialogOpen = true;
	present();
	const bool chosen = chooseSource(*_saveFileMan, destination, current, source, image);
	_transferDialogOpen = false;
	return chosen ? kTransferSelected : kTransferCancelled;
}

} // namespace Wizardry
