/*
 * zencli.c by CyrIng
 *
 * Copyright (C) 2020-2021 CYRIL INGENIERIE
 * Licenses: GPL2
 *
 * - Build Instructions -
 *	cc zencli.c -o zencli
 *   with HSMP registers as options
 *	cc zencli.c -o zencli \
 *	-DHSMP_CMD_REG=0x3B10524 \
 *	-DHSMP_ARG_REG=0x3B10A40 \
 *	-DHSMP_RSP_REG=0x3B10570
 */
/* This file, as seen above, is entirely property under 
 * Cyring. I repeat, this file does not belong to me.
 * My project is licensed under GPL v3, and
 *  I have made Cyring aware that it exists. */

#define _GNU_SOURCE
#include <sys/io.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

typedef unsigned long long int	Bit64;

#define LOCKLESS " "
#define BUS_LOCK "lock "

#define BIT_ATOM_INIT(atom, seed)					\
({									\
	__asm__ volatile						\
	(								\
		"leaq	%[_atom],	%%rdx"		"\n\t"		\
		"movq	%[_seed],	%%rax"		"\n\t"		\
		"movq	%%rax	,	(%%rdx)" 			\
		:							\
		: [_atom]	"m"	(atom) ,			\
		  [_seed]	"i"	(seed)				\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define BIT_ATOM_TRYLOCK(_lock, atom, seed)				\
({									\
	volatile unsigned char _ret;					\
									\
	__asm__ volatile						\
	(								\
		"movq	%[_seed],	%%rdx"		"\n\t"		\
		"movl	%%edx	,	%%eax"		"\n\t"		\
		"movq	$0xffffffff,	%%rbx"		"\n\t"		\
		"andq	%%rbx	,	%%rax"		"\n\t"		\
		"shrq	$32	,	%%rdx"		"\n\t"		\
		"xorq	%%rcx	,	%%rcx"		"\n\t"		\
		"xorq	%%rbx	,	%%rbx"		"\n\t"		\
		_lock	"cmpxchg8b	%[_atom]"	"\n\t"		\
		"setz	%[_ret]"					\
		: [_ret]	"+m"	(_ret)				\
		: [_atom]	"m"	(atom) ,			\
		  [_seed]	"i"	(seed)				\
		: "%rax", "%rbx", "%rcx", "%rdx", "cc", "memory"	\
	);								\
	_ret;								\
})

#define BIT_ATOM_UNLOCK(_lock, atom, seed)				\
({									\
	volatile unsigned long long tries;				\
									\
	__asm__ volatile						\
	(								\
		"movq	%[count],	%%r12"		"\n\t"		\
	"1:"						"\n\t"		\
		"subq	$1	,	%%r12"		"\n\t"		\
		"jz	2f"				"\n\t"		\
		"movq	%[_seed],	%%rcx"		"\n\t"		\
		"movl	%%ecx	,	%%ebx"		"\n\t"		\
		"movq	$0xffffffff,	%%rax"		"\n\t"		\
		"andq	%%rax	,	%%rbx"		"\n\t"		\
		"shrq	$32	,	%%rcx"		"\n\t"		\
		"xorq	%%rdx	,	%%rdx"		"\n\t"		\
		"xorq	%%rax	,	%%rax"		"\n\t"		\
		_lock	"cmpxchg8b	%[_atom]"	"\n\t"		\
		"jnz	1b"				"\n\t"		\
	"2:"						"\n\t"		\
		"movq	%%r12	,	%[tries]"			\
		: [tries]	"+m"	(tries) 			\
		: [_atom]	"m"	(atom) ,			\
		  [_seed]	"i"	(seed) ,			\
		  [count]	"i"	(BIT_IO_RETRIES_COUNT)		\
		: "%rax", "%rbx", "%rcx", "%rdx", "%r12", "cc", "memory"\
	);								\
	tries;								\
})

#define _BITVAL_GPR(_lock,_base, _offset)				\
({									\
	Bit64 _tmp __attribute__ ((aligned (8))) = _base;		\
	volatile unsigned char _ret;					\
									\
	__asm__ volatile						\
	(								\
	_lock	"btcq	%%rdx, %[tmp]"		"\n\t"			\
		"setc	%[ret]" 					\
		: [ret] "+m" (_ret)					\
		: [tmp] "m" (_tmp),					\
		  "d" (_offset) 					\
		: "cc", "memory"					\
	);								\
	_ret;								\
})

#define _BITVAL_IMM(_lock, _base, _imm8)				\
({									\
	Bit64 _tmp __attribute__ ((aligned (8))) = _base;		\
	volatile unsigned char _ret;					\
									\
	__asm__ volatile						\
	(								\
	_lock	"btcq	%[imm8], %[tmp]"	"\n\t"			\
		"setc	%[ret]" 					\
		: [ret] "+m" (_ret)					\
		: [tmp] "m"  (_tmp),					\
		  [imm8] "i" (_imm8)					\
		: "cc", "memory"					\
	);								\
	_ret;								\
})

#define _BIT_TEST_GPR(_base, _offset)					\
({									\
	volatile unsigned char _ret;					\
									\
	__asm__ volatile						\
	(								\
		"btq	%%rdx, %[base]" 	"\n\t"			\
		"setc	%[ret]" 					\
		: [ret] "+m" (_ret)					\
		: [base] "m" (_base),					\
		  "d" ( _offset )					\
		: "cc", "memory"					\
	);								\
	_ret;								\
})

#define _BIT_TEST_IMM(_base, _imm8)					\
({									\
	volatile unsigned char _ret;					\
									\
	__asm__ volatile						\
	(								\
		"btq	%[imm8], %[base]"	"\n\t"			\
		"setc	%[ret]" 					\
		: [ret] "+m" (_ret)					\
		: [base] "m" (_base),					\
		  [imm8] "i" (_imm8)					\
		: "cc", "memory"					\
	);								\
	_ret;								\
})

#define BITVAL_2xPARAM(_base, _offset)					\
(									\
	__builtin_constant_p(_offset) ? 				\
		_BIT_TEST_IMM(_base, _offset)				\
	:	_BIT_TEST_GPR(_base, _offset)				\
)

#define BITVAL_3xPARAM(_lock, _base, _offset)				\
(									\
	__builtin_constant_p(_offset) ? 				\
		_BITVAL_IMM(_lock, _base, _offset)			\
	:	_BITVAL_GPR(_lock, _base, _offset)			\
)

#define BITVAL_DISPATCH(_1,_2,_3,BITVAL_CURSOR, ...)			\
	BITVAL_CURSOR

#define BITVAL(...)							\
	BITVAL_DISPATCH( __VA_ARGS__ ,	BITVAL_3xPARAM ,		\
					BITVAL_2xPARAM ,		\
					NULL)( __VA_ARGS__ )

#define PCI_CONFIG_ADDRESS(bus, dev, fn, reg)				\
	(0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

#define RDPCI(_data, _reg)						\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1,	%%eax"	"\n\t"				\
		"movl	$0xcf8,	%%edx"	"\n\t"				\
		"outl	%%eax,	%%dx"	"\n\t"				\
		"movl	$0xcfc,	%%edx"	"\n\t"				\
		"inl	%%dx,	%%eax"	"\n\t"				\
		"movl	%%eax,	%0"					\
		: "=m"	(_data)						\
		: "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define WRPCI(_data, _reg)						\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1,	%%eax"	"\n\t"				\
		"movl	$0xcf8,	%%edx"	"\n\t"				\
		"outl	%%eax,	%%dx"	"\n\t"				\
		"movl	%0,	%%eax"	"\n\t"				\
		"movl	$0xcfc,	%%edx"	"\n\t"				\
		"outl	%%eax,	%%dx"					\
		:							\
		: "irm" (_data),					\
		  "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define pr_warn(fmt, ...)	fprintf(stderr, fmt, __VA_ARGS__)

#define TIMESPEC(nsec)							\
({									\
	struct timespec tsec = {					\
		.tv_sec  = (time_t) 0,					\
		.tv_nsec = nsec						\
	};								\
	tsec;								\
})

#define udelay(interval)						\
({									\
	struct timespec rqtp = TIMESPEC(interval);			\
	nanosleep(&rqtp, NULL);						\
})

#define ATOMIC_SEED 0x436f726546726571LLU

#define BIT_IO_RETRIES_COUNT	80
#define BIT_IO_DELAY_INTERVAL	150	/*	in udelay() unit	*/

static Bit64	AMD_SMN_LOCK __attribute__ ((aligned (8)));

#define SMU_AMD_INDEX_REGISTER_F15H	PCI_CONFIG_ADDRESS(0, 0, 0, 0xb8)
#define SMU_AMD_DATA_REGISTER_F15H	PCI_CONFIG_ADDRESS(0, 0, 0, 0xbc)

#define SMU_AMD_INDEX_REGISTER_F17H	PCI_CONFIG_ADDRESS(0, 0, 0, 0x60)
#define SMU_AMD_DATA_REGISTER_F17H	PCI_CONFIG_ADDRESS(0, 0, 0, 0x64)
/* F17h PCI alternates addr: { 0xc4 , 0xc8 } - or - { 0xb4 , 0xb8 }	*/
#define SMU_AMD_INDEX_REGISTER_ALT_F17H PCI_CONFIG_ADDRESS(0, 0, 0, 0xc4)
#define SMU_AMD_DATA_REGISTER_ALT_F17H	PCI_CONFIG_ADDRESS(0, 0, 0, 0xc8)

#define AMD_FCH_READ16(_data, _reg)					\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1	,	%%eax"		"\n\t"		\
		"movl	$0xcd6	,	%%edx"		"\n\t"		\
		"outl	%%eax	,	%%dx"		"\n\t"		\
		"movl	$0xcd7	,	%%edx"		"\n\t"		\
		"inw	%%dx	,	%%ax"		"\n\t"		\
		"movw	%%ax	,	%0"				\
		: "=m"	(_data) 					\
		: "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define AMD_FCH_WRITE16(_data, _reg)					\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1	,	%%eax"		"\n\t"		\
		"movl	$0xcd6	,	%%edx"		"\n\t"		\
		"outl	%%eax	,	%%dx"		"\n\t"		\
		"movw	%0	,	%%ax" 		"\n\t"		\
		"movl	$0xcd7	,	%%edx"		"\n\t"		\
		"outw	%%ax	,	%%dx"		"\n\t"		\
		:							\
		: "im"	(_data),					\
		  "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define AMD_BIOS_READ16(_data, _reg)					\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1	,	%%eax"		"\n\t"		\
		"movl	$0xcd4	,	%%edx"		"\n\t"		\
		"outl	%%eax	,	%%dx"		"\n\t"		\
		"movl	$0xcd5	,	%%edx"		"\n\t"		\
		"inw	%%dx	,	%%ax"		"\n\t"		\
		"movw	%%ax	,	%0"				\
		: "=m"	(_data) 					\
		: "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define AMD_BIOS_WRITE16(_data, _reg)					\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1	,	%%eax"		"\n\t"		\
		"movl	$0xcd4	,	%%edx"		"\n\t"		\
		"outl	%%eax	,	%%dx"		"\n\t"		\
		"movw	%0	,	%%ax" 		"\n\t"		\
		"movl	$0xcd5	,	%%edx"		"\n\t"		\
		"outw	%%ax	,	%%dx"		"\n\t"		\
		:							\
		: "im"	(_data),					\
		  "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define AMD_PM2_READ16(_data, _reg)					\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1	,	%%eax"		"\n\t"		\
		"movl	$0xcd0	,	%%edx"		"\n\t"		\
		"outl	%%eax	,	%%dx"		"\n\t"		\
		"movl	$0xcd1	,	%%edx"		"\n\t"		\
		"inw	%%dx	,	%%ax"		"\n\t"		\
		"movw	%%ax	,	%0"				\
		: "=m"	(_data) 					\
		: "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

#define AMD_PM2_WRITE16(_data, _reg)					\
({									\
	__asm__ volatile						\
	(								\
		"movl	%1	,	%%eax"		"\n\t"		\
		"movl	$0xcd0	,	%%edx"		"\n\t"		\
		"outl	%%eax	,	%%dx"		"\n\t"		\
		"movw	%0	,	%%ax" 		"\n\t"		\
		"movl	$0xcd1	,	%%edx"		"\n\t"		\
		"outw	%%ax	,	%%dx"		"\n\t"		\
		:							\
		: "im"	(_data),					\
		  "ir"	(_reg)						\
		: "%rax", "%rdx", "memory"				\
	);								\
})

union DATA {
	unsigned int	dword;
	unsigned short	word[2];
};

void OTH_Read(union DATA *data, unsigned int addr)
{
	WRPCI(addr, SMU_AMD_INDEX_REGISTER_F15H);
	RDPCI(data->dword, SMU_AMD_DATA_REGISTER_F15H);
}

void PCI_Read(union DATA *data, unsigned int addr)
{
	RDPCI(data->dword, addr);
}

void PCI_Write(union DATA *data, unsigned int addr)
{
	WRPCI(addr, data->dword);
}

void SMU_Read(union DATA *data, unsigned int addr)
{
	WRPCI(addr, SMU_AMD_INDEX_REGISTER_ALT_F17H);
	RDPCI(data->dword, SMU_AMD_DATA_REGISTER_ALT_F17H);
#if DEBUG == 1
	printf(" SMU_Read (%08x, %08x)\n",addr, data->dword);
#endif
}

void SMU_Write(union DATA *data, unsigned int addr)
{
#if DEBUG == 1
	printf("SMU_Write (%08x, %08x)\n",addr, data->dword);
#endif
	WRPCI(addr, SMU_AMD_INDEX_REGISTER_ALT_F17H);
	WRPCI(SMU_AMD_DATA_REGISTER_ALT_F17H, data->dword);
}

void FCH_Read(union DATA *data, unsigned int addr)
{
	AMD_FCH_READ16(data->word[0], addr);
}

void FCH_WRITE(union DATA *data, unsigned int addr)
{
	AMD_FCH_WRITE16(data->word[0], addr);
}

void BIOS_Read(union DATA *data, unsigned int addr)
{
	AMD_BIOS_READ16(data->word[0], addr);
}

void BIOS_Write(union DATA *data, unsigned int addr)
{
	AMD_BIOS_WRITE16(data->word[0], addr);
}

void PM2_Read(union DATA *data, unsigned int addr)
{
	AMD_PM2_READ16(data->word[0], addr);
}

void PM2_Write(union DATA *data, unsigned int addr)
{
	AMD_PM2_WRITE16(data->word[0], addr);
}

/* Sources: PPR Vol 2 for AMD Family 19h Model 01h B1			*/
#ifndef HSMP_CMD_REG
	#define HSMP_CMD_REG 0x3b10534
#endif
#ifndef HSMP_ARG_REG
	#define HSMP_ARG_REG 0x3b109e0
#endif
#ifndef HSMP_RSP_REG
	#define HSMP_RSP_REG 0x3b10980
#endif

#define SMU_HSMP_REGISTERS	/*Cmd:*/HSMP_CMD_REG,	\
				/*Arg:*/HSMP_ARG_REG,	\
				/*Rsp:*/HSMP_RSP_REG

enum HSMP_FUNC {
	HSMP_TEST_MSG	= 0x1,	/* Returns [ARG0] + 1			*/
	HSMP_RD_SMU_VER = 0x2,	/* SMU FW Version			*/
	HSMP_RD_VERSION = 0x3,	/* Interface Version			*/
	HSMP_RD_CUR_PWR = 0x4,	/* Current Socket power (mWatts)	*/
	HSMP_WR_PKG_PL1 = 0x5,	/* Input within [31:0]; Limit (mWatts)	*/
	HSMP_RD_PKG_PL1 = 0x6,	/* Returns Socket power limit (mWatts)	*/
	HSMP_RD_MAX_PPT = 0x7,	/* Max Socket power limit (mWatts)	*/
	HSMP_WR_SMT_BOOST=0x8,	/* ApicId[31:16], Max Freq. (MHz)[15:0] */
	HSMP_WR_ALL_BOOST=0x9,	/* Max Freq. (MHz)[15:0] for ALL	*/
	HSMP_RD_SMT_BOOST=0xa,	/* Input ApicId[15:0]; Dflt Fmax[15:0]	*/
	HSMP_RD_PROCHOT = 0xb,	/* 1 = PROCHOT is asserted		*/
	HSMP_WR_XGMI_WTH= 0xc,	/* 0 = x2, 1 = x8, 2 = x16		*/
	HSMP_RD_APB_PST = 0xd,	/* Data Fabric P-state[7-0]={0,1,2,3}	*/
	HSMP_ENABLE_APB = 0xe,	/* Data Fabric P-State Performance Boost*/
	HSMP_RD_DF_MCLK = 0xf,	/* FCLK[ARG:0], MEMCLK[ARG:1] (MHz)	*/
	HSMP_RD_CCLK	= 0x10, /* CPU core clock limit (MHz)		*/
	HSMP_RD_PC0	= 0x11, /* Socket C0 Residency (100%)		*/
	HSMP_WR_DPM_LCLK= 0x12, /* NBIO[24:16]; Max[15:8], Min[7:0] DPM */
	HSMP_RESERVED	= 0x13,
	HSMP_RD_DDR_BW	= 0x14	/* Max[31:20];Usage{Gbps[19:8],Pct[7:0]}*/
};

enum {
	HSMP_UNSPECIFIED= 0x0,
	HSMP_RESULT_OK	= 0x1,
	HSMP_FAIL_BGN	= 0x2,
	HSMP_FAIL_END	= 0xfd,
	HSMP_INVAL_MSG	= 0xfe,
	HSMP_INVAL_INPUT= 0xff
};

#define ARG_DIM 8

typedef union
{
	unsigned int		value;
	struct
	{
		unsigned int
		bits		: 32-0;
	};
} HSMP_ARG;

#define AMD_HSMP_Mailbox(	MSG_FUNC,				\
				MSG_ARG,				\
				HSMP_CmdRegister,			\
				HSMP_ArgRegister,			\
				HSMP_RspRegister,			\
				SMU_IndexRegister,			\
				SMU_DataRegister )			\
({									\
	HSMP_ARG MSG_RSP = {.value = 0x0};				\
	HSMP_ARG MSG_ID = {.value = MSG_FUNC};				\
	unsigned int tries = BIT_IO_RETRIES_COUNT;			\
	unsigned char ret;						\
  do {									\
	ret = BIT_ATOM_TRYLOCK( BUS_LOCK,				\
				AMD_SMN_LOCK,				\
				ATOMIC_SEED );				\
    if ( ret == 0 ) {							\
	udelay(BIT_IO_DELAY_INTERVAL);					\
    }									\
    else								\
    {									\
	unsigned int idx;						\
	unsigned char wait;						\
									\
	WRPCI(HSMP_RspRegister	, SMU_IndexRegister);			\
	WRPCI(MSG_RSP.value	, SMU_DataRegister);			\
									\
	for (idx = 0; idx < ARG_DIM; idx++) {				\
		WRPCI(HSMP_ArgRegister + (idx << 2), SMU_IndexRegister);\
		WRPCI(MSG_ARG[idx].value, SMU_DataRegister);		\
	}								\
	WRPCI(HSMP_CmdRegister	, SMU_IndexRegister);			\
	WRPCI(MSG_ID.value	, SMU_DataRegister);			\
									\
	idx = BIT_IO_RETRIES_COUNT;					\
	do {								\
		WRPCI(HSMP_RspRegister	, SMU_IndexRegister);		\
		RDPCI(MSG_RSP.value	, SMU_DataRegister);		\
									\
		idx--;							\
		wait = (idx != 0) && (MSG_RSP.value == 0x0) ? 1 : 0;	\
		if (wait == 1) {					\
			udelay(BIT_IO_DELAY_INTERVAL);			\
		}							\
	} while (wait == 1);						\
	if (idx == 0) { 						\
		pr_warn("CoreFreq: AMD_HSMP_Mailbox(%x) Timeout\n",	\
			MSG_FUNC);					\
	}								\
	else if (MSG_RSP.value == 0x1)					\
	{								\
	    for (idx = 0; idx < ARG_DIM; idx++) {			\
		WRPCI(HSMP_ArgRegister + (idx << 2), SMU_IndexRegister);\
		RDPCI(MSG_ARG[idx].value, SMU_DataRegister);		\
	    }								\
	}								\
	BIT_ATOM_UNLOCK(BUS_LOCK,					\
			AMD_SMN_LOCK,					\
			ATOMIC_SEED);					\
    }									\
	tries--;							\
  } while ( (tries != 0) && (ret != 1) );				\
  if (tries == 0) {							\
	pr_warn("CoreFreq: AMD_HSMP_Mailbox(%x) TryLock\n", MSG_FUNC);	\
  }									\
	MSG_RSP.value;							\
})

#define IS_HSMP_OOO(_rx) (_rx == HSMP_UNSPECIFIED			\
			|| (_rx >= HSMP_FAIL_BGN && _rx <= HSMP_FAIL_END))

#define RESET_ARRAY(_array, _cnt, _val, ... )				\
({									\
	unsigned int rst;						\
	for (rst = 0; rst < _cnt; rst++) {				\
		_array[rst] __VA_ARGS__ = _val;				\
	}								\
})

#define COPY_ARRAY(_dest, _source, _cnt, _d_mbr, _s_mbr)		\
({									\
	unsigned int idx;						\
	for (idx = 0; idx < _cnt; idx++) {				\
		_dest[idx] _d_mbr = _source[idx] _s_mbr;		\
	}								\
})

unsigned int AMD_HSMP_Exec(	enum HSMP_FUNC MSG_FUNC,
				HSMP_ARG MSG_ARG[],
				unsigned int HSMP_CmdRegister,
				unsigned int HSMP_ArgRegister,
				unsigned int HSMP_RspRegister,
				unsigned int SMU_IndexRegister,
				unsigned int SMU_DataRegister )
{
	return(AMD_HSMP_Mailbox(MSG_FUNC,
				MSG_ARG,
				HSMP_CmdRegister,
				HSMP_ArgRegister,
				HSMP_RspRegister,
				SMU_IndexRegister,
				SMU_DataRegister));
}

void HSMP_Read(union DATA *data, unsigned int addr)
{
	unsigned int rx;
	enum HSMP_FUNC msg = (enum HSMP_FUNC) addr;
	HSMP_ARG arg[ARG_DIM];

	COPY_ARRAY(arg, data, ARG_DIM, .value, .dword);

	rx = AMD_HSMP_Exec(	msg, arg,
				SMU_HSMP_REGISTERS,
				SMU_AMD_INDEX_REGISTER_F17H,
				SMU_AMD_DATA_REGISTER_F17H );

	if (rx == HSMP_RESULT_OK) {
		COPY_ARRAY(data, arg, ARG_DIM, .dword, .value);
	} else if (IS_HSMP_OOO(rx)) {
		pr_warn("HSMP mailbox error code %u\n", rx);
	}
}

#define ZEN2_Read	HSMP_Read

#define ZEN3_Read	HSMP_Read

#define MAX_CHANNELS	8
#define SMU_AMD_UMC_BASE_CHA_F17H( _bar, _cha )	( _bar + (_cha << 20) )

void UMC_Read(union DATA *data, unsigned int _addr)
{
	unsigned int addr = _addr == 0x0 ? 0x00050000 : _addr;
	unsigned int UMC_BAR[MAX_CHANNELS] = { 0,0,0,0,0,0,0,0 };
	unsigned short ChannelCount = 0, cha, chip, sec;

	printf("\nData Fabric: scanning UMC @ BAR[0x%08x] : ", addr);
    for (cha = 0; cha < MAX_CHANNELS; cha++)
    {
	union DATA SdpCtrl = {.dword = 0};

	SMU_Read(&SdpCtrl, SMU_AMD_UMC_BASE_CHA_F17H(addr, cha) + 0x104);

	if ((SdpCtrl.dword != 0xffffffff) && (BITVAL(SdpCtrl.dword, 31)))
	{
		UMC_BAR[ChannelCount++] = SMU_AMD_UMC_BASE_CHA_F17H(addr, cha);
	}
	printf("%u ", cha);
    }
	printf("for %u Channels\n\n", ChannelCount);

    for (cha = 0; cha < ChannelCount; cha++)
    {
	unsigned long long DIMM_Size = 0;

	const unsigned int CHIP_BAR[2][2] = {
	[0] =	{
		[0] = UMC_BAR[cha] + 0x0,
		[1] = UMC_BAR[cha] + 0x20
		},
	[1] =	{
		[0] = UMC_BAR[cha] + 0x10,
		[1] = UMC_BAR[cha] + 0x28
		}
	};
	for (chip = 0; chip < 4; chip++)
	{
	    for (sec = 0; sec < 2; sec++)
	    {
		union DATA ChipReg, MaskReg;
		unsigned int addr[2], state, rank = 0;

		addr[1] = CHIP_BAR[sec][1] + 4 * (chip >> 1);

		SMU_Read(&MaskReg, addr[1]);

		if ((rank == 0) && (MaskReg.dword != 0)) {
			rank = BITVAL(MaskReg.dword, 9) ? 1 : 2;
		}
		if (rank == 2) {
			addr[0] = CHIP_BAR[sec][0] + 4 * chip;
		} else {
			addr[0] = CHIP_BAR[sec][0] + 4 * (chip - (chip > 2));
		}
		SMU_Read(&ChipReg, addr[0]);

		state = BITVAL(ChipReg.dword, 0);

		printf("CHA[%u] CHIP[%u:%u] @ 0x%08x[0x%08x] %sable, Rank=%u\n",
			cha, chip, sec, addr[0], ChipReg.dword,
			state ? "En":"Dis", rank);

		if (state)
		{
			unsigned int chipSize;

			__asm__ volatile
			(
			"DECODER:"				"\n\t"
				"xorl	%%edx, %%edx"		"\n\t"
				"bsrl	%[base], %%ecx" 	"\n\t"
				"jz	1f"			"\n\t"
				"incl	%%edx"			"\n\t"
				"shll	%%ecx, %%edx"	 	"\n\t"
				"negl	%%edx"			"\n\t"
				"notl	%%edx"			"\n\t"
				"andl	$0xfffffffe, %%edx"	"\n\t"
				"shrl	$2, %%edx"		"\n\t"
				"incl	%%edx"			"\n\t"
			"1:"					"\n\t"
				"movl	%%edx, %[dest]"
				: [dest] "=m" (chipSize)
				: [base] "m"  (MaskReg.dword)
				: "cc", "memory", "%ecx", "%edx"
			);

			DIMM_Size += chipSize;

		printf( "CHA[%u] MASK[%u:%u] @ 0x%08x[0x%08x] ChipSize[%u]\n",
			cha, chip, sec, addr[1], MaskReg.dword, chipSize );
		} else {
		printf( "CHA[%u] MASK[%u:%u] @ 0x%08x[0x%08x]\n",
			cha, chip, sec, addr[1], MaskReg.dword );
		}
	    }
	}
	printf( "\nDIMM Size[%llu KB] [%llu MB]\n\n",
		DIMM_Size, (DIMM_Size >> 10) );
    }
}

const char *BIN[0x10] =	{
	"0000",
	"0001",
	"0010",
	"0011",

	"0100",
	"0101",
	"0110",
	"0111",

	"1000",
	"1001",
	"1010",
	"1011",

	"1100",
	"1101",
	"1110",
	"1111",
};

void Convert2Binary(unsigned long long value, char *pBinStr)
{
	unsigned int I, H = 0xf;
	for (I = 1; I <= 16; I++)
	{
		const unsigned int B =H<<2, nibble = value & 0xf;

		pBinStr[B  ] = BIN[nibble][0];
		pBinStr[B+1] = BIN[nibble][1];
		pBinStr[B+2] = BIN[nibble][2];
		pBinStr[B+3] = BIN[nibble][3];

		H--;
		value = value >> 4;
	}
}

void PrettyBin(char *pBinStr)
{
	unsigned short bit;
	for (bit = 63; bit > 0; bit--) {
		if (bit % 4 == 0) {
			printf("%02hu", bit);
		} else {
			printf(" ");
		}
	}
	printf("00\n ");
	for (bit = 0; bit < 64; bit++) {
		printf("%c", pBinStr[bit]);
		if (bit < 63 && bit % 4 == 3) {
			printf(" ");
		}
	}
}

enum OP {
	READ,
	WRITE,
	OPS
};

enum IC {
	SMU,
	FCH,
	UMC,
	PCI,
	BIOS,
	PM2,
	ZEN1,
	ZEN12,
	ZEN2,
	ZEN3,
	HSMP,
	OTH,
	LAST
};

char *component[LAST] = {
	[SMU] = "smu" , [FCH] = "fch", [UMC] = "umc", [PCI] = "pci",
	[BIOS] = "bios", [PM2] = "pm2",
	[ZEN1] = "zen1", [ZEN12] = "zen12", [ZEN2] = "zen2", [ZEN3] = "zen3",
	[HSMP] = "hsmp", [OTH] = "oth" 
};

void (*IC_Func[LAST][OPS])(union DATA*, unsigned int) = {
	[SMU]	= { SMU_Read, SMU_Write },
	[FCH]	= { FCH_Read, FCH_WRITE },
	[UMC]	= { UMC_Read, NULL	},
	[PCI]	= { PCI_Read, PCI_Write },
	[BIOS]	= { BIOS_Read,BIOS_Write},
	[PM2]	= { PM2_Read, PM2_Write },
	[ZEN1]	= { NULL,	NULL	},
	[ZEN12] = { NULL,	NULL	},
	[ZEN2]	= { ZEN2_Read,	NULL	},
	[ZEN3]	= { ZEN3_Read,	NULL	},
	[HSMP]	= { HSMP_Read,	NULL	},
	[OTH]	= { OTH_Read, NULL	}
};

void Help_Argument(void)
{
	enum IC ic;
	for (ic = SMU; ic < LAST; ic++) {
		printf(" %s", component[ic]);
	}
}

void Help_Usage(int rc, char *ctx)
{
	switch (rc) {
	case 6:
		printf("Sorry: %s Operation Unimplemented\n", ctx);
		break;
	case 5:
		printf("Prerequisite: IOPL Not Permitted\n\n");
		break;
	case 4:
		printf("Prerequisite: Missing root privileges\n");
		break;
	case 3:
		printf("Syntax: Invalid Hexadecimal Address or Data value\n" \
			"\tExpected  <addr> and <data> like 0x1a2b3c4d\n"	\
			"\tOr"					\
			"\t  <addr> like bus:0x1-dev:0x2-fn:0x3-reg:0xff\n");
		break;
	case 2:
		printf("Syntax: '%s' Undefined component\n", ctx);
		break;
	case 1:
	default:
		printf( "Usage: %s <component> [<addr>] [<data> ... <data>]\n" \
			"Where: <component> is one of {\n\t", ctx );
		Help_Argument();
		printf("\n}\n");
		break;
	}
}

int main(int argc, char *argv[])
{
	BIT_ATOM_INIT(AMD_SMN_LOCK, ATOMIC_SEED);
	int rc = 0;
    if (argc < 2) {
	rc = 1;
	Help_Usage(rc, argv[0]);
    }
    else
    {
	enum IC ic;
	for (ic = SMU; ic < LAST; ic++)
	{
		const size_t nc = strlen(component[ic]);
	    if ((strlen(argv[1]) == nc) && !strncmp(component[ic], argv[1], nc))
	    {
		break;
	    }
	}
	if (ic == LAST) {
		rc = 2;
		Help_Usage(rc, argv[1]);
	}
	else
	{
		union DATA data __attribute__ ((aligned (4))) = { .dword = 0 };
		unsigned int addr = 0x0;
		char tr = 0;
	    if (argc > 3)
	    {
		if (1 != sscanf(argv[3], "0x%x%c", &data.dword, &tr)) {
			rc = 3;
			Help_Usage(rc, argv[0]);
		}
	    }
	    if (argc > 2) {
	      if (1 != sscanf(argv[2], "0x%x%c", &addr, &tr))
	      {
		unsigned char _bus, _dev, _fn, _reg;
		if (4 != sscanf(argv[2], "bus:0x%x-dev:0x%x-fn:0x%x-reg:0x%x%c",
				&_bus, &_dev, &_fn, &_reg, &tr))
		{
			rc = 3;
			Help_Usage(rc, argv[0]);
		} else {
			addr = PCI_CONFIG_ADDRESS(_bus, _dev, _fn, _reg);
		}
	      }
	    }
	    if (rc == 0)
	    {
		uid_t uid = geteuid();
		if (uid != 0) {
			rc = 4;
			Help_Usage(rc, argv[0]);
		}
		else if (!iopl(3))
		{
		  if (rc == 0)
		  {
			char binStr[64];
			const enum OP op = (argc > 3) ? WRITE : READ;
			char *what[OPS] = { "READ" , "WRITE" };

		    if (ic == UMC)
		    {
			if (IC_Func[ic][op] != NULL)
			{
				IC_Func[ic][op](&data, addr);
			}
			else
			{
				rc = 6;
				Help_Usage(rc, what[op]);
			}
		    }
		    else
			if((ic == ZEN1)
			|| (ic == ZEN12)
			|| (ic == ZEN2)
			|| (ic == ZEN3)
			|| (ic == HSMP))
		    {
			union DATA out[ARG_DIM];
			unsigned int arg, idx;

			RESET_ARRAY(out, ARG_DIM, 0, .dword);
		      for (arg = 3, idx = 0;
				arg < argc && idx < ARG_DIM;
					arg++, idx++)
		      {
			char tr = 0;
			if(1 != sscanf(argv[arg],"0x%x%c",&out[idx].dword, &tr))
			{
				rc = 3;
				Help_Usage(rc, argv[0]);
			}
		      }
		      if ((IC_Func[ic][READ] != NULL) && (rc == 0))
		      {
			printf( "[0x%08x] %s(%s)\n",
				addr, what[READ], component[ic] );

			IC_Func[ic][READ](out, addr);

			for (idx = 0; idx < ARG_DIM; idx++)
			{
				printf( "\n0x%08x (%u)\n",
					out[idx].dword, out[idx].dword );

				Convert2Binary(out[idx].dword, binStr);
				PrettyBin(binStr);

				printf("\n");
			}
		      }
		      else if (IC_Func[ic][READ] == NULL)
		      {
			rc = 6;
			Help_Usage(rc, what[READ]);
		      }
		    }
		    else if (argc > 2)
		    {
		      if (IC_Func[ic][op] != NULL)
		      {
			IC_Func[ic][op](&data, addr);

			printf("[0x%08x] %s(%s) = 0x%08x (%u)\n", addr,
				what[op], component[ic],
				data.dword, data.dword);

			Convert2Binary(data.dword, binStr);
			PrettyBin(binStr);

			printf("\n");
		      }
		      else
		      {
			rc = 6;
			Help_Usage(rc, what[op]);
		      }
		    } else {
			rc = 1;
			Help_Usage(rc, argv[0]);
		    }
		  }
			iopl(0);
		} else {
			rc = 5;
			Help_Usage(rc, argv[0]);
		}
	    }
	}
    }
	return (rc);
}
