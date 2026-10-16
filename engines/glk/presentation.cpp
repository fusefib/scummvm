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

#include "glk/presentation.h"
#include "common/util.h"

namespace Glk {

bool PresentationGeometry::calculate(const Common::Point &canvas, const Common::Point &output,
		PresentationMode mode, int textScale, double density, int bytesPerPixel) {
	if (canvas.x <= 0 || canvas.y <= 0 || output.x <= 0 || output.y <= 0 ||
			bytesPerPixel < 2 || bytesPerPixel > 4)
		return false;
	int logicalWidth = canvas.x, logicalHeight = canvas.y;
	int renderWidth = canvas.x, renderHeight = canvas.y;
	double scale = 1.0;
	if (mode == kPresentationOriginal) {
		scale = MIN((double)output.x / canvas.x, (double)output.y / canvas.y);
		renderWidth = MAX(1, (int)(canvas.x * scale));
		renderHeight = MAX(1, (int)(canvas.y * scale));
	} else if (mode == kPresentationReflow) {
		if (textScale < 50 || textScale > 300 || !(density > 0))
			return false;
		scale = textScale * density / 100.0;
		if (!(output.x / scale <= 32767 && output.y / scale <= 32767))
			return false;
		logicalWidth = (int)(output.x / scale);
		logicalHeight = (int)(output.y / scale);
		renderWidth = output.x;
		renderHeight = output.y;
	} else if (mode != kPresentationLegacy) {
		return false;
	}
	// Rect coordinates are signed 16-bit. Bound pixels and the logical link mask.
	if (logicalWidth <= 0 || logicalHeight <= 0 || logicalWidth > 32767 || logicalHeight > 32767 ||
			(uint64)renderWidth * renderHeight * bytesPerPixel > 64 * 1024 * 1024 ||
			(uint64)(logicalWidth + 1) * (logicalHeight + 1) * sizeof(uint) > 64 * 1024 * 1024)
		return false;
	logicalSize = Common::Point(logicalWidth, logicalHeight);
	renderSize = Common::Point(renderWidth, renderHeight);
	fontScale = scale;
	return true;
}

int PresentationGeometry::renderX(int x) const {
	return (int)((int64)x * renderSize.x / logicalSize.x);
}

int PresentationGeometry::renderY(int y) const {
	return (int)((int64)y * renderSize.y / logicalSize.y);
}

Common::Rect PresentationGeometry::toRender(const Common::Rect &rect) const {
	return Common::Rect(renderX(rect.left), renderY(rect.top), renderX(rect.right), renderY(rect.bottom));
}

Common::Point PresentationGeometry::toLogical(const Common::Point &point) const {
	const int x = CLIP<int>(point.x, 0, renderSize.x - 1);
	const int y = CLIP<int>(point.y, 0, renderSize.y - 1);
	// Invert the half-open intervals used by toRender(), including rounded edges.
	return Common::Point(((x + 1) * logicalSize.x - 1) / renderSize.x,
		((y + 1) * logicalSize.y - 1) / renderSize.y);
}

} // End of namespace Glk
