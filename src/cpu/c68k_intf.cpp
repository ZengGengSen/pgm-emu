// 680x0 (Sixty Eight K) Interface

#include "burnint.h"
#include "m68000_intf.h"
#include "m68000_debug.h"

#include "c68k/c68k.c"

c68k_struc * SekC68KCurrentContext = NULL;
c68k_struc * SekC68KContext[SEK_MAX];

INT32 nSekCount = -1;							// Number of allocated 68000s
struct SekExt *SekExt[SEK_MAX] = { NULL, }, *pSekExt = NULL;

INT32 nSekActive = -1;								// The cpu which is currently being emulated
INT32 nSekCyclesTotal, nSekCyclesScanline, nSekCyclesSegment, nSekCyclesDone, nSekCyclesToDo;

INT32 nSekCPUType[SEK_MAX], nSekCycles[SEK_MAX], nSekIRQPending[SEK_MAX], nSekRESETLine[SEK_MAX], nSekHALT[SEK_MAX];
INT32 nSekVIRQPending[SEK_MAX][8];
INT32 nSekCyclesToDoCache[SEK_MAX], nSekm68k_ICount[SEK_MAX];
INT32 nSekCPUOffsetAddress[SEK_MAX];

static UINT32 nSekAddressMask[SEK_MAX], nSekAddressMaskActive;

cpu_core_config SekConfig =
{
	"68k",
	SekCPUPush, //SekOpen,
	SekCPUPop, //SekClose,
	SekCheatRead,
	SekWriteByteROM,
	SekGetActive,
	SekTotalCycles,
	SekNewFrame,
	SekIdle,
	SekSetIRQLine,
	SekRun,
	SekRunEnd,
	SekReset,
	SekScan,
	SekExit,
	0x1000000,
	1 // big endian
};

#if defined (FBNEO_DEBUG)

void (*SekDbgBreakpointHandlerRead)(UINT32, INT32);
void (*SekDbgBreakpointHandlerFetch)(UINT32, INT32);
void (*SekDbgBreakpointHandlerWrite)(UINT32, INT32);

UINT32 (*SekDbgFetchByteDisassembler)(UINT32);
UINT32 (*SekDbgFetchWordDisassembler)(UINT32);
UINT32 (*SekDbgFetchLongDisassembler)(UINT32);

static struct { UINT32 address; INT32 id; } BreakpointDataRead[9]  = { { 0, 0 }, };
static struct { UINT32 address; INT32 id; } BreakpointDataWrite[9] = { { 0, 0 }, };
static struct { UINT32 address; INT32 id; } BreakpointFetch[9] = { { 0, 0 }, };

#endif

#if defined (FBNEO_DEBUG)

inline static void CheckBreakpoint_R(UINT32 a, const UINT32 m)
{
	a &= m;

	for (INT32 i = 0; BreakpointDataRead[i].address; i++) {
		if ((BreakpointDataRead[i].address & m) == a) {
			SekDbgBreakpointHandlerRead(a, BreakpointDataRead[i].id);
			return;
		}
	}
}

inline static void CheckBreakpoint_W(UINT32 a, const UINT32 m)
{
	a &= m;

	for (INT32 i = 0; BreakpointDataWrite[i].address; i++) {
		if ((BreakpointDataWrite[i].address & m) == a) {
			SekDbgBreakpointHandlerWrite(a, BreakpointDataWrite[i].id);
			return;
		}
	}
}

inline static void CheckBreakpoint_PC(unsigned int /*pc*/)
{
	for (INT32 i = 0; BreakpointFetch[i].address; i++) {
		if (BreakpointFetch[i].address == (UINT32)SekGetPC(-1)) {
			SekDbgBreakpointHandlerFetch(SekGetPC(-1), BreakpointFetch[i].id);
			return;
		}
	}
}

inline static void SingleStep_PC(unsigned int /*pc*/)
{
	SekDbgBreakpointHandlerFetch(SekGetPC(-1), 0);
}

#endif

// ----------------------------------------------------------------------------
// Default memory access handlers

UINT8 __fastcall DefReadByte(UINT32) { return 0; }
void __fastcall DefWriteByte(UINT32, UINT8) { }

#define DEFWORDHANDLERS(i)																				\
	UINT16 __fastcall DefReadWord##i(UINT32 a) { SEK_DEF_READ_WORD(i, a) }				\
	void __fastcall DefWriteWord##i(UINT32 a, UINT16 d) { SEK_DEF_WRITE_WORD(i, a ,d) }
#define DEFLONGHANDLERS(i)																				\
	UINT32 __fastcall DefReadLong##i(UINT32 a) { SEK_DEF_READ_LONG(i, a) }					\
	void __fastcall DefWriteLong##i(UINT32 a, UINT32 d) { SEK_DEF_WRITE_LONG(i, a , d) }

DEFWORDHANDLERS(0)
DEFLONGHANDLERS(0)

#if SEK_MAXHANDLER >= 2
 DEFWORDHANDLERS(1)
 DEFLONGHANDLERS(1)
#endif

#if SEK_MAXHANDLER >= 3
 DEFWORDHANDLERS(2)
 DEFLONGHANDLERS(2)
#endif

#if SEK_MAXHANDLER >= 4
 DEFWORDHANDLERS(3)
 DEFLONGHANDLERS(3)
#endif

#if SEK_MAXHANDLER >= 5
 DEFWORDHANDLERS(4)
 DEFLONGHANDLERS(4)
#endif

#if SEK_MAXHANDLER >= 6
 DEFWORDHANDLERS(5)
 DEFLONGHANDLERS(5)
#endif

#if SEK_MAXHANDLER >= 7
 DEFWORDHANDLERS(6)
 DEFLONGHANDLERS(6)
#endif

#if SEK_MAXHANDLER >= 8
 DEFWORDHANDLERS(7)
 DEFLONGHANDLERS(7)
#endif

#if SEK_MAXHANDLER >= 9
 DEFWORDHANDLERS(8)
 DEFLONGHANDLERS(8)
#endif

#if SEK_MAXHANDLER >= 10
 DEFWORDHANDLERS(9)
 DEFLONGHANDLERS(9)
#endif

// ----------------------------------------------------------------------------
// Memory access functions

// Mapped Memory lookup (               for read)
#define FIND_R(x) pSekExt->MemMap[ x >> SEK_SHIFT]
// Mapped Memory lookup (+ SEK_WADD     for write)
#define FIND_W(x) pSekExt->MemMap[(x >> SEK_SHIFT) + SEK_WADD]
// Mapped Memory lookup (+ SEK_WADD * 2 for fetch)
#define FIND_F(x) pSekExt->MemMap[(x >> SEK_SHIFT) + SEK_WADD * 2]

// Normal memory access functions
inline static UINT8 ReadByte(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("read8 0x%08X\n"), a);
	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		return pr[a & SEK_PAGEM];
	}
	return pSekExt->ReadByte[(uintptr_t)pr](a);
}

inline static UINT8 FetchByte(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("fetch8 0x%08X\n"), a);

	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		return pr[a & SEK_PAGEM];
	}
	return pSekExt->ReadByte[(uintptr_t)pr](a);
}

inline static void WriteByte(UINT32 a, UINT8 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;
//	bprintf(PRINT_NORMAL, _T("write8 0x%08X\n"), a);

	pr = FIND_W(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		pr[a & SEK_PAGEM] = (UINT8)d;
		return;
	}
	pSekExt->WriteByte[(uintptr_t)pr](a, d);
}

inline static void WriteByteROM(UINT32 a, UINT8 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;
	// changed from FIND_R to allow for encrypted games (fd1094 etc) to work -dink apr. 23, 2021
	// (on non-encrypted games, Fetch is mapped to Read)
	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		pr[a & SEK_PAGEM] = (UINT8)d;
		return;
	}
	pSekExt->WriteByte[(uintptr_t)pr](a, d);
}

inline static UINT16 ReadWord(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("read16 0x%08X\n"), a);
	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		if (a & 1)
		{
			return (ReadByte(a + 0) * 256) + ReadByte(a + 1);
		}
		else
		{
			return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(pr + (a & SEK_PAGEM))));
		}
	}

	return pSekExt->ReadWord[(uintptr_t)pr](a);
}

inline static UINT16 FetchWord(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("fetch16 0x%08X\n"), a);

	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		if (a & 1)
		{
			return (ReadByte(a + 0) * 256) + ReadByte(a + 1);
		}
		else
		{
			return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(pr + (a & SEK_PAGEM))));
		}
	}

	return pSekExt->ReadWord[(uintptr_t)pr](a);
}

inline static void WriteWord(UINT32 a, UINT16 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("write16 0x%08X\n"), a);
	pr = FIND_W(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		if (a & 1)
		{
		//	bprintf(PRINT_NORMAL, _T("write16 0x%08X\n"), a);

			WriteByte(a + 0, d / 0x100);
			WriteByte(a + 1, d);

			return;
		}
		else
		{
			*((UINT16*)(pr + (a & SEK_PAGEM))) = (UINT16)BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
	}

	pSekExt->WriteWord[(uintptr_t)pr](a, d);
}

inline static void WriteWordROM(UINT32 a, UINT16 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;
	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		*((UINT16*)(pr + (a & SEK_PAGEM))) = (UINT16)d;
		return;
	}
	pSekExt->WriteWord[(uintptr_t)pr](a, d);
}

// [x] byte #
// be [3210] -> (r >> 16) | (r << 16) -> [1032] -> UINT32(le) = -> [0123]
// mem [0123]

inline static UINT32 ReadLong(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("read32 0x%08X\n"), a);
	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		UINT32 r = 0;

		if (a & nSekCPUOffsetAddress[nSekActive])
		{
			r  = ReadByte((a + 0)) * 0x1000000;
			r += ReadByte((a + 1)) * 0x10000;
			r += ReadByte((a + 2)) * 0x100;
			r += ReadByte((a + 3));

			return r;
		}
		else
		{
			r = *((UINT32*)(pr + (a & SEK_PAGEM)));
			r = (r >> 16) | (r << 16);

			return BURN_ENDIAN_SWAP_INT32(r);
		}
	}

	return pSekExt->ReadLong[(uintptr_t)pr](a);
}

inline static UINT32 FetchLong(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("fetch32 0x%08X\n"), a);

	//if (a&3) bprintf(0, _T("fetchlong offset-read @ %x\n"), a);

	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		UINT32 r = 0;

		if (a & nSekCPUOffsetAddress[nSekActive])
		{
			r  = ReadByte((a + 0)) * 0x1000000;
			r += ReadByte((a + 1)) * 0x10000;
			r += ReadByte((a + 2)) * 0x100;
			r += ReadByte((a + 3));

			return r;
		}
		else
		{
			r = *((UINT32*)(pr + (a & SEK_PAGEM)));
			r = (r >> 16) | (r << 16);

			return BURN_ENDIAN_SWAP_INT32(r);
		}
	}
	return pSekExt->ReadLong[(uintptr_t)pr](a);
}

inline static void WriteLong(UINT32 a, UINT32 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("write32 0x%08X\n"), a);
	pr = FIND_W(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		if (a & nSekCPUOffsetAddress[nSekActive])
		{
		//	bprintf(PRINT_NORMAL, _T("write32 0x%08X 0x%8.8x\n"), a,d);

			WriteByte((a + 0), d / 0x1000000);
			WriteByte((a + 1), d / 0x10000);
			WriteByte((a + 2), d / 0x100);
			WriteByte((a + 3), d);

			return;
		}
		else
		{
			d = (d >> 16) | (d << 16);
			*((UINT32*)(pr + (a & SEK_PAGEM))) = BURN_ENDIAN_SWAP_INT32(d);

			return;
		}
	}
	pSekExt->WriteLong[(uintptr_t)pr](a, d);
}

inline static void WriteLongROM(UINT32 a, UINT32 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;
	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		d = (d >> 16) | (d << 16);
		*((UINT32*)(pr + (a & SEK_PAGEM))) = d;
		return;
	}
	pSekExt->WriteLong[(uintptr_t)pr](a, d);
}

#if defined (FBNEO_DEBUG)

// Breakpoint checking memory access functions
UINT8 __fastcall ReadByteBP(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_R(a);

	CheckBreakpoint_R(a, ~0);

	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		return pr[a & SEK_PAGEM];
	}
	return pSekExt->ReadByte[(uintptr_t)pr](a);
}

void __fastcall WriteByteBP(UINT32 a, UINT8 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_W(a);

	CheckBreakpoint_W(a, ~0);

	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		pr[a & SEK_PAGEM] = (UINT8)d;
		return;
	}
	pSekExt->WriteByte[(uintptr_t)pr](a, d);
}

UINT16 __fastcall ReadWordBP(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_R(a);

	CheckBreakpoint_R(a, ~1);

	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		return *((UINT16*)(pr + (a & SEK_PAGEM)));
	}
	return pSekExt->ReadWord[(uintptr_t)pr](a);
}

void __fastcall WriteWordBP(UINT32 a, UINT16 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_W(a);

	CheckBreakpoint_W(a, ~1);

	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		*((UINT16*)(pr + (a & SEK_PAGEM))) = (UINT16)d;
		return;
	}
	pSekExt->WriteWord[(uintptr_t)pr](a, d);
}

UINT32 __fastcall ReadLongBP(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_R(a);

	CheckBreakpoint_R(a, ~1);

	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		UINT32 r = *((UINT32*)(pr + (a & SEK_PAGEM)));
		r = (r >> 16) | (r << 16);
		return r;
	}
	return pSekExt->ReadLong[(uintptr_t)pr](a);
}

void __fastcall WriteLongBP(UINT32 a, UINT32 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_W(a);

	CheckBreakpoint_W(a, ~1);

	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		d = (d >> 16) | (d << 16);
		*((UINT32*)(pr + (a & SEK_PAGEM))) = d;
		return;
	}
	pSekExt->WriteLong[(uintptr_t)pr](a, d);
}

#endif

#if defined (FBNEO_DEBUG)
void A68KCheckBreakpoint(unsigned int pc) { CheckBreakpoint_PC(pc); }
void A68KSingleStep(unsigned int pc) { SingleStep_PC(pc); }
#endif

extern "C" {
UINT8 C68KReadByte(UINT32 a) { return (UINT32)ReadByte(a); }
UINT16 C68KReadWord(UINT32 a) { return (UINT32)ReadWord(a); }
UINT32 C68KReadLong(UINT32 a) { return               ReadLong(a); }

UINT8 C68KFetchByte(UINT32 a) { return (UINT32)FetchByte(a); }
UINT16 C68KFetchWord(UINT32 a) { return (UINT32)FetchWord(a); }
UINT32 C68KFetchLong(UINT32 a) { return FetchLong(a); }
void C68KWriteByte(UINT32 a, UINT8 d) { WriteByte(a, d); }
void C68KWriteWord(UINT32 a, UINT16 d) { WriteWord(a, d); }
uintptr_t C68KRebasePC(UINT32 pc) {
//	bprintf(PRINT_NORMAL, _T("C68KRebasePC 0x%08x\n"), pc);
	pc &= 0xFFFFFF;
	SekC68KCurrentContext->BasePC = (uintptr_t)FIND_F(pc) - (pc & ~SEK_PAGEM);
	return SekC68KCurrentContext->BasePC + pc;
}

int C68KInterruptCallBack(int irqline)
{
	if (nSekIRQPending[nSekActive] & SEK_IRQSTATUS_AUTO) {
		SekC68KContext[nSekActive]->IRQState = 0;	//CLEAR_LINE
		SekC68KContext[nSekActive]->IRQLine = 0;
	}

	nSekIRQPending[nSekActive] = 0;

	if (pSekExt->IrqCallback) {
		return pSekExt->IrqCallback(irqline);
	}

	return C68K_INTERRUPT_AUTOVECTOR_EX + irqline;
}

void C68KResetCallBack()
{
	if (pSekExt->ResetCallback)
		pSekExt->ResetCallback();
}

}

// ----------------------------------------------------------------------------
// Memory accesses (non-emu specific)

UINT32 SekReadByte(UINT32 a) { return (UINT32)ReadByte(a); }
UINT32 SekReadWord(UINT32 a) { return (UINT32)ReadWord(a); }
UINT32 SekReadLong(UINT32 a) { return ReadLong(a); }

UINT32 SekFetchByte(UINT32 a) { return (UINT32)FetchByte(a); }
UINT32 SekFetchWord(UINT32 a) { return (UINT32)FetchWord(a); }
UINT32 SekFetchLong(UINT32 a) { return FetchLong(a); }

void SekWriteByte(UINT32 a, UINT8 d) { WriteByte(a, d); }
void SekWriteWord(UINT32 a, UINT16 d) { WriteWord(a, d); }
void SekWriteLong(UINT32 a, UINT32 d) { WriteLong(a, d); }

void SekWriteByteROM(UINT32 a, UINT8 d) { WriteByteROM(a, d); }
void SekWriteWordROM(UINT32 a, UINT16 d) { WriteWordROM(a, d); }
void SekWriteLongROM(UINT32 a, UINT32 d) { WriteLongROM(a, d); }

// ## SekCPUPush() / SekCPUPop() ## internal helpers for sending signals to other 68k's
struct m68kpstack {
	INT32 nHostCPU;
	INT32 nPushedCPU;
};
#define MAX_PSTACK 20

static m68kpstack pstack[MAX_PSTACK];
static INT32 pstacknum = 0;

INT32 SekCPUGetStackNum()
{
	return pstacknum;
}

void SekCPUPush(INT32 nCPU)
{
	m68kpstack *p = &pstack[pstacknum++];

	if (pstacknum + 1 >= MAX_PSTACK) {
		bprintf(0, _T("SekCPUPush(): out of stack!  Possible infinite recursion?  Crash pending..\n"));
	}

	p->nPushedCPU = nCPU;

	p->nHostCPU = SekGetActive();

	if (p->nHostCPU != p->nPushedCPU) {
		if (p->nHostCPU != -1) SekClose();
		SekOpen(p->nPushedCPU);
	}
}

void SekCPUPop()
{
	m68kpstack *p = &pstack[--pstacknum];

	if (p->nHostCPU != p->nPushedCPU) {
		SekClose();
		if (p->nHostCPU != -1) SekOpen(p->nHostCPU);
	}
}

static INT32 SekInitCPUC68K(INT32 nCount, INT32 nCPUType) {
	nSekCPUType[nCount] = nCPUType;

	nSekCPUOffsetAddress[nCount] = 1; // 3 for 020!

	switch (nCPUType) {
		case 0x68000:
			break;
		default:
			return 1;
	}

	SekC68KContext[nCount] = (c68k_struc*)malloc(sizeof( c68k_struc ));
	if (SekC68KContext[nCount] == NULL) {
		return 1;
	}

	memset(SekC68KContext[nCount], 0, sizeof( c68k_struc ));
	SekC68KCurrentContext = SekC68KContext[nCount];
	SekC68KCurrentContext->Rebase_PC = C68KRebasePC;
    SekC68KCurrentContext->Read_Byte = C68KReadByte;
    SekC68KCurrentContext->Read_Word = C68KReadWord;
    SekC68KCurrentContext->Read_Byte_PC_Relative = C68KFetchByte;
    SekC68KCurrentContext->Read_Word_PC_Relative = C68KFetchWord;
    SekC68KCurrentContext->Write_Byte = C68KWriteByte;
    SekC68KCurrentContext->Write_Word = C68KWriteWord;
    SekC68KCurrentContext->Interrupt_CallBack = C68KInterruptCallBack;
    SekC68KCurrentContext->Reset_CallBack = C68KResetCallBack;

	return 0;
}

void SekNewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekNewFrame called without init\n"));
#endif

	for (INT32 i = 0; i <= nSekCount; i++) {
		nSekCycles[i] = 0;
		nSekCyclesToDoCache[i] = 0;
		nSekm68k_ICount[i] = 0;
	}

	nSekCyclesToDo = m68k_ICount = 0;
	nSekCyclesTotal = 0;
}

void SekCyclesBurnRun(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetCyclesBurnRun called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetCyclesBurnRun called when no CPU open\n"));
#endif
	m68k_ICount -= nCycles;
}

void SekSetCyclesScanline(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetCyclesScanline called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetCyclesScanline called when no CPU open\n"));
#endif

	nSekCyclesScanline = nCycles;
}

UINT8 SekCheatRead(UINT32 a)
{
	return SekReadByte(a);
}

INT32 SekInit(INT32 nCount, INT32 nCPUType)
{
	DebugCPU_SekInitted = 1;

	struct SekExt* ps = NULL;

	if (nSekActive >= 0) {
		SekClose();
		nSekActive = -1;
	}

	if (nCount > nSekCount) {
		nSekCount = nCount;
	}

	// Allocate cpu extenal data (memory map etc)
	SekExt[nCount] = (struct SekExt*)malloc(sizeof(struct SekExt));
	if (SekExt[nCount] == NULL) {
		SekExit();
		return 1;
	}
	memset(SekExt[nCount], 0, sizeof(struct SekExt));

	// Put in default memory handlers
	ps = SekExt[nCount];

	for (INT32 j = 0; j < SEK_MAXHANDLER; j++) {
		ps->ReadByte[j]  = DefReadByte;
		ps->WriteByte[j] = DefWriteByte;
	}

	ps->ReadWord[0]  = DefReadWord0;
	ps->WriteWord[0] = DefWriteWord0;
	ps->ReadLong[0]  = DefReadLong0;
	ps->WriteLong[0] = DefWriteLong0;

#if SEK_MAXHANDLER >= 2
	ps->ReadWord[1]  = DefReadWord1;
	ps->WriteWord[1] = DefWriteWord1;
	ps->ReadLong[1]  = DefReadLong1;
	ps->WriteLong[1] = DefWriteLong1;
#endif

#if SEK_MAXHANDLER >= 3
	ps->ReadWord[2]  = DefReadWord2;
	ps->WriteWord[2] = DefWriteWord2;
	ps->ReadLong[2]  = DefReadLong2;
	ps->WriteLong[2] = DefWriteLong2;
#endif

#if SEK_MAXHANDLER >= 4
	ps->ReadWord[3]  = DefReadWord3;
	ps->WriteWord[3] = DefWriteWord3;
	ps->ReadLong[3]  = DefReadLong3;
	ps->WriteLong[3] = DefWriteLong3;
#endif

#if SEK_MAXHANDLER >= 5
	ps->ReadWord[4]  = DefReadWord4;
	ps->WriteWord[4] = DefWriteWord4;
	ps->ReadLong[4]  = DefReadLong4;
	ps->WriteLong[4] = DefWriteLong4;
#endif

#if SEK_MAXHANDLER >= 6
	ps->ReadWord[5]  = DefReadWord5;
	ps->WriteWord[5] = DefWriteWord5;
	ps->ReadLong[5]  = DefReadLong5;
	ps->WriteLong[5] = DefWriteLong5;
#endif

#if SEK_MAXHANDLER >= 7
	ps->ReadWord[6]  = DefReadWord6;
	ps->WriteWord[6] = DefWriteWord6;
	ps->ReadLong[6]  = DefReadLong6;
	ps->WriteLong[6] = DefWriteLong6;
#endif

#if SEK_MAXHANDLER >= 8
	ps->ReadWord[7]  = DefReadWord7;
	ps->WriteWord[7] = DefWriteWord7;
	ps->ReadLong[7]  = DefReadLong7;
	ps->WriteLong[7] = DefWriteLong7;
#endif

#if SEK_MAXHANDLER >= 9
	ps->ReadWord[8]  = DefReadWord8;
	ps->WriteWord[8] = DefWriteWord8;
	ps->ReadLong[8]  = DefReadLong8;
	ps->WriteLong[8] = DefWriteLong8;
#endif

#if SEK_MAXHANDLER >= 10
	ps->ReadWord[9]  = DefReadWord9;
	ps->WriteWord[9] = DefWriteWord9;
	ps->ReadLong[9]  = DefReadLong9;
	ps->WriteLong[9] = DefWriteLong9;
#endif

#if SEK_MAXHANDLER >= 11
	for (int j = 10; j < SEK_MAXHANDLER; j++) {
		ps->ReadWord[j]  = DefReadWord0;
		ps->WriteWord[j] = DefWriteWord0;
		ps->ReadLong[j]  = DefReadLong0;
		ps->WriteLong[j] = DefWriteLong0;
	}
#endif

	// Map the normal memory handlers
	SekDbgDisableBreakpoints();

	if ( SekInitCPUC68K(nCount, nCPUType) ) {
		SekExit();
		return 1;
	}
	C68k_Init( SekC68KCurrentContext );

	nSekAddressMask[nCount] = 0xffffff;

	nSekCycles[nCount] = 0;
	nSekCyclesToDoCache[nCount] = 0;
	nSekm68k_ICount[nCount] = 0;

	nSekIRQPending[nCount] = 0;
	for (INT32 i = 0; i < 8; i++) {
		nSekVIRQPending[nCount][i] = 0;
	}
	nSekRESETLine[nCount] = 0;
	nSekHALT[nCount] = 0;

	nSekCyclesTotal = 0;
	nSekCyclesScanline = 0;

	CpuCheatRegister(nCount, &SekConfig);

	pstacknum = 0;

	return 0;
}

static void SekCPUExitC68K(INT32 i) {
	if (SekC68KContext[i]) {
		free(SekC68KContext[i]);
		SekC68KContext[i] = NULL;
	}
}


void SekExit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekExit called without init\n"));
#endif

	if (!DebugCPU_SekInitted) return;

	// Deallocate cpu extenal data (memory map etc)
	for (INT32 i = 0; i <= nSekCount; i++) {
		SekCPUExitC68K(i);

		// Deallocate other context data
		if (SekExt[i]) {
			free(SekExt[i]);
			SekExt[i] = NULL;
		}

		nSekCPUOffsetAddress[i] = 0;
	}

	pSekExt = NULL;

	nSekActive = -1;
	nSekCount = -1;

	DebugCPU_SekInitted = 0;
}

void SekReset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekReset called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekReset called when no CPU open\n"));
#endif

	C68k_Reset( SekC68KCurrentContext );
	for (INT32 i = 0; i < 8; i++) {
		nSekVIRQPending[nSekActive][i] = 0;
	}
}

void SekReset(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekReset called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekReset();

	SekCPUPop();
}
// ----------------------------------------------------------------------------
// Control the active CPU

// Open a CPU
void SekOpen(const INT32 i)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekOpen called without init\n"));
	if (i > nSekCount) bprintf(PRINT_ERROR, _T("SekOpen called with invalid index %x\n"), i);
	if (nSekActive != -1) bprintf(PRINT_ERROR, _T("SekOpen called when CPU already open (%x) with index %x\n"), nSekActive, i);
#endif

	if (i != nSekActive) {
		nSekActive = i;

		pSekExt = SekExt[nSekActive];						// Point to cpu context

		nSekAddressMaskActive = nSekAddressMask[nSekActive];

		SekC68KCurrentContext = SekC68KContext[nSekActive];

		nSekCyclesTotal = nSekCycles[nSekActive];

		// Allow for SekRun() reentrance:
		nSekCyclesToDo = nSekCyclesToDoCache[nSekActive];
		m68k_ICount = nSekm68k_ICount[nSekActive];
	}
}

// Close the active cpu
void SekClose()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekClose called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekClose called when no CPU open\n"));
#endif

	nSekCycles[nSekActive] = nSekCyclesTotal;

	// Allow for SekRun() reentrance:
	nSekCyclesToDoCache[nSekActive] = nSekCyclesToDo;
	nSekm68k_ICount[nSekActive] = m68k_ICount;

	nSekActive = -1;
}

// Get the current CPU
INT32 SekGetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetActive called without init\n"));
#endif

	return nSekActive;
}

// For Megadrive - check if the vdp controlport should set IRQ
INT32 SekShouldInterrupt()
{
	// return m68k_check_shouldinterrupt();
	return 0;
}

void SekBurnUntilInt()
{
	// m68k_burn_until_irq(1);
}

INT32 SekGetRESETLine()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetRESETLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetRESETLine called when no CPU open\n"));
#endif


	if (nSekActive != -1)
	{
		return nSekRESETLine[nSekActive];
	}

	return 0;
}

INT32 SekGetRESETLine(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetRESETLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 rc = SekGetRESETLine();

	SekCPUPop();

	return rc;
}

void SekSetRESETLine(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetRESETLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetRESETLine called when no CPU open\n"));
#endif

	if (nSekActive != -1)
	{
		if (nSekRESETLine[nSekActive] && nStatus == 0)
		{
			SekReset();
			//bprintf(0, _T("SEK: cleared resetline.\n"));
		}

		nSekRESETLine[nSekActive] = nStatus;
	}
}

void SekSetRESETLine(INT32 nCPU, INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetRESETLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetRESETLine(nStatus);

	SekCPUPop();
}

INT32 SekGetHALT()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetHALT called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetHALT called when no CPU open\n"));
#endif


	if (nSekActive != -1)
	{
		return nSekHALT[nSekActive];
	}

	return 0;
}

INT32 SekGetHALT(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetHALT called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 rc = SekGetHALT();

	SekCPUPop();

	return rc;
}

INT32 SekTotalCycles(INT32 nCPU)
{
	SekCPUPush(nCPU);

	INT32 rc = SekTotalCycles();

	SekCPUPop();

	return rc;
}

void SekSetHALT(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetHALT called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetHALT called when no CPU open\n"));
#endif

	if (nSekActive != -1)
	{
		if (nSekHALT[nSekActive] == 1 && nStatus == 0)
		{
			//bprintf(0, _T("SEK: cleared HALT.\n"));
		}

		if (nSekHALT[nSekActive] == 0 && nStatus == 1)
		{
			//bprintf(0, _T("SEK: entered HALT.\n"));
			// we must halt in the cpu core too
			SekRunEnd();
		}

		nSekHALT[nSekActive] = nStatus;
	}
}

void SekSetHALT(INT32 nCPU, INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetHALT called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetHALT(nStatus);

	SekCPUPop();
}


// Set the status of an IRQ line on the active CPU
void SekSetIRQLine(const INT32 line, INT32 nstatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIRQLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetIRQLine called when no CPU open\n"));
#endif

	if (nstatus == CPU_IRQSTATUS_HOLD)
		nstatus = CPU_IRQSTATUS_AUTO; // on sek, AUTO is HOLD.

	INT32 status = nstatus << 12; // needed for compatibility

//	bprintf(PRINT_NORMAL, _T("  - irq line %i -> %i\n"), line, status);

	if (status) {
		nSekIRQPending[nSekActive] = line | status;

		SekC68KCurrentContext->IRQState = 1;	//ASSERT_LINE
		SekC68KCurrentContext->IRQLine = line;
		SekC68KCurrentContext->HaltState = 0;
		return;
	}

	nSekIRQPending[nSekActive] = 0;

	SekC68KCurrentContext->IRQState = 0;	//CLEAR_LINE
	SekC68KCurrentContext->IRQLine = 0;
}

void SekSetIRQLine(INT32 nCPU, const INT32 line, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIRQLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetIRQLine(line, status);

	SekCPUPop();
}

void SekSetVIRQLine(const INT32 line, INT32 nstatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIRQLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetIRQLine called when no CPU open\n"));
#endif

	if (nstatus == CPU_IRQSTATUS_AUTO) {
		nstatus = 4; // special handling for virq
	}

	INT32 status = nstatus << 12; // needed for compatibility

	if (status) {
		nSekVIRQPending[nSekActive][line] = status;

#ifdef EMU_M68K
			m68k_set_virq(line, 1);
#endif

		return;
	}

	nSekVIRQPending[nSekActive][line] = 0;

#ifdef EMU_M68K
	m68k_set_virq(line, 0);
#endif
}

void SekSetVIRQLine(INT32 nCPU, const INT32 line, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetVIRQLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetVIRQLine(line, status);

	SekCPUPop();
}


// Adjust the active CPU's timeslice
void SekRunAdjust(const INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekRunAdjust called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekRunAdjust called when no CPU open\n"));
#endif

	if (nCycles < 0 && m68k_ICount < -nCycles) {
		SekRunEnd();
		return;
	}

	nSekCyclesToDo += nCycles;
	SekC68KCurrentContext->ICount += nCycles;
	nSekCyclesSegment += nCycles;
}

// End the active CPU's timeslice
void SekRunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekRunEnd called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekRunEnd called when no CPU open\n"));
#endif

	nSekCyclesTotal += (nSekCyclesToDo - nSekCyclesDone) - SekC68KCurrentContext->ICount;
	nSekCyclesDone += (nSekCyclesToDo - nSekCyclesDone) - SekC68KCurrentContext->ICount;
	nSekCyclesSegment = nSekCyclesDone;
	nSekCyclesToDo = SekC68KCurrentContext->ICount = -1;
}

// Run the active CPU
INT32 SekRun(const INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekRun called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekRun called when no CPU open\n"));
#endif
	nSekCyclesToDo = nCycles;

	if (nSekRESETLine[nSekActive] || nSekHALT[nSekActive]) {
		nSekCyclesSegment = nCycles; // idle when RESET high or halted
	} else {
		nSekCyclesSegment = C68k_Exec(SekC68KCurrentContext, nCycles);
	}
	nSekCyclesTotal += nSekCyclesSegment;
	nSekCyclesToDo = SekC68KCurrentContext->ICount = -1;

	return nSekCyclesSegment;
}

INT32 SekRun(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekRun called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 nRet = SekRun(nCycles);

	SekCPUPop();

	return nRet;
}

INT32 SekIdle(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekIdle called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 nRet = SekIdle(nCycles);

	SekCPUPop();

	return nRet;
}


// ----------------------------------------------------------------------------
// Breakpoint support

void SekDbgDisableBreakpoints()
{
#if defined FBNEO_DEBUG && defined EMU_M68K
		m68k_set_instr_hook_callback(NULL);

		M68KReadByteDebug = M68KReadByte;
		M68KReadWordDebug = M68KReadWord;
		M68KReadLongDebug = M68KReadLong;

		M68KWriteByteDebug = M68KWriteByte;
		M68KWriteWordDebug = M68KWriteWord;
		M68KWriteLongDebug = M68KWriteLong;
#endif
}

#if defined (FBNEO_DEBUG)

void SekDbgEnableBreakpoints()
{
	if (BreakpointDataRead[0].address || BreakpointDataWrite[0].address || BreakpointFetch[0].address) {
	} else {
		SekDbgDisableBreakpoints();
	}
}

void SekDbgEnableSingleStep()
{
}

INT32 SekDbgSetBreakpointDataRead(UINT32 nAddress, INT32 nIdentifier)
{
	for (INT32 i = 0; i < 8; i++) {
		if (BreakpointDataRead[i].id == nIdentifier) {

			if	(nAddress) {							// Change breakpoint
				BreakpointDataRead[i].address = nAddress;
			} else {									// Delete breakpoint
				for ( ; i < 8; i++) {
					BreakpointDataRead[i] = BreakpointDataRead[i + 1];
				}
			}

			SekDbgEnableBreakpoints();
			return 0;
		}
	}

	// No breakpoints present, add it to the 1st slot
	BreakpointDataRead[0].address = nAddress;
	BreakpointDataRead[0].id = nIdentifier;

	SekDbgEnableBreakpoints();
	return 0;
}

INT32 SekDbgSetBreakpointDataWrite(UINT32 nAddress, INT32 nIdentifier)
{
	for (INT32 i = 0; i < 8; i++) {
		if (BreakpointDataWrite[i].id == nIdentifier) {

			if (nAddress) {								// Change breakpoint
				BreakpointDataWrite[i].address = nAddress;
			} else {									// Delete breakpoint
				for ( ; i < 8; i++) {
					BreakpointDataWrite[i] = BreakpointDataWrite[i + 1];
				}
			}

			SekDbgEnableBreakpoints();
			return 0;
		}
	}

	// No breakpoints present, add it to the 1st slot
	BreakpointDataWrite[0].address = nAddress;
	BreakpointDataWrite[0].id = nIdentifier;

	SekDbgEnableBreakpoints();
	return 0;
}

INT32 SekDbgSetBreakpointFetch(UINT32 nAddress, INT32 nIdentifier)
{
	for (INT32 i = 0; i < 8; i++) {
		if (BreakpointFetch[i].id == nIdentifier) {

			if (nAddress) {								// Change breakpoint
				BreakpointFetch[i].address = nAddress;
			} else {									// Delete breakpoint
				for ( ; i < 8; i++) {
					BreakpointFetch[i] = BreakpointFetch[i + 1];
				}
			}

			SekDbgEnableBreakpoints();
			return 0;
		}
	}

	// No breakpoints present, add it to the 1st slot
	BreakpointFetch[0].address = nAddress;
	BreakpointFetch[0].id = nIdentifier;

	SekDbgEnableBreakpoints();
	return 0;
}

#endif

// ----------------------------------------------------------------------------
// Memory map setup

void SekSetAddressMask(UINT32 nAddressMask)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetAddressMask called without init\n"));
	if (nSekActive == -1) { bprintf(PRINT_ERROR, _T("SekSetAddressMask called when no CPU open\n")); return; }
	if ((nAddressMask & 1) == 0) bprintf(PRINT_ERROR, _T("SekSetAddressMask called with invalid mask! (%x)\n"), nAddressMask);
#endif

	nSekAddressMask[nSekActive] = nSekAddressMaskActive = nAddressMask;
}

// Note - each page is 1 << SEK_BITS.
INT32 SekMapMemory(UINT8* pMemory, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekMapMemory called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekMapMemory called when no CPU open\n"));
	if (pMemory == NULL) bprintf(0, _T("SekMapMemory() mapped NULL block!  start, end, type:  %x - %x  0x%x\n"), nStart, nEnd, nType);
#endif

	UINT8* Ptr = pMemory - nStart;
	UINT8** pMemMap = pSekExt->MemMap + (nStart >> SEK_SHIFT);

	// Special case for ROM banks
	if (nType == MAP_ROM) {
		for (UINT32 i = (nStart & ~SEK_PAGEM); i <= nEnd; i += SEK_PAGE_SIZE, pMemMap++) {
			pMemMap[0]			  = Ptr + i;
			pMemMap[SEK_WADD * 2] = Ptr + i;
		}

		return 0;
	}

	for (UINT32 i = (nStart & ~SEK_PAGEM); i <= nEnd; i += SEK_PAGE_SIZE, pMemMap++) {

		if (nType & MAP_READ) {					// Read
			pMemMap[0]			  = Ptr + i;
		}
		if (nType & MAP_WRITE) {					// Write
			pMemMap[SEK_WADD]	  = Ptr + i;
		}
		if (nType & MAP_FETCH) {					// Fetch
			pMemMap[SEK_WADD * 2] = Ptr + i;
		}
	}

	return 0;
}

INT32 SekMapHandler(uintptr_t nHandler, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekMapHander called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekMapHandler called when no CPU open\n"));
#endif

	UINT8** pMemMap = pSekExt->MemMap + (nStart >> SEK_SHIFT);

	// Add to memory map
	for (UINT32 i = (nStart & ~SEK_PAGEM); i <= nEnd; i += SEK_PAGE_SIZE, pMemMap++) {

		if (nType & MAP_READ) {					// Read
			pMemMap[0]			  = (UINT8*)nHandler;
		}
		if (nType & MAP_WRITE) {					// Write
			pMemMap[SEK_WADD]	  = (UINT8*)nHandler;
		}
		if (nType & MAP_FETCH) {					// Fetch
			pMemMap[SEK_WADD * 2] = (UINT8*)nHandler;
		}
	}

	return 0;
}

// Set callbacks
INT32 SekSetResetCallback(pSekResetCallback pCallback)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetResetCallback called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetResetCallback called when no CPU open\n"));
#endif

	pSekExt->ResetCallback = pCallback;

	return 0;
}

INT32 SekSetRTECallback(pSekRTECallback pCallback)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetRTECallback called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetRTECallback called when no CPU open\n"));
#endif

	pSekExt->RTECallback = pCallback;

	return 0;
}

INT32 SekSetIrqCallback(pSekIrqCallback pCallback)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIrqCallback called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetIrqCallback called when no CPU open\n"));
#endif

	pSekExt->IrqCallback = pCallback;

	return 0;
}

INT32 SekSetCmpCallback(pSekCmpCallback pCallback)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetCmpCallback called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetCmpCallback called when no CPU open\n"));
#endif

	pSekExt->CmpCallback = pCallback;

	return 0;
}

INT32 SekSetTASCallback(pSekTASCallback pCallback)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetTASCallback called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetTASCallback called when no CPU open\n"));
#endif

	pSekExt->TASCallback = pCallback;

	return 0;
}

// Set handlers
INT32 SekSetReadByteHandler(INT32 i, pSekReadByteHandler pHandler)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetReadByteHandler called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetReadByteHandler called when no CPU open\n"));
#endif

	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->ReadByte[i] = pHandler;

	return 0;
}

INT32 SekSetWriteByteHandler(INT32 i, pSekWriteByteHandler pHandler)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetWriteByteHandler called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetWriteByteHandler called when no CPU open\n"));
#endif

	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->WriteByte[i] = pHandler;

	return 0;
}

INT32 SekSetReadWordHandler(INT32 i, pSekReadWordHandler pHandler)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetReadWordHandler called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetReadWordHandler called when no CPU open\n"));
#endif

	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->ReadWord[i] = pHandler;

	return 0;
}

INT32 SekSetWriteWordHandler(INT32 i, pSekWriteWordHandler pHandler)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetWriteWordHandler called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetWriteWordHandler called when no CPU open\n"));
#endif

	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->WriteWord[i] = pHandler;

	return 0;
}

INT32 SekSetReadLongHandler(INT32 i, pSekReadLongHandler pHandler)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetReadLongHandler called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetReadLongHandler called when no CPU open\n"));
#endif

	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->ReadLong[i] = pHandler;

	return 0;
}

INT32 SekSetWriteLongHandler(INT32 i, pSekWriteLongHandler pHandler)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetWriteLongHandler called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetWriteLongHandler called when no CPU open\n"));
#endif

	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->WriteLong[i] = pHandler;

	return 0;
}

// ----------------------------------------------------------------------------
// Query register values

UINT32 SekGetPC(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetPC called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetPC called when no CPU open\n"));
#endif

	return SekC68KCurrentContext->PC - SekC68KCurrentContext->BasePC;
}

UINT32 SekGetPPC(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetPC called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetPC called when no CPU open\n"));
#endif

#ifdef EMU_M68K
		return m68k_get_reg(NULL, M68K_REG_PPC);
#else
		return 0;
#endif
}

INT32 SekDbgGetCPUType()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekDbgGetCPUType called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekDbgGetCPUType called when no CPU open\n"));
#endif

	switch (nSekCPUType[nSekActive]) {
		case 0:
		case 0x68000:
			return 0;
	}

	return 0;
}

INT32 SekDbgGetPendingIRQ()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekDbgGetPendingIRQ called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekDbgGetPendingIRQ called when no CPU open\n"));
#endif

	return nSekIRQPending[nSekActive] & 7;
}

UINT32 SekDbgGetRegister(SekRegister nRegister)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekDbgGetRegister called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekDbgGetRegister called when no CPU open\n"));
#endif
	return 0;
}

bool SekDbgSetRegister(SekRegister nRegister, UINT32 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekDbgSetRegister called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekDbgSetRegister called when no CPU open\n"));
#endif

	return false;
}

// ----------------------------------------------------------------------------
// Savestate support

INT32 SekScan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekScan called without init\n"));
#endif

	// Scan the 68000 states
	struct BurnArea ba;

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	memset(&ba, 0, sizeof(ba));

	nSekActive = -1;

	for (INT32 i = 0; i <= nSekCount; i++) {
		char szName[] = "MC68000 #n";
		szName[9] = '0' + i;

		SCAN_VAR(nSekCPUType[i]);
		SCAN_VAR(nSekIRQPending[i]);
		SCAN_VAR(nSekVIRQPending[i]);
		SCAN_VAR(nSekCycles[i]);
		SCAN_VAR(nSekRESETLine[i]);
		SCAN_VAR(nSekHALT[i]);

		ba.Data = SekC68KContext[i];
		ba.nLen = (uintptr_t)&(SekC68KContext[i]->Rebase_PC) - (uintptr_t)SekC68KContext[i];
		ba.szName = szName;
		BurnAcb(&ba);
		SekC68KContext[i]->PC = C68KRebasePC(SekGetPC(i));
	}

	return 0;
}
