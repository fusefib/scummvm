/* Mewilo resource-family anchors use the detector's first 5000 bytes. */

#ifndef GOB_DETECTION_TABLES_MEWILO_H
#define GOB_DETECTION_TABLES_MEWILO_H

{
	{"mewilo",
	 "",
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
	 "",
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
	 "AMSDOS",
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
	 "raw",
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


{
	{"mewilo",
	 "",
	 AD_ENTRY2s(
		 "LOAD.TOT", "8ceda438ecc01148f0da4c9dd771aa49", 16523,
		 "LETTRES.BIN", "89daf7e6ab6bd6c5127832a6145c3b31", 1024),
	 DE_DEU,
	 kPlatformAmiga,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors,
	nullptr,
	"LOAD.TOT",
	0},

{
	{"mewilo",
	 "",
	 AD_ENTRY2s(
		 "LOAD.TOT", "b33149fd0b5c5aefc5b64ce2ded74a1e", 16349,
		 "LETTRES.BIN", "89daf7e6ab6bd6c5127832a6145c3b31", 1024),
	 FR_FRA,
	 kPlatformAmiga,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors,
	nullptr,
	"LOAD.TOT",
	0},

// Thomson TO8-family French release. Canonical DOS extraction only: 255
// data bytes per physical sector for BIN/BASIC; custom TOT/I00 retain 256.
// The earlier 7907/1038-byte BIN extraction is deliberately unsupported.
// Common::Platform has no Thomson target,
// so a Mewilo-specific feature selects the THLOAD/native-6809 host profile.
{
	{"mewilo",
	 "Thomson TO",
	 AD_ENTRY3s(
		 "PARTIE4.BIN", "4ff70a80f5ec33b7eb3ddb8df0739af5", 7877,
		 "THLOAD.TOT", "65b9355f7ad1c15c468e39d57aa3f240", 17549,
		 "LETTRES.BIN", "fa838215f362888651db1ef7f0f20370", 1034),
	 FR_FRA,
	 kPlatformUnknown,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors | kFeaturesMewiloThomson,
	nullptr,
	"THLOAD.TOT",
	0},

// Same canonical Thomson resources read directly from the verified raw FD.
// Rename the image to mewilo.fd without modifying its bytes. The first 5000
// bytes coincide with THLOAD.TOT; FAT/directory and all resource contracts are
// validated by the separate reader and normal loaders, not this anchor alone.
{
	{"mewilo",
	 "Thomson TO (FD image)",
	 AD_ENTRY1s("mewilo.fd", "65b9355f7ad1c15c468e39d57aa3f240", 327680),
	 FR_FRA,
	 kPlatformUnknown,
	 ADGF_UNSTABLE,
	 GUIO2(GUIO_NOSUBTITLES, GUIO_NOSPEECH)},
	kFeatures16Colors | kFeaturesMewiloThomson | kFeaturesMewiloThomsonFD,
	nullptr,
	"THLOAD.TOT",
	0},

#endif // GOB_DETECTION_TABLES_MEWILO_H
