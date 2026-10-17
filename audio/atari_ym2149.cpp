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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "audio/atari_ym2149.h"

#include "common/textconsole.h"

#include <mint/falcon.h>
#include <mint/osbind.h>

namespace Audio {

extern "C" long atariCallMappedRsconf(void *proc, int baud);

namespace {

enum {
	kPreciseTimerFrequency = 6400
};

volatile byte *const kYmSelect = (volatile byte *)0xffff8800L;
volatile byte *const kYmData = (volatile byte *)0xffff8802L;
volatile byte *const kMfpInterruptEnableB = (volatile byte *)0xfffffa09L;
volatile byte *const kMfpInterruptPendingB = (volatile byte *)0xfffffa0dL;
volatile byte *const kMfpInterruptInServiceB = (volatile byte *)0xfffffa11L;
volatile byte *const kMfpInterruptMaskB = (volatile byte *)0xfffffa15L;
volatile byte *const kMfpVector = (volatile byte *)0xfffffa17L;
volatile byte *const kMfpTimerControlCD = (volatile byte *)0xfffffa1dL;
volatile byte *const kMfpTimerDataD = (volatile byte *)0xfffffa25L;
byte kGlobalRsconfToken;

typedef void *AtariRsconfProc;

struct AtariMapTabView {
	void *bconstat;
	void *bconin;
	void *bcostat;
	void *bconout;
	AtariRsconfProc rsconf;
	void *iorec;
};

struct AtariBconmapView {
	AtariMapTabView *maptab;
	int16 maptabSize;
};

} // End of anonymous namespace

class YM2149Atari::TimerDHardware {
public:
	typedef AtariRsconfProc RsconfProc;

	TimerDHardware() : _pendingRsconf(nullptr), _pendingBaud(-1) {
	}

	uint16 enterCritical() {
		uint16 oldSr;
		__asm__ volatile("move.w %%sr,%0\n\tori.w #0x0700,%%sr" :
				"=d"(oldSr) : : "cc", "memory");
		return oldSr;
	}

	void leaveCritical(uint16 oldSr) {
		__asm__ volatile("move.w %0,%%sr" : : "d"(oldSr) : "cc", "memory");
	}

	byte read(AtariMfpTimerDRegister reg) const {
		switch (reg) {
		case kAtariMfpInterruptEnableB:
			return *kMfpInterruptEnableB;
		case kAtariMfpInterruptPendingB:
			return *kMfpInterruptPendingB;
		case kAtariMfpInterruptInServiceB:
			return *kMfpInterruptInServiceB;
		case kAtariMfpInterruptMaskB:
			return *kMfpInterruptMaskB;
		case kAtariMfpVector:
			return *kMfpVector;
		case kAtariMfpTimerControlCD:
			return *kMfpTimerControlCD;
		case kAtariMfpTimerDataD:
			return *kMfpTimerDataD;
		}
		return 0;
	}

	void write(AtariMfpTimerDRegister reg, byte value) {
		switch (reg) {
		case kAtariMfpInterruptEnableB:
			*kMfpInterruptEnableB = value;
			break;
		case kAtariMfpInterruptPendingB:
			*kMfpInterruptPendingB = value;
			break;
		case kAtariMfpInterruptInServiceB:
			*kMfpInterruptInServiceB = value;
			break;
		case kAtariMfpInterruptMaskB:
			*kMfpInterruptMaskB = value;
			break;
		case kAtariMfpVector:
			*kMfpVector = value;
			break;
		case kAtariMfpTimerControlCD:
			*kMfpTimerControlCD = value;
			break;
		case kAtariMfpTimerDataD:
			*kMfpTimerDataD = value;
			break;
		}
	}

	TimerDController::InterruptProc readVector(byte vectorNumber) const {
		TimerDController::InterruptProc *vectors =
				(TimerDController::InterruptProc *)0;
		return vectors[vectorNumber];
	}

	void writeVector(byte vectorNumber,
			TimerDController::InterruptProc interruptProc) {
		TimerDController::InterruptProc *vectors =
				(TimerDController::InterruptProc *)0;
		vectors[vectorNumber] = interruptProc;
	}

	RsconfProc resolveMfpRsconf() const {
		if (Bconmap(0) == 0) {
			const AtariBconmapView *bconmap =
					(const AtariBconmapView *)Bconmap(-2);
			if (!bconmap || !bconmap->maptab || bconmap->maptabSize < 1)
				return nullptr;
			return bconmap->maptab[0].rsconf;
		}
		return &kGlobalRsconfToken;
	}

	long callRsconf(RsconfProc rsconf, int baud) {
		if (rsconf == &kGlobalRsconfToken)
			return Rsconf(baud, -1, -1, -1, -1, -1);
		if (!rsconf || _rsconfCallInstance)
			return -1;

		_pendingRsconf = rsconf;
		_pendingBaud = baud;
		_rsconfCallInstance = this;
		const long result = Supexec(supervisorRsconfCall);
		_rsconfCallInstance = nullptr;
		_pendingRsconf = nullptr;
		_pendingBaud = -1;
		return result;
	}

private:
	static long supervisorRsconfCall() {
		TimerDHardware *hardware = _rsconfCallInstance;
		assert(hardware && hardware->_pendingRsconf);
		return atariCallMappedRsconf(hardware->_pendingRsconf,
				hardware->_pendingBaud);
	}

	static TimerDHardware *_rsconfCallInstance;
	RsconfProc _pendingRsconf;
	int _pendingBaud;
};

YM2149Atari::TimerDHardware *
		YM2149Atari::TimerDHardware::_rsconfCallInstance = nullptr;

YM2149Atari *YM2149Atari::_timerDInstance = nullptr;

YM2149Atari::YM2149Atari()
	: _timerDHardware(new TimerDHardware()),
	  _timerDController(new TimerDController(_timerDHardware)),
	  _timerDSerialAuthority(new TimerDSerialAuthority(_timerDHardware)),
	  _pendingCallback(nullptr), _pendingCallbackRef(nullptr),
	  _insidePreciseCallback(false) {
	for (int r = 0; r < 14; r++)
		_savedRegs[r] = Giaccess(0, r);
}

YM2149Atari::~YM2149Atari() {
	stopPreciseCallback();
	stop();

	for (int r = 0; r < 14; r++)
		Giaccess(_savedRegs[r], r | 0x80);

	delete _timerDController;
	delete _timerDSerialAuthority;
	delete _timerDHardware;

	// mix only the connection matrix to the output
	Soundcmd(ADDERIN, MATIN);
}

bool YM2149Atari::init() {
	// route both PSG channels to the ADC instead of the microphone
	Soundcmd(ADCINPUT, ADCLT|ADCRT);
	// enable and mix both sources (ADC and connection matrix) to the output
	Soundcmd(ADDERIN, MATIN|ADCIN);
	return true;
}

void YM2149Atari::reset() {
	// silence channels/envelope, but keep ports as outputs
	writeReg(7, 0xFF);	// all tone+noise off, PA/PB output
	writeReg(8, 0);		// volume A
	writeReg(9, 0);		// volume B
	writeReg(10, 0);	// volume C
	// skip reg 13 (envelope shape) — writing it retriggers the envelope
}

void YM2149Atari::writeRegDirect(int r, uint8 v) {
	if (r == 7)
		v |= 0xC0;
	*kYmSelect = r;
	*kYmData = v;
}

void YM2149Atari::writeReg(int r, uint8 v) {
	if (_insidePreciseCallback) {
		writeRegDirect(r, v);
		return;
	}
	if (r == 7)
		v |= 0xC0;	// keep Port A/B as outputs
	Giaccess(v, r | 0x80);
}

YM2149::YM2149::PreciseCallbackResult YM2149Atari::startPreciseCallback(
		PreciseCallback callback, void *ref, uint frequency) {
	if (frequency != kPreciseTimerFrequency)
		return kPreciseCallbackUnsupported;
	if (_timerDInstance || _timerDController->state() != TimerDController::kIdle)
		return kPreciseCallbackUnavailable;

	_timerDSerialAuthority->prepare(Supexec(tosVersion) >= 0x0104);
	_pendingCallback = callback;
	_pendingCallbackRef = ref;
	_timerDInstance = this;
	const long result = Supexec(acquireTimerD);
	_pendingCallback = nullptr;
	_pendingCallbackRef = nullptr;
	if (result != TimerDController::kStarted) {
		_timerDInstance = nullptr;
		_timerDSerialAuthority->reset();
		return kPreciseCallbackUnavailable;
	}
	return kPreciseCallbackStarted;
}

void YM2149Atari::suspendPreciseCallback() {
	_timerDController->suspendFromInterrupt();
}

void YM2149Atari::stopPreciseCallback() {
	if (_timerDController->state() == TimerDController::kIdle)
		return;
	assert(_timerDInstance == this);
	const long baud = Supexec(releaseTimerD);
	if (baud >= 0) {
		if (!_timerDSerialAuthority->prepared() ||
				_timerDSerialAuthority->baud() != baud ||
				!_timerDSerialAuthority->restore())
			warning("Unable to restore Atari ST-MFP serial timing");
	} else {
		_timerDSerialAuthority->reset();
	}
	_timerDInstance = nullptr;
}

void YM2149Atari::beginPreciseUpdate() {
	if (_timerDController->state() == TimerDController::kRunning)
		Supexec(beginTimerDUpdate);
}

void YM2149Atari::endPreciseUpdate() {
	if (_timerDController->updateMasked())
		Supexec(endTimerDUpdate);
}

void YM2149Atari::startCallbacks(int timerFrequency) {
	RealChip::startCallbacks(timerFrequency);
}

void YM2149Atari::stopCallbacks() {
	RealChip::stopCallbacks();
}

void YM2149Atari::timerDProc() {
	YM2149Atari *instance = _timerDInstance;
	if (!instance)
		return;
	instance->_insidePreciseCallback = true;
	instance->_timerDController->dispatchInterrupt();
	instance->_insidePreciseCallback = false;
}

long YM2149Atari::acquireTimerD() {
	YM2149Atari *instance = _timerDInstance;
	assert(instance);
	return instance->_timerDController->acquire(instance->_pendingCallback,
			instance->_pendingCallbackRef, timerDProc,
			instance->_timerDSerialAuthority->prepared(),
			instance->_timerDSerialAuthority->baud());
}

long YM2149Atari::releaseTimerD() {
	YM2149Atari *instance = _timerDInstance;
	assert(instance);
	return instance->_timerDController->release();
}

long YM2149Atari::beginTimerDUpdate() {
	YM2149Atari *instance = _timerDInstance;
	assert(instance);
	instance->_timerDController->beginUpdate();
	return 0;
}

long YM2149Atari::endTimerDUpdate() {
	YM2149Atari *instance = _timerDInstance;
	assert(instance);
	instance->_timerDController->endUpdate();
	return 0;
}

long YM2149Atari::tosVersion() {
	const uint32 sysbase = *(volatile uint32 *)0x000004f2L;
	return sysbase ? *(volatile uint16 *)(sysbase + 2) : 0;
}

} // End of namespace Audio
