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

#ifndef WIZARDRY_VM_GRAPHICS_RESIDUE_H
#define WIZARDRY_VM_GRAPHICS_RESIDUE_H

#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"

namespace Wizardry {

/** Established bytes in the original CS graphics buffers. */
class GraphicsResidue {
public:
	GraphicsResidue() { initialize(0); }
	void initialize(byte revision);
	bool contains(uint16 address, uint32 count) const;
	byte *data(uint16 address);
	byte *known(uint16 address);
	const Common::Array<byte> &bytes() const { return _data; }
	const Common::Array<byte> &knowledge() const { return _known; }
	uint16 base() const { return _base; }
	uint16 extent() const { return _extent; }
	void establish(uint16 address, uint16 count);
	bool needsPersistence() const;
	uint32 size() const { return 8 + (_data.size() + 7) / 8 + _data.size(); }
	bool validate(byte revision, Common::String &error) const;
	bool save(Common::WriteStream &out) const;
	bool load(Common::SeekableReadStream &in, byte revision, Common::String &error);

private:
	uint16 _base = 0, _extent = 0;
	Common::Array<byte> _data, _known;
	uint32 offset(uint16 address) const;
};

} // namespace Wizardry
#endif
