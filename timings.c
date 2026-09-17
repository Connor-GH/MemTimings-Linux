/*
 * zencli.c by CyrIng
 *
 * Copyright (C) 2020-2021 CYRIL INGENIERIE
 * Licenses: GPL2
 *
 */

#if !defined(DDR4) && !defined(DDR5)
#define DDR4 1
#define DDR5 0
#endif
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <unistd.h>

#ifdef __clang__
#define NONNULL _Nonnull
#else
#define NONNULL
#endif

// Format found in AMD Ryzen 15H BKDG (BIOS and Kernel Developer's Guide)
// Under "IO Space Registers", and IOCF8.
// IOCF8 (0xCF8) is the config address, and IOCFC (0xCFC) is the data port.
// "~3" because the bottom two bits are reserved.
#define PCI_CONFIG_ADDRESS(bus, dev, fn, reg)                                  \
  (0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

#define RDPCI(_data, _reg)                                                     \
  do                                                                           \
    __asm__ volatile("movl	%1,	%%eax\n\t"                                      \
                     "movl	$0xcf8,	%%edx\n\t"                                  \
                     "outl	%%eax,	%%dx\n\t"                                    \
                     "movl	$0xcfc,	%%edx\n\t"                                  \
                     "inl	%%dx,	%%eax\n\t"                                     \
                     "movl	%%eax,	%0\n\t"                                      \
                     : "=m"(_data)                                             \
                     : "ir"(_reg)                                              \
                     : "%rax", "%rdx", "memory");                              \
  while (0)

#define WRPCI(_data, _reg)                                                     \
  do                                                                           \
    __asm__ volatile("movl	%1,	%%eax\n\t"                                      \
                     "movl	$0xcf8,	%%edx\n\t"                                  \
                     "outl	%%eax,	%%dx\n\t"                                    \
                     "movl	%0,	%%eax\n\t"                                      \
                     "movl	$0xcfc,	%%edx\n\t"                                  \
                     "outl	%%eax,	%%dx\n\t"                                    \
                     :                                                         \
                     : "irm"(_data), "ir"(_reg)                                \
                     : "%rax", "%rdx", "memory");                              \
  while (0)

#define SMU_AMD_INDEX_REGISTER_F15H PCI_CONFIG_ADDRESS(0, 0, 0, 0xb8)
#define SMU_AMD_DATA_REGISTER_F15H PCI_CONFIG_ADDRESS(0, 0, 0, 0xbc)

#define SMU_AMD_INDEX_REGISTER_F17H PCI_CONFIG_ADDRESS(0, 0, 0, 0x60)
#define SMU_AMD_DATA_REGISTER_F17H PCI_CONFIG_ADDRESS(0, 0, 0, 0x64)
/* F17h PCI alternates addr: { 0xc4 , 0xc8 } - or - { 0xb4 , 0xb8 }	*/
#define SMU_AMD_INDEX_REGISTER_ALT_F17H PCI_CONFIG_ADDRESS(0, 0, 0, 0xc4)
#define SMU_AMD_DATA_REGISTER_ALT_F17H PCI_CONFIG_ADDRESS(0, 0, 0, 0xc8)

#define display_error(...)                                                     \
  {                                                                            \
    fprintf(stdout, "error: " __VA_ARGS__);                                    \
    fprintf(stdout, "\n");                                                     \
    exit(EXIT_FAILURE);                                                        \
  }

#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define dbg_printf(...)                                                        \
  do                                                                           \
    if (DEBUG) {                                                               \
      printf("DEBUG: " __VA_ARGS__);                                           \
    }                                                                          \
  while (0)

static void smu_read(uint32_t *restrict NONNULL dword, unsigned int addr) {
  dbg_printf("smu_read(%08x, %08x)\n", addr, *dword);

  WRPCI(addr, SMU_AMD_INDEX_REGISTER_ALT_F17H);
  RDPCI(*dword, SMU_AMD_DATA_REGISTER_ALT_F17H);
}

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

enum refresh_mode {
  REFRESH_MODE_NORMAL,
  REFRESH_MODE_FGR,
  REFRESH_MODE_MIXED,
  REFRESH_MOE_PBONLY,
};
struct smu_timings {
  double MCLK;
  double MCLK_mts;
  bool Cmd_Rate;
  bool Gear_Down_Mode;
  bool Power_Down_Mode;

  bool Bank_Group_Swap;
  bool Bank_Group_Swap_Alt;

  u8 tCL;
  u8 tRAS;
  u8 tRCDRD;
  u8 tRCDWR;

  u8 tRC;
  u8 tRCpb; // tRC per-bank
  u8 tRP;
  u8 tRPpb; // tRP per-bank

  u8 tRRDS;
  u8 tRRDL;
  u8 tRRDDLR; // tRRD Different Logical Ranks
  u8 tRTP;

  u8 tFAW;
  u8 tFAWSLR; // tFAW Same Logical Rank
  u8 tFAWDLR; // tFAW Different Logical Rank

  u8 tCWL;
  u8 tWTRS;
  u8 tWTRL;

  u8 tWR;

  u8 tRDRDDD; // read-read different DIMM
  u8 tRDRDSD; // read-read same DIMM
  u8 tRDRDSC; // read-read same channel
  u8 tRDRDSCL;

  u8 tWRWRDD; // write-write different DIMM
  u8 tWRWRSD;
  u8 tWRWRSC;
  u8 tWRWRSCL;

  u8 tWRRD;
  u8 tRDWR;

  u16 tREFI;
  double tREFI_ns;

  u8 tMODPDA;
  u8 tMRDPDA;
  u8 tMOD;
  u8 tMRD;

  u8 tSTAG;

  u8 tCKE;

  u8 tPHYWRD;
  u8 tPHYRDL;
  u8 tPHYWRL;

#if DDR5
  u8 tWRPRE;
  u8 tRDPRE;
#endif

#if DDR4
  u32 tRFC1 : 11, tRFC2 : 11, tRFC4 : 10;
#elif DDR5
  u32 tRFC1 : 16, tRFC2 : 16;
  u16 tRFCsb;
  double tRFCsb_ns;
  u8 RxData, TxData, CtrlLine;
  enum refresh_mode refresh_mode;
#endif
  double tRFC_ns;
};

#if DDR4
static void ddr4_timings(struct smu_timings *t) {
  unsigned int value = {}, value2 = {};
  smu_read(&value, 0x50260);
  smu_read(&value2, 0x50264);

  if (value != value2 && value == 0x21060138) {
    value = value2;
  }
  t->tRFC1 = value & 0x3ff;
  t->tRFC2 = (value >> 11) & 0x3ff;
  t->tRFC4 = (value >> 22) & 0x1ff;
}
#endif

static const char *display_refresh_mode(enum refresh_mode r) {
  switch (r) {
  case REFRESH_MODE_NORMAL:
    return "Normal";
  case REFRESH_MODE_MIXED:
    return "Mixed";
  case REFRESH_MODE_FGR:
    return "FGR";
  case REFRESH_MOE_PBONLY:
    return "Per-Bank Only";
  }
}

#if DDR5
static void ddr5_timings(struct smu_timings *t) {
  unsigned int value = {}, tRFC = {}, tRFCsb = {};
  unsigned int tRFC_addresses[4] = {0x50260, 0x50264, 0x50268, 0x5026c};
  for (int i = 0; i < sizeof(tRFC_addresses) / sizeof(*tRFC_addresses); i++) {
    smu_read(&value, tRFC_addresses[i]);
    if (value != 0x00c00138) {
      tRFC = value;
    }
  }
  t->tRFC1 = tRFC & 0xffff;
  t->tRFC2 = (tRFC >> 16) & 0xffff;

  smu_read(&value, 0x502a4);
  t->tRDPRE = value & 0b11;
  t->tWRPRE = (value >> 8) & 0b11;

  unsigned int tRFCsb_addresses[4] = {0x502c0, 0x502c4, 0x502c8, 0x502cc};
  for (int i = 0; i < sizeof(tRFCsb_addresses) / sizeof(*tRFCsb_addresses);
       i++) {
    smu_read(&value, tRFCsb_addresses[i]);
    if (value != 0) {
      tRFCsb = value;
    }
  }
  t->tRFCsb = tRFCsb & 0xffff;
  t->tRFCsb_ns = t->tRFCsb * 2000.0 / t->MCLK_mts;

  smu_read(&value, 0x50284);
  value = value & 0x3ff;
  t->CtrlLine = value & 0b11;
  t->TxData = (value >> 4) & 0b11;
  t->RxData = (value >> 8) & 0b11;

  enum refresh_mode refresh_mode = REFRESH_MODE_NORMAL;
  smu_read(&value, 0x5012c);
  // Fine-grained refresh -- tRFC2 is used
  u8 FGR = (value >> 16) & 0b11;
  bool per_bank_refresh = (value >> 1) & 0b1;

  if (!per_bank_refresh) {
    if (FGR) {
      refresh_mode = REFRESH_MODE_FGR;
    } else {
      refresh_mode = REFRESH_MODE_NORMAL;
    }
  } else {
    if (FGR) {
      refresh_mode = REFRESH_MODE_MIXED;
    } else {
      refresh_mode = REFRESH_MOE_PBONLY;
    }
  }
  t->refresh_mode = refresh_mode;
}
#endif

static void smu_get_mem_timings(struct smu_timings *t) {
  u32 value, value2;

  smu_read(&value, 0x50050);
  smu_read(&value2, 0x50058);
  t->Bank_Group_Swap = !(value == value2 && value == 0x87654321);

  smu_read(&value, 0x500D0);
  smu_read(&value2, 0x500D4);
  t->Bank_Group_Swap_Alt = ((value >> 4 & 0x7F)) || ((value2 >> 4 & 0x7F));

  smu_read(&value, 0x5012C);
  t->Power_Down_Mode = (value >> 28) & 0b1;

  smu_read(&value, 0x50200);
#if DDR5
  t->MCLK = (value & 0xffff);
  t->Cmd_Rate = (value >> 17) & 0b1;
  t->Gear_Down_Mode = (value >> 18) & 0b1;
#elif DDR4
  t->MCLK = (value & 0x7f) * 100 /* bclk */ / 3.0;
  t->Cmd_Rate = (value >> 10) & 0b1;
  t->Gear_Down_Mode = (value >> 11) & 0b1;
#endif
  t->MCLK_mts = t->MCLK * 2;

  smu_read(&value, 0x50204);
  t->tCL = (value) & 0xff;
  t->tRAS = (value >> 8) & 0xff;
  t->tRCDRD = (value >> 16) & 0b111111;
  t->tRCDWR = (value >> 24) & 0b111111;

  smu_read(&value, 0x50208);
  t->tRC = (value) & 0xff;
  t->tRCpb = (value >> 8) & 0xff;
  t->tRP = (value >> 16) & 0b111111;
  t->tRPpb = (value >> 24) & 0b111111;

  smu_read(&value, 0x5020C);
  t->tRRDS = (value) & 0b11111;
  t->tRRDL = (value >> 8) & 0b11111;
  t->tRRDDLR = (value >> 16) & 0b11111;
  t->tRTP = (value >> 24) & 0b11111;

  smu_read(&value, 0x50210);
  t->tFAW = value & 0xff;
  t->tFAWSLR = (value >> 18) & 0b111111;
  t->tFAWDLR = (value >> 25) & 0b111111;

  smu_read(&value, 0x50214);
  t->tCWL = value & 0b111111;
  t->tWTRS = (value >> 8) & 0b11111;
  t->tWTRL = (value >> 16) & 0b111111;

  smu_read(&value, 0x50218);
  t->tWR = value & 0xff;

  smu_read(&value, 0x50220);
  t->tRDRDDD = value & 0b1111;
  t->tRDRDSD = (value >> 8) & 0xf;
  t->tRDRDSC = (value >> 16) & 0xf;
  t->tRDRDSCL = (value >> 24) & 0b111111;

  smu_read(&value, 0x50224);
  t->tWRWRDD = value & 0b1111;
  t->tWRWRSD = (value >> 8) & 0xf;
  t->tWRWRSC = (value >> 16) & 0xf;
  t->tWRWRSCL = (value >> 24) & 0b111111;

  smu_read(&value, 0x50228);
  t->tWRRD = value & 0xf;
  t->tRDWR = (value >> 8) & 0b11111;

  smu_read(&value, 0x50230);
  t->tREFI = value & 0xffff;
  t->tREFI_ns = t->tREFI * 2000.0 / t->MCLK_mts;

  smu_read(&value, 0x50234);
  t->tMRD = value & 0b11111;
  t->tMOD = (value >> 8) & 0b11111;
  t->tMRDPDA = (value >> 16) & 0b11111;
  t->tMODPDA = (value >> 24) & 0b11111;

  smu_read(&value, 0x50250);
  t->tSTAG = (value >> 16) & 0x3ff;

  smu_read(&value, 0x50254);
  t->tCKE = (value >> 24) & 0b11111;

  smu_read(&value, 0x50258);
  t->tPHYWRL = (value >> 8) & 0x7f;
  t->tPHYRDL = (value >> 16) & 0x7f;
  t->tPHYWRD = (value >> 24) & 0b11;

  unsigned int tRFC_addresses[4] = {0x50260, 0x50264, 0x50268, 0x5026c};
  for (int i = 0; i < sizeof(tRFC_addresses) / sizeof(*tRFC_addresses); i++) {
    smu_read(&value, tRFC_addresses[i]);
  }
  smu_read(&value, 0x50260);
  smu_read(&value2, 0x50264);

  if (value != value2 && value == 0x21060138) {
    value = value2;
  }
#if DDR4
  ddr4_timings(t);
#elif DDR5
  ddr5_timings(t);
#endif
  t->tRFC_ns = t->tRFC1 * 2000.0 / t->MCLK_mts;
}

static const char *bool_to_str(bool b) { return b ? "Enabled" : "Disabled"; }

#define BLUE_COLOR "\033[1;34m"
#define CLEAR_COLOR "\033[0m"
#define B(x) BLUE_COLOR x CLEAR_COLOR

static void display_info_cli(const struct smu_timings *const t) {

  printf("%-12s " B("%.0f MT/s") "\n", "Speed:", t->MCLK * 2);
  printf("%-12s " B("%-12s") " %-12s " B("%s") "\n",
         "BGS:", bool_to_str(t->Bank_Group_Swap),
         "BGS Alt:", bool_to_str(t->Bank_Group_Swap_Alt));
  printf("%-12s " B("%-12s") " %-12s " B("%dT") "\n",
         "GDM:", bool_to_str(t->Gear_Down_Mode), "Cmd2T:", t->Cmd_Rate ? 2 : 1);
  puts("");

  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tCL:", t->tCL,
         "tRDRDSCL:", t->tRDRDSCL);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRCDWR:", t->tRCDWR,
         "tWRWRSCL:", t->tWRWRSCL);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRCDRD:", t->tRCDRD,
         "tCWL:", t->tCWL);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRP:", t->tRP,
         "tRTP:", t->tRTP);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRAS:", t->tRAS,
         "tRDWR:", t->tRDWR);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRC:", t->tRC,
         "tWRRD:", t->tWRRD);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRRDS:", t->tRRDS,
         "tRDRDSC:", t->tRDRDSC);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRRDL:", t->tRRDL,
         "tRDRDSD:", t->tRDRDSD);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tFAW:", t->tFAW,
         "tRDRDDD:", t->tRDRDDD);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tWTRS:", t->tWTRS,
         "tWRWRSC:", t->tWRWRSC);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tWTRL:", t->tWTRL,
         "tWRWRSD:", t->tWRWRSD);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tWR:", t->tWR,
         "tWRWRDD:", t->tWRWRDD);
  printf("%-12s " B("%-12.3f") " %-12s " B("%-12d") "\n",
         "tRFC (ns):", t->tRFC_ns, "tCKE:", t->tCKE);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRFC:", t->tRFC1,
         "tREFI:", t->tREFI);
  printf("%-12s " B("%-12d")
#if DDR4
             " %-12s " B("%-12.2f")
#elif DDR5
             " %-12s " B("%s")
#endif
                 "\n",
         "tRFC2:", t->tRFC2,
#if DDR4
         "tREFI (ns):", t->tREFI_ns
#elif DDR5
         "Ref. Mode:", display_refresh_mode(t->refresh_mode)
#endif

  );
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n",
#if DDR4
         "tRFC4:", t->tRFC4,
#else
         "tRFCsb:", t->tRFCsb,
#endif
         "tSTAG:", t->tSTAG);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tMOD:", t->tMOD,
         "tMRD:", t->tMRD);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tMODPDA:", t->tMODPDA,
         "tMRDPDA:", t->tMRDPDA);
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tPHYWRD:", t->tPHYWRD,
         "tPHYDRL:", t->tPHYRDL);
  printf("%-12s " B("%-12d") " %-12s " B("%-12s") "\n", "tPHYWRL:", t->tPHYWRL,
         "PowerDown:", bool_to_str(t->Power_Down_Mode));
#if DDR5
  printf("%-12s " B("%-12d") " %-12s " B("%-12d") "\n", "tRDPRE:", t->tRDPRE,
         "tWRPRE:", t->tWRPRE);
  printf("%-12s " B("%d/%d/%d") "\n", "Nitro:", t->RxData, t->TxData,
         t->CtrlLine);
#endif
}

int main(int argc, char **argv) {
  if (argc > 1 && strcmp(argv[1], "-h") == 0) {
    printf("usage: %s [-h]\n", argv[0]);
    printf("  -h   display this help info\n");
    printf("\n");
    printf("root/sudo privileges are required for reading "
           "and writing to raw I/O ports\n");
  }
  uid_t uid = geteuid();

  if (uid != 0) {
    display_error("need root for CAP_SYS_RAWIO");
  }
  if (iopl(3) != 0) {
    int tmp_errno = errno;
    display_error("failed to set I/O privilege level: %s", strerror(tmp_errno));
  }
  struct smu_timings t = {};

  smu_get_mem_timings(&t);
  display_info_cli(&t);
}
