#include "SoundBuffer.h"
#include "Sound.h"
#include <cstring>  // memset, memcpy

SoundBuffer::SoundBuffer(long lInitTStates, SoundSample** InSamples)
    : nActiveSample(0), lInitTStates(lInitTStates), samples(InSamples)
{
    // Alokace bufferu o velikosti 2 * BUFFER_SIZE bajtù
    data = new char[2 * Sound::BUFFER_SIZE];
    memset(data, 0, 2 * Sound::BUFFER_SIZE);
}

SoundBuffer::~SoundBuffer() {
    if (data) {
        delete[] data;
    }
}

void SoundBuffer::fillWithSample(int nSampleNum, long lTstates) {
    if (nSampleNum < 0 || nSampleNum >= 8 || samples == NULL ||
        samples[nSampleNum] == NULL) {
        return;
    }
    int nLastSample = nActiveSample;
    if (nActiveSample != nSampleNum) {
        // Resetujeme pozici pøedchozího sample
        samples[nActiveSample]->resetPosition();
        nActiveSample = nSampleNum;
    }
    if (nLastSample != nSampleNum) {
        // Vypoèítáme poèáteèní pozici do bufferu podle rozdílu t-states
        long lStart = (long)(((double)(lTstates - lInitTStates)) / Sound::dStates2BufferRatio);
        if (lStart % 2 != 0) {
            lStart--; // Zajistíme, že pozice bude sudá
        }
        if ((lStart >= 0) && (lStart < 2 * Sound::BUFFER_SIZE)) {
            for (int i = (int)lStart; i < 2 * Sound::BUFFER_SIZE; i++) {
                data[i] = samples[nSampleNum]->getNextByte();
            }
        }
    }
}

void SoundBuffer::emptyBuffer(long InInitTStates) {
    lInitTStates = InInitTStates;
    nActiveSample = 0;
    memset(data, 0, 2 * Sound::BUFFER_SIZE);
}


void SoundBuffer::fillAllWithActiveSample() {
    // Pokracovani aktivniho tonu pres hranici bufferu.
    if (samples == NULL || nActiveSample < 0 || nActiveSample >= 8 ||
        samples[nActiveSample] == NULL ||
        samples[nActiveSample]->sample == NULL ||
        samples[nActiveSample]->nLen <= 0) {
        memset(data, 0, 2 * Sound::BUFFER_SIZE);
        nActiveSample = 0;
        return;
    }

    SoundSample* activeSample = samples[nActiveSample];
    char* sampleData = activeSample->sample;
    int sampleLength = activeSample->nLen;
    int pos = activeSample->nPos;
    for (int i = 0; i < 2 * Sound::BUFFER_SIZE; i++) {
        data[i] = sampleData[pos];
        pos = (pos + 1) % sampleLength;
    }
    activeSample->nPos = pos;
}

void SoundBuffer::emptyTransferActiveSample(long InInitTStates, SoundBuffer* bfrLast) {
    emptyBuffer(InInitTStates);
    if (bfrLast != NULL) {
        nActiveSample = bfrLast->nActiveSample;
    }
    fillAllWithActiveSample();
}
