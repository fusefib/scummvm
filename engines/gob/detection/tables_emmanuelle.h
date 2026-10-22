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
	 AD_ENTRY3s(
		 "LOADER.COM", "9014bd4b990022428ff1f9bb2794f559", 57868,
		 "INIT.TOT", "ad6cc0d8ba2964848250d56ca2e5a6ab", 26368,
		 "INIT.I01", "b8fac1a5213832a0d3420b9ad42e7253", 36736),
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
	  AD_ENTRY3s(
		  "LOADER.PRG", "b642dcefc3544287508caa41350d6463", 69800,
		  "INIT.TOT", "ad6cc0d8ba2964848250d56ca2e5a6ab", 26368,
		  "INIT.I01", "b8fac1a5213832a0d3420b9ad42e7253", 36736),
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
		  "INIT.TOT", "ad6cc0d8ba2964848250d56ca2e5a6ab", 26368,
		  "INIT.I01", "b8fac1a5213832a0d3420b9ad42e7253", 36736),
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
		  "INIT.TOT", "ad6cc0d8ba2964848250d56ca2e5a6ab", 26368,
		  "INIT.I01", "b8fac1a5213832a0d3420b9ad42e7253", 36736),
	  EN_ANY,
	  kPlatformAtariST,
	  ADGF_UNSTABLE,
	  GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	 kFeaturesEGA | kFeatures16Colors |
		 kFeaturesEmmanuelleOptionalAtariAudio,
	 nullptr,
	 "INIT.TOT",
	 0},

#endif // GOB_DETECTION_TABLES_EMMANUELLE_H
