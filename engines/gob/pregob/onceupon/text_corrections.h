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

#ifndef GOB_PREGOB_ONCEUPON_TEXT_CORRECTIONS_H
#define GOB_PREGOB_ONCEUPON_TEXT_CORRECTIONS_H

#include "common/util.h"
#include "gob/detection/detection.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

struct BrokenString {
	const char *wrong;
	const char *correct;
};

struct BrokenStringLanguage {
	const BrokenString *strings;
	uint count;
};

static const BrokenString kBrokenStringsGerman[] = {
	{ "Zeichungen von Kaki,"         , "Zeichnungen von Kaki,"        },
	{ "die es in seine Wachtr\204ume", "die es in seine Tagtr\204ume" },
	{ "   Spielerfahrung"            , "    Spielerfahren"            },
	{ "  Fortgeschrittene"           , "  Fortgeschritten"            },
	{ "die Vespe"                    , "die Wespe"                    },
	{ "das Rhinoceros"               , "das Rhinozeros"               },
	{ "die Heusschrecke"             , "die Heuschrecke"              },
	{ "Das, von Drachen gebrachte"   , "Das vom Drachen gebrachte"    },
	{ "Am Waldesrand es sieht"       , "Am Waldesrand sieht es"       },
	{ " das Kind den Palast."        , "das Kind den Palast."         },
	{ "Am Waldessaum sieht"          , "Am Waldesrand sieht"          },
	{ "tipp auf ESC!"                , "dr\201cke ESC!"               },
	{ "Wohin fliegt der Drachen?"    , "Wohin fliegt der Drache?"     }
};

static const BrokenString kBrokenStringsEnglish[] = {
	{ "the child reaches the far", "The child reaches the far" },
	{ "If not press any key.", "If not, press any key." }
};

static const BrokenString kBrokenStringsItalian[] = {
	{ "Confermi la tua scelta,", "Confermi la tua scelta?" },
	{ "preme ESC!", "premi ESC!" }
};

// TXT corrections match complete lines within the active language.
static const BrokenStringLanguage kBrokenStrings[OnceUpon::kLanguageCount] = {
	{                    0,                               0 }, // French
	{ kBrokenStringsGerman, ARRAYSIZE(kBrokenStringsGerman) }, // German
	{ kBrokenStringsEnglish, ARRAYSIZE(kBrokenStringsEnglish) }, // English
	{                    0,                               0 }, // Spanish
	{ kBrokenStringsItalian, ARRAYSIZE(kBrokenStringsItalian) }, // Italian
};

struct BrokenGCTString {
	const char *resource;
	GameType game;
	uint item;
	uint16 line;
	const char *wrong;
	const char *correct;
};

// Match source chunks before expanding house/name references and wrapping.
static const BrokenGCTString kBrokenGCTStrings[] = {
	// Correct literal resource errors before reference expansion and wrapping.
	{ "mefait.gca", kGameTypeNone, 9, 0, "\"We're leaving you alone in the castle. Don't raise the drawbridge for anyone!\"", "\"We're leaving you alone in the castle. Don't lower the drawbridge for anyone!\"" },
	{ "mefait.gca", kGameTypeBabaYaga, 15, 5, "ti find Baba Yaga!", "to find Baba Yaga!" },
	{ "mefait.gct", kGameTypeNone, 11, 1, "\"Soit sage, nous reviendrons bient\223t!\"", "\"Sois sage, nous reviendrons bient\223t!\"" },
	{ "mefait.gcg", kGameTypeAbracadabra, 9, 0, "\"Du wirst allein im Schol\341 bleiben. Lasse niemanden die Zugbr\201cke runter!\"", "\"Du wirst allein im Schlo\341 bleiben. Lasse niemanden die Zugbr\201cke runter!\"" },
	{ "mefait.gcg", kGameTypeAbracadabra, 9, 1, "\"Du darfst under Schlo\341 nicht verlassen!\"", "\"Du darfst unser Schlo\341 nicht verlassen!\"" },
	{ "mefait.gcg", kGameTypeAbracadabra, 12, 0, "\"Der Hofstaat wird uns begleiten; Du bleibst also in Deihen Gem\204chern!\"", "\"Der Hofstaat wird uns begleiten; Du bleibst also in Deinen Gem\204chern!\"" },
	{ "mefait.gcg", kGameTypeBabaYaga, 9, 0, "\"Du wirst allein im Sclo\341 bleiben. Lasse niemanden die Zugbr\201cke auf!\"", "\"Du wirst allein im Schlo\341 bleiben. Lasse niemanden die Zugbr\201cke auf!\"" },
	{ "choix.gci", kGameTypeAbracadabra, 4, 0, "gli signori", "i signori" },
	// English parents movie and the Abracadabra wand hint.
	{ "mefait.gca", kGameTypeNone, 0, 1, "in his parents'", "in his parents' " },
	{ "mefait.gca", kGameTypeNone, 19, 1, "wand hidden in his parents'dwelling...", "wand hidden in his parents' dwelling..." },
	// With an 11-character name, the Abracadabra line has 47 meaningful
	// cells. The trailing space forces a blank row and another caption wait.
	{ "mefait.gcg", kGameTypeNone, 18, 0, " denkt an den Schmerz seiner Eltern, ", " denkt an den Schmerz seiner Eltern," },
	// Spanish palace warning at parents-movie frame 110 (item 12, alternative 0).
	// Exact source matches preserve each edition's wording.
	// Abracadabra on DOS, Atari ST and Amiga.
	{ "mefait.gce", kGameTypeNone, 12, 0,
		"\"\255 Toda la corte nos acompa\244a,as\241 que,\255no salgas de tus aposentos !\"",
		"\"\255 Toda la corte nos acompa\244a, as\241 que, \255no salgas de tus aposentos !\"" },
	// Baba Yaga on Atari ST and Amiga.
	{ "mefait.gce", kGameTypeNone, 12, 0,
		"\"\255 Toda la corte nos acompa\244a,as\241 que,\255no salgas de tu aposentos !\"",
		"\"\255 Toda la corte nos acompa\244a, as\241 que, \255no salgas de tus aposentos !\"" },
	// Baba Yaga DOS uses unaccented "asi".
	{ "mefait.gce", kGameTypeNone, 12, 0,
		"\"\255 Toda la corte nos acompa\244a,asi que,\255no salgas de tu aposentos !\"",
		"\"\255 Toda la corte nos acompa\244a, as\241 que, \255no salgas de tus aposentos !\"" },
	// FORETM splices the child name onto these English and Spanish chunks.
	{ "foretm.gca", kGameTypeNone, 6, 0, "After all these adventures,", "After all these adventures, " },
	{ "foretm.gce", kGameTypeNone, 2, 1, "\255 Defi\202ndete , ", "\255 Defi\202ndete, " },
	// Rebalance the Spanish Chapter 3 defeat and English parents-movie hint.
	// Otherwise "bosque..." or "glove!" spills onto a page of its own.
	// Each first chunk is also present in the other game's different caption.
	{ "mechant1.gce", kGameTypeAbracadabra, 10, 0, " muy agotado, debe", " muy agotado, debe descansar," },
	{ "mechant1.gce", kGameTypeAbracadabra, 10, 1,
		"descansar, para volver al principio del bosque...", "para volver al principio del bosque..." },
	{ "mefait.gca", kGameTypeBabaYaga, 20, 0, " could find it,", " could find it, he could" },
	{ "mefait.gca", kGameTypeBabaYaga, 20, 1,
		"he could go after Baba Yaga and the magic glove!", "go after Baba Yaga and the magic glove!" },
	// Baba Yaga DOS's longer Spanish warning strands the closing punctuation.
	{ "mefait.gce", kGameTypeNone, 10, 0,
		"\"\255 Cierra bien las puertas y no abras a nadie !\"", "\"\255Cierra bien las puertas y no abras a nadie!\"" },
	{ "mefait.gca", kGameTypeNone, 16, 3, "Look some where else!", "Look somewhere else!" },
	// Add vocative punctuation around the child-name reference without
	// replacing the name or the German and Spanish spelling variants.
	{ "mechant1.gct", kGameTypeNone, 2, 0, "Attention ", "Attention, " },
	{ "mechant1.gcg", kGameTypeNone, 2, 0, "Aufgepa\341t ", "Aufgepa\341t, " },
	{ "mechant1.gcg", kGameTypeNone, 2, 0, "Aufgepasst ", "Aufgepasst, " },
	{ "mechant1.gca", kGameTypeNone, 2, 0, "Look out ", "Look out, " },
	{ "mechant1.gce", kGameTypeNone, 2, 0, "\255 Atenci\242n ", "\255Atenci\242n, " },
	{ "mechant1.gce", kGameTypeNone, 2, 0, " !", "!" },
	{ "mechant1.gci", kGameTypeNone, 2, 0, "Attenzione ", "Attenzione, " },
	{ "sorciere.gct", kGameTypeNone, 2, 0, "Attention ", "Attention, " },
	{ "sorciere.gcg", kGameTypeNone, 2, 0, "Achtung ", "Achtung, " },
	{ "sorciere.gca", kGameTypeNone, 2, 0, "Look out ", "Look out, " },
	{ "sorciere.gca", kGameTypeNone, 2, 1, "Fight back ", "Fight back, " },
	{ "sorciere.gce", kGameTypeNone, 2, 0, "\255 Atenci\242n ", "\255Atenci\242n, " },
	{ "sorciere.gce", kGameTypeNone, 2, 0, " !", "!" },
	{ "sorciere.gci", kGameTypeNone, 2, 0, "Attenzione ", "Attenzione, " },
	{ "foretm.gce", kGameTypeNone, 2, 2, "\255 Vamos ", "\255Vamos, " },
	{ "foretm.gce", kGameTypeNone, 2, 2, " !", "!" },
	{ "foretg.gce", kGameTypeNone, 2, 4, "S\202 valiente ", "S\202 valiente, " },
	{ "foretg.gce", kGameTypeNone, 2, 4, "Se valiente ", "Se valiente, " }
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_TEXT_CORRECTIONS_H
