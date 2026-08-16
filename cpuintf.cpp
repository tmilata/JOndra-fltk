/* cpuintf.c
 *
 * CPU Interface (mz80)
 *
 * Allows ZXEM to be used with different cpu cores
 *
 */


#include "z80emu.h"
#include "Ondra.h"
#include "Debug.h"
#include "cpuintf.h"
#include <stdlib.h>


#define MAX_CPU 10

Z80_STATE *zxcpu[MAX_CPU];
uint8_t num_cpus = -1;

// create CPU context and return a handle
uint8_t CPU_Create(void) {
	num_cpus++;
	if(num_cpus<MAX_CPU) {
		zxcpu[num_cpus]=(Z80_STATE*)malloc(sizeof(Z80_STATE));
		{
			int i;
			for (i = 0; i < 5; ++i) {
				zxcpu[num_cpus]->user_breakpoint_enabled[i] = 0;
				zxcpu[num_cpus]->user_breakpoint_address[i] = 0;
			}
			zxcpu[num_cpus]->user_breakpoint_hit = 0;
			zxcpu[num_cpus]->user_breakpoints_suspended = 0;
			zxcpu[num_cpus]->memory_breakpoint_hit = CPU_MEM_BREAK_NONE;
			zxcpu[num_cpus]->memory_breakpoint_hit_address = 0;
			zxcpu[num_cpus]->memory_read_breakpoint_enabled = 0;
			zxcpu[num_cpus]->memory_read_breakpoint_address = 0;
			zxcpu[num_cpus]->memory_write_breakpoint_enabled = 0;
			zxcpu[num_cpus]->memory_write_breakpoint_address = 0;
			zxcpu[num_cpus]->memory_breakpoint_skip_enabled = 0;
			zxcpu[num_cpus]->memory_breakpoint_skip_pc = 0;
			zxcpu[num_cpus]->memory_breakpoint_instruction_active = 0;
		}
		zxcpu[num_cpus]->memory = NULL;
		zxcpu[num_cpus]->fetchbyte = Ondra::fetch8;
		zxcpu[num_cpus]->readbyte = Ondra::peek8;
		zxcpu[num_cpus]->readword = Ondra::peek16;
		zxcpu[num_cpus]->writeword = Ondra::poke16;
		zxcpu[num_cpus]->writebyte = Ondra::poke8;
		zxcpu[num_cpus]->input = Ondra::inPort;
		zxcpu[num_cpus]->output = Ondra::outPort;
		zxcpu[num_cpus]->addtstates = Ondra::addTstates;
		return num_cpus;
	}
	return 255;
}

// reset a CPU context
void CPU_Reset(uint8_t cpuid) {

	Z80Reset(zxcpu[cpuid]);
	zxcpu[cpuid]->im = Z80_INTERRUPT_MODE_0;
}


// Assign a block of memory as read/write
void CPU_MEMRW(int addr, int length) {

}

// Assign a block of memory as read only
void CPU_MEMR(int addr, int length) {

}

// Destroy a CPU context
void CPU_Destroy(uint8_t cpuid) {
	free(zxcpu[cpuid]);
	zxcpu[cpuid]=NULL;
}

// Emulatoe virtual CPU
uint64_t CPU_Emulate(uint8_t cpuid, uint64_t ticks) {
	return Z80Emulate(zxcpu[cpuid], ticks);
}

void CPU_RequestBreak(uint8_t cpuid) {
	zxcpu[cpuid]->break_requested = 1;
}

void CPU_ClearBreak(uint8_t cpuid) {
	zxcpu[cpuid]->break_requested = 0;
}

void CPU_SetTempBreakpoint(uint8_t cpuid, uint16_t address) {
	zxcpu[cpuid]->temp_breakpoint_address = address & 0xffff;
	zxcpu[cpuid]->temp_breakpoint_hit = 0;
	zxcpu[cpuid]->temp_breakpoint_enabled = 1;
}

void CPU_ClearTempBreakpoint(uint8_t cpuid) {
	zxcpu[cpuid]->temp_breakpoint_enabled = 0;
	zxcpu[cpuid]->temp_breakpoint_hit = 0;
}

bool CPU_TempBreakpointHit(uint8_t cpuid) {
	return zxcpu[cpuid]->temp_breakpoint_hit != 0;
}

void CPU_SetUserBreakpointSlot(uint8_t cpuid, int slot, uint16_t address, bool enabled) {
	if (slot < 0 || slot >= 5) {
		return;
	}
	zxcpu[cpuid]->user_breakpoint_address[slot] = address & 0xffff;
	zxcpu[cpuid]->user_breakpoint_enabled[slot] = enabled ? 1 : 0;
}

void CPU_ClearUserBreakpointHit(uint8_t cpuid) {
	zxcpu[cpuid]->user_breakpoint_hit = 0;
}

bool CPU_UserBreakpointHit(uint8_t cpuid) {
	return zxcpu[cpuid]->user_breakpoint_hit != 0;
}

void CPU_SuspendUserBreakpoints(uint8_t cpuid, bool suspend) {
	zxcpu[cpuid]->user_breakpoints_suspended = suspend ? 1 : 0;
}

void CPU_SetMemoryReadBreakpoint(uint8_t cpuid, uint16_t address, bool enabled) {
	zxcpu[cpuid]->memory_read_breakpoint_address = address & 0xffff;
	zxcpu[cpuid]->memory_read_breakpoint_enabled = enabled ? 1 : 0;
}

void CPU_SetMemoryWriteBreakpoint(uint8_t cpuid, uint16_t address, bool enabled) {
	zxcpu[cpuid]->memory_write_breakpoint_address = address & 0xffff;
	zxcpu[cpuid]->memory_write_breakpoint_enabled = enabled ? 1 : 0;
}

static bool CPU_MemoryBreakpointCanTrigger(uint8_t cpuid) {
	Z80_STATE* state = zxcpu[cpuid];

	if (!state->memory_breakpoint_instruction_active) {
		return false;
	}

	/* After a memory BP the stopped instruction must be allowed to execute
	 * once when the user continues.  As soon as PC moves to another
	 * instruction the one-shot suppression expires automatically. */
	if (state->memory_breakpoint_skip_enabled) {
		if ((state->pc_step & 0xffff) == (state->memory_breakpoint_skip_pc & 0xffff)) {
			return false;
		}
		state->memory_breakpoint_skip_enabled = 0;
	}

	return true;
}

bool CPU_CheckMemoryReadBreakpoint(uint8_t cpuid, uint16_t address) {
	Z80_STATE* state = zxcpu[cpuid];
	if (!CPU_MemoryBreakpointCanTrigger(cpuid)) {
		return false;
	}
	if (state->memory_read_breakpoint_enabled &&
		((state->memory_read_breakpoint_address & 0xffff) == (address & 0xffff))) {
		state->memory_breakpoint_hit = CPU_MEM_BREAK_READ;
		state->memory_breakpoint_hit_address = address & 0xffff;
		state->break_requested = 1;
		return true;
	}
	return false;
}

bool CPU_CheckMemoryWriteBreakpoint(uint8_t cpuid, uint16_t address) {
	Z80_STATE* state = zxcpu[cpuid];
	if (!CPU_MemoryBreakpointCanTrigger(cpuid)) {
		return false;
	}
	if (state->memory_write_breakpoint_enabled &&
		((state->memory_write_breakpoint_address & 0xffff) == (address & 0xffff))) {
		state->memory_breakpoint_hit = CPU_MEM_BREAK_WRITE;
		state->memory_breakpoint_hit_address = address & 0xffff;
		state->break_requested = 1;
		return true;
	}
	return false;
}

int CPU_MemoryBreakpointHit(uint8_t cpuid) {
	return zxcpu[cpuid]->memory_breakpoint_hit;
}

void CPU_ClearMemoryBreakpointHit(uint8_t cpuid) {
	zxcpu[cpuid]->memory_breakpoint_hit = CPU_MEM_BREAK_NONE;
	zxcpu[cpuid]->memory_breakpoint_hit_address = 0;
}

void CPU_ArmMemoryBreakpointResume(uint8_t cpuid) {
	Z80_STATE* state = zxcpu[cpuid];
	if (state->memory_breakpoint_hit != CPU_MEM_BREAK_NONE) {
		state->memory_breakpoint_skip_enabled = 1;
		state->memory_breakpoint_skip_pc = state->pc & 0xffff;
	}
	CPU_ClearMemoryBreakpointHit(cpuid);
}

int oim;

// Call interrupt on a CPU
int CPU_Interrupt(uint8_t cpuid) {
	if(oim!=zxcpu[cpuid]->im) {
		oim = zxcpu[cpuid]->im;
		//printf("New interrupt: MODE: %d ",zxcpu[cpuid]->im);
		switch(oim) {
		case Z80_INTERRUPT_MODE_0:
			//printf("mode 0\n");
			break;
		case Z80_INTERRUPT_MODE_1:
			//printf("mode 1\n");
			break;
		case Z80_INTERRUPT_MODE_2:
			//printf("mode 2\n");
			break;
			//default:
			//printf("????\n");
		}

	}
	return Z80Interrupt(zxcpu[cpuid],0xff);
}

// Call an NMI on a CPU
int CPU_NMI(uint8_t cpuid) {
	int cycles;

	/* NMI is accepted regardless of IFF1 and it also releases HALT.
	 * Z80NonMaskableInterrupt() performs the Z80-visible state changes
	 * (IFF2<-IFF1, IFF1=0, push PC, PC=0066h) and returns the 11 T-states
	 * consumed by NMI acknowledge.  Unlike Z80Interrupt(), this core does
	 * not publish those cycles through addtstates(), so do it here. */
	zxcpu[cpuid]->bHalt = false;
	cycles = Z80NonMaskableInterrupt(zxcpu[cpuid]);
	zxcpu[cpuid]->addtstates(cycles);
	return cycles;
}

bool CPU_getHaltFlag(uint8_t cpuid) {
	return zxcpu[cpuid]->bHalt;
}

void CPU_setHaltFlag(uint8_t cpuid, bool halted) {
	zxcpu[cpuid]->bHalt = halted;
}

void CPU_resetHaltFlag(uint8_t cpuid) {
	zxcpu[cpuid]->bHalt=false;
}

uint8_t CPU_GetReg8(uint8_t cpuid, reg rWhich) {

	switch(rWhich) {
	case REG_I:
		return zxcpu[cpuid]->i;
		break;
	case REG_R:
		return zxcpu[cpuid]->r;
		break;
	case REG_A:
		return zxcpu[cpuid]->registers.byte[Z80_A];
		break;
	case REG_F:
		return zxcpu[cpuid]->registers.byte[Z80_F];
		break;
	case REG_B:
		return zxcpu[cpuid]->registers.byte[Z80_B];
		break;
	case REG_C:
		return zxcpu[cpuid]->registers.byte[Z80_C];
		break;
	case REG_D:
		return zxcpu[cpuid]->registers.byte[Z80_D];
		break;
	case REG_E:
		return zxcpu[cpuid]->registers.byte[Z80_E];
		break;
	case REG_H:
		return zxcpu[cpuid]->registers.byte[Z80_H];
		break;
	case REG_L:
		return zxcpu[cpuid]->registers.byte[Z80_L];
		break;
	case REG_IXH:
		return zxcpu[cpuid]->registers.byte[Z80_IXH];
		break;
	case REG_IXL:
		return zxcpu[cpuid]->registers.byte[Z80_IXL];
		break;
	case REG_IYH:
		return zxcpu[cpuid]->registers.byte[Z80_IYH];
		break;
	case REG_IYL:
		return zxcpu[cpuid]->registers.byte[Z80_IYL];
		break;
	case REG_SPH:
		return zxcpu[cpuid]->registers.byte[Z80_SPH];
		break;
	case REG_SPL:
		return zxcpu[cpuid]->registers.byte[Z80_SPL];
		break;
	default:
		return 0;
		break;
	}
}

uint16_t CPU_GetReg16(uint8_t cpuid, reg rWhich) {

	switch(rWhich) {
	case REG_PC:
		return zxcpu[cpuid]->pc;
		break;
	case REG_AF:
		return zxcpu[cpuid]->registers.word[Z80_AF];
		break;
	case REG_BC:
		return zxcpu[cpuid]->registers.word[Z80_BC];
		break;
	case REG_DE:
		return zxcpu[cpuid]->registers.word[Z80_DE];
		break;
	case REG_HL:
		return zxcpu[cpuid]->registers.word[Z80_HL];
		break;
	case REG_IX:
		return zxcpu[cpuid]->registers.word[Z80_IX];
		break;
	case REG_IY:
		return zxcpu[cpuid]->registers.word[Z80_IY];
		break;
	case REG_SP:
		return zxcpu[cpuid]->registers.word[Z80_SP];
		break;
	default:
		return 0;
		break;
	}
}

uint16_t CPU_GetReg16Alt(uint8_t cpuid, reg rWhich) {

	switch(rWhich) {
	case REG_AF:
		return zxcpu[cpuid]->alternates.word[Z80_AF];
		break;
	case REG_BC:
		return zxcpu[cpuid]->alternates.word[Z80_BC];
		break;
	case REG_DE:
		return zxcpu[cpuid]->alternates.word[Z80_DE];
		break;
	case REG_HL:
		return zxcpu[cpuid]->alternates.word[Z80_HL];
		break;
	default:
		return 0;
		break;
	}
}

void CPU_SetPC(uint8_t cpuid, unsigned short value) {
	zxcpu[cpuid]->pc=value;
	zxcpu[cpuid]->pc_step=value;
}

void CPU_PutReg8(uint8_t cpuid, reg rWhich, unsigned char value) {

	switch(rWhich) {
	case REG_I:
		zxcpu[cpuid]->i=value;
		break;
	case REG_R:
		zxcpu[cpuid]->r=value;
		break;
	case REG_A:
		zxcpu[cpuid]->registers.byte[Z80_A]=value;
		break;
	case REG_F:
		zxcpu[cpuid]->registers.byte[Z80_F]=value;
		break;
	case REG_B:
		zxcpu[cpuid]->registers.byte[Z80_B]=value;
		break;
	case REG_C:
		zxcpu[cpuid]->registers.byte[Z80_C]=value;
		break;
	case REG_D:
		zxcpu[cpuid]->registers.byte[Z80_D]=value;
		break;
	case REG_E:
		zxcpu[cpuid]->registers.byte[Z80_E]=value;
		break;
	case REG_H:
		zxcpu[cpuid]->registers.byte[Z80_H]=value;
		break;
	case REG_L:
		zxcpu[cpuid]->registers.byte[Z80_L]=value;
		break;
	case REG_IXH:
		zxcpu[cpuid]->registers.byte[Z80_IXH]=value;
		break;
	case REG_IXL:
		zxcpu[cpuid]->registers.byte[Z80_IXL]=value;
		break;
	case REG_IYH:
		zxcpu[cpuid]->registers.byte[Z80_IYH]=value;
		break;
	case REG_IYL:
		zxcpu[cpuid]->registers.byte[Z80_IYL]=value;
		break;
	case REG_SPH:
		zxcpu[cpuid]->registers.byte[Z80_SPH]=value;
		break;
	case REG_SPL:
		zxcpu[cpuid]->registers.byte[Z80_SPL]=value;
		break;
	default:
		break;
		//printf("Unimplemented reg return %s\n",reg);
	}
}

void CPU_PutReg8Alt(uint8_t cpuid, reg rWhich, unsigned char value) {

	switch(rWhich) {
	case REG_A:
		zxcpu[cpuid]->alternates.byte[Z80_A]=value;
		break;
	case REG_F:
		zxcpu[cpuid]->alternates.byte[Z80_F]=value;
		break;
	case REG_B:
		zxcpu[cpuid]->alternates.byte[Z80_B]=value;
		break;
	case REG_C:
		zxcpu[cpuid]->alternates.byte[Z80_C]=value;
		break;
	case REG_D:
		zxcpu[cpuid]->alternates.byte[Z80_D]=value;
		break;
	case REG_E:
		zxcpu[cpuid]->alternates.byte[Z80_E]=value;
		break;
	case REG_H:
		zxcpu[cpuid]->alternates.byte[Z80_H]=value;
		break;
	case REG_L:
		zxcpu[cpuid]->alternates.byte[Z80_L]=value;
		break;
	default:
		break;
		//printf("Unimplemented reg return %s\n",reg);
	}
}

void CPU_SetIff(uint8_t cpuid, reg rWhich, int value) {
	switch(rWhich) {
	case REG_IFF1:
		zxcpu[cpuid]->iff1=value;
		break;
	case REG_IFF2:
		zxcpu[cpuid]->iff2=value;
		break;
	default:
		break;
		//printf("Unimplemented reg return %s\n",reg);
	}
}

uint8_t CPU_GetIff(uint8_t cpuid, reg rWhich){
 switch(rWhich) {
	case REG_IFF1:
		return zxcpu[cpuid]->iff1;
		break;
	case REG_IFF2:
		return zxcpu[cpuid]->iff2;
		break;
	default:
		break;
		//printf("Unimplemented reg return %s\n",reg);
	}
 return 0;
}

void CPU_SetIntMode(uint8_t cpuid, int value) {
	zxcpu[cpuid]->im=value;
}

uint8_t CPU_GetIntMode(uint8_t cpuid) {
   return zxcpu[cpuid]->im;
}

bool CPU_GetPendingEI(uint8_t cpuid) {
	return zxcpu[cpuid]->pending_ei != 0;
}

void CPU_SetPendingEI(uint8_t cpuid, bool pending) {
	/* A restored state is already at an instruction boundary.  Value 1 means
	 * exactly one following instruction must execute before INT is accepted. */
	zxcpu[cpuid]->pending_ei = pending ? 1 : 0;
}

void CPU_PrepareTimelineRestore(uint8_t cpuid) {
	Z80_STATE* state = zxcpu[cpuid];
	if (state == NULL) return;

	/* None of these fields is architectural Z80 state.  They describe how the
	 * debugger stopped the *future* execution that we just abandoned. */
	state->status = 0;
	state->pc_step = state->pc & 0xffff;
	state->elapsed_cycles = 0;
	state->temp_breakpoint_enabled = 0;
	state->temp_breakpoint_hit = 0;
	state->user_breakpoint_hit = 0;
	state->user_breakpoints_suspended = 0;
	state->memory_breakpoint_hit = CPU_MEM_BREAK_NONE;
	state->memory_breakpoint_hit_address = 0;
	state->memory_breakpoint_skip_enabled = 0;
	state->memory_breakpoint_skip_pc = 0;
	state->memory_breakpoint_instruction_active = 0;
}

uint64_t CPU_getCycles(uint8_t cpuid) {
	return zxcpu[cpuid]->elapsed_cycles;
}

