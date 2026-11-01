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

#include "wizardry/transfer_import.h"
#include "common/algorithm.h"
#include "common/endian.h"
#include "common/savefile.h"
#include "wizardry/data/ucsd_disk.h"
#include "wizardry/vm/codefile.h"

namespace Wizardry {

namespace {

bool campaignFromName(const Common::String &name, TransferCampaign &source, uint &slot) {
	const Common::String marker = "-wizardry-campaign-g";
	Common::String normalized = name;
	normalized.toLowercase();
	const size_t pos = normalized.rfind(marker);
	if (pos == Common::String::npos || !pos || pos > 128)
		return false;
	unsigned game, profile, masterCRC, saveCRC, campaign;
	char copy;
	int length = 0;
	if (sscanf(normalized.c_str() + pos,
		"-wizardry-campaign-g%1u-%1u-%8x-%8x-%4x-%c.wzc%n",
		&game, &profile, &masterCRC, &saveCRC, &campaign, &copy, &length) != 6 ||
		pos + length != name.size() || game < 1 || game > 3 || profile > CampaignStore::kPaired ||
		(copy != 'a' && copy != 'b'))
		return false;
	source.target = Common::String(name.c_str(), pos);
	for (uint i = 0; i < source.target.size(); ++i) {
		const byte c = source.target[i];
		if (c < 32 || c > 126 || strchr("/\\:*?\"<>|", c))
			return false;
	}
	source.identity.gameNumber = game;
	source.identity.profile = CampaignStore::Profile(profile);
	source.identity.masterCRC = masterCRC;
	source.identity.saveCRC = saveCRC;
	source.identity.campaign = campaign;
	source.identity.masterSize = 640 * 512;
	source.identity.saveSize = profile == CampaignStore::kPaired ? 1280 * 512 : 0;
	slot = copy == 'b';
	return profile == CampaignStore::kPaired || saveCRC == 0;
}

bool hasMatchingHeader(Common::SaveFileManager &manager, const Common::String &name,
		const TransferCampaign &source) {
	Common::InSaveFile *in = manager.openForLoading(name);
	byte h[48];
	bool ok = in && in->read(h, sizeof(h)) == sizeof(h) && !in->err();
	if (ok) {
		const CampaignStore::Identity &id = source.identity;
		const uint32 transferSize = READ_LE_UINT32(h + 36);
		ok = !memcmp(h, "WZCP", 4) && READ_LE_UINT16(h + 4) == 2 &&
			READ_LE_UINT16(h + 6) == id.profile && READ_LE_UINT64(h + 8) &&
			READ_LE_UINT32(h + 16) == id.gameNumber && READ_LE_UINT32(h + 20) == id.masterCRC &&
			READ_LE_UINT32(h + 24) == id.saveCRC && READ_LE_UINT32(h + 28) == id.masterSize &&
			READ_LE_UINT32(h + 32) == id.saveSize && transferSize >= 32 &&
			transferSize <= 32 + 3 * 1280 * 512 &&
			in->size() == int64(48 + id.masterSize + id.saveSize + transferSize) &&
			READ_LE_UINT32(h + 44) == wizardryCrc32(h, 44);
	}
	delete in;
	return ok;
}

} // namespace

Common::Array<TransferCampaign> TransferImport::listCampaigns(Common::SaveFileManager &manager, byte destination) {
	Common::Array<TransferCampaign> result;
	const byte maxSource = TransferDisks::sourceCount(destination);
	if (!maxSource)
		return result;
	Common::StringArray names = manager.listSavefiles("*.wzc");
	Common::sort(names.begin(), names.end());
	Common::StringArray seen;
	for (uint i = 0; i < names.size(); ++i) {
		TransferCampaign source;
		uint slot;
		if (!campaignFromName(names[i], source, slot) || source.identity.gameNumber > maxSource)
			continue;
		CampaignStore store(manager, source.target, source.identity);
		if (!store.filename(slot).equalsIgnoreCase(names[i]) || !hasMatchingHeader(manager, names[i], source))
			continue;
		const Common::String key = store.filename(0);
		bool duplicate = false;
		for (uint j = 0; j < seen.size(); ++j)
			duplicate |= key.equalsIgnoreCase(seen[j]);
		if (!duplicate) {
			seen.push_back(key);
			result.push_back(source);
		}
	}
	return result;
}

bool TransferImport::prepareImage(byte source, const Common::Array<byte> &disk,
		Common::Array<byte> &image, Common::String &error) {
	error.clear();
	Common::Array<byte> candidate;
	if (disk.size() == 640 * 512) {
		// The I-III scenario maker copies 632 blocks into a zeroed 1280-block disk.
		UcsdDisk volume;
		if (!volume.loadBytes(disk, error))
			return false;
		const UcsdDisk::Entry *scenario = volume.find("SCENARIO.DATA");
		if (!scenario || !scenario->available || scenario->endBlock > 632 || !scenario->size()) {
			error = "The saved scenario data is not wholly inside the supported I-III disk area";
			return false;
		}
		candidate.resize(1280 * 512);
		memcpy(candidate.data(), disk.data(), 632 * 512);
	} else if (disk.size() == 1280 * 512) {
		candidate = disk;
	} else {
		error = "Import requires an authenticated I-III campaign disk or complete SAVE disk";
		return false;
	}
	if (!TransferDisks::validImage(source, candidate, error))
		return false;
	image = Common::move(candidate);
	return true;
}

bool TransferImport::loadCampaign(Common::SaveFileManager &manager, const TransferCampaign &source,
		Common::Array<byte> &image, Common::String &error) {
	CampaignStore store(manager, source.target, source.identity);
	Common::Array<byte> master, save;
	const CampaignStore::LoadResult loaded = store.load(master, save, error);
	if (loaded == CampaignStore::kError)
		return false;
	if (loaded == CampaignStore::kEmpty) {
		error = "The selected source campaign is no longer available";
		return false;
	}
	return prepareImage(byte(source.identity.gameNumber),
		source.identity.profile == CampaignStore::kPaired ? save : master, image, error);
}

} // namespace Wizardry
