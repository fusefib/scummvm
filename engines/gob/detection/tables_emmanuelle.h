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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#ifndef GOB_DETECTION_TABLES_EMMANUELLE_H
#define GOB_DETECTION_TABLES_EMMANUELLE_H

{
	{"emmanuelle",
	 nullptr,
	 AD_ENTRY2s(
		 "LOADAERO.TOT", "060b4ead740a296fcdd57ac1a90025f2", 46336,
		 "LOADER.COM", nullptr, AD_NO_SIZE),
	 EN_ANY,
	 kPlatformDOS,
	 ADGF_UNSTABLE,
	 GUIO5(GUIO_NOSUBTITLES, GUIO_NOSPEECH,
		   GAMEOPTION_EMMANUELLE_ENHANCED_FIGHTS,
		   GUIO_RENDERCGA, GUIO_RENDEREGA)},
	kFeaturesEGA | kFeatures16Colors,
	nullptr,
	"INIT.TOT",
	0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY2s(
		  "LOADAERO.TOT", "cc74ff1aa461469e9995e243d4b0692f", 45824,
		  "LOADER.COM", nullptr, AD_NO_SIZE),
	  ES_ESP,
	  kPlatformDOS,
	  ADGF_UNSTABLE,
	  GUIO5(GUIO_NOSUBTITLES, GUIO_NOSPEECH,
			GAMEOPTION_EMMANUELLE_ENHANCED_FIGHTS,
			GUIO_RENDERCGA, GUIO_RENDEREGA)},
	 kFeaturesEGA | kFeatures16Colors,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY2s(
		  "LOADAERO.TOT", "060b4ead740a296fcdd57ac1a90025f2", 46336,
		  "S4.SPL", "32467e3bf25c2f81c8b022845e025886", 4912),
	  EN_ANY,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY2s(
		  "LOADAERO.TOT", "cc74ff1aa461469e9995e243d4b0692f", 45824,
		  "S4.SPL", "32467e3bf25c2f81c8b022845e025886", 4912),
	  ES_ESP,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY2s(
		  "LOADAERO.TOT", "e1ea97cb1d2a0bec567d62c9154db080", 65408,
		  "S4.SPL", "32467e3bf25c2f81c8b022845e025886", 4912),
	  FR_FRA,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors |
		 kFeaturesEmmanuelleInlineLocalGraphics |
		 kFeaturesEmmanuelleAlternateInputGeometry,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY1s("LOADAERO.TOT", "060b4ead740a296fcdd57ac1a90025f2", 46336),
	  EN_ANY,
	  kPlatformDOS,
	  ADGF_UNSTABLE,
	  GUIO5(GUIO_NOSUBTITLES, GUIO_NOSPEECH,
			GAMEOPTION_EMMANUELLE_ENHANCED_FIGHTS,
			GUIO_RENDERCGA, GUIO_RENDEREGA)},
	 kFeaturesEGA | kFeatures16Colors,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY1s("LOADAERO.TOT", "060b4ead740a296fcdd57ac1a90025f2", 46336),
	  EN_ANY,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors |
		 kFeaturesEmmanuelleOptionalAtariAudio,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY1s("LOADAERO.TOT", "cc74ff1aa461469e9995e243d4b0692f", 45824),
	  ES_ESP,
	  kPlatformDOS,
	  ADGF_UNSTABLE,
	  GUIO5(GUIO_NOSUBTITLES, GUIO_NOSPEECH,
			GAMEOPTION_EMMANUELLE_ENHANCED_FIGHTS,
			GUIO_RENDERCGA, GUIO_RENDEREGA)},
	 kFeaturesEGA | kFeatures16Colors,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY1s("LOADAERO.TOT", "cc74ff1aa461469e9995e243d4b0692f", 45824),
	  ES_ESP,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors |
		 kFeaturesEmmanuelleOptionalAtariAudio,
	 nullptr,
	 "INIT.TOT",
	 0},

	{{"emmanuelle",
	  nullptr,
	  AD_ENTRY1s("LOADAERO.TOT", "e1ea97cb1d2a0bec567d62c9154db080", 65408),
	  FR_FRA,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors |
		 kFeaturesEmmanuelleInlineLocalGraphics |
		 kFeaturesEmmanuelleAlternateInputGeometry |
		 kFeaturesEmmanuelleOptionalAtariAudio,
	 nullptr,
	 "INIT.TOT",
	 0},

#endif // GOB_DETECTION_TABLES_EMMANUELLE_H
