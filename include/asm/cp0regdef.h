#ifndef _cp0regdef_h_
#define _cp0regdef_h_

#define CP0_INDEX $0
#define CP0_RANDOM $1
#define CP0_ENTRYLO0 $2
#define CP0_ENTRYLO1 $3
#define CP0_CONTEXT $4
#define CP0_PAGEMASK $5
#define CP0_WIRED $6
#define CP0_BADVADDR $8
#define CP0_COUNT $9
#define CP0_ENTRYHI $10
#define CP0_COMPARE $11
#define CP0_STATUS $12
#define CP0_CAUSE $13
#define CP0_EPC $14
#define CP0_PRID $15
#define CP0_EBASE $15, 1
#define CP0_CONFIG $16
#define CP0_LLADDR $17
#define CP0_WATCHLO $18
#define CP0_WATCHHI $19
#define CP0_XCONTEXT $20
#define CP0_FRAMEMASK $21
#define CP0_DIAGNOSTIC $22
#define CP0_PERFORMANCE $25
#define CP0_ECC $26
#define CP0_CACHEERR $27
#define CP0_TAGLO $28
#define CP0_TAGHI $29
#define CP0_ERROREPC $30

#define STATUS_CU3 0x80000000
#define STATUS_CU2 0x40000000
#define STATUS_CU1 0x20000000
#define STATUS_CU0 0x10000000
#define STATUS_BEV 0x00400000
#define STATUS_IM0 0x0100
#define STATUS_IM1 0x0200
#define STATUS_IM2 0x0400
#define STATUS_IM3 0x0800
#define STATUS_IM4 0x1000
#define STATUS_IM5 0x2000
#define STATUS_IM6 0x4000
#define STATUS_IM7 0x8000
#define STATUS_UM 0x0010
#define STATUS_R0 0x0008
#define STATUS_ERL 0x0004
#define STATUS_EXL 0x0002//表示异常级别被锁定，当前异常处理程序执行完毕前，不会响应其他异常。
#define STATUS_IE 0x0001//中断使能
#endif
