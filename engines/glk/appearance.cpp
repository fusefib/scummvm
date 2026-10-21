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

#include "glk/glk.h"
#include "glk/conf.h"
#include "glk/screen.h"
#include "glk/windows.h"

namespace Glk {

template<class T>
static bool updatePreference(T &installed, const T &before, const T &after) {
	if (before == after)
		return false;
	installed = after;
	return true;
}

static bool prepareMetrics(Conf &candidate, const Conf &before, const Conf &after) {
	bool changed = false;
	changed |= updatePreference(candidate._monoInfo._size, before._monoInfo._size, after._monoInfo._size);
	changed |= updatePreference(candidate._propInfo._size, before._propInfo._size, after._propInfo._size);
	changed |= updatePreference(candidate._leadingMin, before._leadingMin, after._leadingMin);
	changed |= updatePreference(candidate._baseLineMin, before._baseLineMin, after._baseLineMin);
	changed |= updatePreference(candidate._propInfo._lineSeparation, before._propInfo._lineSeparation, after._propInfo._lineSeparation);
	changed |= updatePreference(candidate._wMarginSaveX, before._wMarginSaveX, after._wMarginSaveX);
	changed |= updatePreference(candidate._wMarginSaveY, before._wMarginSaveY, after._wMarginSaveY);
	if (before._wMarginSaveX != after._wMarginSaveX || (before._lockCols && !after._lockCols))
		candidate._wMarginX = candidate._wMarginSaveX;
	if (before._wMarginSaveY != after._wMarginSaveY || (before._lockRows && !after._lockRows))
		candidate._wMarginY = candidate._wMarginSaveY;
	changed |= updatePreference(candidate._wPaddingX, before._wPaddingX, after._wPaddingX);
	changed |= updatePreference(candidate._wPaddingY, before._wPaddingY, after._wPaddingY);
	changed |= updatePreference(candidate._wBorderX, before._wBorderX, after._wBorderX);
	changed |= updatePreference(candidate._wBorderY, before._wBorderY, after._wBorderY);
	changed |= updatePreference(candidate._tMarginX, before._tMarginX, after._tMarginX);
	changed |= updatePreference(candidate._tMarginY, before._tMarginY, after._tMarginY);
	changed |= updatePreference(candidate._lockCols, before._lockCols, after._lockCols);
	changed |= updatePreference(candidate._lockRows, before._lockRows, after._lockRows);
	changed |= updatePreference(candidate._cols, before._cols, after._cols);
	changed |= updatePreference(candidate._rows, before._rows, after._rows);
	changed |= updatePreference(candidate._scrollWidth, before._scrollWidth, after._scrollWidth);
	return changed;
}

static void installMetrics(Conf &current, const Conf &candidate) {
	// FontInfo objects are referenced by existing windows and stay in place.
	current._leadingMin = candidate._leadingMin;
	current._baseLineMin = candidate._baseLineMin;
	FontInfo *active[] = { &current._monoInfo, &current._propInfo };
	const FontInfo *prepared[] = { &candidate._monoInfo, &candidate._propInfo };
	for (int i = 0; i < 2; ++i) {
		active[i]->_size = prepared[i]->_size;
		active[i]->_cellW = prepared[i]->_cellW;
		active[i]->_cellH = prepared[i]->_cellH;
		active[i]->_leading = prepared[i]->_leading;
		active[i]->_baseLine = prepared[i]->_baseLine;
	}
	current._propInfo._lineSeparation = candidate._propInfo._lineSeparation;
	current._wMarginX = candidate._wMarginX;
	current._wMarginY = candidate._wMarginY;
	current._wMarginSaveX = candidate._wMarginSaveX;
	current._wMarginSaveY = candidate._wMarginSaveY;
	current._wPaddingX = candidate._wPaddingX;
	current._wPaddingY = candidate._wPaddingY;
	current._wBorderX = candidate._wBorderX;
	current._wBorderY = candidate._wBorderY;
	current._tMarginX = candidate._tMarginX;
	current._tMarginY = candidate._tMarginY;
	current._lockCols = candidate._lockCols;
	current._lockRows = candidate._lockRows;
	current._cols = candidate._cols;
	current._rows = candidate._rows;
	current._scrollWidth = candidate._scrollWidth;
}

static bool updateColor(uint &active, uint &saved, uint before, uint after, bool requested) {
	if (before != after) {
		saved = after;
		if (!requested)
			active = after;
		return true;
	}
	return false;
}

static bool updatePresentationPreferences(Conf &current, const Conf &before, const Conf &after, bool fonts) {
	bool changed = false;
	const uint requests = current._appearanceRequests;
	changed |= updateColor(current._propInfo._caretColor, current._propInfo._caretSave,
		before._propInfo._caretColor, after._propInfo._caretColor, requests & kAppearanceCaretColor);
	changed |= updateColor(current._monoInfo._caretColor, current._monoInfo._caretSave,
		before._monoInfo._caretColor, after._monoInfo._caretColor, false);
	changed |= updateColor(current._propInfo._linkColor, current._propInfo._linkSave,
		before._propInfo._linkColor, after._propInfo._linkColor, requests & kAppearanceLinkColor);
	changed |= updateColor(current._monoInfo._linkColor, current._monoInfo._linkSave,
		before._monoInfo._linkColor, after._monoInfo._linkColor, false);
	changed |= updateColor(current._propInfo._moreColor, current._propInfo._moreSave,
		before._propInfo._moreColor, after._propInfo._moreColor, requests & kAppearanceMoreColor);
	changed |= updateColor(current._windowColor, current._windowSave,
		before._windowColor, after._windowColor, requests & kAppearanceWindowColor);
	changed |= updateColor(current._borderColor, current._borderSave,
		before._borderColor, after._borderColor, requests & kAppearanceBorderColor);
	changed |= updatePreference(current._propInfo._caretShape, before._propInfo._caretShape, after._propInfo._caretShape);
	changed |= updatePreference(current._monoInfo._caretShape, before._monoInfo._caretShape, after._monoInfo._caretShape);
	changed |= updatePreference(current._propInfo._linkStyle, before._propInfo._linkStyle, after._propInfo._linkStyle);
	changed |= updatePreference(current._monoInfo._linkStyle, before._monoInfo._linkStyle, after._monoInfo._linkStyle);
	changed |= updatePreference(current._propInfo._justify, before._propInfo._justify, after._propInfo._justify);
	if (!(requests & kAppearancePagingPrompt))
		changed |= updatePreference(current._propInfo._morePrompt, before._propInfo._morePrompt, after._propInfo._morePrompt);
	changed |= updatePreference(current._propInfo._moreAlign, before._propInfo._moreAlign, after._propInfo._moreAlign);
	changed |= updatePreference(current._propInfo._moreFont, before._propInfo._moreFont, after._propInfo._moreFont);
	changed |= updatePreference(current._scrollBg, before._scrollBg, after._scrollBg);
	changed |= updatePreference(current._scrollFg, before._scrollFg, after._scrollFg);
	if (!(requests & kAppearanceTypography)) {
		changed |= updatePreference(current._propInfo._quotes, before._propInfo._quotes, after._propInfo._quotes);
		changed |= updatePreference(current._propInfo._dashes, before._propInfo._dashes, after._propInfo._dashes);
		changed |= updatePreference(current._propInfo._spaces, before._propInfo._spaces, after._propInfo._spaces);
	}
	changed |= updatePreference(current._propInfo._caps, before._propInfo._caps, after._propInfo._caps);
	changed |= updatePreference(current._safeClicks, before._safeClicks, after._safeClicks);
	for (int i = 0; i < style_NUMSTYLES; ++i) {
		changed |= before._tStyles[i].fg != after._tStyles[i].fg ||
			before._tStyles[i].bg != after._tStyles[i].bg ||
			before._gStyles[i].fg != after._gStyles[i].fg ||
			before._gStyles[i].bg != after._gStyles[i].bg ||
			(fonts && (before._tStyles[i].font != after._tStyles[i].font ||
			before._gStyles[i].font != after._gStyles[i].font));
		current._tStyles[i].updatePreferences(before._tStyles[i], after._tStyles[i], current._tStyleRequests[i], fonts);
		current._gStyles[i].updatePreferences(before._gStyles[i], after._gStyles[i], current._gStyleRequests[i], fonts);
		current._tStylesDefault[i].updatePreferences(before._tStyles[i], after._tStyles[i], current._tDefaultRequests[i], fonts);
		current._gStylesDefault[i].updatePreferences(before._gStyles[i], after._gStyles[i], current._gDefaultRequests[i], fonts);
	}
	return changed;
}

static bool windowFontsChange(Windows &windows, const Conf &before, const Conf &after) {
	for (Windows::iterator i = windows.begin(); i != windows.end(); ++i) {
		Window *window = *i;
		if (window->_type != wintype_TextBuffer && window->_type != wintype_TextGrid)
			continue;
		WindowStyle styles[style_NUMSTYLES];
		static_cast<TextWindow *>(window)->resolveStyles(before, after, styles, true);
		for (int style = 0; style < style_NUMSTYLES; ++style) {
			if (styles[style].font != window->getStyles()[style].font)
				return true;
		}
	}
	return false;
}

AppearanceResult GlkEngine::applyAppearance(const Common::String &target,
		const Conf &before, const Conf &after) {
	AppearanceResult result;
	if (target != _targetName || !_conf || !_screen || !_windows || !_events)
		return result;
	result.startup = before._styleHint != after._styleHint || before._graphics != after._graphics;

	Conf candidate(*_conf);
	const bool metrics = prepareMetrics(candidate, before, after) || windowFontsChange(*_windows, before, after);
	bool installed = false;
	if (metrics) {
		if (!_screen->supportsLiveMetrics()) {
			result.metrics = kAppearanceInterpreter;
		} else {
			PreparedFonts fonts;
			Windows::PreparedLayout layout;
			const bool replace = (int)candidate._monoInfo._size != (int)_conf->_monoInfo._size ||
				(int)candidate._propInfo._size != (int)_conf->_propInfo._size;
			candidate._monoInfo._leading = candidate._propInfo._leading = candidate._leadingMin;
			candidate._monoInfo._baseLine = 0;
			candidate._propInfo._baseLine = candidate._baseLineMin;
			if (!_screen->prepareFonts(candidate, replace, fonts)) {
				result.metrics = kAppearanceFontResources;
			} else if (!_windows->prepareLayout(candidate, before, after, fonts, layout)) {
				result.metrics = kAppearanceLayout;
			} else {
				_screen->installFonts(fonts);
				installMetrics(*_conf, candidate);
				_windows->installLayout(layout);
				_events->refreshTextCursor();
				installed = true;
			}
		}
	}
	const bool fonts = result.metrics == kAppearanceApplied;
	const bool presentation = updatePresentationPreferences(*_conf, before, after, fonts);
	if (presentation || installed) {
		_windows->updateAppearance(before, after, fonts);
		appearanceChanged(before, after, installed);
		Windows::_forceRedraw = true;
		_events->redraw();
	}
	if (installed) {
		_events->store(evtype_Arrange, nullptr);
		_events->store(evtype_Redraw, nullptr);
	}
	const bool modeChanged = updatePreference(_conf->_presentationMode, before._presentationMode, after._presentationMode);
	const bool scaleChanged = updatePreference(_conf->_reflowScale, before._reflowScale, after._reflowScale);
	const bool picturesChanged = updatePreference(_conf->_reflowIntegerPictures, before._reflowIntegerPictures, after._reflowIntegerPictures);
	if (modeChanged || picturesChanged)
		_screen->refreshPictureScaler();
	if (modeChanged || scaleChanged)
		updatePresentation();
	if (picturesChanged) {
		Windows::_forceRedraw = true;
		_events->redraw();
	}
	return result;
}

} // End of namespace Glk
