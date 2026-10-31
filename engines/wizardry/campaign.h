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

#ifndef WIZARDRY_CAMPAIGN_H
#define WIZARDRY_CAMPAIGN_H

#include "common/array.h"
#include "common/str.h"
#include "wizardry/vm/transfer.h"

namespace Common {
class SaveFileManager;
}

namespace Wizardry {

/** Private campaign images, independent of suspended-game snapshots. */
class CampaignStore {
public:
	enum Profile : uint16 { kCompatibility = 0,
							kOriginal = 1,
							kPaired = 2 };
	enum LoadResult { kEmpty,
					  kLoaded,
					  kRecovered,
					  kError };
	struct Identity {
		uint32 gameNumber = 1;
		uint16 campaign = 0;
		Profile profile = kCompatibility;
		uint32 masterCRC = 0, saveCRC = 0;
		uint32 masterSize = 0, saveSize = 0;
	};
	CampaignStore(Common::SaveFileManager &manager, const Common::String &target, const Identity &identity);
	LoadResult load(Common::Array<byte> &master, Common::Array<byte> &save, Common::String &error,
					TransferDisks *transfer = nullptr);
	bool commit(const Common::Array<byte> &master, const Common::Array<byte> &save, Common::String &error,
				const TransferDisks *transfer = nullptr);
	// The caller must first validate a complete game snapshot.
	bool recoverFromSnapshot(const Common::Array<byte> &master, const Common::Array<byte> &save,
							 Common::String &error, const TransferDisks *transfer = nullptr);
	Common::String filename(uint slot) const;
	uint64 generation() const { return _generation; }
	static bool readSelectedCampaignId(Common::SaveFileManager &manager, const Common::String &target,
									   Identity &identity, Common::String &error);
	static bool findUnusedCampaignId(Common::SaveFileManager &manager, const Common::String &target,
									 Identity &identity, Common::String &error);
	bool select(Common::String &error) const;

private:
	struct Record {
		uint64 generation = 0;
		Common::Array<byte> master, save;
		TransferDisks transfer;
	};
	enum SlotResult { kAbsent,
					  kValid,
					  kInvalid };
	Common::SaveFileManager &_manager;
	Common::String _target;
	Identity _identity;
	Common::Array<byte> _committedMaster, _committedSave;
	TransferDisks _committedTransfer;
	uint64 _generation = 0;
	int _active = -1;
	bool _opened = false;
	bool _writeUncertain = false;
	bool _recoveryAllowed = false;
	uint64 _recoveryGeneration = 0;
	bool validIdentity() const;
	byte transferTarget() const;
	bool preserveCopies(Common::String &error);
	SlotResult readSlot(uint slot, Record &record, Common::String &error);
	static bool same(const Common::Array<byte> &a, const Common::Array<byte> &b);
	Common::String selectionFilename(uint slot) const;
	bool readSelection(uint16 &campaign, uint64 &generation, int &active, Common::String &error) const;
};

} // namespace Wizardry

#endif
