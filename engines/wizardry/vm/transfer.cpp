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

} // namespace Wizardry
