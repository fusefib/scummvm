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

#include "common/util.h"

#include "gob/pregob/mewilo/executor.h"

namespace Gob {
namespace Mewilo {

static const uint kMaxExecDepth = 256;

MewiloExecutor::MewiloExecutor(MewiloExecHost &host) : _host(host), _tot(nullptr), _dispatchesUntilService(0) {
	reset();
}

void MewiloExecutor::detachModule() {
	_tot = nullptr;
	_frames.clear();
	_dispatchesUntilService = 0;
}

void MewiloExecutor::reset() {
	detachModule();
	for (uint i = 0; i < kMewiloVariableCount; ++i) {
		_variables[i].word0 = 0;
		_variables[i].word1 = 0;
	}

	// The Atari startup clears cells 0..50 and initializes selector cell 0x33
	// separately to FF/FF.
	_variables[0x33].word0 = 0x00FF;
	_variables[0x33].word1 = 0x00FF;
}

bool MewiloExecutor::executeRoot(const MewiloTotData &tot) {
	if (!tot.valid())
		return false;
	_tot = &tot;
	_frames.clear();
	_dispatchesUntilService = 0;
	return pushFrame(MewiloExecFrame::kSequence, tot.rootOffset()) && resume();
}

bool MewiloExecutor::serviceHost() {
	if (_host.hasError()) return false;
	if (!_dispatchesUntilService) {
		_host.serviceEvents();
		_dispatchesUntilService = 256;
	}
	--_dispatchesUntilService;
	return !_host.shouldQuit() && !_host.hasError();
}

bool MewiloExecutor::pushFrame(byte kind, uint16 offset, uint16 index) {
	if (_frames.size() >= kMaxExecDepth)
		return false;
	_frames.push_back(MewiloExecFrame(kind, offset, index));
	return true;
}

bool MewiloExecutor::resume() {
	_dispatchesUntilService = 0;
	return _tot && runFrames(0);
}

bool MewiloExecutor::runFrames(uint baseDepth) {
	while (_frames.size() > baseDepth) {
		if (!serviceHost())
			return false;
		const MewiloExecFrame frame = _frames.back(); // callbacks may grow/reallocate the stack
		if (frame.kind == MewiloExecFrame::kContinue) {
			const bool input = _host.waitContinueInput();
			if (!input || _host.shouldQuit() || _host.hasError() || !_host.waitInputRelease())
				return false;
			_frames.pop_back();
			continue;
		}
		if (frame.kind == MewiloExecFrame::kSequence) {
			Common::Array<MewiloSequenceItem> items;
			uint16 end = 0;
			if (!_tot->decodeSequence(frame.offset, items, end) || frame.index > items.size())
				return false;
			if (frame.index == items.size()) {
				_frames.pop_back();
				continue;
			}
			const MewiloSequenceItem item = items[frame.index];
			++_frames.back().index; // a resumed caller must not repeat a completed effect
			if (item.type == MewiloSequenceItem::kInlineFF) {
				if (!executeInlineFF(item.offset))
					return false;
			} else if (item.target) {
				if (frame.index + 1 == items.size())
					_frames.pop_back(); // retain the original last-item tail optimization
				if (!pushFrame(MewiloExecFrame::kTarget, item.target))
					return false;
			}
			continue;
		}
		_frames.pop_back();
		const uint16 offset = frame.offset;
		if (frame.kind != MewiloExecFrame::kTarget || !_tot->has(offset) || offset < 16)
			return false;
		switch (_tot->byteAt(offset)) {
		case 0x1C:
			if (!pushFrame(MewiloExecFrame::kSequence, offset + 1)) return false;
			break;
		case 0x1D: _host.drawTextNode(*_tot, offset); break;
		case 0x41: _host.drawInlineImage(*_tot, offset); break;
		case 0x60: _host.drawExternalImage(*_tot, offset); break;
		case 0x20: {
			const byte count = _tot->byteAt(offset + 1);
			if (!count || !_tot->has(offset + 2, count * 2)) return false;
			const uint16 index = _host.randomModulo(count);
			if (index >= count) return false;
			const uint16 target = _tot->uint16BEAt(offset + 2 + index * 2);
			if (target && !pushFrame(MewiloExecFrame::kTarget, target)) return false;
			break;
		}
		case 0x21: {
			if (!_tot->has(offset, 10)) return false;
			const MewiloBranch branch = _tot->branchAt(offset + (conditionTrue(offset) ? 4 : 7));
			if (branch.target && !pushFrame(MewiloExecFrame::kTarget, branch.target)) return false;
			break;
		}
		default: return false;
		}
	}
	return !_host.shouldQuit() && !_host.hasError();
}

bool MewiloExecutor::conditionTrue(uint16 offset) const {
	const byte mode = _tot->byteAt(offset + 1);
	const byte lhsIndex = _tot->byteAt(offset + 2);
	const byte rhs = _tot->byteAt(offset + 3);
	if (lhsIndex >= kMewiloVariableCount)
		return false;

	const int16 lhs = _variables[lhsIndex].word0;
	switch (mode) {
	case 0:
		return lhs >= (int16)rhs;
	case 1:
		return lhs != (int16)rhs;
	case 2:
		return rhs < kMewiloVariableCount && lhs >= _variables[rhs].word0;
	case 3:
		return rhs < kMewiloVariableCount && lhs != _variables[rhs].word0;
	default:
		return false;
	}
}

bool MewiloExecutor::executeInlineFF(uint16 offset) {
	if (!_tot || !_tot->has(offset, 2) || _tot->byteAt(offset) != 0xFF)
		return false;

	const byte sub = _tot->byteAt(offset + 1);
	const byte size = MewiloTotData::inlineFFSize(sub);
	if (!size || !_tot->has(offset, size))
		return false;

	switch (sub) {
	case 0x01:
		_host.saveRect(_tot->byteAt(offset + 2), _tot->byteAt(offset + 3),
			_tot->byteAt(offset + 4), _tot->byteAt(offset + 5));
		break;
	case 0x02:
		_host.restoreRect();
		break;
	case 0x03:
		if (!_tot->byteAt(offset + 2)) return pushFrame(MewiloExecFrame::kContinue, offset);
		_host.waitOrDelayAt(*_tot, offset, _tot->byteAt(offset + 2));
		break;
	case 0x04:
		_host.fillScreen(_tot->byteAt(offset + 2));
		break;
	case 0x05: {
		const byte dst = _tot->byteAt(offset + 2);
		const byte value = _tot->byteAt(offset + 3);
		if (dst < kMewiloVariableCount)
			_variables[dst].word0 = _variables[dst].word1 = value;
		break;
	}
	case 0x06: {
		const byte dst = _tot->byteAt(offset + 2);
		const byte src = _tot->byteAt(offset + 3);
		if (dst < kMewiloVariableCount && src < kMewiloVariableCount)
			_variables[dst] = _variables[src];
		break;
	}
	case 0x07: {
		const byte dst = _tot->byteAt(offset + 2);
		const byte value = _tot->byteAt(offset + 3);
		if (dst < kMewiloVariableCount) {
			MewiloVar &v = _variables[dst];
			const int16 sum = (int16)((uint16)v.word0 + value);
			if (sum < (int16)value)
				v.word0 = v.word1 = 255;
			else {
				v.word0 = sum;
				v.word1 = (int16)((uint16)v.word1 + value);
			}
		}
		break;
	}
	case 0x08: {
		const byte dst = _tot->byteAt(offset + 2);
		const byte src = _tot->byteAt(offset + 3);
		if (dst < kMewiloVariableCount && src < kMewiloVariableCount) {
			MewiloVar &d = _variables[dst];
			const MewiloVar &s = _variables[src];
			const int16 sum = (int16)((uint16)d.word0 + (uint16)s.word0);
			// Native/generated-code quirk: compare against the raw source index.
			if (sum < (int16)src)
				d.word0 = d.word1 = 255;
			else {
				d.word0 = sum;
				d.word1 = (int16)((uint16)d.word1 + (uint16)s.word1);
			}
		}
		break;
	}
	case 0x09: {
		const byte dst = _tot->byteAt(offset + 2);
		const byte value = _tot->byteAt(offset + 3);
		if (dst < kMewiloVariableCount) {
			MewiloVar &v = _variables[dst];
			if (v.word0 < (int16)value)
				v.word0 = v.word1 = 0;
			else {
				v.word0 = (int16)((uint16)v.word0 - value);
				v.word1 = (int16)((uint16)v.word1 - value);
			}
		}
		break;
	}
	case 0x0A: {
		const byte dst = _tot->byteAt(offset + 2);
		const byte src = _tot->byteAt(offset + 3);
		if (dst < kMewiloVariableCount && src < kMewiloVariableCount) {
			MewiloVar &d = _variables[dst];
			const MewiloVar &s = _variables[src];
			if (d.word0 < s.word0)
				d.word0 = d.word1 = 0;
			else {
				d.word0 = (int16)((uint16)d.word0 - (uint16)s.word0);
				d.word1 = (int16)((uint16)d.word1 - (uint16)s.word1);
			}
		}
		break;
	}
	case 0x0B:
		_host.specialCommand(_tot->byteAt(offset + 2), _variables, kMewiloVariableCount);
		break;
	default:
		return false;
	}
	return true;
}

} // End of namespace Mewilo
} // End of namespace Gob
