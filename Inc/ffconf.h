/*---------------------------------------------------------------------------/
/  Configurations of FatFs Module  R0.15
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	80286	/* Revision ID — must match FF_DEFINED in ff.h */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	0
/* 0: Read/Write — required for recording (f_write, f_open FA_CREATE_ALWAYS) */

#define FF_FS_MINIMIZE	0
/* 0: All basic functions enabled — required for f_lseek (WAV header patch on stop) */

#define FF_USE_FIND		0

#define FF_USE_MKFS		1
/* 1: Enable f_mkfs() — useful if the SD card needs formatting */

#define FF_USE_FASTSEEK	1
/* 1: Speeds up f_lseek when rewriting the WAV header size fields */

#define FF_USE_EXPAND	0

#define FF_USE_CHMOD	0

#define FF_USE_LABEL	0

#define FF_USE_FORWARD	0

#define FF_USE_STRFUNC	0
#define FF_PRINT_LLI	0
#define FF_PRINT_FLOAT	0
#define FF_STRF_ENCODE	3

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	850
/* Latin-1 — sufficient for plain ASCII filenames (Wave.wav, etc.) */

#define FF_USE_LFN		1
#define FF_MAX_LFN		255
/* 1: LFN with static buffer on BSS — single-threaded, no RTOS, safe here.
/  Allows long filenames. Requires ffunicode.c in the build. */

#define FF_LFN_UNICODE	0
/* 0: ANSI/OEM (char paths) */

#define FF_LFN_BUF		255
#define FF_SFN_BUF		12

#define FF_FS_RPATH		0

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		1
/* 1 volume — SD card only, USB host removed */

#define FF_STR_VOLUME_ID	0
#define FF_VOLUME_STRS		"SD"

#define FF_MULTI_PARTITION	0

#define FF_MIN_SS		512
#define FF_MAX_SS		512
/* SD cards use 512-byte sectors */

#define FF_LBA64		0

#define FF_MIN_GPT		0x10000000

#define FF_USE_TRIM		0

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		0

#define FF_FS_EXFAT		0

#define FF_FS_NORTC		1
#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2024
/* 1: No RTC on F769-DISCO — use fixed timestamp, no get_fattime() needed */

#define FF_FS_NOFSINFO	0

#define FF_FS_LOCK		2
/* 2: Allow up to 2 simultaneously open file objects (record + playback) */

#define FF_FS_REENTRANT	0
#define FF_FS_TIMEOUT	1000
/* 0: No RTOS, single-threaded */

/*--- End of configuration options ---*/
