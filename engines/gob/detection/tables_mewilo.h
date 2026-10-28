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

// Amstrad CPC French release, extracted from both original disk sides/images.
// This entry matches CP/M files with their 128-byte AMSDOS wrappers retained;
// the runtime strips valid wrappers before handing data to the shared parser.
{
	{"mewilo",
	 "AMSDOS files",
	 AD_ENTRY3s(
		 "AMSOAD.TOT", "d9d0dcbdf64ef28712aa13cc285a293a", 2688,
		 "AMSOAD1.TOT", "22e4651a0a443de16d7b9fe8147b39c8", 8832,
		 "LETTRES.BIN", "9d6eddc84b382b0e42e60f55c5ddc0d9", 1152),
	 FR_FRA,
	 kPlatformAmstradCPC,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors,
	nullptr,
	"AMSOAD.TOT",
	0},

// The same CPC release with AMSDOS headers stripped by an extraction tool.
{
	{"mewilo",
	 "raw files",
	 AD_ENTRY3s(
		 "AMSOAD.TOT", "7b9eadd6c0f2f5e1bcb7cec3b6fa7347", 2544,
		 "AMSOAD1.TOT", "a12465d6e78ae7d7c65c0bb51057b17f", 8585,
		 "LETTRES.BIN", "35a907ed7ab1ce85311558559f7b078d", 1024),
	 FR_FRA,
	 kPlatformAmstradCPC,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors,
	nullptr,
	"AMSOAD.TOT",
	0},


#endif // GOB_DETECTION_TABLES_MEWILO_H
