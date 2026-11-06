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

#ifndef GOB_PREGOB_EMMANUELLE_EXECUTOR_H
#define GOB_PREGOB_EMMANUELLE_EXECUTOR_H

#include "common/array.h"
#include "common/scummsys.h"
#include "gob/pregob/emmanuelle/interpreter.h"
#include "gob/pregob/emmanuelle/oldtot.h"

namespace Gob {
namespace Emmanuelle {

class OldTotExecHost {
public:
	virtual ~OldTotExecHost() {}
	virtual uint16 randomBounded(uint16 upperExclusive) = 0;
	/** Return -1=no region, -2=explicit fallback, -3=suspend, or 0..count-1. */
	virtual int16 chooseInputRegion(uint16 nodeOffset, byte count,
									bool initialEntry, bool reentering) = 0;
	virtual void defineCursor(uint16 descriptorOffset) = 0;
	virtual void drawNode(uint16 nodeOffset) = 0;
	virtual void textNode(uint16 nodeOffset) = 0;
	virtual void animationNode(uint16 nodeOffset) = 0;
	virtual bool shouldAbortExecution() const { return false; }
	virtual bool executionFailed() const { return false; }
	/** Command 201 suppresses intermediate sequence entries on ST and Amiga. */
	virtual bool skipIntermediateSequenceNodes() const { return false; }
};

struct OldTotExecFrame {
	enum Kind { kDispatch,
				kSequence };
	enum InputState { kInputInitial,
					  kInputPolling,
					  kInputReenter };
	OldTotExecFrame() {
		kind = kDispatch;
		current = 0;
		cursor = 0;

		index = 0;
		count = 0;
		inputState = kInputInitial;
	}
	Kind kind;
	uint16 current; // dispatch offset OR sequence owner/root sentinel
	uint16 cursor;  // sequence data cursor
	byte index;
	byte count;
	InputState inputState;
};

struct OldTotExecutorState {
	Common::Array<OldTotExecFrame> frames;
	uint32 steps;
	bool started;
	bool suspended;
	uint16 suspendedNode;
	OldTotExecutorState() : steps(0), started(false), suspended(false), suspendedNode(0) {}
};

enum OldTotExecutorStopReason {
	kOldTotExecutorRunning = 0,
	kOldTotExecutorCompleted,
	kOldTotExecutorSuspended,
	kOldTotExecutorHostAbort,
	kOldTotExecutorHostFailure,
	kOldTotExecutorMalformedData,
	kOldTotExecutorStackOverflow,
	kOldTotExecutorAnalysisLimit
};

const char *oldTotExecutorStopReasonName(OldTotExecutorStopReason reason);

class OldTotExecutor {
public:
	enum { kMaxFrames = 256 };

	OldTotExecutor(const OldTotData &data, OldTotVM &vm, OldTotExecHost &host);
	/** Run the module's root sequence, resuming it if already started. */
	bool runRoot();
	/** Resume a suspended executor. */
	bool resume();
	/** Run a standalone dispatch on the current continuation stack. */
	bool dispatch(uint16 offset);
	bool dispatchScheduled(uint16 offset);
	/** Limit execution steps for tests. Zero disables the limit. */
	void setMaxSteps(uint32 n) { _maxSteps = n; }
	uint32 steps() const { return _steps; }
	bool suspended() const { return _suspended; }
	uint16 suspendedNode() const { return _suspendedNode; }
	/** Return the opcode-1B node that currently owns the logical input wait. */
	bool activeInputNode(uint16 &node) const;
	bool finished() const { return _started && _frames.empty() && !_suspended; }
	OldTotExecutorStopReason stopReason() const { return _stopReason; }

	void exportState(OldTotExecutorState &out) const;
	/** Export the active input wait as a suspended state without mutating it. */
	bool exportSuspendedInputState(OldTotExecutorState &out) const;
	bool importState(const OldTotExecutorState &in);

private:
	bool valid(uint32 offset, uint32 length = 1) const;
	bool stop(OldTotExecutorStopReason reason);
	bool step();
	bool pushDispatch(uint16 offset);
	bool replaceWithSequence(uint16 owner, uint16 cursor, byte count);
	bool applyBranchRecord(uint16 record);
	bool runLoop(uint baseDepth);
	bool processDispatch(OldTotExecFrame &frame);
	bool processSequence(OldTotExecFrame &frame);

	const OldTotData &_data;
	OldTotVM &_vm;
	OldTotExecHost &_host;
	Common::Array<OldTotExecFrame> _frames;
	uint32 _steps;
	uint32 _maxSteps;
	bool _started;
	bool _suspended;
	uint16 _suspendedNode;
	OldTotExecutorStopReason _stopReason;
};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_EXECUTOR_H
