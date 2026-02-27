// Burn - Drivers module

#include "version.h"
#include "burnint.h"
#include "timer.h"
//#include "burn_sound.h" // included in burnint.h
#include "driverlist.h"

#ifdef BUILD_WIN32
#include "mbtwc.h"
#endif

#ifndef __LIBRETRO__
// filler function, used if the application is not printing debug messages
static INT32 __cdecl BurnbprintfFiller(INT32, TCHAR* , ...) { return 0; }
// pointer to burner printing function
#ifndef bprintf
INT32 (__cdecl *bprintf)(INT32 nStatus, TCHAR* szFormat, ...) = BurnbprintfFiller;
#endif
#endif

INT32 nBurnVer = BURN_VERSION;	// Version number of the library

UINT32 nBurnDrvCount     = 0;	// Count of game drivers
UINT32 nBurnDrvActive    = ~0U;	// Which game driver is selected
INT32 nBurnDrvSubActive  = -1;	// Which sub-game driver is selected
UINT32 nBurnDrvSelect[8] = { ~0U, ~0U, ~0U, ~0U, ~0U, ~0U, ~0U, ~0U }; // Which games are selected (i.e. loaded but not necessarily active)

char* pszCustomNameA = NULL;
char szBackupNameA[MAX_PATH];
TCHAR szBackupNameW[MAX_PATH];

char** szShortNamesExArray = NULL;
TCHAR** szLongNamesExArray = NULL;
UINT32 nNamesExArray       = 0;

bool bBurnUseMMX;
#if defined BUILD_A68K
bool bBurnUseASMCPUEmulation = false;
#endif

// Just so we can start using FBNEO_DEBUG and keep backwards compatablity should whatever is left of FB Alpha rise from it's grave.
#if defined (FBNEO_DEBUG) && (!defined FBA_DEBUG)
#define FBA_DEBUG 1
#endif

#if defined (FBNEO_DEBUG)
 clock_t starttime = 0;
#endif

UINT32 nCurrentFrame;				// Framecount for emulated game

UINT32 nFramesEmulated;				// Counters for FPS	display
UINT32 nFramesRendered;
bool bForce60Hz           = false;
bool bSpeedLimit60hz      = true;
double dForcedFrameRate   = 60.00;
bool bBurnUseBlend        = true;
INT32 nBurnFPS            = 6000;
INT32 nBurnCPUSpeedAdjust = 0x0100;	// CPU speed adjustment (clock * nBurnCPUSpeedAdjust / 0x0100)

// Burn Draw:
UINT8* pBurnDraw = NULL;			// Pointer to correctly sized bitmap
INT32 nBurnPitch = 0;				// Pitch between each line
INT32 nBurnBpp;						// Bytes per pixel (2, 3, or 4)

INT32 nBurnSoundRate = 0;			// sample rate of sound or zero for no sound
INT32 nBurnSoundLen  = 0;			// length in samples per frame
INT16* pBurnSoundOut = NULL;		// pointer to output buffer

INT32 nInterpolation   = 1;			// Desired interpolation level for ADPCM/PCM sound
INT32 nFMInterpolation = 0;			// Desired interpolation level for FM sound

UINT8 nBurnLayer    = 0xFF;			// Can be used externally to select which layers to show
UINT8 nSpriteEnable = 0xFF;			// Can be used externally to select which layers to show

INT32 bRunAhead = 0;

INT32 nMaxPlayers;

bool bSaveCRoms = 0;

UINT32 *pBurnDrvPalette;

static char** pszShortName = NULL, ** pszFullNameA = NULL;
static wchar_t** pszFullNameW = NULL;

bool BurnCheckMMXSupport()
{
#if defined BUILD_X86_ASM
	UINT32 nSignatureEAX = 0, nSignatureEBX = 0, nSignatureECX = 0, nSignatureEDX = 0;

	CPUID(1, nSignatureEAX, nSignatureEBX, nSignatureECX, nSignatureEDX);

	return (nSignatureEDX >> 23) & 1;						// bit 23 of edx indicates MMX support
#else
	return 0;
#endif
}

static void BurnGameListInit()
{	// Avoid broken references, RomData requires separate string storage
	if (0 == nBurnDrvCount) return;

		pszShortName = (char**   )malloc(nBurnDrvCount * sizeof(char*));
		pszFullNameA = (char**   )malloc(nBurnDrvCount * sizeof(char*));
		pszFullNameW = (wchar_t**)malloc(nBurnDrvCount * sizeof(wchar_t*));

		if ((NULL != pszShortName) && (NULL != pszFullNameA) && (NULL != pszFullNameW)) {
			for (UINT32 i = 0; i < nBurnDrvCount; i++) {
				pszShortName[i] = (char*   )malloc(100      * sizeof(char));
				pszFullNameA[i] = (char*   )malloc(MAX_PATH * sizeof(char));
				pszFullNameW[i] = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));

				memset(pszShortName[i], '\0', 100      * sizeof(char));
				memset(pszFullNameA[i], '\0', MAX_PATH * sizeof(char));
				memset(pszFullNameW[i], '\0', MAX_PATH * sizeof(wchar_t));

				if (NULL != pszShortName[i]) {
					strcpy(pszShortName[i], pDriver[i]->szShortName);
					pDriver[i]->szShortName = pszShortName[i];
				}
				if (NULL != pszFullNameA[i]) {
					strcpy(pszFullNameA[i], pDriver[i]->szFullNameA);
					pDriver[i]->szFullNameA = pszFullNameA[i];
				}
#if defined (_UNICODE)
				if (NULL != pDriver[i]->szFullNameW) {
					wmemcpy(pszFullNameW[i], pDriver[i]->szFullNameW, MAX_PATH);	// Include '\0'
				}
				pDriver[i]->szFullNameW = pszFullNameW[i];
#endif
			}
		}

}

static void BurnGameListExit()
{
	// Release of storage space
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		if ((NULL != pszShortName) && (NULL != pszShortName[i])) free(pszShortName[i]);
		if ((NULL != pszFullNameA) && (NULL != pszFullNameA[i])) free(pszFullNameA[i]);
		if ((NULL != pszFullNameW) && (NULL != pszFullNameW[i])) free(pszFullNameW[i]);
	}
	if (NULL != pszShortName) free(pszShortName);
	if (NULL != pszFullNameA) free(pszFullNameA);
	if (NULL != pszFullNameW) free(pszFullNameW);
}

extern "C" INT32 BurnLibInit()
{
	BurnLibExit();

	nBurnDrvCount = sizeof(pDriver) / sizeof(pDriver[0]);	// count available drivers

	BurnGameListInit();

	BurnSoundInit();

	bBurnUseMMX = BurnCheckMMXSupport();

	return 0;
}

extern "C" INT32 BurnLibExit()
{
	BurnGameListExit();

	nBurnDrvCount = 0;

	return 0;
}

INT32 BurnGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = pDriver[nBurnDrvActive]->szShortName;
	} else {
		INT32 nOldBurnDrvSelect = nBurnDrvActive;
		UINT32 j = pDriver[nBurnDrvActive]->szBoardROM ? 1 : 0;

		// Try BIOS/board ROMs first
		if (i == 1 && j == 1) {										// There is a BIOS/board ROM
			pszGameName = pDriver[nBurnDrvActive]->szBoardROM;
		}

		if (pszGameName == NULL) {
			// Go through the list to seek out the parent
			while (j < i) {
				char* pszParent = pDriver[nBurnDrvActive]->szParent;
				pszGameName = NULL;

				if (pszParent == NULL) {							// No parent
					break;
				}

				for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
		            if (strcmp(pszParent, pDriver[nBurnDrvActive]->szShortName) == 0) {	// Found parent
						pszGameName = pDriver[nBurnDrvActive]->szShortName;
						break;
					}
				}

				j++;
			}
		}

		nBurnDrvActive = nOldBurnDrvSelect;
	}

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}

	strcpy(szFilename, pszGameName);

	*pszName = szFilename;

	return 0;
}

// ----------------------------------------------------------------------------
// Static functions which forward to each driver's data and functions

INT32 BurnStateMAMEScan(INT32 nAction, INT32* pnMin);
void BurnStateExit();
INT32 BurnStateInit();

// Get the text fields for the driver in TCHARs
extern "C" TCHAR* BurnDrvGetText(UINT32 i)
{
	char* pszStringA = NULL;
	wchar_t* pszStringW = NULL;
	static char* pszCurrentNameA;
	static wchar_t* pszCurrentNameW;

#if defined (_UNICODE)

	static wchar_t szShortNameW[32];
	static wchar_t szDateW[32];
	static wchar_t szFullNameW[256];
	static wchar_t szCommentW[256];
	static wchar_t szManufacturerW[256];
	static wchar_t szSystemW[256];
	static wchar_t szParentW[32];
	static wchar_t szBoardROMW[32];
	static wchar_t szSampleNameW[32];

#else

	static char szShortNameA[32];
	static char szDateA[32];
	static char szFullNameA[256];
	static char szCommentA[256];
	static char szManufacturerA[256];
	static char szSystemA[256];
	static char szParentA[32];
	static char szBoardROMA[32];
	static char szSampleNameA[32];

#endif

	if (!(i & DRV_ASCIIONLY)) {
		switch (i & 0xFF) {
#if !defined(__LIBRETRO__) && !defined(BUILD_SDL) && !defined(BUILD_SDL2) && !defined(BUILD_MACOS)
			case DRV_FULLNAME:
				pszStringW = pDriver[nBurnDrvActive]->szFullNameW;

				if (i & DRV_NEXTNAME) {
					if (pszCurrentNameW && pDriver[nBurnDrvActive]->szFullNameW) {
						pszCurrentNameW += wcslen(pszCurrentNameW) + 1;
						if (!pszCurrentNameW[0]) {
							return NULL;
						}
						pszStringW = pszCurrentNameW;
					}
				} else {

#if !defined (_UNICODE)

					// Ensure all of the Unicode titles are printable in the current locale
					pszCurrentNameW = pDriver[nBurnDrvActive]->szFullNameW;
					if (pszCurrentNameW && pszCurrentNameW[0]) {
						INT32 nRet;

						do {
							nRet = wcstombs(szFullNameA, pszCurrentNameW, 256);
							pszCurrentNameW += wcslen(pszCurrentNameW) + 1;
						} while	(nRet >= 0 && pszCurrentNameW[0]);

						// If all titles can be printed, we can use the Unicode versions
						if (nRet >= 0) {
							pszStringW = pszCurrentNameW = pDriver[nBurnDrvActive]->szFullNameW;
						}
					}

#else

					pszStringW = pszCurrentNameW = pDriver[nBurnDrvActive]->szFullNameW;

#endif

				}
				break;
#endif // !defined(__LIBRETRO__) && !defined(BUILD_SDL)
			case DRV_COMMENT:
				pszStringW = pDriver[nBurnDrvActive]->szCommentW;
				break;
			case DRV_MANUFACTURER:
				pszStringW = pDriver[nBurnDrvActive]->szManufacturerW;
				break;
			case DRV_SYSTEM:
				pszStringW = pDriver[nBurnDrvActive]->szSystemW;
		}

#if defined (_UNICODE)

		if (pszStringW && pszStringW[0]) {
			return pszStringW;
		}

#else

		switch (i & 0xFF) {
			case DRV_NAME:
				pszStringA = szShortNameA;
				break;
			case DRV_DATE:
				pszStringA = szDateA;
				break;
			case DRV_FULLNAME:
				pszStringA = szFullNameA;
				break;
			case DRV_COMMENT:
				pszStringA = szCommentA;
				break;
			case DRV_MANUFACTURER:
				pszStringA = szManufacturerA;
				break;
			case DRV_SYSTEM:
				pszStringA = szSystemA;
				break;
			case DRV_PARENT:
				pszStringA = szParentA;
				break;
			case DRV_BOARDROM:
				pszStringA = szBoardROMA;
				break;
			case DRV_SAMPLENAME:
				pszStringA = szSampleNameA;
				break;
		}

		if (pszStringW && pszStringA && pszStringW[0]) {
			if (wcstombs(pszStringA, pszStringW, 256) != -1U) {
				return pszStringA;
			}

		}

		pszStringA = NULL;

#endif

	}

	if (i & DRV_UNICODEONLY) {
		return NULL;
	}

	switch (i & 0xFF) {
		case DRV_NAME:
			pszStringA = pDriver[nBurnDrvActive]->szShortName;
			break;
		case DRV_DATE:
			pszStringA = pDriver[nBurnDrvActive]->szDate;
			break;
		case DRV_FULLNAME:
			pszStringA = pDriver[nBurnDrvActive]->szFullNameA;

			if (i & DRV_NEXTNAME) {
				if (!pszCurrentNameW && pDriver[nBurnDrvActive]->szFullNameA) {
					pszCurrentNameA += strlen(pszCurrentNameA) + 1;
					if (!pszCurrentNameA[0]) {
						return NULL;
					}
					pszStringA = pszCurrentNameA;
				}
			} else {
				pszStringA = pszCurrentNameA = pDriver[nBurnDrvActive]->szFullNameA;
				pszCurrentNameW = NULL;
			}
			break;
		case DRV_COMMENT:
			pszStringA = pDriver[nBurnDrvActive]->szCommentA;
			break;
		case DRV_MANUFACTURER:
			pszStringA = pDriver[nBurnDrvActive]->szManufacturerA;
			break;
		case DRV_SYSTEM:
			pszStringA = pDriver[nBurnDrvActive]->szSystemA;
			break;
		case DRV_PARENT:
			pszStringA = pDriver[nBurnDrvActive]->szParent;
			break;
		case DRV_BOARDROM:
			pszStringA = pDriver[nBurnDrvActive]->szBoardROM;
			break;
		case DRV_SAMPLENAME:
			pszStringA = pDriver[nBurnDrvActive]->szSampleName;
	}

#if defined (_UNICODE)

	switch (i & 0xFF) {
		case DRV_NAME:
			pszStringW = szShortNameW;
			break;
		case DRV_DATE:
			pszStringW = szDateW;
			break;
		case DRV_FULLNAME:
			pszStringW = szFullNameW;
			break;
		case DRV_COMMENT:
			pszStringW = szCommentW;
			break;
		case DRV_MANUFACTURER:
			pszStringW = szManufacturerW;
			break;
		case DRV_SYSTEM:
			pszStringW = szSystemW;
			break;
		case DRV_PARENT:
			pszStringW = szParentW;
			break;
		case DRV_BOARDROM:
			pszStringW = szBoardROMW;
			break;
		case DRV_SAMPLENAME:
			pszStringW = szSampleNameW;
			break;
	}

	if (pszStringW && pszStringA && pszStringA[0]) {
#ifdef BUILD_WIN32
		// CP_UTF8 65001
		const int n = _MultiByteToWideChar(65001, 0, pszStringA, -1, NULL, 0);
		if (0 == n)
			return NULL;

		if (0 == _MultiByteToWideChar(65001, 0, pszStringA, -1, pszStringW, n))
			return NULL;
#else
		if (mbstowcs(pszStringW, pszStringA, 256) == -1U) {
			return NULL;
		}
#endif
		return pszStringW;
	}

#else

	if (pszStringA && pszStringA[0]) {
		return pszStringA;
	}

#endif

	return NULL;
}


// Get the ASCII text fields for the driver in ASCII format;
extern "C" char* BurnDrvGetTextA(UINT32 i)
{
	switch (i) {
		case DRV_NAME:
			return pDriver[nBurnDrvActive]->szShortName;
		case DRV_DATE:
			return pDriver[nBurnDrvActive]->szDate;
		case DRV_FULLNAME:
			return pDriver[nBurnDrvActive]->szFullNameA;
		case DRV_COMMENT:
			return pDriver[nBurnDrvActive]->szCommentA;
		case DRV_MANUFACTURER:
			return pDriver[nBurnDrvActive]->szManufacturerA;
		case DRV_SYSTEM:
			return pDriver[nBurnDrvActive]->szSystemA;
		case DRV_PARENT:
			return pDriver[nBurnDrvActive]->szParent;
		case DRV_BOARDROM:
			return pDriver[nBurnDrvActive]->szBoardROM;
		case DRV_SAMPLENAME:
			return pDriver[nBurnDrvActive]->szSampleName;
		default:
			return NULL;
	}
}

static INT32 BurnDrvSetFullNameA(char* szName, UINT32 i = nBurnDrvActive)
{
	// Preventing the emergence of ~0U
	// If not NULL, then FullNameA is customized
	if ((i >= 0) && (NULL != szName)) {
		memset(pszFullNameA[i], '\0', MAX_PATH * sizeof(char));
		strcpy(pszFullNameA[i], szName);

		return 0;
	}

	return -1;
}

INT32 BurnDrvSetFullNameW(TCHAR* szName, INT32 i = nBurnDrvActive)
{
	if ((-1 == i) || (NULL == szName)) return -1;

#if defined (_UNICODE)
	memset(pszFullNameW[i], '\0', MAX_PATH * sizeof(wchar_t));
	wcscpy(pszFullNameW[i], szName);
#endif

	return 0;
}

#if defined (_UNICODE)
void BurnLocalisationSetName(char* szName, TCHAR* szLongName)
{
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (!strcmp(szName, pDriver[i]->szShortName)) {
//			pDriver[i]->szFullNameW = szLongName;
			memset(pszFullNameW[i], '\0', MAX_PATH * sizeof(wchar_t));
			_tcscpy(pszFullNameW[i], szLongName);
		}
	}
}
#endif

static void BurnLocalisationSetNameEx()
{
	if (-1 == nBurnDrvSubActive) return;

	memset(szBackupNameA, '\0', sizeof(szBackupNameA));
	strcpy(szBackupNameA, BurnDrvGetTextA(DRV_FULLNAME));
	BurnDrvSetFullNameA(pszCustomNameA);

#if defined (_UNICODE)

	const TCHAR* _str1 = _T(""), * _str2 = BurnDrvGetFullNameW(nBurnDrvActive);

	if (0 != _tcscmp(_str1, _str2)) {
		memset(szBackupNameW, _T('\0'), sizeof(szBackupNameW));
		_tcscpy(szBackupNameW, _str2);
	}

	char szShortNames[100] = { '\0'};

	sprintf(szShortNames, "%s[0x%02x]", pDriver[nBurnDrvActive]->szShortName, nBurnDrvSubActive);

	for (INT32 nIndex = 0; nIndex < nNamesExArray; nIndex++) {
		if (0 == strcmp(szShortNamesExArray[nIndex], szShortNames)) {
			BurnDrvSetFullNameW(szLongNamesExArray[nIndex]);
			return;
		}
	}
#endif
}

extern "C" INT32 BurnDrvGetIndex(char* szName)
{
	if (NULL == szName) return -1;

	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		if (0 == strcmp(szName, pDriver[i]->szShortName)) {
//			nBurnDrvActive = i;
			return i;
		}
	}

	return -1;
}

extern "C" wchar_t* BurnDrvGetFullNameW(UINT32 i)
{
	return pDriver[i]->szFullNameW;
}

// Get the zip names for the driver
extern "C" INT32 BurnDrvGetZipName(char** pszName, UINT32 i)
{
	if (pDriver[nBurnDrvActive]->GetZipName) {									// Forward to drivers function
		return pDriver[nBurnDrvActive]->GetZipName(pszName, i);
	}

	return BurnGetZipName(pszName, i);											// Forward to general function
}

extern "C" INT32 BurnDrvSetZipName(char* szName, INT32 i)
{
	if ((NULL == szName) || (-1 == i)) return -1;

	strcpy(pszShortName[i], szName);

	return 0;
}

extern "C" INT32 BurnDrvGetRomInfo(struct BurnRomInfo* pri, UINT32 i)		// Forward to drivers function
{
	return pDriver[nBurnDrvActive]->GetRomInfo(pri, i);
}

extern "C" INT32 BurnDrvGetRomName(char** pszName, UINT32 i, INT32 nAka)		// Forward to drivers function
{
	return pDriver[nBurnDrvActive]->GetRomName(pszName, i, nAka);
}

extern "C" INT32 BurnDrvGetInputInfo(struct BurnInputInfo* pii, UINT32 i)	// Forward to drivers function
{
	return pDriver[nBurnDrvActive]->GetInputInfo(pii, i);
}

extern "C" INT32 BurnDrvGetDIPInfo(struct BurnDIPInfo* pdi, UINT32 i)
{
	if (pDriver[nBurnDrvActive]->GetDIPInfo) {									// Forward to drivers function
		return pDriver[nBurnDrvActive]->GetDIPInfo(pdi, i);
	}

	return 1;																	// Fail automatically
}

extern "C" INT32 BurnDrvGetSampleInfo(struct BurnSampleInfo* pri, UINT32 i)		// Forward to drivers function
{
	return pDriver[nBurnDrvActive]->GetSampleInfo(pri, i);
}

extern "C" INT32 BurnDrvGetSampleName(char** pszName, UINT32 i, INT32 nAka)		// Forward to drivers function
{
	return pDriver[nBurnDrvActive]->GetSampleName(pszName, i, nAka);
}

extern "C" INT32 BurnDrvGetHDDInfo(struct BurnHDDInfo* pri, UINT32 i)		// Forward to drivers function
{
	if (pDriver[nBurnDrvActive]->GetHDDInfo) {
		return pDriver[nBurnDrvActive]->GetHDDInfo(pri, i);
	} else {
		return 0;
	}
}

extern "C" INT32 BurnDrvGetHDDName(char** pszName, UINT32 i, INT32 nAka)		// Forward to drivers function
{
	if (pDriver[nBurnDrvActive]->GetHDDName) {
		return pDriver[nBurnDrvActive]->GetHDDName(pszName, i, nAka);
	} else {
		return 0;
	}
}

// Get the screen size
extern "C" INT32 BurnDrvGetVisibleSize(INT32* pnWidth, INT32* pnHeight)
{
	*pnWidth =pDriver[nBurnDrvActive]->nWidth;
	*pnHeight=pDriver[nBurnDrvActive]->nHeight;

	return 0;
}

extern "C" INT32 BurnDrvGetVisibleOffs(INT32* pnLeft, INT32* pnTop)
{
	*pnLeft = 0;
	*pnTop = 0;

	return 0;
}

extern "C" INT32 BurnDrvGetFullSize(INT32* pnWidth, INT32* pnHeight)
{
	if (pDriver[nBurnDrvActive]->Flags & BDF_ORIENTATION_VERTICAL) {
		*pnWidth =pDriver[nBurnDrvActive]->nHeight;
		*pnHeight=pDriver[nBurnDrvActive]->nWidth;
	} else {
		*pnWidth =pDriver[nBurnDrvActive]->nWidth;
		*pnHeight=pDriver[nBurnDrvActive]->nHeight;
	}

	return 0;
}

// Get screen aspect ratio
extern "C" INT32 BurnDrvGetAspect(INT32* pnXAspect, INT32* pnYAspect)
{
	*pnXAspect = pDriver[nBurnDrvActive]->nXAspect;
	*pnYAspect = pDriver[nBurnDrvActive]->nYAspect;

	return 0;
}

extern "C" INT32 BurnDrvSetVisibleSize(INT32 pnWidth, INT32 pnHeight)
{
	if (pDriver[nBurnDrvActive]->Flags & BDF_ORIENTATION_VERTICAL) {
		pDriver[nBurnDrvActive]->nHeight = pnWidth;
		pDriver[nBurnDrvActive]->nWidth = pnHeight;
	} else {
		pDriver[nBurnDrvActive]->nWidth = pnWidth;
		pDriver[nBurnDrvActive]->nHeight = pnHeight;
	}

	return 0;
}

extern "C" INT32 BurnDrvSetAspect(INT32 pnXAspect,INT32 pnYAspect)
{
	pDriver[nBurnDrvActive]->nXAspect = pnXAspect;
	pDriver[nBurnDrvActive]->nYAspect = pnYAspect;

	return 0;
}

// Get the hardware code
extern "C" INT32 BurnDrvGetHardwareCode()
{
	return pDriver[nBurnDrvActive]->Hardware;
}

// Get flags, including BDF_GAME_WORKING flag
extern "C" INT32 BurnDrvGetFlags()
{
	return pDriver[nBurnDrvActive]->Flags;
}

// Return BDF_WORKING flag
extern "C" bool BurnDrvIsWorking()
{
	return pDriver[nBurnDrvActive]->Flags & BDF_GAME_WORKING;
}

// Return max. number of players
extern "C" INT32 BurnDrvGetMaxPlayers()
{
	return pDriver[nBurnDrvActive]->Players;
}

// Return genre flags
extern "C" INT32 BurnDrvGetGenreFlags()
{
	return pDriver[nBurnDrvActive]->Genre;
}

// Return family flags
extern "C" INT32 BurnDrvGetFamilyFlags()
{
	return pDriver[nBurnDrvActive]->Family;
}

// Return sourcefile
extern "C" char* BurnDrvGetSourcefile()
{
	char* szShortName = pDriver[nBurnDrvActive]->szShortName;
	for (INT32 i = 0; sourcefile_table[i].game_name[0] != '\0'; i++) {
		if (!strcmp(sourcefile_table[i].game_name, szShortName)) {
			return sourcefile_table[i].sourcefile;
		}
	}
	return "";
}

// Save Aspect & Screensize in BurnDrvInit(), restore in BurnDrvExit()
// .. as games may need to change modes, etc.
static INT32 DrvAspectX, DrvAspectY;
static INT32 DrvX, DrvY;
static INT32 DrvCached = 0;

// Game's original resolution
// Some games change the resolution by modifying the original size with
// BurnDrvSetVisibleSize() then running Reinitialise() or ReinitialiseVideo()
extern "C" INT32 BurnDrvGetOriginalVisibleSize(INT32* pnWidth, INT32* pnHeight)
{
	*pnWidth  = DrvX;
	*pnHeight = DrvY;

	return 0;
}

static void BurnCacheSizeAspect_Internal()
{
	BurnDrvGetFullSize(&DrvX, &DrvY);
	BurnDrvGetAspect(&DrvAspectX, &DrvAspectY);
	DrvCached = 1;
}

static void BurnRestoreSizeAspect_Internal()
{
	if (DrvCached) {
		BurnDrvSetVisibleSize(DrvX, DrvY);
		BurnDrvSetAspect(DrvAspectX, DrvAspectY);
		DrvCached = 0;
	}
}

// Init game emulation (loading any needed roms)
extern "C" INT32 BurnDrvInit()
{
	INT32 nReturnValue;

	if (nBurnDrvActive >= nBurnDrvCount) {
		return 1;
	}

#if defined (FBNEO_DEBUG)
	{
		TCHAR szText[1024] = _T("");
		TCHAR* pszPosition = szText;
		TCHAR* pszName = BurnDrvGetText(DRV_FULLNAME);
		INT32 nName = 1;

		while ((pszName = BurnDrvGetText(DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
			nName++;
		}

		// Print the title

		bprintf(PRINT_IMPORTANT, _T("*** Starting emulation of %s - %s.\n"), BurnDrvGetText(DRV_NAME), BurnDrvGetText(DRV_FULLNAME));

#ifdef BUILD_A68K
		if (bBurnUseASMCPUEmulation)
			bprintf(PRINT_ERROR, _T("*** WARNING: Assembly MC68000 core is enabled for this session!\n"));
#endif

		// Then print the alternative titles

		if (nName > 1) {
			bprintf(PRINT_IMPORTANT, _T("    Alternative %s "), (nName > 2) ? _T("titles are") : _T("title is"));
			pszName = BurnDrvGetText(DRV_FULLNAME);
			nName = 1;
			while ((pszName = BurnDrvGetText(DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
				if (pszPosition + _tcslen(pszName) - 1022 > szText) {
					break;
				}
				if (nName > 1) {
					bprintf(PRINT_IMPORTANT, _T(SEPERATOR_1));
				}
				bprintf(PRINT_IMPORTANT, _T("%s"), pszName);
				nName++;
			}
			bprintf(PRINT_IMPORTANT, _T(".\n"));
		}
	}
#endif

	BurnSetMouseDivider(1);

	BurnSetRefreshRate(60.0);

	BurnCacheSizeAspect_Internal();

	CheatInit();
	HiscoreInit();
	BurnStateInit();
	BurnInitMemoryManager();
	BurnRandomInit();
	BurnSoundDCFilterReset();
	BurnTimerPreInit();

	nReturnValue = pDriver[nBurnDrvActive]->Init();	// Forward to drivers function

	if (-1 != nBurnDrvSubActive) {
		BurnLocalisationSetNameEx();
	}

	nMaxPlayers = pDriver[nBurnDrvActive]->Players;

	nCurrentFrame = 0;

#if defined (FBNEO_DEBUG)
	if (!nReturnValue) {
		starttime = clock();
		nFramesEmulated = 0;
		nFramesRendered = 0;
	} else {
		starttime = 0;
	}
#endif

	return nReturnValue;
}

// Exit game emulation
extern "C" INT32 BurnDrvExit()
{
#if defined (FBNEO_DEBUG)
	if (starttime) {
		clock_t endtime;
		clock_t nElapsedSecs;

		endtime = clock();
		nElapsedSecs = (endtime - starttime);
		bprintf(PRINT_IMPORTANT, _T(" ** Emulation ended (running for %.2f seconds).\n"), (float)nElapsedSecs / CLOCKS_PER_SEC);
		bprintf(PRINT_IMPORTANT, _T("    %.2f%% of frames rendered (%d out of a total %d).\n"), (float)nFramesRendered / nFramesEmulated * 100, nFramesRendered, nFramesEmulated);
		bprintf(PRINT_IMPORTANT, _T("    %.2f frames per second (average).\n"), (float)nFramesRendered / nFramesEmulated * nBurnFPS / 100);
		bprintf(PRINT_NORMAL, _T("\n"));
	}
#endif

	HiscoreExit(); // must come before CheatExit() (uses cheat cpu-registry)
	CheatExit();
	CheatSearchExit();
	BurnStateExit();

	nBurnCPUSpeedAdjust = 0x0100;

	pBurnDrvPalette = NULL;

	if (-1 != nBurnDrvSubActive) {
		pszCustomNameA = szBackupNameA;
		BurnDrvSetFullNameA(szBackupNameA);
		pszCustomNameA = NULL;

#if defined (_UNICODE)
		const wchar_t* _str1 = L"", * _str2 = BurnDrvGetFullNameW(nBurnDrvActive);

		if (0 != _tcscmp(_str1, _str2)) {
			BurnDrvSetFullNameW(szBackupNameW);
		}
#endif
	}

	INT32 nRet = pDriver[nBurnDrvActive]->Exit();			// Forward to drivers function

	nBurnDrvSubActive = -1;	// Rest to -1;

	BurnExitMemoryManager();
#if defined FBNEO_DEBUG
	DebugTrackerExit();
#endif

	BurnRestoreSizeAspect_Internal();

	return nRet;
}

INT32 (__cdecl* BurnExtCartridgeSetupCallback)(BurnCartrigeCommand nCommand) = NULL;

INT32 BurnDrvCartridgeSetup(BurnCartrigeCommand nCommand)
{
	if (nBurnDrvActive >= nBurnDrvCount || BurnExtCartridgeSetupCallback == NULL) {
		return 1;
	}

	if (nCommand == CART_EXIT) {
		return pDriver[nBurnDrvActive]->Exit();
	}

	if (nCommand != CART_INIT_END && nCommand != CART_INIT_START) {
		return 1;
	}

	BurnExtCartridgeSetupCallback(CART_INIT_END);

#if defined FBNEO_DEBUG
		bprintf(PRINT_NORMAL, _T("  * Loading Cartridge\n"));
#endif

	if (BurnExtCartridgeSetupCallback(CART_INIT_START)) {
		return 1;
	}

	if (nCommand == CART_INIT_START) {
		return pDriver[nBurnDrvActive]->Init();
	}

	return 0;
}

// Do one frame of game emulation
extern "C" INT32 BurnDrvFrame()
{
	CheatApply();									// Apply cheats (if any)
	HiscoreApply();
	return pDriver[nBurnDrvActive]->Frame();		// Forward to drivers function
}

// Force redraw of the screen
extern "C" INT32 BurnDrvRedraw()
{
	if (pDriver[nBurnDrvActive]->Redraw) {
		return pDriver[nBurnDrvActive]->Redraw();	// Forward to drivers function
	}

	return 1;										// No funtion provide, so simply return
}

// Refresh Palette
extern "C" INT32 BurnRecalcPal()
{
	if (nBurnDrvActive < nBurnDrvCount) {
		UINT8* pr = pDriver[nBurnDrvActive]->pRecalcPal;
		if (pr == NULL) return 1;
		*pr = 1;									// Signal for the driver to refresh it's palette
	}

	return 0;
}

extern "C" INT32 BurnDrvGetPaletteEntries()
{
	return pDriver[nBurnDrvActive]->nPaletteEntries;
}

// ----------------------------------------------------------------------------

INT32 (__cdecl *BurnExtProgressRangeCallback)(double fProgressRange) = NULL;
INT32 (__cdecl *BurnExtProgressUpdateCallback)(double fProgress, const TCHAR* pszText, bool bAbs) = NULL;

INT32 BurnSetProgressRange(double fProgressRange)
{
	if (BurnExtProgressRangeCallback) {
		return BurnExtProgressRangeCallback(fProgressRange);
	}

	return 1;
}

INT32 BurnUpdateProgress(double fProgress, const TCHAR* pszText, bool bAbs)
{
	if (BurnExtProgressUpdateCallback) {
		return BurnExtProgressUpdateCallback(fProgress, pszText, bAbs);
	}

	return 1;
}

// ----------------------------------------------------------------------------

void (__cdecl *BurnResizeCallback)(INT32 width, INT32 height) = NULL;

void BurnSetResolution(INT32 width, INT32 height)
{
	if (BurnResizeCallback) BurnResizeCallback(width, height);
}

// ----------------------------------------------------------------------------
// NOTE: Make sure this is called before any soundcore init!
INT32 BurnSetRefreshRate(double dFrameRate)
{
	if (bSpeedLimit60hz && dFrameRate > 60.00)
		dFrameRate = 60.00;

	if (bForce60Hz && dFrameRate > 50.00) {
		// Force 60hz w/ games that are near 60hz & avoid breaking
		// vector (30-42hz), 30hz Midway, NES/MSX/Spectrum 50hz PAL mode.
		dFrameRate = dForcedFrameRate;
	}

	nBurnFPS = (INT32)(100.0 * dFrameRate);
#ifdef __LIBRETRO__
	// By design, libretro dislike having nBurnSoundRate > nBurnFPS * 10
	if (nBurnSoundRate > nBurnFPS * 10)
		nBurnSoundRate = nBurnFPS * 10;
#endif

	nBurnSoundLen = (nBurnSoundRate * 100 + (nBurnFPS >> 1)) / nBurnFPS;

	return 0;
}

// Bring the mouse x/y delta's down to a usable rate for, ex. Trackball emulation
void BurnSetMouseDivider(INT32 nDivider)
{
	if (nDivider == 0) {

		nDivider = 1;
	}

	nInputIntfMouseDivider = nDivider;

	//bprintf(0, _T("BurnSetMouseDivider() @ %d\n"), nDivider);
}

inline static INT32 BurnClearSize(INT32 w, INT32 h)
{
	UINT8 *pl;
	INT32 y;

	w *= nBurnBpp;

	// clear the screen to zero
	for (pl = pBurnDraw, y = 0; y < h; pl += nBurnPitch, y++) {
		memset(pl, 0x00, w);
	}

	return 0;
}

INT32 BurnClearScreen()
{
	struct BurnDriver* pbd = pDriver[nBurnDrvActive];

	if (pbd->Flags & BDF_ORIENTATION_VERTICAL) {
		BurnClearSize(pbd->nHeight, pbd->nWidth);
	} else {
		BurnClearSize(pbd->nWidth, pbd->nHeight);
	}

	return 0;
}

// Byteswaps an area of memory
INT32 BurnByteswap(UINT8* pMem, INT32 nLen)
{
	nLen >>= 1;
	for (INT32 i = 0; i < nLen; i++, pMem += 2) {
		UINT8 t = pMem[0];
		pMem[0] = pMem[1];
		pMem[1] = t;
	}

	return 0;
}

// useful for expanding 4bpp pixels packed into one byte using little to-no
// extra memory.
// use 'swap' (1) to swap whether the high or low nibble goes into byte 0 or 1
// 'nxor' is useful for inverting the data
// this is an example of a graphics decode that can be converted to use this
// function:
//	INT32 Plane[4] = { STEP4(0,1) }; - 0,1,2,3
//	INT32 XOffs[8] = { STEP8(0,4) }; - 0,4,8,12,16,20,24,28 (swap is useful for when this is 4,0,12,8...)
//	INT32 YOffs[8] = { STEP8(0,32) }; - 0, 32, 64, 96, 128, 160, 192, 224
//
void BurnNibbleExpand(UINT8 *source, UINT8 *dst, INT32 length, INT32 swap, UINT8 nxor)
{
	if (source == NULL) {
		bprintf (0, _T("BurnNibbleExpand() source passed as NULL!\n"));
		return;
	}

	if (length <= 0) {
		bprintf (0, _T("BurnNibbleExpand() length passed as <= 0 (%d)!\n"), length);
		return;
	}

	int swap_src = (swap & 2) >> 1; // swap src
	swap &= 1; // swap nibble

	if (dst == NULL) dst = source;

	for (INT32 i = length - 1; i >= 0; i--)
	{
		INT32 t = source[i ^ swap_src] ^ nxor;
		dst[(i * 2 + 0) ^ swap] = t >> 4;
		dst[(i * 2 + 1) ^ swap] = t & 0xf;
	}
}

// Application-defined rom loading function:
INT32 (__cdecl *BurnExtLoadRom)(UINT8 *Dest, INT32 *pnWrote, INT32 i) = NULL;

// Application-defined colour conversion function
static UINT32 __cdecl BurnHighColFiller(INT32, INT32, INT32, INT32) { return (UINT32)(~0); }
UINT32 (__cdecl *BurnHighCol) (INT32 r, INT32 g, INT32 b, INT32 i) = BurnHighColFiller;

// ----------------------------------------------------------------------------
// Savestate support

// Application-defined callback for processing the area
static INT32 __cdecl DefAcb (struct BurnArea* /* pba */) { return 1; }
INT32 (__cdecl *BurnAcb) (struct BurnArea* pba) = DefAcb;

// Scan driver data
INT32 BurnAreaScan(INT32 nAction, INT32* pnMin)
{
	INT32 nRet = 0;

	// Handle any MAME-style variables
	if (nAction & ACB_DRIVER_DATA) {
		nRet = BurnStateMAMEScan(nAction, pnMin);
	}

	// Forward to the driver
	if (pDriver[nBurnDrvActive]->AreaScan) {
		nRet |= pDriver[nBurnDrvActive]->AreaScan(nAction, pnMin);
	}

	return nRet;
}

// --------- State-ing for RunAhead ----------
// for drivers, hiscore, etc, to recognize that this is the "runahead frame"
INT32 bBurnRunAheadFrame = 0;

// ----------------------------------------------------------------------------
// Get the local time - make tweaks if netgame or input recording/playback
// tweaks are needed for the game to to remain in-sync! (pgm, neogeo, etc)
struct MovieExtInfo
{
	// date & time
	UINT32 year, month;
	UINT16 day, dayofweek;
	UINT32 hour, minute, second;
};

#if !defined(BUILD_SDL) && !defined(BUILD_SDL2) && !defined(BUILD_MACOS)
extern struct MovieExtInfo MovieInfo; // from replay.cpp
#else
struct MovieExtInfo MovieInfo = { 0, 0, 0, 0, 0, 0, 0 };
#endif

void BurnGetLocalTime(tm *nTime)
{
	if (is_netgame_or_recording()) {
		if (is_netgame_or_recording() & 2) { // recording/playback
			nTime->tm_sec = MovieInfo.second;
			nTime->tm_min = MovieInfo.minute;
			nTime->tm_hour = MovieInfo.hour;
			nTime->tm_mday = MovieInfo.day;
			nTime->tm_wday = MovieInfo.dayofweek;
			nTime->tm_mon = MovieInfo.month;
			nTime->tm_year = MovieInfo.year;
		} else {
			nTime->tm_sec = 0; // defaults for netgame
			nTime->tm_min = 0;
			nTime->tm_hour = 0;
			nTime->tm_mday = 1;
			nTime->tm_wday = 3;
			nTime->tm_mon = 6 - 1;
			nTime->tm_year = 2018;
		}
	} else {
		time_t nLocalTime = time(NULL); // query current time from this machine
		tm* tmLocalTime = localtime(&nLocalTime);
		memcpy(nTime, tmLocalTime, sizeof(tm));
	}
}


// ----------------------------------------------------------------------------
// State-able random generator, based on early BSD LCG rand
static UINT64 nBurnRandSeed = 0;

UINT16 BurnRandom()
{
	nBurnRandSeed = nBurnRandSeed * 1103515245 + 12345;

	return (UINT32)(nBurnRandSeed / 65536) % 0x10000;
}

void BurnRandomScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(nBurnRandSeed);
	}
}

void BurnRandomSetSeed(UINT64 nSeed)
{
	nBurnRandSeed = nSeed;
}

void BurnRandomInit()
{ // for states & input recordings - init before emulation starts
	if (is_netgame_or_recording()) {
		BurnRandomSetSeed(0x303808909313ULL);
	} else {
		BurnRandomSetSeed(time(NULL));
	}
}

// ----------------------------------------------------------------------------
// Handy FM default callbacks

INT32 BurnSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)BurnTimerCPUTotalCycles() * nSoundRate / BurnTimerCPUClockspeed;
}

double BurnGetTime()
{
	return (double)BurnTimerCPUTotalCycles() / BurnTimerCPUClockspeed;
}

// CPU Speed adjuster
INT32 BurnSpeedAdjust(INT32 cyc)
{
	return (INT32)((INT64)cyc * nBurnCPUSpeedAdjust / 0x0100);
}

// ----------------------------------------------------------------------------
// Wrappers for MAME-specific function calls

// #include "driver.h"

struct BurnStateEntry { BurnStateEntry* pNext; BurnStateEntry* pPrev; char szName[256]; void* pValue; UINT32 nSize; };

static BurnStateEntry* pStateEntryAnchor = NULL;
typedef void (*BurnPostloadFunction)();
static BurnPostloadFunction BurnPostload[8];

void BurnStateExit()
{
	if (pStateEntryAnchor) {
		BurnStateEntry* pCurrentEntry = pStateEntryAnchor;
		BurnStateEntry* pNextEntry;

		do {
			pNextEntry = pCurrentEntry->pNext;
			BurnFree(pCurrentEntry);
		} while ((pCurrentEntry = pNextEntry) != 0);
	}

	pStateEntryAnchor = NULL;

	for (INT32 i = 0; i < 8; i++) {
		BurnPostload[i] = NULL;
	}
}

INT32 BurnStateInit()
{
	BurnStateExit();

	return 0;
}

INT32 BurnStateMAMEScan(INT32 nAction, INT32* pnMin)
{
	if (nAction & ACB_VOLATILE) {

		if (pnMin && *pnMin < 0x029418) {						// Return minimum compatible version
			*pnMin = 0x029418;
		}

		if (pStateEntryAnchor) {
			struct BurnArea ba;
			BurnStateEntry* pCurrentEntry = pStateEntryAnchor;

			do {
			   	ba.Data		= pCurrentEntry->pValue;
				ba.nLen		= pCurrentEntry->nSize;
				ba.nAddress = 0;
				ba.szName	= pCurrentEntry->szName;
				BurnAcb(&ba);

			} while ((pCurrentEntry = pCurrentEntry->pNext) != 0);
		}

		if (nAction & ACB_WRITE) {
			for (INT32 i = 0; i < 8; i++) {
				if (BurnPostload[i]) {
					BurnPostload[i]();
				}
			}
		}
	}

	return 0;
}
