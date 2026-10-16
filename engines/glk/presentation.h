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

#ifndef GLK_PRESENTATION_H
#define GLK_PRESENTATION_H

#include "common/rect.h"

namespace Glk {

enum PresentationMode {
	kPresentationLegacy,
	kPresentationOriginal,
	kPresentationReflow
};

/** Game coordinates stay logical; only the screen draws in render pixels. */
struct PresentationGeometry {
	Common::Point logicalSize;
	Common::Point renderSize;
	double fontScale;

	PresentationGeometry() : fontScale(1.0) {}
	bool calculate(const Common::Point &canvas, const Common::Point &output,
		PresentationMode mode, int textScale, double density, int bytesPerPixel);
	Common::Rect toRender(const Common::Rect &rect) const;
	Common::Point toLogical(const Common::Point &point) const;
	int renderX(int x) const;
	int renderY(int y) const;
};

} // End of namespace Glk

#endif
