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

#include "wizardry/vm/graphics_residue.h"
#include "common/util.h"

namespace Wizardry {

void GraphicsResidue::initialize(byte revision) {
	_base = revision == 1 ? 0x7252 : revision == 4 ? 0x73a2
								 : revision == 5   ? 0x71a2
												   : 0;
	_extent = revision == 1 || revision == 4 ? 10120 : revision == 5 ? 17408
																	 : 0;
	_data.resize(_extent);
	_known.resize(_data.size());
	if (!_data.empty()) {
		memset(_data.data(), 0, _data.size());
		memset(_known.data(), 0, _known.size());
	}
}

bool GraphicsResidue::contains(uint16 address, uint32 count) const {
	return _extent && address >= _base && uint32(address) + count <= uint32(_base) + _extent;
}

uint32 GraphicsResidue::offset(uint16 address) const {
	return address - _base;
}

byte *GraphicsResidue::data(uint16 address) {
	return contains(address, 1) ? _data.data() + offset(address) : nullptr;
}

byte *GraphicsResidue::known(uint16 address) {
	return contains(address, 1) ? _known.data() + offset(address) : nullptr;
}

void GraphicsResidue::establish(uint16 address, uint16 count) {
	if (count && contains(address, count))
		memset(_known.data() + offset(address), 1, count);
}

} // namespace Wizardry
