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

#include "gob/pregob/mewilo/executor.h"

namespace Gob {
namespace Mewilo {

static const uint kMaxExecDepth = 256;

MewiloExecutor::MewiloExecutor(MewiloExecHost &host) : _host(host), _tot(nullptr), _dispatchesUntilService(0), _modalDepth(0), _atInput(false) {
	reset();
}

void MewiloExecutor::detachModule() {
	_tot = nullptr;
	_atInput = false;
	_modalDepth = 0;
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
	_atInput = false;
	_dispatchesUntilService = 0;
	return _tot && runFrames(0);
}

bool MewiloExecutor::runFrames(uint baseDepth) {
	while (_frames.size() > baseDepth) {
		if (!serviceHost())
			return false;
		const MewiloExecFrame frame = _frames.back(); // callbacks may grow/reallocate the stack
		if (frame.kind == MewiloExecFrame::kMouse || frame.kind == MewiloExecFrame::kNumbered) {
			if (!stepInput(frame))
				return false;
			continue;
		}
		if (frame.kind == MewiloExecFrame::kContinue) {
			_atInput = true;
			const bool input = _host.waitContinueInput();
			_atInput = false;
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
				if (uint(frame.index) + 1 == items.size())
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
		case 0x1B:
			if (!pushFrame(MewiloExecFrame::kMouse, offset)) return false;
			break;
		case 0x22: {
			if (!_tot->has(offset, 7)) return false;
			const uint16 setup = _tot->uint16BEAt(offset + 3);
			if (!pushFrame(MewiloExecFrame::kNumbered, offset)) return false;
			if (setup && !pushFrame(MewiloExecFrame::kTarget, setup)) return false;
			break;
		}
		case 0x12: {
			uint16 tail = 0;
			++_modalDepth;
			const bool ok = executeFreeText(offset, _frames.size(), tail);
			--_modalDepth;
			if (!ok || (tail && !pushFrame(MewiloExecFrame::kTarget, tail))) return false;
			break;
		}
		default: return false;
		}
	}
	return !_host.shouldQuit() && !_host.hasError();
}

bool MewiloExecutor::executeTarget(uint16 offset, uint depth) {
	if (!offset)
		return true;
	const uint base = _frames.size();
	return depth <= kMaxExecDepth && pushFrame(MewiloExecFrame::kTarget, offset) && runFrames(base);
}

bool MewiloExecutor::executeBranch(const MewiloBranch &branch, uint depth,
		bool &tailTaken, uint16 &tailTarget) {
	tailTaken = branch.target && branch.kind == 1;
	tailTarget = tailTaken ? branch.target : 0;
	return !branch.target || tailTaken || executeTarget(branch.target, depth + 1);
}

bool MewiloExecutor::stepInput(const MewiloExecFrame &frame) {
	const uint16 offset = frame.offset;
	const bool mouse = frame.kind == MewiloExecFrame::kMouse;
	if (!_tot->has(offset, mouse ? 4 : 7)) return false;
	const byte count = _tot->byteAt(offset + (mouse ? 3 : 6));
	const uint32 branchBase = mouse ? offset + 4 + uint32(count) * 4 : offset + 7;
	if (!_tot->has(branchBase, mouse ? uint32(count + 1) * 3 : uint32(count) * 4)) return false;
	MewiloInputEvent event;
	_atInput = true;
	const bool gotInput = _host.waitInput(event);
	_atInput = false;
	if (!gotInput || _host.shouldQuit() || _host.hasError()) return false;
	if (event.key == kMewiloKeyEscape) {
		const uint16 callback = _tot->uint16BEAt(offset + 1);
		return !callback || pushFrame(MewiloExecFrame::kTarget, callback);
	}
	MewiloBranch branch;
	if (mouse) {
		if (!event.mouseButtons) return true;
		int selected = -1;
		for (uint i = 0; i < count; ++i) {
			const uint32 p = offset + 4 + i * 4;
			const int left = _tot->byteAt(p) * 8, top = _tot->byteAt(p + 1);
			if (event.x >= left && event.x < left + _tot->byteAt(p + 2) * 8 &&
				event.y >= top && event.y < top + _tot->byteAt(p + 3)) selected = i;
		}
		branch = _tot->branchAt(branchBase + (selected + 1) * 3);
	} else {
		if (event.key < '1' || event.key >= uint16('1' + count)) return true;
		const uint32 entry = branchBase + (event.key - '1') * 4;
		const byte value = _tot->byteAt(entry);
		if (value <= 2)
			_host.choiceFeedback(value);
		if (_host.shouldQuit() || _host.hasError())
			return false;
		const byte accumulator = _tot->byteAt(offset + 3); // native overlapping access
		if (accumulator < kMewiloVariableCount) {
			_variables[accumulator].word0 = int16(uint16(_variables[accumulator].word0) + value);
			_variables[accumulator].word1 = int16(uint16(_variables[accumulator].word1) + 2);
		}
		branch = _tot->branchAt(entry + 1);
	}
	if (!branch.target) return true;
	if (branch.kind == 1) _frames.pop_back();
	return pushFrame(MewiloExecFrame::kTarget, branch.target);
}

bool MewiloExecutor::exportState(MewiloExecState &out) const {
	if (!atSaveBoundary() || !_tot) return false;
	out.frames = _frames;
	for (uint i = 0; i < kMewiloVariableCount; ++i) out.variables[i] = _variables[i];
	return validateState(*_tot, out);
}

bool MewiloExecutor::validateState(const MewiloTotData &tot, const MewiloExecState &state) {
	if (state.frames.empty() || state.frames.size() > kMaxExecDepth) return false;
	MewiloGraphStats stats;
	Common::Array<uint16> nodes;
	if (!tot.validateReachableGraph(stats, &nodes)) return false;
	for (uint i = 0; i < state.frames.size(); ++i) {
		const MewiloExecFrame &f = state.frames[i];
		uint16 node = f.offset;
		if (f.kind == MewiloExecFrame::kSequence) {
			if (f.offset != tot.rootOffset()) {
				if (!f.offset) return false;
				node = f.offset - 1;
				if (tot.byteAt(node) != 0x1C) return false;
			} else node = 0;
			Common::Array<MewiloSequenceItem> items;
			uint16 end = 0;
			if (!tot.decodeSequence(f.offset, items, end) || !f.index || f.index > items.size()) return false;
		} else if (f.kind == MewiloExecFrame::kContinue) {
			// An inline wait is not an independently addressable graph node. It
			// must be the just-dispatched item of its suspended sequence owner.
			if (f.index || !i || i + 1 != state.frames.size() || !tot.has(f.offset, 3) ||
				tot.byteAt(f.offset) != 0xFF || tot.byteAt(f.offset + 1) != 3 || tot.byteAt(f.offset + 2) != 0)
				return false;
			const MewiloExecFrame &owner = state.frames[i - 1];
			Common::Array<MewiloSequenceItem> items; uint16 end = 0;
			if (owner.kind != MewiloExecFrame::kSequence || !tot.decodeSequence(owner.offset, items, end) ||
				!owner.index || owner.index > items.size() ||
				items[owner.index - 1].type != MewiloSequenceItem::kInlineFF ||
				items[owner.index - 1].offset != f.offset) return false;
			node = 0;
		} else {
			if (f.index || (f.kind != MewiloExecFrame::kMouse && f.kind != MewiloExecFrame::kNumbered)) return false;
			if (tot.byteAt(node) != (f.kind == MewiloExecFrame::kMouse ? 0x1B : 0x22)) return false;
		}
		if (node) {
			bool reachable = false;
			for (uint j = 0; j < nodes.size(); ++j) if (nodes[j] == node) reachable = true;
			if (!reachable) return false;
		}
	}
	const byte top = state.frames.back().kind;
	return top == MewiloExecFrame::kMouse || top == MewiloExecFrame::kNumbered || top == MewiloExecFrame::kContinue;
}

bool MewiloExecutor::importState(const MewiloTotData &tot, const MewiloExecState &state) {
	if (!validateState(tot, state)) return false;
	_tot = &tot;
	_frames = state.frames;
	for (uint i = 0; i < kMewiloVariableCount; ++i) _variables[i] = state.variables[i];
	_atInput = false;
	_modalDepth = 0;
	return true;
}


void MewiloExecutor::adoptState(MewiloExecutor &p) {
	_frames.swap(p._frames);
	_tot = p._tot;
	for (uint i = 0; i < kMewiloVariableCount; ++i) _variables[i] = p._variables[i];
	_atInput = false;
	_modalDepth = 0;
	_dispatchesUntilService = 0;
	p.detachModule();
}

bool MewiloExecutor::parseTextField(uint32 descriptorOffset, MewiloTextField &field,
		uint32 &nextOffset) const {
	if (!_tot || !_tot->has(descriptorOffset, 10))
		return false;
	field.xCell = _tot->byteAt(descriptorOffset + 0);
	field.y = _tot->byteAt(descriptorOffset + 1);
	field.maxChars = _tot->byteAt(descriptorOffset + 2);
	field.serializedHeight = _tot->byteAt(descriptorOffset + 3);
	field.lineAdvance = _tot->byteAt(descriptorOffset + 4);
	field.colorBase = _tot->byteAt(descriptorOffset + 5);
	field.colorInput = _tot->byteAt(descriptorOffset + 6);
	field.colorAlt = _tot->byteAt(descriptorOffset + 7);
	field.reserved = _tot->byteAt(descriptorOffset + 8);
	field.answerLength = _tot->byteAt(descriptorOffset + 9);
	field.answerOffset = descriptorOffset + 10;
	if (!_tot->has(field.answerOffset, field.answerLength))
		return false;
	nextOffset = field.answerOffset + field.answerLength;
	return true;
}

void MewiloExecutor::normalizeAnswer(Common::Array<byte> &text) {
	for (uint i = 0; i < text.size(); ++i) {
		byte &c = text[i];
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		else if (c == 0x16 || c == 0x17 || c == 0x1B || c == 0x82 || c == 0x8A)
			c = 'e';
		else if (c == 0x18 || c == 0x1C || c == 0x85)
			c = 'a';
		else if (c == 0x19 || c == 0x97)
			c = 'u';
		else if (c == 0x1A || c == 0x87)
			c = 'c';
		else if (c == 0x1E || c == 0x1F)
			c = 'i';
		else if (c == 0x1D)
			c = 'o';
		else if ((c >= 0x21 && c <= 0x26) || (c >= 0x28 && c <= 0x2A) ||
				 c == 0x2C || c == 0x2E || c == 0x2F ||
				 (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) ||
				 (c >= 0x7B && c <= 0x7F))
			c = ' ';
	}

	while (!text.empty() && text.front() == ' ')
		text.remove_at(0);
	while (!text.empty() && text.back() == ' ')
		text.remove_at(text.size() - 1);

	for (uint i = 1; i < text.size();) {
		if (text[i - 1] == ' ' && text[i] == ' ')
			text.remove_at(i);
		else
			++i;
	}
}

bool MewiloExecutor::answerMatches(const MewiloTextField &field,
		const Common::Array<byte> &input) const {
	Common::Array<byte> normalizedInput = input;
	normalizeAnswer(normalizedInput);

	if (!_tot || !field.answerLength || !_tot->has(field.answerOffset, field.answerLength))
		return false;

	Common::Array<byte> candidate;
	for (uint i = 0; i < field.answerLength; ++i) {
		const byte c = _tot->byteAt(field.answerOffset + i);
		if (c == '/' || c == 0) {
			Common::Array<byte> normalizedCandidate = candidate;
			normalizeAnswer(normalizedCandidate);
			if (normalizedCandidate.size() == normalizedInput.size() &&
				(normalizedInput.empty() || !memcmp(normalizedCandidate.data(), normalizedInput.data(), normalizedInput.size())))
				return true;
			candidate.clear();
			if (c == 0)
				break;
		} else {
			candidate.push_back(c);
		}
	}
	return false;
}

bool MewiloExecutor::executeFreeText(uint16 offset, uint depth, uint16 &tailTarget) {
	tailTarget = 0;
	if (!_tot->has(offset, 15))
		return false;

	const uint16 escapeCallback = _tot->uint16BEAt(offset + 1);
	const byte scoreVar = _tot->byteAt(offset + 5);
	const byte maxAttempts = _tot->byteAt(offset + 6);
	const byte successThreshold = _tot->byteAt(offset + 7);
	const MewiloBranch success = _tot->branchAt(offset + 8);
	const MewiloBranch failure = _tot->branchAt(offset + 11);
	const byte rejectLength = _tot->byteAt(offset + 14);
	uint32 p = offset + 15;
	if (!_tot->has(p, rejectLength))
		return false;
	const uint32 rejectOffset = p;
	p += rejectLength;
	if (!_tot->has(p))
		return false;
	const byte fieldCount = _tot->byteAt(p++);

	Common::Array<MewiloTextField> fields;
	for (uint i = 0; i < fieldCount; ++i) {
		MewiloTextField field;
		uint32 next = 0;
		if (!parseTextField(p, field, next))
			return false;
		fields.push_back(field);
		p = next;
	}

	byte attemptsRemaining = maxAttempts;
	byte acceptedFields = 0;
	while (attemptsRemaining && !_host.shouldQuit() && !_host.hasError()) {
		acceptedFields = 0;
		for (uint i = 0; i < fields.size(); ++i) {
			Common::Array<byte> input;
			bool completed = false;
			while (!completed && !_host.shouldQuit() && !_host.hasError()) {
				const bool edited = _host.editTextField(fields[i], input);
				if (_host.shouldQuit() || _host.hasError())
					return false;
				if (edited) {
					completed = true;
					break;
				}

				if (escapeCallback) {
					if (!executeTarget(escapeCallback, depth + 1))
						return false;
					// Native Escape callbacks are side calls here; return to the
					// modal field unless the engine itself is quitting.
				}
				input.clear();
			}
			if (_host.shouldQuit() || _host.hasError())
				return false;
			bool handled = false;
			const bool translated = _host.translatedAnswer(*_tot, offset, i, input, handled);
			if (handled ? translated : answerMatches(fields[i], input))
				++acceptedFields;
		}

		--attemptsRemaining;
		if (scoreVar < kMewiloVariableCount) {
			MewiloVar &score = _variables[scoreVar];
			score.word0 = (int16)((uint16)score.word0 + acceptedFields);
			score.word1 = (int16)((uint16)score.word1 + fieldCount);
			if (acceptedFields == fieldCount) {
				score.word0 = (int16)((uint16)score.word0 + acceptedFields * attemptsRemaining);
				score.word1 = (int16)((uint16)score.word1 + fieldCount * attemptsRemaining);
			}
		}

		if (acceptedFields == fieldCount || attemptsRemaining == 0)
			break;

		uint rejectChars = rejectLength;
		if (rejectChars && _tot->byteAt(rejectOffset + rejectChars - 1) == 0)
			--rejectChars;
		_host.showRejectMessageAt(*_tot, offset, _tot->dataAt(rejectOffset), rejectChars);
	}

	if (_host.shouldQuit() || _host.hasError())
		return false;
	const MewiloBranch &branch = acceptedFields >= successThreshold ? success : failure;
	bool tailTaken = false;
	return executeBranch(branch, depth, tailTaken, tailTarget);
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
