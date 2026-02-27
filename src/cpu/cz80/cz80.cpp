/// Z80 Implement

static UINT8 SZ[256];		/* zero and sign flags */
static UINT8 SZ_BIT[256];	/* zero, sign and parity/overflow (=zero) flags for BIT opcode */
static UINT8 SZP[256];		/* zero, sign and parity flags */
static UINT8 SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
static UINT8 SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */

#define CF					0x01
#define NF					0x02
#define PF					0x04
#define VF					PF
#define XF					0x08
#define HF					0x10
#define YF					0x20
#define ZF					0x40
#define SF					0x80

#define IFF		PF

#if Z80_BIG_FLAGS_ARRAY
static UINT8 *SZHVC_add = 0;
static UINT8 *SZHVC_sub = 0;
#endif

#define CPU			Z80

#define zR8(a)		(*CPU->pR8[a])

#define pzR16(a)	CPU->pR16[a]
#define zR16(a)		(CPU->pR16[a]->w)

#define pzAF		&(CPU->af)
#define zAF			CPU->af.w
#define zA			CPU->af.b.h
#define zF			CPU->af.b.l

#define pzBC		&(CPU->bc)
#define zBC			CPU->bc.w
#define zB			CPU->bc.b.h
#define zC			CPU->bc.b.l

#define pzDE		&(CPU->de)
#define zDE			CPU->de.w
#define zD			CPU->de.b.h
#define zE			CPU->de.b.l

#define pzHL		&(CPU->hl)
#define zHL			CPU->hl.w
#define zH			CPU->hl.b.h
#define zL			CPU->hl.b.l

#define zAF2		CPU->af2.w
#define zA2			CPU->af2.b.h
#define zF2			CPU->af2.b.l

#define zBC2		CPU->bc2.w
#define zDE2		CPU->de2.w
#define zHL2		CPU->hl2.w

#define pzIX		&(CPU->ix)
#define zIX			CPU->ix.w
#define zlIX		CPU->ix.b.h
#define zhIX		CPU->ix.b.l

#define pzIY		&(CPU->iy)
#define zIY			CPU->iy.w
#define zlIY		CPU->iy.b.h
#define zhIY		CPU->iy.b.l

#define pzSP		&(CPU->sp)
#define zSP			CPU->sp.w
#define zlSP		CPU->sp.b.h
#define zhSP		CPU->sp.b.l

#define zI			CPU->i
#define zIM			CPU->im

#define zBasePC		CPU->base_pc
#define zRealPC		(PC - CPU->base_pc)
#define zPC			CPU->pc

#define zwR			CPU->r.w
#define zR1			CPU->r.b.h
#define zR2			CPU->r.b.l
#define zR			zR1

#define zIFF		CPU->iff.w
#define zIFF1		CPU->iff.b.h
#define zIFF2		CPU->iff.b.l

#define GET_OP()	(*(UINT8 *)(PC + PCDiff))
#define READ_OP()   GET_OP(); PC++

#define _SSOP(A,B)			A##B
#define OP(A)				_SSOP(OP,A)
#define OPCB(A)				_SSOP(OPCB,A)
#define OPED(A)				_SSOP(OPED,A)
#define OPXY(A)				_SSOP(OPXY,A)
#define OPXYCB(A)			_SSOP(OPXYCB,A)

#define ARG()			(*(UINT8 *)PC++)
#ifdef LSB_FIRST
#define ARG16()			(*(UINT8 *)PC | (*(UINT8 *)(PC + 1) << 8)); PC += 2
#else
#define ARG16()			(*(UINT8 *)(PC + 1) | (*(UINT8 *)PC << 8)); PC += 2
#endif

#define USE_CYCLES(A)		Z80->nCyclesLeft -= (A);
#define ADD_CYCLES(A)		Z80->nCyclesLeft += (A);
#define RET(A) {															\
	Z80->nCyclesLeft -= A;													\
	if (!Z80->bStopRun && Z80->nCyclesLeft > 0)								\
		goto Cz80_Exec;														\
	goto Cz80_Exec_End;														\
}

#define READ_MEM8(A, D) {													\
	UINT8 *ptr = Z80->pZetMemMap[0x000 | (A) >> 8];							\
	if (ptr) {																\
		ptr -= (A & ~0xff);													\
		D = ptr[A];															\
	} else {																\
		zPC = PC;															\
		D = Z80->ZetRead(A);												\
	}																		\
}

#define READ_MEM16(A, D) {													\
	UINT8 *ptr = Z80->pZetMemMap[0x000 | (A) >> 8];							\
	if (ptr) {																\
		ptr -= (A & ~0xff);													\
		D = ptr[A] | (ptr[(A)+1] << 8);										\
	} else {																\
		zPC = PC;															\
		D = Z80->ZetRead(A) | (Z80->ZetRead((A)+1) << 8);					\
	}																		\
}

#define WRITE_MEM8(A, D) {													\
	UINT8 *ptr = Z80->pZetMemMap[0x100 | (A) >> 8];							\
	if ( ptr ) {															\
		ptr -= (A & ~0xff);													\
		ptr[A] = D;															\
	} else {																\
		zPC = PC;															\
		Z80->ZetWrite(A, D);												\
	}																		\
}

#define WRITE_MEM16(A, D) {													\
	UINT8 *ptr = Z80->pZetMemMap[0x100 | (A) >> 8];							\
	if ( ptr ) {															\
		ptr -= (A & ~0xff);													\
		ptr[A] = D;															\
		ptr[(A)+1] = (D) >> 8;												\
	} else {																\
		zPC = PC;															\
		Z80->ZetWrite(A, D);												\
		Z80->ZetWrite((A) + 1, (D) >> 8);									\
	}																		\
}

#define PUSH_16(A) {														\
	UINT8 *ptr = Z80->pZetMemMap[0x100 | (zSP) >> 8];						\
	if (ptr) {																\
		ptr -= (zSP & ~0xff);												\
		zSP -= 2;															\
		ptr[zSP] = A;														\
		ptr[(zSP)+1] = (A) >> 8;											\
	} else {																\
		zSP -= 2;															\
	}																		\
}

#define POP_16(A) {															\
	UINT8 *ptr = Z80->pZetMemMap[0x000 | (zSP) >> 8];						\
	if (ptr) {																\
		ptr -= (zSP & ~0xff);												\
		A = ptr[zSP] | (ptr[(zSP)+1] << 8);									\
	}																		\
	zSP += 2; 																\
}

#define SET_PC(A)															\
	zBasePC = (uintptr_t)Z80->pZetMemMap[0x300 | (A) >> 8];					\
	if (zBasePC == 0) {														\
		Z80->nCyclesLeft = -1;												\
		goto Cz80_Exec_Really_End;											\
	}																		\
	zBasePC -= (A & ~0xff);													\
	PCDiff = PC_DIFF(A);													\
	PC = zBasePC + (uintptr_t)(A);

#define PC_DIFF(A)															\
	(uintptr_t)(Z80->pZetMemMap[0x300 | (A) >> 8]) - 						\
		(uintptr_t)(Z80->pZetMemMap[0x200 | (A) >> 8])

#define Z80_IRQSTATUS_NONE	0x8000
#define Z80_IRQSTATUS_HOLD	0x4000
#define Z80_IRQSTATUS_AUTO	0x2000
#define Z80_IRQSTATUS_ACK	0x1000

static void Z80InitFlags() {
	INT32 i, p;
#if Z80_BIG_FLAGS_ARRAY
	if (!SZHVC_add || !SZHVC_sub) {
		int oldval, newval, val;
		UINT8 *padd, *padc, *psub, *psbc;
		/* allocate big flag arrays once */
		SZHVC_add = (UINT8 *)malloc(2*256*256);
		SZHVC_sub = (UINT8 *)malloc(2*256*256);
		if (!SZHVC_add || !SZHVC_sub) {
//			fatalerror("Z80: failed to allocate 2 * 128K flags arrays!!!");
		}
		padd = &SZHVC_add[	0*256];
		padc = &SZHVC_add[256*256];
		psub = &SZHVC_sub[	0*256];
		psbc = &SZHVC_sub[256*256];
		for (oldval = 0; oldval < 256; oldval++) {
			for (newval = 0; newval < 256; newval++) {
				/* add or adc w/o carry set */
				val = newval - oldval;
				*padd = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
				*padd |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
				if( (newval & 0x0f) < (oldval & 0x0f) ) *padd |= HF;
				if( newval < oldval ) *padd |= CF;
				if( (val^oldval^0x80) & (val^newval) & 0x80 ) *padd |= VF;
				padd++;

				/* adc with carry set */
				val = newval - oldval - 1;
				*padc = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
				*padc |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
				if( (newval & 0x0f) <= (oldval & 0x0f) ) *padc |= HF;
				if( newval <= oldval ) *padc |= CF;
				if( (val^oldval^0x80) & (val^newval) & 0x80 ) *padc |= VF;
				padc++;

				/* cp, sub or sbc w/o carry set */
				val = oldval - newval;
				*psub = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
				*psub |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
				if( (newval & 0x0f) > (oldval & 0x0f) ) *psub |= HF;
				if( newval > oldval ) *psub |= CF;
				if( (val^oldval) & (oldval^newval) & 0x80 ) *psub |= VF;
				psub++;

				/* sbc with carry set */
				val = oldval - newval - 1;
				*psbc = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
				*psbc |= (newval & (YF | XF));	/* undocumented flag bits 5+3 */
				if( (newval & 0x0f) >= (oldval & 0x0f) ) *psbc |= HF;
				if( newval >= oldval ) *psbc |= CF;
				if( (val^oldval) & (oldval^newval) & 0x80 ) *psbc |= VF;
				psbc++;
			}
		}
	}
#endif
	for (i = 0; i < 256; i++) {
		p = 0;
		if (i & 0x01) ++p;
		if (i & 0x02) ++p;
		if (i & 0x04) ++p;
		if (i & 0x08) ++p;
		if (i & 0x10) ++p;
		if (i & 0x20) ++p;
		if (i & 0x40) ++p;
		if (i & 0x80) ++p;
		SZ[i] = i ? i & SF : ZF;
		SZ[i] |= (i & (YF | XF));		/* undocumented flag bits 5+3 */
		SZ_BIT[i] = i ? i & SF : ZF | PF;
		SZ_BIT[i] |= (i & (YF | XF));	/* undocumented flag bits 5+3 */
		SZP[i] = SZ[i] | ((p & 1) ? 0 : PF);
		SZHV_inc[i] = SZ[i];
		if (i == 0x80) SZHV_inc[i] |= VF;
		if ((i & 0x0f) == 0x00) SZHV_inc[i] |= HF;
		SZHV_dec[i] = SZ[i] | NF;
		if (i == 0x7f) SZHV_dec[i] |= VF;
		if ((i & 0x0f) == 0x0f) SZHV_dec[i] |= HF;
	}
}

static void RebasePC(UINT16 address) {
	ZetExt *Z80 = ZetCPUContext[nOpenedCPU];

	zBasePC = (uintptr_t)Z80->pZetMemMap[0x300 | (address) >> 8];
	if (zBasePC == 0)
		return;

	zBasePC -= (address & ~0xff);
	zPC = zBasePC + (uintptr_t)(address);
}

static void Z80Init() {
	ZetExt *Z80 = ZetCPUContext[nOpenedCPU];

	zBasePC = 0;
	zPC = 0;
	zSP = 0;
	zAF = 0;
	zBC = 0;
	zDE = 0;
	zHL = 0;
	zIX = 0;
	zIY = 0;
	zAF2 = 0;
	zBC2 = 0;
	zDE2 = 0;
	zHL2 = 0;
	zR = 0;
	zR2 = 0;
	zIFF1 = 0;
	zIFF2 = 0;
	// zHalt
	// zIM
	// zI
	// NmiState
	// IrqState
	// WaitState
	// BusAckState
	// EA

	zIX = zIY = 0xffff; // IX and IY are FFFF after a reset!
	zF  = ZF; // Zero flag is set

	Z80->nInterruptLatch = -1;

	Z80->nCyclesLeft = 0;

	CPU->pR8[0] = &zB;
	CPU->pR8[1] = &zC;
	CPU->pR8[2] = &zD;
	CPU->pR8[3] = &zE;
	CPU->pR8[4] = &zH;
	CPU->pR8[5] = &zL;
	CPU->pR8[6] = &zF;
	CPU->pR8[7] = &zA;

	CPU->pR16[0] = pzBC;
	CPU->pR16[1] = pzDE;
	CPU->pR16[2] = pzHL;
	CPU->pR16[3] = pzAF;
}

static void Z80Exit() {
	if (SZHVC_add) free(SZHVC_add);
	SZHVC_add = NULL;
	if (SZHVC_sub) free(SZHVC_sub);
	SZHVC_sub = NULL;
}

static void Z80Reset() {
	ZetExt *Z80 = ZetCPUContext[nOpenedCPU];

	RebasePC(0);

	zI = 0;
	zR = 0;
	zR2 = 0;
	zIFF1 = 0;
	zIFF2 = 0;

	Z80->nInterruptLatch = Z80_IRQSTATUS_NONE | 0xff;

	// TODO: nmi state
	// nmi_pending
	// after_ei
	// ldair

	// zIX = zIY = 0xffff; in mame is not set
}

static INT32 Z80Execute(INT32 nCycles) {
	ZetExt *Z80 = ZetCPUContext[nOpenedCPU];

	static const void *JumpTable[0x100] = {
		&&OP0x00, &&OP0x01, &&OP0x02, &&OP0x03,
		&&OP0x04, &&OP0x05, &&OP0x06, &&OP0x07,
		&&OP0x08, &&OP0x09, &&OP0x0a, &&OP0x0b,
		&&OP0x0c, &&OP0x0d, &&OP0x0e, &&OP0x0f,

		&&OP0x10, &&OP0x11, &&OP0x12, &&OP0x13,
		&&OP0x14, &&OP0x15, &&OP0x16, &&OP0x17,
		&&OP0x18, &&OP0x19, &&OP0x1a, &&OP0x1b,
		&&OP0x1c, &&OP0x1d, &&OP0x1e, &&OP0x1f,

		&&OP0x20, &&OP0x21, &&OP0x22, &&OP0x23,
		&&OP0x24, &&OP0x25, &&OP0x26, &&OP0x27,
		&&OP0x28, &&OP0x29, &&OP0x2a, &&OP0x2b,
		&&OP0x2c, &&OP0x2d, &&OP0x2e, &&OP0x2f,

		&&OP0x30, &&OP0x31, &&OP0x32, &&OP0x33,
		&&OP0x34, &&OP0x35, &&OP0x36, &&OP0x37,
		&&OP0x38, &&OP0x39, &&OP0x3a, &&OP0x3b,
		&&OP0x3c, &&OP0x3d, &&OP0x3e, &&OP0x3f,

		&&OP0x40, &&OP0x41, &&OP0x42, &&OP0x43,
		&&OP0x44, &&OP0x45, &&OP0x46, &&OP0x47,
		&&OP0x48, &&OP0x49, &&OP0x4a, &&OP0x4b,
		&&OP0x4c, &&OP0x4d, &&OP0x4e, &&OP0x4f,

		&&OP0x50, &&OP0x51, &&OP0x52, &&OP0x53,
		&&OP0x54, &&OP0x55, &&OP0x56, &&OP0x57,
		&&OP0x58, &&OP0x59, &&OP0x5a, &&OP0x5b,
		&&OP0x5c, &&OP0x5d, &&OP0x5e, &&OP0x5f,

		&&OP0x60, &&OP0x61, &&OP0x62, &&OP0x63,
		&&OP0x64, &&OP0x65, &&OP0x66, &&OP0x67,
		&&OP0x68, &&OP0x69, &&OP0x6a, &&OP0x6b,
		&&OP0x6c, &&OP0x6d, &&OP0x6e, &&OP0x6f,

		&&OP0x70, &&OP0x71, &&OP0x72, &&OP0x73,
		&&OP0x74, &&OP0x75, &&OP0x76, &&OP0x77,
		&&OP0x78, &&OP0x79, &&OP0x7a, &&OP0x7b,
		&&OP0x7c, &&OP0x7d, &&OP0x7e, &&OP0x7f,

		&&OP0x80, &&OP0x81, &&OP0x82, &&OP0x83,
		&&OP0x84, &&OP0x85, &&OP0x86, &&OP0x87,
		&&OP0x88, &&OP0x89, &&OP0x8a, &&OP0x8b,
		&&OP0x8c, &&OP0x8d, &&OP0x8e, &&OP0x8f,

		&&OP0x90, &&OP0x91, &&OP0x92, &&OP0x93,
		&&OP0x94, &&OP0x95, &&OP0x96, &&OP0x97,
		&&OP0x98, &&OP0x99, &&OP0x9a, &&OP0x9b,
		&&OP0x9c, &&OP0x9d, &&OP0x9e, &&OP0x9f,

		&&OP0xa0, &&OP0xa1, &&OP0xa2, &&OP0xa3,
		&&OP0xa4, &&OP0xa5, &&OP0xa6, &&OP0xa7,
		&&OP0xa8, &&OP0xa9, &&OP0xaa, &&OP0xab,
		&&OP0xac, &&OP0xad, &&OP0xae, &&OP0xaf,

		&&OP0xb0, &&OP0xb1, &&OP0xb2, &&OP0xb3,
		&&OP0xb4, &&OP0xb5, &&OP0xb6, &&OP0xb7,
		&&OP0xb8, &&OP0xb9, &&OP0xba, &&OP0xbb,
		&&OP0xbc, &&OP0xbd, &&OP0xbe, &&OP0xbf,

		&&OP0xc0, &&OP0xc1, &&OP0xc2, &&OP0xc3,
		&&OP0xc4, &&OP0xc5, &&OP0xc6, &&OP0xc7,
		&&OP0xc8, &&OP0xc9, &&OP0xca, &&OP0xcb,
		&&OP0xcc, &&OP0xcd, &&OP0xce, &&OP0xcf,

		&&OP0xd0, &&OP0xd1, &&OP0xd2, &&OP0xd3,
		&&OP0xd4, &&OP0xd5, &&OP0xd6, &&OP0xd7,
		&&OP0xd8, &&OP0xd9, &&OP0xda, &&OP0xdb,
		&&OP0xdc, &&OP0xdd, &&OP0xde, &&OP0xdf,

		&&OP0xe0, &&OP0xe1, &&OP0xe2, &&OP0xe3,
		&&OP0xe4, &&OP0xe5, &&OP0xe6, &&OP0xe7,
		&&OP0xe8, &&OP0xe9, &&OP0xea, &&OP0xeb,
		&&OP0xec, &&OP0xed, &&OP0xee, &&OP0xef,

		&&OP0xf0, &&OP0xf1, &&OP0xf2, &&OP0xf3,
		&&OP0xf4, &&OP0xf5, &&OP0xf6, &&OP0xf7,
		&&OP0xf8, &&OP0xf9, &&OP0xfa, &&OP0xfb,
		&&OP0xfc, &&OP0xfd, &&OP0xfe, &&OP0xff
	};

	static const void *JumpTableCB[0x100] = {
		&&OPCB0x00, &&OPCB0x01, &&OPCB0x02, &&OPCB0x03,
		&&OPCB0x04, &&OPCB0x05, &&OPCB0x06, &&OPCB0x07,
		&&OPCB0x08, &&OPCB0x09, &&OPCB0x0a, &&OPCB0x0b,
		&&OPCB0x0c, &&OPCB0x0d, &&OPCB0x0e, &&OPCB0x0f,

		&&OPCB0x10, &&OPCB0x11, &&OPCB0x12, &&OPCB0x13,
		&&OPCB0x14, &&OPCB0x15, &&OPCB0x16, &&OPCB0x17,
		&&OPCB0x18, &&OPCB0x19, &&OPCB0x1a, &&OPCB0x1b,
		&&OPCB0x1c, &&OPCB0x1d, &&OPCB0x1e, &&OPCB0x1f,

		&&OPCB0x20, &&OPCB0x21, &&OPCB0x22, &&OPCB0x23,
		&&OPCB0x24, &&OPCB0x25, &&OPCB0x26, &&OPCB0x27,
		&&OPCB0x28, &&OPCB0x29, &&OPCB0x2a, &&OPCB0x2b,
		&&OPCB0x2c, &&OPCB0x2d, &&OPCB0x2e, &&OPCB0x2f,

		&&OPCB0x30, &&OPCB0x31, &&OPCB0x32, &&OPCB0x33,
		&&OPCB0x34, &&OPCB0x35, &&OPCB0x36, &&OPCB0x37,
		&&OPCB0x38, &&OPCB0x39, &&OPCB0x3a, &&OPCB0x3b,
		&&OPCB0x3c, &&OPCB0x3d, &&OPCB0x3e, &&OPCB0x3f,

		&&OPCB0x40, &&OPCB0x41, &&OPCB0x42, &&OPCB0x43,
		&&OPCB0x44, &&OPCB0x45, &&OPCB0x46, &&OPCB0x47,
		&&OPCB0x48, &&OPCB0x49, &&OPCB0x4a, &&OPCB0x4b,
		&&OPCB0x4c, &&OPCB0x4d, &&OPCB0x4e, &&OPCB0x4f,

		&&OPCB0x50, &&OPCB0x51, &&OPCB0x52, &&OPCB0x53,
		&&OPCB0x54, &&OPCB0x55, &&OPCB0x56, &&OPCB0x57,
		&&OPCB0x58, &&OPCB0x59, &&OPCB0x5a, &&OPCB0x5b,
		&&OPCB0x5c, &&OPCB0x5d, &&OPCB0x5e, &&OPCB0x5f,

		&&OPCB0x60, &&OPCB0x61, &&OPCB0x62, &&OPCB0x63,
		&&OPCB0x64, &&OPCB0x65, &&OPCB0x66, &&OPCB0x67,
		&&OPCB0x68, &&OPCB0x69, &&OPCB0x6a, &&OPCB0x6b,
		&&OPCB0x6c, &&OPCB0x6d, &&OPCB0x6e, &&OPCB0x6f,

		&&OPCB0x70, &&OPCB0x71, &&OPCB0x72, &&OPCB0x73,
		&&OPCB0x74, &&OPCB0x75, &&OPCB0x76, &&OPCB0x77,
		&&OPCB0x78, &&OPCB0x79, &&OPCB0x7a, &&OPCB0x7b,
		&&OPCB0x7c, &&OPCB0x7d, &&OPCB0x7e, &&OPCB0x7f,

		&&OPCB0x80, &&OPCB0x81, &&OPCB0x82, &&OPCB0x83,
		&&OPCB0x84, &&OPCB0x85, &&OPCB0x86, &&OPCB0x87,
		&&OPCB0x88, &&OPCB0x89, &&OPCB0x8a, &&OPCB0x8b,
		&&OPCB0x8c, &&OPCB0x8d, &&OPCB0x8e, &&OPCB0x8f,

		&&OPCB0x90, &&OPCB0x91, &&OPCB0x92, &&OPCB0x93,
		&&OPCB0x94, &&OPCB0x95, &&OPCB0x96, &&OPCB0x97,
		&&OPCB0x98, &&OPCB0x99, &&OPCB0x9a, &&OPCB0x9b,
		&&OPCB0x9c, &&OPCB0x9d, &&OPCB0x9e, &&OPCB0x9f,

		&&OPCB0xa0, &&OPCB0xa1, &&OPCB0xa2, &&OPCB0xa3,
		&&OPCB0xa4, &&OPCB0xa5, &&OPCB0xa6, &&OPCB0xa7,
		&&OPCB0xa8, &&OPCB0xa9, &&OPCB0xaa, &&OPCB0xab,
		&&OPCB0xac, &&OPCB0xad, &&OPCB0xae, &&OPCB0xaf,

		&&OPCB0xb0, &&OPCB0xb1, &&OPCB0xb2, &&OPCB0xb3,
		&&OPCB0xb4, &&OPCB0xb5, &&OPCB0xb6, &&OPCB0xb7,
		&&OPCB0xb8, &&OPCB0xb9, &&OPCB0xba, &&OPCB0xbb,
		&&OPCB0xbc, &&OPCB0xbd, &&OPCB0xbe, &&OPCB0xbf,

		&&OPCB0xc0, &&OPCB0xc1, &&OPCB0xc2, &&OPCB0xc3,
		&&OPCB0xc4, &&OPCB0xc5, &&OPCB0xc6, &&OPCB0xc7,
		&&OPCB0xc8, &&OPCB0xc9, &&OPCB0xca, &&OPCB0xcb,
		&&OPCB0xcc, &&OPCB0xcd, &&OPCB0xce, &&OPCB0xcf,

		&&OPCB0xd0, &&OPCB0xd1, &&OPCB0xd2, &&OPCB0xd3,
		&&OPCB0xd4, &&OPCB0xd5, &&OPCB0xd6, &&OPCB0xd7,
		&&OPCB0xd8, &&OPCB0xd9, &&OPCB0xda, &&OPCB0xdb,
		&&OPCB0xdc, &&OPCB0xdd, &&OPCB0xde, &&OPCB0xdf,

		&&OPCB0xe0, &&OPCB0xe1, &&OPCB0xe2, &&OPCB0xe3,
		&&OPCB0xe4, &&OPCB0xe5, &&OPCB0xe6, &&OPCB0xe7,
		&&OPCB0xe8, &&OPCB0xe9, &&OPCB0xea, &&OPCB0xeb,
		&&OPCB0xec, &&OPCB0xed, &&OPCB0xee, &&OPCB0xef,

		&&OPCB0xf0, &&OPCB0xf1, &&OPCB0xf2, &&OPCB0xf3,
		&&OPCB0xf4, &&OPCB0xf5, &&OPCB0xf6, &&OPCB0xf7,
		&&OPCB0xf8, &&OPCB0xf9, &&OPCB0xfa, &&OPCB0xfb,
		&&OPCB0xfc, &&OPCB0xfd, &&OPCB0xfe, &&OPCB0xff
	};

	static const void *JumpTableED[0x100] = {
		&&OPED0x00, &&OPED0x01, &&OPED0x02, &&OPED0x03,
		&&OPED0x04, &&OPED0x05, &&OPED0x06, &&OPED0x07,
		&&OPED0x08, &&OPED0x09, &&OPED0x0a, &&OPED0x0b,
		&&OPED0x0c, &&OPED0x0d, &&OPED0x0e, &&OPED0x0f,

		&&OPED0x10, &&OPED0x11, &&OPED0x12, &&OPED0x13,
		&&OPED0x14, &&OPED0x15, &&OPED0x16, &&OPED0x17,
		&&OPED0x18, &&OPED0x19, &&OPED0x1a, &&OPED0x1b,
		&&OPED0x1c, &&OPED0x1d, &&OPED0x1e, &&OPED0x1f,

		&&OPED0x20, &&OPED0x21, &&OPED0x22, &&OPED0x23,
		&&OPED0x24, &&OPED0x25, &&OPED0x26, &&OPED0x27,
		&&OPED0x28, &&OPED0x29, &&OPED0x2a, &&OPED0x2b,
		&&OPED0x2c, &&OPED0x2d, &&OPED0x2e, &&OPED0x2f,

		&&OPED0x30, &&OPED0x31, &&OPED0x32, &&OPED0x33,
		&&OPED0x34, &&OPED0x35, &&OPED0x36, &&OPED0x37,
		&&OPED0x38, &&OPED0x39, &&OPED0x3a, &&OPED0x3b,
		&&OPED0x3c, &&OPED0x3d, &&OPED0x3e, &&OPED0x3f,

		&&OPED0x40, &&OPED0x41, &&OPED0x42, &&OPED0x43,
		&&OPED0x44, &&OPED0x45, &&OPED0x46, &&OPED0x47,
		&&OPED0x48, &&OPED0x49, &&OPED0x4a, &&OPED0x4b,
		&&OPED0x4c, &&OPED0x4d, &&OPED0x4e, &&OPED0x4f,

		&&OPED0x50, &&OPED0x51, &&OPED0x52, &&OPED0x53,
		&&OPED0x54, &&OPED0x55, &&OPED0x56, &&OPED0x57,
		&&OPED0x58, &&OPED0x59, &&OPED0x5a, &&OPED0x5b,
		&&OPED0x5c, &&OPED0x5d, &&OPED0x5e, &&OPED0x5f,

		&&OPED0x60, &&OPED0x61, &&OPED0x62, &&OPED0x63,
		&&OPED0x64, &&OPED0x65, &&OPED0x66, &&OPED0x67,
		&&OPED0x68, &&OPED0x69, &&OPED0x6a, &&OPED0x6b,
		&&OPED0x6c, &&OPED0x6d, &&OPED0x6e, &&OPED0x6f,

		&&OPED0x70, &&OPED0x71, &&OPED0x72, &&OPED0x73,
		&&OPED0x74, &&OPED0x75, &&OPED0x76, &&OPED0x77,
		&&OPED0x78, &&OPED0x79, &&OPED0x7a, &&OPED0x7b,
		&&OPED0x7c, &&OPED0x7d, &&OPED0x7e, &&OPED0x7f,

		&&OPED0x80, &&OPED0x81, &&OPED0x82, &&OPED0x83,
		&&OPED0x84, &&OPED0x85, &&OPED0x86, &&OPED0x87,
		&&OPED0x88, &&OPED0x89, &&OPED0x8a, &&OPED0x8b,
		&&OPED0x8c, &&OPED0x8d, &&OPED0x8e, &&OPED0x8f,

		&&OPED0x90, &&OPED0x91, &&OPED0x92, &&OPED0x93,
		&&OPED0x94, &&OPED0x95, &&OPED0x96, &&OPED0x97,
		&&OPED0x98, &&OPED0x99, &&OPED0x9a, &&OPED0x9b,
		&&OPED0x9c, &&OPED0x9d, &&OPED0x9e, &&OPED0x9f,

		&&OPED0xa0, &&OPED0xa1, &&OPED0xa2, &&OPED0xa3,
		&&OPED0xa4, &&OPED0xa5, &&OPED0xa6, &&OPED0xa7,
		&&OPED0xa8, &&OPED0xa9, &&OPED0xaa, &&OPED0xab,
		&&OPED0xac, &&OPED0xad, &&OPED0xae, &&OPED0xaf,

		&&OPED0xb0, &&OPED0xb1, &&OPED0xb2, &&OPED0xb3,
		&&OPED0xb4, &&OPED0xb5, &&OPED0xb6, &&OPED0xb7,
		&&OPED0xb8, &&OPED0xb9, &&OPED0xba, &&OPED0xbb,
		&&OPED0xbc, &&OPED0xbd, &&OPED0xbe, &&OPED0xbf,

		&&OPED0xc0, &&OPED0xc1, &&OPED0xc2, &&OPED0xc3,
		&&OPED0xc4, &&OPED0xc5, &&OPED0xc6, &&OPED0xc7,
		&&OPED0xc8, &&OPED0xc9, &&OPED0xca, &&OPED0xcb,
		&&OPED0xcc, &&OPED0xcd, &&OPED0xce, &&OPED0xcf,

		&&OPED0xd0, &&OPED0xd1, &&OPED0xd2, &&OPED0xd3,
		&&OPED0xd4, &&OPED0xd5, &&OPED0xd6, &&OPED0xd7,
		&&OPED0xd8, &&OPED0xd9, &&OPED0xda, &&OPED0xdb,
		&&OPED0xdc, &&OPED0xdd, &&OPED0xde, &&OPED0xdf,

		&&OPED0xe0, &&OPED0xe1, &&OPED0xe2, &&OPED0xe3,
		&&OPED0xe4, &&OPED0xe5, &&OPED0xe6, &&OPED0xe7,
		&&OPED0xe8, &&OPED0xe9, &&OPED0xea, &&OPED0xeb,
		&&OPED0xec, &&OPED0xed, &&OPED0xee, &&OPED0xef,

		&&OPED0xf0, &&OPED0xf1, &&OPED0xf2, &&OPED0xf3,
		&&OPED0xf4, &&OPED0xf5, &&OPED0xf6, &&OPED0xf7,
		&&OPED0xf8, &&OPED0xf9, &&OPED0xfa, &&OPED0xfb,
		&&OPED0xfc, &&OPED0xfd, &&OPED0xfe, &&OPED0xff
	};

	static const void *JumpTableXY[0x100] = {
		&&OPXY0x00, &&OPXY0x01, &&OPXY0x02, &&OPXY0x03,
		&&OPXY0x04, &&OPXY0x05, &&OPXY0x06, &&OPXY0x07,
		&&OPXY0x08, &&OPXY0x09, &&OPXY0x0a, &&OPXY0x0b,
		&&OPXY0x0c, &&OPXY0x0d, &&OPXY0x0e, &&OPXY0x0f,

		&&OPXY0x10, &&OPXY0x11, &&OPXY0x12, &&OPXY0x13,
		&&OPXY0x14, &&OPXY0x15, &&OPXY0x16, &&OPXY0x17,
		&&OPXY0x18, &&OPXY0x19, &&OPXY0x1a, &&OPXY0x1b,
		&&OPXY0x1c, &&OPXY0x1d, &&OPXY0x1e, &&OPXY0x1f,

		&&OPXY0x20, &&OPXY0x21, &&OPXY0x22, &&OPXY0x23,
		&&OPXY0x24, &&OPXY0x25, &&OPXY0x26, &&OPXY0x27,
		&&OPXY0x28, &&OPXY0x29, &&OPXY0x2a, &&OPXY0x2b,
		&&OPXY0x2c, &&OPXY0x2d, &&OPXY0x2e, &&OPXY0x2f,

		&&OPXY0x30, &&OPXY0x31, &&OPXY0x32, &&OPXY0x33,
		&&OPXY0x34, &&OPXY0x35, &&OPXY0x36, &&OPXY0x37,
		&&OPXY0x38, &&OPXY0x39, &&OPXY0x3a, &&OPXY0x3b,
		&&OPXY0x3c, &&OPXY0x3d, &&OPXY0x3e, &&OPXY0x3f,

		&&OPXY0x40, &&OPXY0x41, &&OPXY0x42, &&OPXY0x43,
		&&OPXY0x44, &&OPXY0x45, &&OPXY0x46, &&OPXY0x47,
		&&OPXY0x48, &&OPXY0x49, &&OPXY0x4a, &&OPXY0x4b,
		&&OPXY0x4c, &&OPXY0x4d, &&OPXY0x4e, &&OPXY0x4f,

		&&OPXY0x50, &&OPXY0x51, &&OPXY0x52, &&OPXY0x53,
		&&OPXY0x54, &&OPXY0x55, &&OPXY0x56, &&OPXY0x57,
		&&OPXY0x58, &&OPXY0x59, &&OPXY0x5a, &&OPXY0x5b,
		&&OPXY0x5c, &&OPXY0x5d, &&OPXY0x5e, &&OPXY0x5f,

		&&OPXY0x60, &&OPXY0x61, &&OPXY0x62, &&OPXY0x63,
		&&OPXY0x64, &&OPXY0x65, &&OPXY0x66, &&OPXY0x67,
		&&OPXY0x68, &&OPXY0x69, &&OPXY0x6a, &&OPXY0x6b,
		&&OPXY0x6c, &&OPXY0x6d, &&OPXY0x6e, &&OPXY0x6f,

		&&OPXY0x70, &&OPXY0x71, &&OPXY0x72, &&OPXY0x73,
		&&OPXY0x74, &&OPXY0x75, &&OPXY0x76, &&OPXY0x77,
		&&OPXY0x78, &&OPXY0x79, &&OPXY0x7a, &&OPXY0x7b,
		&&OPXY0x7c, &&OPXY0x7d, &&OPXY0x7e, &&OPXY0x7f,

		&&OPXY0x80, &&OPXY0x81, &&OPXY0x82, &&OPXY0x83,
		&&OPXY0x84, &&OPXY0x85, &&OPXY0x86, &&OPXY0x87,
		&&OPXY0x88, &&OPXY0x89, &&OPXY0x8a, &&OPXY0x8b,
		&&OPXY0x8c, &&OPXY0x8d, &&OPXY0x8e, &&OPXY0x8f,

		&&OPXY0x90, &&OPXY0x91, &&OPXY0x92, &&OPXY0x93,
		&&OPXY0x94, &&OPXY0x95, &&OPXY0x96, &&OPXY0x97,
		&&OPXY0x98, &&OPXY0x99, &&OPXY0x9a, &&OPXY0x9b,
		&&OPXY0x9c, &&OPXY0x9d, &&OPXY0x9e, &&OPXY0x9f,

		&&OPXY0xa0, &&OPXY0xa1, &&OPXY0xa2, &&OPXY0xa3,
		&&OPXY0xa4, &&OPXY0xa5, &&OPXY0xa6, &&OPXY0xa7,
		&&OPXY0xa8, &&OPXY0xa9, &&OPXY0xaa, &&OPXY0xab,
		&&OPXY0xac, &&OPXY0xad, &&OPXY0xae, &&OPXY0xaf,

		&&OPXY0xb0, &&OPXY0xb1, &&OPXY0xb2, &&OPXY0xb3,
		&&OPXY0xb4, &&OPXY0xb5, &&OPXY0xb6, &&OPXY0xb7,
		&&OPXY0xb8, &&OPXY0xb9, &&OPXY0xba, &&OPXY0xbb,
		&&OPXY0xbc, &&OPXY0xbd, &&OPXY0xbe, &&OPXY0xbf,

		&&OPXY0xc0, &&OPXY0xc1, &&OPXY0xc2, &&OPXY0xc3,
		&&OPXY0xc4, &&OPXY0xc5, &&OPXY0xc6, &&OPXY0xc7,
		&&OPXY0xc8, &&OPXY0xc9, &&OPXY0xca, &&OPXY0xcb,
		&&OPXY0xcc, &&OPXY0xcd, &&OPXY0xce, &&OPXY0xcf,

		&&OPXY0xd0, &&OPXY0xd1, &&OPXY0xd2, &&OPXY0xd3,
		&&OPXY0xd4, &&OPXY0xd5, &&OPXY0xd6, &&OPXY0xd7,
		&&OPXY0xd8, &&OPXY0xd9, &&OPXY0xda, &&OPXY0xdb,
		&&OPXY0xdc, &&OPXY0xdd, &&OPXY0xde, &&OPXY0xdf,

		&&OPXY0xe0, &&OPXY0xe1, &&OPXY0xe2, &&OPXY0xe3,
		&&OPXY0xe4, &&OPXY0xe5, &&OPXY0xe6, &&OPXY0xe7,
		&&OPXY0xe8, &&OPXY0xe9, &&OPXY0xea, &&OPXY0xeb,
		&&OPXY0xec, &&OPXY0xed, &&OPXY0xee, &&OPXY0xef,

		&&OPXY0xf0, &&OPXY0xf1, &&OPXY0xf2, &&OPXY0xf3,
		&&OPXY0xf4, &&OPXY0xf5, &&OPXY0xf6, &&OPXY0xf7,
		&&OPXY0xf8, &&OPXY0xf9, &&OPXY0xfa, &&OPXY0xfb,
		&&OPXY0xfc, &&OPXY0xfd, &&OPXY0xfe, &&OPXY0xff
	};

	static const void *JumpTableXYCB[0x100] = {
		&&OPXYCB0x00, &&OPXYCB0x01, &&OPXYCB0x02, &&OPXYCB0x03,
		&&OPXYCB0x04, &&OPXYCB0x05, &&OPXYCB0x06, &&OPXYCB0x07,
		&&OPXYCB0x08, &&OPXYCB0x09, &&OPXYCB0x0a, &&OPXYCB0x0b,
		&&OPXYCB0x0c, &&OPXYCB0x0d, &&OPXYCB0x0e, &&OPXYCB0x0f,

		&&OPXYCB0x10, &&OPXYCB0x11, &&OPXYCB0x12, &&OPXYCB0x13,
		&&OPXYCB0x14, &&OPXYCB0x15, &&OPXYCB0x16, &&OPXYCB0x17,
		&&OPXYCB0x18, &&OPXYCB0x19, &&OPXYCB0x1a, &&OPXYCB0x1b,
		&&OPXYCB0x1c, &&OPXYCB0x1d, &&OPXYCB0x1e, &&OPXYCB0x1f,

		&&OPXYCB0x20, &&OPXYCB0x21, &&OPXYCB0x22, &&OPXYCB0x23,
		&&OPXYCB0x24, &&OPXYCB0x25, &&OPXYCB0x26, &&OPXYCB0x27,
		&&OPXYCB0x28, &&OPXYCB0x29, &&OPXYCB0x2a, &&OPXYCB0x2b,
		&&OPXYCB0x2c, &&OPXYCB0x2d, &&OPXYCB0x2e, &&OPXYCB0x2f,

		&&OPXYCB0x30, &&OPXYCB0x31, &&OPXYCB0x32, &&OPXYCB0x33,
		&&OPXYCB0x34, &&OPXYCB0x35, &&OPXYCB0x36, &&OPXYCB0x37,
		&&OPXYCB0x38, &&OPXYCB0x39, &&OPXYCB0x3a, &&OPXYCB0x3b,
		&&OPXYCB0x3c, &&OPXYCB0x3d, &&OPXYCB0x3e, &&OPXYCB0x3f,

		&&OPXYCB0x40, &&OPXYCB0x41, &&OPXYCB0x42, &&OPXYCB0x43,
		&&OPXYCB0x44, &&OPXYCB0x45, &&OPXYCB0x46, &&OPXYCB0x47,
		&&OPXYCB0x48, &&OPXYCB0x49, &&OPXYCB0x4a, &&OPXYCB0x4b,
		&&OPXYCB0x4c, &&OPXYCB0x4d, &&OPXYCB0x4e, &&OPXYCB0x4f,

		&&OPXYCB0x50, &&OPXYCB0x51, &&OPXYCB0x52, &&OPXYCB0x53,
		&&OPXYCB0x54, &&OPXYCB0x55, &&OPXYCB0x56, &&OPXYCB0x57,
		&&OPXYCB0x58, &&OPXYCB0x59, &&OPXYCB0x5a, &&OPXYCB0x5b,
		&&OPXYCB0x5c, &&OPXYCB0x5d, &&OPXYCB0x5e, &&OPXYCB0x5f,

		&&OPXYCB0x60, &&OPXYCB0x61, &&OPXYCB0x62, &&OPXYCB0x63,
		&&OPXYCB0x64, &&OPXYCB0x65, &&OPXYCB0x66, &&OPXYCB0x67,
		&&OPXYCB0x68, &&OPXYCB0x69, &&OPXYCB0x6a, &&OPXYCB0x6b,
		&&OPXYCB0x6c, &&OPXYCB0x6d, &&OPXYCB0x6e, &&OPXYCB0x6f,

		&&OPXYCB0x70, &&OPXYCB0x71, &&OPXYCB0x72, &&OPXYCB0x73,
		&&OPXYCB0x74, &&OPXYCB0x75, &&OPXYCB0x76, &&OPXYCB0x77,
		&&OPXYCB0x78, &&OPXYCB0x79, &&OPXYCB0x7a, &&OPXYCB0x7b,
		&&OPXYCB0x7c, &&OPXYCB0x7d, &&OPXYCB0x7e, &&OPXYCB0x7f,

		&&OPXYCB0x80, &&OPXYCB0x81, &&OPXYCB0x82, &&OPXYCB0x83,
		&&OPXYCB0x84, &&OPXYCB0x85, &&OPXYCB0x86, &&OPXYCB0x87,
		&&OPXYCB0x88, &&OPXYCB0x89, &&OPXYCB0x8a, &&OPXYCB0x8b,
		&&OPXYCB0x8c, &&OPXYCB0x8d, &&OPXYCB0x8e, &&OPXYCB0x8f,

		&&OPXYCB0x90, &&OPXYCB0x91, &&OPXYCB0x92, &&OPXYCB0x93,
		&&OPXYCB0x94, &&OPXYCB0x95, &&OPXYCB0x96, &&OPXYCB0x97,
		&&OPXYCB0x98, &&OPXYCB0x99, &&OPXYCB0x9a, &&OPXYCB0x9b,
		&&OPXYCB0x9c, &&OPXYCB0x9d, &&OPXYCB0x9e, &&OPXYCB0x9f,

		&&OPXYCB0xa0, &&OPXYCB0xa1, &&OPXYCB0xa2, &&OPXYCB0xa3,
		&&OPXYCB0xa4, &&OPXYCB0xa5, &&OPXYCB0xa6, &&OPXYCB0xa7,
		&&OPXYCB0xa8, &&OPXYCB0xa9, &&OPXYCB0xaa, &&OPXYCB0xab,
		&&OPXYCB0xac, &&OPXYCB0xad, &&OPXYCB0xae, &&OPXYCB0xaf,

		&&OPXYCB0xb0, &&OPXYCB0xb1, &&OPXYCB0xb2, &&OPXYCB0xb3,
		&&OPXYCB0xb4, &&OPXYCB0xb5, &&OPXYCB0xb6, &&OPXYCB0xb7,
		&&OPXYCB0xb8, &&OPXYCB0xb9, &&OPXYCB0xba, &&OPXYCB0xbb,
		&&OPXYCB0xbc, &&OPXYCB0xbd, &&OPXYCB0xbe, &&OPXYCB0xbf,

		&&OPXYCB0xc0, &&OPXYCB0xc1, &&OPXYCB0xc2, &&OPXYCB0xc3,
		&&OPXYCB0xc4, &&OPXYCB0xc5, &&OPXYCB0xc6, &&OPXYCB0xc7,
		&&OPXYCB0xc8, &&OPXYCB0xc9, &&OPXYCB0xca, &&OPXYCB0xcb,
		&&OPXYCB0xcc, &&OPXYCB0xcd, &&OPXYCB0xce, &&OPXYCB0xcf,

		&&OPXYCB0xd0, &&OPXYCB0xd1, &&OPXYCB0xd2, &&OPXYCB0xd3,
		&&OPXYCB0xd4, &&OPXYCB0xd5, &&OPXYCB0xd6, &&OPXYCB0xd7,
		&&OPXYCB0xd8, &&OPXYCB0xd9, &&OPXYCB0xda, &&OPXYCB0xdb,
		&&OPXYCB0xdc, &&OPXYCB0xdd, &&OPXYCB0xde, &&OPXYCB0xdf,

		&&OPXYCB0xe0, &&OPXYCB0xe1, &&OPXYCB0xe2, &&OPXYCB0xe3,
		&&OPXYCB0xe4, &&OPXYCB0xe5, &&OPXYCB0xe6, &&OPXYCB0xe7,
		&&OPXYCB0xe8, &&OPXYCB0xe9, &&OPXYCB0xea, &&OPXYCB0xeb,
		&&OPXYCB0xec, &&OPXYCB0xed, &&OPXYCB0xee, &&OPXYCB0xef,

		&&OPXYCB0xf0, &&OPXYCB0xf1, &&OPXYCB0xf2, &&OPXYCB0xf3,
		&&OPXYCB0xf4, &&OPXYCB0xf5, &&OPXYCB0xf6, &&OPXYCB0xf7,
		&&OPXYCB0xf8, &&OPXYCB0xf9, &&OPXYCB0xfa, &&OPXYCB0xfb,
		&&OPXYCB0xfc, &&OPXYCB0xfd, &&OPXYCB0xfe, &&OPXYCB0xff
	};

	uintptr_t PC;
	uintptr_t PCDiff;
	intptr_t offset;

	UINT32 Opcode;

	UINT32 adr = 0;
	UINT32 res;
	UINT32 val;

	INT32 nTodo = 0;

	PC = zPC;
	PCDiff = PC_DIFF(zRealPC);

	Z80->bStopRun = 0;
	Z80->nCyclesSegment = Z80->nCyclesLeft = nCycles;
	//	Z80->nEI = 0;

	goto Cz80_Try_Int;

Cz80_Exec:
	{
		Reg16 *data = pzHL;
		Opcode = READ_OP();

		// printf("op:%02x, PC:%04x, A:%02x, F:%02x, BC:%04x, DE:%04x, HL:%04x, ICount: %d, irq_vector: %02x\n",
		// 	Opcode, PC - zBasePC, zA, zF, zBC, zDE, zHL, Z80->nCyclesLeft,
		// 	Z80->nInterruptLatch & 0xff
		// );

		goto *JumpTable[Opcode];
		// NOP
		OP(0x00):   // NOP
		// LD r8 (same register)
		OP(0x40):   // LD   B,B
		OP(0x49):   // LD   C,C
		OP(0x52):   // LD   D,D
		OP(0x5b):   // LD   E,E
		OP(0x64):   // LD   H,H
		OP(0x6d):   // LD   L,L
		OP(0x7f):   // LD   A,A
			RET(4)

		// LD r8
		OP(0x41):   // LD   B,C
		OP(0x42):   // LD   B,D
		OP(0x43):   // LD   B,E
		OP(0x44):   // LD   B,H
		OP(0x45):   // LD   B,L
		OP(0x47):   // LD   B,A

		OP(0x48):   // LD   C,B
		OP(0x4a):   // LD   C,D
		OP(0x4b):   // LD   C,E
		OP(0x4c):   // LD   C,H
		OP(0x4d):   // LD   C,L
		OP(0x4f):   // LD   C,A

		OP(0x50):   // LD   D,B
		OP(0x51):   // LD   D,C
		OP(0x53):   // LD   D,E
		OP(0x54):   // LD   D,H
		OP(0x55):   // LD   D,L
		OP(0x57):   // LD   D,A

		OP(0x58):   // LD   E,B
		OP(0x59):   // LD   E,C
		OP(0x5a):   // LD   E,D
		OP(0x5c):   // LD   E,H
		OP(0x5d):   // LD   E,L
		OP(0x5f):   // LD   E,A

		OP(0x60):   // LD   H,B
		OP(0x61):   // LD   H,C
		OP(0x62):   // LD   H,D
		OP(0x63):   // LD   H,E
		OP(0x65):   // LD   H,L
		OP(0x67):   // LD   H,A

		OP(0x68):   // LD   L,B
		OP(0x69):   // LD   L,C
		OP(0x6a):   // LD   L,D
		OP(0x6b):   // LD   L,E
		OP(0x6c):   // LD   L,H
		OP(0x6f):   // LD   L,A

		OP(0x78):   // LD   A,B
		OP(0x79):   // LD   A,C
		OP(0x7a):   // LD   A,D
		OP(0x7b):   // LD   A,E
		OP(0x7c):   // LD   A,H
		OP(0x7d):   // LD   A,L
		OP_LD_R_R:
			zR8((Opcode >> 3) & 7) = zR8(Opcode & 7);
			RET(4)

		OP(0x06):   // LD   B,#imm
		OP(0x0e):   // LD   C,#imm
		OP(0x16):   // LD   D,#imm
		OP(0x1e):   // LD   E,#imm
		OP(0x26):   // LD   H,#imm
		OP(0x2e):   // LD   L,#imm
		OP(0x3e):   // LD   A,#imm
		OP_LD_R_imm:
			zR8(Opcode >> 3) = ARG();
			RET(7)

		OP(0x46):   // LD   B,(HL)
		OP(0x4e):   // LD   C,(HL)
		OP(0x56):   // LD   D,(HL)
		OP(0x5e):   // LD   E,(HL)
		OP(0x66):   // LD   H,(HL)
		OP(0x6e):   // LD   L,(HL)
		OP(0x7e):   // LD   A,(HL)
			READ_MEM8(zHL, zR8((Opcode >> 3) & 7));
			RET(7)

		OP(0x70):   // LD   (HL),B
		OP(0x71):   // LD   (HL),C
		OP(0x72):   // LD   (HL),D
		OP(0x73):   // LD   (HL),E
		OP(0x74):   // LD   (HL),H
		OP(0x75):   // LD   (HL),L
		OP(0x77):   // LD   (HL),A
			WRITE_MEM8(zHL, zR8(Opcode & 7));
			RET(7)

		OP(0x36):   // LD (HL), #imm
			//WRITE_MEM8(zHL, ARG());
			res = ARG();
			WRITE_MEM8(zHL, res);
			RET(10)

		OP(0x0a):   // LD   A,(BC)
		OP_LOAD_A_mBC:
			adr = zBC;
			goto OP_LOAD_A_mxx;

		OP(0x1a):   // LD   A,(DE)
		OP_LOAD_A_mDE:
			adr = zDE;

		OP_LOAD_A_mxx:
			READ_MEM8(adr, zA);
			RET(7)

		OP(0x3a):   // LD   A,(nn)
		OP_LOAD_A_mNN:
			adr = ARG16();
			READ_MEM8(adr, zA);
			RET(13)

		OP(0x02):   // LD   (BC),A
		OP_LOAD_mBC_A:
			adr = zBC;
			goto OP_LOAD_mxx_A;

		OP(0x12):   // LD   (DE),A
		OP_LOAD_mDE_A:
			adr = zDE;

		OP_LOAD_mxx_A:
			WRITE_MEM8(adr, zA);
			RET(7)

		OP(0x32):   // LD   (nn),A
		OP_LOAD_mNN_A:
			adr = ARG16();
			WRITE_MEM8(adr, zA);
			RET(13)

		OP(0x01):   // LD   BC,nn
		OP(0x11):   // LD   DE,nn
		OP(0x21):   // LD   HL,nn
		OP_LOAD_RR_imm16:
			zR16(Opcode >> 4) = ARG16();
			RET(10)

		OP(0x31):   // LD   SP,nn
		OP_LOAD_SP_imm16:
			zSP = ARG16();
			RET(10)

		OP(0xf9):   // LD   SP,HL
		OP_LD_SP_xx:
			zSP = data->w;
			RET(6)

		OP(0x2a):   // LD   HL,(nn)
		OP_LD_xx_mNN:
			adr = ARG16();
			READ_MEM16(adr, data->w);
			RET(16)

		OP(0x22):   // LD   (nn),HL
		OP_LD_mNN_xx:
			adr = ARG16();
			WRITE_MEM16(adr, data->w);
			RET(16)

		OP(0xc1):   // POP  BC
		OP(0xd1):   // POP  DE
		OP(0xf1):   // POP  AF
		OP_POP_RR:
			data = pzR16((Opcode >> 4) & 3);

		OP(0xe1):   // POP  HL
		OP_POP:
			POP_16(data->w)
			RET(10)

		OP(0xc5):   // PUSH BC
		OP(0xd5):   // PUSH DE
		OP(0xf5):   // PUSH AF
		OP_PUSH_RR:
			data = pzR16((Opcode >> 4) & 3);

		OP(0xe5):   // PUSH HL
		OP_PUSH:
			PUSH_16(data->w);
			RET(11)

		OP(0x08):   // EX   AF,AF'
		OP_EX_AF_AF2:
			res = zAF;
			zAF = zAF2;
			zAF2 = res;
			RET(4)

		OP(0xeb):   // EX   DE,HL
		OP_EX_DE_HL:
			res = zDE;
			zDE = zHL;
			zHL = res;
			RET(4)

		OP(0xd9):   // EXX
		OP_EXX:
			res = zBC;
			zBC = zBC2;
			zBC2 = res;
			res = zDE;
			zDE = zDE2;
			zDE2 = res;
			res = zHL;
			zHL = zHL2;
			zHL2 = res;
			RET(4)

		OP(0xe3):   // EX   HL,(SP)
		OP_EX_xx_mSP:
			adr = zSP;
			res = data->w;
			READ_MEM16(adr, data->w);
			WRITE_MEM16(adr, res);
			RET(19)

		OP(0x04):   // INC  B
		OP(0x0c):   // INC  C
		OP(0x14):   // INC  D
		OP(0x1c):   // INC  E
		OP(0x24):   // INC  H
		OP(0x2c):   // INC  L
		OP(0x3c):   // INC  A
		OP_INC_R:
			zR8(Opcode >> 3)++;
			zF = (zF & CF) | SZHV_inc[zR8(Opcode >> 3)];
			RET(4)

		OP(0x34):   // INC  (HL)
			adr = zHL;

		OP_INC_m:
			READ_MEM8(adr, res);
			res = (res + 1) & 0xff;
			zF = (zF & CF) | SZHV_inc[res];
			WRITE_MEM8(adr, res);
			RET(11)

		OP(0x05):   // DEC  B
		OP(0x0d):   // DEC  C
		OP(0x15):   // DEC  D
		OP(0x1d):   // DEC  E
		OP(0x25):   // DEC  H
		OP(0x2d):   // DEC  L
		OP(0x3d):   // DEC  A
		OP_DEC_R:
			zR8(Opcode >> 3)--;
			zF = (zF & CF) | SZHV_dec[zR8(Opcode >> 3)];
			RET(4)

			OP(0x35):   // DEC  (HL)
			adr = zHL;

		OP_DEC_m:
			READ_MEM8(adr, res);
			res = (res - 1) & 0xff;
			zF = (zF & CF) | SZHV_dec[res];
			WRITE_MEM8(adr, res);
			RET(11)

		OP(0x86):   // ADD  A,(HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_ADD;

		OP(0xc6):   // ADD  A,n
		OP_ADD_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_ADD;

		OP(0x80):   // ADD  A,B
		OP(0x81):   // ADD  A,C
		OP(0x82):   // ADD  A,D
		OP(0x83):   // ADD  A,E
		OP(0x84):   // ADD  A,H
		OP(0x85):   // ADD  A,L
		OP(0x87):   // ADD  A,A
		OP_ADD_R:
			val = zR8(Opcode & 7);

		OP_ADD:
#if Z80_BIG_FLAGS_ARRAY
			{
				UINT16 A = zA;
				res = (UINT8)(A + val);
				zF = SZHVC_add[(A << 8) | res];
				zA = res;
			}
#else
			res = zA + val;
			zF = SZ[(UINT8)res] | ((res >> 8) & CF) |
				((zA ^ res ^ val) & HF) |
				(((val ^ zA ^ 0x80) & (val ^ res) & 0x80) >> 5);
			zA = res;
#endif
			RET(4)

		OP(0x8e):   // ADC  A,(HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_ADC;

		OP(0xce):   // ADC  A,n
		OP_ADC_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_ADC;

		OP(0x88):   // ADC  A,B
		OP(0x89):   // ADC  A,C
		OP(0x8a):   // ADC  A,D
		OP(0x8b):   // ADC  A,E
		OP(0x8c):   // ADC  A,H
		OP(0x8d):   // ADC  A,L
		OP(0x8f):   // ADC  A,A
		OP_ADC_R:
			val = zR8(Opcode & 7);

		OP_ADC:
#if Z80_BIG_FLAGS_ARRAY
			{
				UINT8 A = zA;
				UINT8 c = zF & CF;
				res = (UINT8)(A + val + c);
				zF = SZHVC_add[(c << 16) | (A << 8) | res];
				zA = res;
			}
#else
			res = zA + val + (zF & CF);
			zF = SZ[res & 0xff] | ((res >> 8) & CF) |
				((zA ^ res ^ val) & HF) |
				(((val ^ zA ^ 0x80) & (val ^ res) & 0x80) >> 5);
			zA = res;
#endif
			RET(4)

		OP(0x96):   // SUB  (HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_SUB;

		OP(0xd6):   // SUB  A,n
		OP_SUB_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_SUB;

		OP(0x90):   // SUB  B
		OP(0x91):   // SUB  C
		OP(0x92):   // SUB  D
		OP(0x93):   // SUB  E
		OP(0x94):   // SUB  H
		OP(0x95):   // SUB  L
		OP(0x97):   // SUB  A
		OP_SUB_R:
			val = zR8(Opcode & 7);

		OP_SUB:
#if Z80_BIG_FLAGS_ARRAY
			{
				UINT8 A = zA;
				res = (UINT8)(A - val);
				zF = SZHVC_sub[(A << 8) | res];
				zA = res;
			}
#else
			res = zA - val;
			zF = SZ[res & 0xff] | ((res >> 8) & CF) | NF |
				((zA ^ res ^ val) & HF) |
				(((val ^ zA) & (zA ^ res) & 0x80) >> 5);
			zA = res;
#endif
			RET(4)

		OP(0x9e):   // SBC  A,(HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_SBC;

		OP(0xde):   // SBC  A,n
		OP_SBC_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_SBC;

		OP(0x98):   // SBC  A,B
		OP(0x99):   // SBC  A,C
		OP(0x9a):   // SBC  A,D
		OP(0x9b):   // SBC  A,E
		OP(0x9c):   // SBC  A,H
		OP(0x9d):   // SBC  A,L
		OP(0x9f):   // SBC  A,A
		OP_SBC_R:
			val = zR8(Opcode & 7);

		OP_SBC:
#if Z80_BIG_FLAGS_ARRAY
			{
				UINT8 A = zA;
				UINT8 c = zF & CF;
				res = (UINT8)(A - val - c);
				zF = SZHVC_sub[(c << 16) | (A << 8) | res];
				zA = res;
			}
#else
			res = zA - val - (zF & CF);
			zF = SZ[res & 0xff] | ((res >> 8) & CF) | NF |
				((zA ^ res ^ val) & HF) |
				(((val ^ zA) & (zA ^ res) & 0x80) >> 5);
			zA = res;
#endif
			RET(4)

		OP(0xbe):   // CP   (HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_CP;

		OP(0xfe):   // CP   n
		OP_CP_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_CP;

		OP(0xb8):   // CP   B
		OP(0xb9):   // CP   C
		OP(0xba):   // CP   D
		OP(0xbb):   // CP   E
		OP(0xbc):   // CP   H
		OP(0xbd):   // CP   L
		OP(0xbf):   // CP   A
		OP_CP_R:
			val = zR8(Opcode & 7);

		OP_CP:
#if Z80_BIG_FLAGS_ARRAY
			{
				UINT8 A = zA;
				res = (UINT8)(A - val);
				zF = (SZHVC_sub[(A << 8) | res] & ~(YF | XF)) |
					(val & (YF | XF));
			}
#else
			res = zA - val;
			zF = (SZ[res & 0xff] & (SF | ZF)) |
				(val & (YF | XF)) | ((res >> 8) & CF) | NF |
				((zA ^ res ^ val) & HF) |
				(((val ^ zA) & (zA ^ res) >> 5) & VF);
#endif
			RET(4)

		OP(0xa6):   // AND  (HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_AND;

		OP(0xe6):   // AND  A,n
		OP_AND_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_AND;

		OP(0xa0):   // AND  B
		OP(0xa1):   // AND  C
		OP(0xa2):   // AND  D
		OP(0xa3):   // AND  E
		OP(0xa4):   // AND  H
		OP(0xa5):   // AND  L
		OP(0xa7):   // AND  A
		OP_AND_R:
			val = zR8(Opcode & 7);

		OP_AND:
			zA &= val;
			zF = SZP[zA] | HF;
			RET(4)

		OP(0xae):   // XOR  (HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_XOR;

		OP(0xee):   // XOR  A,n
		OP_XOR_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_XOR;

		OP(0xa8):   // XOR  B
		OP(0xa9):   // XOR  C
		OP(0xaa):   // XOR  D
		OP(0xab):   // XOR  E
		OP(0xac):   // XOR  H
		OP(0xad):   // XOR  L
		OP(0xaf):   // XOR  A
		OP_XOR_R:
			val = zR8(Opcode & 7);

		OP_XOR:
			zA ^= val;
			zF = SZP[zA];
			RET(4)

		OP(0xb6):   // OR   (HL)
			READ_MEM8(zHL, val);
			USE_CYCLES(3)
			goto OP_OR;

		OP(0xf6):   // OR   A,n
		OP_OR_imm:
			val = ARG();
			USE_CYCLES(3)
			goto OP_OR;

		OP(0xb0):   // OR   B
		OP(0xb1):   // OR   C
		OP(0xb2):   // OR   D
		OP(0xb3):   // OR   E
		OP(0xb4):   // OR   H
		OP(0xb5):   // OR   L
		OP(0xb7):   // OR   A
		OP_OR_R:
			val = zR8(Opcode & 7);

		OP_OR:
			zA |= val;
			zF = SZP[zA];
			RET(4)

		OP(0x27):   // DAA
		OP_DAA: {
			UINT8 F;
			UINT8 cf, nf, hf, lo, hi, diff;

			F = zF;
			cf = F & CF;
			nf = F & NF;
			hf = F & HF;
			lo = zA & 0x0f;
			hi = zA >> 4;

			if (cf) {
				diff = (lo <= 9 && !hf) ? 0x60 : 0x66;
			} else {
				if (lo >= 10) {
					diff = hi <= 8 ? 0x06 : 0x66;
				} else {
					if (hi >= 10) {
						diff = hf ? 0x66 : 0x60;
					} else {
						diff = hf ? 0x06 : 0x00;
					}
				}
			}
			if (nf) zA -= diff;
			else zA += diff;

			F = SZP[zA] | (F & NF);
			if (cf || (lo <= 9 ? hi >= 10 : hi >= 9)) F |= CF;
			if (nf ? hf && lo <= 5 : lo >= 10) F |= HF;
			zF = F;
			RET(4)
		}

		OP(0x2f):   // CPL
		OP_CPL:
			zA ^= 0xff;
			zF = (zF & (SF | ZF | PF | CF)) | HF | NF | (zA & (YF | XF));
			RET(4)

		OP(0x37):   // SCF
		OP_SCF:
			zF = (zF & (SF | ZF | PF)) | CF | (zA & (YF | XF));
			RET(4)

		OP(0x3f):   // CCF
		OP_CCF:
			zF = ((zF & (SF | ZF | PF | CF)) | ((zF & CF) << 4) | (zA & (YF | XF))) ^ CF;
			RET(4)

		OP(0x76):   // HALT
		OP_HALT:
			// CPU->HaltState = 1;
			// Z80->nCyclesLeft = 0;
			// goto Cz80_Check_Interrupt;
			PC--;
			Z80->nCyclesLeft &= 3;
			RET(4)

		OP(0xf3):   // DI
		OP_DI:
			zIFF = 0;
			RET(4)

		OP(0xfb):   // EI
		OP_EI:
#if 0
			USE_CYCLES(4)
			if (!zIFF1)
			{
				zIFF1 = zIFF2 = (1 << 2);
				while (GET_OP() == 0xfb)
				{
					USE_CYCLES(4)
						PC++;
#if Z80_EMULATE_R_EXACTLY
					zR++;
#endif
				}
				if (CPU->IRQState)
				{
					afterEI = 1;
				}
				if (Z80->nCyclesLeft <= 0)
				{
					Z80->nCyclesLeft = 1;
				}
			} else {
				zIFF2 = (1 << 2);
			}
			goto Cz80_Exec;
#endif

			// See if we need to quit after enabling interrupts
			if (Z80->nEI != 1) {
				zIFF = IFF | (IFF << 8);
				RET(4)
			}

			if ( zIFF == 0 ) {
				zIFF = IFF | (IFF << 8);
				RET(4)
			}

			// need to quit now
			zIFF = IFF | (IFF << 8);
			Z80->nEI = 2;
			USE_CYCLES(4);
			goto Cz80_Exec_End;

		OP(0x03):   // INC  BC
		OP_INC_BC:
			zBC++;
			RET(6)

		OP(0x13):   // INC  DE
		OP_INC_DE:
			zDE++;
			RET(6)

		OP(0x23):   // INC  HL
		OP_INC_xx:
			data->w++;
			RET(6)

		OP(0x33):   // INC  SP
		OP_INC_SP:
			zSP++;
			RET(6)

		OP(0x0b):   // DEC  BC
		OP_DEC_BC:
			zBC--;
			RET(6)

		OP(0x1b):   // DEC  DE
		OP_DEC_DE:
			zDE--;
			RET(6)

		OP(0x2b):   // DEC  HL
		OP_DEC_xx:
			data->w--;
			RET(6)

		OP(0x3b):   // DEC  SP
		OP_DEC_SP:
			zSP--;
			RET(6)

		OP(0x39):   // ADD  xx,SP
		OP_ADD16_xx_SP:
			val = zSP;
			goto OP_ADD16;

		OP(0x29):   // ADD  xx,xx
		OP_ADD16_xx_xx:
			val = data->w;
			goto OP_ADD16;

		OP(0x09):   // ADD  xx,BC
		OP_ADD16_xx_BC:
			val = zBC;
			goto OP_ADD16;

		OP(0x19):   // ADD  xx,DE
		OP_ADD16_xx_DE:
			val = zDE;

		OP_ADD16:
			res = data->w + val;
			zF = (zF & (SF | ZF | VF)) |
				(((data->w ^ res ^ val) >> 8) & HF) |
				((res >> 16) & CF) | ((res >> 8) & (YF | XF));
			data->w = (UINT16)res;
			RET(11)

			{
				UINT8 A;
				UINT8 F;

			OP(0x07):   // RLCA
			OP_RLCA:
				A = zA;
				zA = (A << 1) | (A >> 7);
				zF = (zF & (SF | ZF | PF)) | (zA & (YF | XF | CF));
				RET(4)

			OP(0x0f):   // RRCA
			OP_RRCA:
				A = zA;
				F = zF;
				F = (F & (SF | ZF | PF)) | (A & CF);
				zA = (A >> 1) | (A << 7);
				zF = F | (zA & (YF | XF));
				RET(4)

			OP(0x17):   // RLA
			OP_RLA:
				A = zA;
				F = zF;
				zA = (A << 1) | (F & CF);
				zF = (F & (SF | ZF | PF)) | (A >> 7) | (zA & (YF | XF));
				RET(4)

			OP(0x1f):   // RRA
			OP_RRA:
				A = zA;
				F = zF;
				zA = (A >> 1) | (F << 7);
				zF = (F & (SF | ZF | PF)) | (A & CF) | (zA & (YF | XF));
				RET(4)
			}

		OP(0xc3):   // JP   nn
		OP_JP:
			res = ARG16();
			SET_PC(res);
			RET(10)

		OP(0xc2):   // JP   NZ,nn
		OP_JP_NZ:
			if (!(zF & ZF)) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xca):   // JP   Z,nn
		OP_JP_Z:
			if (zF & ZF) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xd2):   // JP   NC,nn
		OP_JP_NC:
			if (!(zF & CF)) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xda):   // JP   C,nn
		OP_JP_C:
			if (zF & CF) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xe2):   // JP   PO,nn
		OP_JP_PO:
			if (!(zF & VF)) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xea):   // JP   PE,nn
		OP_JP_PE:
			if (zF & VF) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xf2):   // JP   P,nn
		OP_JP_P:
			if (!(zF & SF)) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xfa):   // JP   M,nn
		OP_JP_M:
			if (zF & SF) goto OP_JP;
			PC += 2;
			RET(10)

		OP(0xe9):   // JP   (xx)
		OP_JP_xx:
			res = data->w;
			SET_PC(res);
			RET(4)

		OP(0x10):   // DJNZ n
		OP_DJNZ:
			USE_CYCLES(1)
			if (--zB) goto OP_JR;
			PC++;
			RET(7)

		OP(0x18):   // JR   n
		OP_JR:
			offset = (intptr_t)(INT8)ARG();
			PC += offset;
			RET(12)

		OP(0x20):   // JR   NZ,n
		OP_JR_NZ:
			if (!(zF & ZF)) goto OP_JR;
			PC++;
			RET(7)

		OP(0x28):   // JR   Z,n
		OP_JR_Z:
			if (zF & ZF) goto OP_JR;
			PC++;
			RET(7)

		OP(0x38):   // JR   C,n
		OP_JR_C:
			if (zF & CF) goto OP_JR;
			PC++;
			RET(7)

		OP(0x30):   // JR   NC,n
		OP_JR_NC:
			if (!(zF & CF)) goto OP_JR;
			PC++;
			RET(7)

		OP(0xcd):   // CALL nn
		OP_CALL:
			res = ARG16();
			val = zRealPC;
			PUSH_16(val);
			SET_PC(res);
			RET(17)

		OP(0xc4):   // CALL NZ,nn
		OP_CALL_NZ:
			if (!(zF & ZF)) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xcc):   // CALL Z,nn
		OP_CALL_Z:
			if (zF & ZF) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xd4):   // CALL NC,nn
		OP_CALL_NC:
			if (!(zF & CF)) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xdc):   // CALL C,nn
		OP_CALL_C:
			if (zF & CF) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xe4):   // CALL PO,nn
		OP_CALL_PO:
			if (!(zF & VF)) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xec):   // CALL PE,nn
		OP_CALL_PE:
			if (zF & VF) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xf4):   // CALL P,nn
		OP_CALL_P:
			if (!(zF & SF)) goto OP_CALL;
			PC += 2;
			RET(10)

		OP(0xfc):   // CALL M,nn
		OP_CALL_M:
			if (zF & SF) goto OP_CALL;
			PC += 2;
			RET(10)

		OP_RET_COND:
			USE_CYCLES(1)

		OP(0xc9):   // RET
		OP_RET:
			POP_16(res);
			SET_PC(res);
			RET(10)

		OP(0xc0):   // RET  NZ
		OP_RET_NZ:
			if (!(zF & ZF)) goto OP_RET_COND;
			RET(5)

		OP(0xc8):   // RET  Z
		OP_RET_Z:
			if (zF & ZF) goto OP_RET_COND;
			RET(5)

		OP(0xd0):   // RET  NC
		OP_RET_NC:
			if (!(zF & CF)) goto OP_RET_COND;
			RET(5)

		OP(0xd8):   // RET  C
		OP_RET_C:
			if (zF & CF) goto OP_RET_COND;
			RET(5)

		OP(0xe0):   // RET  PO
		OP_RET_PO:
			if (!(zF & VF)) goto OP_RET_COND;
			RET(5)

		OP(0xe8):   // RET  PE
		OP_RET_PE:
			if (zF & VF) goto OP_RET_COND;
			RET(5)

		OP(0xf0):   // RET  P
		OP_RET_P:
			if (!(zF & SF)) goto OP_RET_COND;
			RET(5)

		OP(0xf8):   // RET  M
		OP_RET_M:
			if (zF & SF) goto OP_RET_COND;
			RET(5)

		OP(0xc7):   // RST  0
		OP(0xcf):   // RST  1
		OP(0xd7):   // RST  2
		OP(0xdf):   // RST  3
		OP(0xe7):   // RST  4
		OP(0xef):   // RST  5
		OP(0xf7):   // RST  6
		OP(0xff):   // RST  7
		OP_RST:
			res = zRealPC;
			PUSH_16(res);
			res = Opcode & 0x38;
			SET_PC(res);
			RET(11)

		OP(0xd3):   // OUT  (n),A
		OP_OUT_mN_A:
			adr = (zA << 8) | ARG();
			Z80->ZetOut(adr, zA);
			RET(11)

		OP(0xdb):   // IN   A,(n)
		OP_IN_A_mN:
			adr = (zA << 8) | ARG();
			zA = Z80->ZetIn(adr);
			RET(11)

		OP(0xcb): { // CB prefix (BIT & SHIFT INSTRUCTIONS)
			UINT8 src;
			UINT8 res;

			Opcode = READ_OP();
#if Z80_EMULATE_R_EXACTLY
			zR++;
#endif
//			printf("cb:%02x, PC:%04x, A:%02x, F:%02x, BC:%04x, DE:%04x, HL:%04x, ICount: %d\n", Opcode, PC - zBasePC, zA, zF, zBC, zDE, zHL, Z80->nCyclesLeft);
			goto *JumpTableCB[Opcode];

			OPCB(0x00): // RLC  B
			OPCB(0x01): // RLC  C
			OPCB(0x02): // RLC  D
			OPCB(0x03): // RLC  E
			OPCB(0x04): // RLC  H
			OPCB(0x05): // RLC  L
			OPCB(0x07): // RLC  A
				src = zR8(Opcode);
				res = (src << 1) | (src >> 7);
				zF = SZP[res] | (src >> 7);
				zR8(Opcode) = res;
				RET(8)

			OPCB(0x06): // RLC  (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = (src << 1) | (src >> 7);
				zF = SZP[res] | (src >> 7);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x08): // RRC  B
			OPCB(0x09): // RRC  C
			OPCB(0x0a): // RRC  D
			OPCB(0x0b): // RRC  E
			OPCB(0x0c): // RRC  H
			OPCB(0x0d): // RRC  L
			OPCB(0x0f): // RRC  A
				src = zR8(Opcode & 7);
				res = (src >> 1) | (src << 7);
				zF = SZP[res] | (src & CF);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x0e): // RRC  (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = (src >> 1) | (src << 7);
				zF = SZP[res] | (src & CF);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x10): // RL   B
			OPCB(0x11): // RL   C
			OPCB(0x12): // RL   D
			OPCB(0x13): // RL   E
			OPCB(0x14): // RL   H
			OPCB(0x15): // RL   L
			OPCB(0x17): // RL   A
				src = zR8(Opcode & 7);
				res = (src << 1) | (zF & CF);
				zF = SZP[res] | (src >> 7);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x16): // RL   (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = (src << 1) | (zF & CF);
				zF = SZP[res] | (src >> 7);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x18): // RR   B
			OPCB(0x19): // RR   C
			OPCB(0x1a): // RR   D
			OPCB(0x1b): // RR   E
			OPCB(0x1c): // RR   H
			OPCB(0x1d): // RR   L
			OPCB(0x1f): // RR   A
				src = zR8(Opcode & 7);
				res = (src >> 1) | (zF << 7);
				zF = SZP[res] | (src & CF);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x1e): // RR   (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = (src >> 1) | (zF << 7);
				zF = SZP[res] | (src & CF);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x20): // SLA  B
			OPCB(0x21): // SLA  C
			OPCB(0x22): // SLA  D
			OPCB(0x23): // SLA  E
			OPCB(0x24): // SLA  H
			OPCB(0x25): // SLA  L
			OPCB(0x27): // SLA  A
				src = zR8(Opcode & 7);
				res = src << 1;
				zF = SZP[res] | (src >> 7);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x26): // SLA  (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = src << 1;
				zF = SZP[res] | (src >> 7);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x28): // SRA  B
			OPCB(0x29): // SRA  C
			OPCB(0x2a): // SRA  D
			OPCB(0x2b): // SRA  E
			OPCB(0x2c): // SRA  H
			OPCB(0x2d): // SRA  L
			OPCB(0x2f): // SRA  A
				src = zR8(Opcode & 7);
				res = (src >> 1) | (src & 0x80);
				zF = SZP[res] | (src & CF);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x2e): // SRA  (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = (src >> 1) | (src & 0x80);
				zF = SZP[res] | (src & CF);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x30): // SLL  B
			OPCB(0x31): // SLL  C
			OPCB(0x32): // SLL  D
			OPCB(0x33): // SLL  E
			OPCB(0x34): // SLL  H
			OPCB(0x35): // SLL  L
			OPCB(0x37): // SLL  A
				src = zR8(Opcode & 7);
				res = (src << 1) | 0x01;
				zF = SZP[res] | (src >> 7);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x36): // SLL  (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = (src << 1) | 0x01;
				zF = SZP[res] | (src >> 7);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x38): // SRL  B
			OPCB(0x39): // SRL  C
			OPCB(0x3a): // SRL  D
			OPCB(0x3b): // SRL  E
			OPCB(0x3c): // SRL  H
			OPCB(0x3d): // SRL  L
			OPCB(0x3f): // SRL  A
				src = zR8(Opcode & 7);
				res = src >> 1;
				zF = SZP[res] | (src & CF);
				zR8(Opcode & 7) = res;
				RET(8)

			OPCB(0x3e): // SRL  (HL)
				adr = zHL;
				READ_MEM8(adr, src);
				res = src >> 1;
				zF = SZP[res] | (src & CF);
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0x40): // BIT  0,B
			OPCB(0x41): // BIT  0,C
			OPCB(0x42): // BIT  0,D
			OPCB(0x43): // BIT  0,E
			OPCB(0x44): // BIT  0,H
			OPCB(0x45): // BIT  0,L
			OPCB(0x47): // BIT  0,A

			OPCB(0x48): // BIT  1,B
			OPCB(0x49): // BIT  1,C
			OPCB(0x4a): // BIT  1,D
			OPCB(0x4b): // BIT  1,E
			OPCB(0x4c): // BIT  1,H
			OPCB(0x4d): // BIT  1,L
			OPCB(0x4f): // BIT  1,A

			OPCB(0x50): // BIT  2,B
			OPCB(0x51): // BIT  2,C
			OPCB(0x52): // BIT  2,D
			OPCB(0x53): // BIT  2,E
			OPCB(0x54): // BIT  2,H
			OPCB(0x55): // BIT  2,L
			OPCB(0x57): // BIT  2,A

			OPCB(0x58): // BIT  3,B
			OPCB(0x59): // BIT  3,C
			OPCB(0x5a): // BIT  3,D
			OPCB(0x5b): // BIT  3,E
			OPCB(0x5c): // BIT  3,H
			OPCB(0x5d): // BIT  3,L
			OPCB(0x5f): // BIT  3,A

			OPCB(0x60): // BIT  4,B
			OPCB(0x61): // BIT  4,C
			OPCB(0x62): // BIT  4,D
			OPCB(0x63): // BIT  4,E
			OPCB(0x64): // BIT  4,H
			OPCB(0x65): // BIT  4,L
			OPCB(0x67): // BIT  4,A

			OPCB(0x68): // BIT  5,B
			OPCB(0x69): // BIT  5,C
			OPCB(0x6a): // BIT  5,D
			OPCB(0x6b): // BIT  5,E
			OPCB(0x6c): // BIT  5,H
			OPCB(0x6d): // BIT  5,L
			OPCB(0x6f): // BIT  5,A

			OPCB(0x70): // BIT  6,B
			OPCB(0x71): // BIT  6,C
			OPCB(0x72): // BIT  6,D
			OPCB(0x73): // BIT  6,E
			OPCB(0x74): // BIT  6,H
			OPCB(0x75): // BIT  6,L
			OPCB(0x77): // BIT  6,A

			OPCB(0x78): // BIT  7,B
			OPCB(0x79): // BIT  7,C
			OPCB(0x7a): // BIT  7,D
			OPCB(0x7b): // BIT  7,E
			OPCB(0x7c): // BIT  7,H
			OPCB(0x7d): // BIT  7,L
			OPCB(0x7f): // BIT  7,A
				zF = (zF & CF) | HF | SZ_BIT[zR8(Opcode & 7) & (1 << ((Opcode >> 3) & 7))];
				RET(8)

			OPCB(0x46): // BIT  0,(HL)
			OPCB(0x4e): // BIT  1,(HL)
			OPCB(0x56): // BIT  2,(HL)
			OPCB(0x5e): // BIT  3,(HL)
			OPCB(0x66): // BIT  4,(HL)
			OPCB(0x6e): // BIT  5,(HL)
			OPCB(0x76): // BIT  6,(HL)
			OPCB(0x7e): // BIT  7,(HL)
				READ_MEM8(zHL, src);
				zF = (zF & CF) | HF | SZ_BIT[src & (1 << ((Opcode >> 3) & 7))];
				RET(12)

			OPCB(0x80): // RES  0,B
			OPCB(0x81): // RES  0,C
			OPCB(0x82): // RES  0,D
			OPCB(0x83): // RES  0,E
			OPCB(0x84): // RES  0,H
			OPCB(0x85): // RES  0,L
			OPCB(0x87): // RES  0,A

			OPCB(0x88): // RES  1,B
			OPCB(0x89): // RES  1,C
			OPCB(0x8a): // RES  1,D
			OPCB(0x8b): // RES  1,E
			OPCB(0x8c): // RES  1,H
			OPCB(0x8d): // RES  1,L
			OPCB(0x8f): // RES  1,A

			OPCB(0x90): // RES  2,B
			OPCB(0x91): // RES  2,C
			OPCB(0x92): // RES  2,D
			OPCB(0x93): // RES  2,E
			OPCB(0x94): // RES  2,H
			OPCB(0x95): // RES  2,L
			OPCB(0x97): // RES  2,A

			OPCB(0x98): // RES  3,B
			OPCB(0x99): // RES  3,C
			OPCB(0x9a): // RES  3,D
			OPCB(0x9b): // RES  3,E
			OPCB(0x9c): // RES  3,H
			OPCB(0x9d): // RES  3,L
			OPCB(0x9f): // RES  3,A

			OPCB(0xa0): // RES  4,B
			OPCB(0xa1): // RES  4,C
			OPCB(0xa2): // RES  4,D
			OPCB(0xa3): // RES  4,E
			OPCB(0xa4): // RES  4,H
			OPCB(0xa5): // RES  4,L
			OPCB(0xa7): // RES  4,A

			OPCB(0xa8): // RES  5,B
			OPCB(0xa9): // RES  5,C
			OPCB(0xaa): // RES  5,D
			OPCB(0xab): // RES  5,E
			OPCB(0xac): // RES  5,H
			OPCB(0xad): // RES  5,L
			OPCB(0xaf): // RES  5,A

			OPCB(0xb0): // RES  6,B
			OPCB(0xb1): // RES  6,C
			OPCB(0xb2): // RES  6,D
			OPCB(0xb3): // RES  6,E
			OPCB(0xb4): // RES  6,H
			OPCB(0xb5): // RES  6,L
			OPCB(0xb7): // RES  6,A

			OPCB(0xb8): // RES  7,B
			OPCB(0xb9): // RES  7,C
			OPCB(0xba): // RES  7,D
			OPCB(0xbb): // RES  7,E
			OPCB(0xbc): // RES  7,H
			OPCB(0xbd): // RES  7,L
			OPCB(0xbf): // RES  7,A
				zR8(Opcode & 7) &= ~(1 << ((Opcode >> 3) & 7));
				RET(8)

			OPCB(0x86): // RES  0,(HL)
			OPCB(0x8e): // RES  1,(HL)
			OPCB(0x96): // RES  2,(HL)
			OPCB(0x9e): // RES  3,(HL)
			OPCB(0xa6): // RES  4,(HL)
			OPCB(0xae): // RES  5,(HL)
			OPCB(0xb6): // RES  6,(HL)
			OPCB(0xbe): // RES  7,(HL)
				adr = zHL;
				READ_MEM8(adr, res);
				res &= ~(1 << ((Opcode >> 3) & 7));
				WRITE_MEM8(adr, res);
				RET(15)

			OPCB(0xc0): // SET  0,B
			OPCB(0xc1): // SET  0,C
			OPCB(0xc2): // SET  0,D
			OPCB(0xc3): // SET  0,E
			OPCB(0xc4): // SET  0,H
			OPCB(0xc5): // SET  0,L
			OPCB(0xc7): // SET  0,A

			OPCB(0xc8): // SET  1,B
			OPCB(0xc9): // SET  1,C
			OPCB(0xca): // SET  1,D
			OPCB(0xcb): // SET  1,E
			OPCB(0xcc): // SET  1,H
			OPCB(0xcd): // SET  1,L
			OPCB(0xcf): // SET  1,A

			OPCB(0xd0): // SET  2,B
			OPCB(0xd1): // SET  2,C
			OPCB(0xd2): // SET  2,D
			OPCB(0xd3): // SET  2,E
			OPCB(0xd4): // SET  2,H
			OPCB(0xd5): // SET  2,L
			OPCB(0xd7): // SET  2,A

			OPCB(0xd8): // SET  3,B
			OPCB(0xd9): // SET  3,C
			OPCB(0xda): // SET  3,D
			OPCB(0xdb): // SET  3,E
			OPCB(0xdc): // SET  3,H
			OPCB(0xdd): // SET  3,L
			OPCB(0xdf): // SET  3,A

			OPCB(0xe0): // SET  4,B
			OPCB(0xe1): // SET  4,C
			OPCB(0xe2): // SET  4,D
			OPCB(0xe3): // SET  4,E
			OPCB(0xe4): // SET  4,H
			OPCB(0xe5): // SET  4,L
			OPCB(0xe7): // SET  4,A

			OPCB(0xe8): // SET  5,B
			OPCB(0xe9): // SET  5,C
			OPCB(0xea): // SET  5,D
			OPCB(0xeb): // SET  5,E
			OPCB(0xec): // SET  5,H
			OPCB(0xed): // SET  5,L
			OPCB(0xef): // SET  5,A

			OPCB(0xf0): // SET  6,B
			OPCB(0xf1): // SET  6,C
			OPCB(0xf2): // SET  6,D
			OPCB(0xf3): // SET  6,E
			OPCB(0xf4): // SET  6,H
			OPCB(0xf5): // SET  6,L
			OPCB(0xf7): // SET  6,A

			OPCB(0xf8): // SET  7,B
			OPCB(0xf9): // SET  7,C
			OPCB(0xfa): // SET  7,D
			OPCB(0xfb): // SET  7,E
			OPCB(0xfc): // SET  7,H
			OPCB(0xfd): // SET  7,L
			OPCB(0xff): // SET  7,A
				zR8(Opcode & 7) |= 1 << ((Opcode >> 3) & 7);
				RET(8)

			OPCB(0xc6): // SET  0,(HL)
			OPCB(0xce): // SET  1,(HL)
			OPCB(0xd6): // SET  2,(HL)
			OPCB(0xde): // SET  3,(HL)
			OPCB(0xe6): // SET  4,(HL)
			OPCB(0xee): // SET  5,(HL)
			OPCB(0xf6): // SET  6,(HL)
			OPCB(0xfe): // SET  7,(HL)
				adr = zHL;
				READ_MEM8(adr, res);
				res |= 1 << ((Opcode >> 3) & 7);
				WRITE_MEM8(adr, res);
				RET(15)
		}

		OP(0xed): {
		ED_PREFIX:
			Opcode = READ_OP();
#if Z80_EMULATE_R_EXACTLY
			zR++;
#endif
//			printf("ed:%02x, PC:%04x, A:%02x, F:%02x, BC:%04x, DE:%04x, HL:%04x, ICount: %d\n", Opcode, PC - zBasePC, zA, zF, zBC, zDE, zHL, Z80->nCyclesLeft);

			USE_CYCLES(4)
			goto *JumpTableED[Opcode];

			#include "cz80_oped.cpp"
		}

		OP(0xdd):   // DD prefix (IX)
		DD_PREFIX:
			data = pzIX;
			goto XY_PREFIX;

		OP(0xfd):   // FD prefix (IY)
		FD_PREFIX:
			data = pzIY;

		XY_PREFIX: {
			Opcode = READ_OP();
#if Z80_EMULATE_R_EXACTLY
			zR++;
#endif
//			printf("xy:%02x, PC:%04x, A:%02x, F:%02x, BC:%04x, DE:%04x, HL:%04x, ICount: %d\n", Opcode, PC - zBasePC, zA, zF, zBC, zDE, zHL, Z80->nCyclesLeft);

			USE_CYCLES(4)
			goto *JumpTableXY[Opcode];

			OPXY(0x00): // NOP

			OPXY(0x40): // LD   B,B
			OPXY(0x49): // LD   C,C
			OPXY(0x52): // LD   D,D
			OPXY(0x5b): // LD   E,E
			OPXY(0x64): // LD   H,H
			OPXY(0x6d): // LD   L,L
			OPXY(0x7f): // LD   A,A
				RET(4)

			OPXY(0x41): // LD   B,C
			OPXY(0x42): // LD   B,D
			OPXY(0x43): // LD   B,E
			OPXY(0x47): // LD   B,A

			OPXY(0x48): // LD   C,B
			OPXY(0x4a): // LD   C,D
			OPXY(0x4b): // LD   C,E
			OPXY(0x4f): // LD   C,A

			OPXY(0x50): // LD   D,B
			OPXY(0x51): // LD   D,C
			OPXY(0x53): // LD   D,E
			OPXY(0x57): // LD   D,A

			OPXY(0x58): // LD   E,B
			OPXY(0x59): // LD   E,C
			OPXY(0x5a): // LD   E,D
			OPXY(0x5f): // LD   E,A

			OPXY(0x78): // LD   A,B
			OPXY(0x79): // LD   A,C
			OPXY(0x7a): // LD   A,D
			OPXY(0x7b): // LD   A,E
				goto OP_LD_R_R;

			OPXY(0x44): // LD   B,HX
			OPXY(0x4c): // LD   C,HX
			OPXY(0x54): // LD   D,HX
			OPXY(0x5c): // LD   E,HX
			OPXY(0x7c): // LD   A,HX
				zR8((Opcode >> 3) & 7) = data->b.h;
				RET(5)

			OPXY(0x45): // LD   B,LX
			OPXY(0x4d): // LD   C,LX
			OPXY(0x55): // LD   D,LX
			OPXY(0x5d): // LD   E,LX
			OPXY(0x7d): // LD   A,LX
				zR8((Opcode >> 3) & 7) = data->b.l;
				RET(5)

			OPXY(0x60): // LD   HX,B
			OPXY(0x61): // LD   HX,C
			OPXY(0x62): // LD   HX,D
			OPXY(0x63): // LD   HX,E
			OPXY(0x67): // LD   HX,A
				data->b.h = zR8(Opcode & 7);
				RET(5)

			OPXY(0x68): // LD   LX,B
			OPXY(0x69): // LD   LX,C
			OPXY(0x6a): // LD   LX,D
			OPXY(0x6b): // LD   LX,E
			OPXY(0x6f): // LD   LX,A
				data->b.l = zR8(Opcode & 7);
				RET(5)

			OPXY(0x65): // LD   HX,LX
				data->b.h = data->b.l;
				RET(5)

			OPXY(0x6c): // LD   LX,HX
				data->b.l = data->b.h;
				RET(5)

			OPXY(0x06): // LD   B,#imm
			OPXY(0x0e): // LD   C,#imm
			OPXY(0x16): // LD   D,#imm
			OPXY(0x1e): // LD   E,#imm
			OPXY(0x3e): // LD   A,#imm
				goto OP_LD_R_imm;

			OPXY(0x26): // LD   HX,#imm
				data->b.h = ARG();
				RET(5)

			OPXY(0x2e): // LD   LX,#imm
				data->b.l = ARG();
				RET(5)

			OPXY(0x0a): // LD   A,(BC)
				goto OP_LOAD_A_mBC;

			OPXY(0x1a): // LD   A,(DE)
				goto OP_LOAD_A_mDE;

			OPXY(0x3a): // LD   A,(nn)
				goto OP_LOAD_A_mNN;

			OPXY(0x02): // LD   (BC),A
				goto OP_LOAD_mBC_A;

			OPXY(0x12): // LD   (DE),A
				goto OP_LOAD_mDE_A;

			OPXY(0x32): // LD   (nn),A
				goto OP_LOAD_mNN_A;

			OPXY(0x46): // LD   B,(IX+o)
			OPXY(0x4e): // LD   C,(IX+o)
			OPXY(0x56): // LD   D,(IX+o)
			OPXY(0x5e): // LD   E,(IX+o)
			OPXY(0x66): // LD   H,(IX+o)
			OPXY(0x6e): // LD   L,(IX+o)
			OPXY(0x7e): // LD   A,(IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, zR8((Opcode >> 3) & 7));
				RET(15)

			OPXY(0x70): // LD   (IX+o),B
			OPXY(0x71): // LD   (IX+o),C
			OPXY(0x72): // LD   (IX+o),D
			OPXY(0x73): // LD   (IX+o),E
			OPXY(0x74): // LD   (IX+o),H
			OPXY(0x75): // LD   (IX+o),L
			OPXY(0x77): // LD   (IX+o),A
				adr = data->w + (INT8)ARG();
				WRITE_MEM8(adr, zR8(Opcode & 7));
				RET(15)

			OPXY(0x36): // LD   (IX+o),#imm
				adr = data->w + (INT8)ARG();
				res = ARG();
				WRITE_MEM8(adr, res);
				RET(15)

			OPXY(0x01): // LD   BC,nn
			OPXY(0x11): // LD   DE,nn
				goto OP_LOAD_RR_imm16;

			OPXY(0x21): // LD   IX,nn
				data->w = ARG16();
				RET(10)

			OPXY(0x31): // LD   SP,nn
				goto OP_LOAD_SP_imm16;

			OPXY(0x2a): // LD   IX,(w)
				goto OP_LD_xx_mNN;

			OPXY(0x22): // LD   (w),IX
				goto OP_LD_mNN_xx;

			OPXY(0xf9): // LD   SP,IX
				goto OP_LD_SP_xx;

			OPXY(0xc1): // POP  BC
			OPXY(0xd1): // POP  DE
			OPXY(0xf1): // POP  AF
				goto OP_POP_RR;

			OPXY(0xe1): // POP  IX
				goto OP_POP;

			OPXY(0xc5): // PUSH BC
			OPXY(0xd5): // PUSH DE
			OPXY(0xf5): // PUSH AF
				goto OP_PUSH_RR;

			OPXY(0xe5): // PUSH IX
				goto OP_PUSH;

			OPXY(0x08): // EX   AF,AF'
				goto OP_EX_AF_AF2;

			OPXY(0xeb): // EX   DE,HL
				goto OP_EX_DE_HL;

			OPXY(0xd9): // EXX
				goto OP_EXX;

			OPXY(0xe3): // EX   (SP),IX
				goto OP_EX_xx_mSP;

			OPXY(0x04): // INC  B
			OPXY(0x0c): // INC  C
			OPXY(0x14): // INC  D
			OPXY(0x1c): // INC  E
			OPXY(0x3c): // INC  A
				goto OP_INC_R;

			OPXY(0x24): // INC  HX
				data->b.h++;
				zF = (zF & CF) | SZHV_inc[data->b.h];
				RET(5)

			OPXY(0x2c): // INC  LX
				data->b.l++;
				zF = (zF & CF) | SZHV_inc[data->b.l];
				RET(5)

			OPXY(0x34): // INC  (IX+o)
				adr = data->w + (INT8)ARG();
				USE_CYCLES(8)
				goto OP_INC_m;

			OPXY(0x05): // DEC  B
			OPXY(0x0d): // DEC  C
			OPXY(0x15): // DEC  D
			OPXY(0x1d): // DEC  E
			OPXY(0x3d): // DEC  A
				goto OP_DEC_R;

			OPXY(0x25): // DEC  HX
				data->b.h--;
				zF = (zF & CF) | SZHV_dec[data->b.h];
				RET(5)

			OPXY(0x2d): // DEC  LX
				data->b.l--;
				zF = (zF & CF) | SZHV_dec[data->b.l];
				RET(5)

			OPXY(0x35): // DEC  (IX+o)
				adr = data->w + (INT8)ARG();
				USE_CYCLES(8)
				goto OP_DEC_m;

			OPXY(0x80): // ADD  A,B
			OPXY(0x81): // ADD  A,C
			OPXY(0x82): // ADD  A,D
			OPXY(0x83): // ADD  A,E
			OPXY(0x87): // ADD  A,A
				goto OP_ADD_R;

			OPXY(0xc6): // ADD  A,n
				goto OP_ADD_imm;

			OPXY(0x84): // ADD  A,HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_ADD;

			OPXY(0x85): // ADD  A,LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_ADD;

			OPXY(0x86): // ADD  A,(IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_ADD;

			OPXY(0x88): // ADC  A,B
			OPXY(0x89): // ADC  A,C
			OPXY(0x8a): // ADC  A,D
			OPXY(0x8b): // ADC  A,E
			OPXY(0x8f): // ADC  A,A
				goto OP_ADC_R;

			OPXY(0xce): // ADC  A,n
				goto OP_ADC_imm;

			OPXY(0x8c): // ADC  A,HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_ADC;

			OPXY(0x8d): // ADC  A,LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_ADC;

			OPXY(0x8e): // ADC  A,(IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_ADC;

			OPXY(0x90): // SUB  B
			OPXY(0x91): // SUB  C
			OPXY(0x92): // SUB  D
			OPXY(0x93): // SUB  E
			OPXY(0x97): // SUB  A
				goto OP_SUB_R;

			OPXY(0xd6): // SUB  A,n
				goto OP_SUB_imm;

			OPXY(0x94): // SUB  HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_SUB;

			OPXY(0x95): // SUB  LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_SUB;

			OPXY(0x96): // SUB  (IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_SUB;

			OPXY(0x98): // SBC  A,B
			OPXY(0x99): // SBC  A,C
			OPXY(0x9a): // SBC  A,D
			OPXY(0x9b): // SBC  A,E
			OPXY(0x9f): // SBC  A,A
				goto OP_SBC_R;

			OPXY(0xde): // SBC  A,n
				goto OP_SBC_imm;

			OPXY(0x9c): // SBC  A,HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_SBC;

			OPXY(0x9d): // SBC  A,LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_SBC;

			OPXY(0x9e): // SBC  A,(IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_SBC;

			OPXY(0xb8): // CP   B
			OPXY(0xb9): // CP   C
			OPXY(0xba): // CP   D
			OPXY(0xbb): // CP   E
			OPXY(0xbf): // CP   A
				goto OP_CP_R;

			OPXY(0xfe): // CP   n
				goto OP_CP_imm;

			OPXY(0xbc): // CP   HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_CP;

			OPXY(0xbd): // CP   LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_CP;

			OPXY(0xbe): // CP   (IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_CP;

			OPXY(0xa0): // AND  B
			OPXY(0xa1): // AND  C
			OPXY(0xa2): // AND  D
			OPXY(0xa3): // AND  E
			OPXY(0xa7): // AND  A
				goto OP_AND_R;

			OPXY(0xe6): // AND  A,n
				goto OP_AND_imm;

			OPXY(0xa4): // AND  HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_AND;

			OPXY(0xa5): // AND  LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_AND;

			OPXY(0xa6): // AND  (IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_AND;

			OPXY(0xa8): // XOR  B
			OPXY(0xa9): // XOR  C
			OPXY(0xaa): // XOR  D
			OPXY(0xab): // XOR  E
			OPXY(0xaf): // XOR  A
				goto OP_XOR_R;

			OPXY(0xee): // XOR  A,n
				goto OP_XOR_imm;

			OPXY(0xac): // XOR  HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_XOR;

			OPXY(0xad): // XOR  LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_XOR;

			OPXY(0xae): // XOR  (IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_XOR;

			OPXY(0xb0): // OR   B
			OPXY(0xb1): // OR   C
			OPXY(0xb2): // OR   D
			OPXY(0xb3): // OR   E
			OPXY(0xb7): // OR   A
				goto OP_OR_R;

			OPXY(0xf6): // OR   A,n
				goto OP_OR_imm;

			OPXY(0xb4): // OR   HX
				val = data->b.h;
				USE_CYCLES(1)
				goto OP_OR;

			OPXY(0xb5): // OR   LX
				val = data->b.l;
				USE_CYCLES(1)
				goto OP_OR;

			OPXY(0xb6): // OR   (IX+o)
				adr = data->w + (INT8)ARG();
				READ_MEM8(adr, val);
				USE_CYCLES(11)
				goto OP_OR;

			OPXY(0x27): // DAA
				goto OP_DAA;

			OPXY(0x2f): // CPL
				goto OP_CPL;

			OPXY(0x37): // SCF
				goto OP_SCF;

			OPXY(0x3f): // CCF
				goto OP_CCF;

			OPXY(0x76): // HALT
				goto OP_HALT;

			OPXY(0xf3): // DI
				goto OP_DI;

			OPXY(0xfb): // EI
				goto OP_EI;

			OPXY(0x03): // INC  BC
				goto OP_INC_BC;

			OPXY(0x13): // INC  DE
				goto OP_INC_DE;

			OPXY(0x23): // INC  IX
				goto OP_INC_xx;

			OPXY(0x33): // INC  SP
				goto OP_INC_SP;

			OPXY(0x0b): // DEC  BC
				goto OP_DEC_BC;

			OPXY(0x1b): // DEC  DE
				goto OP_DEC_DE;

			OPXY(0x2b): // DEC  IX
				goto OP_DEC_xx;

			OPXY(0x3b): // DEC  SP
				goto OP_DEC_SP;

			OPXY(0x09): // ADD  IX,BC
				goto OP_ADD16_xx_BC;

			OPXY(0x19): // ADD  IX,DE
				goto OP_ADD16_xx_DE;

			OPXY(0x29): // ADD  IX,IX
				goto OP_ADD16_xx_xx;

			OPXY(0x39): // ADD  IX,SP
				goto OP_ADD16_xx_SP;

			OPXY(0x07): // RLCA
				goto OP_RLCA;

			OPXY(0x0f): // RRCA
				goto OP_RRCA;

			OPXY(0x17): // RLA
				goto OP_RLA;

			OPXY(0x1f): // RRA
				goto OP_RRA;

			OPXY(0xc3): // JP   nn
				goto OP_JP;

			OPXY(0xe9): // JP   (IX)
				goto OP_JP_xx;

			OPXY(0xc2): // JP   NZ,nn
				goto OP_JP_NZ;

			OPXY(0xca): // JP   Z,nn
				goto OP_JP_Z;

			OPXY(0xd2): // JP   NC,nn
				goto OP_JP_NC;

			OPXY(0xda): // JP   C,nn
				goto OP_JP_C;

			OPXY(0xe2): // JP   PO,nn
				goto OP_JP_PO;

			OPXY(0xea): // JP   PE,nn
				goto OP_JP_PE;

			OPXY(0xf2): // JP   P,nn
				goto OP_JP_P;

			OPXY(0xfa): // JP   M,nn
				goto OP_JP_M;

			OPXY(0x10): // DJNZ n
				goto OP_DJNZ;

			OPXY(0x18): // JR   n
				goto OP_JR;

			OPXY(0x20): // JR   NZ,n
				goto OP_JR_NZ;

			OPXY(0x28): // JR   Z,n
				goto OP_JR_Z;

			OPXY(0x30): // JR   NC,n
				goto OP_JR_NC;

			OPXY(0x38): // JR   C,n
				goto OP_JR_C;

			OPXY(0xcd): // CALL nn
				goto OP_CALL;

			OPXY(0xc4): // CALL NZ,nn
				goto OP_CALL_NZ;

			OPXY(0xcc): // CALL Z,nn
				goto OP_CALL_Z;

			OPXY(0xd4): // CALL NC,nn
				goto OP_CALL_NC;

			OPXY(0xdc): // CALL C,nn
				goto OP_CALL_C;

			OPXY(0xe4): // CALL PO,nn
				goto OP_CALL_PO;

			OPXY(0xec): // CALL PE,nn
				goto OP_CALL_PE;

			OPXY(0xf4): // CALL P,nn
				goto OP_CALL_P;

			OPXY(0xfc): // CALL M,nn
				goto OP_CALL_M;

			OPXY(0xc9): // RET
				goto OP_RET;

			OPXY(0xc0): // RET  NZ
				goto OP_RET_NZ;

			OPXY(0xc8): // RET  Z
				goto OP_RET_Z;

			OPXY(0xd0): // RET  NC
				goto OP_RET_NC;

			OPXY(0xd8): // RET  C
				goto OP_RET_C;

			OPXY(0xe0): // RET  PO
				goto OP_RET_PO;

			OPXY(0xe8): // RET  PE
				goto OP_RET_PE;

			OPXY(0xf0): // RET  P
				goto OP_RET_P;

			OPXY(0xf8): // RET  M
				goto OP_RET_M;

			OPXY(0xc7): // RST  0
			OPXY(0xcf): // RST  1
			OPXY(0xd7): // RST  2
			OPXY(0xdf): // RST  3
			OPXY(0xe7): // RST  4
			OPXY(0xef): // RST  5
			OPXY(0xf7): // RST  6
			OPXY(0xff): // RST  7
				goto OP_RST;

			OPXY(0xd3): // OUT  (n),A
				goto OP_OUT_mN_A;

			OPXY(0xdb): // IN   A,(n)
				goto OP_IN_A_mN;

			OPXY(0xcb): {// XYCB prefix (BIT & SHIFT INSTRUCTIONS)
				UINT8 src;
				UINT8 res;

				adr = data->w + (INT8)ARG();
				Opcode = ARG();
#if Z80_EMULATE_R_EXACTLY
				zR++;
#endif
//				printf("xycb:%02x, PC:%04x, A:%02x, F:%02x, BC:%04x, DE:%04x, HL:%04x, \n", Opcode, PC - zBasePC, zA, zF, zBC, zDE, zHL);
				goto *JumpTableXYCB[Opcode];

				OPXYCB(0x00):   // RLC  (Ix+d), B
				OPXYCB(0x01):   // RLC  (Ix+d), C
				OPXYCB(0x02):   // RLC  (Ix+d), D
				OPXYCB(0x03):   // RLC  (Ix+d), E
				OPXYCB(0x04):   // RLC  (Ix+d), H
				OPXYCB(0x05):   // RLC  (Ix+d), L
				OPXYCB(0x07):   // RLC  (Ix+d), A
					READ_MEM8(adr, src);
					res = ((src << 1) | (src >> 7)) & 0xff;
					zF = SZP[res] | (src >> 7);
					zR8(Opcode) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x06):   // RLC  (Ix+d)
					READ_MEM8(adr, src);
					res = ((src << 1) | (src >> 7)) & 0xff;
					zF = SZP[res] | (src >> 7);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x08):   // RRC  (Ix+d), B
				OPXYCB(0x09):   // RRC  (Ix+d), C
				OPXYCB(0x0a):   // RRC  (Ix+d), D
				OPXYCB(0x0b):   // RRC  (Ix+d), E
				OPXYCB(0x0c):   // RRC  (Ix+d), H
				OPXYCB(0x0d):   // RRC  (Ix+d), L
				OPXYCB(0x0f):   // RRC  (Ix+d), A
					READ_MEM8(adr, src);
					res = ((src >> 1) | (src << 7)) & 0xff;
					zF = SZP[res] | (src & CF);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x0e):   // RRC  (Ix+d)
					READ_MEM8(adr, src);
					res = ((src >> 1) | (src << 7)) & 0xff;
					zF = SZP[res] | (src & CF);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x10):   // RL   (Ix+d), B
				OPXYCB(0x11):   // RL   (Ix+d), C
				OPXYCB(0x12):   // RL   (Ix+d), D
				OPXYCB(0x13):   // RL   (Ix+d), E
				OPXYCB(0x14):   // RL   (Ix+d), H
				OPXYCB(0x15):   // RL   (Ix+d), L
				OPXYCB(0x17):   // RL   (Ix+d), A
					READ_MEM8(adr, src);
					res = ((src << 1) | (zF & CF)) & 0xff;
					zF = SZP[res] | (src >> 7);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x16):   // RL   (Ix+d)
					READ_MEM8(adr, src);
					res = ((src << 1) | (zF & CF)) & 0xff;
					zF = SZP[res] | (src >> 7);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x18):   // RR   (Ix+d), B
				OPXYCB(0x19):   // RR   (Ix+d), C
				OPXYCB(0x1a):   // RR   (Ix+d), D
				OPXYCB(0x1b):   // RR   (Ix+d), E
				OPXYCB(0x1c):   // RR   (Ix+d), H
				OPXYCB(0x1d):   // RR   (Ix+d), L
				OPXYCB(0x1f):   // RR   (Ix+d), A
					READ_MEM8(adr, src);
					res = ((src >> 1) | (zF << 7)) & 0xff;
					zF = SZP[res] | (src & CF);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x1e):   // RR   (Ix+d)
					READ_MEM8(adr, src);
					res = ((src >> 1) | (zF << 7)) & 0xff;
					zF = SZP[res] | (src & CF);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x20):   // SLA  (Ix+d), B
				OPXYCB(0x21):   // SLA  (Ix+d), C
				OPXYCB(0x22):   // SLA  (Ix+d), D
				OPXYCB(0x23):   // SLA  (Ix+d), E
				OPXYCB(0x24):   // SLA  (Ix+d), H
				OPXYCB(0x25):   // SLA  (Ix+d), L
				OPXYCB(0x27):   // SLA  (Ix+d), A
					READ_MEM8(adr, src);
					res = (src << 1) & 0xff;
					zF = SZP[res] | (src >> 7);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x26):   // SLA  (Ix+d)
					READ_MEM8(adr, src);
					res = (src << 1) & 0xff;
					zF = SZP[res] | (src >> 7);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x28):   // SRA  (Ix+d), B
				OPXYCB(0x29):   // SRA  (Ix+d), C
				OPXYCB(0x2a):   // SRA  (Ix+d), D
				OPXYCB(0x2b):   // SRA  (Ix+d), E
				OPXYCB(0x2c):   // SRA  (Ix+d), H
				OPXYCB(0x2d):   // SRA  (Ix+d), L
				OPXYCB(0x2f):   // SRA  (Ix+d), A
					READ_MEM8(adr, src);
					res = ((src >> 1) | (src & 0x80)) & 0xff;
					zF = SZP[res] | (src & CF);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x2e):   // SRA  (Ix+d)
					READ_MEM8(adr, src);
					res = ((src >> 1) | (src & 0x80)) & 0xff;
					zF = SZP[res] | (src & CF);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x30):   // SLL  (Ix+d), B
				OPXYCB(0x31):   // SLL  (Ix+d), C
				OPXYCB(0x32):   // SLL  (Ix+d), D
				OPXYCB(0x33):   // SLL  (Ix+d), E
				OPXYCB(0x34):   // SLL  (Ix+d), H
				OPXYCB(0x35):   // SLL  (Ix+d), L
				OPXYCB(0x37):   // SLL  (Ix+d), A
					READ_MEM8(adr, src);
					res = ((src << 1) | 0x01) & 0xff;
					zF = SZP[res] | (src >> 7);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x36):   // SLL  (Ix+d)
					READ_MEM8(adr, src);
					res = ((src << 1) | 0x01) & 0xff;
					zF = SZP[res] | (src >> 7);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x38):   // SRL  (Ix+d), B
				OPXYCB(0x39):   // SRL  (Ix+d), C
				OPXYCB(0x3a):   // SRL  (Ix+d), D
				OPXYCB(0x3b):   // SRL  (Ix+d), E
				OPXYCB(0x3c):   // SRL  (Ix+d), H
				OPXYCB(0x3d):   // SRL  (Ix+d), L
				OPXYCB(0x3f):   // SRL  (Ix+d), A
					READ_MEM8(adr, src);
					res = (src >> 1) & 0xff;
					zF = SZP[res] | (src & CF);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x3e):   // SRL  (Ix+d)
					READ_MEM8(adr, src);
					res = (src >> 1) & 0xff;
					zF = SZP[res] | (src & CF);
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x40):   // BIT  0,(Ix+d)
				OPXYCB(0x41):   // BIT  0,(Ix+d)
				OPXYCB(0x42):   // BIT  0,(Ix+d)
				OPXYCB(0x43):   // BIT  0,(Ix+d)
				OPXYCB(0x44):   // BIT  0,(Ix+d)
				OPXYCB(0x45):   // BIT  0,(Ix+d)
				OPXYCB(0x47):   // BIT  0,(Ix+d)

				OPXYCB(0x48):   // BIT  1,(Ix+d)
				OPXYCB(0x49):   // BIT  1,(Ix+d)
				OPXYCB(0x4a):   // BIT  1,(Ix+d)
				OPXYCB(0x4b):   // BIT  1,(Ix+d)
				OPXYCB(0x4c):   // BIT  1,(Ix+d)
				OPXYCB(0x4d):   // BIT  1,(Ix+d)
				OPXYCB(0x4f):   // BIT  1,(Ix+d)

				OPXYCB(0x50):   // BIT  2,(Ix+d)
				OPXYCB(0x51):   // BIT  2,(Ix+d)
				OPXYCB(0x52):   // BIT  2,(Ix+d)
				OPXYCB(0x53):   // BIT  2,(Ix+d)
				OPXYCB(0x54):   // BIT  2,(Ix+d)
				OPXYCB(0x55):   // BIT  2,(Ix+d)
				OPXYCB(0x57):   // BIT  2,(Ix+d)

				OPXYCB(0x58):   // BIT  3,(Ix+d)
				OPXYCB(0x59):   // BIT  3,(Ix+d)
				OPXYCB(0x5a):   // BIT  3,(Ix+d)
				OPXYCB(0x5b):   // BIT  3,(Ix+d)
				OPXYCB(0x5c):   // BIT  3,(Ix+d)
				OPXYCB(0x5d):   // BIT  3,(Ix+d)
				OPXYCB(0x5f):   // BIT  3,(Ix+d)

				OPXYCB(0x60):   // BIT  4,(Ix+d)
				OPXYCB(0x61):   // BIT  4,(Ix+d)
				OPXYCB(0x62):   // BIT  4,(Ix+d)
				OPXYCB(0x63):   // BIT  4,(Ix+d)
				OPXYCB(0x64):   // BIT  4,(Ix+d)
				OPXYCB(0x65):   // BIT  4,(Ix+d)
				OPXYCB(0x67):   // BIT  4,(Ix+d)

				OPXYCB(0x68):   // BIT  5,(Ix+d)
				OPXYCB(0x69):   // BIT  5,(Ix+d)
				OPXYCB(0x6a):   // BIT  5,(Ix+d)
				OPXYCB(0x6b):   // BIT  5,(Ix+d)
				OPXYCB(0x6c):   // BIT  5,(Ix+d)
				OPXYCB(0x6d):   // BIT  5,(Ix+d)
				OPXYCB(0x6f):   // BIT  5,(Ix+d)

				OPXYCB(0x70):   // BIT  6,(Ix+d)
				OPXYCB(0x71):   // BIT  6,(Ix+d)
				OPXYCB(0x72):   // BIT  6,(Ix+d)
				OPXYCB(0x73):   // BIT  6,(Ix+d)
				OPXYCB(0x74):   // BIT  6,(Ix+d)
				OPXYCB(0x75):   // BIT  6,(Ix+d)
				OPXYCB(0x77):   // BIT  6,(Ix+d)

				OPXYCB(0x78):   // BIT  7,(Ix+d)
				OPXYCB(0x79):   // BIT  7,(Ix+d)
				OPXYCB(0x7a):   // BIT  7,(Ix+d)
				OPXYCB(0x7b):   // BIT  7,(Ix+d)
				OPXYCB(0x7c):   // BIT  7,(Ix+d)
				OPXYCB(0x7d):   // BIT  7,(Ix+d)
				OPXYCB(0x7f):   // BIT  7,(Ix+d)

				OPXYCB(0x46):   // BIT  0,(Ix+d)
				OPXYCB(0x4e):   // BIT  1,(Ix+d)
				OPXYCB(0x56):   // BIT  2,(Ix+d)
				OPXYCB(0x5e):   // BIT  3,(Ix+d)
				OPXYCB(0x66):   // BIT  4,(Ix+d)
				OPXYCB(0x6e):   // BIT  5,(Ix+d)
				OPXYCB(0x76):   // BIT  6,(Ix+d)
				OPXYCB(0x7e):   // BIT  7,(Ix+d)
					READ_MEM8(adr, src);
					zF = (zF & CF) | HF |
						 (SZ_BIT[src & (1 << ((Opcode >> 3) & 7))] & ~(YF | XF)) |
						 ((adr >> 8) & (YF | XF));
					RET(16)

				OPXYCB(0x80):   // RES  0,(Ix+d),B
				OPXYCB(0x81):   // RES  0,(Ix+d),C
				OPXYCB(0x82):   // RES  0,(Ix+d),D
				OPXYCB(0x83):   // RES  0,(Ix+d),E
				OPXYCB(0x84):   // RES  0,(Ix+d),H
				OPXYCB(0x85):   // RES  0,(Ix+d),L
				OPXYCB(0x87):   // RES  0,(Ix+d),A

				OPXYCB(0x88):   // RES  1,(Ix+d),B
				OPXYCB(0x89):   // RES  1,(Ix+d),C
				OPXYCB(0x8a):   // RES  1,(Ix+d),D
				OPXYCB(0x8b):   // RES  1,(Ix+d),E
				OPXYCB(0x8c):   // RES  1,(Ix+d),H
				OPXYCB(0x8d):   // RES  1,(Ix+d),L
				OPXYCB(0x8f):   // RES  1,(Ix+d),A

				OPXYCB(0x90):   // RES  2,(Ix+d),B
				OPXYCB(0x91):   // RES  2,(Ix+d),C
				OPXYCB(0x92):   // RES  2,(Ix+d),D
				OPXYCB(0x93):   // RES  2,(Ix+d),E
				OPXYCB(0x94):   // RES  2,(Ix+d),H
				OPXYCB(0x95):   // RES  2,(Ix+d),L
				OPXYCB(0x97):   // RES  2,(Ix+d),A

				OPXYCB(0x98):   // RES  3,(Ix+d),B
				OPXYCB(0x99):   // RES  3,(Ix+d),C
				OPXYCB(0x9a):   // RES  3,(Ix+d),D
				OPXYCB(0x9b):   // RES  3,(Ix+d),E
				OPXYCB(0x9c):   // RES  3,(Ix+d),H
				OPXYCB(0x9d):   // RES  3,(Ix+d),L
				OPXYCB(0x9f):   // RES  3,(Ix+d),A

				OPXYCB(0xa0):   // RES  4,(Ix+d),B
				OPXYCB(0xa1):   // RES  4,(Ix+d),C
				OPXYCB(0xa2):   // RES  4,(Ix+d),D
				OPXYCB(0xa3):   // RES  4,(Ix+d),E
				OPXYCB(0xa4):   // RES  4,(Ix+d),H
				OPXYCB(0xa5):   // RES  4,(Ix+d),L
				OPXYCB(0xa7):   // RES  4,(Ix+d),A

				OPXYCB(0xa8):   // RES  5,(Ix+d),B
				OPXYCB(0xa9):   // RES  5,(Ix+d),C
				OPXYCB(0xaa):   // RES  5,(Ix+d),D
				OPXYCB(0xab):   // RES  5,(Ix+d),E
				OPXYCB(0xac):   // RES  5,(Ix+d),H
				OPXYCB(0xad):   // RES  5,(Ix+d),L
				OPXYCB(0xaf):   // RES  5,(Ix+d),A

				OPXYCB(0xb0):   // RES  6,(Ix+d),B
				OPXYCB(0xb1):   // RES  6,(Ix+d),C
				OPXYCB(0xb2):   // RES  6,(Ix+d),D
				OPXYCB(0xb3):   // RES  6,(Ix+d),E
				OPXYCB(0xb4):   // RES  6,(Ix+d),H
				OPXYCB(0xb5):   // RES  6,(Ix+d),L
				OPXYCB(0xb7):   // RES  6,(Ix+d),A

				OPXYCB(0xb8):   // RES  7,(Ix+d),B
				OPXYCB(0xb9):   // RES  7,(Ix+d),C
				OPXYCB(0xba):   // RES  7,(Ix+d),D
				OPXYCB(0xbb):   // RES  7,(Ix+d),E
				OPXYCB(0xbc):   // RES  7,(Ix+d),H
				OPXYCB(0xbd):   // RES  7,(Ix+d),L
				OPXYCB(0xbf):   // RES  7,(Ix+d),A
					READ_MEM8(adr, res);
					res &= ~(1 << ((Opcode >> 3) & 7));
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0x86):   // RES  0,(Ix+d)
				OPXYCB(0x8e):   // RES  1,(Ix+d)
				OPXYCB(0x96):   // RES  2,(Ix+d)
				OPXYCB(0x9e):   // RES  3,(Ix+d)
				OPXYCB(0xa6):   // RES  4,(Ix+d)
				OPXYCB(0xae):   // RES  5,(Ix+d)
				OPXYCB(0xb6):   // RES  6,(Ix+d)
				OPXYCB(0xbe):   // RES  7,(Ix+d)
					READ_MEM8(adr, res);
					res &= ~(1 << ((Opcode >> 3) & 7));
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0xc0):   // SET  0,(Ix+d),B
				OPXYCB(0xc1):   // SET  0,(Ix+d),C
				OPXYCB(0xc2):   // SET  0,(Ix+d),D
				OPXYCB(0xc3):   // SET  0,(Ix+d),E
				OPXYCB(0xc4):   // SET  0,(Ix+d),H
				OPXYCB(0xc5):   // SET  0,(Ix+d),L
				OPXYCB(0xc7):   // SET  0,(Ix+d),A

				OPXYCB(0xc8):   // SET  1,(Ix+d),B
				OPXYCB(0xc9):   // SET  1,(Ix+d),C
				OPXYCB(0xca):   // SET  1,(Ix+d),D
				OPXYCB(0xcb):   // SET  1,(Ix+d),E
				OPXYCB(0xcc):   // SET  1,(Ix+d),H
				OPXYCB(0xcd):   // SET  1,(Ix+d),L
				OPXYCB(0xcf):   // SET  1,(Ix+d),A

				OPXYCB(0xd0):   // SET  2,(Ix+d),B
				OPXYCB(0xd1):   // SET  2,(Ix+d),C
				OPXYCB(0xd2):   // SET  2,(Ix+d),D
				OPXYCB(0xd3):   // SET  2,(Ix+d),E
				OPXYCB(0xd4):   // SET  2,(Ix+d),H
				OPXYCB(0xd5):   // SET  2,(Ix+d),L
				OPXYCB(0xd7):   // SET  2,(Ix+d),A

				OPXYCB(0xd8):   // SET  3,(Ix+d),B
				OPXYCB(0xd9):   // SET  3,(Ix+d),C
				OPXYCB(0xda):   // SET  3,(Ix+d),D
				OPXYCB(0xdb):   // SET  3,(Ix+d),E
				OPXYCB(0xdc):   // SET  3,(Ix+d),H
				OPXYCB(0xdd):   // SET  3,(Ix+d),L
				OPXYCB(0xdf):   // SET  3,(Ix+d),A

				OPXYCB(0xe0):   // SET  4,(Ix+d),B
				OPXYCB(0xe1):   // SET  4,(Ix+d),C
				OPXYCB(0xe2):   // SET  4,(Ix+d),D
				OPXYCB(0xe3):   // SET  4,(Ix+d),E
				OPXYCB(0xe4):   // SET  4,(Ix+d),H
				OPXYCB(0xe5):   // SET  4,(Ix+d),L
				OPXYCB(0xe7):   // SET  4,(Ix+d),A

				OPXYCB(0xe8):   // SET  5,(Ix+d),B
				OPXYCB(0xe9):   // SET  5,(Ix+d),C
				OPXYCB(0xea):   // SET  5,(Ix+d),D
				OPXYCB(0xeb):   // SET  5,(Ix+d),E
				OPXYCB(0xec):   // SET  5,(Ix+d),H
				OPXYCB(0xed):   // SET  5,(Ix+d),L
				OPXYCB(0xef):   // SET  5,(Ix+d),A

				OPXYCB(0xf0):   // SET  6,(Ix+d),B
				OPXYCB(0xf1):   // SET  6,(Ix+d),C
				OPXYCB(0xf2):   // SET  6,(Ix+d),D
				OPXYCB(0xf3):   // SET  6,(Ix+d),E
				OPXYCB(0xf4):   // SET  6,(Ix+d),H
				OPXYCB(0xf5):   // SET  6,(Ix+d),L
				OPXYCB(0xf7):   // SET  6,(Ix+d),A

				OPXYCB(0xf8):   // SET  7,(Ix+d),B
				OPXYCB(0xf9):   // SET  7,(Ix+d),C
				OPXYCB(0xfa):   // SET  7,(Ix+d),D
				OPXYCB(0xfb):   // SET  7,(Ix+d),E
				OPXYCB(0xfc):   // SET  7,(Ix+d),H
				OPXYCB(0xfd):   // SET  7,(Ix+d),L
				OPXYCB(0xff):   // SET  7,(Ix+d),A
					READ_MEM8(adr, res);
					res |= 1 << ((Opcode >> 3) & 7);
					zR8(Opcode & 7) = res;
					WRITE_MEM8(adr, res);
					RET(19)

				OPXYCB(0xc6):   // SET  0,(Ix+d)
				OPXYCB(0xce):   // SET  1,(Ix+d)
				OPXYCB(0xd6):   // SET  2,(Ix+d)
				OPXYCB(0xde):   // SET  3,(Ix+d)
				OPXYCB(0xe6):   // SET  4,(Ix+d)
				OPXYCB(0xee):   // SET  5,(Ix+d)
				OPXYCB(0xf6):   // SET  6,(Ix+d)
				OPXYCB(0xfe):   // SET  7,(Ix+d)
					READ_MEM8(adr, res);
					res |= 1 << ((Opcode >> 3) & 7);
					WRITE_MEM8(adr, res);
					RET(19)
			}

			OPXY(0xed): // ED prefix
				goto ED_PREFIX;

			OPXY(0xdd): // DD prefix (IX)
				goto DD_PREFIX;

			OPXY(0xfd): // FD prefix (IY)
				goto FD_PREFIX;
		}
	}

Cz80_Try_Int:
	if ((Z80->nInterruptLatch & Z80_IRQSTATUS_NONE) == 0) {
		if (zIFF1 == 0) goto Cz80_Exec_End;

		if (GET_OP() == 0x76) PC++;

		// printf("Z80 Interrupt: irq_vector $%06x\n", Z80->nInterruptLatch & 0xff);
		/* "hold_irq" assures that an irq request (with CPU_IRQSTATUS_HOLD) gets
		   acknowleged.  This is designed to get around the following 2 problems:

		   1) Requests made with CPU_IRQSTATUS_AUTO might get skipped in
		   circumstances where IRQs are disabled at the moment it was requested.

		   2) Requests made with CPU_IRQSTATUS_ACK might cause more than 1 irq to
		   get taken if is held in the _ACK state for too long(!) - dink jan.2016
		*/
		if (Z80->nInterruptLatch & Z80_IRQSTATUS_HOLD) {
			Z80->nInterruptLatch &= ~Z80_IRQSTATUS_HOLD;
			Z80->nInterruptLatch |= Z80_IRQSTATUS_NONE;
		}

		zIFF = 0;
		if (zIM == 2) {
			int nTabAddr = 0, nIntAddr = 0;
			nTabAddr = ((unsigned short)zI << 8) + (Z80->nInterruptLatch & 0xFF);
			READ_MEM16( nTabAddr, nIntAddr );
			PUSH_16(zRealPC);
			SET_PC(nIntAddr);
			if (Z80->nInterruptLatch & Z80_IRQSTATUS_AUTO) Z80->nInterruptLatch = Z80_IRQSTATUS_NONE;
			Z80->nCyclesLeft -= 19;
		} else if (zIM == 1) {
			PUSH_16(zRealPC);
			SET_PC( 0x38 );
			if (Z80->nInterruptLatch & Z80_IRQSTATUS_AUTO) Z80->nInterruptLatch = Z80_IRQSTATUS_NONE;
			Z80->nCyclesLeft -= 13;
		} else {
			PUSH_16( zRealPC );
			SET_PC( Z80->nInterruptLatch & 0x38 );
			if (Z80->nInterruptLatch & Z80_IRQSTATUS_AUTO) Z80->nInterruptLatch = Z80_IRQSTATUS_NONE;
			Z80->nCyclesLeft -= 13;
		}
	}

	if (Z80->nEI == 8) {
		Z80->nEI = 0;
		goto Cz80_Exec;
	}

	if (Z80->nCyclesLeft < 0) {
		goto Cz80_Exec_Really_End;
	}

	if (GET_OP() == 0x76) {
		int nDid = (Z80->nCyclesLeft >> 2) + 1;
		zR1 = (zR + nDid) & 0x7F;
		Z80->nCyclesLeft -= nDid;
		goto Cz80_Exec_Really_End;
	}

	Z80->nEI = 1;
	goto Cz80_Exec;

Cz80_Exec_End:
	if (Z80->nEI == 8) {
		Z80->nEI = 0;
		goto Cz80_Exec;
	}

	if (Z80->nEI == 2) {
		// (do one more instruction before interrupt)
		nTodo = Z80->nCyclesLeft;
		Z80->nCyclesLeft = 0;
		Z80->nEI = 4;
		goto Cz80_Exec;
	}

	if (Z80->nEI == 4) {
		Z80->nCyclesLeft += nTodo;
		nTodo = 0;
		Z80->nEI = 8;
		goto Cz80_Try_Int;
	}

	if (!Z80->bStopRun && Z80->nCyclesLeft > 0) goto Cz80_Exec;

Cz80_Exec_Really_End:
	zPC = PC;
	// update R register
	zR1 = (zR + ((Z80->nCyclesSegment - Z80->nCyclesLeft) >> 2)) & 0x7F;

	nCycles = Z80->nCyclesSegment - Z80->nCyclesLeft;
	Z80->nCyclesSegment = Z80->nCyclesLeft = 0;
	return nCycles;
}

static INT32 Z80Nmi() {
	ZetExt *Z80 = ZetCPUContext[nOpenedCPU];

	zIFF1 = 0;
	PUSH_16(zPC - zBasePC);
	RebasePC(0x66);
	return 12;
}
