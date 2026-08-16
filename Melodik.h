#ifndef MELODIK_H
#define MELODIK_H

#ifdef _MSC_VER
#include "vs_stdint.h"
#else
#include <stdint.h>
#endif

/*
 * SN76489 sound core used by the Melodik module.
 *
 * The implementation is derived from Chris White's console-emulator core; its
 * original licence notice is retained in Melodik.cpp. Register, antialiasing,
 * volume and noise-generator behaviour are kept compatible with the Melodik
 * emulation used by JOndra.
 */
class SN76489 {
public:
    SN76489();

    void init(int clockSpeedHz, int sampleRateHz);
    void write(int value);

    /* Generate exactly byteCount signed 8-bit PCM bytes. */
    void update(unsigned char* buffer, int offset, int byteCount);

    int getRegister(int index) const;
    int getLastRegister() const;
    unsigned long getWriteCount() const;
    int getNoiseShiftRegister() const;

private:
    enum {
        SCALE = 4,
        REGISTER_COUNT = 8,
        CHANNEL_COUNT = 4,
        NO_ANTIALIAS = (-2147483647 - 1),
        SHIFT_RESET = 0x8000,
        FEEDBACK_PATTERN = 0x0009
    };

    int clock;
    int clockFrac;
    int reg[REGISTER_COUNT];
    int regLatch;
    int freqCounter[CHANNEL_COUNT];
    int freqPolarity[CHANNEL_COUNT];
    int freqPos[3];
    int noiseFreq;
    int noiseShiftReg;
    int outputChannel[CHANNEL_COUNT];
    unsigned long writeCount;

    static const int PSG_VOLUME[16];
    int generateByte();
};

class Melodik {
public:
    enum {
        SAMPLE_RATE = 44100,
        BUFFER_SAMPLES = SAMPLE_RATE / 50,
        BUFFER_BYTES = 2 * BUFFER_SAMPLES,
        FRAME_TSTATES = 312 * 128,
        DEFAULT_CLOCK_RATE = 2000000,
        DETECT_INDEX = 0x0F,
        DETECT_MASK = 0x20
    };

    SN76489 sndChip;

    explicit Melodik(unsigned char* ioVector);
    ~Melodik();

    void setEnabled(bool value);
    bool isEnabled() const;

    void init(uint64_t initialTStates);
    void deinit();
    void initChip();

    /*
     * Render the old chip state up to currentTStates and only then apply the
     * register write.  This places register changes correctly inside the
     * current 20 ms PCM block.
     */
    void write(int value, uint64_t currentTStates);

    /* Hardware-presence detection in the memory-mapped I/O vector. */
    void setMelodikDetectOn();
    void setMelodikDetectOff();
    bool isMelodikDetected() const;

    void updateSound(uint64_t currentTStates);
    void switchBuffers();
    void setDataReady();

    const unsigned char* getPlayBuffer() const;
    int getBufferByteCount() const;
    bool isDataReady() const;
    void clearDataReady();
    unsigned long getGeneratedBlockCount() const;

private:
    unsigned char* iov;
    bool enabled;
    bool initialized;
    bool melodikDetected;
    uint64_t initTStates;
    uint64_t lastTStates;

    unsigned char bufferA[BUFFER_BYTES];
    unsigned char bufferB[BUFFER_BYTES];
    unsigned char* playBuffer;
    unsigned char* fillBuffer;
    int fillPositionBytes;
    uint64_t frameStartTStates;
    bool dataReady;
    unsigned long generatedBlockCount;

    void clearBuffers();
    void renderToByte(int targetByte);
};

#endif // MELODIK_H
