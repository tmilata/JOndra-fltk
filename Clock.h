#ifndef CLOCK_H
#define CLOCK_H

#include <vector>
#include <stdexcept>
#if defined(_MSC_VER) && (_MSC_VER < 1300)
    #include "vs_stdint.h"
#else
    #include <stdint.h>
#endif

// Listener notified when the emulated clock reaches a scheduled timeout.
class ClockTimeoutListener {
public:
    virtual void clockTimeout() = 0;
};


// Tøída Clock – spravuje t-states, timeout a seznam posluchaèù.
class Clock {
public:
    Clock();

    // Pøidá posluchaèe; vyhodí výjimku, pokud je NULL.
    void addClockTimeoutListener(ClockTimeoutListener* listener);

    // Odebere posluchaèe; vyhodí výjimku, pokud je NULL nebo nebyl nalezen.
    void removeClockTimeoutListener(ClockTimeoutListener* listener);

    uint64_t getTstates() const;
    void setTstates(uint64_t states);
    void addTstates(uint64_t states);
    long getFrames() const;
    void reset();
    void setTimeout(int64_t ntstates);

private:
    uint64_t tstates;
    long frames;
    int64_t timeout;
    std::vector<ClockTimeoutListener*> clockListeners;
};

#endif // CLOCK_H
