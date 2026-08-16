#include "Tape.h"
#include "Ondra.h"
#include "Memory.h"
#include "TapFile.h"
#include "TapeSignalProc.h"
#include "Debug.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Small CSW reader                                                         */
/* ------------------------------------------------------------------------- */

static unsigned long tapeReadLE32(FILE* f, bool* ok) {
    int b0 = fgetc(f);
    int b1 = fgetc(f);
    int b2 = fgetc(f);
    int b3 = fgetc(f);
    if (b0 == EOF || b1 == EOF || b2 == EOF || b3 == EOF) {
        if (ok) *ok = false;
        return 0;
    }
    if (ok) *ok = true;
    return ((unsigned long)(b0 & 0xff)) |
           ((unsigned long)(b1 & 0xff) << 8) |
           ((unsigned long)(b2 & 0xff) << 16) |
           ((unsigned long)(b3 & 0xff) << 24);
}

static bool tapeWriteLE32(FILE* f, unsigned long value) {
    if (!f) return false;
    if (fputc((int)(value & 0xff), f) == EOF) return false;
    if (fputc((int)((value >> 8) & 0xff), f) == EOF) return false;
    if (fputc((int)((value >> 16) & 0xff), f) == EOF) return false;
    if (fputc((int)((value >> 24) & 0xff), f) == EOF) return false;
    return true;
}

class CswTapeReader {
public:
    CswTapeReader(const char* path)
        : f(0), valid(false), finished(false), sampleRate(0), runLength(0), level(false)
    {
        static const char signature[] = "Compressed Square Wave";
        unsigned char tmp[32];
        bool ok = false;

        f = fopen(path, "rb");
        if (!f) return;

        if (fread(tmp, 1, sizeof(signature) - 1, f) != sizeof(signature) - 1 ||
            memcmp(tmp, signature, sizeof(signature) - 1) != 0) {
            closeFile();
            return;
        }

        int terminator = fgetc(f);
        int major = fgetc(f);
        int minor = fgetc(f);
        (void)minor;
        if (terminator != 0x1a || major < 2) {
            closeFile();
            return;
        }

        sampleRate = (long)tapeReadLE32(f, &ok);
        if (!ok || sampleRate <= 0) {
            closeFile();
            return;
        }

        // Number of pulses is present in the CSW header but is not needed by the decoder.
        tapeReadLE32(f, &ok);
        if (!ok) {
            closeFile();
            return;
        }

        int compression = fgetc(f);
        int flags = fgetc(f);
        int extensionLength = fgetc(f);
        (void)flags; // Polarity flags are intentionally ignored.
        if (compression != 1 || extensionLength == EOF) {
            closeFile();
            return;
        }

        // 16-byte application name.
        if (fread(tmp, 1, 16, f) != 16) {
            closeFile();
            return;
        }

        if (extensionLength > 0 && fseek(f, extensionLength, SEEK_CUR) != 0) {
            closeFile();
            return;
        }

        valid = true;
    }

    ~CswTapeReader() {
        closeFile();
    }

    bool isValid() const { return valid; }
    bool isFinished() const { return finished; }
    long getSampleRate() const { return sampleRate; }

    bool readSample() {
        if (!valid || finished) return level;

        while (runLength == 0) {
            int b = fgetc(f);
            if (b == EOF) {
                finished = true;
                return level;
            }

            level = !level;
            if ((b & 0xff) == 0) {
                bool ok = false;
                runLength = tapeReadLE32(f, &ok);
                if (!ok) {
                    finished = true;
                    runLength = 0;
                    return level;
                }
            } else {
                runLength = (unsigned long)(b & 0xff);
            }
        }

        --runLength;
        return level;
    }

private:
    void closeFile() {
        if (f) {
            fclose(f);
            f = 0;
        }
    }

    FILE* f;
    bool valid;
    bool finished;
    long sampleRate;
    unsigned long runLength;
    bool level;
};

/* ------------------------------------------------------------------------- */
/* Small CSW writer                                                         */
/* ------------------------------------------------------------------------- */

class CswTapeWriter {
public:
    CswTapeWriter(const char* path)
        : f(0), valid(false), sampleRate(22050), pulseCount(0),
          runLength(0), haveSample(false), lastInput(false),
          checkpointed(false), checkpointOffset(0), checkpointRunLength(0)
    {
        static const char signature[] = "Compressed Square Wave";
        static const unsigned char app[16] = {
            'J','O','n','d','r','a',' ','e','m','u','l','a','t','o','r',0
        };

        /* "wb+" intentionally truncates an existing file. The GUI asks the
           user for overwrite confirmation before this constructor is called. */
        f = fopen(path, "wb+");
        if (!f) return;

        if (fwrite(signature, 1, sizeof(signature) - 1, f) != sizeof(signature) - 1 ||
            fputc(0x1a, f) == EOF ||
            fputc(2, f) == EOF ||
            fputc(0, f) == EOF ||
            !tapeWriteLE32(f, (unsigned long)sampleRate) ||
            !tapeWriteLE32(f, 0) ||
            fputc(1, f) == EOF ||
            /* The encoded waveform is normalized so that its first pulse is
               logical HIGH. Absolute polarity is irrelevant to Ondra's tape
               decoder, while setting this flag makes the CSW header correct. */
            fputc(1, f) == EOF ||
            fputc(0, f) == EOF ||
            fwrite(app, 1, sizeof(app), f) != sizeof(app)) {
            closeFile(false);
            return;
        }

        if (fflush(f) != 0) {
            closeFile(false);
            return;
        }

        valid = true;
    }

    ~CswTapeWriter() {
        closeFile(true);
    }

    bool isValid() const { return valid && f != 0; }
    long getSampleRate() const { return sampleRate; }

    bool writeSample(bool sample) {
        if (!isValid()) return false;

        /* CSW stores pulse lengths, not individual amplitudes.  Preserve the
           transition timing of TAPE OUT; the first observed level is simply
           normalized to logical HIGH as advertised in the header flags. */
        if (!haveSample) {
            haveSample = true;
            lastInput = sample;
            runLength = 1;
            return true;
        }

        /* checkpoint() writes the still-open final pulse so the CSW is valid
           immediately when the tape motor stops.  If recording later resumes
           at the same input level, seek back and extend that pulse instead of
           inventing a false transition. */
        if (checkpointed) {
            if (sample == lastInput) {
                if (fseek(f, checkpointOffset, SEEK_SET) != 0) {
                    valid = false;
                    return false;
                }
                if (pulseCount != 0) --pulseCount;
                runLength = checkpointRunLength + 1;
            } else {
                runLength = 1;
                lastInput = sample;
            }
            checkpointed = false;
            return true;
        }

        if (sample == lastInput) {
            ++runLength;
            return true;
        }

        if (!writeRun(runLength)) {
            valid = false;
            return false;
        }

        lastInput = sample;
        runLength = 1;
        return true;
    }

    bool checkpoint() {
        long endPos;
        if (!isValid()) return false;

        if (!checkpointed && haveSample && runLength != 0) {
            checkpointOffset = ftell(f);
            if (checkpointOffset < 0) {
                valid = false;
                return false;
            }
            checkpointRunLength = runLength;
            if (!writeRun(runLength)) {
                valid = false;
                return false;
            }
            runLength = 0;
            checkpointed = true;
        }

        endPos = ftell(f);
        if (endPos < 0 || fseek(f, 0x1d, SEEK_SET) != 0 ||
            !tapeWriteLE32(f, pulseCount) ||
            fseek(f, endPos, SEEK_SET) != 0 ||
            fflush(f) != 0) {
            valid = false;
            return false;
        }
        return true;
    }

private:
    bool writeRun(unsigned long length) {
        if (!f || length == 0) return false;

        if (length <= 0xffUL) {
            if (fputc((int)(length & 0xff), f) == EOF) return false;
        } else {
            if (fputc(0, f) == EOF) return false;
            if (!tapeWriteLE32(f, length)) return false;
        }

        ++pulseCount;
        return true;
    }

    void closeFile(bool finalize) {
        if (!f) return;

        if (finalize) {
            if (!checkpointed && haveSample && runLength != 0) {
                if (!writeRun(runLength)) valid = false;
                runLength = 0;
            }

            /* CSW v2 offset 0x1d contains the number of decompressed pulses,
               i.e. the number of RLE runs, not the total number of samples. */
            {
                long endPos = ftell(f);
                if (endPos < 0 || fseek(f, 0x1d, SEEK_SET) != 0 ||
                    !tapeWriteLE32(f, pulseCount) ||
                    fseek(f, endPos, SEEK_SET) != 0) {
                    valid = false;
                }
            }
            fflush(f);
        }

        fclose(f);
        f = 0;
    }

    FILE* f;
    bool valid;
    long sampleRate;
    unsigned long pulseCount;
    unsigned long runLength;
    bool haveSample;
    bool lastInput;
    bool checkpointed;
    long checkpointOffset;
    unsigned long checkpointRunLength;
};

/* ------------------------------------------------------------------------- */
/* Small PCM WAV reader                                                     */
/* ------------------------------------------------------------------------- */

class WavTapeReader {
public:
    WavTapeReader(const char* path)
        : f(0), valid(false), finished(false), sampleRate(0), channels(0),
          bitsPerSample(0), blockAlign(0), dataRemaining(0)
    {
        unsigned char header[12];
        bool haveFormat = false;

        f = fopen(path, "rb");
        if (!f) return;

        if (fread(header, 1, 12, f) != 12 ||
            memcmp(header, "RIFF", 4) != 0 ||
            memcmp(header + 8, "WAVE", 4) != 0) {
            closeFile();
            return;
        }

        while (!valid) {
            unsigned char chunkHeader[8];
            if (fread(chunkHeader, 1, 8, f) != 8) {
                closeFile();
                return;
            }

            unsigned long chunkSize =
                ((unsigned long)chunkHeader[4]) |
                ((unsigned long)chunkHeader[5] << 8) |
                ((unsigned long)chunkHeader[6] << 16) |
                ((unsigned long)chunkHeader[7] << 24);

            if (memcmp(chunkHeader, "fmt ", 4) == 0) {
                if (chunkSize < 16) {
                    closeFile();
                    return;
                }

                unsigned char fmt[16];
                if (fread(fmt, 1, 16, f) != 16) {
                    closeFile();
                    return;
                }

                int formatTag = (int)fmt[0] | ((int)fmt[1] << 8);
                channels = (int)fmt[2] | ((int)fmt[3] << 8);
                sampleRate = (long)((unsigned long)fmt[4] |
                                    ((unsigned long)fmt[5] << 8) |
                                    ((unsigned long)fmt[6] << 16) |
                                    ((unsigned long)fmt[7] << 24));
                blockAlign = (int)fmt[12] | ((int)fmt[13] << 8);
                bitsPerSample = (int)fmt[14] | ((int)fmt[15] << 8);

                if (formatTag != 1 || sampleRate <= 0 ||
                    (channels != 1 && channels != 2) ||
                    (bitsPerSample != 8 && bitsPerSample != 16)) {
                    closeFile();
                    return;
                }

                int expectedAlign = channels * (bitsPerSample / 8);
                if (blockAlign != expectedAlign) {
                    closeFile();
                    return;
                }

                unsigned long rest = chunkSize - 16;
                if (rest > 0 && fseek(f, (long)rest, SEEK_CUR) != 0) {
                    closeFile();
                    return;
                }
                if ((chunkSize & 1) != 0 && fseek(f, 1, SEEK_CUR) != 0) {
                    closeFile();
                    return;
                }
                haveFormat = true;
            }
            else if (memcmp(chunkHeader, "data", 4) == 0) {
                if (!haveFormat) {
                    closeFile();
                    return;
                }
                dataRemaining = chunkSize;
                valid = true;
            }
            else {
                unsigned long skip = chunkSize + (chunkSize & 1);
                if (skip > 0 && fseek(f, (long)skip, SEEK_CUR) != 0) {
                    closeFile();
                    return;
                }
            }
        }
    }

    ~WavTapeReader() {
        closeFile();
    }

    bool isValid() const { return valid; }
    bool isFinished() const { return finished; }
    long getSampleRate() const { return sampleRate; }

    bool readFrame(int* sampleOut) {
        if (!valid || finished || !sampleOut) return false;
        if (dataRemaining < (unsigned long)blockAlign) {
            finished = true;
            return false;
        }

        int sum = 0;
        int c;
        for (c = 0; c < channels; ++c) {
            int sample = 0;
            if (bitsPerSample == 8) {
                int b = fgetc(f);
                if (b == EOF) {
                    finished = true;
                    return false;
                }
                sample = b & 0xff;
            } else {
                int lo = fgetc(f);
                int hi = fgetc(f);
                if (lo == EOF || hi == EOF) {
                    finished = true;
                    return false;
                }
                unsigned int raw = (unsigned int)(lo & 0xff) |
                                   ((unsigned int)(hi & 0xff) << 8);
                short signedSample = (short)raw;
                // Normalize signed 16-bit PCM to the same 0..255 range as 8-bit PCM.
                sample = ((int)signedSample + 32768) >> 8;
            }
            sum += sample;
        }

        dataRemaining -= (unsigned long)blockAlign;
        *sampleOut = (sum + channels / 2) / channels;
        return true;
    }

private:
    void closeFile() {
        if (f) {
            fclose(f);
            f = 0;
        }
    }

    FILE* f;
    bool valid;
    bool finished;
    long sampleRate;
    int channels;
    int bitsPerSample;
    int blockAlign;
    unsigned long dataRemaining;
};

/* ------------------------------------------------------------------------- */
/* Tape                                                                      */
/* ------------------------------------------------------------------------- */

Tape::Tape(Ondra* machine)
    : m(machine),
      tsp(new TapeSignalProc(256)),
      tbuff0(0),
      lcsw(0), scsw(0), lwav(0), ltap(0),
      lrate(0), srate(0),
      lst(CLOSE), sst(CLOSE),
      record(false)
{
}

Tape::~Tape() {
    closeLoadTape();
    closeSaveTape();
    delete tsp;
    tsp = 0;
}

void Tape::closeLoadTape() {
    // Any accelerated LOAD must leave the emulator at its normal speed.
    if (m && m->getClockSpeed() != 20) {
        m->setClockSpeed(20);
    }

    if (lcsw) {
        delete lcsw;
        lcsw = 0;
    }
    if (lwav) {
        delete lwav;
        lwav = 0;
    }
    if (ltap) {
        delete ltap;
        ltap = 0;
    }
    lst = CLOSE;
}

void Tape::closeSaveTape() {
    if (scsw) {
        delete scsw;
        scsw = 0;
    }
    sst = CLOSE;
}

int Tape::rateToTstates(long sampleRateIn) const {
    if (sampleRateIn <= 0) return 512;
    long value = 2000000L / sampleRateIn;
    if (value < 1) value = 1;
    if (value > 0x7fffffffL) value = 0x7fffffffL;
    return (int)value;
}

void Tape::openLoadTape(const std::string &canonicalPath) {
    closeLoadTape();
    bitDump.erase();

    // Detect the format in the order CSW, TAP, WAV.
    lcsw = new CswTapeReader(canonicalPath.c_str());
    if (lcsw && lcsw->isValid()) {
        lrate = rateToTstates(lcsw->getSampleRate());
        lst = CSW;
        return;
    }
    delete lcsw;
    lcsw = 0;

    // Then TAP.
    ltap = TapFile::openTapFile(canonicalPath.c_str());
    if (ltap) {
        lrate = 4;
        lst = TAP;
        return;
    }

    // Finally WAV.
    lwav = new WavTapeReader(canonicalPath.c_str());
    if (lwav && lwav->isValid()) {
        lrate = rateToTstates(lwav->getSampleRate());
        lst = WAV;

        // Start each WAV with a fresh adaptive comparator.
        delete tsp;
        tsp = new TapeSignalProc(256);
        return;
    }
    delete lwav;
    lwav = 0;
    lst = CLOSE;
}

bool Tape::openSaveTape(const std::string &canonicalPath) {
    closeSaveTape();

    scsw = new CswTapeWriter(canonicalPath.c_str());
    if (!scsw || !scsw->isValid()) {
        delete scsw;
        scsw = 0;
        sst = CLOSE;
        return false;
    }

    srate = rateToTstates(scsw->getSampleRate());
    sst = CSW;
    return true;
}

void Tape::tapeStart() {
    /*
     * TAP, CSW and WAV are all driven by emulated T-states.  Therefore the
     * whole machine can be accelerated exactly the same way for every LOAD
     * format without changing tape timing.
     *
     * This function is called from the emulation timer thread via outPort().
     * Do not stop/restart the timer here; only change its interval.
     */
    if (!record && (lst == TAP || lst == CSW || lst == WAV)) {
        m->setClockSpeed(2);
    }

    m->clk->addClockTimeoutListener(this);
    m->clk->setTimeout(512);
}

void Tape::tapeStop() {
    m->clk->removeClockTimeoutListener(this);

    /* Make a recorded CSW immediately usable when the ROM stops the motor.
       The writer can still continue later without adding a false transition. */
    if (scsw && scsw->isValid()) {
        scsw->checkpoint();
    }

    // If the ROM stops the tape motor before EOF, leave fast LOAD mode too.
    if (m->getClockSpeed() != 20) {
        m->setClockSpeed(20);
    }
}

void Tape::setTapeMode(bool rec) {
    record = rec;

    /* Recording is deliberately realtime. If REC is selected while a fast
       LOAD is active, immediately return the machine to normal speed. */
    if (record && m && m->getClockSpeed() != 20) {
        m->setClockSpeed(20);
    }
}

void Tape::saveDump() {
    CDebug::debug("%s", bitDump.c_str());
}

void Tape::clockTimeout() {
    if (record) {
        if (sst == CSW && scsw && scsw->isValid()) {
            m->clk->setTimeout(srate);
            scsw->writeSample((m->getPortA3() & 0x08) != 0);
        } else {
            m->clk->setTimeout(512);
        }
        return;
    }

    switch (lst) {
        case CSW:
            if (!lcsw || lcsw->isFinished()) {
                if (m->getClockSpeed() != 20) {
                    m->setClockSpeed(20);
                }
                lst = CLOSE;
                m->mem->setTapeIn(false);
                m->clk->setTimeout(512);
                break;
            }
            m->clk->setTimeout(lrate);
            m->mem->setTapeIn(lcsw->readSample());
            if (lcsw->isFinished()) {
                if (m->getClockSpeed() != 20) {
                    m->setClockSpeed(20);
                }
                lst = CLOSE;
            }
            break;

        case TAP:
            if (!ltap) {
                lst = CLOSE;
                m->clk->setTimeout(512);
                break;
            }
            if (ltap->bFinished) {
                if (m->getClockSpeed() != 20) {
                    m->setClockSpeed(20);
                }
                lst = CLOSE;
                m->clk->setTimeout(512);
            } else {
                m->clk->setTimeout(lrate);
                m->mem->setTapeIn(ltap->generateFrame() != 0);
            }
            break;

        case WAV:
            if (!lwav || lwav->isFinished()) {
                if (m->getClockSpeed() != 20) {
                    m->setClockSpeed(20);
                }
                lst = CLOSE;
                m->mem->setTapeIn(false);
                m->clk->setTimeout(512);
                break;
            }
            m->clk->setTimeout(lrate);
            if (lwav->readFrame(&tbuff0)) {
                m->mem->setTapeIn(tsp->addSample(tbuff0));
            } else {
                if (m->getClockSpeed() != 20) {
                    m->setClockSpeed(20);
                }
                lst = CLOSE;
                m->mem->setTapeIn(false);
                m->clk->setTimeout(512);
            }
            break;

        default:
            m->clk->setTimeout(512);
            break;
    }
}

void Tape::closeCleanup() {
    closeLoadTape();
    closeSaveTape();
}
