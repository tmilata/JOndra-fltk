#include "Clock.h"

Clock::Clock()
    : tstates(0), frames(0), timeout(0)
{
}

void Clock::addClockTimeoutListener(ClockTimeoutListener* listener) {
    if (listener == NULL) {
        throw std::runtime_error("Error: Listener can't be null");
    }
    // Pøidáme pouze, pokud ještì není registrován.
    for (size_t i = 0; i < clockListeners.size(); i++) {
        if (clockListeners[i] == listener) {
            return;
        }
    }
    clockListeners.push_back(listener);
}

void Clock::removeClockTimeoutListener(ClockTimeoutListener* listener) {
    if (listener == NULL) {
        throw std::runtime_error("Internal Error: Listener can't be null");
    }
    bool found = false;
    for (size_t i = 0; i < clockListeners.size(); i++) {
        if (clockListeners[i] == listener) {
            // Odebereme posluchaèe.
            clockListeners.erase(clockListeners.begin() + i);
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("Listener wasn't registered");
    }
}

uint64_t Clock::getTstates() const {
    return tstates;
}

void Clock::setTstates(uint64_t states) {
    tstates = states;
    frames = 0;
}

void Clock::addTstates(uint64_t states) {
    tstates += states;
    if (timeout > 0) {
        timeout -= states;
        if (timeout < 1) {
            // Vyvoláme clockTimeout() u všech registrovaných posluchaèù.
            for (size_t i = 0; i < clockListeners.size(); i++) {
                clockListeners[i]->clockTimeout();
            }
        }
    }
}

long Clock::getFrames() const {
    return frames;
}

void Clock::reset() {
    frames = tstates = 0;
}

void Clock::setTimeout(int64_t ntstates) {
    timeout = (ntstates > 0) ? ntstates : 1;
}
