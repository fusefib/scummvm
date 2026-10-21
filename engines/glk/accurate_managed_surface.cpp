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

#include "glk/accurate_managed_surface.h"

namespace Glk {

AccurateManagedSurface::AccurateManagedSurface() : Graphics::ManagedSurface() {
}

AccurateManagedSurface::AccurateManagedSurface(int width, int height, const Graphics::PixelFormat &pixelFormat) :
		Graphics::ManagedSurface(width, height, pixelFormat) {
}

AccurateManagedSurface::AccurateManagedSurface(Graphics::ManagedSurface &owner, const Common::Rect &bounds) :
		Graphics::ManagedSurface(owner, bounds) {
}

template<class Pixel>
static void copyScaled(Graphics::ManagedSurface &dest, const Graphics::Surface &source,
		const Common::Rect &sourceRect, const Common::Rect &destRect, bool keyed, uint32 transparent) {
	if (sourceRect.isEmpty() || destRect.isEmpty())
		return;
	assert(source.format == dest.format);
	assert(Common::Rect(source.w, source.h).contains(sourceRect));
	Common::Rect visible = destRect;
	visible.clip(dest.getBounds());
	if (visible.isEmpty())
		return;

	// Rounding upwards keeps floor(i * source / destination) exact for Rect's bounds.
	const uint64 stepX = (((uint64)sourceRect.width() << 32) + destRect.width() - 1) / destRect.width();
	const uint64 stepY = (((uint64)sourceRect.height() << 32) + destRect.height() - 1) / destRect.height();
	const uint32 alphaMask = source.format.ARGBToColor(255, 0, 0, 0);
	const uint32 keyMask = source.format.aBits() && transparent && transparent != (uint32)-1 ?
		source.format.ARGBToColor(0, 255, 255, 255) : (uint32)(Pixel)-1;
	const uint32 key = transparent & keyMask;
	const bool destinationKey = keyed && dest.hasTransparentColor();
	uint64 sy = (visible.top - destRect.top) * stepY;
	for (int y = visible.top; y < visible.bottom; ++y, sy += stepY) {
		const int sourceY = sourceRect.top + (sy >> 32);
		const Pixel *row = (const Pixel *)source.getBasePtr(sourceRect.left, sourceY);
		Pixel *out = (Pixel *)dest.getBasePtr(visible.left, y);
		uint64 sx = (visible.left - destRect.left) * stepX;
		for (int x = visible.left; x < visible.right; ++x, sx += stepX, ++out) {
			const Pixel pixel = row[sx >> 32];
			if (keyed && (pixel & keyMask) == key)
				continue;
			if ((pixel & alphaMask) == alphaMask) {
				*out = pixel;
			} else if ((pixel & alphaMask) || destinationKey) {
				// Keep each existing copy operation's alpha rounding and destination-key handling.
				const int sourceX = sourceRect.left + (sx >> 32);
				const Common::Rect sample(sourceX, sourceY, sourceX + 1, sourceY + 1);
				if (keyed)
					dest.transBlitFrom(source, sample, Common::Point(x, y), transparent);
				else
					dest.blitFrom(source, sample, Common::Point(x, y));
			}
		}
	}
	dest.addDirtyRect(visible);
}

static void accurateCopy(Graphics::ManagedSurface &dest, const Graphics::Surface &source,
		const Common::Rect &sourceRect, const Common::Rect &destRect, bool keyed, uint32 transparent) {
	assert(source.format == dest.format);
	assert(source.format.bytesPerPixel == 2 || source.format.bytesPerPixel == 4);
	if (sourceRect.isEmpty() || destRect.isEmpty())
		return;
	if (sourceRect.width() == destRect.width() && sourceRect.height() == destRect.height()) {
		if (keyed)
			dest.transBlitFrom(source, sourceRect, destRect, transparent);
		else
			dest.blitFrom(source, sourceRect, destRect);
	} else if (source.format.bytesPerPixel == 2) {
		copyScaled<uint16>(dest, source, sourceRect, destRect, keyed, transparent);
	} else {
		copyScaled<uint32>(dest, source, sourceRect, destRect, keyed, transparent);
	}
}

void AccurateManagedSurface::accurateBlitFrom(const Graphics::Surface &source,
		const Common::Rect &sourceRect, const Common::Rect &destRect) {
	accurateCopy(*this, source, sourceRect, destRect, false, 0);
}

void AccurateManagedSurface::accurateTransBlitFrom(const Graphics::Surface &source,
		const Common::Rect &sourceRect, const Common::Rect &destRect, uint32 transparent) {
	accurateCopy(*this, source, sourceRect, destRect, true, transparent);
}

} // End of namespace Glk
