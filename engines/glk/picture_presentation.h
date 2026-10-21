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

#ifndef GLK_PICTURE_PRESENTATION_H
#define GLK_PICTURE_PRESENTATION_H

#include "glk/accurate_managed_surface.h"
#include "graphics/scaler/normal.h"

namespace Glk {

// Render coordinates can exceed Rect's range while a picture scrolls off screen.
struct PicturePlacement {
	int x, y, width, height;
	uint factor;
	bool filtered;

	Common::Rect clipped(const Common::Rect &bounds) const;
	bool contains(const Common::Point &point) const;
	Common::Point sourcePoint(const Common::Point &point, int sourceWidth, int sourceHeight) const;
};

class PictureScaler {
	NormalScaler _normal;
	Graphics::PixelFormat _hqFormat;
	Scaler *_hq;
	PictureScaler(const PictureScaler &);
	PictureScaler &operator=(const PictureScaler &);
public:
	explicit PictureScaler(const Graphics::PixelFormat &format);
	~PictureScaler();
	bool select(const Common::String &name);
	bool hasHQ() const;
	uint factor(int available, bool filtered = false) const;
	bool fits(const Graphics::Surface &source, uint factor, bool filtered) const;
	void scale(const Graphics::Surface &source, Graphics::ManagedSurface &dest, uint factor, bool filtered);
};

// Pixel changes invalidate preparation, including writes through existing surface callers.
class PictureSurface : public AccurateManagedSurface {
	Graphics::ManagedSurface _prepared;
	uint _factor;
	bool _valid, _filtered;
	mutable bool _opacityKnown, _opaque, _keyed;
	mutable uint32 _key;
	PictureSurface(const PictureSurface &);
	PictureSurface &operator=(const PictureSurface &);
public:
	PictureSurface();
	PictureSurface(int width, int height, const Graphics::PixelFormat &pixelFormat);
	void addDirtyRect(const Common::Rect &rect) override;
	void invalidate();
	bool opaque(const uint32 *transparent) const;
	const Graphics::Surface &prepare(PictureScaler &scaler, uint factor, bool filtered = false);
};

} // End of namespace Glk

#endif
