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

#ifndef WIZARDRY_VM_LOADER_KEYBOARD_H
#define WIZARDRY_VM_LOADER_KEYBOARD_H

#include "common/array.h"
#include "common/str.h"

namespace Wizardry {

class LoaderKeyboard {
public:
	enum Action { kNone,
				  kStart,
				  kReturn,
				  kSelectSave,
				  kExit,
				  kAuxiliary,
				  kMagicWord,
				  kValidationCode,
				  kTransferSource,
				  kTransferClose };
	static Action match(byte revision, const byte *vram);
	static void drawTransferChoice(byte *vram, byte profile = 3);
	static bool transferChoice(byte profile, byte key, byte &source, Common::Array<uint16> &keys);
	static bool validQueue(const Common::Array<uint16> &keys);
	static bool validQueue(const Common::Array<uint16> &keys, byte revision, const Common::Array<byte> &data);
	static void enqueue(Action action, Common::Array<uint16> &keys);
	static bool extractData(byte revision, const Common::Array<byte> &com, Common::Array<byte> &data, Common::String &error);
	static bool validData(byte revision, const Common::Array<byte> &data);
	static void challengeKeys(byte revision, const byte *vram, const Common::Array<byte> &data, Common::Array<uint16> &keys);
};

} // namespace Wizardry
#endif
