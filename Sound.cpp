#include "Sound.h"
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "Debug.h"
#include "EmbeddedResources.h"

#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
#endif

double Sound::sampleRate = 44100.0;
int Sound::BUFFER_SIZE = (int)(Sound::sampleRate / 50); // 882 samples = 20 ms
// Buffer contains 16-bit mono data, therefore 2 bytes per sample.
double Sound::dStates2BufferRatio = (312.0 * 128.0) / (2.0 * Sound::BUFFER_SIZE);

Sound::Sound()
    : playBuffer(NULL), fillBuffer(NULL),
#ifdef _WIN32
      hWaveOut(NULL), waveWriteIndex(0), waveHeadersPrepared(false),
#else
      pcmHandle(NULL), pcmRate(44100), pcmPeriodFrames(0), pcmBufferFrames(0),
      pcmThreadCreated(false), pcmThreadRunning(false),
      pcmQueueRead(0), pcmQueueWrite(0), pcmQueueCount(0),
      pcmXrunCount(0), pcmQueueDropCount(0),
#endif
      FULL_BUFFER_SIZE(6 * BUFFER_SIZE),
      limit5ms(2 * ((int)sampleRate / 100)),
      silent(NULL), mixBuffer(NULL), bFirstFill(true),
      bEnabled(false), bOutputEnabled(false)
{
    int i;

    // This is 10 ms of 16-bit mono silence (882 bytes at 44.1 kHz).
    // It is queued twice at startup, i.e. 20 ms in total.
    silent = new char[limit5ms];
    memset(silent, 0, limit5ms);

    mixBuffer = new char[2 * BUFFER_SIZE];
    memset(mixBuffer, 0, 2 * BUFFER_SIZE);

    for (i = 0; i < 8; i++) {
        samples[i] = NULL;
    }

#ifdef _WIN32
    memset(waveHeaders, 0, sizeof(waveHeaders));
    for (i = 0; i < WAVE_BUFFER_COUNT; i++) {
        waveData[i] = NULL;
        waveBufferQueued[i] = false;
    }
#else
    pthread_mutex_init(&pcmMutex, NULL);
    pthread_cond_init(&pcmCond, NULL);
    for (i = 0; i < PCM_QUEUE_COUNT; i++) {
        pcmQueueData[i] = new char[2 * BUFFER_SIZE];
        pcmQueueLength[i] = 0;
    }
#endif
}

Sound::~Sound() {
    int i;

    deinit();

    if (silent) {
        delete[] silent;
        silent = NULL;
    }

    if (mixBuffer) {
        delete[] mixBuffer;
        mixBuffer = NULL;
    }

    for (i = 0; i < 8; i++) {
        if (samples[i]) {
            delete samples[i];
            samples[i] = NULL;
        }
    }

#ifndef _WIN32
    for (i = 0; i < PCM_QUEUE_COUNT; i++) {
        if (pcmQueueData[i]) {
            delete[] pcmQueueData[i];
            pcmQueueData[i] = NULL;
        }
    }
    pthread_cond_destroy(&pcmCond);
    pthread_mutex_destroy(&pcmMutex);
#endif
}

void Sound::init() {
    // Samples survive deinit/init cycles. Load them only once.
    if (samples[0] == NULL) {
        char sample0[2] = {0, 0};
        int sampleIndex;
        samples[0] = new SoundSample(sample0, 2);

        for (sampleIndex = 1; sampleIndex < 8; ++sampleIndex) {
            char resourceName[32];
            unsigned long resourceSize = 0;
            EmbeddedResourceType resourceType = EMBEDDED_RESOURCE_BINARY;
            const unsigned char* resourceData;

            sprintf(resourceName, "sound/%d.sample", sampleIndex);
            resourceData = EmbeddedResources::getData(resourceName,
                                                       &resourceSize,
                                                       &resourceType);
            if (resourceData != NULL &&
                resourceType == EMBEDDED_RESOURCE_BINARY &&
                resourceSize > 0 && resourceSize <= 0x7fffffffUL) {
                samples[sampleIndex] = new SoundSample((const char*)resourceData,
                                                       (int)resourceSize);
            }
        }

        // A missing embedded resource must never turn into a NULL dereference later.
        for (sampleIndex = 1; sampleIndex < 8; ++sampleIndex) {
            if (samples[sampleIndex] == NULL ||
                samples[sampleIndex]->sample == NULL ||
                samples[sampleIndex]->nLen <= 0) {
                fprintf(stderr,
                        "JOndra: missing embedded sound/%d.sample; audio disabled.\n",
                        sampleIndex);
                bEnabled = false;
                break;
            }
        }
    }

    if (playBuffer == NULL) {
        playBuffer = new SoundBuffer(0, samples);
    }
    if (fillBuffer == NULL) {
        fillBuffer = new SoundBuffer(0, samples);
    }

    bFirstFill = true;

#ifdef _WIN32
    // waveOut initialization is fast and does not depend on a desktop
    // session audio server, so the legacy Windows path stays synchronous.
    openAudio();
    if (bOutputEnabled) {
        startPlaying();
    }
#else
    // Never open ALSA/PulseAudio from the FLTK GUI thread. On older Ubuntu
    // installations, especially when JOndra is started as root while
    // PulseAudio belongs to the desktop user, snd_pcm_open() can wait for a
    // session server and make the whole window look frozen. The existing
    // audio writer thread now owns both ALSA initialization and playback.
    if (bOutputEnabled) {
        startPlaying();
    }
#endif
}

void Sound::deinit() {
    closeAudio();

    if (playBuffer) {
        delete playBuffer;
        playBuffer = NULL;
    }
    if (fillBuffer) {
        delete fillBuffer;
        fillBuffer = NULL;
    }
}

void Sound::openAudio() {
    if (!bOutputEnabled) {
        return;
    }

#ifdef _WIN32
    WAVEFORMATEX wf;
    MMRESULT res;
    int i;
    int preparedCount;

    if (hWaveOut != NULL) {
        return;
    }

    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = (DWORD)sampleRate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = (WORD)(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize = 0;

    res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
    if (res != MMSYSERR_NOERROR) {
        CDebug::debug("waveOutOpen failed: %u", (unsigned int)res);
        hWaveOut = NULL;
        bOutputEnabled = false;
        return;
    }

    waveWriteIndex = 0;
    waveHeadersPrepared = false;
    preparedCount = 0;
    memset(waveHeaders, 0, sizeof(waveHeaders));

    for (i = 0; i < WAVE_BUFFER_COUNT; i++) {
        waveBufferQueued[i] = false;
        waveData[i] = new char[2 * BUFFER_SIZE];
        memset(waveData[i], 0, 2 * BUFFER_SIZE);

        waveHeaders[i].lpData = waveData[i];
        waveHeaders[i].dwBufferLength = (DWORD)(2 * BUFFER_SIZE);
        waveHeaders[i].dwFlags = 0;
        waveHeaders[i].dwLoops = 0;

        res = waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR) {
            CDebug::debug("waveOutPrepareHeader failed: %u", (unsigned int)res);
            break;
        }
        preparedCount++;
    }

    if (preparedCount != WAVE_BUFFER_COUNT) {
        waveOutReset(hWaveOut);

        for (i = 0; i < preparedCount; i++) {
            waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        }
        for (i = 0; i < WAVE_BUFFER_COUNT; i++) {
            if (waveData[i]) {
                delete[] waveData[i];
                waveData[i] = NULL;
            }
            waveBufferQueued[i] = false;
        }

        waveOutClose(hWaveOut);
        hWaveOut = NULL;
        bOutputEnabled = false;
        return;
    }

    waveHeadersPrepared = true;
#else
    int err;
    int deviceIndex;
    // Desktop sessions normally work best through the default ALSA plugin.
    // When running as root, old Ubuntu/PulseAudio installations often have no
    // usable per-user PulseAudio socket, so try the VirtualBox/physical ALSA
    // device first. All opens are non-blocking and happen in this worker
    // thread, therefore an unavailable server can never freeze FLTK.
    const char* desktopDeviceNames[] = {
        "plug:default",
        "default",
        "pipewire",
        "pulse",
        "plughw:0,0",
        "hw:0,0",
        NULL
    };
    const char* rootDeviceNames[] = {
        "plughw:0,0",
        "hw:0,0",
        "plug:default",
        "default",
        "pulse",
        "pipewire",
        NULL
    };
    const char** deviceNames = (geteuid() == 0)
        ? rootDeviceNames
        : desktopDeviceNames;
    snd_pcm_sw_params_t* swParams;
    snd_pcm_hw_params_t* hwParams;
    unsigned int actualRate;
    int dir;

    if (pcmHandle != NULL) {
        return;
    }

    // Open non-blocking. Once configuration has completed, the handle is
    // switched back to blocking mode for the dedicated writer thread.
    for (deviceIndex = 0; deviceNames[deviceIndex] != NULL; deviceIndex++) {
        if (!pcmThreadRunning) {
            return;
        }

        err = snd_pcm_open(&pcmHandle,
                           deviceNames[deviceIndex],
                           SND_PCM_STREAM_PLAYBACK,
                           SND_PCM_NONBLOCK);
        if (err >= 0) {
            break;
        }
        pcmHandle = NULL;
    }

    if (pcmHandle == NULL) {
        bOutputEnabled = false;
        return;
    }

    // snd_pcm_set_params() also enables software conversion. This is important
    // on current Linux desktops, where the physical/PipeWire graph commonly
    // runs at 48 kHz while JOndra generates its original 44.1 kHz samples.
    err = snd_pcm_set_params(pcmHandle,
                             SND_PCM_FORMAT_S16_LE,
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             1,
                             (unsigned int)sampleRate,
                             1,        // allow software resampling
                             120000); // requested latency: 120 ms
    if (err < 0) {
        snd_pcm_close(pcmHandle);
        pcmHandle = NULL;
        bOutputEnabled = false;
        return;
    }

    // Query the actual parameters selected by ALSA/plug first. ALSA is free
    // to choose a period different from JOndra's 882-frame generation block;
    // arbitrary write sizes are valid and do not need to match the period.
    pcmRate = (unsigned int)sampleRate;
    pcmPeriodFrames = (snd_pcm_uframes_t)BUFFER_SIZE;
    pcmBufferFrames = (snd_pcm_uframes_t)(BUFFER_SIZE * 6);

    snd_pcm_hw_params_alloca(&hwParams);
    if (snd_pcm_hw_params_current(pcmHandle, hwParams) >= 0) {
        actualRate = pcmRate;
        dir = 0;
        if (snd_pcm_hw_params_get_rate(hwParams, &actualRate, &dir) >= 0) {
            pcmRate = actualRate;
        }
        snd_pcm_hw_params_get_period_size(hwParams, &pcmPeriodFrames, &dir);
        snd_pcm_hw_params_get_buffer_size(hwParams, &pcmBufferFrames);
    }

    // Do not start after the first 20 ms. That left almost no reserve and an
    // occasional longer FLTK/emulation callback produced an ALSA XRUN. Wait
    // until 60 ms is queued. The first callback provides 20 ms of startup
    // silence plus 20 ms of generated audio; the next callback completes the
    // 60 ms pre-roll and starts playback with a useful safety margin.
    {
        snd_pcm_uframes_t startThreshold =
            (snd_pcm_uframes_t)(3 * BUFFER_SIZE); // 60 ms

        if (pcmBufferFrames > pcmPeriodFrames &&
            startThreshold > pcmBufferFrames - pcmPeriodFrames) {
            startThreshold = pcmBufferFrames - pcmPeriodFrames;
        }
        if (startThreshold < (snd_pcm_uframes_t)BUFFER_SIZE) {
            startThreshold = (snd_pcm_uframes_t)BUFFER_SIZE;
        }

        snd_pcm_sw_params_alloca(&swParams);
        err = snd_pcm_sw_params_current(pcmHandle, swParams);
        if (err >= 0) {
            err = snd_pcm_sw_params_set_start_threshold(
                pcmHandle, swParams, startThreshold);
        }
        if (err >= 0) {
            err = snd_pcm_sw_params_set_avail_min(
                pcmHandle, swParams, (snd_pcm_uframes_t)BUFFER_SIZE);
        }
        if (err >= 0) {
            err = snd_pcm_sw_params(pcmHandle, swParams);
        }
        if (err < 0) {
            snd_pcm_close(pcmHandle);
            pcmHandle = NULL;
            bOutputEnabled = false;
            return;
        }

        err = snd_pcm_prepare(pcmHandle);
        if (err < 0) {
            snd_pcm_close(pcmHandle);
            pcmHandle = NULL;
            bOutputEnabled = false;
            return;
        }

        // Only the audio worker uses this handle from now on. Blocking writes
        // are desirable there because they preserve every generated 20 ms
        // block without delaying the CPU/FLTK threads.
        err = snd_pcm_nonblock(pcmHandle, 0);
        if (err < 0) {
            snd_pcm_close(pcmHandle);
            pcmHandle = NULL;
            bOutputEnabled = false;
            return;
        }

        if (!pcmThreadRunning) {
            snd_pcm_close(pcmHandle);
            pcmHandle = NULL;
            return;
        }

    }
#endif
}

void Sound::closeAudio() {
#ifdef _WIN32
    int i;

    if (hWaveOut != NULL) {
        // Return all queued buffers to us before unpreparing them.
        waveOutReset(hWaveOut);

        if (waveHeadersPrepared) {
            for (i = 0; i < WAVE_BUFFER_COUNT; i++) {
                waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
            }
        }

        waveOutClose(hWaveOut);
        hWaveOut = NULL;
    }

    for (i = 0; i < WAVE_BUFFER_COUNT; i++) {
        if (waveData[i]) {
            delete[] waveData[i];
            waveData[i] = NULL;
        }
        waveBufferQueued[i] = false;
    }

    memset(waveHeaders, 0, sizeof(waveHeaders));
    waveHeadersPrepared = false;
    waveWriteIndex = 0;
#else
    // Wake and stop the ALSA writer before closing its PCM handle. drop()
    // also releases a thread currently blocked inside snd_pcm_writei().
    if (pcmThreadCreated) {
        pthread_mutex_lock(&pcmMutex);
        pcmThreadRunning = false;
        pthread_cond_broadcast(&pcmCond);
        pthread_mutex_unlock(&pcmMutex);

        if (pcmHandle != NULL) {
            snd_pcm_drop(pcmHandle);
        }
        pthread_join(pcmThread, NULL);
        pcmThreadCreated = false;
    }

    if (pcmHandle != NULL) {
        snd_pcm_drop(pcmHandle);
        snd_pcm_close(pcmHandle);
        pcmHandle = NULL;
    }

    pthread_mutex_lock(&pcmMutex);
    pcmQueueRead = 0;
    pcmQueueWrite = 0;
    pcmQueueCount = 0;
    pthread_mutex_unlock(&pcmMutex);

    pcmPeriodFrames = 0;
    pcmBufferFrames = 0;
#endif
}

void Sound::setEnabled(bool bVal) {
    bEnabled = bVal;
}

bool Sound::isEnabled() {
    return bEnabled;
}

void Sound::setOutputEnabled(bool bVal) {
    bOutputEnabled = bVal;
}

bool Sound::isOutputEnabled() {
    return bOutputEnabled;
}

void Sound::resetSource(long nInitTStates) {
    int i;

    if (playBuffer != NULL) {
        playBuffer->emptyBuffer(nInitTStates);
    }
    if (fillBuffer != NULL) {
        fillBuffer->emptyBuffer(nInitTStates);
    }

    for (i = 0; i < 8; ++i) {
        if (samples[i] != NULL) {
            samples[i]->resetPosition();
        }
    }
}

void Sound::switchBuffers(long nInitTStates) {
    if (playBuffer == NULL || fillBuffer == NULL) {
        return;
    }

    playBuffer->emptyTransferActiveSample(nInitTStates, fillBuffer);

    SoundBuffer* tmp = playBuffer;
    playBuffer = fillBuffer;
    fillBuffer = tmp;
}

void Sound::startPlaying() {
#ifdef _WIN32
    // waveOut playback is driven directly by writeWaveBuffer().
#else
    if (!bOutputEnabled || pcmThreadCreated) {
        return;
    }

    pthread_mutex_lock(&pcmMutex);
    pcmQueueRead = 0;
    pcmQueueWrite = 0;
    pcmQueueCount = 0;
    pcmXrunCount = 0;
    pcmQueueDropCount = 0;
    pcmThreadRunning = true;
    pthread_mutex_unlock(&pcmMutex);

    if (pthread_create(&pcmThread, NULL, Sound::pcmThreadProc, this) != 0) {
        pthread_mutex_lock(&pcmMutex);
        pcmThreadRunning = false;
        pthread_mutex_unlock(&pcmMutex);
        bOutputEnabled = false;
        return;
    }
    pcmThreadCreated = true;
#endif
}

void Sound::setDataReady() {
    setDataReady(NULL, 0);
}

void Sound::setDataReady(const unsigned char* melodikData, int melodikByteCount) {
    bool useSound;
    bool useMelodik;
    int i;

    if (!bOutputEnabled) {
        return;
    }

    useSound = bEnabled && (playBuffer != NULL);
    useMelodik = (melodikData != NULL) && (melodikByteCount >= 2);

    if (!useSound && !useMelodik) {
        return;
    }

    /*
     * Startup silence belongs to the shared output, not to either source.
     * This also gives Melodik-only playback the same 20 ms cushion that the
     * original Sound path has always used.
     */
    if (bFirstFill) {
        bFirstFill = false;
        fillBufferByZero();
    }

    /*
     * Keep the old Sound-only path byte-for-byte unchanged.  This is an
     * important regression guarantee for the already working Ondra sound.
     */
    if (useSound && !useMelodik) {
        writeWaveBuffer(playBuffer->getData(), 2 * BUFFER_SIZE);
        return;
    }

    if (melodikByteCount > 2 * BUFFER_SIZE) {
        melodikByteCount = 2 * BUFFER_SIZE;
    }

    for (i = 0; i < BUFFER_SIZE; ++i) {
        int soundSample = 0;
        int melodikSample = 0;
        int mixed;
        unsigned int encoded;

        if (useSound) {
            const unsigned char* source =
                (const unsigned char*)playBuffer->getData();
            unsigned int value = (unsigned int)source[2 * i] |
                                 ((unsigned int)source[2 * i + 1] << 8);
            soundSample = (value & 0x8000U)
                ? ((int)value - 65536)
                : (int)value;
        }

        if (useMelodik && (2 * i + 1) < melodikByteCount) {
            unsigned int value = (unsigned int)melodikData[2 * i] |
                                 ((unsigned int)melodikData[2 * i + 1] << 8);
            melodikSample = (value & 0x8000U)
                ? ((int)value - 65536)
                : (int)value;
        }

        mixed = soundSample + melodikSample;
        if (mixed > 32767) {
            mixed = 32767;
        } else if (mixed < -32768) {
            mixed = -32768;
        }

        encoded = (unsigned int)(mixed & 0xffff);
        mixBuffer[2 * i] = (char)(encoded & 0xff);
        mixBuffer[2 * i + 1] = (char)((encoded >> 8) & 0xff);
    }

    writeWaveBuffer(mixBuffer, 2 * BUFFER_SIZE);
}

bool Sound::writeWaveBuffer(const char* data, int length) {
    if (!bOutputEnabled || data == NULL || length <= 0) {
        return false;
    }

    if (length > 2 * BUFFER_SIZE) {
        length = 2 * BUFFER_SIZE;
    }

#ifdef _WIN32
    int tryCount;
    int index;
    WAVEHDR* header;
    MMRESULT res;

    if (hWaveOut == NULL || !waveHeadersPrepared) {
        return false;
    }

    // Never wait for a particular WAVEHDR. Find any free/done one.
    // This keeps CPU emulation independent of audio-driver timing.
    for (tryCount = 0; tryCount < WAVE_BUFFER_COUNT; tryCount++) {
        index = waveWriteIndex + tryCount;
        while (index >= WAVE_BUFFER_COUNT) {
            index -= WAVE_BUFFER_COUNT;
        }

        header = &waveHeaders[index];
        if (!waveBufferQueued[index] || ((header->dwFlags & WHDR_DONE) != 0)) {
            memcpy(waveData[index], data, length);
            header->dwBufferLength = (DWORD)length;

            res = waveOutWrite(hWaveOut, header, sizeof(WAVEHDR));
            if (res != MMSYSERR_NOERROR) {
                CDebug::debug("waveOutWrite failed: %u", (unsigned int)res);
                return false;
            }

            waveBufferQueued[index] = true;
            waveWriteIndex = index + 1;
            if (waveWriteIndex >= WAVE_BUFFER_COUNT) {
                waveWriteIndex = 0;
            }
            return true;
        }
    }

    // Audio is more than the ring depth behind. Do not stall emulation.
    CDebug::debug("Sound: all waveOut buffers busy, dropping 20ms frame");
    return false;
#else
    // Producer side only: copy the completed PCM block into a bounded queue
    // and return immediately. The ALSA thread performs all blocking writes.
    if (!pcmThreadCreated) {
        return false;
    }

    pthread_mutex_lock(&pcmMutex);

    if (!pcmThreadRunning) {
        pthread_mutex_unlock(&pcmMutex);
        return false;
    }

    if (pcmQueueCount >= PCM_QUEUE_COUNT) {
        // Keep real-time behaviour. If the audio device ever falls more than
        // 320 ms behind, discard the oldest block rather than stalling CPU
        // emulation or allowing latency to grow without bound.
        pcmQueueRead++;
        if (pcmQueueRead >= PCM_QUEUE_COUNT) {
            pcmQueueRead = 0;
        }
        pcmQueueCount--;
        pcmQueueDropCount++;
    }

    memcpy(pcmQueueData[pcmQueueWrite], data, length);
    pcmQueueLength[pcmQueueWrite] = length;
    pcmQueueWrite++;
    if (pcmQueueWrite >= PCM_QUEUE_COUNT) {
        pcmQueueWrite = 0;
    }
    pcmQueueCount++;

    pthread_cond_signal(&pcmCond);
    pthread_mutex_unlock(&pcmMutex);
    return true;
#endif
}

#ifndef _WIN32
void* Sound::pcmThreadProc(void* arg) {
    Sound* sound = (Sound*)arg;
    sound->pcmThreadLoop();
    return NULL;
}

void Sound::pcmThreadLoop() {
    char* localData = new char[2 * BUFFER_SIZE];

    // ALSA initialization belongs to this worker thread. This is essential on
    // Ubuntu 18.04 where opening the default PulseAudio-backed ALSA device as
    // root can otherwise block the FLTK event loop before emulation starts.
    openAudio();

    if (pcmHandle == NULL || !pcmThreadRunning) {
        pthread_mutex_lock(&pcmMutex);
        pcmThreadRunning = false;
        pthread_cond_broadcast(&pcmCond);
        pthread_mutex_unlock(&pcmMutex);
        delete[] localData;
        return;
    }

    while (true) {
        int localLength = 0;

        pthread_mutex_lock(&pcmMutex);
        while (pcmQueueCount == 0 && pcmThreadRunning) {
            pthread_cond_wait(&pcmCond, &pcmMutex);
        }

        if (!pcmThreadRunning) {
            pthread_mutex_unlock(&pcmMutex);
            break;
        }

        localLength = pcmQueueLength[pcmQueueRead];
        if (localLength > 2 * BUFFER_SIZE) {
            localLength = 2 * BUFFER_SIZE;
        }
        memcpy(localData, pcmQueueData[pcmQueueRead], localLength);

        pcmQueueRead++;
        if (pcmQueueRead >= PCM_QUEUE_COUNT) {
            pcmQueueRead = 0;
        }
        pcmQueueCount--;
        pthread_mutex_unlock(&pcmMutex);

        if (!pcmWriteDirect(localData, localLength) && !pcmThreadRunning) {
            break;
        }
    }

    pthread_mutex_lock(&pcmMutex);
    pcmThreadRunning = false;
    pthread_cond_broadcast(&pcmCond);
    pthread_mutex_unlock(&pcmMutex);

    delete[] localData;
}

bool Sound::pcmWriteDirect(const char* data, int length) {
    snd_pcm_sframes_t written;
    snd_pcm_sframes_t remaining;
    const char* cursor;
    snd_pcm_uframes_t frames;

    if (pcmHandle == NULL || data == NULL || length <= 0) {
        return false;
    }

    frames = (snd_pcm_uframes_t)(length / 2); // 16-bit mono
    remaining = (snd_pcm_sframes_t)frames;
    cursor = data;

    while (remaining > 0 && pcmThreadRunning) {
        written = snd_pcm_writei(pcmHandle,
                                 cursor,
                                 (snd_pcm_uframes_t)remaining);

        if (written == -EAGAIN) {
            snd_pcm_wait(pcmHandle, 20);
            continue;
        }
        if (written == -EINTR) {
            continue;
        }
        if (written < 0) {
            int recoverError;

            if (!pcmThreadRunning) {
                return false;
            }

            recoverError = snd_pcm_recover(pcmHandle, (int)written, 1);
            if (recoverError < 0) {
                return false;
            }

            pcmXrunCount++;
            // Retry the same real PCM block. Do not insert artificial silence;
            // ALSA's start threshold will pre-roll subsequent queued blocks.
            continue;
        }
        if (written == 0) {
            snd_pcm_wait(pcmHandle, 20);
            continue;
        }

        remaining -= written;
        cursor += written * 2;
    }

    return remaining == 0;
}
#endif

void Sound::fillBufferByZero() {
    // Startup cushion: two 10 ms silent blocks.
    writeWaveBuffer(silent, limit5ms);
    writeWaveBuffer(silent, limit5ms);
}
