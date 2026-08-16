#ifndef ONDRA_H
#define ONDRA_H


#include <string>
#include "z80emu.h"
#include "Clock.h"

// Forward deklarace
class JScreen;
class Config;
class Memory;
class Keyboard;
class Z80;
class Tape;
class Jondra;
class Sound;
class Melodik;
class Debugger;
class MTimer;
class Z80State;
class DirtyTiles;
class MemoryTimeline;


// Konstanty
#define T_DMAOFF (312 * 128)
#define T_DMAON ((312 - 255) * 128)
#define OSN_VERSION 0x02


class Ondra {
public:
    // Konstruktor a destruktor
    Ondra();
    ~Ondra();

    void setDebugger(Debugger* indeb);
    void setFrame(Jondra* inJon);
    Debugger* getDebugger();
    Keyboard* getKeyboard();
    Config* getConfig();
    void setScreen(JScreen* screen);
    void setGreenLed(void* led);
    void setYellowLed(void* led);
    void setTapeLed(void* led);
    void setRecButton(void* b);
    void setClockSpeed(int inSpeed);
    int getClockSpeed();
    void setCpuSpeedPercent(int percent);
    int getCpuSpeedPercent() const;
    void Reset(bool dirty);
    void Nmi();
    void startEmulation();
    void stopEmulation();
    uint64_t debugStepInto();
    bool debugStartStepOver(uint16_t targetAddress);
    bool debugFinishStepOver();
    void debugCancelStepOver();
    bool debugFinishUserBreakpoint();
    int debugFinishMemoryBreakpoint();
    bool isPaused();
    unsigned char getPortA0() const { return portA0; }
    unsigned char getPortA1() const { return portA1; }
    unsigned char getPortA3() const { return portA3; }
    void genDispTables();
    void ms20();
#ifndef _WIN32
    // Called only by the FLTK main thread. Returns the latest speed value
    // prepared by the emulation thread without touching X11 from that thread.
    bool takePendingWindowSpeed(int* speedPercent);
#endif
    void StartArgumentImage(bool bAutoStart);
    void run();
    void processVram(int address);
    void dmaEnable();
    void dmaDisable();
	uint64_t getTstates();
	void interruptHandlingTime(uint64_t wstates);
    uint8_t fetchOpcode(uint16_t address);
    
    static uint8_t fetch8(uint16_t address);
	static uint8_t peek8(uint16_t address);
    static void poke8(uint16_t address, uint8_t value);
    static uint16_t peek16(uint16_t address);
    static void poke16(uint16_t address, uint16_t word);
	static uint8_t inPort(uint16_t port);
	static void outPort(uint16_t port, uint8_t value);
	static void addTstates(uint64_t states);
	
	bool isActiveINT();
	void setActiveINT(bool inInt);
    void changeResolution();	
	void addressOnBus(uint16_t address, int32_t wstates);
    int atAddress(int address, int opcode);
    void execDone();
	uint8_t breakpoint(uint16_t address, uint8_t opcode);
    void openLoadTape(const std::string& canonicalPath);
    bool openSaveTape(const std::string& canonicalPath);
    void setTapeMode(bool rec);
    void closeClenaup();
    bool loadSnapshot(const std::string& filename);
    bool saveSnapshot(const std::string& filename);

    // Debugger timeline. Ordinary records are deltas; RAM keyframes use
    // shared 2 KB pages so old machines do not need a 64 KB copy per keyframe.
    void setTimelineEnabled(bool enabled);
    bool isTimelineEnabled() const;
    int getTimelineSize() const;
    int getTimelineLoadedIndex() const;
    bool loadTimelineIndex(int index);
    void truncateTimeline(int index);
    int exportTimelineToText(const char* filename, std::string* error) const;
    void clearTimeline();
    void timelineInstructionBoundary();
    void timelineDiscardPendingChanges();
    void timelineRecordRamWrite(uint16_t address, uint8_t oldValue, uint8_t newValue);
    void captureTimelineHeader(unsigned char* dest);
    void copyTimelineRamPage(int page, unsigned char* dest);
    bool restoreTimelineState(const unsigned char* state, int size);
    uint64_t getTimelineSliceTarget() const;
    void restoreTimelineSliceTarget(uint64_t target);


	int nCnt;

public:
	Memory* mem;
    uint8_t cpu;
	Clock* clk;
	DirtyTiles* til;
    MTimer* task;
	uint8_t* px;
	uint8_t* px_shadow;
	static Ondra* machine;
	JScreen* scr;
	std::string strArgFile;
	int32_t nStartAddress;
private:    	
    Config* cfg;
    Keyboard* key;


    Tape* tap;
    Jondra* frame;
    Sound* snd;
    Melodik* melodik;
    Debugger* deb;
    MemoryTimeline* timeline;

    void* GreenLed;
    void* YellowLed;
    void* TapeLed;
    void* RecButton;

    bool paused;
    bool dmaEnabled;
	bool activeINT;
    bool tapestart;
    bool bFrst;
    bool bAutoRunAfterReset;

    int msSpeed;
    int cpuSpeedPercent;
	int nSpeedPercent;
#ifndef _WIN32
    volatile int pendingWindowSpeed;
#endif
    int nSpeedPercentUpdateMaxCycles;
    int nSpeedPercentUpdateDec;
    int t_resolution_correct;
    int t_frame;
    int nDMAStatus;
    int nRozliseni;
    int memorySize;
    int maxRecords;
    int snapshotInterval;
    uint64_t timelineSliceTarget;
    uint64_t timelineResumeSliceTarget;

    uint64_t nLastSeen;
    uint64_t nLastMilisec;

    int* dispAdr;             // Pole adres
    unsigned char* iov;       // Pole pro I/O
    unsigned char* snapshotBuffer;     // Buffer pro snapshot
    unsigned char portA0, portA1, portA3;

    bool running;
	static uint64_t tstates;
	static uint64_t Nexttstates;
	
};

#endif // ONDRA_H
