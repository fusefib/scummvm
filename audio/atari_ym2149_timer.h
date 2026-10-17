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

#ifndef AUDIO_ATARI_YM2149_TIMER_H
#define AUDIO_ATARI_YM2149_TIMER_H

#include "common/scummsys.h"

namespace Audio {

enum AtariMfpTimerDRegister {
	kAtariMfpInterruptEnableB,
	kAtariMfpInterruptPendingB,
	kAtariMfpInterruptInServiceB,
	kAtariMfpInterruptMaskB,
	kAtariMfpVector,
	kAtariMfpTimerControlCD,
	kAtariMfpTimerDataD
};

template<class System>
class AtariTimerDSerialAuthority {
public:
	typedef typename System::RsconfProc RsconfProc;

	AtariTimerDSerialAuthority(System *system)
		: _system(system), _rsconf(nullptr), _baud(-1) {
	}

	bool prepare(bool querySupported) {
		reset();
		if (!querySupported)
			return false;
		_rsconf = _system->resolveMfpRsconf();
		if (!_rsconf)
			return false;
		const long baud = _system->callRsconf(_rsconf, -2);
		if (baud < 0 || baud > 15) {
			reset();
			return false;
		}
		_baud = (int)baud;
		return true;
	}

	bool restore() {
		if (!_rsconf || _baud < 0 || _baud > 15)
			return false;
		const RsconfProc rsconf = _rsconf;
		const int baud = _baud;
		_system->callRsconf(rsconf, baud);
		const bool restored = _system->callRsconf(rsconf, -2) == baud;
		reset();
		return restored;
	}

	void reset() {
		_rsconf = nullptr;
		_baud = -1;
	}

	bool prepared() const { return _rsconf != nullptr; }
	int baud() const { return _baud; }

private:
	System *_system;
	RsconfProc _rsconf;
	int _baud;
};

template<class Hardware>
class AtariTimerDController {
public:
	typedef void (*ClientProc)(void *ref);
	typedef void (*InterruptProc)();

	enum State {
		kIdle,
		kRunning,
		kSuspended
	};

	enum AcquireResult {
		kStarted,
		kUnavailable
	};

	AtariTimerDController(Hardware *hardware)
		: _hardware(hardware), _clientProc(nullptr), _clientRef(nullptr),
		  _savedVector(nullptr), _savedTimerData(0), _savedInterruptMask(0),
		  _savedBaud(-1), _state(kIdle), _updateDepth(0),
		  _updateMaskWasEnabled(false) {
	}

	AcquireResult acquire(ClientProc clientProc, void *clientRef,
			InterruptProc interruptProc, bool baudQuerySupported,
			int baud) {
		if (_state != kIdle || !clientProc || !interruptProc)
			return kUnavailable;

		const uint16 oldSr = _hardware->enterCritical();
		const byte timerControl =
				_hardware->read(kAtariMfpTimerControlCD);
		const bool timerRunning = (timerControl & 0x0f) != 0;
		if ((_hardware->read(kAtariMfpInterruptEnableB) & kInterruptMask) ||
				(timerRunning && (!baudQuerySupported || baud < 0 || baud > 15))) {
			_hardware->leaveCritical(oldSr);
			return kUnavailable;
		}

		_savedVectorNumber =
				(_hardware->read(kAtariMfpVector) & 0xf0) + kInterruptNumber;
		_savedVector = _hardware->readVector(_savedVectorNumber);
		_savedTimerData = timerRunning ? 0 :
				_hardware->read(kAtariMfpTimerDataD);
		_savedInterruptMask =
				_hardware->read(kAtariMfpInterruptMaskB) & kInterruptMask;
		_savedBaud = timerRunning ? baud : -1;
		_clientProc = clientProc;
		_clientRef = clientRef;

		writeTimerControl(0);
		_hardware->write(kAtariMfpInterruptEnableB,
				_hardware->read(kAtariMfpInterruptEnableB) & ~kInterruptMask);
		_hardware->write(kAtariMfpInterruptMaskB,
				_hardware->read(kAtariMfpInterruptMaskB) & ~kInterruptMask);
		clearTimerInterrupts();
		_hardware->writeVector(_savedVectorNumber, interruptProc);
		_hardware->write(kAtariMfpTimerDataD, kTimerData);
		_hardware->write(kAtariMfpInterruptEnableB,
				_hardware->read(kAtariMfpInterruptEnableB) | kInterruptMask);
		_hardware->write(kAtariMfpInterruptMaskB,
				_hardware->read(kAtariMfpInterruptMaskB) | kInterruptMask);
		writeTimerControl(kTimerControl);
		_state = kRunning;
		_hardware->leaveCritical(oldSr);
		return kStarted;
	}

	int release() {
		if (_state == kIdle)
			return -1;

		const uint16 oldSr = _hardware->enterCritical();
		writeTimerControl(0);
		_hardware->write(kAtariMfpInterruptEnableB,
				_hardware->read(kAtariMfpInterruptEnableB) & ~kInterruptMask);
		_hardware->write(kAtariMfpInterruptMaskB,
				_hardware->read(kAtariMfpInterruptMaskB) & ~kInterruptMask);
		clearTimerInterrupts();
		_hardware->writeVector(_savedVectorNumber, _savedVector);
		if (_savedBaud < 0) {
			_hardware->write(kAtariMfpTimerDataD, _savedTimerData);
			writeTimerControl(0);
		}
		// Acquisition requires Timer D's interrupt to be disabled.
		_hardware->write(kAtariMfpInterruptEnableB,
				_hardware->read(kAtariMfpInterruptEnableB) & ~kInterruptMask);
		_hardware->write(kAtariMfpInterruptMaskB,
				(_hardware->read(kAtariMfpInterruptMaskB) & ~kInterruptMask) |
				_savedInterruptMask);
		const int baud = _savedBaud;
		_clientProc = nullptr;
		_clientRef = nullptr;
		_savedBaud = -1;
		_state = kIdle;
		_updateDepth = 0;
		_updateMaskWasEnabled = false;
		_hardware->leaveCritical(oldSr);
		return baud;
	}

	void beginUpdate() {
		const uint16 oldSr = _hardware->enterCritical();
		if (_state == kRunning) {
			if (_updateDepth == 0) {
				const byte mask = _hardware->read(kAtariMfpInterruptMaskB);
				_updateMaskWasEnabled = (mask & kInterruptMask) != 0;
				_hardware->write(kAtariMfpInterruptMaskB,
						mask & ~kInterruptMask);
			}
			++_updateDepth;
		}
		_hardware->leaveCritical(oldSr);
	}

	void endUpdate() {
		const uint16 oldSr = _hardware->enterCritical();
		if (_updateDepth != 0 && --_updateDepth == 0) {
			byte mask = _hardware->read(kAtariMfpInterruptMaskB) &
					~kInterruptMask;
			if (_updateMaskWasEnabled)
				mask |= kInterruptMask;
			_hardware->write(kAtariMfpInterruptMaskB, mask);
			_updateMaskWasEnabled = false;
		}
		_hardware->leaveCritical(oldSr);
	}

	void suspendFromInterrupt() {
		if (_state != kRunning)
			return;
		writeTimerControl(0);
		_state = kSuspended;
	}

	void dispatchInterrupt() {
		if (_state == kRunning && _clientProc)
			(*_clientProc)(_clientRef);
		_hardware->write(kAtariMfpInterruptInServiceB,
				(byte)~kInterruptMask);
	}

	State state() const { return _state; }
	bool updateMasked() const { return _updateDepth != 0; }

	static byte interruptMask() { return kInterruptMask; }
	static byte timerControl() { return kTimerControl; }
	static byte timerData() { return kTimerData; }

private:
	enum {
		kInterruptNumber = 4,
		kInterruptMask = 1 << kInterruptNumber,
		kTimerControl = 3,
		kTimerData = 24
	};

	void writeTimerControl(byte timerDControl) {
		_hardware->write(kAtariMfpTimerControlCD,
				(_hardware->read(kAtariMfpTimerControlCD) & 0xf0) |
				(timerDControl & 0x0f));
	}

	void clearTimerInterrupts() {
		_hardware->write(kAtariMfpInterruptPendingB,
				(byte)~kInterruptMask);
		_hardware->write(kAtariMfpInterruptInServiceB,
				(byte)~kInterruptMask);
	}

	Hardware *_hardware;
	ClientProc _clientProc;
	void *_clientRef;
	InterruptProc _savedVector;
	byte _savedVectorNumber;
	byte _savedTimerData;
	byte _savedInterruptMask;
	int _savedBaud;
	volatile State _state;
	uint16 _updateDepth;
	bool _updateMaskWasEnabled;
};

} // End of namespace Audio

#endif
