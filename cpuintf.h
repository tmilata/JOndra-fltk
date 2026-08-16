#ifndef CPUINTF_H
#define CPUINTF_H

//enum reg {A=0,F,B,C,D,E,H,L,IXH,IXL,IYH,IYL,SPL,SPH,I,R,PC,AF,BC,DE,HL,IX,IY,SP,iff1,iff2};

//typedef enum reg reg;

enum reg {
    REG_A = 0, REG_F, REG_B, REG_C, REG_D, REG_E, REG_H, REG_L,
    REG_IXH, REG_IXL, REG_IYH, REG_IYL, REG_SPL, REG_SPH, REG_I, REG_R, 
    REG_PC, REG_AF, REG_BC, REG_DE, REG_HL, REG_IX, REG_IY, REG_SP, 
    REG_IFF1, REG_IFF2
};

typedef enum reg reg;


uint8_t CPU_Create(void);
void CPU_Reset(uint8_t cpuid);
void CPU_Destroy(uint8_t cpuid);
int CPU_Interrupt(uint8_t cpuid);
int CPU_NMI(uint8_t cpuid);
uint64_t CPU_Emulate(uint8_t cpuid, uint64_t ticks);
void CPU_RequestBreak(uint8_t cpuid);
void CPU_ClearBreak(uint8_t cpuid);
void CPU_SetTempBreakpoint(uint8_t cpuid, uint16_t address);
void CPU_ClearTempBreakpoint(uint8_t cpuid);
bool CPU_TempBreakpointHit(uint8_t cpuid);
void CPU_SetUserBreakpointSlot(uint8_t cpuid, int slot, uint16_t address, bool enabled);
void CPU_ClearUserBreakpointHit(uint8_t cpuid);
bool CPU_UserBreakpointHit(uint8_t cpuid);
void CPU_SuspendUserBreakpoints(uint8_t cpuid, bool suspend);

#define CPU_MEM_BREAK_NONE  0
#define CPU_MEM_BREAK_READ  1
#define CPU_MEM_BREAK_WRITE 2

void CPU_SetMemoryReadBreakpoint(uint8_t cpuid, uint16_t address, bool enabled);
void CPU_SetMemoryWriteBreakpoint(uint8_t cpuid, uint16_t address, bool enabled);
bool CPU_CheckMemoryReadBreakpoint(uint8_t cpuid, uint16_t address);
bool CPU_CheckMemoryWriteBreakpoint(uint8_t cpuid, uint16_t address);
int CPU_MemoryBreakpointHit(uint8_t cpuid);
void CPU_ClearMemoryBreakpointHit(uint8_t cpuid);
void CPU_ArmMemoryBreakpointResume(uint8_t cpuid);

uint8_t CPU_GetReg8(uint8_t cpuid, reg rWhich);
uint16_t CPU_GetReg16(uint8_t cpuid, reg rWhich);
uint16_t CPU_GetReg16Alt(uint8_t cpuid, reg rWhich);
void CPU_PutReg8(uint8_t cpuid, reg rWhich, unsigned char value);
void CPU_PutReg8Alt(uint8_t cpuid, reg rWhich, unsigned char value);
void CPU_SetPC(uint8_t cpuid, unsigned short value);
void CPU_SetIff(uint8_t cpuid, reg rWhich, int value);
uint8_t CPU_GetIff(uint8_t cpuid, reg rWhich);
void CPU_SetIntMode(uint8_t cpuid, int value);
uint8_t CPU_GetIntMode(uint8_t cpuid);
bool CPU_GetPendingEI(uint8_t cpuid);
void CPU_SetPendingEI(uint8_t cpuid, bool pending);
void CPU_PrepareTimelineRestore(uint8_t cpuid);
bool CPU_getHaltFlag(uint8_t cpuid);
void CPU_setHaltFlag(uint8_t cpuid, bool halted);
void CPU_resetHaltFlag(uint8_t cpuid);
uint64_t CPU_getCycles(uint8_t cpuid);

#endif