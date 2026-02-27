// Burn - Rom Loading module
#include "burnint.h"

INT32 BurnLoadRom(UINT8 *Dest, INT32 i, INT32 nGap)
{
	INT32 nRet = 0, nLen = 0;
	struct BurnRomInfo ri;

	if (BurnExtLoadRom == NULL) return 1; // Load function was not defined by the application

	// Find the length of the rom (as given by the current driver)
	{
		ri.nType = 0;
		ri.nLen = 0;
		BurnDrvGetRomInfo(&ri,i);
		if (ri.nType == 0) return 0; // Empty rom slot - don't load anything and return success
		nLen = ri.nLen;
	}

	char* RomName = ""; //added by emufan
	BurnDrvGetRomName(&RomName, i, 0);

	if (nLen <= 0) return 1;

	if (nGap > 1)
	{
		// Use temporary memory to load ROM, ips patching is also done here, enough space must be reserved.
		if (bDoIpsPatch) {
			if (0 == nIpsMemExpLen[EXP_FLAG]) {			// Unspecified nIpsMemExpLen[LOAD_ROM].
				IpsApplyPatches(NULL, RomName, ri.nCrc, true);	// Get the maximum offset of ips.
				if (nIpsMemExpLen[LOAD_ROM] > nLen) {	// ips offset is greater than rom length.
					nLen = nIpsMemExpLen[LOAD_ROM];
				}
			} else {									// Customized nIpsMemExpLen[LOAD_ROM].
				nLen += nIpsMemExpLen[LOAD_ROM];
			}
		}

		INT32 nLoadLen = 0;
		UINT8* Load = (UINT8*)BurnMalloc(nLen);
		if (Load == NULL) return 1;
		memset(Load, 0, nLen);

		// Load in the file
		nRet = BurnExtLoadRom(Load, &nLoadLen, i);
		if (bDoIpsPatch) IpsApplyPatches(Load, RomName, ri.nCrc);
		if (nRet != 0) { if (Load) { BurnFree(Load); Load = NULL; } return 1; }

		if (nLoadLen < 0) nLoadLen = 0;
		if (nLoadLen > nLen || bDoIpsPatch) nLoadLen = nLen;

		INT32 nGroup = 1;
		UINT8 *Src = Load;

		for (INT32 n = 0, z = 0; n < nLoadLen; n += nGroup, z += nGap) {
			Dest[z] = Src[n];
		}

		if (Load) {
			BurnFree(Load);
			Load = NULL;
		}
	} else {
 		// If no XOR, and gap of 1, just copy straight in
		nRet = BurnExtLoadRom(Dest, NULL, i);
		if (bDoIpsPatch) {
			IpsApplyPatches(NULL, RomName, ri.nCrc, true);	// Get the maximum offset of ips. & megadrive needs.
			IpsApplyPatches(Dest, RomName, ri.nCrc);
		}
		if (nRet != 0) return 1;
	}

	return 0;
}
