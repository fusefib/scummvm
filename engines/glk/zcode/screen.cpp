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

#include "glk/zcode/screen.h"
#include "glk/zcode/bitmap_font.h"
#include "glk/zcode/zcode.h"
#include "glk/conf.h"
#include "common/file.h"
#include "graphics/fonts/ttf.h"
#include "image/xbm.h"

#include "glk/zcode/infocom6x8.xbm"
#include "glk/zcode/infocom_graphics.xbm"

namespace Glk {
namespace ZCode {

FrotzScreen::FrotzScreen() : Glk::Screen(), _version6(false) {
	g_conf->_tStyles[style_User1].font = CUSTOM;
	g_conf->_gStyles[style_User1].font = CUSTOM;
	g_conf->_tStyles[style_User2].font = CUSTOM2;
	g_conf->_tStyleRequests[style_User1] = kStyleRequestFont;
	g_conf->_gStyleRequests[style_User1] = kStyleRequestFont;
	g_conf->_tStyleRequests[style_User2] = kStyleRequestFont;
}

void FrotzScreen::loadFonts(Common::Archive *archive) {
	// Get the zmachine version. At this point the header isn't loaded, so we have to do it manually
	g_vm->_gameFile.seek(0);
	byte version = g_vm->_gameFile.readByte();
	_version6 = version == 6;

	if (version == 6) {
		loadVersion6Fonts(archive);
	} else {
		// Load the basic fonts
		Screen::loadFonts(archive);
	}

	// Add character graphics and runic fonts
	if (!loadExtraFonts(archive, g_conf->_propInfo._size, _fonts))
		error("Could not load Z-code character graphics or runic fonts");
}

bool FrotzScreen::loadReplacementFonts(Common::Archive *archive, const Conf &settings,
		Common::Array<const Graphics::Font *> &fonts, double pixelAspect) {
	return !_version6 && Screen::loadReplacementFonts(archive, settings, fonts, pixelAspect) &&
		loadExtraFonts(archive, settings._propInfo._size, fonts, pixelAspect);
}

void FrotzScreen::loadVersion6Fonts(Common::Archive *archive) {
	// Set the basic font properties
	MonoFontInfo &mi = g_conf->_monoInfo;
	PropFontInfo &pi = g_conf->_propInfo;
	mi._size = pi._size = 7;
	mi._aspect = pi._aspect = 1.0;
	pi._quotes = 0;
	pi._dashes = 0;
	pi._spaces = 0;
	pi._morePrompt = "[MORE]";
	pi._lineSeparation = 0;
	g_conf->_appearanceRequests |= kAppearancePagingPrompt | kAppearanceTypography;

	// Capture the defaults before bitmap windows use transparent backgrounds.
	g_vm->_defaultForeground = g_conf->_tStyles[style_Normal].fg;
	g_vm->_defaultBackground = g_conf->_tStyles[style_Normal].bg;
	g_conf->_tMarginX = 3;
	g_conf->_tMarginY = 3;

	for (uint idx = 0; idx < style_NUMSTYLES; ++idx) {
		g_conf->_tStyles[idx].bg = g_conf->_tStylesDefault[idx].bg = zcolor_Transparent;
		g_conf->_gStyles[idx].bg = g_conf->_gStylesDefault[idx].bg = zcolor_Transparent;
		g_conf->_tStyleRequests[idx] |= kStyleRequestBackground;
		g_conf->_gStyleRequests[idx] |= kStyleRequestBackground;
		g_conf->_tDefaultRequests[idx] |= kStyleRequestBackground;
		g_conf->_gDefaultRequests[idx] |= kStyleRequestBackground;
	}

	_fonts.resize(8);

	// Load up the 8x8 Infocom font
	Image::XBMDecoder decoder;
	decoder.loadBits(infocom6x8_bits, infocom6x8_width, infocom6x8_height);

	Common::Point fontSize(6, 8);

	// Add normal fonts
	_fonts[MONOR] = new FixedWidthBitmapFont(*decoder.getSurface(), fontSize, 6, 8);
	_fonts[MONOB] = new FixedWidthBitmapFont(*decoder.getSurface(), fontSize, 6, 8);
	_fonts[PROPR] = new VariableWidthBitmapFont(*decoder.getSurface(), fontSize, 6, 8);
	_fonts[PROPB] = new VariableWidthBitmapFont(*decoder.getSurface(), fontSize, 6, 8);

	// Create a new version of the font with every character unlined for the emphasized fonts
	const Graphics::Surface &norm = *decoder.getSurface();
	Graphics::ManagedSurface emph(norm.w, norm.h);
	emph.blitFrom(norm);

	for (int y = 8 - 2; y < emph.h; y += 8) {
		byte *lineP = (byte *)emph.getBasePtr(0, y);
		Common::fill(lineP, lineP + emph.w, 1);
	}

	// Add them to the font list
	_fonts[MONOI] = new FixedWidthBitmapFont(emph, fontSize, 6, 8);
	_fonts[MONOZ] = new FixedWidthBitmapFont(emph, fontSize, 6, 8);
	_fonts[PROPI] = new VariableWidthBitmapFont(emph, fontSize, 6, 8);
	_fonts[PROPZ] = new VariableWidthBitmapFont(emph, fontSize, 6, 8);
}

bool FrotzScreen::loadExtraFonts(Common::Archive *archive, double size,
		Common::Array<const Graphics::Font *> &fonts, double pixelAspect) {
	Image::XBMDecoder decoder;
	decoder.loadBits(infocom_graphics_bits, infocom_graphics_width, infocom_graphics_height);

	Common::Point fontSize(fonts[0]->getMaxCharWidth(), fonts[0]->getFontHeight());
	fonts.push_back(new FixedWidthBitmapFont(*decoder.getSurface(), fontSize));

	// Add Runic font. It provides cleaner versions of the runic characters in the
	// character graphics font
	Common::File *f = new Common::File();
	if (!f->open("NotoSansRunic-Regular.ttf", *archive)) {
		delete f;
		return false;
	}
	const Graphics::Font *font = Graphics::loadTTFFont(f, DisposeAfterUse::YES,
		(int)size, Graphics::kTTFSizeModeCharacter, 72, (uint)(72 * pixelAspect + 0.5));
	if (!font) {
		delete f;
		return false;
	}
	fonts.push_back(font);
	return true;
}

} // End of namespace ZCode
} // End of namespace Glk
