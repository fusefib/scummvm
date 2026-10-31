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

#include "wizardry/vm/random.h"

namespace Wizardry {

void NativeRandom::reset(Algorithm algorithm, uint16 state[4]) {
	static const uint16 initial[2][4] = {
		{0x5bab, 0xd02b, 0x7e15, 0x7351}, {3000, 1, 29999, 0}};
	memcpy(state, initial[algorithm], sizeof(initial[0]));
}

void NativeRandom::stir(Algorithm algorithm, uint16 state[4]) {
	if (algorithm == kFourWord) {
		static const uint16 increment[4] = {0x0183, 0x7fc9, 0x3619, 0xff8b};
		for (uint i = 0; i < 4; ++i)
			state[i] = uint16(state[i] + increment[i]);
		return;
	}
	static const int divisor[3] = {177, 176, 178};
	static const int multiplier[3] = {171, 172, 170};
	static const int subtract[3] = {2, 35, 63};
	static const int modulus[3] = {30269, 30307, 30323};
	for (uint i = 0; i < 3; ++i) {
		const int32 value = int16(state[i]);
		const int32 quotient = value / divisor[i];
		const int32 remainder = value % divisor[i];
		const int16 next = int16(multiplier[i] * remainder - subtract[i] * quotient);
		state[i] = uint16(next < 0 ? int32(next) + modulus[i] : next);
	}
	state[3] = 0;
}

uint16 NativeRandom::next(Algorithm algorithm, uint16 state[4]) {
	if (algorithm == kFourWord) {
		static const uint16 increment[4] = {0x3619, 0xff8b, 0x0183, 0x7fc9};
		static const uint16 multiplier[4] = {0x6a2d, 0xfff1, 0xffaf, 0xffd9};
		for (uint i = 0; i < 4; ++i)
			state[i] = uint16(uint32(state[i]) * multiplier[i] + increment[i]);
		const uint16 swapped = uint16((state[3] << 8) | (state[3] >> 8));
		return swapped & 0x700f;
	}
	stir(algorithm, state);
	static const uint16 modulus[3] = {30269, 30307, 30323};
	uint32 result = 0;
	for (uint i = 0; i < 3; ++i)
		result += 10000U * uint32(state[i] % modulus[i]) / modulus[i];
	return result & 0x7fff;
}

void NativeRandom::poll(Algorithm algorithm, uint16 state[4], bool available) {
	stir(algorithm, state);
	if (algorithm == kFourWord && !available)
		state[0] = state[1] = 0;
}

} // namespace Wizardry
