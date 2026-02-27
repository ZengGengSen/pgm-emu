// Z80 (Zed Eight-Ty) Interface
#include "burnint.h"
#include "z80_intf.h"
#include <stddef.h>

#define Z80_BIG_FLAGS_ARRAY 1
#define Z80_EMULATE_R_EXACTLY 0

#define MAX_Z80		8
static struct ZetExt * ZetCPUContext[MAX_Z80] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

typedef UINT8 (__fastcall *pZetInHandler)(UINT16 a);
typedef void (__fastcall *pZetOutHandler)(UINT16 a, UINT8 d);
typedef UINT8 (__fastcall *pZetReadHandler)(UINT16 a);
typedef void (__fastcall *pZetWriteHandler)(UINT16 a, UINT8 d);

union Reg16 {
	struct {
#ifdef LSB_FIRST
		UINT8 l;
		UINT8 h;
#else
		UINT8 h;
		UINT8 l;
#endif
	} b;
	UINT16 w;
};

static void Z80InitFlags();
static void Z80Init();
static void Z80Exit();
static void Z80Reset();
static INT32 Z80Execute(INT32 nCycles);
static INT32 Z80Nmi();

struct ZetExt {
	struct {
		Reg16 bc;
		Reg16 de;
		Reg16 hl;
		Reg16 af;
	};

	Reg16 ix;
	Reg16 iy;
	Reg16 sp;

	UINT8 i;
	UINT8 im;

	Reg16 bc2;
	Reg16 de2;
	Reg16 hl2;
	Reg16 af2;

	Reg16 r;
	Reg16 iff;

	uintptr_t pc;
	uintptr_t base_pc;

	UINT8  bStopRun;
	UINT32 BusReq;
	UINT32 ResetLine;

	INT32 nCyclesSegment;
	INT32 nCyclesLeft;

	INT32 nCyclesDelayed;
	INT32 nCyclesDone;

	int nEI;
	int nInterruptLatch;

	UINT8 *pR8[8];
	Reg16 *pR16[4];

	UINT8* pZetMemMap[0x100 * 4];

	pZetInHandler ZetIn;
	pZetOutHandler ZetOut;
	pZetReadHandler ZetRead;
	pZetWriteHandler ZetWrite;
};

INT32 nZetCyclesTotal;

static INT32 nOpenedCPU = -1;
static INT32 nCPUCount = 0;
INT32 nHasZet = -1;

cpu_core_config ZetConfig = {
	"Z80",
	ZetCPUPush, //ZetOpen,
	ZetCPUPop, //ZetClose,
	ZetCheatRead,
	ZetCheatWriteROM,
	ZetGetActive,
	ZetTotalCycles,
	ZetNewFrame,
	ZetIdle,
	ZetSetIRQLine,
	ZetRun,
	ZetRunEnd,
	ZetReset,
	ZetScan,
	ZetExit,
	0x10000,
	0
};

UINT8 __fastcall ZetDummyReadHandler(UINT16) { return 0; }
void __fastcall ZetDummyWriteHandler(UINT16, UINT8) { }
UINT8 __fastcall ZetDummyInHandler(UINT16) { return 0; }
void __fastcall ZetDummyOutHandler(UINT16, UINT8) { }

void ZetSetReadHandler(UINT8 (__fastcall *pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetReadHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetReadHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetRead = pHandler;
}

void ZetSetWriteHandler(void (__fastcall *pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetWriteHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetWriteHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetWrite = pHandler;
}

void ZetSetInHandler(UINT8 (__fastcall *pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetInHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetInHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetIn = pHandler;
}

void ZetSetOutHandler(void (__fastcall *pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetOutHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetOutHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetOut = pHandler;
}

void ZetNewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetNewFrame called without init\n"));
#endif

	for (INT32 i = 0; i < nCPUCount; i++) {
		ZetCPUContext[i]->nCyclesDone = 0;
	}
	nZetCyclesTotal = 0;
}

void ZetCheatWriteROM(UINT32 a, UINT8 d)
{
	// ZetWriteRom(a, d);
}

UINT8 ZetCheatRead(UINT32 a)
{
	// return ZetReadByte(a);
	return 0;
}

INT32 ZetInit(INT32 nCPU)
{
	DebugCPU_ZetInitted = 1;

	nOpenedCPU = nCPU;

	ZetCPUContext[nCPU] = (struct ZetExt*)BurnMalloc(sizeof(ZetExt));
	memset (ZetCPUContext[nCPU], 0, sizeof(ZetExt));

	if (nCPU == 0)
		Z80InitFlags(); // initialize flags arrays

	Z80Init(); // clear/init next z80 slot (internal to z80.cpp)

	{
		ZetCPUContext[nCPU]->ZetIn = ZetDummyInHandler;
		ZetCPUContext[nCPU]->ZetOut = ZetDummyOutHandler;
		ZetCPUContext[nCPU]->ZetRead = ZetDummyReadHandler;
		ZetCPUContext[nCPU]->ZetWrite = ZetDummyWriteHandler;
		ZetCPUContext[nCPU]->bStopRun = 0;
		ZetCPUContext[nCPU]->BusReq = 0;
		ZetCPUContext[nCPU]->ResetLine = 0;

		ZetCPUContext[nCPU]->nCyclesDone = 0;
		ZetCPUContext[nCPU]->nCyclesDelayed = 0;

		ZetCPUContext[nCPU]->nCyclesSegment = 0;
		ZetCPUContext[nCPU]->nCyclesLeft = 0;

		for (INT32 j = 0; j < (0x0100 * 4); j++) {
			ZetCPUContext[nCPU]->pZetMemMap[j] = NULL;
		}
	}

	nZetCyclesTotal = 0;

	nCPUCount = (nCPU+1) % MAX_Z80;

	nHasZet = nCPU+1;

	CpuCheatRegister(nCPU, &ZetConfig);

	nOpenedCPU = -1;

	return 0;
}

void ZetClose()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetClose called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetClose called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->nCyclesDone = nZetCyclesTotal;

	nOpenedCPU = -1;
}

void ZetOpen(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetOpen called without init\n"));
	if (nCPU >= nCPUCount) bprintf(PRINT_ERROR, _T("ZetOpen called with invalid index %x\n"), nCPU);
	if (nOpenedCPU != -1) bprintf(PRINT_ERROR, _T("ZetOpen called when CPU already open with index %x\n"), nCPU);
	if (ZetCPUContext[nCPU] == NULL) bprintf (PRINT_ERROR, _T("ZetOpen called for uninitialized cpu %x\n"), nCPU);
#endif

	nZetCyclesTotal = ZetCPUContext[nCPU]->nCyclesDone;

	nOpenedCPU = nCPU;
}

INT32 ZetGetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetActive called without init\n"));
	//if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetGetActive called when no CPU open\n"));
#endif

	return nOpenedCPU;
}

// ## ZetCPUPush() / ZetCPUPop() ## internal helpers for sending signals to other 68k's
struct z80pstack {
	INT32 nHostCPU;
	INT32 nPushedCPU;
};
#define MAX_PSTACK 10

static z80pstack pstack[MAX_PSTACK];
static INT32 pstacknum = 0;

void ZetCPUPush(INT32 nCPU)
{
	z80pstack *p = &pstack[pstacknum++];

	if (pstacknum + 1 >= MAX_PSTACK) {
		bprintf(0, _T("ZetCPUPush(): out of stack!  Possible infinite recursion?  Crash pending..\n"));
	}

	p->nPushedCPU = nCPU;

	p->nHostCPU = ZetGetActive();

	if (p->nHostCPU != p->nPushedCPU) {
		if (p->nHostCPU != -1) ZetClose();
		ZetOpen(p->nPushedCPU);
	}
}

void ZetCPUPop()
{
	z80pstack *p = &pstack[--pstacknum];

	if (p->nHostCPU != p->nPushedCPU) {
		ZetClose();
		if (p->nHostCPU != -1) ZetOpen(p->nHostCPU);
	}
}

INT32 ZetRun(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetRun called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetRun called when no CPU open\n"));
#endif

	if (nCycles <= 0) return 0;

	INT32 nDelayed = 0;  // handle delayed cycle counts (from nmi / irq)
	if (ZetCPUContext[nOpenedCPU]->nCyclesDelayed) {
		nDelayed = ZetCPUContext[nOpenedCPU]->nCyclesDelayed;
		ZetCPUContext[nOpenedCPU]->nCyclesDelayed = 0;
		nCycles -= nDelayed;
	}

	if (!ZetCPUContext[nOpenedCPU]->BusReq && !ZetCPUContext[nOpenedCPU]->ResetLine) {
		nCycles = Z80Execute(nCycles);
	}

	nCycles += nDelayed;

	nZetCyclesTotal += nCycles;

	return nCycles;
}

void ZetRunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetRunEnd called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetRunEnd called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->bStopRun = 1;
}

void ZetExit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetExit called without init\n"));
#endif

	if (!DebugCPU_ZetInitted) return;

	for (INT32 i = 0; i < nCPUCount; i++) {
		ZetOpen(i);
		Z80Exit(); // exit daisy chain & peripherals attached to this cpu.
		ZetClose();
	}

	for (INT32 i = 0; i < MAX_Z80; i++) {
		if (ZetCPUContext[i]) {
			BurnFree(ZetCPUContext[i]);
			ZetCPUContext[i] = NULL;
		}
	}

	nCPUCount = 0;
	nHasZet = -1;

	DebugCPU_ZetInitted = 0;
}

void ZetMapMemory(UINT8 *Mem, INT32 nStart, INT32 nEnd, INT32 nFlags)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMapMemory called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nFlags & MAP_READ)		pMemMap[0x000 + i] = Mem + ((i - cStart) << 8);
		if (nFlags & MAP_WRITE)		pMemMap[0x100 + i] = Mem + ((i - cStart) << 8);
		if (nFlags & MAP_FETCHOP)	pMemMap[0x200 + i] = Mem + ((i - cStart) << 8);
		if (nFlags & MAP_FETCHARG)	pMemMap[0x300 + i] = Mem + ((i - cStart) << 8);
	}
}

// This function will make an area callback ZetRead/ZetWrite
INT32 ZetMemCallback(INT32 nStart, INT32 nEnd, INT32 nMode)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMemCallback called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMemCallback called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		switch (nMode) {
			case 0: pMemMap[0x000 + i] = NULL; break;
			case 1: pMemMap[0x100 + i] = NULL; break;
			case 2: pMemMap[0x200 + i] = NULL;
					pMemMap[0x300 + i] = NULL; break;
		}
	}

	return 0;
}

INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMapArea called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMapArea called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		switch (nMode) {
			case 0: pMemMap[0x000 + i] = Mem + ((i - cStart) << 8); break;
			case 1: pMemMap[0x100 + i] = Mem + ((i - cStart) << 8); break;
			case 2: pMemMap[0x200 + i] = Mem + ((i - cStart) << 8);
					pMemMap[0x300 + i] = Mem + ((i - cStart) << 8); break;
		}
	}

	return 0;
}

INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem01, UINT8 *Mem02)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMapArea called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMapArea called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	if (nMode != 2) {
		return 1;
	}

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		pMemMap[0x200 + i] = Mem01 + ((i - cStart) << 8);
		pMemMap[0x300 + i] = Mem02 + ((i - cStart) << 8);
	}

	return 0;
}

void ZetReset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetReset called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetReset called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->nCyclesDelayed = 0;
	Z80Reset();
}

INT32 ZetScan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetScan called without init\n"));
#endif

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 0;
	}

	char szText[] = "Z80 #0";

	for (INT32 i = 0; i < nCPUCount; i++) {
		uintptr_t nRealPC;
		szText[5] = '1' + i;

		ScanVar(&ZetCPUContext[i], STRUCT_SIZE_HELPER(ZetExt, nInterruptLatch), szText);

		nRealPC = ZetCPUContext[i]->pc - ZetCPUContext[i]->base_pc;
		ZetCPUContext[i]->base_pc = (uintptr_t)ZetCPUContext[i]->pZetMemMap[0x200 | (nRealPC >> 8)] - (nRealPC & ~0xff);
		ZetCPUContext[i]->pc = nRealPC + ZetCPUContext[i]->base_pc;
	}

	return 0;
}

void ZetSetIRQLine(const INT32 line, const INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetIRQLine called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetIRQLine called when no CPU open\n"));
#endif

	switch ( status ) {
		case CPU_IRQSTATUS_NONE:
			ZetCPUContext[nOpenedCPU]->nInterruptLatch = 0x8000 | line;
			break;
		case CPU_IRQSTATUS_ACK:
			ZetCPUContext[nOpenedCPU]->nInterruptLatch = 0x1000 | line;
			break;
		case CPU_IRQSTATUS_AUTO:
			ZetCPUContext[nOpenedCPU]->nInterruptLatch = 0x2000 | line;
			break;
		case CPU_IRQSTATUS_HOLD:
			ZetCPUContext[nOpenedCPU]->nInterruptLatch = 0x4000 | line;
			break;
	}
}

void ZetSetIRQLine(INT32 nCPU, const INT32 line, const INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetIRQLine called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetSetIRQLine(line, status);

	ZetCPUPop();
}

INT32 ZetNmi()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetNmi called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetNmi called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->nCyclesDelayed += Z80Nmi();

	return 0;
}

INT32 ZetIdle(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetIdle called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetIdle called when no CPU open\n"));
#endif

	nZetCyclesTotal += nCycles;

	return nCycles;
}

INT32 ZetTotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetTotalCycles called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetTotalCycles called when no CPU open\n"));
#endif

	return nZetCyclesTotal + ZetCPUContext[nOpenedCPU]->nCyclesSegment - ZetCPUContext[nOpenedCPU]->nCyclesLeft;
}

void ZetSetHALT(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetHALT called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetHALT called when no CPU open\n"));
#endif

	if (nOpenedCPU < 0) return;

	ZetCPUContext[nOpenedCPU]->BusReq = nStatus;
	if (nStatus) ZetRunEnd(); // end current timeslice since we're halted
}

#include "cz80/cz80.cpp"
