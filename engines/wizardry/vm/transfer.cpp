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

#include "wizardry/vm/transfer.h"
#include "common/endian.h"
#include "wizardry/data/ucsd_disk.h"
#include "wizardry/vm/codefile.h"

namespace Wizardry {

byte TransferDisks::scenario(uint32 programCRC) {
	switch (programCRC) {
	case 0xbf1ca510:
		return 1;
	case 0x3e572a62:
		return 2;
	case 0xde057d53:
		return 3;
	case 0xb063c7b0:
		return 5;
	default:
		return 0;
	}
}

bool TransferDisks::validImage(byte source, const Common::Array<byte> &image, Common::String &error) {
	if (source < 1 || source > 3 || image.size() != 1280 * 512 || READ_LE_UINT16(image.data() + 3) != 0x0880) {
		error = "Character transfer requires a complete earlier-scenario SAVE image";
		return false;
	}
	UcsdDisk disk;
	Common::Array<byte> code, interpreter;
	if (!disk.loadBytes(image, error) || !disk.readFile("SYSTEM.PASCAL", code, error) ||
		!disk.readFile("SYSTEM.INTERP", interpreter, error))
		return false;
	if (scenario(wizardryCrc32(code.data(), code.size())) != source || interpreter.size() != 16384 ||
		wizardryCrc32(interpreter.data(), interpreter.size()) != 0xb8bbf065) {
		error = "Character-transfer image belongs to a different scenario or interpreter";
		return false;
	}
	return true;
}

bool TransferDisks::attach(byte target, byte source, const Common::Array<byte> &image, Common::String &error) {
	if (source < 1 || source > sourceCount(target) || active || choosing || transaction) {
		error = "Character-transfer source cannot be attached in this state";
		return false;
	}
	if (!validImage(source, image, error))
		return false;
	images[source - 1] = image;
	sourceCRC[source - 1] = wizardryCrc32(image.data(), image.size());
	return true;
}

bool TransferDisks::validate(byte target, bool campaign, Common::String &error) const {
	if (empty())
		return true;
	if (!sourceCount(target) || active > sourceCount(target) ||
		(choosing && ((target != 3 && target != 5) || (target != 5 && active) || !transaction)) ||
		(active && (!transaction || !present(active))) ||
		(campaign && (active || choosing || transaction))) {
		error = "Invalid character-transfer state";
		return false;
	}
	for (byte source = 1; source <= 3; ++source) {
		const Common::Array<byte> &image = images[source - 1];
		if (image.empty()) {
			if (sourceCRC[source - 1]) {
				error = "Empty transfer disk has a source identity";
				return false;
			}
		} else if (source > sourceCount(target) || !sourceCRC[source - 1] || !validImage(source, image, error)) {
			if (error.empty())
				error = "Invalid earlier-scenario transfer image";
			return false;
		}
	}
	return true;
}

bool TransferDisks::save(Common::WriteStream &out, bool campaign) const {
	const byte count = 3;
	out.writeUint32BE(0x54523137); // TR17
	out.writeByte(campaign ? 0 : (choosing ? 1 : 0) | (transaction ? 2 : 0));
	out.writeByte(campaign ? 0 : active);
	out.writeUint16LE(0);
	for (uint i = 0; i < count; ++i)
		out.writeUint32LE(sourceCRC[i]);
	for (uint i = 0; i < count; ++i)
		out.writeUint32LE(images[i].size());
	for (uint i = 0; i < count; ++i)
		if (!images[i].empty())
			out.write(images[i].data(), images[i].size());
	return !out.err();
}

bool TransferDisks::load(Common::SeekableReadStream &in, byte target, bool campaign, Common::String &error) {
	TransferDisks next;
	if (in.size() - in.pos() < 32) {
		error = "Missing character-transfer extension";
		return false;
	}
	const uint32 magic = in.readUint32BE();
	const byte count = 3;
	if (magic != 0x54523137) {
		error = "Invalid character-transfer extension profile";
		return false;
	}
	const byte flags = in.readByte();
	next.active = in.readByte();
	if (flags > 3 || in.readUint16LE()) {
		error = "Invalid character-transfer flags";
		return false;
	}
	next.choosing = flags & 1;
	next.transaction = flags & 2;
	for (uint i = 0; i < count; ++i)
		next.sourceCRC[i] = in.readUint32LE();
	uint32 sizes[3] = {0, 0, 0};
	for (uint i = 0; i < count; ++i)
		sizes[i] = in.readUint32LE();
	if ((sizes[0] && sizes[0] != 1280 * 512) || (sizes[1] && sizes[1] != 1280 * 512) ||
		(sizes[2] && sizes[2] != 1280 * 512) ||
		uint64(sizes[0]) + sizes[1] + sizes[2] != uint64(in.size() - in.pos())) {
		error = "Invalid character-transfer image extent";
		return false;
	}
	for (uint i = 0; i < count; ++i) {
		next.images[i].resize(sizes[i]);
		if (sizes[i] && in.read(next.images[i].data(), sizes[i]) != sizes[i]) {
			error = "Truncated character-transfer image";
			return false;
		}
	}
	if (in.err() || !next.validate(target, campaign, error))
		return false;
	*this = Common::move(next);
	return true;
}

bool TransferDisks::same(const TransferDisks &other) const {
	if (active != other.active || choosing != other.choosing || transaction != other.transaction)
		return false;
	for (uint i = 0; i < 3; ++i) {
		if (sourceCRC[i] != other.sourceCRC[i] || images[i].size() != other.images[i].size() ||
			(!images[i].empty() && memcmp(images[i].data(), other.images[i].data(), images[i].size())))
			return false;
	}
	return true;
}

} // namespace Wizardry
