#ifndef MTIMER_H
#define MTIMER_H


#ifdef _WIN32
#include <windows.h>
#if defined(_MSC_VER) && (_MSC_VER < 1300)
    #include "vs_stdint.h"
#else
    #include <stdint.h>
#endif
#else
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>
#endif

// Predbežná deklarace trídy Ondra – predpokládáme, že obsahuje metody ms20(), isPaused(),
// a cleny til (s metodou DispUpdate), scr (s metodou isNeedRedraw()), px apod.
class Ondra;

class MTimer {
public:
    // Konstruktor prijímá ukazatel na instanci Ondra
    MTimer(Ondra* ondra);
    ~MTimer();

    // Startuje timer s daným intervalem (v milisekundách)
    void StartTimer(int intervalMs);
    // Zastaví timer – ceká na ukoncení vlákna
    void StopTimer();

    // Vrací aktuální cas v milisekundách
    static uint64_t getCurrentTimeMillis();

#ifdef _WIN32
    static DWORD WINAPI TimerLoop(LPVOID arg);
#else
    static void* TimerLoop(void* arg);
#endif

    volatile bool running; // Príznak behu timeru
    int intervalMs;        // Casový interval v milisekundách

private:
    Ondra* m;  // Ukazatel na instanci emulátoru Ondra
#ifdef _WIN32
    HANDLE threadHandle;
    DWORD threadID;
    static LARGE_INTEGER frequency;
    static double invFreqMs;
#else
    pthread_t threadHandle;
#endif
};

#endif // MTIMER_H
