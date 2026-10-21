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

PictureScaler::PictureScaler(const Graphics::PixelFormat &format) : _normal(format), _hq(nullptr) {
	const Graphics::PixelFormat rgb565(2, 5, 6, 5, 0, 11, 5, 0, 0);
	const Graphics::PixelFormat rgb555(2, 5, 5, 5, 0, 10, 5, 0, 0);
	_hqFormat = format == rgb565 || format == rgb555 ? format :
		Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24);
}

PictureScaler::~PictureScaler() {
	delete _hq;
}

bool PictureScaler::select(const Common::String &name) {
	const bool before = hasHQ();
	if (name != "hq") {
		delete _hq;
		_hq = nullptr;
	} else if (!_hq) {
#if defined(USE_SCALERS) && defined(USE_HQ_SCALERS)
		Plugin *plugin = ScalerMan.findScalerPlugin("hq");
		if (plugin)
			_hq = plugin->get<ScalerPluginObject>().createInstance(_hqFormat);
#endif
	}
	return before != hasHQ();
}

bool PictureScaler::hasHQ() const {
	return _hq != nullptr;
}

uint PictureScaler::factor(int available, bool filtered) const {
#ifdef USE_SCALERS
	return CLIP(available, 0, filtered && _hq ? 3 : 5);
#else
	return CLIP(available, 0, 1);
#endif
}

bool PictureScaler::fits(const Graphics::Surface &source, uint factor, bool filtered) const {
	const uint width = source.w * factor, height = source.h * factor;
	uint64 bytes = (uint64)width * height * source.format.bytesPerPixel;
	if (filtered) {
		if (source.w > 32765 || source.h > 32765)
			return false;
		bytes += (uint64)(source.w + 2) * (source.h + 2) * _hqFormat.bytesPerPixel;
		if (source.format != _hqFormat)
			bytes += (uint64)width * height * _hqFormat.bytesPerPixel;
	}
	return factor && width <= 32767 && height <= 32767 && bytes <= 64 * 1024 * 1024;
}

void PictureScaler::scale(const Graphics::Surface &source, Graphics::ManagedSurface &dest, uint factor, bool filtered) {
	if (!filtered || !_hq) {
		_normal.setFactor(factor);
		_normal.scale((const byte *)source.getPixels(), source.pitch, (byte *)dest.getPixels(),
			dest.pitch, source.w, source.h, 0, 0);
		return;
	}
	Graphics::ManagedSurface padded(source.w + 2, source.h + 2, _hqFormat);
	padded.blitFrom(source, Common::Point(1, 1));
	for (int y = 1; y <= source.h; ++y) {
		padded.setPixel(0, y, padded.getPixel(1, y));
		padded.setPixel(source.w + 1, y, padded.getPixel(source.w, y));
	}
	padded.copyRectToSurface(padded.getBasePtr(0, 1), padded.pitch, 0, 0, padded.w, 1);
	padded.copyRectToSurface(padded.getBasePtr(0, source.h), padded.pitch, 0, source.h + 1, padded.w, 1);
	_hq->setFactor(factor);
	if (dest.format == _hqFormat) {
		_hq->scale((const byte *)padded.getBasePtr(1, 1), padded.pitch, (byte *)dest.getPixels(),
			dest.pitch, source.w, source.h, 0, 0);
	} else {
		Graphics::ManagedSurface converted(dest.w, dest.h, _hqFormat);
		_hq->scale((const byte *)padded.getBasePtr(1, 1), padded.pitch, (byte *)converted.getPixels(),
			converted.pitch, source.w, source.h, 0, 0);
		dest.blitFrom(converted);
	}
}

PictureSurface::PictureSurface() : _factor(0), _valid(false), _filtered(false),
		_opacityKnown(false), _opaque(false), _keyed(false), _key(0) {
}

PictureSurface::PictureSurface(int width, int height, const Graphics::PixelFormat &pixelFormat) :
		AccurateManagedSurface(width, height, pixelFormat), _factor(0), _valid(false), _filtered(false),
		_opacityKnown(false), _opaque(false), _keyed(false), _key(0) {
}

void PictureSurface::invalidate() {
	_valid = false;
	_opacityKnown = false;
}

void PictureSurface::addDirtyRect(const Common::Rect &rect) {
	invalidate();
	AccurateManagedSurface::addDirtyRect(rect);
}

bool PictureSurface::opaque(const uint32 *transparent) const {
	if (_opacityKnown && _keyed == (transparent != nullptr) && (!transparent || _key == *transparent))
		return _opaque;
	_keyed = transparent != nullptr;
	_key = transparent ? *transparent : 0;
	_opacityKnown = true;
	_opaque = false;
	const uint32 alpha = format.ARGBToColor(255, 0, 0, 0);
	const uint32 mask = format.aBits() && _key && _key != (uint32)-1 ?
		format.ARGBToColor(0, 255, 255, 255) : format.bytesPerPixel == 2 ? 0xffff : 0xffffffff;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint32 pixel = getPixel(x, y);
			if ((pixel & alpha) != alpha || (_keyed && (pixel & mask) == (_key & mask)))
				return false;
		}
	}
	_opaque = true;
	return true;
}

const Graphics::Surface &PictureSurface::prepare(PictureScaler &scaler, uint factor, bool filtered) {
	if (factor <= 1)
		return rawSurface();
	if (!_valid || _factor != factor || _filtered != filtered) {
		if (_prepared.w != w * (int)factor || _prepared.h != h * (int)factor || _prepared.format != format)
			_prepared.create(w * factor, h * factor, format);
		scaler.scale(rawSurface(), _prepared, factor, filtered);
		_factor = factor;
		_filtered = filtered;
		_valid = true;
	}
	return _prepared.rawSurface();
}

} // End of namespace Glk
