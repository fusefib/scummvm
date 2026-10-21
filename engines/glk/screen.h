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

#ifndef GLK_DRAW_H
#define GLK_DRAW_H

#include "common/archive.h"
#include "common/array.h"
#include "graphics/screen.h"
#include "graphics/font.h"
#include "glk/fonts.h"
#include "glk/presentation.h"
#include "glk/picture_presentation.h"
#include "glk/utils.h"

namespace Glk {

class Conf;

/** Temporary ownership of a replacement font set; installation swaps owners. */
class PreparedFonts {
	friend class Screen;
	Common::Array<const Graphics::Font *> _fonts;
	Common::Array<const Graphics::Font *> _renderFonts;
	PreparedFonts(const PreparedFonts &);
	PreparedFonts &operator=(const PreparedFonts &);
public:
	PreparedFonts() {}
	~PreparedFonts();
	void clear();
};

class PreparedPresentation {
	friend class Screen;
	Graphics::Surface _surface;
	Common::Array<const Graphics::Font *> _fonts;
	PresentationGeometry _geometry;
	PreparedPresentation(const PreparedPresentation &);
	PreparedPresentation &operator=(const PreparedPresentation &);
public:
	PreparedPresentation() {}
	~PreparedPresentation();
};

#define FONTS_TOTAL 8

enum CaretShape {
	SMALL_DOT = 0, FAT_DOT = 1, THIN_LINE = 2, FAT_LINE = 3, BLOCK = 4
};

/**
 * Screen surface class
 */
class Screen : public Graphics::Screen {
private:
	Common::Point _canvasSize;
	PresentationGeometry _presentation;
	PictureScaler _pictureScaler;
	Common::Array<const Graphics::Font *> _renderFonts;
	bool loadRenderFonts(Common::Archive *archive, const Conf &settings, const PresentationGeometry &geometry,
		Common::Array<const Graphics::Font *> &fonts);
	int drawRenderString(int x, int y, int fontIdx, uint color,
		const Common::U32String &text, int spw, bool kern);
	/**
	 * Open the fonts archive and load all the fonts
	 */
	void loadFonts();

	/**
	 * Load a single font
	 */
	const Graphics::Font *loadFont(FACES face, Common::Archive *archive,
		double size);
protected:
	Common::Array<const Graphics::Font *> _fonts;
protected:
	/**
	 * Load the fonts
	 */
	virtual void loadFonts(Common::Archive *archive);
	virtual bool loadReplacementFonts(Common::Archive *archive, const Conf &settings,
		Common::Array<const Graphics::Font *> &fonts, double pixelAspect = 1.0);
public:
	/**
	 * Return the name for a given font Id
	 */
	static Common::String getFontName(FACES font);

	/**
	 * Load one bundled GLK font for an independently owned user.
	 */
	static Common::Archive *openFontArchive(const Common::Archive *resources = nullptr);
	static void measureFont(FontInfo &info, const Graphics::Font &font,
		const Graphics::Font &fixedFont, int lineSeparation);
	static const Graphics::Font *loadFontFromArchive(FACES face,
		Common::Archive *archive, double size, double pixelAspect = 1.0);
public:
	/**
	 * Constructor
	 */
	Screen();

	/**
	 * Destructor
	 */
	~Screen() override;

	/**
	 * Initialize the screen
	 */
	void initialize();

	virtual bool supportsLiveMetrics() const { return true; }
	bool prepareFonts(Conf &settings, bool replace, PreparedFonts &prepared);
	void installFonts(PreparedFonts &prepared);
	const Common::Point &canvasSize() const { return _canvasSize; }
	const PresentationGeometry &presentation() const { return _presentation; }
	bool preparePresentation(const Conf &settings, const PresentationGeometry &geometry,
		PreparedPresentation &prepared);
	void installPresentation(PreparedPresentation &prepared);
	size_t preparedTextWidth(const PreparedFonts &prepared, int fontIdx,
		const Common::U32String &text) const;

	/**
	 * Fills the screen with a given color
	 */
	void fill(uint color);

	/**
	 * Fill a given area of the screen with a given color
	 */
	void fillRect(const Rect &box, uint color);
	void hLine(int x, int y, int x2, uint color);
	void vLine(int x, int y, int y2, uint color);
	bool integerPictures() const;
	bool refreshPictureScaler();
	PicturePlacement picturePlacement(const PictureSurface &image, const Point &position, bool centered,
		bool rightAligned = false, const uint32 *transparent = nullptr) const;
	void drawImage(PictureSurface &image, const Point &position, uint background);
	PicturePlacement drawInlinePicture(PictureSurface &image, const Point &position, const Common::Rect &clip, uint transparent, bool rightAligned);
	void drawPicture(const Graphics::ManagedSurface &image, const Point &position,
		const Common::Rect &clip, uint transparent);

	/**
	 * Draws a string using the specified font at the given co-ordinates
	 * @param x         Logical horizontal position in GLI_SUBPIX units
	 * @param y         Logical baseline position in pixels
	 * @param fontIdx   Which font to use
	 * @param color     Text color
	 * @param text      The text to draw
	 * @param spw       ??
	 */
	int drawString(int x, int y, int fontIdx, uint color, const Common::String &text, int spw = 0);

	/**
	 * Draws a unicode string using the specified font at the given co-ordinates
	 * @param x         Logical horizontal position in GLI_SUBPIX units
	 * @param y         Logical baseline position in pixels
	 * @param fontIdx   Which font to use
	 * @param color     Text color
	 * @param text      The text to draw
	 * @param spw       ??
	 */
	int drawStringUni(int x, int y, int fontIdx, uint color, const Common::U32String &text, int spw = 0);

	/**
	 * Get the width in pixels of a string
	 * @param fontIdx   Which font to use
	 * @param text      Text to get the width of
	 * @param spw       Delta X
	 * @returns         Width of string multiplied by GLI_SUBPIX
	 */
	size_t stringWidth(int fontIdx, const Common::String &text, int spw = 0);

	/**
	 * Get the width in pixels of a unicode string
	 * @param fontIdx   Which font to use
	 * @param text      Text to get the width of
	 * @param spw       Delta X
	 * @returns         Width of string multiplied by GLI_SUBPIX
	 */
	size_t stringWidthUni(int fontIdx, const Common::U32String &text, int spw = 0);
};

} // End of namespace Glk

#endif
