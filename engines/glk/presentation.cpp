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
#include "glk/glk.h"
#include "glk/conf.h"
#include "glk/screen.h"
#include "glk/selection.h"
#include "glk/windows.h"
#include "common/config-manager.h"
#include "common/ptr.h"
#include "common/util.h"

namespace Glk {

bool PresentationGeometry::calculate(const Common::Point &canvas, const Common::Point &output,
		PresentationMode mode, int textScale, double density, int bytesPerPixel, bool aspectCorrection) {
	if (canvas.x <= 0 || canvas.y <= 0 || output.x <= 0 || output.y <= 0 ||
			bytesPerPixel < 2 || bytesPerPixel > 4)
		return false;
	int logicalWidth = canvas.x, logicalHeight = canvas.y;
	int renderWidth = canvas.x, renderHeight = canvas.y;
	double scale = 1.0;
	double aspect = 1.0;
	if (mode == kPresentationOriginal) {
		int displayHeight = canvas.y;
		if (aspectCorrection && ((canvas.x == 320 && canvas.y == 200) ||
				(canvas.x == 640 && (canvas.y == 400 || canvas.y == 350)) ||
				(canvas.x == 720 && (canvas.y == 348 || canvas.y == 350))))
			displayHeight = canvas.x * 3 / 4;
		aspect = (double)displayHeight / canvas.y;
		scale = MIN((double)output.x / canvas.x, (double)output.y / displayHeight);
		renderWidth = MAX(1, (int)(canvas.x * scale));
		renderHeight = MAX(1, (int)(displayHeight * scale));
	} else if (mode == kPresentationReflow || mode == kPresentationAdaptiveReflow) {
		if (textScale < 50 || textScale > 300 || !(density > 0))
			return false;
		scale = textScale * density / 100.0;
		if (mode == kPresentationAdaptiveReflow) {
			// Keep at least the startup canvas available for text and split windows.
			scale = MIN(scale, MIN((double)output.x / canvas.x, (double)output.y / canvas.y));
		}
		if (!(output.x / scale <= 32767 && output.y / scale <= 32767))
			return false;
		logicalWidth = (int)(output.x / scale);
		logicalHeight = (int)(output.y / scale);
		if (mode == kPresentationAdaptiveReflow) {
			logicalWidth = MAX(logicalWidth, (int)canvas.x);
			logicalHeight = MAX(logicalHeight, (int)canvas.y);
		}
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
	pixelAspect = aspect;
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

bool GlkEngine::updatePresentation(bool backendChanged) {
	if (!_screen || !_conf || !_windows || !_events)
		return false;
	_presentationDeferred = false;
	const PresentationMode mode = (PresentationMode)_conf->_presentationMode;
	if (mode == kPresentationLegacy && !_nativePresentation)
		return true;
	if (g_system->isOverlayVisible())
		return false;
	if (!_screen->supportsLiveMetrics())
		return false;
	float density;
	const Common::Point output = g_system->getGameOutputSize(&density);
	const bool aspect = _nativePresentation ? _savedAspectCorrection :
		g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection);
	PresentationGeometry geometry;
	if (!geometry.calculate(_screen->canvasSize(), output, mode, _conf->_reflowScale,
			density, _screen->format.bytesPerPixel, aspect))
		return false;
	const PresentationGeometry &current = _screen->presentation();
	const bool resize = geometry.logicalSize != current.logicalSize;
	const bool redraw = resize || geometry.renderSize != current.renderSize ||
		geometry.fontScale != current.fontScale || geometry.pixelAspect != current.pixelAspect;
	if (_nativePresentation && mode != kPresentationLegacy && !redraw && !backendChanged &&
			_presentationScreenChangeID == g_system->getScreenChangeID())
		return true;
	PreparedPresentation presentation;
	if (redraw && !_screen->preparePresentation(*_conf, geometry, presentation))
		return false;
	Windows::PreparedLayout layout;
	Common::ScopedPtr<WindowMask> mask;
	int marginX = _conf->_wMarginX, marginY = _conf->_wMarginY;
	if (resize) {
		Conf candidate(*_conf);
		candidate._imageW = geometry.logicalSize.x;
		candidate._imageH = geometry.logicalSize.y;
		candidate._wMarginX = candidate._wMarginSaveX;
		candidate._wMarginY = candidate._wMarginSaveY;
		PreparedFonts fonts;
		if (!_windows->prepareLayout(candidate, *_conf, *_conf, fonts, layout)) {
			_presentationDeferred = true;
			return false;
		}
		mask.reset(new WindowMask(candidate._imageW, candidate._imageH));
		layout.copyGraphicsLinks(*_selection, *mask);
		marginX = candidate._wMarginX;
		marginY = candidate._wMarginY;
	}
	const int scaler = g_system->getScaler();
	const int scaleFactor = g_system->getScaleFactor();
	const int stretch = g_system->getStretchMode();
	if (mode == kPresentationLegacy) {
		g_system->beginGFXTransaction();
		g_system->initSize(geometry.renderSize.x, geometry.renderSize.y, &_screen->format);
		g_system->setScaler(_savedScaler, _savedScaleFactor);
		g_system->setStretchMode(_savedStretchMode);
		g_system->setFeatureState(OSystem::kFeatureAspectRatioCorrection, _savedAspectCorrection);
		g_system->endGFXTransaction();
		// A failed shader can still leave the requested surface installed.
		if (g_system->getWidth() != geometry.renderSize.x || g_system->getHeight() != geometry.renderSize.y ||
				g_system->getScreenFormat() != _screen->format) {
			g_system->setGameRenderSize(current.renderSize.x, current.renderSize.y);
			_screen->makeAllDirty();
			return false;
		}
	} else if (!g_system->setGameRenderSize(geometry.renderSize.x, geometry.renderSize.y)) {
		return false;
	}
	if (!_nativePresentation) {
		_savedScaler = scaler;
		_savedScaleFactor = scaleFactor;
		_savedStretchMode = stretch;
		_savedAspectCorrection = aspect;
	}
	_nativePresentation = mode != kPresentationLegacy;
	_presentationDeferred = false;
	_presentationScreenChangeID = g_system->getScreenChangeID();
	if (redraw)
		_screen->installPresentation(presentation);
	if (resize) {
		_conf->_imageW = geometry.logicalSize.x;
		_conf->_imageH = geometry.logicalSize.y;
		_conf->_wMarginX = marginX;
		_conf->_wMarginY = marginY;
		_selection->swapStorage(*mask);
		_copySelect = false;
		_windows->installLayout(layout);
		if (_windows->getRoot()) {
			appearanceChanged(*_conf, *_conf, true);
			_events->store(evtype_Arrange, nullptr);
			_events->store(evtype_Redraw, nullptr);
		}
	}
	if (redraw)
		_events->refreshCursors();
	// Rebuild hit areas before the next input, including after backend-only resets.
	Windows::_forceRedraw = true;
	_windows->redraw();
	_screen->makeAllDirty();
	return true;
}

void GlkEngine::retryPresentation() {
	if (_presentationDeferred)
		updatePresentation();
}

void GlkEngine::applyGameSettings() {
	// The game menu has hidden its overlay and restored backend preferences.
	if (_nativePresentation)
		_savedAspectCorrection = ConfMan.getBool("aspect_ratio");
	updatePresentation(true);
}

} // End of namespace Glk
