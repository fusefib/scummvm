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

#ifndef AUDIO_YM2149_H
#define AUDIO_YM2149_H

#include "audio/chip.h"
#include "common/scummsys.h"

namespace YM2149 {

class YM2149;

class Config {
public:
	/**
	 * Creates a YM2149 driver.
	 */
	static YM2149 *create();
};

class YM2149 : virtual public Audio::Chip {
private:
	static bool _hasInstance;

public:
	typedef void (*PreciseCallback)(void *ref);

	enum PreciseCallbackResult {
		kPreciseCallbackStarted,
		kPreciseCallbackUnsupported,
		kPreciseCallbackUnavailable
	};

	YM2149();
	virtual ~YM2149();

	/**
	 * Initializes the YM2149 emulator.
	 *
	 * @return		true on success, false on failure
	 */
	virtual bool init() = 0;

	/**
	 * Reinitializes the YM2149 emulator
	 */
	virtual void reset() = 0;

	/**
	 * Function to directly write to a specific YM2149 register.
	 *
	 * @param r		hardware register number to write to
	 * @param v		value, which will be written
	 */
	virtual void writeReg(int r, uint8 v) = 0;

	/**
	 * Starts callback delivery with a hardware-defined precise clock.
	 *
	 * This foreground-only API is mutually exclusive with Chip::start(). The
	 * callback runs in interrupt context and must not allocate, block, log,
	 * invoke OS services, or release resources.
	 */
	virtual PreciseCallbackResult startPreciseCallback(PreciseCallback callback,
			void *ref, uint frequency) {
		(void)callback;
		(void)ref;
		(void)frequency;
		return kPreciseCallbackUnsupported;
	}

	/** Stops callback generation from interrupt context. */
	virtual void suspendPreciseCallback() {}

	/**
	 * Releases precise callback ownership from foreground context.
	 *
	 * This is idempotent and also consumes outstanding update guards.
	 */
	virtual void stopPreciseCallback() {}

	/**
	 * Guards foreground state changes against the precise callback.
	 *
	 * Guards may nest. Only the outer pair changes hardware masking. They do
	 * not affect ordinary Chip callbacks.
	 */
	virtual void beginPreciseUpdate() {}
	virtual void endPreciseUpdate() {}
};

} // End of namespace YM2149

#endif
