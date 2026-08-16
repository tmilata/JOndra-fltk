/*
    SN76489 sound core

    Copyright (c) 2002-2008 Chris White
    All rights reserved.

    Redistribution and use of this code or any derivative works are permitted
    provided that the following conditions are met:

    * Redistributions may not be sold, nor may they be used in a commercial
      product or activity.

    * Redistributions that are modified from the original source must include
      the complete source code, including the source code for all components
      used by a binary built from the modified sources. However, as a special
      exception, the source code distributed need not include anything that is
      normally distributed (in either source or binary form) with the major
      components (compiler, kernel, and so on) of the operating system on which
      the executable runs, unless that component itself accompanies the
      executable.

    * Redistributions must reproduce the above copyright notice, this list of
      conditions and the following disclaimer in the documentation and/or other
      materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

    Register, antialiasing, volume and noise behaviour are preserved by this
    implementation.
*/

#include "Melodik.h"
#include <string.h>

const int SN76489::PSG_VOLUME[16] = {
    25, 20, 16, 13, 10, 8, 6, 5,
     4,  3,  3,  2,  2, 1, 1, 0
};

SN76489::SN76489()
    : clock(0), clockFrac(0), regLatch(0), noiseFreq(0x10),
      noiseShiftReg(SHIFT_RESET), writeCount(0)
{
    init(2000000, 44100);
}

void SN76489::init(int clockSpeedHz, int sampleRateHz)
{
    int i;

    if (sampleRateHz <= 0) {
        sampleRateHz = 44100;
    }
    if (clockSpeedHz <= 0) {
        clockSpeedHz = 2000000;
    }

    /* Master clock divided by 16, using the fixed-point scale above. */
    clock = (clockSpeedHz << SCALE) / 16 / sampleRateHz;

    regLatch = 0;
    clockFrac = 0;
    noiseShiftReg = SHIFT_RESET;
    noiseFreq = 0x10;
    writeCount = 0;

    for (i = 0; i < CHANNEL_COUNT; ++i) {
        reg[i << 1] = 1;
        reg[(i << 1) + 1] = 0x0F;
        freqCounter[i] = 0;
        freqPolarity[i] = 1;
        outputChannel[i] = 0;

        if (i != 3) {
            freqPos[i] = NO_ANTIALIAS;
        }
    }
}

void SN76489::write(int value)
{
    value &= 0xFF;
    ++writeCount;

    if ((value & 0x80) != 0) {
        regLatch = (value >> 4) & 7;
        reg[regLatch] = (reg[regLatch] & 0x3F0) | (value & 0x0F);
    } else {
        if (regLatch == 0 || regLatch == 2 || regLatch == 4) {
            reg[regLatch] = (reg[regLatch] & 0x0F) |
                            ((value & 0x3F) << 4);
        } else {
            reg[regLatch] = value & 0x0F;
        }
    }

    switch (regLatch) {
    case 0:
    case 2:
    case 4:
        if (reg[regLatch] == 0) {
            reg[regLatch] = 1;
        }
        break;

    case 6:
        noiseFreq = 0x10 << (reg[6] & 3);
        noiseShiftReg = SHIFT_RESET;
        break;
    }
}

int SN76489::generateByte()
{
    int sample;
    int output;
    int clockCycles;
    int clockCyclesScaled;
    int i;

    for (i = 0; i < 3; ++i) {
        if (freqPos[i] != NO_ANTIALIAS) {
            outputChannel[i] =
                (PSG_VOLUME[reg[(i << 1) + 1]] * freqPos[i]) >> SCALE;
        } else {
            outputChannel[i] =
                PSG_VOLUME[reg[(i << 1) + 1]] * freqPolarity[i];
        }
    }

    outputChannel[3] =
        (PSG_VOLUME[reg[7]] * (noiseShiftReg & 1)) << 1;

    output = outputChannel[0] + outputChannel[1] +
             outputChannel[2] + outputChannel[3];

    if (output > 0x7F) {
        output = 0x7F;
    } else if (output < -0x80) {
        output = -0x80;
    }

    clockFrac += clock;
    clockCycles = clockFrac >> SCALE;
    clockCyclesScaled = clockCycles << SCALE;
    clockFrac -= clockCyclesScaled;

    freqCounter[0] -= clockCycles;
    freqCounter[1] -= clockCycles;
    freqCounter[2] -= clockCycles;

    if (noiseFreq == 0x80) {
        freqCounter[3] = freqCounter[2];
    } else {
        freqCounter[3] -= clockCycles;
    }

    for (i = 0; i < 3; ++i) {
        int counter = freqCounter[i];

        if (counter <= 0) {
            int tone = reg[i << 1];

            if (tone > 6) {
                int denominator = clockCyclesScaled + clockFrac;

                /* The normal 2 MHz / 44.1 kHz path always has denominator > 0. */
                if (denominator != 0) {
                    freqPos[i] =
                        ((clockCyclesScaled - clockFrac +
                          (2 << SCALE) * counter) *
                         (1 << SCALE) * freqPolarity[i]) / denominator;
                } else {
                    freqPos[i] = NO_ANTIALIAS;
                }

                freqPolarity[i] = -freqPolarity[i];
            } else {
                freqPolarity[i] = 1;
                freqPos[i] = NO_ANTIALIAS;
            }

            freqCounter[i] += tone * (clockCycles / tone + 1);
        } else {
            freqPos[i] = NO_ANTIALIAS;
        }
    }

    if (freqCounter[3] <= 0) {
        freqPolarity[3] = -freqPolarity[3];

        if (noiseFreq != 0x80) {
            freqCounter[3] +=
                noiseFreq * (clockCycles / noiseFreq + 1);
        }

        if (freqPolarity[3] == 1) {
            int feedback;

            if ((reg[6] & 0x04) != 0) {
                feedback = ((noiseShiftReg & FEEDBACK_PATTERN) != 0 &&
                            (((noiseShiftReg & FEEDBACK_PATTERN) ^
                              FEEDBACK_PATTERN) != 0)) ? 1 : 0;
            } else {
                feedback = noiseShiftReg & 1;
            }

            noiseShiftReg = (noiseShiftReg >> 1) | (feedback << 15);
        }
    }

    sample = output;
    return sample;
}

void SN76489::update(unsigned char* buffer, int offset, int byteCount)
{
    int sample;

    if (buffer == NULL || offset < 0 || byteCount <= 0) {
        return;
    }

    for (sample = 0; sample < byteCount; ++sample) {
        int output = generateByte();
        buffer[offset + sample] = (unsigned char)(output & 0xFF);
    }
}

int SN76489::getRegister(int index) const
{
    if (index < 0 || index >= REGISTER_COUNT) {
        return 0;
    }
    return reg[index];
}

int SN76489::getLastRegister() const
{
    return regLatch;
}

unsigned long SN76489::getWriteCount() const
{
    return writeCount;
}

int SN76489::getNoiseShiftRegister() const
{
    return noiseShiftReg;
}

Melodik::Melodik(unsigned char* ioVector)
    : iov(ioVector), enabled(false), initialized(false),
      melodikDetected(false), initTStates(0), lastTStates(0),
      playBuffer(bufferA), fillBuffer(bufferB), fillPositionBytes(0),
      frameStartTStates(0), dataReady(false), generatedBlockCount(0)
{
    clearBuffers();
}

Melodik::~Melodik()
{
    deinit();
}

void Melodik::clearBuffers()
{
    memset(bufferA, 0, sizeof(bufferA));
    memset(bufferB, 0, sizeof(bufferB));
}

void Melodik::setEnabled(bool value)
{
    enabled = value;

    if (!enabled) {
        dataReady = false;
        setMelodikDetectOff();
    } else if (initialized) {
        setMelodikDetectOn();
    }
}

bool Melodik::isEnabled() const
{
    return enabled;
}

void Melodik::init(uint64_t initialTStates)
{
    initTStates = initialTStates;
    lastTStates = initialTStates;
    frameStartTStates = initialTStates;
    fillPositionBytes = 0;
    dataReady = false;
    generatedBlockCount = 0;
    initialized = true;

    playBuffer = bufferA;
    fillBuffer = bufferB;
    clearBuffers();
    sndChip.init(DEFAULT_CLOCK_RATE, SAMPLE_RATE);

    if (enabled) {
        setMelodikDetectOn();
    } else {
        setMelodikDetectOff();
    }
}

void Melodik::deinit()
{
    setMelodikDetectOff();
    initialized = false;
    dataReady = false;
    fillPositionBytes = 0;
    clearBuffers();
}

void Melodik::initChip()
{
    sndChip.init(DEFAULT_CLOCK_RATE, SAMPLE_RATE);

    dataReady = false;
    fillPositionBytes = 0;
    frameStartTStates = lastTStates;
    playBuffer = bufferA;
    fillBuffer = bufferB;
    clearBuffers();

    /* Keyboard::Reset() fills the whole I/O vector with 0xFF. */
    if (enabled) {
        setMelodikDetectOn();
    } else {
        setMelodikDetectOff();
    }
}

void Melodik::renderToByte(int targetByte)
{
    int count;

    if (targetByte < 0) {
        targetByte = 0;
    }
    if (targetByte > BUFFER_BYTES) {
        targetByte = BUFFER_BYTES;
    }

    /* Keep 16-bit little-endian output frames intact. */
    targetByte &= ~1;

    if (targetByte <= fillPositionBytes) {
        return;
    }

    count = targetByte - fillPositionBytes;
    sndChip.update(fillBuffer, fillPositionBytes, count);
    fillPositionBytes = targetByte;
}

void Melodik::write(int value, uint64_t currentTStates)
{
    if (!enabled || !initialized) {
        return;
    }

    updateSound(currentTStates);
    sndChip.write(value);
}

void Melodik::setMelodikDetectOn()
{
    if (iov != NULL && enabled) {
        iov[DETECT_INDEX] =
            (unsigned char)(iov[DETECT_INDEX] & (unsigned char)~DETECT_MASK);
        melodikDetected = true;
    } else {
        melodikDetected = false;
    }
}

void Melodik::setMelodikDetectOff()
{
    if (iov != NULL) {
        iov[DETECT_INDEX] =
            (unsigned char)(iov[DETECT_INDEX] | DETECT_MASK);
    }
    melodikDetected = false;
}

bool Melodik::isMelodikDetected() const
{
    if (iov == NULL) {
        return false;
    }
    return (iov[DETECT_INDEX] & DETECT_MASK) == 0;
}

void Melodik::updateSound(uint64_t currentTStates)
{
    uint64_t delta;
    uint64_t targetFrame;

    if (!enabled || !initialized) {
        return;
    }

    if (currentTStates < frameStartTStates) {
        frameStartTStates = currentTStates;
        lastTStates = currentTStates;
        fillPositionBytes = 0;
        memset(fillBuffer, 0, BUFFER_BYTES);
        return;
    }

    delta = currentTStates - frameStartTStates;
    targetFrame = (delta * (uint64_t)BUFFER_SAMPLES) /
                  (uint64_t)FRAME_TSTATES;

    if (targetFrame > (uint64_t)BUFFER_SAMPLES) {
        targetFrame = BUFFER_SAMPLES;
    }

    renderToByte((int)targetFrame * 2);
    lastTStates = currentTStates;
}

void Melodik::switchBuffers()
{
    unsigned char* temporary;

    if (!enabled || !initialized) {
        return;
    }

    renderToByte(BUFFER_BYTES);

    temporary = playBuffer;
    playBuffer = fillBuffer;
    fillBuffer = temporary;

    memset(fillBuffer, 0, BUFFER_BYTES);
    fillPositionBytes = 0;
    frameStartTStates = lastTStates;
    dataReady = false;
    ++generatedBlockCount;
}

void Melodik::setDataReady()
{
    if (enabled && initialized) {
        dataReady = true;
    }
}

const unsigned char* Melodik::getPlayBuffer() const
{
    return playBuffer;
}

int Melodik::getBufferByteCount() const
{
    return BUFFER_BYTES;
}

bool Melodik::isDataReady() const
{
    return dataReady;
}

void Melodik::clearDataReady()
{
    dataReady = false;
}

unsigned long Melodik::getGeneratedBlockCount() const
{
    return generatedBlockCount;
}
