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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/endian.h"
#include "common/ptr.h"
#include "common/stream.h"
#include "cruise/sound/amiga_paula.h"
#include "cruise/sound/amiga_resources.h"

namespace Cruise {

namespace {

// Bounded form of Cruise's backward Delphine decoder, for optional files.
class MusicUnpacker {
public:
	MusicUnpacker(const byte *source, uint32 size)
		: _source(source), _position((int)size - 12), _bits(0), _crc(0), _valid(size >= 12) {}
	bool unpack(byte *output, uint32 size) {
		if (!_valid || READ_BE_UINT32(_source + _position + 8) != size)
			return false;
		_crc = READ_BE_UINT32(_source + _position + 4);
		_bits = word();
		uint32 remaining = size;
		while (_valid && remaining) {
			uint32 count, offset = 0;
			if (!bits(1)) {
				if (!bits(1))
					count = bits(3) + 1;
				else {
					count = 2;
					offset = bits(8);
					if (!offset)
						return false;
				}
			} else {
				const uint32 code = bits(2);
				if (code == 3)
					count = bits(8) + 9;
				else {
					count = code == 2 ? bits(8) + 1 : code + 3;
					offset = bits(code == 2 ? 12 : code + 9);
					if (!offset)
						return false;
				}
			}
			if (!_valid || count > remaining || (offset && offset > size - remaining))
				return false;
			while (count--) {
				--remaining;
				output[remaining] = offset ? output[remaining + offset] : bits(8);
			}
		}
		return _valid && _crc == 0;
	}
private:
	uint32 word() {
		if (_position < 0) {
			_valid = false;
			return 0;
		}
		const uint32 value = READ_BE_UINT32(_source + _position);
		_position -= 4;
		_crc ^= value;
		return value;
	}
	uint32 bits(int count) {
		uint32 value = 0;
		while (count--) {
			uint32 bit = _bits & 1;
			_bits >>= 1;
			if (!_bits) {
				_bits = word();
				bit = _bits & 1;
				_bits = (_bits >> 1) | 0x80000000;
			}
			value = (value << 1) | bit;
		}
		return value;
	}
	const byte *_source;
	int _position;
	uint32 _bits, _crc;
	bool _valid;
};

bool isVolumeName(const Common::String &name) {
	if (name.size() < 2 || name[0] != 'D')
		return false;
	for (uint i = 1; i < name.size(); ++i) {
		if (name[i] < '0' || name[i] > '9')
			return false;
	}
	return true;
}

} // End of anonymous namespace

bool AmigaMusicResources::isModuleName(const Common::String &name) {
	if (!name.hasPrefix("CORP") || name.size() == 4)
		return false;
	for (uint i = 4; i < name.size(); ++i) {
		if (name[i] < '0' || name[i] > '9')
			return false;
	}
	return true;
}

bool AmigaMusicResources::index(const Common::Path &directory, Common::String &reason) {
	Common::FSList files;
	if (!Common::FSNode(directory).getChildren(files, Common::FSNode::kListFilesOnly)) {
		reason = "cannot read directory";
		return false;
	}
	for (uint i = 0; i < files.size(); ++i) {
		Common::String name = files[i].getName();
		name.toUppercase();
		if (!isVolumeName(name))
			continue;
		Common::ScopedPtr<Common::SeekableReadStream> stream(files[i].createReadStream());
		if (!stream || stream->size() < 0 || (uint64)stream->size() > 0xffffffffU) {
			reason = "cannot read Amiga music source " + name;
			return false;
		}
		Entry entry;
		entry.file = files[i];
		const uint count = stream->readUint16BE();
		const uint entrySize = stream->readUint16BE();
		if (stream->err() || entrySize != 30 || 4 + count * 30 > stream->size()) {
			reason = "invalid Amiga volume directory " + name;
			return false;
		}
		for (uint j = 0; j < count; ++j) {
			char resourceName[15];
			stream->read(resourceName, 14);
			resourceName[14] = 0;
			entry.name = resourceName;
			entry.name.toUppercase();
			entry.offset = stream->readUint32BE();
			entry.stored = stream->readUint32BE();
			entry.expanded = stream->readUint32BE();
			stream->readUint32BE();
			if (entry.offset < 4 + count * 30)
				entry.stored = 0;
			if (isModuleName(entry.name) || entry.name.hasSuffix(".AMI"))
				_entries.push_back(entry);
		}
	}
	return true;
}

const AmigaMusicResources::Resource *AmigaMusicResources::find(const char *name) const {
	for (uint i = 0; i < _resources.size(); ++i) {
		if (_resources[i].name.equalsIgnoreCase(name))
			return &_resources[i];
	}
	return nullptr;
}

bool AmigaMusicResources::cache(const Common::String &name, Common::String &reason) {
	if (find(name.c_str()))
		return true;
	Resource resource;
	resource.name = name;
	for (uint i = 0; i < _entries.size(); ++i) {
		const Entry &entry = _entries[i];
		if (!entry.name.equalsIgnoreCase(name))
			continue;
		Common::ScopedPtr<Common::SeekableReadStream> stream(entry.file.createReadStream());
		// Native module/sample lengths are much smaller; bound optional input
		// before allocating or converting the decoder's signed positions.
		if (!stream || !entry.stored || !entry.expanded ||
				entry.stored > 2 * 1024 * 1024 || entry.expanded > 2 * 1024 * 1024 ||
				(uint64)entry.offset + entry.stored > (uint64)stream->size() ||
				!stream->seek(entry.offset)) {
			reason = "invalid Amiga resource bounds: " + name;
			return false;
		}
		Common::Array<byte> stored, decoded;
		stored.resize(entry.stored);
		if (stream->read(stored.begin(), stored.size()) != stored.size()) {
			reason = "short Amiga resource: " + name;
			return false;
		}
		if (entry.stored == entry.expanded)
			decoded.swap(stored);
		else {
			decoded.resize(entry.expanded);
			MusicUnpacker unpacker(stored.begin(), stored.size());
			if (!unpacker.unpack(decoded.begin(), decoded.size())) {
				reason = "invalid Amiga compressed resource: " + name;
				return false;
			}
		}
		if (resource.data.empty())
			resource.data.swap(decoded);
		else if (resource.data.size() != decoded.size() ||
				memcmp(resource.data.begin(), decoded.begin(), decoded.size())) {
			reason = "conflicting Amiga music copies: " + name;
			return false;
		}
	}
	if (resource.data.empty()) {
		reason = "missing Amiga music resource: " + name;
		return false;
	}
	_resources.push_back(resource);
	return true;
}

bool AmigaMusicResources::validateModule(const Common::String &name, Common::String &reason) {
	// Keep a private copy while adding dependencies, which can grow the cache.
	const Common::Array<byte> song = find(name.c_str())->data;
	if (song.size() < AmigaPaulaStream::kPatternDataOffset || !READ_BE_UINT16(song.begin() + 4) ||
			!song[AmigaPaulaStream::kOrderCountOffset] || song[AmigaPaulaStream::kOrderCountOffset] > 128) {
		reason = "invalid Amiga module: " + name;
		return false;
	}
	for (uint i = 0; i < song[AmigaPaulaStream::kOrderCountOffset]; ++i) {
		if (AmigaPaulaStream::kPatternDataOffset +
				(song[AmigaPaulaStream::kOrderTableOffset + i] + 1U) * AmigaPaulaStream::kPatternSize > song.size()) {
			reason = "invalid Amiga pattern: " + name;
			return false;
		}
	}
	for (int i = 0; i < AmigaPaulaStream::kNumInstruments; ++i) {
		const byte *record = song.begin() + AmigaPaulaStream::kInstrumentTableOffset +
				i * AmigaPaulaStream::kInstrumentRecordSize;
		if (!record[0])
			continue;
		if (!memchr(record, 0, AmigaPaulaStream::kInstrumentNameSize)) {
			reason = "unterminated Amiga instrument name: " + name;
			return false;
		}
		Common::String sample((const char *)record);
		sample.toUppercase();
		if (!sample.hasSuffix(".AMI") || sample.contains('/') || sample.contains('\\') ||
				sample.contains(':') || !cache(sample, reason)) {
			if (reason.empty())
				reason = "invalid Amiga instrument name: " + name;
			return false;
		}
		const byte *params = record + AmigaPaulaStream::kInstrumentParamsOffset;
		const uint32 length = READ_BE_UINT16(params) * 2U;
		const uint32 loop = READ_BE_UINT16(params + 4);
		const uint32 loopLength = READ_BE_UINT16(params + 6) * 2U;
		const uint32 size = find(sample.c_str())->data.size();
		if ((size & 1) || length > size || (loopLength && loop + loopLength > length)) {
			reason = "invalid Amiga sample/loop: " + sample;
			return false;
		}
	}
	return true;
}

bool AmigaMusicResources::open(const Common::Path &directory,
		const Common::StringArray &modules, Common::String &reason) {
	_entries.clear();
	_resources.clear();
	reason.clear();
	bool valid = !modules.empty() && index(directory, reason);
	for (uint i = 0; valid && i < modules.size(); ++i)
		valid = cache(modules[i], reason) && validateModule(modules[i], reason);
	_entries.clear();
	if (!valid) {
		_resources.clear();
		if (reason.empty())
			reason = "no required DOS music modules found";
	}
	return valid;
}

bool AmigaMusicResources::contains(const char *name) const {
	return find(name) != nullptr;
}

byte *AmigaMusicResources::load(void *context, const char *name, uint32 *size) {
	const Resource *resource = static_cast<AmigaMusicResources *>(context)->find(name);
	if (size)
		*size = 0;
	if (!resource)
		return nullptr;
	byte *copy = (byte *)malloc(resource->data.size());
	if (!copy)
		return nullptr;
	memcpy(copy, resource->data.begin(), resource->data.size());
	if (size)
		*size = resource->data.size();
	return copy;
}

void AmigaMusicResources::release(void *data) {
	free(data);
}

} // End of namespace Cruise
