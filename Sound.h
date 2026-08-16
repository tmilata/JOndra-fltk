#ifndef SOUND_H
#define SOUND_H

#include "SoundBuffer.h"
#include "SoundSample.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <alsa/asoundlib.h>
#include <pthread.h>
#endif

class Sound {
public:
    static double sampleRate;
    static int BUFFER_SIZE;
    static double dStates2BufferRatio;

    SoundBuffer* playBuffer;
    SoundBuffer* fillBuffer;
    SoundSample* samples[8];

#ifdef _WIN32
    HWAVEOUT hWaveOut;

    enum { WAVE_BUFFER_COUNT = 4 };
    WAVEHDR waveHeaders[WAVE_BUFFER_COUNT];
    char* waveData[WAVE_BUFFER_COUNT];
    bool waveBufferQueued[WAVE_BUFFER_COUNT];
    int waveWriteIndex;
    bool waveHeadersPrepared;
#else
    snd_pcm_t* pcmHandle;
    unsigned int pcmRate;
    snd_pcm_uframes_t pcmPeriodFrames;
    snd_pcm_uframes_t pcmBufferFrames;

    // ALSA writes run in a separate thread. Blocking snd_pcm_writei() must
    // never lengthen the emulator's 20 ms timer cycle.
    enum { PCM_QUEUE_COUNT = 16 };
    pthread_t pcmThread;
    pthread_mutex_t pcmMutex;
    pthread_cond_t pcmCond;
    bool pcmThreadCreated;
    volatile bool pcmThreadRunning;
    char* pcmQueueData[PCM_QUEUE_COUNT];
    int pcmQueueLength[PCM_QUEUE_COUNT];
    int pcmQueueRead;
    int pcmQueueWrite;
    int pcmQueueCount;
    unsigned long pcmXrunCount;
    unsigned long pcmQueueDropCount;

    static void* pcmThreadProc(void* arg);
    void pcmThreadLoop();
    bool pcmWriteDirect(const char* data, int length);
#endif

    int FULL_BUFFER_SIZE;
    int limit5ms;
    char* silent;
    char* mixBuffer;
    bool bFirstFill;

    /*
     * bEnabled controls only the built-in Ondra speaker source.
     * bOutputEnabled controls the shared waveOut/ALSA device.  The device
     * stays active while either Sound or Melodik is enabled.
     */
    bool bEnabled;
    bool bOutputEnabled;

    Sound();
    ~Sound();

    void init();
    void deinit();
    void openAudio();
    void closeAudio();
    void setEnabled(bool bVal);
    bool isEnabled();
    void setOutputEnabled(bool bVal);
    bool isOutputEnabled();
    void resetSource(long nInitTStates);
    void switchBuffers(long nInitTStates);
    void startPlaying();
    void setDataReady();
    void setDataReady(const unsigned char* melodikData, int melodikByteCount);
    void fillBufferByZero();
    bool writeWaveBuffer(const char* data, int length);
};

#endif // SOUND_H
