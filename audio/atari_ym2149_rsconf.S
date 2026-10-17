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

#include "../backends/platform/atari/symbols.h"

	.global SYM(atariCallMappedRsconf)

	.text

/* long atariCallMappedRsconf(void *proc, int baud); */
SYM(atariCallMappedRsconf):
#ifdef __FASTCALL__
	move.l	a0,a1
#else
	move.l	4(sp),a1
	move.l	8(sp),d0
#endif
	move.w	#-1,-(sp)
	move.w	#-1,-(sp)
	move.w	#-1,-(sp)
	move.w	#-1,-(sp)
	move.w	#-1,-(sp)
	move.w	d0,-(sp)
	jsr	(a1)
	lea	12(sp),sp
	rts
