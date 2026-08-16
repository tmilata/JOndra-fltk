#include "MTimer.h"
#include "Ondra.h"       // Musí mít metody ms20(), isPaused() a cleny til (s metodou DispUpdate), scr (s metodou isNeedRedraw()), px, atd.
#include "Screen.h"      // Napr. m->scr->isNeedRedraw()
#include "DirtyTiles.h"  // Napr. m->til->DispUpdate()
#include "Debug.h"       // //CDebug::debug() – predpokládáme, že tato funkce zapisuje do logu
#include <FL/Fl.H>

#ifdef _WIN32
#include <mmsystem.h>    // timeBeginPeriod, timeEndPeriod
#pragma comment(lib, "winmm.lib")
#else
#include <unistd.h>
#endif

#ifdef _WIN32
LARGE_INTEGER MTimer::frequency;
double MTimer::invFreqMs = 0;
#endif

MTimer::MTimer(Ondra* ondra)
    : m(ondra), intervalMs(20), running(false)
#ifdef _WIN32
    , threadHandle(NULL)
#endif
{
#ifdef _WIN32
    if (frequency.QuadPart == 0) { // Inicializace jednou
        QueryPerformanceFrequency(&frequency);
        invFreqMs = 1000.0 / (double)frequency.QuadPart;
    }
#endif
}

MTimer::~MTimer() {
    //CDebug::debug("MTimer destructor called, stopping timer.");
    StopTimer();
}

void MTimer::StartTimer(int intervalMs) {
    if (running) return;
    this->intervalMs = intervalMs;
    running = true;
    //CDebug::debug("StartTimer called, intervalMs=%d", intervalMs);
#ifdef _WIN32
    timeBeginPeriod(1); // Pro presnejší casování
    threadHandle = CreateThread(NULL, 0, TimerLoop, this, 0, &threadID);
#else
    pthread_create(&threadHandle, NULL, TimerLoop, this);
#endif
}

void MTimer::StopTimer() {
    //CDebug::debug("StopTimer called.");
    running = false;
#ifdef _WIN32
    if (threadHandle) {
        /*
         * stopEmulation() is also the CPU/debugger synchronization barrier.
         * Do not return while the emulation thread can still modify Z80_STATE.
         * Linux already has the same semantics through pthread_join().
         */
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
        threadHandle = NULL;
    }
    timeEndPeriod(1); // Vrací casovac na puvodní hodnotu
#else
    if (threadHandle) {
        pthread_join(threadHandle, NULL);
    }
#endif
    //CDebug::debug("StopTimer finished.");
}

uint64_t MTimer::getCurrentTimeMillis() {
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * invFreqMs);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
#endif
}

#ifdef _WIN32
DWORD WINAPI MTimer::TimerLoop(LPVOID arg)
#else
void* MTimer::TimerLoop(void* arg)
#endif
{
    MTimer* timer = (MTimer*)arg;
    uint64_t lastDisplayUpdate = 0;

    while (timer->running) {
        uint64_t startTime = getCurrentTimeMillis();
        //CDebug::debug("TimerLoop: iteration start, startTime=%I64u ms, interval=%d ms", startTime, timer->intervalMs);

        timer->m->ms20();
        //CDebug::debug("TimerLoop: ms20() returned");

        /* During fast TAP loading the timer runs at 2 ms (10x), so drawing every
         * block would unnecessarily throttle emulation.  Completely suppressing
         * drawing, however, hides the real Ondra ROM's DMA flicker during LOAD.
         * In fast mode publish the current framebuffer about every 20 ms of real
         * time. */
        {
            uint64_t nowForDisplay = getCurrentTimeMillis();
            bool fastTapeMode = (timer->intervalMs <= 2);
            bool updateDisplay = !fastTapeMode;

            if (fastTapeMode) {
                if (lastDisplayUpdate == 0 ||
                    (nowForDisplay - lastDisplayUpdate) >= 20) {
                    updateDisplay = true;
                }
            }

            if (updateDisplay && !timer->m->scr->isNeedRedraw()) {
                /*
                 * Never wait here for the FLTK thread to paint the frame.
                 * stopEmulation() is called from GUI callbacks (Reset,
                 * Settings, debugger...) and joins this worker thread. If the
                 * worker waited for needsRedraw to be cleared by the GUI at
                 * exactly that moment, both threads could wait for each other
                 * forever.
                 *
                 * If a frame is still pending, keep the dirty-tile state
                 * accumulated and publish it on a later iteration.
                 */
                timer->m->til->DispUpdate(timer->m->px);
                lastDisplayUpdate = nowForDisplay;
            }
        }

        // Cekáme, dokud neuplyne celý interval
        while (timer->running) {
            uint64_t now = getCurrentTimeMillis();
            int elapsed = (int)(now - startTime);
            int remainingTime = timer->intervalMs - elapsed;
            //CDebug::debug("TimerLoop: elapsed=%d ms, remainingTime=%d ms", elapsed, remainingTime);
            if (remainingTime <= 0)
                break; // Interval vypršel

            if (remainingTime > 5) {
#ifdef _WIN32
                Sleep(remainingTime - 2);
#else
                usleep((remainingTime - 2) * 1000);
#endif
                //CDebug::debug("TimerLoop: Slept for %d ms", remainingTime - 2);
            }

            // Aktivní cekací smycka – kontrolujeme running
            while (timer->running && ((int)(getCurrentTimeMillis() - startTime) < timer->intervalMs)) {
#ifdef _WIN32
                Sleep(1);
#else
                usleep(1000);
#endif
            }
        }
        uint64_t endTime = getCurrentTimeMillis();
        //CDebug::debug("TimerLoop: iteration end, cycle time=%d ms", (int)(endTime - startTime));
    }
    //CDebug::debug("TimerLoop: Exiting thread, running=%d", timer->running);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
