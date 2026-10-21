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

#include "glk/picture_presentation.h"

namespace Glk {

Common::Rect PicturePlacement::clipped(const Common::Rect &bounds) const {
	const int left = MAX(x, (int)bounds.left), top = MAX(y, (int)bounds.top);
	const int right = MIN(x + width, (int)bounds.right), bottom = MIN(y + height, (int)bounds.bottom);
	return left < right && top < bottom ? Common::Rect(left, top, right, bottom) : Common::Rect();
}

bool PicturePlacement::contains(const Common::Point &point) const {
	return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
}

Common::Point PicturePlacement::sourcePoint(const Common::Point &point, int sourceWidth, int sourceHeight) const {
	return Common::Point((int64)(point.x - x) * sourceWidth / width,
		(int64)(point.y - y) * sourceHeight / height);
}

PictureScaler::PictureScaler(const Graphics::PixelFormat &format) : _normal(format) {
}

uint PictureScaler::factor(int available) const {
#ifdef USE_SCALERS
	return CLIP(available, 0, 5);
#else
	return CLIP(available, 0, 1);
#endif
}

void PictureScaler::scale(const Graphics::Surface &source, Graphics::Surface &dest, uint scale) {
	_normal.setFactor(scale);
	_normal.scale((const byte *)source.getPixels(), source.pitch, (byte *)dest.getPixels(),
		dest.pitch, source.w, source.h, 0, 0);
}

PictureSurface::PictureSurface() : _factor(0), _valid(false) {
}

PictureSurface::PictureSurface(int width, int height, const Graphics::PixelFormat &pixelFormat) :
		AccurateManagedSurface(width, height, pixelFormat), _factor(0), _valid(false) {
}

void PictureSurface::invalidate() {
	_valid = false;
}

void PictureSurface::addDirtyRect(const Common::Rect &rect) {
	invalidate();
	AccurateManagedSurface::addDirtyRect(rect);
}

const Graphics::Surface &PictureSurface::prepare(PictureScaler &scaler, uint scale) {
	if (scale <= 1)
		return rawSurface();
	if (!_valid || _factor != scale) {
		if (_prepared.w != w * (int)scale || _prepared.h != h * (int)scale || _prepared.format != format)
			_prepared.create(w * scale, h * scale, format);
		scaler.scale(rawSurface(), *_prepared.surfacePtr(), scale);
		_factor = scale;
		_valid = true;
	}
	return _prepared.rawSurface();
}

} // End of namespace Glk
