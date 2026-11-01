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

#ifndef WIZARDRY_TRANSFER_IMPORT_H
#define WIZARDRY_TRANSFER_IMPORT_H

#include "wizardry/campaign.h"

namespace Wizardry {

/** Identity shared by a campaign's alternating journal files. */
struct TransferCampaign {
	Common::String target;
	CampaignStore::Identity identity;
};

class TransferImport {
public:
	// Discovery checks headers; loading validates complete campaign records.
	static Common::Array<TransferCampaign> listCampaigns(Common::SaveFileManager &manager, byte destination);
	static bool loadCampaign(Common::SaveFileManager &manager, const TransferCampaign &source,
		Common::Array<byte> &image, Common::String &error);
	// Expand a saved DOS I-III disk to the guest's auxiliary-disk geometry.
	static bool prepareImage(byte source, const Common::Array<byte> &disk,
		Common::Array<byte> &image, Common::String &error);
};

} // namespace Wizardry

#endif
