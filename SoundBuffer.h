#ifndef SOUNDBUFFER_H
#define SOUNDBUFFER_H

// Konstanty podle originálu
//#define SAMPLE_RATE 44100.0
//#define BUFFER_SIZE ((int)(SAMPLE_RATE / 50))  // cca 882
//#define D_STATES2BUFFER_RATIO ((312.0 * 128.0) / (2.0 * BUFFER_SIZE))

#include "SoundSample.h"

class SoundBuffer {
public:
    // Data buffer – 16-bitovı WAV (2*BUFFER_SIZE bajtù)
    char* data;
    int nActiveSample;      // Index aktivního sample
    long lInitTStates;      // Poèáteèní hodnota t-states
    SoundSample** samples;  // Pole ukazatelù na SoundSample (napø. 8 prvkù)

    // Konstruktor: nastaví poèáteèní t-states a ukazatel na pole vzorkù
    SoundBuffer(long lInitTStates, SoundSample** InSamples);

    // Destruktor: uvolní alokovanou pamì
    ~SoundBuffer();

    // Naplní buffer samplem od vypoètené pozice (podle t-states) a do konce bufferu
    void fillWithSample(int nSampleNum, long lTstates);

    // Vyprázdní buffer – resetuje aktivní sample a naplní data nulami
    void emptyBuffer(long InInitTStates);

    // Naplní celı buffer aktivním samplem – verze s kopírováním blokù
    void fillAllWithActiveSampleOld();

    // Naplní celı buffer aktivním samplem – první verze
    void fillAllWithActiveSampleFst();

    // Naplní celı buffer aktivním samplem – aktuální verze (shodná s Fst)
    void fillAllWithActiveSample();

    // Vyprázdní buffer, pøenese aktivní sample z pøedaného bufferu a naplní novı buffer
    void emptyTransferActiveSample(long InInitTStates, SoundBuffer* bfrLast);

    // Getter pro pøístup k datovému bufferu
    char* getData() const { return data; }
};

#endif // SOUNDBUFFER_H
