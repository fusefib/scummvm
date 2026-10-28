/* Mewilo resource-family anchors use the detector's first 5000 bytes. */

#ifndef GOB_DETECTION_TABLES_MEWILO_H
#define GOB_DETECTION_TABLES_MEWILO_H

{
	{"mewilo",
	 "German resources - Atari ST profile",
	 AD_ENTRY2s(
		 "LOAD.TOT", "8ceda438ecc01148f0da4c9dd771aa49", 16523,
		 "LETTRES.BIN", "89daf7e6ab6bd6c5127832a6145c3b31", 1024),
	 DE_DEU,
	 kPlatformAtariST,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors,
	nullptr,
	"LOAD.TOT",
	0},

{
	{"mewilo",
	 "French resources - Atari ST profile",
	 AD_ENTRY2s(
		 "LOAD.TOT", "b33149fd0b5c5aefc5b64ce2ded74a1e", 16349,
		 "LETTRES.BIN", "89daf7e6ab6bd6c5127832a6145c3b31", 1024),
	 FR_FRA,
	 kPlatformAtariST,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors,
	nullptr,
	"LOAD.TOT",
	0},

#endif // GOB_DETECTION_TABLES_MEWILO_H
