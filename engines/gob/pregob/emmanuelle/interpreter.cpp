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

#include "gob/pregob/emmanuelle/interpreter.h"

namespace Gob {
namespace Emmanuelle {

static bool contains(const byte *values, uint count, uint16 value) {
	for (uint i = 0; i < count; ++i)
		if (values[i] == value)
			return true;
	return false;
}

static const byte kRouletteRed[] = {
	1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
static const byte kRouletteBlack[] = {
	2, 4, 6, 8, 10, 11, 13, 15, 17, 20, 22, 24, 26, 28, 29, 31, 33, 35};

OldTotVM::OldTotVM(const OldTotData &data, OldTotHost &host) :
	_data(data), _host(host) {
	_savedRegionDepth = 0;

	_schedulerIndex = 0;
	_schedulerReload = 0;
	_schedulerCountdown = 1;

	// Detect scheduler replacement while a callback is running.
	_schedulerGeneration = 0;
}

uint16 OldTotVM::readVariable(uint16 index) const {
	return index < _variables.size() ? _variables[index] : 0;
}

void OldTotVM::writeVariable(uint16 index, uint16 value) {
	if (index >= _variables.size()) {
		const uint oldSize = _variables.size();
		_variables.resize(index + 1);
		for (uint i = oldSize; i < _variables.size(); ++i)
			_variables[i] = 0;
	}
	_variables[index] = value;
}

void OldTotVM::loadVariables(const uint16 *values, uint count) {
	_variables.resize(count);
	for (uint i = 0; i < count; ++i)
		_variables[i] = values ? values[i] : 0;
}

void OldTotVM::storeVariables(uint16 *values, uint count) const {
	if (!values)
		return;
	for (uint i = 0; i < count; ++i)
		values[i] = i < _variables.size() ? _variables[i] : 0;
}

void OldTotVM::exportTransientState(OldTotVMTransientState &out) const {
	out.savedRegionDepth = _savedRegionDepth;
	out.schedulerTargets = _schedulerTargets;
	out.schedulerIndex = _schedulerIndex;
	out.schedulerReload = _schedulerReload;
	out.schedulerCountdown = _schedulerCountdown;
}

bool OldTotVM::importTransientState(const OldTotVMTransientState &in) {
	if (!in.schedulerTargets.empty() && in.schedulerIndex >= in.schedulerTargets.size())
		return false;
	if (in.schedulerTargets.empty() && in.schedulerIndex != 0)
		return false;
	_savedRegionDepth = in.savedRegionDepth;
	_schedulerTargets = in.schedulerTargets;
	_schedulerIndex = in.schedulerIndex;
	_schedulerReload = in.schedulerReload;
	_schedulerCountdown = in.schedulerCountdown;
	++_schedulerGeneration;
	return true;
}

uint16 OldTotVM::subtractClamp(uint16 lhs, uint16 rhs) const {
	const uint16 result = lhs - rhs;
	return (int16)result < 0 ? 0 : result;
}

void OldTotVM::casinoPrepareChips() {
	uint16 bankroll = readVariable(21);
	if (bankroll > 250)
		bankroll = 250;
	writeVariable(21, bankroll);

	uint16 a = bankroll / 10;
	uint16 b = (bankroll % 10) / 5;
	uint16 c = readVariable(211) % 5;

	if (c <= 1 && b != 0) {
		c += 5;
		--b;
	}
	if (c <= 1 && a != 0) {
		c += 5;
		++b;
		--a;
	}
	if (b <= 1 && a != 0) {
		b += 2;
		--a;
	}

	// The native scratch words alias the script-visible variable table.
	writeVariable(210, a);
	writeVariable(212, b);
	writeVariable(213, c);
}

uint16 OldTotVM::casinoPhase1Score(uint16 outcome, uint16 code, uint16 stake) const {
	uint32 score = 0;
	if (outcome >= 1 && outcome <= 12) {
		if (code == 0x89 || code == 0x06)
			score = stake * 3;
		else if (code == 0x86 || code == 0x82)
			score = stake + stake / 2;
	} else if (outcome >= 13 && outcome <= 24) {
		if (code == 0x88 || code == 0x05)
			score = stake * 3;
		else if (code == 0x81 || code == 0x85)
			score = stake + stake / 2;
	} else if (outcome >= 25 && outcome <= 36) {
		if (code == 0x87 || code == 0x04)
			score = stake * 3;
		else if (code == 0x7D || code == 0x85)
			score = stake + stake / 2;
	}

	if (outcome >= 1 && outcome <= 18 && code == 1)
		score = stake * 2;
	else if (outcome >= 19 && outcome <= 36 && code == 7)
		score = stake * 2;

	if (outcome >= 1 && outcome <= 34 && ((outcome - 1) % 3) == 0) {
		if (code == 10)
			score = stake * 3;
		else if (code == 0x84)
			score = stake + stake / 2;
	} else if (outcome >= 2 && outcome <= 35 && ((outcome - 2) % 3) == 0) {
		if (code == 11)
			score = stake * 3;
		else if (code == 0x84 || code == 0x83)
			score = stake + stake / 2;
	} else if (outcome >= 3 && outcome <= 36 && (outcome % 3) == 0) {
		if (code == 12)
			score = stake * 3;
		else if (code == 0x83)
			score = stake + stake / 2;
	}

	if ((outcome & 1) == 0 && code == 8)
		score = stake * 2;
	if ((outcome & 1) != 0 && code == 2)
		score = stake * 2;
	if (contains(kRouletteRed, ARRAYSIZE(kRouletteRed), outcome) && code == 3)
		score = stake * 2;
	if (contains(kRouletteBlack, ARRAYSIZE(kRouletteBlack), outcome) && code == 9)
		score = stake * 2;

	return (uint16)score;
}

bool OldTotVM::casinoPhase2Score(uint16 outcome, uint16 code, uint16 stake,
								 uint slot, uint16 &score) const {
	if (code <= 13 || code >= 119 || outcome < 1 || outcome > 36 || slot < 1 || slot > 3)
		return false;

	const uint j = (outcome - 1) / 3;
	if ((uint)(((outcome - 1) % 3) + 1) != slot)
		return false;
	const uint base = 10 * j;

	if (code == 12 + 2 * slot + base) {
		score = stake * 37;
		return true;
	}

	if (slot == 1) {
		if (code == 15 + base || code == 19 + base || code == 9 + base) {
			score = stake * 19;
			return true;
		}
		if (code == 10 + base || code == 20 + base) {
			score = stake * 9;
			return true;
		}
	} else if (slot == 2) {
		if (code == 11 + base || code == 21 + base || code == 15 + base || code == 17 + base) {
			score = stake * 19;
			return true;
		}
		if (code == 10 + base || code == 20 + base || code == 12 + base || code == 22 + base) {
			score = stake * 9;
			return true;
		}
	} else {
		if (code == 13 + base || code == 17 + base || code == 23 + base) {
			score = stake * 19;
			return true;
		}
		if (code == 12 + base || code == 22 + base) {
			score = stake * 9;
			return true;
		}
	}
	return false;
}

void OldTotVM::casinoSettle() {
	const uint16 outcome = readVariable(127);
	uint32 total = 0;
	for (uint slot = 1; slot <= 3; ++slot) {
		const uint16 code = readVariable(0x77 + slot);
		const uint16 stake = readVariable(0xD8 + slot);
		uint16 score = casinoPhase1Score(outcome, code, stake);
		uint16 replacement = 0;
		if (casinoPhase2Score(outcome, code, stake, slot, replacement))
			score = replacement;
		total += score;
	}
	writeVariable(21, (uint16)(readVariable(21) + total));
}

void OldTotVM::reconfigureVideoBackend() {
	// 0x4293 returns immediately while DS:06B2 == 0x28.
	if (_runtime.videoInitMarker == 0x28)
		return;
	_host.reinitializeVideoBackend(_runtime.videoMode);
	_runtime.videoInitMarker = 0x28;
}

bool OldTotVM::executeInline(uint16 offset) {
	OldTotInline op;
	if (!_data.decodeInline(offset, op))
		return false;

	switch (op.subOpcode) {
	case 0x01:
		if (_savedRegionDepth < 10) {
			_host.saveScreenRect(op.words[0], op.words[1], op.words[2], op.words[3]);
			++_savedRegionDepth;
		}
		break;
	case 0x02:
		if (_savedRegionDepth != 0) {
			_host.restoreScreenRect();
			--_savedRegionDepth;
		}
		break;
	case 0x03:
		_host.waitOpcode03(op.arg);
		break;
	case 0x04:
		_host.fillScreen(op.arg);
		break;
	case 0x05:
		writeVariable(op.words[0], op.words[1]);
		break;
	case 0x06:
		writeVariable(op.words[0], readVariable(op.words[1]));
		break;
	case 0x07:
		writeVariable(op.words[0], readVariable(op.words[0]) + op.words[1]);
		break;
	case 0x08:
		writeVariable(op.words[0], readVariable(op.words[0]) + readVariable(op.words[1]));
		break;
	case 0x09:
		writeVariable(op.words[0], subtractClamp(readVariable(op.words[0]), op.words[1]));
		break;
	case 0x0A:
		writeVariable(op.words[0], subtractClamp(readVariable(op.words[0]), readVariable(op.words[1])));
		break;
	case 0x0B:
		switch (op.arg) {
		case 100:
			casinoPrepareChips();
			break;
		case 101:
			casinoSettle();
			break;
		case 200:
			_host.runFight(4, 6 + 3 * readVariable(50), 0, 0, 50, 50);
			break;
		default:
			// Display and sound commands differ between DOS, ST and Amiga.
			_host.handleNativeFF0B(op.arg, _runtime);
			break;
		}
		break;
	case 0x0C:
		// Operand ignored by DOS handler.
		reconfigureVideoBackend();
		break;
	default:
		return false;
	}

	return true;
}

bool OldTotVM::installScheduledCycle(uint16 offset, bool timingVariant) {
	OldTotScheduledCycle cycle;
	if (!_data.decodeScheduledCycle(offset, cycle))
		return false;

	_schedulerTargets = cycle.targets;
	_schedulerIndex = 0;
	++_schedulerGeneration;
	if (!_schedulerTargets.empty() && cycle.randomizeInitial == 1)
		_schedulerIndex = _host.randomBounded(_schedulerTargets.size());

	_host.schedulerTiming(cycle.period, timingVariant, _schedulerReload, _schedulerCountdown);
	return true;
}

uint32 OldTotVM::advanceTimer(uint32 quanta, uint maxCallbacks) {
	uint callbacks = 0;
	if (_schedulerTargets.empty())
		return 0;

	while (quanta != 0 && !_schedulerTargets.empty()) {
		uint32 untilDispatch = 1;
		if (_schedulerReload != 0) {
			untilDispatch = _schedulerCountdown;
			if (_host.schedulerChecksZeroBeforeDecrement())
				++untilDispatch;
			else if (untilDispatch == 0)
				untilDispatch = 1;
		}

		if (quanta < untilDispatch) {
			_schedulerCountdown -= (uint16)quanta;
			return 0;
		}

		quanta -= untilDispatch;
		_schedulerCountdown = _schedulerReload;
		const uint currentIndex = _schedulerIndex;
		const uint16 target = _schedulerTargets[currentIndex];
		const uint32 generation = _schedulerGeneration;
		_host.dispatchScheduled(target);
		++callbacks;

		if (_host.shouldQuit())
			return 0;
		if (generation == _schedulerGeneration && !_schedulerTargets.empty())
			_schedulerIndex = (currentIndex + 1) % _schedulerTargets.size();
		if (callbacks >= maxCallbacks)
			return quanta;
	}

	return 0;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
