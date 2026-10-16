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

#ifndef GLK_ZCODE_FONTS
#define GLK_ZCODE_FONTS

#include "glk/screen.h"

namespace Glk {
namespace ZCode {

/**
 * Derived screen class that adds in the Infocom character graphics font
 */
class FrotzScreen : public Glk::Screen {
private:
	bool _version6;
	/**
	 * Handles loading fonts for V6 games
	 */
	void loadVersion6Fonts(Common::Archive *archive);

	/**
	 * Handles loading the character graphics and runic fonts
	 */
	bool loadExtraFonts(Common::Archive *archive, double size,
		Common::Array<const Graphics::Font *> &fonts, double pixelAspect = 1.0);
protected:
	/**
	 * Load the fonts
	 */
	void loadFonts(Common::Archive *archive) override;
	bool loadReplacementFonts(Common::Archive *archive, const Conf &settings,
		Common::Array<const Graphics::Font *> &fonts, double pixelAspect = 1.0) override;
public:
	/**
	 * Constructor
	 */
	FrotzScreen();
	bool supportsLiveMetrics() const override { return !_version6; }
};

} // End of namespace ZCode
} // End of namespace Glk

#endif
