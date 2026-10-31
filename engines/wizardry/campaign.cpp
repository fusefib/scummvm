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

#include "wizardry/campaign.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "common/savefile.h"
#include "wizardry/vm/codefile.h"

namespace Wizardry {

static const uint32 kCampaignHeaderSize = 48;

CampaignStore::CampaignStore(Common::SaveFileManager &manager, const Common::String &target,
							 const Identity &identity) : _manager(manager), _target(target), _identity(identity) {}

bool CampaignStore::validIdentity() const {
	if (_target.empty() || _target.size() > 128 || _target == "." || _target == "..")
		return false;
	for (uint i = 0; i < _target.size(); ++i) {
		const byte c = _target[i];
		if (c < 32 || c > 126 || strchr("/\\:*?\"<>|", c))
			return false;
	}
	return _identity.gameNumber >= 1 && _identity.gameNumber <= 5 &&
		   (_identity.gameNumber <= 3 || _identity.profile == kPaired) &&
		   _identity.profile >= kCompatibility && _identity.profile <= kPaired && _identity.masterSize == (_identity.gameNumber == 5 ? 1280U : 640U) * 512 &&
		   (_identity.profile == kPaired ? _identity.saveSize == 1280 * 512 : (!_identity.saveSize && !_identity.saveCRC));
}

byte CampaignStore::transferTarget() const {
	return _identity.profile == kPaired && (_identity.gameNumber == 2 || _identity.gameNumber == 3 || _identity.gameNumber == 5) ? byte(_identity.gameNumber) : 0;
}

Common::String CampaignStore::filename(uint slot) const {
	return Common::String::format("%s-wizardry-campaign-g%u-%u-%08x-%08x-%04x-%c.wzc", _target.c_str(),
								  unsigned(_identity.gameNumber), unsigned(_identity.profile), _identity.masterCRC,
								  _identity.saveCRC, _identity.campaign, slot ? 'b' : 'a');
}

Common::String CampaignStore::selectionFilename(uint slot) const {
	return Common::String::format("%s-wizardry-campaign-g%u-%u-%08x-%08x-selected-%c.wzc", _target.c_str(),
								  unsigned(_identity.gameNumber), unsigned(_identity.profile), _identity.masterCRC,
								  _identity.saveCRC, slot ? 'b' : 'a');
}

bool CampaignStore::readSelection(uint16 &campaign, uint64 &generation, int &active, Common::String &error) const {
	error.clear();
	campaign = 0;
	generation = 0;
	active = -1;
	bool found = false;
	for (uint slot = 0; slot < 2; ++slot) {
		const Common::String name = selectionFilename(slot);
		if (!_manager.exists(name))
			continue;
		found = true;
		Common::InSaveFile *input = _manager.openForLoading(name);
		byte bytes[20];
		const bool valid = input && input->size() == sizeof(bytes) &&
						   input->read(bytes, sizeof(bytes)) == sizeof(bytes) && !input->err() &&
						   !memcmp(bytes, "WZCS", 4) && !READ_LE_UINT16(bytes + 6) &&
						   READ_LE_UINT64(bytes + 8) && READ_LE_UINT32(bytes + 16) == wizardryCrc32(bytes, 16);
		delete input;
		if (valid && READ_LE_UINT64(bytes + 8) == generation && READ_LE_UINT16(bytes + 4) != campaign) {
			error = "Campaign selections disagree at the same generation";
			return false;
		}
		if (valid && READ_LE_UINT64(bytes + 8) > generation) {
			active = slot;
			generation = READ_LE_UINT64(bytes + 8);
			campaign = READ_LE_UINT16(bytes + 4);
		}
	}
	if (found && active < 0) {
		error = "The selected Wizardry campaign could not be read";
		return false;
	}
	return true;
}

bool CampaignStore::readSelectedCampaignId(Common::SaveFileManager &manager, const Common::String &target,
										   Identity &identity, Common::String &error) {
	CampaignStore store(manager, target, identity);
	uint64 generation;
	int active;
	if (!store.validIdentity()) {
		error = "Invalid Wizardry campaign identity";
		return false;
	}
	return store.readSelection(identity.campaign, generation, active, error);
}

bool CampaignStore::findUnusedCampaignId(Common::SaveFileManager &manager, const Common::String &target,
										 Identity &identity, Common::String &error) {
	CampaignStore check(manager, target, identity);
	if (!check.validIdentity()) {
		error = "Invalid Wizardry campaign identity";
		return false;
	}
	error.clear();
	for (uint id = 1; id <= 0xffff; ++id) {
		identity.campaign = id;
		CampaignStore store(manager, target, identity);
		if (!manager.exists(store.filename(0)) && !manager.exists(store.filename(1)))
			return true;
	}
	error = "No unused Wizardry campaign identity is available";
	return false;
}

bool CampaignStore::select(Common::String &error) const {
	uint16 previous;
	uint64 generation;
	int active;
	if (!_generation) {
		error = "A campaign must be saved before it can be selected";
		return false;
	}
	if (!readSelection(previous, generation, active, error))
		return false;
	if (previous == _identity.campaign)
		return true;
	if (generation == ~(uint64)0) {
		error = "Campaign selection limit reached";
		return false;
	}
	byte bytes[20] = {};
	memcpy(bytes, "WZCS", 4);
	WRITE_LE_UINT16(bytes + 4, _identity.campaign);
	WRITE_LE_UINT64(bytes + 8, generation + 1);
	WRITE_LE_UINT32(bytes + 16, wizardryCrc32(bytes, 16));
	const uint slot = active < 0 ? 0 : 1 - active;
	const Common::String name = selectionFilename(slot);
	Common::OutSaveFile *output = _manager.openForSaving(name);
	const bool opened = output != nullptr;
	if (output) {
		output->write(bytes, sizeof(bytes));
		output->finalize();
		delete output;
	}
	// A failed write can still leave a complete record. Verify it before
	// deciding whether to keep it or remove it to retain the old selection.
	Common::InSaveFile *input = opened ? _manager.openForLoading(name) : nullptr;
	byte verify[20];
	const bool complete = input && input->size() == sizeof(verify) &&
						  input->read(verify, sizeof(verify)) == sizeof(verify) && !input->err() && !memcmp(bytes, verify, sizeof(bytes));
	delete input;
	if (!complete) {
		if (opened && !_manager.removeSavefile(name)) {
			error = "The campaign selection could not be verified or removed. Restore the previous saved game before continuing.";
			return false;
		}
		error = "Could not verify the new campaign selection; the previous campaign is retained";
		return false;
	}
	return true;
}

bool CampaignStore::same(const Common::Array<byte> &a, const Common::Array<byte> &b) {
	return a.size() == b.size() && (a.empty() || !memcmp(a.data(), b.data(), a.size()));
}

CampaignStore::SlotResult CampaignStore::readSlot(uint slot, Record &record, Common::String &error) {
	const Common::String name = filename(slot);
	if (!_manager.exists(name))
		return kAbsent;
	Common::InSaveFile *input = _manager.openForLoading(name);
	if (!input) {
		error = "Could not read a campaign copy";
		return kInvalid;
	}
	byte header[kCampaignHeaderSize];
	const uint32 disksSize = _identity.masterSize + _identity.saveSize;
	uint32 transferSize = 0, total = disksSize;
	bool valid = input->size() >= int64(kCampaignHeaderSize + total) &&
				 input->size() <= int64(kCampaignHeaderSize + total + 32 + 3 * 1280 * 512) &&
				 input->read(header, sizeof(header)) == sizeof(header);
	if (valid) {
		const uint16 version = READ_LE_UINT16(header + 4);
		transferSize = READ_LE_UINT32(header + 36);
		valid = !memcmp(header, "WZCP", 4) &&
				version == 2 && transferSize >= 32 && transferSize <= 32 + 3 * 1280 * 512 &&
				READ_LE_UINT16(header + 6) == _identity.profile && READ_LE_UINT64(header + 8) != 0 &&
				READ_LE_UINT32(header + 16) == _identity.gameNumber && READ_LE_UINT32(header + 20) == _identity.masterCRC &&
				READ_LE_UINT32(header + 24) == _identity.saveCRC &&
				READ_LE_UINT32(header + 28) == _identity.masterSize &&
				READ_LE_UINT32(header + 32) == _identity.saveSize &&
				READ_LE_UINT32(header + 44) == wizardryCrc32(header, 44);
		if (valid) {
			total += transferSize;
			valid = input->size() == int64(kCampaignHeaderSize + total);
		}
	}
	Common::Array<byte> payload;
	if (valid) {
		payload.resize(total);
		valid = input->read(payload.data(), total) == total && !input->err() &&
				READ_LE_UINT32(header + 40) == wizardryCrc32(payload.data(), total);
	}
	delete input;
	TransferDisks transfer;
	if (valid) {
		Common::MemoryReadStream stream(payload.data() + disksSize, transferSize);
		valid = transfer.load(stream, transferTarget(), true, error);
	}
	if (!valid) {
		error = "A campaign copy is incomplete, damaged, or belongs to different game data";
		return kInvalid;
	}
	record.generation = READ_LE_UINT64(header + 8);
	record.master.resize(_identity.masterSize);
	memcpy(record.master.data(), payload.data(), record.master.size());
	record.save.resize(_identity.saveSize);
	if (!record.save.empty())
		memcpy(record.save.data(), payload.data() + record.master.size(), record.save.size());
	record.transfer = Common::move(transfer);
	return kValid;
}

CampaignStore::LoadResult CampaignStore::load(Common::Array<byte> &master,
											  Common::Array<byte> &save, Common::String &error, TransferDisks *transfer) {
	error.clear();
	if (_opened || !validIdentity()) {
		error = "Invalid campaign identity or repeated campaign open";
		return kError;
	}
	Record records[2];
	const SlotResult a = readSlot(0, records[0], error), b = readSlot(1, records[1], error);
	if (a != kValid && b != kValid) {
		if (a == kAbsent && b == kAbsent) {
			_opened = true;
			master.clear();
			save.clear();
			return kEmpty;
		}
		_recoveryAllowed = true;
		error = "Neither campaign copy can be loaded. Restore a ScummVM saved game or use a new game target.";
		return kError;
	}
	if (a == kValid && b == kValid && records[0].generation == records[1].generation &&
		(!same(records[0].master, records[1].master) || !same(records[0].save, records[1].save) ||
		 !records[0].transfer.same(records[1].transfer))) {
		_recoveryAllowed = true;
		_recoveryGeneration = records[0].generation;
		error = "Campaign copies have the same generation but different contents";
		return kError;
	}
	_active = b == kValid && (a != kValid || records[1].generation > records[0].generation) ? 1 : 0;
	_generation = records[_active].generation;
	_committedMaster = records[_active].master;
	_committedSave = records[_active].save;
	_committedTransfer = records[_active].transfer;
	master = _committedMaster;
	save = _committedSave;
	if (transfer)
		*transfer = _committedTransfer;
	_opened = true;
	const bool recovered = a == kInvalid || b == kInvalid;
	error.clear();
	return recovered ? kRecovered : kLoaded;
}

bool CampaignStore::commit(const Common::Array<byte> &master, const Common::Array<byte> &save,
						   Common::String &error, const TransferDisks *transfer) {
	error.clear();
	const TransferDisks &sources = transfer ? *transfer : _committedTransfer;
	if (!_opened || !validIdentity() || master.size() != _identity.masterSize || save.size() != _identity.saveSize) {
		error = "Campaign images do not match the opened campaign";
		return false;
	}
	if (!sources.validate(transferTarget(), true, error))
		return false;
	if (!_writeUncertain && _active >= 0 && same(master, _committedMaster) && same(save, _committedSave) &&
		sources.same(_committedTransfer))
		return true;
	if (_generation == ~(uint64)0) {
		error = "Campaign generation limit reached";
		return false;
	}
	const uint slot = _active < 0 ? 0 : 1 - _active;
	const uint64 next = _generation + 1;
	byte header[kCampaignHeaderSize] = {};
	memcpy(header, "WZCP", 4);
	WRITE_LE_UINT16(header + 4, 2);
	WRITE_LE_UINT16(header + 6, _identity.profile);
	WRITE_LE_UINT64(header + 8, next);
	WRITE_LE_UINT32(header + 16, _identity.gameNumber);
	WRITE_LE_UINT32(header + 20, _identity.masterCRC);
	WRITE_LE_UINT32(header + 24, _identity.saveCRC);
	WRITE_LE_UINT32(header + 28, master.size());
	WRITE_LE_UINT32(header + 32, save.size());
	const uint32 transferSize = sources.size();
	WRITE_LE_UINT32(header + 36, transferSize);
	const uint32 payloadSize = master.size() + save.size() + transferSize;
	Common::Array<byte> payload(payloadSize);
	memcpy(payload.data(), master.data(), master.size());
	if (!save.empty())
		memcpy(payload.data() + master.size(), save.data(), save.size());
	Common::MemoryWriteStream stream(payload.data() + master.size() + save.size(), transferSize);
	if (!sources.save(stream, true) || stream.pos() != transferSize) {
		error = "Could not encode complete campaign transfer images";
		return false;
	}
	WRITE_LE_UINT32(header + 40, wizardryCrc32(payload.data(), payload.size()));
	WRITE_LE_UINT32(header + 44, wizardryCrc32(header, 44));
	// Even a reported failure can leave a complete newer copy on disk. A later
	// request for the old images must overwrite that uncertain copy as well.
	_writeUncertain = true;
	Common::OutSaveFile *output = _manager.openForSaving(filename(slot));
	if (!output) {
		error = "Could not open the next campaign copy for saving";
		return false;
	}
	const bool complete = output->write(header, sizeof(header)) == sizeof(header) &&
						  output->write(payload.data(), payload.size()) == payload.size();
	output->finalize();
	const bool written = complete && !output->err();
	delete output;
	if (!written) {
		error = "Could not finish saving the campaign";
		return false;
	}
	Record verified;
	if (readSlot(slot, verified, error) != kValid || verified.generation != next ||
		!same(verified.master, master) || !same(verified.save, save) || !verified.transfer.same(sources)) {
		error = "The new campaign copy could not be verified";
		return false;
	}
	_active = slot;
	_generation = next;
	_committedMaster = master;
	_committedSave = save;
	_committedTransfer = sources;
	_writeUncertain = false;
	return true;
}

bool CampaignStore::preserveCopies(Common::String &error) {
	for (uint slot = 0; slot < 2; ++slot) {
		const Common::String source = filename(slot);
		if (!_manager.exists(source))
			continue;
		Common::InSaveFile *input = _manager.openRawFile(source);
		if (!input || input->size() < 0 || input->size() > 4 * 1024 * 1024) {
			delete input;
			error = "The damaged campaign could not be preserved; use a new game target";
			return false;
		}
		Common::Array<byte> raw(uint32(input->size()));
		const bool read = (raw.empty() || input->read(raw.data(), raw.size()) == raw.size()) && !input->err();
		delete input;
		if (!read) {
			error = "Could not read the damaged campaign for recovery";
			return false;
		}
		Common::String backup;
		for (uint index = 1; index <= 999; ++index) {
			backup = Common::String::format("%s-recovery-%03u", source.c_str(), index);
			if (!_manager.exists(backup))
				break;
			backup.clear();
		}
		if (backup.empty()) {
			error = "No unused campaign recovery filename is available";
			return false;
		}
		Common::OutSaveFile *output = _manager.openForSaving(backup, false);
		if (!output) {
			error = "Could not preserve the damaged campaign";
			return false;
		}
		bool complete = raw.empty() || output->write(raw.data(), raw.size()) == raw.size();
		output->finalize();
		complete = complete && !output->err();
		delete output;
		if (complete) {
			input = _manager.openRawFile(backup);
			Common::Array<byte> verify(raw.size());
			complete = input && input->size() == int64(raw.size()) &&
					   (raw.empty() || input->read(verify.data(), verify.size()) == verify.size()) &&
					   !input->err() && same(raw, verify);
			delete input;
		}
		if (!complete) {
			error = "The campaign recovery copy could not be verified";
			return false;
		}
	}
	return true;
}

bool CampaignStore::recoverFromSnapshot(const Common::Array<byte> &master,
										const Common::Array<byte> &save, Common::String &error, const TransferDisks *transfer) {
	if (!_recoveryAllowed || !validIdentity() ||
		master.size() != _identity.masterSize || save.size() != _identity.saveSize ||
		_recoveryGeneration == ~(uint64)0) {
		error = "Campaign recovery requires a matching verified snapshot and a failed campaign open";
		return false;
	}
	if (transfer && !transfer->validate(transferTarget(), true, error))
		return false;
	if (!_opened) {
		if (!preserveCopies(error))
			return false;
		_opened = true;
		_generation = _recoveryGeneration;
	}
	// The preserved raw copies need not be duplicated when the final verified
	// write fails transiently. Retry that commit with the current snapshot.
	if (!commit(master, save, error, transfer))
		return false;
	_recoveryAllowed = false;
	return true;
}

} // namespace Wizardry
