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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "gob/pregob/emmanuelle/executor.h"

namespace Gob {
namespace Emmanuelle {

const char *oldTotExecutorStopReasonName(OldTotExecutorStopReason reason) {
	switch (reason) {
	case kOldTotExecutorRunning:
		return "running";
	case kOldTotExecutorCompleted:
		return "completed";
	case kOldTotExecutorSuspended:
		return "suspended";
	case kOldTotExecutorHostAbort:
		return "host abort";
	case kOldTotExecutorMalformedData:
		return "malformed data";
	case kOldTotExecutorStackOverflow:
		return "executor stack overflow";
	case kOldTotExecutorAnalysisLimit:
		return "analysis limit";
	default:
		return "unknown";
	}
}

OldTotExecutor::OldTotExecutor(const OldTotData &data, OldTotVM &vm,
		OldTotExecHost &host) :
	_data(data), _vm(vm), _host(host) {
	_steps = 0;
	_maxSteps = 0;

	_started = false;
	_suspended = false;
	_suspendedNode = 0;
	_stopReason = kOldTotExecutorRunning;

	// Input waits may synchronously dispatch a scheduler child. Keep references
	// to the active frame stable up to the executor's enforced stack limit.
	_frames.reserve(256);
}

bool OldTotExecutor::valid(uint32 o, uint32 n) const {
	return o <= _data.size() && n <= _data.size() - o;
}
bool OldTotExecutor::stop(OldTotExecutorStopReason reason) {
	_stopReason = reason;
	return false;
}
bool OldTotExecutor::step() {
	if (_steps != 0xFFFFFFFFU)
		++_steps;
	if (_maxSteps && _steps > _maxSteps)
		return stop(kOldTotExecutorAnalysisLimit);
	return true;
}

bool OldTotExecutor::activeInputNode(uint16 &node) const {
	if (_suspended) {
		node = _suspendedNode;
		return valid(node) && _data.byteAt(node) == 0x1B;
	}
	if (_frames.empty())
		return false;

	const OldTotExecFrame &frame = _frames.back();
	if (frame.kind != OldTotExecFrame::kDispatch ||
		frame.inputState != OldTotExecFrame::kInputPolling ||
		!valid(frame.current) || _data.byteAt(frame.current) != 0x1B)
		return false;
	node = frame.current;
	return true;
}

bool OldTotExecutor::pushDispatch(uint16 offset) {
	if (!offset)
		return true;
	if (!valid(offset))
		return stop(kOldTotExecutorMalformedData);
	if (_frames.size() >= 256)
		return stop(kOldTotExecutorStackOverflow);
	OldTotExecFrame f;
	f.kind = OldTotExecFrame::kDispatch;
	f.current = offset;
	_frames.push_back(f);
	return true;
}

bool OldTotExecutor::replaceWithSequence(uint16 owner, uint16 cursor, byte count) {
	if (_frames.empty())
		return stop(kOldTotExecutorMalformedData);
	OldTotExecFrame f;
	f.kind = OldTotExecFrame::kSequence;
	f.current = owner;
	f.cursor = cursor;
	f.index = 0;
	f.count = count;
	_frames.back() = f;
	return true;
}

bool OldTotExecutor::runRoot() {
	if (!_started) {
		uint16 root;
		if (!_data.getRootSequenceOffset(root) || !valid(root, 2))
			return stop(kOldTotExecutorMalformedData);
		_frames.clear();
		_steps = 0;
		_started = true;
		_suspended = false;
		_suspendedNode = 0;
		_stopReason = kOldTotExecutorRunning;
		OldTotExecFrame f;
		f.kind = OldTotExecFrame::kSequence;
		f.current = root;
		f.cursor = root + 2;
		f.index = 0;
		f.count = _data.byteAt(root + 1);
		_frames.push_back(f);
	}
	return resume();
}

bool OldTotExecutor::resume() {
	if (!_started)
		return runRoot();
	_suspended = false;
	_suspendedNode = 0;
	_stopReason = kOldTotExecutorRunning;
	return runLoop(0);
}

bool OldTotExecutor::dispatch(uint16 offset) {
	if (_suspended)
		return stop(kOldTotExecutorSuspended);
	_stopReason = kOldTotExecutorRunning;
	const uint base = _frames.size();
	if (!pushDispatch(offset))
		return false;
	return runLoop(base);
}

bool OldTotExecutor::dispatchScheduled(uint16 offset) {
	// Timer callbacks are synchronous subroutine calls. Preserve the suspended
	// main continuation underneath their temporary frames.
	const bool wasSuspended = _suspended;
	const uint16 oldNode = _suspendedNode;
	_suspended = false;
	_suspendedNode = 0;
	_stopReason = kOldTotExecutorRunning;
	const uint base = _frames.size();
	if (!pushDispatch(offset)) {
		_suspended = wasSuspended;
		_suspendedNode = oldNode;
		return false;
	}
	const bool ok = runLoop(base);
	if (wasSuspended && !_suspended) {
		_suspended = true;
		_suspendedNode = oldNode;
		_stopReason = kOldTotExecutorSuspended;
	}
	return ok;
}

bool OldTotExecutor::applyBranchRecord(uint16 r) {
	if (!valid(r, 3) || _frames.empty())
		return stop(kOldTotExecutorMalformedData);
	const byte kind = _data.byteAt(r);
	const uint16 target = _data.uint16BEAt(r + 1);
	if (kind == 1) {
		// Tail branches replace the current dispatcher.
		if (!target) {
			_frames.pop_back();
			return true;
		}
		_frames.back().kind = OldTotExecFrame::kDispatch;
		_frames.back().current = target;
		_frames.back().inputState = OldTotExecFrame::kInputInitial;
		return true;
	}
	// Call-style branches return to and re-evaluate the parent construct. A zero
	// target therefore returns to the same construct rather than exiting it.
	if (!target)
		return true;
	return pushDispatch(target);
}

bool OldTotExecutor::processSequence(OldTotExecFrame &f) {
	if (f.index >= f.count) {
		_frames.pop_back();
		return true;
	}
	if (!valid(f.cursor))
		return stop(kOldTotExecutorMalformedData);
	if (_data.byteAt(f.cursor) == 0xFF) {
		OldTotInline in;
		if (!_data.decodeInline(f.cursor, in) || !_vm.executeInline(f.cursor))
			return stop(kOldTotExecutorMalformedData);
		f.cursor += in.size;
		++f.index;
		return true;
	}
	if (!valid(f.cursor, 2))
		return stop(kOldTotExecutorMalformedData);
	const uint16 target = _data.uint16BEAt(f.cursor);
	f.cursor += 2;
	const bool final = (f.index + 1 == f.count);
	++f.index;
	uint16 root = 0;
	const bool isRoot = _data.getRootSequenceOffset(root) && f.current == root;
	if (!final && !isRoot && _host.skipIntermediateSequenceNodes())
		return true;
	if (!target) {
		if (final)
			_frames.pop_back();
		return true;
	}
	if (final) {
		_frames.pop_back();
		return pushDispatch(target);
	}
	return pushDispatch(target);
}

bool OldTotExecutor::processDispatch(OldTotExecFrame &f) {
	const uint16 current = f.current;
	if (!step())
		return false;
	if (!valid(current))
		return stop(kOldTotExecutorMalformedData);
	const byte op = _data.byteAt(current);
	if (op == 0x1C || op == 0x1E) {
		if (!valid(current, 2))
			return stop(kOldTotExecutorMalformedData);
		return replaceWithSequence(current, current + 2, _data.byteAt(current + 1));
	}
	if (op == 0x21) {
		if (!valid(current, 12))
			return stop(kOldTotExecutorMalformedData);
		const byte mode = _data.byteAt(current + 1);
		const uint16 lhs = _vm.readVariable(_data.uint16BEAt(current + 2));
		const uint16 raw = _data.uint16BEAt(current + 4);
		const uint16 rhs = (mode >= 2) ? _vm.readVariable(raw) : raw;
		const bool yes = (mode == 0 || mode == 2)
			? (int16)lhs < (int16)rhs
			: lhs == rhs;
		return applyBranchRecord(current + (yes ? 9 : 6));
	}
	if (op == 0x08) {
		if (!valid(current, 4))
			return stop(kOldTotExecutorMalformedData);
		const uint16 var = _data.uint16BEAt(current + 1);
		const byte n = _data.byteAt(current + 3);
		const uint32 rec = current + 4 + n;
		if (!valid(rec, 3 * (n + 1)))
			return stop(kOldTotExecutorMalformedData);
		uint chosen = 0;
		const uint16 v = _vm.readVariable(var);
		for (uint i = 0; i < n; ++i)
			if (v == _data.byteAt(current + 4 + i)) {
				chosen = i + 1;
				break;
			}
		return applyBranchRecord((uint16)(rec + 3 * chosen));
	}
	if (op == 0x20) {
		if (!valid(current, 3))
			return stop(kOldTotExecutorMalformedData);
		const byte n = _data.byteAt(current + 2);
		if (!n || !valid(current + 3, 2 * n))
			return stop(kOldTotExecutorMalformedData);
		const byte chosen = (byte)_host.randomBounded(n);
		f.current = _data.uint16BEAt(current + 3 + 2 * chosen);
		return true;
	}
	if (op == 0x1B) {
		if (!valid(current, 6))
			return stop(kOldTotExecutorMalformedData);
		const uint16 fallback = _data.uint16BEAt(current + 1);
		const byte n = _data.byteAt(current + 5);
		const uint32 rec = current + 6 + 4 * n;
		if (!valid(rec, 3 * (n + 1)))
			return stop(kOldTotExecutorMalformedData);
		const bool initialEntry =
			f.inputState == OldTotExecFrame::kInputInitial;
		const bool reentering =
			f.inputState == OldTotExecFrame::kInputReenter;
		f.inputState = OldTotExecFrame::kInputPolling;
		const int16 selected = _host.chooseInputRegion(
			current, n, initialEntry, reentering);
		if (selected == -3) {
			_suspended = true;
			_suspendedNode = current;
			_stopReason = kOldTotExecutorSuspended;
			return true;
		}
		if (selected == -2) {
			if (fallback) {
				_frames.back().current = fallback;
				_frames.back().inputState = OldTotExecFrame::kInputInitial;
				return true;
			}
			// The native input routine restarts the same node when cancel is
			// pressed without an explicit fallback target.
			_frames.back().inputState = OldTotExecFrame::kInputReenter;
			_suspended = true;
			_suspendedNode = current;
			_stopReason = kOldTotExecutorSuspended;
			return true;
		}
		const uint idx = (selected >= 0 && selected < n) ? (uint)selected + 1 : 0;
		const uint16 record = (uint16)(rec + 3 * idx);
		if (_data.byteAt(record) != 1)
			_frames.back().inputState = OldTotExecFrame::kInputReenter;
		return applyBranchRecord(record);
	}
	if (op == 0x2A) {
		if (!_vm.installScheduledCycle(current, false))
			return stop(kOldTotExecutorMalformedData);
		_frames.pop_back();
		return true;
	}
	if (op == 0x01) {
		if (!valid(current, 3))
			return stop(kOldTotExecutorMalformedData);
		_host.defineCursor(_data.uint16BEAt(current + 1));
		_frames.pop_back();
		return true;
	}
	if (op == 0x06) {
		_host.animationNode(current);
		_frames.pop_back();
		return true;
	}
	if (op == 0x1D) {
		_host.textNode(current);
		_frames.pop_back();
		return true;
	}
	if (op == 0x24) {
		_host.rememberNode24(current);
		_frames.pop_back();
		return true;
	}
	if (op > 0x3F) {
		_host.drawNode(current);
		_frames.pop_back();
		return true;
	}
	return stop(kOldTotExecutorMalformedData);
}

bool OldTotExecutor::runLoop(uint baseDepth) {
	while (_frames.size() > baseDepth && !_suspended) {
		if (_host.shouldAbortExecution()) {
			_stopReason = kOldTotExecutorHostAbort;
			return true;
		}
		OldTotExecFrame &f = _frames.back();
		if (f.kind == OldTotExecFrame::kDispatch) {
			if (!processDispatch(f))
				return false;
		} else {
			if (!processSequence(f))
				return false;
		}
	}
	_stopReason = _suspended ? kOldTotExecutorSuspended
							 : kOldTotExecutorCompleted;
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
