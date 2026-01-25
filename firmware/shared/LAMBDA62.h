// Opcodes and relevant parameters
#define OP_SETTX 0x83
#define OP_SETRX 0x82

#define OP_STDBY 0x80

#define OP_WREG 0x0D
#define OP_RREG 0x1D
#define OP_WBUFF 0x0E
#define OP_RBUFF 0x1E

#define OP_SETIRQ 0x08
#define OP_GETIRQ 0x12
#define OP_CLEARIRQ 0x02

#define OP_SETRFFREQ 0x86
#define OP_SETPACFG 0x95

#define OP_SETPKTTYPE 0x8A
#define OP_SETTXPARAMS 0x8E
#define OP_SETMODPARAMS 0x8B
#define OP_SETPKTPARAMS 0x8C

#define OP_SETBUFFBASEADDR 0x8F
#define OP_GETRXBUFFSTATUS 0x13

#define OP_GETPKTSTATUS 0x14
#define OP_GETINSTRSSI 0x15

#define OP_GETSTATUS 0xC0
#define OP_GETSTATS 0x10
#define OP_RESETSTATS 0x00

#define OP_NOP 0x00

// Registers
#define RXGAIN 0x08AC