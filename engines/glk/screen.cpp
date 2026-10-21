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

#include "glk/screen.h"
#include "glk/accurate_managed_surface.h"
#include "glk/conf.h"
#include "common/compression/unzip.h"
#include "graphics/fonts/ttf.h"

namespace Glk {


#define FONTS_FILENAME "fonts.dat"

static void deleteFonts(Common::Array<const Graphics::Font *> &fonts) {
	for (uint i = 0; i < fonts.size(); ++i)
		delete fonts[i];
	fonts.clear();
}

Screen::Screen() : Graphics::Screen(), _canvasSize(w, h), _pictureScaler(format) {
	_presentation.logicalSize = _presentation.renderSize = _canvasSize;
}

PreparedFonts::~PreparedFonts() {
	clear();
}

void PreparedFonts::clear() {
	deleteFonts(_fonts);
	deleteFonts(_renderFonts);
}

PreparedPresentation::~PreparedPresentation() {
	_surface.free();
	deleteFonts(_fonts);
}

bool Screen::loadRenderFonts(Common::Archive *archive, const Conf &settings, const PresentationGeometry &geometry,
		Common::Array<const Graphics::Font *> &fonts) {
	Conf scaled(settings);
	scaled._monoInfo._size *= geometry.fontScale;
	scaled._propInfo._size *= geometry.fontScale;
	return loadReplacementFonts(archive, scaled, fonts, geometry.pixelAspect);
}

bool Screen::preparePresentation(const Conf &settings, const PresentationGeometry &geometry,
		PreparedPresentation &prepared) {
	if (!supportsLiveMetrics())
		return false;
	prepared._geometry = geometry;
	if ((geometry.fontScale != _presentation.fontScale || geometry.pixelAspect != _presentation.pixelAspect) &&
			(geometry.fontScale != 1.0 || geometry.pixelAspect != 1.0)) {
		Common::Archive *archive = openFontArchive();
		const bool loaded = archive && loadRenderFonts(archive, settings, geometry, prepared._fonts);
		delete archive;
		if (!loaded)
			return false;
	}
	if (w != geometry.renderSize.x || h != geometry.renderSize.y)
		prepared._surface.create(geometry.renderSize.x, geometry.renderSize.y, format);
	return true;
}

void Screen::installPresentation(PreparedPresentation &prepared) {
	if (prepared._surface.getPixels()) {
		// Both surfaces own their pixels; installation does not allocate.
		SWAP(*surfacePtr(), prepared._surface);
		clearDirtyRects();
	}
	if (prepared._geometry.fontScale != _presentation.fontScale || prepared._geometry.pixelAspect != _presentation.pixelAspect)
		_renderFonts.swap(prepared._fonts);
	_presentation = prepared._geometry;
	makeAllDirty();
}

bool Screen::loadReplacementFonts(Common::Archive *archive, const Conf &settings,
		Common::Array<const Graphics::Font *> &fonts, double pixelAspect) {
	for (int i = MONOR; i <= PROPZ; ++i) {
		const double size = i < PROPR ? settings._monoInfo._size : settings._propInfo._size;
		const Graphics::Font *font = loadFontFromArchive((FACES)i, archive, size, pixelAspect);
		if (!font)
			return false;
		fonts.push_back(font);
	}
	return true;
}

bool Screen::prepareFonts(Conf &settings, bool replace, PreparedFonts &prepared) {
	prepared.clear();
	if (!supportsLiveMetrics())
		return false;
	if (replace) {
		Common::Archive *archive = openFontArchive();
		const bool loaded = archive && loadReplacementFonts(archive, settings, prepared._fonts) &&
			((_presentation.fontScale == 1.0 && _presentation.pixelAspect == 1.0) ||
			loadRenderFonts(archive, settings, _presentation, prepared._renderFonts));
		delete archive;
		if (!loaded) {
			prepared.clear();
			return false;
		}
	}
	const Common::Array<const Graphics::Font *> &fonts = replace ? prepared._fonts : _fonts;
	if (fonts.size() < FONTS_TOTAL)
		return false;
	// The caller supplies configured minima, not previously measured metrics.
	measureFont(settings._monoInfo, *fonts[MONOR], *fonts[MONOR], 0);
	measureFont(settings._propInfo, *fonts[PROPZ], *fonts[MONOR], settings._propInfo._lineSeparation);
	return settings._monoInfo._cellW > 0 && settings._monoInfo._cellH > 0 &&
		settings._propInfo._cellH > 0;
}

void Screen::installFonts(PreparedFonts &prepared) {
	if (!prepared._fonts.empty()) {
		_fonts.swap(prepared._fonts);
		_renderFonts.swap(prepared._renderFonts);
	}
}

size_t Screen::preparedTextWidth(const PreparedFonts &prepared, int fontIdx,
		const Common::U32String &text) const {
	const Common::Array<const Graphics::Font *> &fonts = prepared._fonts.empty() ? _fonts : prepared._fonts;
	assert(fontIdx >= 0 && (uint)fontIdx < fonts.size());
	return fonts[fontIdx]->getStringWidth(text) * GLI_SUBPIX;
}

Screen::~Screen() {
	deleteFonts(_fonts);
	deleteFonts(_renderFonts);
}

void Screen::initialize() {
	refreshPictureScaler();
	loadFonts();

	measureFont(g_conf->_monoInfo, *_fonts[MONOR], *_fonts[MONOR], 0);
	measureFont(g_conf->_propInfo, *_fonts[PROPZ], *_fonts[MONOR], g_conf->_propInfo._lineSeparation);
}

void Screen::fill(uint color) {
	clear(color);
}

void Screen::fillRect(const Rect &box, uint color) {
	if (color != zcolor_Transparent)
		Graphics::Screen::fillRect(_presentation.toRender(box), color);
}

void Screen::hLine(int x, int y, int x2, uint color) {
	fillRect(Rect(x, y, x2 + 1, y + 1), color);
}

void Screen::vLine(int x, int y, int y2, uint color) {
	fillRect(Rect(x, y, x + 1, y2 + 1), color);
}

bool Screen::integerPictures() const {
	return supportsLiveMetrics() && g_conf->_reflowIntegerPictures &&
		(g_conf->_presentationMode == kPresentationReflow || g_conf->_presentationMode == kPresentationAdaptiveReflow);
}

bool Screen::refreshPictureScaler() {
	return _pictureScaler.select(integerPictures() && ConfMan.hasKey("scaler") ? ConfMan.get("scaler") : Common::String("normal"));
}

PicturePlacement Screen::picturePlacement(const PictureSurface &image, const Point &position, bool centered,
		bool rightAligned, const uint32 *transparent) const {
	PicturePlacement result;
	result.x = _presentation.renderX(position.x);
	result.y = _presentation.renderY(position.y);
	result.width = _presentation.renderX(position.x + image.w) - result.x;
	result.height = _presentation.renderY(position.y + image.h) - result.y;
	result.factor = 0;
	result.filtered = false;
	if (integerPictures() && image.w && image.h) {
		const int available = MIN(result.width / image.w, result.height / image.h);
		const bool filtered = available >= 2 && _pictureScaler.hasHQ() && image.opaque(transparent);
		const uint factor = _pictureScaler.factor(available, filtered);
		const uint width = image.w * factor, height = image.h * factor;
		if (_pictureScaler.fits(image, factor, filtered)) {
			result.x += centered ? (result.width - width) / 2 : rightAligned ? result.width - width : 0;
			result.y += centered ? (result.height - height) / 2 : 0;
			result.width = width;
			result.height = height;
			result.factor = factor;
			result.filtered = filtered;
		}
	}
	return result;
}

void Screen::drawImage(PictureSurface &image, const Point &position, uint background) {
	const PicturePlacement placement = picturePlacement(image, position, true);
	AccurateManagedSurface target(*this, getBounds());
	const Rect destination(position.x, position.y, position.x + image.w, position.y + image.h);
	if (placement.factor) {
		fillRect(destination, background);
		const Graphics::Surface &prepared = image.prepare(_pictureScaler, placement.factor, placement.filtered);
		target.blitFrom(prepared, Common::Point(placement.x, placement.y));
	} else {
		target.accurateBlitFrom(image, image.getBounds(), _presentation.toRender(destination));
	}
}

PicturePlacement Screen::drawInlinePicture(PictureSurface &image, const Point &position, const Common::Rect &clip,
		uint transparent, bool rightAligned) {
	const PicturePlacement placement = picturePlacement(image, position, false, rightAligned, &transparent);
	if (!placement.factor) {
		drawPicture(image, position, clip, transparent);
		return placement;
	}
	Common::Rect bounds = _presentation.toRender(clip);
	bounds.clip(getBounds());
	const Common::Rect visible = placement.clipped(bounds);
	if (visible.isEmpty())
		return placement;
	const Graphics::Surface &prepared = image.prepare(_pictureScaler, placement.factor, placement.filtered);
	const Common::Rect source(visible.left - placement.x, visible.top - placement.y,
		visible.right - placement.x, visible.bottom - placement.y);
	AccurateManagedSurface target(*this, getBounds());
	if (placement.filtered)
		target.blitFrom(prepared, source, Common::Point(visible.left, visible.top));
	else
		target.transBlitFrom(prepared, source, Common::Point(visible.left, visible.top), transparent);
	return placement;
}

void Screen::drawPicture(const Graphics::ManagedSurface &image, const Point &position,
		const Common::Rect &clip, uint transparent) {
	const int left = MAX(0, clip.left - position.x);
	const int top = MAX(0, clip.top - position.y);
	const int right = MIN<int>(image.w, clip.right - position.x);
	const int bottom = MIN<int>(image.h, clip.bottom - position.y);
	if (left >= right || top >= bottom)
		return;
	Common::Rect bounds = _presentation.toRender(clip);
	bounds.clip(getBounds());
	if (bounds.isEmpty())
		return;
	AccurateManagedSurface destination(*this, bounds);
	// Clip in logical coordinates before scaling beyond Rect's signed range.
	Common::Rect area = _presentation.toRender(Common::Rect(position.x + left, position.y + top,
		position.x + right, position.y + bottom));
	area.translate(-bounds.left, -bounds.top);
	destination.accurateTransBlitFrom(image, Common::Rect(left, top, right, bottom), area, transparent);
}

void Screen::measureFont(FontInfo &info, const Graphics::Font &font,
		const Graphics::Font &fixedFont, int lineSeparation) {
	const Common::Rect baseline = font.getBoundingBox('o');
	const Common::Rect descender = font.getBoundingBox('y');
	info._leading = MAX(info._leading, descender.bottom + lineSeparation);
	info._baseLine = MAX(info._baseLine, (int)baseline.bottom);
	info._cellW = fixedFont.getMaxCharWidth();
	info._cellH = info._leading;
}

Common::Archive *Screen::openFontArchive(const Common::Archive *resources) {
	Common::Archive *archive = resources ?
		Common::makeZipArchive(resources->createReadStreamForMember(FONTS_FILENAME)) :
		Common::makeZipArchive(FONTS_FILENAME);
	if (!archive)
		return nullptr;
	Common::File version;
	char buffer[5] = { 0, 0, 0, 0, 0 };
	if (!version.open("version.txt", *archive) || version.read(buffer, 4) != 4 ||
			buffer[1] != '.' || buffer[0] < '1' || (buffer[0] == '1' && atoi(buffer + 2) < 2)) {
		delete archive;
		return nullptr;
	}
	return archive;
}

void Screen::loadFonts() {
	Common::Archive *archive = openFontArchive();
	if (!archive)
		error("Could not load compatible GLK fonts from %s (version 1.2 or newer required)", FONTS_FILENAME);
	loadFonts(archive);
	delete archive;
}

void Screen::loadFonts(Common::Archive *archive) {
	const double monoSize = g_conf->_monoInfo._size;
	const double propSize = g_conf->_propInfo._size;

	for (uint index = 0; index < _fonts.size(); ++index)
		delete _fonts[index];
	_fonts.clear();
	_fonts.resize(FONTS_TOTAL);
	_fonts[MONOR] = loadFont(MONOR, archive, monoSize);
	_fonts[MONOB] = loadFont(MONOB, archive, monoSize);
	_fonts[MONOI] = loadFont(MONOI, archive, monoSize);
	_fonts[MONOZ] = loadFont(MONOZ, archive, monoSize);

	_fonts[PROPR] = loadFont(PROPR, archive, propSize);
	_fonts[PROPB] = loadFont(PROPB, archive, propSize);
	_fonts[PROPI] = loadFont(PROPI, archive, propSize);
	_fonts[PROPZ] = loadFont(PROPZ, archive, propSize);
}

const Graphics::Font *Screen::loadFont(FACES face, Common::Archive *archive, double size) {
	const Graphics::Font *font = loadFontFromArchive(face, archive, size);
	if (!font)
		error("Could not load GLK font %s", getFontName(face).c_str());
	return font;
}

const Graphics::Font *Screen::loadFontFromArchive(FACES face,
		Common::Archive *archive, double size, double pixelAspect) {
	if (!archive || face < MONOR || face > PROPZ || !(size >= 1.0 && size <= 32767.0))
		return nullptr;
	Common::File *f = new Common::File();
	const char *const FILENAMES[8] = {
		"GoMono-Regular.ttf", "GoMono-Bold.ttf", "GoMono-Italic.ttf", "GoMono-Bold-Italic.ttf",
		"NotoSerif-Regular.ttf", "NotoSerif-Bold.ttf", "NotoSerif-Italic.ttf", "NotoSerif-Bold-Italic.ttf"
	};

	if (!f->open(FILENAMES[face], *archive)) {
		delete f;
		return nullptr;
	}

	const Graphics::Font *font = Graphics::loadTTFFont(f, DisposeAfterUse::YES, (int)size,
		Graphics::kTTFSizeModeCharacter, 72, (uint)(72 * pixelAspect + 0.5));
	if (!font)
		delete f;
	return font;
}

Common::String Screen::getFontName(FACES font) {
	if (font == MONOR) return "monor";
	if (font == MONOB) return "monob";
	if (font == MONOI) return "monoi";
	if (font == MONOZ) return "monoz";
	if (font == PROPR) return "propr";
	if (font == PROPB) return "propb";
	if (font == PROPI) return "propi";
	if (font == PROPZ) return "propz";
	return "monor";
}

int Screen::drawString(int x, int y, int fontIdx, uint color, const Common::String &text, int spw) {
	if (_presentation.fontScale != 1.0 || _presentation.pixelAspect != 1.0)
		return drawRenderString(x, y, fontIdx, color, Common::U32String(text, Common::kLatin1), 0, true);
	int baseLine = (fontIdx >= PROPR) ? g_conf->_propInfo._baseLine : g_conf->_monoInfo._baseLine;
	Point pt(x / GLI_SUBPIX, y - baseLine);
	const Graphics::Font *font = _fonts[fontIdx];
	font->drawString(this, text, pt.x, pt.y, w - pt.x, color);

	pt.x += font->getStringWidth(text);
	return MIN((int)pt.x, (int)w) * GLI_SUBPIX;
}

int Screen::drawStringUni(int x, int y, int fontIdx, uint color, const Common::U32String &text, int spw) {
	if (_presentation.fontScale != 1.0 || _presentation.pixelAspect != 1.0)
		return drawRenderString(x, y, fontIdx, color, text, spw, false);
	int baseLine = (fontIdx >= PROPR) ? g_conf->_propInfo._baseLine : g_conf->_monoInfo._baseLine;
	Point pt(x / GLI_SUBPIX, y - baseLine);
	const Graphics::Font *font = _fonts[fontIdx];
	
	int xSub = x;
	for (auto c : text) {
		int xPix = xSub / GLI_SUBPIX;
		font->drawString(this, Common::U32String(1, c), xPix, pt.y, w - xPix, color);
		int charWidthPx = font->getStringWidth(Common::U32String(1, c));
		xSub += charWidthPx * GLI_SUBPIX;
		if (spw > 0 && c == ' ')
			xSub += spw;
	}
	int xFinalPix = MIN(xSub / GLI_SUBPIX, (int)w);
	return xFinalPix * GLI_SUBPIX;
}

int Screen::drawRenderString(int x, int y, int fontIdx, uint color,
		const Common::U32String &text, int spw, bool kern) {
	const Graphics::Font &logical = *_fonts[fontIdx];
	const Graphics::Font &render = *_renderFonts[fontIdx];
	const int baseline = fontIdx >= PROPR ? g_conf->_propInfo._baseLine : g_conf->_monoInfo._baseLine;
	y = _presentation.renderY(y - baseline);
	if (kern)
		x = x / GLI_SUBPIX * GLI_SUBPIX;
	uint32 previous = 0;
	for (uint i = 0; i < text.size(); ++i) {
		const uint32 c = text[i];
		if (kern)
			x += logical.getKerningOffset(previous, c) * GLI_SUBPIX;
		const int px = (int)((int64)x * w / (_presentation.logicalSize.x * GLI_SUBPIX));
		render.drawChar(this, c, px, y, color);
		x += logical.getCharWidth(c) * GLI_SUBPIX;
		if (spw > 0 && c == ' ')
			x += spw;
		previous = c;
	}
	return MIN(x / GLI_SUBPIX, (int)_presentation.logicalSize.x) * GLI_SUBPIX;
}

size_t Screen::stringWidth(int fontIdx, const Common::String &text, int spw) {
	const Graphics::Font *font = _fonts[fontIdx];
	return font->getStringWidth(text) * GLI_SUBPIX;
}

size_t Screen::stringWidthUni(int fontIdx, const Common::U32String &text, int spw) {
	const Graphics::Font *font = _fonts[fontIdx];
	size_t width = font->getStringWidth(text)*GLI_SUBPIX;

	if (spw > 0) {
        int spaces = 0;
        for (auto c : text)
            if (c == ' ')
                spaces++;

        width += (size_t)spaces * (size_t)spw;
    }
    return width;
}

} // End of namespace Glk
