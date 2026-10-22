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

#ifndef GOB_PREGOB_EMMANUELLE_INTERPRETER_H
#define GOB_PREGOB_EMMANUELLE_INTERPRETER_H

#include "common/array.h"
#include "common/scummsys.h"

#include "gob/pregob/emmanuelle/oldtot.h"

namespace Gob {
namespace Emmanuelle {

struct OldTotRuntimeState {
	OldTotRuntimeState() : videoMode(2), savedVideoMode(2), videoInitMarker(0) {}

	byte videoMode;       // DOS DS:06AE: 0 mono, 1 CGA/Tandy, 2 EGA, 3 PC1512, 4 PC1640
	byte savedVideoMode;  // DOS DS:06B0
	byte videoInitMarker; // DOS DS:06B2; 0x28 means initialized
};

struct OldTotVMTransientState {
	uint savedRegionDepth;
	Common::Array<uint16> schedulerTargets;
	uint schedulerIndex;
	uint16 schedulerReload;
	uint16 schedulerCountdown;
	OldTotVMTransientState() : savedRegionDepth(0), schedulerIndex(0), schedulerReload(0), schedulerCountdown(1) {}
};

/** Platform drawing, input, sound and timing services for the old-TOT VM. */
class OldTotHost {
public:
	virtual ~OldTotHost() {}

	virtual void saveScreenRect(uint16 x, uint16 y, uint16 width, uint16 height) = 0;
	virtual void restoreScreenRect() = 0;
	virtual void waitReturn() = 0;
	virtual void waitInputRelease() = 0;
	virtual void delay(uint16 milliseconds) = 0;

	/** Execute inline FF03 using DOS wait semantics by default.
	 * ST and Amiga also service presentation and palette changes here.
	 */
	virtual void waitOpcode03(byte arg) {
		if (arg == 0)
			waitReturn();
		else if (arg == 0xFF)
			waitInputRelease();
		else
			delay((uint16)arg * 3);
	}

	virtual void fillScreen(byte color) = 0;

	/** Return a value in [0, upperExclusive). */
	virtual uint16 randomBounded(uint16 upperExclusive) = 0;

	/** Convert opcode-2A's raw period to the native loader countdown.
	 * DOS LOADER.COM scales by 100 (or 70 in its alternate timing path) and
	 * starts at one. Atari ST interrupt_ keeps raw periods, maps 200 -> 170,
	 * while Amiga maps 200 -> 60 and every other value -> 25; both install an
	 * initial zero countdown. The default implements DOS timing.
	 */
	virtual void schedulerTiming(byte period, bool timingVariant, uint16 &reload, uint16 &initialCountdown) {
		reload = (uint16)period * (timingVariant ? 70 : 100);
		initialCountdown = 1;
	}

	/** Atari ST tests the countdown before decrementing it. DOS decrements
	 * first and dispatches when the result reaches zero.
	 */
	virtual bool schedulerChecksZeroBeforeDecrement() const { return false; }

	/** Re-enter the old-TOT dispatcher for a timer callback. */
	virtual void dispatchScheduled(uint16 target) = 0;

	/** FF0B 200 starts a fight. DOS and Atari ST use the same arguments:
	 * (4, 6 + 3*v[50], 0, 0, 50, 50).
	 */
	virtual void runFight(uint16 leftMeterBonus, uint16 rightMeterBonus,
										 uint16 leftDamageOption, uint16 rightDamageOption, uint16 leftBaseMeter, uint16 rightBaseMeter) = 0;

	/** Handle platform-specific FF0B commands. OldTotVM handles commands
	 * 100, 101 and 200 before calling this hook. Return false for a no-op.
	 */
	virtual bool handleNativeFF0B(byte, OldTotRuntimeState &) { return false; }

	/** Runtime routine 0x4293 rebuilds the selected DOS video backend. */
	virtual void reinitializeVideoBackend(byte videoMode) = 0;
	virtual bool shouldQuit() const { return false; }

};

/** Interpreter for the old-TOT format, independent of Gob::Inter. */
class OldTotVM {
public:
	OldTotVM(const OldTotData &data, OldTotHost &host);

	bool executeInline(uint16 offset);
	bool installScheduledCycle(uint16 offset, bool timingVariant);
	uint32 advanceTimer(uint32 quanta, uint maxCallbacks);

	uint16 readVariable(uint16 index) const;
	void writeVariable(uint16 index, uint16 value);
	void loadVariables(const uint16 *values, uint count);
	void storeVariables(uint16 *values, uint count) const;

	uint savedRegionDepth() const { return _savedRegionDepth; }
	bool schedulerEnabled() const { return !_schedulerTargets.empty(); }
	void exportTransientState(OldTotVMTransientState &out) const;
	bool importTransientState(const OldTotVMTransientState &in);

	OldTotRuntimeState &runtimeState() { return _runtime; }
	const OldTotRuntimeState &runtimeState() const { return _runtime; }

private:
	uint16 subtractClamp(uint16 lhs, uint16 rhs) const;
	uint16 casinoPhase1Score(uint16 outcome, uint16 code, uint16 stake) const;
	bool casinoPhase2Score(uint16 outcome, uint16 code, uint16 stake, uint slot, uint16 &score) const;
	void casinoPrepareChips();
	void casinoSettle();
	void reconfigureVideoBackend();

	const OldTotData &_data;
	OldTotHost &_host;
	Common::Array<uint16> _variables;
	OldTotRuntimeState _runtime;
	uint _savedRegionDepth;

	Common::Array<uint16> _schedulerTargets;
	uint _schedulerIndex;
	uint16 _schedulerReload;
	uint16 _schedulerCountdown;
	uint32 _schedulerGeneration;
};

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_INTERPRETER_H
