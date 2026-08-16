#include "Ondra.h"
#include "Screen.h"
#include "Config.h"
#include "Memory.h"
#include "Keyboard.h"
#include "MTimer.h"
#include "Debug.h"
#include "z80emu.h"
#include "cpuintf.h"
#include "Jondra.h"
#include "Clock.h"
#include "DirtyTiles.h"
#include "Sound.h"
#include "Melodik.h"
#include "Tape.h"
#include "MemoryTimeline.h"
#include <stdio.h>



#define DISP_ADDR_LEN 10240

uint64_t Ondra::tstates=0;
uint64_t Ondra::Nexttstates=0;
Ondra* Ondra::machine=NULL;

// Constructor
Ondra::Ondra(){
	machine=NULL;
	scr=NULL;
	px=NULL;
	px_shadow=NULL;
	cfg=new Config();
    key=NULL;
	mem=NULL;
	msSpeed=20;
    cpuSpeedPercent=100;
    task=NULL;
	cpu=NULL;
	tap=NULL;
	clk=NULL;
	frame=NULL;
	nSpeedPercent=0;
#ifndef _WIN32
    pendingWindowSpeed=-1;
#endif
	nSpeedPercentUpdateMaxCycles=50;
	nSpeedPercentUpdateDec=50;
	nLastSeen=0;
	snd=NULL;
	melodik=NULL;
	nLastMilisec=0;
	GreenLed=NULL;
	YellowLed=NULL;
	TapeLed=NULL;
	RecButton=NULL;
	paused=true;
	iov = new unsigned char[PAGE_SIZE];
	dispAdr=new int[DISP_ADDR_LEN];
	dmaEnabled=false;
	portA0=0; portA1=0; portA3=0;
	t_resolution_correct=0;
	t_frame=T_DMAOFF;
	nDMAStatus=0;
	tapestart=false;
	activeINT=false;
	deb=NULL;
	timeline=NULL;
	til=NULL;
	nRozliseni=255;
    bFrst=true;
	bAutoRunAfterReset=false;
	memorySize=42 + 0x10000;
	snapshotBuffer = new unsigned char[memorySize]; // Alokace bufferu pri inicializaci
	maxRecords=500000;
	snapshotInterval=100;
    timelineSliceTarget=0;
    timelineResumeSliceTarget=0;
	timeline = new MemoryTimeline(this, maxRecords, snapshotInterval);
	running=false;
	tstates=0;
	Nexttstates=0;
    

	memset(snapshotBuffer,0,memorySize*sizeof(unsigned char));

	// Create memory and other components with 'this' as required.
	mem = new Memory(this, cfg);
	clk=new Clock();

	cpu = CPU_Create();
	snd = new Sound();
	snd->setEnabled(cfg->getAudio());
	snd->setOutputEnabled(cfg->getAudio() || cfg->getMelodik());
	snd->init();
	

	memset(iov,0,PAGE_SIZE*sizeof(unsigned char));
	mem->setIOVect(iov);
	key = new Keyboard(iov);

	melodik = new Melodik(iov);
	melodik->setEnabled(cfg->getMelodik());
	melodik->init(clk->getTstates());

	dmaEnabled = true;

	tap = new Tape(this);

	paused = true;
	nCnt=0;


//	Reset(true);
}

// Destructor to clean-up allocated memory.
Ondra::~Ondra() {
	stopEmulation();
	if (machine == this) machine = NULL;
	delete timeline;
	timeline = NULL;
	/* Melodik::deinit() updates its presence bit in iov. */
	delete melodik;
	melodik = NULL;
	delete []iov;
	delete []dispAdr;
	delete []snapshotBuffer;
	if(px!=NULL){
		delete []px;
	}
	if(px_shadow!=NULL){
		delete []px_shadow;
	}
	delete cfg;
	delete key;
	delete mem;
	delete clk;
	CPU_Destroy(cpu);
	delete tap;
	delete snd;
}


// Set the debugger.
void Ondra::setDebugger(Debugger* indeb) {
	deb = indeb;
}

// Set the frame.
void Ondra::setFrame(Jondra* inJon) {
	frame = inJon;
}

Debugger* Ondra::getDebugger() {
	return deb;
}

Keyboard* Ondra::getKeyboard() {
	return key;
}


Config* Ondra::getConfig() {
	return cfg;
}

void Ondra::setScreen(JScreen* screen) {
	scr = screen;
	til=new DirtyTiles(scr);
	til->InitDirtyTiles();
	px=new uint8_t[scr->GetWidth()*scr->GetHeight()];
	px_shadow=new uint8_t[scr->GetWidth()*scr->GetHeight()];	
	memset((void*)px_shadow,0,scr->GetWidth()*scr->GetHeight()*sizeof(uint8_t));
	genDispTables();
	Reset(true);
	// Static CPU callbacks may use machine only after the object is fully ready.
	machine=this;
	if (timeline != NULL) {
		timeline->setEnabled(Config::bEnableTimeline);
	}
}


void Ondra::setGreenLed(void* led) {
	GreenLed = led;
}

void Ondra::setYellowLed(void* led) {
	YellowLed = led;
}

void Ondra::setTapeLed(void* led) {
	TapeLed = led;
}

void Ondra::setRecButton(void* b) {
	RecButton = b;
}

void Ondra::setClockSpeed(int inSpeed) {
	msSpeed = inSpeed;

	/*
	 * The tape motor can request a speed change from inside the emulation
	 * timer thread itself.  Do not stop/restart that thread here: StopTimer()
	 * waits for the timer thread and would therefore deadlock when called
	 * from outPort()/Tape::tapeStart().
	 *
	 * MTimer reads intervalMs on every loop, so changing it in-place is enough
	 * to apply the new speed from the next timer interval.
	 */
	if (task != NULL) {
		task->intervalMs = inSpeed;
	}
}

int Ondra::getClockSpeed() {
	return msSpeed;
}

void Ondra::setCpuSpeedPercent(int percent) {
    if (percent < 1) {
        percent = 1;
    }
    if (percent > 4000) {
        percent = 4000;
    }
    if (cpuSpeedPercent == percent) {
        return;
    }

    cpuSpeedPercent = percent;

    /*
     * CPU speed is independent of the 20 ms video/frame timer.  A value of
     * 200 therefore models a 4 MHz Turbo Ondra: twice as many CPU T-states
     * are available between two normal 50 Hz frame interrupts.
     *
     * Audio timing in the original JOndra code is based directly on the
     * standard 2 MHz T-state scale.  JIQ151 therefore produces audio only at
     * normal speed; keep the same rule here and reset the audio phase when
     * the speed changes so returning to 1x starts from a clean frame.
     */
    if (snd != NULL) {
        snd->resetSource((long)tstates);
    }
    if (melodik != NULL && melodik->isEnabled()) {
        melodik->init(clk != NULL ? clk->getTstates() : 0);
    }

    nSpeedPercent = 0;
    nSpeedPercentUpdateDec = nSpeedPercentUpdateMaxCycles;
    nLastSeen = MTimer::getCurrentTimeMillis();
}

int Ondra::getCpuSpeedPercent() const {
    return cpuSpeedPercent;
}

void Ondra::addTstates(uint64_t states)
{
	machine->clk->addTstates(states);
}

// Reset the emulator.
void Ondra::Reset(bool dirty) {
	portA3 = portA0 = portA1 = 0;
	nDMAStatus = 0;
	t_resolution_correct = 0;
	t_frame = T_DMAOFF;
	timelineSliceTarget = 0;
	timelineResumeSliceTarget = 0;
	if (timeline != NULL) timeline->clear();
	mem->Reset(dirty);
	mem->mapRom(true);
	//	clk->reset();
	CPU_Reset(cpu);
	key->Reset();

	nRozliseni = 255;
	genDispTables();
	tapestart = false;
	Fl::focus((Fl_Widget*)key);

	{
		bool wantedSound = cfg->getAudio();
		bool wantedMelodik = cfg->getMelodik();
		bool wantedOutput = wantedSound || wantedMelodik;
		bool sourceChanged = (snd->isEnabled() != wantedSound);
		bool outputChanged = (snd->isOutputEnabled() != wantedOutput);

		if (outputChanged) {
			/* Recreate only the shared playback device. */
			snd->deinit();
			snd->setEnabled(wantedSound);
			snd->setOutputEnabled(wantedOutput);
			snd->init();
		} else if (sourceChanged) {
			/* The output remains open for the other independent source. */
			snd->setEnabled(wantedSound);
			snd->resetSource(tstates);
		}
	}

	if (melodik != NULL) {
		bool wantedMelodik = cfg->getMelodik();

		if (melodik->isEnabled() != wantedMelodik) {
			melodik->setEnabled(wantedMelodik);
			if (wantedMelodik) {
				melodik->init(clk->getTstates());
			} else {
				melodik->deinit();
			}
		} else if (wantedMelodik) {
			melodik->initChip();
		}
	}
}

// Trigger a Non-Maskable Interrupt.
void Ondra::Nmi() {
	bool wasPaused = paused;
	int nmiStates;

	/* NMI is requested from the FLTK/UI thread, while the Z80 normally runs
	 * in MTimer's worker thread.  Stop the worker first so the NMI cannot
	 * modify Z80_STATE in the middle of an instruction. */
	if (!wasPaused) {
		stopEmulation();
	}

	nmiStates = CPU_NMI(cpu);
	tstates += (uint64_t)nmiStates;

	if (!wasPaused) {
		startEmulation();
	}
}

// Start emulation.
void Ondra::startEmulation() {
	if (!paused) {
		return;
	}
	til->DirtyTilesAll();
	CPU_ClearBreak(cpu);
	paused = false;
	task = new MTimer(this);
	task->StartTimer(msSpeed);
}

// Stop emulation.
void Ondra::stopEmulation() {
	if (!paused) {
		paused = true;
		/* Stop the fast Z80 core at the next instruction boundary. */
		CPU_RequestBreak(cpu);
	}

	/*
	 * A debugger breakpoint can set paused from the emulation thread itself.
	 * In that case the timer object still exists and must be joined/removed by
	 * the GUI thread before the debugger reads the final CPU state.
	 */
	if (task != NULL) {
		task->StopTimer();
		delete task;
		task = NULL;
	}	
}

uint64_t Ondra::debugStepInto() {
	uint64_t states;
	uint64_t interruptStates = 0;

	/* Debugger stepping is valid only while the timer thread is stopped. */
	if (!paused) {
		return 0;
	}

	/*
	 * Z80Emulate() checks the cycle limit only after a complete instruction.
	 * Four cycles are the minimum Z80 instruction length, therefore a limit
	 * of 4 executes exactly one complete instruction, including DD/FD/CB/ED
	 * prefixed instructions.
	 */
	/* If the previous stop was a memory BP, allow this exact instruction
	 * to execute once so Step Into can move past the watched access. */
	if (CPU_MemoryBreakpointHit(cpu) != CPU_MEM_BREAK_NONE) {
		CPU_ArmMemoryBreakpointResume(cpu);
	}
	CPU_ClearBreak(cpu);
	CPU_ClearUserBreakpointHit(cpu);
	/* A manual single-step must be able to move away from a breakpoint. */
	CPU_SuspendUserBreakpoints(cpu, true);

	/*
	 * If the debugger stopped while the Z80 was in HALT, Z80Emulate() by
	 * itself only burns cycles and leaves PC unchanged.  Normal JOndra
	 * execution reaches the next frame interrupt before it can continue.
	 * Process the pending interrupt before the opcode so Step Into can leave
	 * HALT correctly.
	 */
	if (CPU_getHaltFlag(cpu)) {
		int intStates;
		setActiveINT(true);
		intStates = CPU_Interrupt(cpu);
		interruptStates = (uint64_t)intStates;
		tstates += intStates;
		setActiveINT(false);
	}

	states = CPU_Emulate(cpu, 4);
	CPU_SuspendUserBreakpoints(cpu, false);
	tstates += states;
	CPU_RequestBreak(cpu);

	/* Step Into repaints the emulated screen after every instruction. */
	if (til != NULL && px != NULL) {
		til->DispUpdate(px);
	}

	return states + interruptStates;
}

bool Ondra::debugStartStepOver(uint16_t targetAddress) {
	if (!paused) {
		return false;
	}

	if (CPU_MemoryBreakpointHit(cpu) != CPU_MEM_BREAK_NONE) {
		CPU_ArmMemoryBreakpointResume(cpu);
	}
	CPU_ClearUserBreakpointHit(cpu);
	CPU_SetTempBreakpoint(cpu, targetAddress);
	if (melodik != NULL && melodik->isEnabled()) {
		melodik->initChip();
	}
	startEmulation();
	return true;
}

bool Ondra::debugFinishStepOver() {
	if (!CPU_TempBreakpointHit(cpu)) {
		return false;
	}

	/* Join the timer thread before the GUI reads registers/memory. */
	stopEmulation();
	CPU_ClearTempBreakpoint(cpu);
	CPU_RequestBreak(cpu);

	if (til != NULL && px != NULL) {
		til->DispUpdate(px);
	}
	return true;
}

void Ondra::debugCancelStepOver() {
	CPU_ClearTempBreakpoint(cpu);
	stopEmulation();
	CPU_ClearUserBreakpointHit(cpu);
	CPU_RequestBreak(cpu);
}

bool Ondra::debugFinishUserBreakpoint() {
	if (!CPU_UserBreakpointHit(cpu)) {
		return false;
	}

	/* Join the emulation timer before the debugger reads CPU/memory state. */
	stopEmulation();
	CPU_ClearUserBreakpointHit(cpu);
	CPU_RequestBreak(cpu);

	if (til != NULL && px != NULL) {
		til->DispUpdate(px);
	}
	return true;
}

int Ondra::debugFinishMemoryBreakpoint() {
	int hit = CPU_MemoryBreakpointHit(cpu);
	if (hit == CPU_MEM_BREAK_NONE) {
		return CPU_MEM_BREAK_NONE;
	}

	/* Keep the hit latched while the debugger is open.  Step/Run/Close can
	 * then arm a one-instruction suppression and continue from the exact
	 * instruction which caused the memory access. */
	stopEmulation();
	CPU_RequestBreak(cpu);

	if (til != NULL && px != NULL) {
		til->DispUpdate(px);
	}
	return hit;
}

bool Ondra::isPaused() {
	return paused;
}

// Generate display tables.
void Ondra::genDispTables() {
	memset(dispAdr,-1,DISP_ADDR_LEN*sizeof(int));
	uint8_t* tmppx=px;
	px=px_shadow;
	px_shadow=tmppx;
	memset((void*)px,0,scr->GetWidth()*scr->GetHeight()*sizeof(uint8_t));
	int nSkew = 255 - nRozliseni;
	int adr = 0;
	int vm;
	for (int y = 255 - nSkew; y != 0; y--) {
		for (int x = 0xff00; x != 0xd700; x -= 0x0100) {
			vm = ((unsigned int)y >> 1) | ((y & 1) << 7) | x;
			dispAdr[vm - 0xd800] = adr;			
			px[adr++] = mem->readRam(vm);
		}
	}
	for(int i=0;i<scr->GetWidth()*scr->GetHeight()*sizeof(uint8_t);i++){
		if(px[i]!=px_shadow[i]){
			int bx=i%40;
			til->DispDirtyRectTiles(bx*8,(int)i/40,8,1);
			}
	}
}


// Method called every 20ms.
void Ondra::ms20() {
	uint64_t nNowSeen = MTimer::getCurrentTimeMillis();
	if ((nNowSeen - nLastSeen) > 0) {
		nSpeedPercent = nSpeedPercent + (int)(200000 / (nNowSeen - nLastSeen));
	}
	//CDebug::debug("rozdil %I64u",nNowSeen-nLastSeen);
	nLastSeen = nNowSeen;
	nSpeedPercentUpdateDec--;
	if (nSpeedPercentUpdateDec <= 0) {
		nSpeedPercentUpdateDec = nSpeedPercentUpdateMaxCycles;
		nSpeedPercent = ((nSpeedPercent / (10 * nSpeedPercentUpdateMaxCycles)) + 7) / 10;
        int effectiveSpeedPercent = (nSpeedPercent * cpuSpeedPercent + 50) / 100;
#ifdef _WIN32
		if (frame) {
			char strTmp[50];
			sprintf(strTmp,"Ondra SPO 186 - %d%%", effectiveSpeedPercent);
			frame->label(strTmp);
		}
#else
        /*
         * Never call FLTK/X11 from the emulation pthread.  On older FLTK/X11
         * this corrupted Xlib's request stream and later caused errors such as
         * X_GetWindowAttributes: BadLength followed by X I/O error.
         * Store only the latest value; the FLTK timeout callback applies it.
         */
        __sync_lock_test_and_set(&pendingWindowSpeed, effectiveSpeedPercent);
#endif
		nSpeedPercent = 2000 * nSpeedPercent;
		nSpeedPercentUpdateDec -= 20;
	}

	if (!paused) {		
		const unsigned char* melodikData = NULL;
		int melodikByteCount = 0;

        /* Like JIQ151, audio is generated only at the normal CPU speed.
         * The old sound path maps 2 MHz T-states directly to a 20 ms PCM
         * buffer; muting it at other multipliers avoids false pitch/timing. */
        if (cpuSpeedPercent == 100) {
			if (snd->isEnabled()) {
				snd->switchBuffers(tstates);
			}
			if ((melodik != NULL) && melodik->isEnabled()) {
				melodik->setMelodikDetectOn();
				melodik->updateSound(clk->getTstates());
				melodik->switchBuffers();
				melodik->setDataReady();
				if (melodik->isDataReady()) {
					melodikData = melodik->getPlayBuffer();
					melodikByteCount = melodik->getBufferByteCount();
				}
			}

			/* One common PCM block is sent to waveOut or ALSA. */
			if (snd->isOutputEnabled()) {
				snd->setDataReady(melodikData, melodikByteCount);
			}
			if (melodik != NULL) {
				melodik->clearDataReady();
			}
        }

        /*
         * A C++ timeline record is made only inside CPU_Emulate().  The frame
         * interrupt has therefore already happened before every ordinary
         * historical instruction record.  When resuming such a record, first
         * finish the remainder of that exact original CPU slice.  Injecting a
         * fresh interrupt immediately would create two frame interrupts inside
         * one historical slice and can leave timing-sensitive ROM code in an
         * invalid state.
         *
         * timelineResumeSliceTarget is the real Clock limit used by the
         * original CPU_Emulate() call, not the Nexttstates accumulator
         * accumulator.
         */
        if (timelineResumeSliceTarget != 0) {
            uint64_t nowStates = clk->getTstates();
            if (timelineResumeSliceTarget > nowStates) {
                uint64_t remaining64 = timelineResumeSliceTarget - nowStates;
                int remaining = remaining64 > (uint64_t)0x7fffffffUL ? 0x7fffffff : (int)remaining64;
                uint64_t emulatedStates;

                if (remaining < 4) remaining = 4;
                timelineSliceTarget = timelineResumeSliceTarget;
                emulatedStates = CPU_Emulate(cpu, remaining);
                tstates += emulatedStates;
                timelineSliceTarget = 0;

                /* If a debugger breakpoint stopped this slice early, keep the
                 * original target so the next Resume can continue the same
                 * historical slice.  Otherwise the slice is complete. */
                if (CPU_TempBreakpointHit(cpu) || CPU_UserBreakpointHit(cpu) ||
                    CPU_MemoryBreakpointHit(cpu) != CPU_MEM_BREAK_NONE ||
                    clk->getTstates() < timelineResumeSliceTarget) {
                    /* target intentionally preserved */
                } else {
                    timelineResumeSliceTarget = 0;
                }

                /* This timer callback has completed (or debugger-stopped) the
                 * historical slice.  The next normal callback starts with the
                 * proven one-shot C++ frame interrupt. */
                return;
            }
            timelineResumeSliceTarget = 0;
        }

		setActiveINT(true);				
		int nIntStates=CPU_Interrupt(cpu);
		tstates+=nIntStates;
		setActiveINT(false);
		if (!paused) {	
			uint64_t emulatedStates;
            int cpuFrameStates = (int)(((long)t_frame * (long)cpuSpeedPercent + 50L) / 100L);
            int cpuSliceStates;

            /* Keep the video/frame period at 20 ms.  Only the amount of CPU
             * work available inside that frame changes. */
            if (cpuFrameStates < nIntStates + 4) {
                cpuFrameStates = nIntStates + 4;
            }
            cpuSliceStates = cpuFrameStates - nIntStates;

			Nexttstates+=cpuFrameStates;
            /* Save the actual absolute Clock limit of this CPU slice.  Unlike
             * Nexttstates, this is the value against which the original
             * CPU_Emulate() execution really runs. */
            timelineSliceTarget = clk->getTstates() + (uint64_t)cpuSliceStates;
			emulatedStates = CPU_Emulate(cpu, cpuSliceStates);
			tstates += emulatedStates;
            timelineSliceTarget = 0;

			/*
			 * A temporary breakpoint is also used by command-line autoload.
			 * In that case do not stop in the debugger: replace the ROM state
			 * with the requested image exactly at this instruction boundary.
			 */
			if (CPU_TempBreakpointHit(cpu)) {
				if (bAutoRunAfterReset && strArgFile.length() > 0) {
					CPU_ClearTempBreakpoint(cpu);
					bAutoRunAfterReset = false;
					nStartAddress = -1;
					StartArgumentImage(true);
				} else {
					paused = true;
				}
			}
			if (CPU_UserBreakpointHit(cpu)) {
				paused = true;
			}
		}
				
	}

	if (bFrst && strArgFile.length() > 0) {
		bFrst = false;
		if (nStartAddress < 0) {
			/* No wait address: load the image immediately after startup. */
			StartArgumentImage(true);
		} else {
			/*
			 * Let the ROM initialize the machine first.  The Z80 core checks
			 * this breakpoint before fetching the instruction at nStartAddress.
			 */
			CPU_ClearTempBreakpoint(cpu);
			CPU_SetTempBreakpoint(cpu, (uint16_t)(nStartAddress & 0xffff));
			bAutoRunAfterReset = true;
		}
	}
//	uint64_t nNowSeenEnd = MTimer::getCurrentTimeMillis();
//	CDebug::debug("ms20= %I64u,%I64u,%I64u",nNowSeenEnd,nNowSeen,nNowSeenEnd-nNowSeen);
}


#ifndef _WIN32
bool Ondra::takePendingWindowSpeed(int* speedPercent) {
    int value = __sync_lock_test_and_set(&pendingWindowSpeed, -1);
    if (value < 0) {
        return false;
    }
    if (speedPercent) {
        *speedPercent = value;
    }
    return true;
}
#endif

void Ondra::StartArgumentImage(bool bAutoStart) {
	(void)bAutoStart;
	frame->LoadArgumentImage(strArgFile);
}


void addressToXY(uint16_t address, int &x, int &y) {
    uint8_t highByte = address >> 8;
    uint8_t lowByte = address & 0xFF;
	
    x = ~highByte & 0xFF;	
    y = ((~lowByte & 0x7F) << 1) | ((~lowByte >> 7) & 0x01);
}

// Process a VRAM update.

void Ondra::processVram(int address) {	
	int x = dispAdr[address - 0xd800];
	if (x != -1 && dmaEnabled) {
		px[x] = mem->readRam(address);
		//int ax,ay;
		//addressToXY(address,ax,ay);	
		int bx,by;
		by=x/40;
		bx=x%40;
		til->DispDirtyRectTiles(bx*8,by,8,1);
	}
}


void Ondra::dmaEnable() {
	bool bChanges=false;
	for (int address = 0xd800; address < 0x10000; address++) {
		int x = dispAdr[address - 0xd800];
		if (x != -1) {
			if(px[x] != mem->readRam(address)){
				px[x] = mem->readRam(address);	
				
				int bx=x%40;
				til->DispDirtyRectTiles(bx*8,(int)x/40,8,1);									
			}
		}
	}	
	
	dmaEnabled = true;
}

void Ondra::dmaDisable() {
	dmaEnabled = false;
	bool bChanges=false;
	for (int address = 0xd800; address < 0x10000; address++) {
		int x = dispAdr[address - 0xd800];
		if (x != -1) {
			if(px[x] != 0){
				px[x] = 0;
				int bx=x%40;
				til->DispDirtyRectTiles(bx*8,(int)x/40,8,1);	
			}
		}
	}    
}


uint8_t Ondra::fetch8(uint16_t address) {
	/* Opcode fetches are intentionally excluded from the Memory Read BP. */
	return machine->mem->readByte(address) & 0xff;
}

uint8_t Ondra::peek8(uint16_t address) {
	if (CPU_CheckMemoryReadBreakpoint(machine->cpu, address)) {
		/* Do not perform the watched read. The Z80 core restores the CPU state
		 * to the start of the instruction. */
		return 0;
	}
	return machine->mem->readByte(address) & 0xff;
}

void Ondra::poke8(uint16_t address, uint8_t value){
	uint8_t oldValue;
	if (CPU_CheckMemoryWriteBreakpoint(machine->cpu, address)) {
		/* Do not perform the watched write. The debugger stops immediately
		 * before the memory access. */
		return;
	}
	oldValue = machine->mem->readRam(address);
	if (machine->mem->writeByte(address, (uint8_t)value)) {
		if (Config::bEnableTimeline) {
			machine->timelineRecordRamWrite(address, oldValue, value);
		}
	}
}

uint16_t Ondra::peek16(uint16_t address) {
    uint16_t adr = address;
    uint16_t lsb = peek8(adr);
    if (CPU_MemoryBreakpointHit(machine->cpu) != CPU_MEM_BREAK_NONE) {
        return 0;
    }
    adr = (adr + 1) & 0xFFFF;
    uint16_t msb = peek8(adr);
    if (CPU_MemoryBreakpointHit(machine->cpu) != CPU_MEM_BREAK_NONE) {
        return 0;
    }
    return (msb << 8) | lsb;
}

void Ondra::poke16(uint16_t address, uint16_t word){
    uint16_t adr = address;
    poke8(adr, (uint8_t)(word & 0xFF));
    if (CPU_MemoryBreakpointHit(machine->cpu) != CPU_MEM_BREAK_NONE) {
        return;
    }
    adr = (adr + 1) & 0xFFFF;
    poke8(adr, (uint8_t)((word >> 8) & 0xFF));
}


bool Ondra::isActiveINT(){
	return activeINT;
}

void Ondra::setActiveINT(bool inInt){
	activeINT=inInt;
}

uint8_t Ondra::inPort(uint16_t port) {
	//addTstates(4);
	// Detect resolution change.
	if ((machine->portA3 & 0x30) == 0x00) {
		int nC = port & 0xff;
		int nCarry = nC & 128;
		if (nCarry > 0) {
			nCarry = 1;
		}
		machine->nRozliseni = (nC << 1) & 0xff;
		machine->nRozliseni |= nCarry;
		machine->changeResolution();
	}
	// std::cout << "In: " << std::hex << port << " (PC=" << cpu->getRegPC() << ", portA0="
	//           << (int)portA0 << ", portA1=" << (int)portA1 << ", portA3=" << (int)portA3 << ")" << std::endl;
	return 0xff;
}

void Ondra::changeResolution() {
	t_resolution_correct = (255 - nRozliseni) * 128;
	if (nDMAStatus == 0) {
		t_frame = T_DMAOFF;
	} else {
		t_frame = T_DMAON + t_resolution_correct;
	}
	genDispTables();
	//til->DispUpdate(px);

}

void Ondra::outPort(uint16_t port, uint8_t value){
	
	//addTstates(4);
	port &= 0xff;
	value &= 0xff;
	//CDebug::debug("PC=%04X,out(%02X),%02X",CPU_GetReg16(machine->cpu,REG_PC),port,value);
	// std::cout << "Out: " << std::hex << port << "," << value << " (" << cpu->getRegPC() << ")" << std::endl;
	if ((port & 0x08) == 0) {
		machine->portA3 = (uint8_t)value;
		if ((machine->portA3 & 0x02) == 0) {
			machine->mem->mapRom(true);
		} else {
			machine->mem->mapRom(false);
		}
		if ((machine->portA3 & 0x04) == 0) {
			machine->mem->mapIO(false);
		} else {
			machine->mem->mapIO(true);
		}
		if ((machine->portA3 & 0x01) == 0) {
			machine->t_frame = T_DMAOFF;
			machine->nDMAStatus = 0;
			machine->dmaDisable();

		} else {
			machine->t_frame = T_DMAON + machine->t_resolution_correct;
			machine->nDMAStatus = 1;
			machine->dmaEnable();

		}

	}
	if ((port & 0x01) == 0) {
		machine->portA0 = (uint8_t)value;
		
		if (machine->cpuSpeedPercent == 100 && machine->snd->isEnabled()) {
			machine->snd->fillBuffer->fillWithSample(((value & 224) >> 5), machine->tstates + CPU_getCycles(machine->cpu));
		}
		
//		int sampleIndex = (value & 224) >> 5;
		
//		machine->snd->fillWithSample(sampleIndex, machine->tstates + CPU_getCycles(machine->cpu));        
		// Adjust LED states. Replace with actual GUI framework calls.
		if ((machine->portA0 & 0x01) == 0) {
			// ((JLabel*)GreenLed)->setEnabled(true);
		} else {
			// ((JLabel*)GreenLed)->setEnabled(false);
		}
		if ((machine->portA0 & 0x02) == 0) {
			// ((JLabel*)YellowLed)->setEnabled(true);
		} else {
			// ((JLabel*)YellowLed)->setEnabled(false);
		}
		if ((machine->portA0 & 0x10) != 0) {
			if (!machine->tapestart) {
				machine->tapestart = true;
				machine->tap->tapeStart();
			}
		} else {
			if (machine->tapestart) {
				machine->tapestart = false;
				machine->tap->tapeStop();
				machine->tap->saveDump();
			}
		}
	}
	if ((port & 0x02) == 0) {
		machine->portA1 = (uint8_t)value;
		if ((machine->portA0 & 0x08) != 0) {
			if ((machine->melodik != NULL) && machine->melodik->isEnabled()) {
				machine->melodik->write(value, machine->tstates + CPU_getCycles(machine->cpu));
			}
		}
	}
//	machine->til->DispUpdate(machine->px);
}

void Ondra::addressOnBus(uint16_t address, int32_t wstates){
	
}


int Ondra::atAddress(int address, int opcode) {
	// std::cout << "bp: " << std::hex << address << "," << opcode << std::endl;
	return opcode;
}

void Ondra::execDone() {
	// No operation.
}

uint8_t Ondra::breakpoint(uint16_t address, uint8_t opcode){
	return opcode;
}



void Ondra::openLoadTape(const std::string& canonicalPath) {
	tap->openLoadTape(canonicalPath);
}

bool Ondra::openSaveTape(const std::string& canonicalPath) {
	return tap->openSaveTape(canonicalPath);
}

void Ondra::setTapeMode(bool rec) {
	tap->setTapeMode(rec);
}

void Ondra::closeClenaup() {
	tap->closeCleanup();
}

void Ondra::setTimelineEnabled(bool enabled) {
    if (timeline != NULL) timeline->setEnabled(enabled);
}

bool Ondra::isTimelineEnabled() const {
    return timeline != NULL && timeline->isEnabled();
}

int Ondra::getTimelineSize() const {
    return timeline != NULL ? timeline->size() : 0;
}

int Ondra::getTimelineLoadedIndex() const {
    return timeline != NULL ? timeline->loadedIndex() : -1;
}

bool Ondra::loadTimelineIndex(int index) {
    if (timeline == NULL || !paused) return false;
    return timeline->loadIndex(index);
}

void Ondra::truncateTimeline(int index) {
    if (timeline != NULL && paused) timeline->truncate(index);
}

int Ondra::exportTimelineToText(const char* filename, std::string* error) const {
    if (timeline == NULL || !paused) {
        if (error != NULL) *error = "Timeline export requires a paused emulator.";
        return -1;
    }
    return timeline->exportToText(filename, error);
}

void Ondra::clearTimeline() {
    if (timeline != NULL) timeline->clear();
}

void Ondra::timelineInstructionBoundary() {
    if (timeline != NULL) timeline->instructionBoundary();
}


void Ondra::timelineDiscardPendingChanges() {
    if (timeline != NULL) timeline->discardPendingChanges();
}

void Ondra::timelineRecordRamWrite(uint16_t address, uint8_t oldValue, uint8_t newValue) {
    if (timeline != NULL) timeline->addRamChange(address, oldValue, newValue);
}

uint64_t Ondra::getTimelineSliceTarget() const {
    return timelineSliceTarget != 0 ? timelineSliceTarget : timelineResumeSliceTarget;
}

void Ondra::restoreTimelineSliceTarget(uint64_t target) {
    timelineResumeSliceTarget = target;
}

void Ondra::captureTimelineHeader(unsigned char* dest) {
    uint16_t altHL, altDE, altBC, altAF;
    uint64_t states;
    int status = 0;
    int i;

    if (dest == NULL) return;
    altHL = CPU_GetReg16Alt(cpu, REG_HL);
    altDE = CPU_GetReg16Alt(cpu, REG_DE);
    altBC = CPU_GetReg16Alt(cpu, REG_BC);
    altAF = CPU_GetReg16Alt(cpu, REG_AF);

    if (CPU_GetIff(cpu, REG_IFF1)) status |= 1;
    if (CPU_GetIff(cpu, REG_IFF2)) status |= 2;
    if (CPU_GetPendingEI(cpu)) status |= 4;
    /* OSN status bit 3 represents a pending NMI line. NMI is accepted
     * synchronously here, so there is no persistent line state to store. */
    if (isActiveINT()) status |= 16;
    if (CPU_getHaltFlag(cpu)) status |= 32;
    if (dmaEnabled) status |= 64;
    if (nDMAStatus == 1) status |= 128;

    dest[0] = CPU_GetReg8(cpu, REG_I);
    dest[1] = (unsigned char)(altHL & 0xff);
    dest[2] = (unsigned char)((altHL >> 8) & 0xff);
    dest[3] = (unsigned char)(altDE & 0xff);
    dest[4] = (unsigned char)((altDE >> 8) & 0xff);
    dest[5] = (unsigned char)(altBC & 0xff);
    dest[6] = (unsigned char)((altBC >> 8) & 0xff);
    dest[7] = (unsigned char)(altAF & 0xff);
    dest[8] = (unsigned char)((altAF >> 8) & 0xff);
    dest[9] = CPU_GetReg8(cpu, REG_L);
    dest[10] = CPU_GetReg8(cpu, REG_H);
    dest[11] = CPU_GetReg8(cpu, REG_E);
    dest[12] = CPU_GetReg8(cpu, REG_D);
    dest[13] = CPU_GetReg8(cpu, REG_C);
    dest[14] = CPU_GetReg8(cpu, REG_B);
    dest[15] = CPU_GetReg8(cpu, REG_IYL);
    dest[16] = CPU_GetReg8(cpu, REG_IYH);
    dest[17] = CPU_GetReg8(cpu, REG_IXL);
    dest[18] = CPU_GetReg8(cpu, REG_IXH);
    dest[19] = (unsigned char)status;
    dest[20] = CPU_GetReg8(cpu, REG_R);
    dest[21] = CPU_GetReg8(cpu, REG_F);
    dest[22] = CPU_GetReg8(cpu, REG_A);
    dest[23] = CPU_GetReg8(cpu, REG_SPL);
    dest[24] = CPU_GetReg8(cpu, REG_SPH);
    dest[25] = (unsigned char)(CPU_GetReg16(cpu, REG_PC) & 0xff);
    dest[26] = (unsigned char)((CPU_GetReg16(cpu, REG_PC) >> 8) & 0xff);
    dest[27] = CPU_GetIntMode(cpu);
    dest[28] = 0; /* MEMPTR is not modeled by this Z80 core. */
    dest[29] = 0;
    dest[30] = portA0;
    dest[31] = portA1;
    dest[32] = portA3;
    dest[33] = (unsigned char)nRozliseni;

    states = clk->getTstates();
    for (i = 0; i < 8; ++i) {
        dest[34 + i] = (unsigned char)((states >> ((7 - i) * 8)) & 0xff);
    }
}

void Ondra::copyTimelineRamPage(int page, unsigned char* dest) {
    if (mem != NULL) mem->copyRawRamPage(page, dest);
}

bool Ondra::restoreTimelineState(const unsigned char* state, int size) {
    uint16_t iy, ix, sp, pc;
    uint64_t states = 0;
    int status;
    int i;

    if (state == NULL || size < 42 + 0x10000 || mem == NULL) return false;

    mem->restoreRawRamFromByteArray(state + 42);

    CPU_PutReg8(cpu, REG_I, state[0]);
    CPU_PutReg8Alt(cpu, REG_L, state[1]);
    CPU_PutReg8Alt(cpu, REG_H, state[2]);
    CPU_PutReg8Alt(cpu, REG_E, state[3]);
    CPU_PutReg8Alt(cpu, REG_D, state[4]);
    CPU_PutReg8Alt(cpu, REG_C, state[5]);
    CPU_PutReg8Alt(cpu, REG_B, state[6]);
    CPU_PutReg8Alt(cpu, REG_F, state[7]);
    CPU_PutReg8Alt(cpu, REG_A, state[8]);
    CPU_PutReg8(cpu, REG_L, state[9]);
    CPU_PutReg8(cpu, REG_H, state[10]);
    CPU_PutReg8(cpu, REG_E, state[11]);
    CPU_PutReg8(cpu, REG_D, state[12]);
    CPU_PutReg8(cpu, REG_C, state[13]);
    CPU_PutReg8(cpu, REG_B, state[14]);

    iy = (uint16_t)(state[15] | ((uint16_t)state[16] << 8));
    ix = (uint16_t)(state[17] | ((uint16_t)state[18] << 8));
    CPU_PutReg8(cpu, REG_IYL, (unsigned char)(iy & 0xff));
    CPU_PutReg8(cpu, REG_IYH, (unsigned char)((iy >> 8) & 0xff));
    CPU_PutReg8(cpu, REG_IXL, (unsigned char)(ix & 0xff));
    CPU_PutReg8(cpu, REG_IXH, (unsigned char)((ix >> 8) & 0xff));

    status = state[19] & 0xff;
    CPU_SetIff(cpu, REG_IFF1, (status & 1) != 0);
    CPU_SetIff(cpu, REG_IFF2, (status & 2) != 0);
    CPU_SetPendingEI(cpu, (status & 4) != 0);
    setActiveINT((status & 16) != 0);
    CPU_setHaltFlag(cpu, (status & 32) != 0);
    CPU_PutReg8(cpu, REG_R, state[20]);
    CPU_PutReg8(cpu, REG_F, state[21]);
    CPU_PutReg8(cpu, REG_A, state[22]);

    sp = (uint16_t)(state[23] | ((uint16_t)state[24] << 8));
    pc = (uint16_t)(state[25] | ((uint16_t)state[26] << 8));
    CPU_PutReg8(cpu, REG_SPL, (unsigned char)(sp & 0xff));
    CPU_PutReg8(cpu, REG_SPH, (unsigned char)((sp >> 8) & 0xff));
    CPU_SetPC(cpu, pc);
    CPU_SetIntMode(cpu, state[27] <= 2 ? state[27] : 0);

    portA0 = state[30];
    portA1 = state[31];
    portA3 = state[32];
    nRozliseni = state[33];

    for (i = 0; i < 8; ++i) {
        states = (states << 8) | state[34 + i];
    }
    clk->setTstates(states);
    tstates = states;
    Nexttstates = states;
    timelineSliceTarget = 0;
    timelineResumeSliceTarget = 0;

    if ((portA3 & 0x02) == 0) mem->mapRom(true);
    else mem->mapRom(false);
    if ((portA3 & 0x04) == 0) mem->mapIO(false);
    else mem->mapIO(true);

    /* These DMA states are stored separately from port A3. They are normally
     * equal to A3 bit 0, but restoring the exact saved state is important at
     * an arbitrary instruction boundary. */
    nDMAStatus = (status & 128) ? 1 : 0;
    t_resolution_correct = (255 - nRozliseni) * 128;
    t_frame = nDMAStatus ? T_DMAON + t_resolution_correct : T_DMAOFF;
    genDispTables();
    if ((status & 64) != 0) dmaEnable();
    else dmaDisable();

    /* Keep the tape motor/listener in sync with the restored port state. */
    {
        bool oldTapeStart = tapestart;
        bool newTapeStart = (portA0 & 0x10) != 0;
        if (tap != NULL && oldTapeStart != newTapeStart) {
            if (newTapeStart) tap->tapeStart();
            else tap->tapeStop();
        }
        tapestart = newTapeStart;
    }

    /* Discard debugger/core transient state belonging to the abandoned future.
     * Breakpoint configuration itself is intentionally preserved. */
    CPU_PrepareTimelineRestore(cpu);

    /* SoundBuffer keeps an absolute T-state origin.  After travelling
     * backwards it must not continue with an origin from the future. */
    if (snd != NULL && snd->isEnabled()) {
        snd->resetSource((long)states);
    }

    if (til != NULL) til->DirtyTilesAll();
    if (scr != NULL) scr->needsRedraw = true;
    return true;
}

static int snapshotReadByte(FILE* file, int* value) {
    int c;
    if (!file || !value) return 0;
    c = fgetc(file);
    if (c == EOF) return 0;
    *value = c & 0xff;
    return 1;
}

static int snapshotReadWord(FILE* file, int* value) {
    int lo, hi;
    if (!snapshotReadByte(file, &lo)) return 0;
    if (!snapshotReadByte(file, &hi)) return 0;
    *value = lo | (hi << 8);
    return 1;
}

static int snapshotWriteByte(FILE* file, int value) {
    return file && fputc(value & 0xff, file) != EOF;
}

static int snapshotWriteWord(FILE* file, int value) {
    if (!snapshotWriteByte(file, value & 0xff)) return 0;
    return snapshotWriteByte(file, (value >> 8) & 0xff);
}

bool Ondra::loadSnapshot(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "rb");
    int magicO, magicS, magicN, version;
    int regI, lx, hx, ex, dx, cx, bx, fx, ax;
    int l, h, e, d, c, b;
    int iy, ix, status, regR, regF, regA, sp, pc, im, memptr;
    int savedPortA0, savedPortA1, savedPortA3;
    int fileRomType, internalRomType, resolution;
    long dataPos, fileSize, bytesNeeded;

    if (!file) return false;

    if (!snapshotReadByte(file, &magicO) ||
        !snapshotReadByte(file, &magicS) ||
        !snapshotReadByte(file, &magicN) ||
        !snapshotReadByte(file, &version)) {
        fclose(file);
        return false;
    }

    if (magicO != 'O' || magicS != 'S' || magicN != 'N' ||
        (version != 1 && version != 2)) {
        fclose(file);
        return false;
    }

    if (!snapshotReadByte(file, &regI) ||
        !snapshotReadByte(file, &lx) || !snapshotReadByte(file, &hx) ||
        !snapshotReadByte(file, &ex) || !snapshotReadByte(file, &dx) ||
        !snapshotReadByte(file, &cx) || !snapshotReadByte(file, &bx) ||
        !snapshotReadByte(file, &fx) || !snapshotReadByte(file, &ax) ||
        !snapshotReadByte(file, &l)  || !snapshotReadByte(file, &h)  ||
        !snapshotReadByte(file, &e)  || !snapshotReadByte(file, &d)  ||
        !snapshotReadByte(file, &c)  || !snapshotReadByte(file, &b)  ||
        !snapshotReadWord(file, &iy) || !snapshotReadWord(file, &ix) ||
        !snapshotReadByte(file, &status) ||
        !snapshotReadByte(file, &regR) ||
        !snapshotReadByte(file, &regF) ||
        !snapshotReadByte(file, &regA) ||
        !snapshotReadWord(file, &sp) || !snapshotReadWord(file, &pc) ||
        !snapshotReadByte(file, &im) || !snapshotReadWord(file, &memptr) ||
        !snapshotReadByte(file, &savedPortA0) ||
        !snapshotReadByte(file, &savedPortA1) ||
        !snapshotReadByte(file, &savedPortA3) ||
        !snapshotReadByte(file, &fileRomType)) {
        fclose(file);
        return false;
    }

    resolution = 255;
    if (version == 2) {
        if (!snapshotReadByte(file, &resolution)) {
            fclose(file);
            return false;
        }
    }

    /* OSN ROM identifiers are BASIC=0, TESLA=1, VILI=2, PLUS=3 and
       CUSTOM=100. Internally, settings use 3 for CUSTOM; PLUS is not
       implemented. */
    if (fileRomType == 0 || fileRomType == 1 || fileRomType == 2) {
        internalRomType = fileRomType;
    } else if (fileRomType == 100) {
        internalRomType = 3;
    } else {
        fclose(file);
        return false;
    }

    /* Reject truncated snapshots before changing the machine state. */
    dataPos = ftell(file);
    if (dataPos < 0 || fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, dataPos, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    bytesNeeded = 32L * PAGE_SIZE;
    if (internalRomType == 3) bytesNeeded += 8L * PAGE_SIZE;
    if ((fileSize - dataPos) < bytesNeeded) {
        fclose(file);
        return false;
    }

    if (internalRomType == 3) {
        if (!mem->loadSnapshotCustomRom(file)) {
            fclose(file);
            return false;
        }
    }
    if (!mem->loadSnapshotRam(file)) {
        fclose(file);
        return false;
    }
    fclose(file);

    Config::setRomType(internalRomType);

    CPU_PutReg8(cpu, REG_I, (unsigned char)regI);
    CPU_PutReg8Alt(cpu, REG_L, (unsigned char)lx);
    CPU_PutReg8Alt(cpu, REG_H, (unsigned char)hx);
    CPU_PutReg8Alt(cpu, REG_E, (unsigned char)ex);
    CPU_PutReg8Alt(cpu, REG_D, (unsigned char)dx);
    CPU_PutReg8Alt(cpu, REG_C, (unsigned char)cx);
    CPU_PutReg8Alt(cpu, REG_B, (unsigned char)bx);
    CPU_PutReg8Alt(cpu, REG_F, (unsigned char)fx);
    CPU_PutReg8Alt(cpu, REG_A, (unsigned char)ax);
    CPU_PutReg8(cpu, REG_L, (unsigned char)l);
    CPU_PutReg8(cpu, REG_H, (unsigned char)h);
    CPU_PutReg8(cpu, REG_E, (unsigned char)e);
    CPU_PutReg8(cpu, REG_D, (unsigned char)d);
    CPU_PutReg8(cpu, REG_C, (unsigned char)c);
    CPU_PutReg8(cpu, REG_B, (unsigned char)b);
    CPU_PutReg8(cpu, REG_IYL, (unsigned char)(iy & 0xff));
    CPU_PutReg8(cpu, REG_IYH, (unsigned char)((iy >> 8) & 0xff));
    CPU_PutReg8(cpu, REG_IXL, (unsigned char)(ix & 0xff));
    CPU_PutReg8(cpu, REG_IXH, (unsigned char)((ix >> 8) & 0xff));
    CPU_SetIff(cpu, REG_IFF1, (status & 1) != 0);
    CPU_SetIff(cpu, REG_IFF2, (status & 2) != 0);
    CPU_SetPendingEI(cpu, (status & 4) != 0);
    CPU_PutReg8(cpu, REG_R, (unsigned char)regR);
    CPU_PutReg8(cpu, REG_F, (unsigned char)regF);
    CPU_PutReg8(cpu, REG_A, (unsigned char)regA);
    CPU_PutReg8(cpu, REG_SPL, (unsigned char)(sp & 0xff));
    CPU_PutReg8(cpu, REG_SPH, (unsigned char)((sp >> 8) & 0xff));
    CPU_SetPC(cpu, (unsigned short)pc);
    CPU_SetIntMode(cpu, im <= 2 ? im : 0);
    CPU_setHaltFlag(cpu, (status & 32) != 0);
    setActiveINT((status & 16) != 0);

    /* The Z80 core has no MEMPTR or latched-NMI state. Their OSN fields are
       still consumed/written to preserve the snapshot file format. */
    (void)memptr;

    nRozliseni = resolution & 0xff;
    changeResolution();

    /* Restore ports in file order. A3 restores ROM/IO mapping and DMA state;
       A0 also restores LEDs and tape control. */
    outPort(0x0e, (uint8_t)savedPortA0);
    outPort(0x0d, (uint8_t)savedPortA1);
    outPort(0x03, (uint8_t)savedPortA3);

    CPU_ClearMemoryBreakpointHit(cpu);
    if (til) til->DirtyTilesAll();
    if (scr) scr->needsRedraw = true;
    return true;
}

bool Ondra::saveSnapshot(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "wb");
    uint16_t altHL, altDE, altBC, altAF;
    int status = 0;
    int fileRomType;
    int ok = 1;

    if (!file) return false;

    altHL = CPU_GetReg16Alt(cpu, REG_HL);
    altDE = CPU_GetReg16Alt(cpu, REG_DE);
    altBC = CPU_GetReg16Alt(cpu, REG_BC);
    altAF = CPU_GetReg16Alt(cpu, REG_AF);

    if (CPU_GetIff(cpu, REG_IFF1)) status |= 1;
    if (CPU_GetIff(cpu, REG_IFF2)) status |= 2;
    if (CPU_GetPendingEI(cpu)) status |= 4;
    if (isActiveINT()) status |= 16;
    if (CPU_getHaltFlag(cpu)) status |= 32;

    /* OSN uses 100 for CUSTOM, while settings use 3 internally. */
    fileRomType = Config::getRomType();
    if (fileRomType == 3) fileRomType = 100;

    ok = ok && snapshotWriteByte(file, 'O');
    ok = ok && snapshotWriteByte(file, 'S');
    ok = ok && snapshotWriteByte(file, 'N');
    ok = ok && snapshotWriteByte(file, OSN_VERSION);
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_I));
    ok = ok && snapshotWriteByte(file, altHL & 0xff);          /* L' */
    ok = ok && snapshotWriteByte(file, (altHL >> 8) & 0xff);   /* H' */
    ok = ok && snapshotWriteByte(file, altDE & 0xff);          /* E' */
    ok = ok && snapshotWriteByte(file, (altDE >> 8) & 0xff);   /* D' */
    ok = ok && snapshotWriteByte(file, altBC & 0xff);          /* C' */
    ok = ok && snapshotWriteByte(file, (altBC >> 8) & 0xff);   /* B' */
    ok = ok && snapshotWriteByte(file, altAF & 0xff);          /* F' */
    ok = ok && snapshotWriteByte(file, (altAF >> 8) & 0xff);   /* A' */
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_L));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_H));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_E));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_D));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_C));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_B));
    ok = ok && snapshotWriteWord(file, CPU_GetReg16(cpu, REG_IY));
    ok = ok && snapshotWriteWord(file, CPU_GetReg16(cpu, REG_IX));
    ok = ok && snapshotWriteByte(file, status);
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_R));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_F));
    ok = ok && snapshotWriteByte(file, CPU_GetReg8(cpu, REG_A));
    ok = ok && snapshotWriteWord(file, CPU_GetReg16(cpu, REG_SP));
    ok = ok && snapshotWriteWord(file, CPU_GetReg16(cpu, REG_PC));
    ok = ok && snapshotWriteByte(file, CPU_GetIntMode(cpu));
    ok = ok && snapshotWriteWord(file, 0); /* MEMPTR is not modeled by this core. */
    ok = ok && snapshotWriteByte(file, portA0);
    ok = ok && snapshotWriteByte(file, portA1);
    ok = ok && snapshotWriteByte(file, portA3);
    ok = ok && snapshotWriteByte(file, fileRomType);
    ok = ok && snapshotWriteByte(file, nRozliseni);

    if (ok && Config::getRomType() == 3) {
        ok = mem->saveSnapshotCustomRom(file);
    }
    if (ok) {
        ok = mem->saveSnapshotRam(file);
    }

    if (fclose(file) != 0) ok = 0;
    return ok != 0;
}


