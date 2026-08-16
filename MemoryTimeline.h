#ifndef MEMORYTIMELINE_H
#define MEMORYTIMELINE_H

#if defined(_MSC_VER) && (_MSC_VER < 1300)
    #include "vs_stdint.h"
#else
    #include <stdint.h>
#endif

#include <string>

class Ondra;
class MemoryTimelineImpl;

/*
 * Instruction history used by the debugger.
 *
 * Ordinary records are stored as deltas and a complete keyframe is inserted
 * periodically. RAM keyframes use copy-on-write 2 KB pages, so unchanged pages
 * are shared instead of copying 64 KB every snapshot interval.
 *
 * No locking is needed here: the emulation thread is the only writer and the
 * debugger reads the timeline only after stopEmulation() has joined that
 * thread.
 */
class MemoryTimeline {
public:
    MemoryTimeline(Ondra* machine, int maxRecords, int snapshotInterval);
    ~MemoryTimeline();

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void clear();
    void captureCurrentState();
    void instructionBoundary();
    void addRamChange(uint16_t address, uint8_t oldValue, uint8_t newValue);
    void discardPendingChanges();

    int size() const;
    int loadedIndex() const;
    bool loadIndex(int index);
    void truncate(int index);

    int exportToText(const char* filename, std::string* error) const;

private:
    MemoryTimelineImpl* impl;

    MemoryTimeline(const MemoryTimeline&);
    MemoryTimeline& operator=(const MemoryTimeline&);
};

#endif
