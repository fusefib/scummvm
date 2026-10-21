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

#ifndef GLK_ACCURATE_MANAGED_SURFACE_H
#define GLK_ACCURATE_MANAGED_SURFACE_H

#include "graphics/managed_surface.h"

namespace Glk {

// Same-format picture copies; decoding and format conversion stay in ManagedSurface.
class AccurateManagedSurface : public Graphics::ManagedSurface {
public:
	AccurateManagedSurface();
	AccurateManagedSurface(int width, int height, const Graphics::PixelFormat &pixelFormat);
	AccurateManagedSurface(Graphics::ManagedSurface &owner, const Common::Rect &bounds);

	void accurateBlitFrom(const Graphics::Surface &source, const Common::Rect &sourceRect, const Common::Rect &destRect);
	void accurateTransBlitFrom(const Graphics::Surface &source, const Common::Rect &sourceRect,
		const Common::Rect &destRect, uint32 transparent);
};

} // End of namespace Glk

#endif
