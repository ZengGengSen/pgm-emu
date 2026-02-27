// FinalBurn Neo - Emulator for MC68000/Z80 based arcade games
//            Refer to the "license.txt" file for more info

// Burner emulation library
#ifndef _BURNH_H
#define _BURNH_H

#ifdef __cplusplus
 extern "C" {
#endif

#if !defined (_WIN32)
 #define __cdecl
#endif

#if !defined (_MSC_VER) && defined(FASTCALL)
 #undef __fastcall
 #define __fastcall __attribute__((fastcall))
#endif

#ifndef MAX_PATH
 #define MAX_PATH 	260
#endif

#include <time.h>

extern TCHAR szAppHiscorePath[MAX_PATH];
extern TCHAR szAppSamplesPath[MAX_PATH];
extern TCHAR szAppHDDPath[MAX_PATH];
extern TCHAR szAppBlendPath[MAX_PATH];
extern TCHAR szAppEEPROMPath[MAX_PATH];

// Macro to determine the size of a struct up to and including "member"
#define STRUCT_SIZE_HELPER(type, member) offsetof(type, member) + sizeof(((type*)0)->member)

// Give access to the CPUID function for various compilers
#if defined (__GNUC__)
 #define CPUID(f,ra,rb,rc,rd) __asm__ __volatile__ ("cpuid"											\
													: "=a" (ra), "=b" (rb), "=c" (rc), "=d" (rd)	\
													: "a"  (f)										\
												   );
#elif defined (_MSC_VER)
 #define CPUID(f,ra,rb,rc,rd) __asm { __asm mov		eax, f		\
									  __asm cpuid				\
									  __asm mov		ra, eax		\
									  __asm mov		rb, ebx		\
									  __asm mov		rc, ecx		\
									  __asm mov		rd, edx }
#else
 #define CPUID(f,ra,rb,rc,rd)
#endif

#ifndef BUILD_X86_ASM
 #undef CPUID
 #define CPUID(f,ra,rb,rc,rd)
#endif

#ifdef _UNICODE
 #define SEPERATOR_1 " \u2022 "
 #define SEPERATOR_2 " \u25E6 "
#else
 #define SEPERATOR_1 " ~ "
 #define SEPERATOR_2 " ~ "
#endif

#ifdef _UNICODE
 #define WRITE_UNICODE_BOM(file) { UINT16 BOM[] = { 0xFEFF }; fwrite(BOM, 2, 1, file); }
#else
 #define WRITE_UNICODE_BOM(file)
#endif

typedef unsigned char						UINT8;
typedef signed char 						INT8;
typedef unsigned short						UINT16;
typedef signed short						INT16;
typedef unsigned int						UINT32;
typedef signed int							INT32;
#ifdef _MSC_VER
typedef signed __int64						INT64;
typedef unsigned __int64					UINT64;
#else
__extension__ typedef unsigned long long	UINT64;
__extension__ typedef long long				INT64;
#endif

#include "state.h"
#include "cheat.h"
#include "hiscore.h"

extern INT32 nBurnVer;						// Version number of the library

enum BurnCartrigeCommand { CART_INIT_START, CART_INIT_END, CART_EXIT };

// ---------------------------------------------------------------------------
// Callbacks

// Application-defined rom loading function
extern INT32 (__cdecl *BurnExtLoadRom)(UINT8* Dest, INT32* pnWrote, INT32 i);

// Application-defined progress indicator functions
extern INT32 (__cdecl *BurnExtProgressRangeCallback)(double dProgressRange);
extern INT32 (__cdecl *BurnExtProgressUpdateCallback)(double dProgress, const TCHAR* pszText, bool bAbs);

// Application-defined catridge initialisation function
extern INT32 (__cdecl *BurnExtCartridgeSetupCallback)(BurnCartrigeCommand nCommand);

// Application-defined colour conversion function
extern UINT32 (__cdecl *BurnHighCol) (INT32 r, INT32 g, INT32 b, INT32 i);

// ---------------------------------------------------------------------------

extern UINT32 nCurrentFrame;

inline static INT32 GetCurrentFrame() {
	return nCurrentFrame;
}

inline static void SetCurrentFrame(const UINT32 n) {
	nCurrentFrame = n;
}

// ---------------------------------------------------------------------------
// Driver info structures

// ROMs

#define BRF_PRG				(1 << 20)
#define BRF_GRA				(1 << 21)
#define BRF_SND				(1 << 22)

#define BRF_ESS				(1 << 24)
#define BRF_BIOS			(1 << 25)
#define BRF_SELECT			(1 << 26)
#define BRF_OPT				(1 << 27)
#define BRF_NODUMP			(1 << 28)

struct BurnRomInfo {
	char *szName;
	UINT32 nLen;
	UINT32 nCrc;
	UINT32 nType;
};

struct BurnSampleInfo {
	char *szName;
	UINT32 nFlags;
};

struct BurnHDDInfo {
	char *szName;
	UINT32 nLen;
	UINT32 nCrc;
};

// ---------------------------------------------------------------------------

// Rom Data

struct RomDataInfo {
	char szZipName[MAX_PATH];
	char szDrvName[MAX_PATH];
	char szExtraRom[MAX_PATH];
	wchar_t szOldName[MAX_PATH];
	wchar_t szFullName[MAX_PATH];
	INT32 nDriverId;
	INT32 nDescCount;
};

extern RomDataInfo* pRDI;
extern BurnRomInfo* pDataRomDesc;

char* RomdataGetDrvName();
void RomDataSetFullName();
void RomDataInit();
void RomDataExit();

// ---------------------------------------------------------------------------

// Inputs

#define BIT_DIGITAL			(1)

#define BIT_GROUP_ANALOG	(4)
#define BIT_ANALOG_REL		(4)
#define BIT_ANALOG_ABS		(5)

#define BIT_GROUP_CONSTANT	(8)
#define BIT_CONSTANT		(8)
#define BIT_DIPSWITCH		(9)

struct BurnInputInfo {
	char* szName;
	UINT8 nType;
	union {
		UINT8* pVal;					// Most inputs use a char*
		UINT16* pShortVal;				// All analog inputs use a short*
	};
	char* szInfo;
};

// DIPs

struct BurnDIPInfo {
	INT32 nInput;
	UINT8 nFlags;
	UINT8 nMask;
	UINT8 nSetting;
	char* szText;
};

#define DIP_OFFSET(x) {x, 0xf0, 0xff, 0xff, NULL},

// ---------------------------------------------------------------------------
// Common CPU definitions

// sync to nCyclesDone[]
#define CPU_RUN(num,proc) do { nCyclesDone[num] += proc ## Run(((i + 1) * nCyclesTotal[num] / nInterleave) - nCyclesDone[num]); } while (0)
#define CPU_IDLE(num,proc) do { nCyclesDone[num] += proc ## Idle(((i + 1) * nCyclesTotal[num] / nInterleave) - nCyclesDone[num]); } while (0)
#define CPU_IDLE_NULL(num) do { nCyclesDone[num] += ((i + 1) * nCyclesTotal[num] / nInterleave) - nCyclesDone[num]; } while (0)
// sync to cpuTotalCycles()
#define CPU_RUN_SYNCINT(num,proc) do { nCyclesDone[num] += proc ## Run(((i + 1) * nCyclesTotal[num] / nInterleave) - proc ## TotalCycles()); } while (0)
#define CPU_IDLE_SYNCINT(num,proc) do { nCyclesDone[num] += proc ## Idle(((i + 1) * nCyclesTotal[num] / nInterleave) - proc ## TotalCycles()); } while (0)
// sync to timer
#define CPU_RUN_TIMER(num) do { BurnTimerUpdate((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrame(nCyclesTotal[num]); } while (0)
#define CPU_RUN_TIMER_YM3812(num) do { BurnTimerUpdateYM3812((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrameYM3812(nCyclesTotal[num]); } while (0)
#define CPU_RUN_TIMER_YM3526(num) do { BurnTimerUpdateYM3526((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrameYM3526(nCyclesTotal[num]); } while (0)
#define CPU_RUN_TIMER_Y8950(num) do { BurnTimerUpdateY8950((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrameY8950(nCyclesTotal[num]); } while (0)
// speed adjuster
INT32 BurnSpeedAdjust(INT32 cyc);

#define CPU_IRQSTATUS_NONE	0
#define CPU_IRQSTATUS_ACK	1
#define CPU_IRQSTATUS_AUTO	2
#define CPU_IRQSTATUS_HOLD	4

#define CPU_IRQLINE0		0
#define CPU_IRQLINE1		1
#define CPU_IRQLINE2		2
#define CPU_IRQLINE3		3
#define CPU_IRQLINE4		4
#define CPU_IRQLINE5		5
#define CPU_IRQLINE6		6
#define CPU_IRQLINE7		7

#define CPU_IRQLINE_IRQ		CPU_IRQLINE0
#define CPU_IRQLINE_FIRQ	CPU_IRQLINE1
#define CPU_IRQLINE_NMI		0x20

#define MAP_READ		1
#define MAP_WRITE		2
#define MAP_FETCHOP		4
#define MAP_FETCHARG		8
#define MAP_FETCH		(MAP_FETCHOP|MAP_FETCHARG)
#define MAP_ROM			(MAP_READ|MAP_FETCH)
#define MAP_RAM			(MAP_ROM|MAP_WRITE)

// Macros to Allocate and Free MemIndex
#define BurnAllocMemIndex() do {                				\
	AllMem = NULL;                                 				\
	MemIndex();                                 				\
	INT32 nLen = MemEnd - (UINT8 *)0;           				\
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;	\
	memset(AllMem, 0, nLen);                       				\
	MemIndex();                                 				\
} while (0)

#define BurnFreeMemIndex() do { BurnFree(AllMem); } while (0)

// ---------------------------------------------------------------------------

extern bool bBurnUseMMX;
#ifdef BUILD_A68K
extern bool bBurnUseASMCPUEmulation;
#endif

extern UINT32 nFramesEmulated;
extern UINT32 nFramesRendered;
extern clock_t starttime;			// system time when emulation started and after roms loaded

extern bool bForce60Hz;
extern bool bSpeedLimit60hz;
extern double dForcedFrameRate;

extern bool bBurnUseBlend;

extern INT32 nBurnFPS;
extern INT32 nBurnCPUSpeedAdjust;

extern UINT32 nBurnDrvCount;		// Count of game drivers
extern UINT32 nBurnDrvActive;		// Which game driver is selected
extern INT32 nBurnDrvSubActive;		// Which sub-game driver is selected
extern UINT32 nBurnDrvSelect[8];	// Which games are selected (i.e. loaded but not necessarily active)

extern char* pszCustomNameA;
extern char szBackupNameA[MAX_PATH];
extern TCHAR szBackupNameW[MAX_PATH];

extern char** szShortNamesExArray;
extern TCHAR** szLongNamesExArray;
extern UINT32 nNamesExArray;

extern INT32 nMaxPlayers;

extern UINT8 *pBurnDraw;			// Pointer to correctly sized bitmap
extern INT32 nBurnPitch;			// Pitch between each line
extern INT32 nBurnBpp;				// Bytes per pixel (2, 3, or 4)

extern UINT8 nBurnLayer;			// Can be used externally to select which layers to show
extern UINT8 nSpriteEnable;			// Can be used externally to select which Sprites to show

extern INT32 bRunAhead;				// "Run Ahead" lag-reduction technique UI option (on/off)

extern INT32 bBurnRunAheadFrame;	// for drivers, hiscore, etc, to recognize that this is the "runahead frame"
									// for instance, you wouldn't want to apply hi-score data on a "runahead frame"

extern INT32 nBurnSoundRate;		// Samplerate of sound
extern INT32 nBurnSoundLen;			// Length in samples per frame
extern INT16* pBurnSoundOut;		// Pointer to output buffer

extern INT32 nInterpolation;		// Desired interpolation level for ADPCM/PCM sound
extern INT32 nFMInterpolation;		// Desired interpolation level for FM sound

extern UINT32 *pBurnDrvPalette;

#define PRINT_NORMAL	(0)
#define PRINT_UI		(1)
#define PRINT_IMPORTANT (2)
#define PRINT_ERROR		(3)
#define PRINT_LEVEL1	(4)
#define PRINT_LEVEL2	(5)
#define PRINT_LEVEL3	(6)
#define PRINT_LEVEL4	(7)
#define PRINT_LEVEL5	(8)
#define PRINT_LEVEL6	(9)
#define PRINT_LEVEL7	(10)
#define PRINT_LEVEL8	(11)
#define PRINT_LEVEL9	(12)
#define PRINT_LEVEL10	(13)

#ifndef bprintf
extern INT32 (__cdecl *bprintf) (INT32 nStatus, TCHAR* szFormat, ...);
#endif

INT32 BurnLibInit();
INT32 BurnLibExit();

INT32 BurnDrvInit();
INT32 BurnDrvExit();

INT32 BurnDrvCartridgeSetup(BurnCartrigeCommand nCommand);

INT32 BurnDrvFrame();
INT32 BurnDrvRedraw();
INT32 BurnRecalcPal();
INT32 BurnDrvGetPaletteEntries();

INT32 BurnSetProgressRange(double dProgressRange);
INT32 BurnUpdateProgress(double dProgressStep, const TCHAR* pszText, bool bAbs);

void BurnLocalisationSetName(char *szName, TCHAR *szLongName);

void BurnGetLocalTime(tm *nTime);                   // Retrieve local-time of machine w/tweaks for netgame and input recordings
UINT16 BurnRandom();                                // State-able Random Number Generator (0-32767)
void BurnRandomScan(INT32 nAction);                 // Must be called in driver's DrvScan() if BurnRandom() is used
void BurnRandomInit();                              // Called automatically in BurnDrvInit() / Internal use only
void BurnRandomSetSeed(UINT64 nSeed);               // Set the seed - useful for netgames / input recordings

// Handy FM default callbacks
INT32 BurnSynchroniseStream(INT32 nSoundRate);
double BurnGetTime();

// Handy functions for changing resolution
extern void (__cdecl *BurnResizeCallback)(INT32 width, INT32 height);
void BurnSetResolution(INT32 width, INT32 height);

// Handy defines
#define d_min(a, b) (((a) < (b)) ? (a) : (b))
#define d_max(a, b) (((a) > (b)) ? (a) : (b))
#define d_abs(z) (((z) < 0) ? -(z) : (z))

// ---------------------------------------------------------------------------
// Retrieve driver information

#define DRV_NAME		 (0)
#define DRV_DATE		 (1)
#define DRV_FULLNAME	 (2)
//#define DRV_MEDIUMNAME	 (3)
#define DRV_COMMENT		 (4)
#define DRV_MANUFACTURER (5)
#define DRV_SYSTEM		 (6)
#define DRV_PARENT		 (7)
#define DRV_BOARDROM	 (8)
#define DRV_SAMPLENAME	 (9)

#define DRV_NEXTNAME	 (1 << 8)
#define DRV_ASCIIONLY	 (1 << 12)
#define DRV_UNICODEONLY	 (1 << 13)

TCHAR* BurnDrvGetText(UINT32 i);
char* BurnDrvGetTextA(UINT32 i);
wchar_t* BurnDrvGetFullNameW(UINT32 i);

INT32 BurnDrvGetIndex(char* szName);
INT32 BurnDrvGetZipName(char** pszName, UINT32 i);
INT32 BurnDrvSetZipName(char* szName, INT32 i);
INT32 BurnDrvGetRomInfo(struct BurnRomInfo *pri, UINT32 i);
INT32 BurnDrvGetRomName(char** pszName, UINT32 i, INT32 nAka);
INT32 BurnDrvGetInputInfo(struct BurnInputInfo* pii, UINT32 i);
INT32 BurnDrvGetDIPInfo(struct BurnDIPInfo* pdi, UINT32 i);
INT32 BurnDrvGetVisibleSize(INT32* pnWidth, INT32* pnHeight);
INT32 BurnDrvGetOriginalVisibleSize(INT32* pnWidth, INT32* pnHeight);
INT32 BurnDrvGetVisibleOffs(INT32* pnLeft, INT32* pnTop);
INT32 BurnDrvGetFullSize(INT32* pnWidth, INT32* pnHeight);
INT32 BurnDrvGetAspect(INT32* pnXAspect, INT32* pnYAspect);
INT32 BurnDrvGetHardwareCode();
INT32 BurnDrvGetFlags();
bool BurnDrvIsWorking();
INT32 BurnDrvGetMaxPlayers();
INT32 BurnDrvSetVisibleSize(INT32 pnWidth, INT32 pnHeight);
INT32 BurnDrvSetAspect(INT32 pnXAspect, INT32 pnYAspect);
INT32 BurnDrvGetGenreFlags();
INT32 BurnDrvGetFamilyFlags();
INT32 BurnDrvGetSampleInfo(struct BurnSampleInfo *pri, UINT32 i);
INT32 BurnDrvGetSampleName(char** pszName, UINT32 i, INT32 nAka);
INT32 BurnDrvGetHDDInfo(struct BurnHDDInfo *pri, UINT32 i);
INT32 BurnDrvGetHDDName(char** pszName, UINT32 i, INT32 nAka);
char* BurnDrvGetSourcefile();


void Reinitialise(); // re-inits everything, including UI window
void ReinitialiseVideo(); // re-init's video w/ new resolution/aspect ratio (see drv/megadrive.cpp)

// ---------------------------------------------------------------------------
// IPS Control

#define IPS_NOT_PROTECT		(1 <<  0)	// Protection switche for NeoGeo, etc.
#define IPS_PGM_SPRHACK		(1 <<  1)	// For PGM hacks...
#define IPS_PGM_MAPHACK		(1 <<  2)	// Id.
#define IPS_PGM_SNDOFFS		(1 <<  3)	// Id.
#define IPS_LOAD_EXPAND		(1 <<  4)	// Change temporary cache size on double load.
#define IPS_EXTROM_INCL		(1 <<  5)	// Extra rom.
#define IPS_PRG1_EXPAND		(1 <<  6)	// Additional request for prg length.
#define IPS_PRG2_EXPAND		(1 <<  7)	// Id.
#define IPS_GRA1_EXPAND		(1 <<  8)	// Additional request for gfx length.
#define IPS_GRA2_EXPAND		(1 <<  9)	// Id.
#define IPS_GRA3_EXPAND		(1 << 10)	// Id.
#define IPS_ACPU_EXPAND		(1 << 11)	// Additional request for audio cpu length.
#define IPS_SND1_EXPAND		(1 << 12)	// Additional request for snd length.
#define IPS_SND2_EXPAND		(1 << 13)	// Id.
#define IPS_SNES_VRAMHK		(1 << 14)	// Allow invalid vram writes.
#define IPS_NEO_RAMHACK		(1 << 15)	// Neo Geo 68kram hack.

enum IpsRomTypes { EXP_FLAG, LOAD_ROM, EXTR_ROM, PRG1_ROM, PRG2_ROM, GRA1_ROM, GRA2_ROM, GRA3_ROM, ACPU_ROM, SND1_ROM, SND2_ROM };
extern UINT32 nIpsDrvDefine, nIpsMemExpLen[SND2_ROM + 1];

extern bool bDoIpsPatch;

void IpsApplyPatches(UINT8* base, char* rom_name, UINT32 rom_crc, bool readonly = false);

// ---------------------------------------------------------------------------
// MISC Helper / utility functions, etc
int BurnComputeSHA1(const UINT8 *buffer, int buffer_size, char *hash_str);
//int BurnComputeSHA1(const char *filename, char *hash_str);

// ---------------------------------------------------------------------------
// Flags used with the Burndriver structure

// Flags for the flags member
#define BDF_GAME_NOT_WORKING							(0)
#define BDF_GAME_WORKING								(1 << 0)
#define BDF_ORIENTATION_FLIPPED							(1 << 1)
#define BDF_ORIENTATION_VERTICAL						(1 << 2)
#define BDF_BOARDROM									(1 << 3)
#define BDF_CLONE										(1 << 4)
#define BDF_BOOTLEG										(1 << 5)
#define BDF_PROTOTYPE									(1 << 6)
#define BDF_HACK										(1 << 7)
#define BDF_HOMEBREW									(1 << 8)
#define BDF_DEMO										(1 << 9)
#define BDF_16BIT_ONLY									(1 << 10)
#define BDF_32BIT_ONLY									(1 << 11)
#define BDF_HISCORE_SUPPORTED							(1 << 12)
#define BDF_RUNAHEAD_DRAWSYNC							(1 << 13)
#define BDF_RUNAHEAD_DISABLED							(1 << 14)

// Flags for the hardware member
// Format: 0xDDEEFFFF, where DD: Manufacturer, EE: Hardware platform, FFFF: Flags (used by driver)

#define HARDWARE_PUBLIC_MASK							(0x7FFF0000)

#define HARDWARE_PREFIX_CARTRIDGE				 ((INT32)0x80000000)

#define HARDWARE_PREFIX_IGS_PGM							(0x08000000)
#define HARDWARE_IGS_PGM								(HARDWARE_PREFIX_IGS_PGM)
#define HARDWARE_IGS_USE_ARM_CPU						(0x0001)

// flags for the genre member
#define GBF_HORSHOOT									(1 << 0)
#define GBF_VERSHOOT									(1 << 1)
#define GBF_SCRFIGHT									(1 << 2)
#define GBF_VSFIGHT										(1 << 3)
#define GBF_BIOS										(1 << 4)
#define GBF_BREAKOUT									(1 << 5)
#define GBF_CASINO										(1 << 6)
#define GBF_BALLPADDLE									(1 << 7)
#define GBF_MAZE										(1 << 8)
#define GBF_MINIGAMES									(1 << 9)
#define GBF_PINBALL										(1 << 10)
#define GBF_PLATFORM									(1 << 11)
#define GBF_PUZZLE										(1 << 12)
#define GBF_QUIZ										(1 << 13)
#define GBF_SPORTSMISC									(1 << 14)
#define GBF_SPORTSFOOTBALL								(1 << 15)
#define GBF_MISC										(1 << 16)
#define GBF_MAHJONG										(1 << 17)
#define GBF_RACING										(1 << 18)
#define GBF_SHOOT										(1 << 19)
#define GBF_MULTISHOOT									(1 << 20)
#define GBF_ACTION  									(1 << 21)
#define GBF_RUNGUN  									(1 << 22)
#define GBF_STRATEGY									(1 << 23)
#define GBF_VECTOR                                      (1 << 24)
#define GBF_RPG                                         (1 << 25)
#define GBF_SIM                                         (1 << 26)
#define GBF_ADV                                         (1 << 27)
#define GBF_CARD                                        (1 << 28)
#define GBF_BOARD                                       (1 << 29)

// flags for the family member
#define FBF_MSLUG										(1 << 0)
#define FBF_SF											(1 << 1)
#define FBF_KOF											(1 << 2)
#define FBF_DSTLK										(1 << 3)
#define FBF_FATFURY										(1 << 4)
#define FBF_SAMSHO										(1 << 5)
#define FBF_19XX										(1 << 6)
#define FBF_SONICWI										(1 << 7)
#define FBF_PWRINST										(1 << 8)
#define FBF_SONIC										(1 << 9)
#define FBF_DONPACHI                                    (1 << 10)
#define FBF_MAHOU                                       (1 << 11)

#ifdef __cplusplus
 } // End of extern "C"
#endif

#endif
