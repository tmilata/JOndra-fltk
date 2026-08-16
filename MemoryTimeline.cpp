#include "MemoryTimeline.h"
#include "Ondra.h"
#include "Memory.h"
#include "z80disassembler.h"

#include <deque>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <string>

#define TIMELINE_HEADER_SIZE 42
#define TIMELINE_RAM_PAGES   32
#define TIMELINE_PAGE_SIZE   2048

/* Shared opcode buffer used by the existing C++ Z80 disassembler. */
extern UBYTE Opcodes[65536];


struct TimelineChange {
    uint16_t address;
    uint8_t oldValue;
    uint8_t newValue;
    uint8_t ram;
};

struct TimelinePage {
    int refs;
    uint8_t data[TIMELINE_PAGE_SIZE];
};

struct TimelineSnapshot {
    uint8_t header[TIMELINE_HEADER_SIZE];
    TimelinePage* pages[TIMELINE_RAM_PAGES];
};

struct TimelineRecord {
    unsigned long firstChange;
    unsigned int changeCount;
    TimelineSnapshot* snapshot;
    uint64_t sliceTarget;
};


struct TimelineMemoryWrite {
    uint16_t address;
    uint8_t oldValue;
    uint8_t newValue;
};

namespace {
    void timelineAppendHex(std::string& out, unsigned int value, int digits) {
        static const char hex[] = "0123456789ABCDEF";
        int shift;
        for (shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
            out += hex[(value >> shift) & 0x0f];
        }
    }

    void timelineAppendPadded(std::string& out, const std::string& value,
                              int width) {
        int i;
        out += value;
        for (i = (int)value.size(); i < width; ++i) out += ' ';
    }

    unsigned int timelineReadWordLE(const uint8_t* state, int offset) {
        return (unsigned int)state[offset] |
               ((unsigned int)state[offset + 1] << 8);
    }

    uint64_t timelineReadU64BE(const uint8_t* state, int offset) {
        uint64_t value = 0;
        int i;
        for (i = 0; i < 8; ++i) {
            value = (value << 8) | (uint64_t)state[offset + i];
        }
        return value;
    }

    std::string timelineUnsigned64(uint64_t value) {
        char reversed[32];
        char normal[32];
        int count = 0;
        int i;

        if (value == 0) return "0";
        while (value != 0 && count < (int)sizeof(reversed)) {
            reversed[count++] = (char)('0' + (unsigned int)(value % 10));
            value /= 10;
        }
        for (i = 0; i < count; ++i) normal[i] = reversed[count - 1 - i];
        normal[count] = '\0';
        return std::string(normal);
    }

    std::string timelineTrim(const char* text) {
        const char* begin;
        const char* end;
        if (text == NULL) return std::string();
        begin = text;
        while (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n') ++begin;
        end = begin + strlen(begin);
        while (end > begin &&
               (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
            --end;
        }
        return std::string(begin, (size_t)(end - begin));
    }

    std::string timelineFormatFlags(unsigned int flags) {
        std::string out;
        out.reserve(18);
        out += (flags & 0x80) ? "M" : "P";
        out += (flags & 0x40) ? ",Z" : ",NZ";
        if (flags & 0x10) out += ",H";
        out += (flags & 0x04) ? ",PE" : ",PO";
        if (flags & 0x02) out += ",N";
        out += (flags & 0x01) ? ",C" : ",NC";
        return out;
    }

    void timelineAppendRegisterPair(std::string& out, const uint8_t* state,
                                    int highOffset, int lowOffset) {
        unsigned int value = ((unsigned int)state[highOffset] << 8) |
                             (unsigned int)state[lowOffset];
        timelineAppendHex(out, value, 4);
    }

    void timelineCopySnapshotState(const TimelineSnapshot* snapshot,
                                   std::vector<uint8_t>& state) {
        int page;
        if (state.size() != TIMELINE_HEADER_SIZE + 0x10000) {
            state.resize(TIMELINE_HEADER_SIZE + 0x10000);
        }
        memcpy(&state[0], snapshot->header, TIMELINE_HEADER_SIZE);
        for (page = 0; page < TIMELINE_RAM_PAGES; ++page) {
            memcpy(&state[TIMELINE_HEADER_SIZE + page * TIMELINE_PAGE_SIZE],
                   snapshot->pages[page]->data, TIMELINE_PAGE_SIZE);
        }
    }

    void timelineCopyRamToDisassembler(const uint8_t* state) {
        memcpy(Opcodes, state + TIMELINE_HEADER_SIZE, 0x10000);
    }

    std::string timelineBuildTraceLine(const uint8_t* state) {
        unsigned int pc = timelineReadWordLE(state, 25) & 0xffff;
        unsigned int status = state[19];
        unsigned int flags = state[21];
        unsigned int length = 1;
        unsigned int i;
        char instruction[1024];
        std::string mnemonic;
        std::string bytes;
        std::string line;

        instruction[0] = '\0';

        /*
         * The old C++ disassembler owns a fixed 64 KB opcode array and reads
         * operands with adr+1..adr+3.  Near FFFFh use the same safe one-byte
         * fallback as the debugger window instead of reading past that array.
         */
        if (pc <= 0xfffb) {
            length = OpcodeLen(pc);
            if (length < 1 || length > 4) length = 1;
            Disassemble(pc, instruction);
            mnemonic = timelineTrim(instruction);
        }
        if (mnemonic.empty()) {
            mnemonic = "DB #";
            timelineAppendHex(mnemonic, Opcodes[pc], 2);
            length = 1;
        }

        for (i = 0; i < length; ++i) {
            if (i != 0) bytes += ' ';
            timelineAppendHex(bytes, Opcodes[pc + i], 2);
        }

        line.reserve(256);
        timelineAppendHex(line, pc, 4);
        line += ": ";
        timelineAppendPadded(line, bytes, 12);
        timelineAppendPadded(line, mnemonic, 20);

        line += "AF=";
        timelineAppendRegisterPair(line, state, 22, 21);
        line += " BC=";
        timelineAppendRegisterPair(line, state, 14, 13);
        line += " DE=";
        timelineAppendRegisterPair(line, state, 12, 11);
        line += " HL=";
        timelineAppendRegisterPair(line, state, 10, 9);

        line += "  AF'=";
        timelineAppendRegisterPair(line, state, 8, 7);
        line += " BC'=";
        timelineAppendRegisterPair(line, state, 6, 5);
        line += " DE'=";
        timelineAppendRegisterPair(line, state, 4, 3);
        line += " HL'=";
        timelineAppendRegisterPair(line, state, 2, 1);

        line += "  IX=";
        timelineAppendHex(line, timelineReadWordLE(state, 17), 4);
        line += " IY=";
        timelineAppendHex(line, timelineReadWordLE(state, 15), 4);
        line += " SP=";
        timelineAppendHex(line, timelineReadWordLE(state, 23), 4);
        line += " I=";
        timelineAppendHex(line, state[0], 2);
        line += " R=";
        timelineAppendHex(line, state[20], 2);
        line += " IM=";
        line += (char)('0' + (state[27] <= 9 ? state[27] : 0));
        line += " IFF=";
        line += (status & 1) ? '1' : '0';
        line += (status & 2) ? '1' : '0';
        line += ' ';

        timelineAppendPadded(line, (status & 0x20) ? "HALT" : "", 5);
        timelineAppendPadded(line, std::string("F=") + timelineFormatFlags(flags), 18);
        line += "T=";
        line += timelineUnsigned64(timelineReadU64BE(state, 34));
        return line;
    }

    void timelineAppendWrites(std::string& line,
                              const std::vector<TimelineMemoryWrite>& writes) {
        size_t i;
        if (writes.empty()) return;
        while (line.size() < 190) line += ' ';
        if (line.size() >= 190) line += ' ';
        line += "M[";
        for (i = 0; i < writes.size(); ++i) {
            if (i != 0) line += ',';
            timelineAppendHex(line, writes[i].address, 4);
            line += ':';
            timelineAppendHex(line, writes[i].oldValue, 2);
            line += "->";
            timelineAppendHex(line, writes[i].newValue, 2);
        }
        line += ']';
    }
}

class MemoryTimelineImpl {
public:
    MemoryTimelineImpl(Ondra* inMachine, int inMaxRecords, int inSnapshotInterval)
        : machine(inMachine), maxRecords(inMaxRecords),
          snapshotInterval(inSnapshotInterval), enabled(false),
          havePrevious(false), stepCount(0), changeBase(0), nextChange(0),
          selectedIndex(-1), activeRecordCount(0) {
        if (maxRecords < 2) maxRecords = 2;
        if (snapshotInterval < 1) snapshotInterval = 1;
        memset(previousHeader, 0, sizeof(previousHeader));
        memset(dirtyPages, 0, sizeof(dirtyPages));
        pending.reserve(16);
    }

    ~MemoryTimelineImpl() {
        clearRecords();
    }

    Ondra* machine;
    int maxRecords;
    int snapshotInterval;
    bool enabled;
    bool havePrevious;
    int stepCount;
    unsigned long changeBase;
    unsigned long nextChange;
    int selectedIndex;
    int activeRecordCount;
    uint8_t previousHeader[TIMELINE_HEADER_SIZE];
    bool dirtyPages[TIMELINE_RAM_PAGES];
    std::deque<TimelineRecord> records;
    std::deque<TimelineChange> changes;
    std::vector<TimelineChange> pending;
    std::vector<uint8_t> rebuild;


    void releasePage(TimelinePage* page) {
        if (page == NULL) return;
        page->refs--;
        if (page->refs <= 0) delete page;
    }

    void releaseSnapshot(TimelineSnapshot* snapshot) {
        int i;
        if (snapshot == NULL) return;
        for (i = 0; i < TIMELINE_RAM_PAGES; ++i) {
            releasePage(snapshot->pages[i]);
            snapshot->pages[i] = NULL;
        }
        delete snapshot;
    }

    void clearRecords() {
        while (!records.empty()) {
            TimelineRecord& record = records.back();
            releaseSnapshot(record.snapshot);
            records.pop_back();
        }
        changes.clear();
        pending.clear();
        rebuild.clear();
        changeBase = 0;
        nextChange = 0;
        havePrevious = false;
        stepCount = 0;
        selectedIndex = -1;
        activeRecordCount = 0;
        memset(previousHeader, 0, sizeof(previousHeader));
        memset(dirtyPages, 0, sizeof(dirtyPages));
    }

    TimelineSnapshot* latestSnapshot() {
        int i;
        for (i = activeRecordCount - 1; i >= 0; --i) {
            if (records[(size_t)i].snapshot != NULL) {
                return records[(size_t)i].snapshot;
            }
        }
        return NULL;
    }

    TimelineSnapshot* createSnapshot(const uint8_t* header) {
        TimelineSnapshot* previous = latestSnapshot();
        TimelineSnapshot* snapshot = new TimelineSnapshot;
        uint8_t livePage[TIMELINE_PAGE_SIZE];
        int page;

        memcpy(snapshot->header, header, TIMELINE_HEADER_SIZE);
        for (page = 0; page < TIMELINE_RAM_PAGES; ++page) {
            bool sharePrevious = false;

            if (previous != NULL && !dirtyPages[page]) {
                /*
                 * dirtyPages[] is only an optimisation hint.  Normal Z80 RAM
                 * writes pass through timelineRecordRamWrite(), but bulk image
                 * loading and a few debugger/file operations write Memory
                 * directly.  Such a write used to leave the page marked clean,
                 * so every later snapshot kept sharing the stale pre-load page.
                 *
                 * Before sharing a supposedly clean page, verify it against
                 * the real physical RAM.  This keeps the page-sharing memory
                 * optimisation while preserving the essential rule that every
                 * periodic snapshot represents the memory that actually exists
                 * at that instant.
                 */
                machine->copyTimelineRamPage(page, livePage);
                sharePrevious =
                    memcmp(livePage, previous->pages[page]->data,
                           TIMELINE_PAGE_SIZE) == 0;
            }

            if (sharePrevious) {
                snapshot->pages[page] = previous->pages[page];
                snapshot->pages[page]->refs++;
            } else {
                TimelinePage* newPage = new TimelinePage;
                newPage->refs = 1;
                if (previous != NULL && !dirtyPages[page]) {
                    /* livePage was already copied for the verification above. */
                    memcpy(newPage->data, livePage, TIMELINE_PAGE_SIZE);
                } else {
                    machine->copyTimelineRamPage(page, newPage->data);
                }
                snapshot->pages[page] = newPage;
            }
        }
        memset(dirtyPages, 0, sizeof(dirtyPages));
        return snapshot;
    }

    void appendChange(const TimelineChange& change) {
        unsigned long offset = nextChange - changeBase;

        /* After travelling back, the old future remains physically allocated
         * for a while.  Reuse its change slots instead of destroying hundreds
         * of thousands of objects synchronously in the GUI thread. */
        if (offset < (unsigned long)changes.size()) {
            changes[(size_t)offset] = change;
        } else {
            changes.push_back(change);
        }
        nextChange++;
    }

    void storeRecord(const TimelineRecord& record) {
        if (activeRecordCount < (int)records.size()) {
            /* This slot belongs to the abandoned future.  Releasing one old
             * snapshot here is cheap and spreads cleanup over normal execution. */
            releaseSnapshot(records[(size_t)activeRecordCount].snapshot);
            records[(size_t)activeRecordCount] = record;
        } else {
            records.push_back(record);
        }
        activeRecordCount++;
    }

    void appendHeaderChanges(const uint8_t* header, unsigned int* count) {
        int i;
        for (i = 0; i < TIMELINE_HEADER_SIZE; ++i) {
            if (previousHeader[i] != header[i]) {
                TimelineChange change;
                change.address = (uint16_t)i;
                change.oldValue = previousHeader[i];
                change.newValue = header[i];
                change.ram = 0;
                appendChange(change);
                (*count)++;
            }
        }
    }

    void appendPendingRamChanges(unsigned int* count) {
        size_t i;
        for (i = 0; i < pending.size(); ++i) {
            appendChange(pending[i]);
            (*count)++;
        }
        pending.clear();
    }

    void removeFrontRecord() {
        TimelineRecord record;
        unsigned int i;
        if (activeRecordCount <= 0 || records.empty()) return;
        record = records.front();
        records.pop_front();
        activeRecordCount--;

        /* The remaining first active record owns the next portion of the
         * change log.  Dormant future data, if any, lives only at the tail. */
        for (i = 0; i < record.changeCount && !changes.empty(); ++i) {
            changes.pop_front();
            changeBase++;
        }
        releaseSnapshot(record.snapshot);
    }

    void trimOldRecords() {
        int nextSnapshot = -1;
        int i;

        if (activeRecordCount <= maxRecords) return;

        /* The first active record is a keyframe.  Find only the following
         * keyframe, then drop one complete block.  With the normal interval
         * this examines roughly 100 records, never the whole 500000-record
         * timeline. */
        for (i = 1; i < activeRecordCount; ++i) {
            if (records[(size_t)i].snapshot != NULL) {
                nextSnapshot = i;
                break;
            }
        }
        if (nextSnapshot < 0) return;

        for (i = 0; i < nextSnapshot; ++i) {
            removeFrontRecord();
        }
        selectedIndex = activeRecordCount - 1;
    }

    void captureInitial() {
        uint8_t header[TIMELINE_HEADER_SIZE];
        TimelineRecord record;

        machine->captureTimelineHeader(header);
        record.firstChange = nextChange;
        record.changeCount = 0;
        record.snapshot = createSnapshot(header);
        record.sliceTarget = machine->getTimelineSliceTarget();
        storeRecord(record);
        memcpy(previousHeader, header, TIMELINE_HEADER_SIZE);
        havePrevious = true;
        stepCount = 0;
        pending.clear();
        selectedIndex = 0;
    }

    void updateBoundary() {
        uint8_t header[TIMELINE_HEADER_SIZE];
        TimelineRecord record;
        unsigned int count = 0;
        bool makeSnapshot;

        if (!enabled || machine == NULL) return;
        if (!havePrevious || activeRecordCount <= 0) {
            captureInitial();
            return;
        }

        machine->captureTimelineHeader(header);
        /* Z80Emulate() is entered repeatedly for 20 ms slices.  The state at
         * the beginning of a new slice is normally identical to the boundary
         * already recorded at the end of the previous slice; do not create an
         * empty duplicate record in that case. */
        if (pending.empty() &&
            memcmp(previousHeader, header, TIMELINE_HEADER_SIZE) == 0) {
            return;
        }

        stepCount++;
        makeSnapshot = (stepCount >= snapshotInterval);

        record.firstChange = nextChange;
        appendHeaderChanges(header, &count);
        appendPendingRamChanges(&count);
        record.changeCount = count;
        record.snapshot = makeSnapshot ? createSnapshot(header) : NULL;
        record.sliceTarget = machine->getTimelineSliceTarget();
        storeRecord(record);

        memcpy(previousHeader, header, TIMELINE_HEADER_SIZE);
        if (makeSnapshot) stepCount = 0;
        selectedIndex = activeRecordCount - 1;
        trimOldRecords();
    }

    const TimelineChange& changeAt(const TimelineRecord& record,
                                   unsigned int index) const {
        unsigned long absoluteIndex = record.firstChange + index;
        unsigned long offset = absoluteIndex - changeBase;
        return changes[(size_t)offset];
    }

    void applyRecord(uint8_t* state, const TimelineRecord& record) const {
        unsigned int i;
        for (i = 0; i < record.changeCount; ++i) {
            const TimelineChange& change = changeAt(record, i);
            if (change.ram) {
                state[TIMELINE_HEADER_SIZE + (unsigned int)change.address] = change.newValue;
            } else if (change.address < TIMELINE_HEADER_SIZE) {
                state[change.address] = change.newValue;
            }
        }
    }

    bool rebuildState(int index) {
        int snapshotIndex;
        int page;
        int i;
        TimelineSnapshot* snapshot;

        if (index < 0 || index >= activeRecordCount) return false;
        snapshotIndex = index;
        while (snapshotIndex >= 0 && records[(size_t)snapshotIndex].snapshot == NULL) {
            snapshotIndex--;
        }
        if (snapshotIndex < 0) return false;

        snapshot = records[(size_t)snapshotIndex].snapshot;
        rebuild.resize(TIMELINE_HEADER_SIZE + 0x10000);
        memcpy(&rebuild[0], snapshot->header, TIMELINE_HEADER_SIZE);
        for (page = 0; page < TIMELINE_RAM_PAGES; ++page) {
            memcpy(&rebuild[TIMELINE_HEADER_SIZE + page * TIMELINE_PAGE_SIZE],
                   snapshot->pages[page]->data, TIMELINE_PAGE_SIZE);
        }

        for (i = snapshotIndex + 1; i <= index; ++i) {
            applyRecord(&rebuild[0], records[(size_t)i]);
        }
        return true;
    }


    void applyRecordForExport(std::vector<uint8_t>& state,
                              const TimelineRecord& record,
                              std::vector<TimelineMemoryWrite>& writes) const {
        unsigned int i;
        writes.clear();

        for (i = 0; i < record.changeCount; ++i) {
            const TimelineChange& change = changeAt(record, i);
            if (change.ram) {
                TimelineMemoryWrite write;
                write.address = change.address;
                write.oldValue = change.oldValue;
                write.newValue = change.newValue;
                writes.push_back(write);
                state[TIMELINE_HEADER_SIZE + (unsigned int)change.address] =
                    change.newValue;
                Opcodes[change.address] = change.newValue;
            } else if (change.address < TIMELINE_HEADER_SIZE) {
                state[change.address] = change.newValue;
            }
        }

        /* A periodic keyframe is authoritative. */
        if (record.snapshot != NULL) {
            timelineCopySnapshotState(record.snapshot, state);
            timelineCopyRamToDisassembler(&state[0]);
        }
    }

    int exportToText(const char* filename, std::string* error) const {
        FILE* file;
        std::vector<uint8_t> state;
        std::vector<uint8_t> savedOpcodes;
        std::vector<TimelineMemoryWrite> writes;
        std::vector<char> fileBuffer;
        int recordIndex;
        int exported = 0;
        bool failed = false;

        if (error != NULL) *error = "";
        if (filename == NULL || filename[0] == '\0') {
            if (error != NULL) *error = "No export file was selected.";
            return -1;
        }
        if (activeRecordCount < 2) return 0;
        if (records.empty() || records[0].snapshot == NULL) {
            if (error != NULL) *error = "Timeline does not start with a valid snapshot.";
            return -1;
        }

        file = fopen(filename, "w");
        if (file == NULL) {
            if (error != NULL) *error = "Cannot create the export file.";
            return -1;
        }

        fileBuffer.resize(64 * 1024);
        if (!fileBuffer.empty()) {
            setvbuf(file, &fileBuffer[0], _IOFBF, fileBuffer.size());
        }

        /* Disassemble against historical RAM without disturbing the debugger. */
        savedOpcodes.resize(0x10000);
        memcpy(&savedOpcodes[0], Opcodes, 0x10000);

        timelineCopySnapshotState(records[0].snapshot, state);
        timelineCopyRamToDisassembler(&state[0]);
        writes.reserve(16);

        for (recordIndex = 1; recordIndex < activeRecordCount; ++recordIndex) {
            std::string line = timelineBuildTraceLine(&state[0]);
            applyRecordForExport(state, records[(size_t)recordIndex], writes);
            timelineAppendWrites(line, writes);
            line += '\n';

            if (fwrite(line.c_str(), 1, line.size(), file) != line.size()) {
                failed = true;
                if (error != NULL) *error = "Cannot write the complete timeline export.";
                break;
            }
            exported++;
        }

        if (!failed && fflush(file) != 0) {
            failed = true;
            if (error != NULL) *error = "Cannot flush the timeline export to disk.";
        }
        if (fclose(file) != 0 && !failed) {
            failed = true;
            if (error != NULL) *error = "Cannot close the timeline export file.";
        }

        memcpy(Opcodes, &savedOpcodes[0], 0x10000);
        return failed ? -1 : exported;
    }


    void rebuildDirtyPageMask() {
        int snapshotIndex;
        int i;
        memset(dirtyPages, 0, sizeof(dirtyPages));
        if (activeRecordCount <= 0) return;

        snapshotIndex = activeRecordCount - 1;
        while (snapshotIndex >= 0 && records[(size_t)snapshotIndex].snapshot == NULL) {
            snapshotIndex--;
        }
        for (i = snapshotIndex + 1; i < activeRecordCount; ++i) {
            const TimelineRecord& record = records[(size_t)i];
            unsigned int c;
            for (c = 0; c < record.changeCount; ++c) {
                const TimelineChange& change = changeAt(record, c);
                if (change.ram) dirtyPages[change.address / TIMELINE_PAGE_SIZE] = true;
            }
        }
        stepCount = activeRecordCount - 1 - snapshotIndex;
        if (stepCount < 0) stepCount = 0;
    }
};

MemoryTimeline::MemoryTimeline(Ondra* machine, int maxRecords, int snapshotInterval)
    : impl(new MemoryTimelineImpl(machine, maxRecords, snapshotInterval)) {
}

MemoryTimeline::~MemoryTimeline() {
    delete impl;
    impl = NULL;
}

void MemoryTimeline::setEnabled(bool enabled) {
    if (impl == NULL) return;
    if (enabled) {
        if (!impl->enabled) {
            impl->enabled = true;
            impl->clearRecords();
            impl->captureInitial();
        } else if (impl->activeRecordCount <= 0) {
            impl->captureInitial();
        }
    } else {
        impl->enabled = false;
        impl->clearRecords();
    }
}

bool MemoryTimeline::isEnabled() const {
    return impl != NULL && impl->enabled;
}

void MemoryTimeline::clear() {
    if (impl != NULL) impl->clearRecords();
}

void MemoryTimeline::captureCurrentState() {
    if (impl == NULL || !impl->enabled) return;
    impl->clearRecords();
    impl->captureInitial();
}

void MemoryTimeline::instructionBoundary() {
    if (impl != NULL) impl->updateBoundary();
}

void MemoryTimeline::addRamChange(uint16_t address, uint8_t oldValue, uint8_t newValue) {
    TimelineChange change;
    if (impl == NULL || !impl->enabled || oldValue == newValue) return;
    change.address = address;
    change.oldValue = oldValue;
    change.newValue = newValue;
    change.ram = 1;
    impl->pending.push_back(change);
    impl->dirtyPages[address / TIMELINE_PAGE_SIZE] = true;
}

void MemoryTimeline::discardPendingChanges() {
    if (impl == NULL) return;
    impl->pending.clear();
    /* The write which triggered a memory breakpoint is not performed.  A rare
     * earlier write in the same aborted instruction cannot safely be undone
     * by the existing Z80 core, so keep the page dirty mask conservative. */
}

int MemoryTimeline::size() const {
    return impl != NULL ? impl->activeRecordCount : 0;
}

int MemoryTimeline::loadedIndex() const {
    return impl != NULL ? impl->selectedIndex : -1;
}

bool MemoryTimeline::loadIndex(int index) {
    uint64_t resumeSliceTarget;

    if (impl == NULL || !impl->enabled || impl->machine == NULL) return false;
    if (index < 0 || index >= impl->activeRecordCount) return false;

    /*
     * Each C++ timeline record is an instruction boundary inside one
     * CPU_Emulate() slice.  Preserve that slice phase across a rewind.
     * Otherwise the first timer callback injects a new frame interrupt at an
     * arbitrary point in the abandoned frame (often immediately after the
     * debugger's one Run step), which is not the state that originally
     * followed this instruction.
     *
     * A zero target is valid for records made outside a normal frame slice
     * (for example a standalone debugger step); those correctly start a fresh
     * frame on Resume.
     */
    resumeSliceTarget = impl->records[(size_t)index].sliceTarget;

    if (!impl->rebuildState(index)) return false;
    if (!impl->machine->restoreTimelineState(&impl->rebuild[0],
                                             (int)impl->rebuild.size())) return false;
    impl->machine->restoreTimelineSliceTarget(resumeSliceTarget);
    impl->selectedIndex = index;
    return true;
}

void MemoryTimeline::truncate(int index) {
    uint8_t header[TIMELINE_HEADER_SIZE];
    TimelineRecord* lastRecord;

    if (impl == NULL || !impl->enabled || impl->activeRecordCount <= 0) return;
    if (index < 0) index = 0;
    if (index >= impl->activeRecordCount) index = impl->activeRecordCount - 1;

    if (impl->selectedIndex != index) {
        if (!loadIndex(index)) return;
    }


    /*
     * Do NOT physically erase the abandoned future here.  On a long history
     * that made Play/Close perform hundreds of thousands of pop_back/delete
     * operations synchronously in the FLTK GUI thread.
     *
     * Make the selected record the logical tail in O(1).  The old physical
     * tail is harmless and will be reused slot-by-slot as the new branch is
     * recorded.  size(), rebuildState() and all debugger navigation only see
     * activeRecordCount, so the abandoned future is immediately unreachable.
     */
    impl->activeRecordCount = index + 1;
    lastRecord = &impl->records[(size_t)index];
    impl->nextChange = lastRecord->firstChange + lastRecord->changeCount;

    impl->pending.clear();
    impl->machine->captureTimelineHeader(header);
    memcpy(impl->previousHeader, header, TIMELINE_HEADER_SIZE);
    impl->havePrevious = true;
    impl->selectedIndex = index;
    impl->rebuildDirtyPageMask();
}


int MemoryTimeline::exportToText(const char* filename, std::string* error) const {
    if (impl == NULL || !impl->enabled) {
        if (error != NULL) *error = "Timeline is not enabled.";
        return -1;
    }
    return impl->exportToText(filename, error);
}
