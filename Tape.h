#ifndef TAPE_HPP
#define TAPE_HPP

#include <string>
#include "Clock.h"

class Ondra;
class TapeSignalProc;
class TapFile;
class CswTapeReader;
class CswTapeWriter;
class WavTapeReader;

class Tape : public ClockTimeoutListener {
public:
    Tape(Ondra* machine);
    ~Tape();

    // Open a tape image for LOAD. The format is detected by file contents
    // in the order CSW, TAP, WAV.
    void openLoadTape(const std::string &canonicalPath);

    // Open a new CSW image for SAVE. Existing files are overwritten.
    bool openSaveTape(const std::string &canonicalPath);

    void tapeStart();
    void tapeStop();
    void setTapeMode(bool rec);
    virtual void clockTimeout();
    void closeCleanup();
    void saveDump();

private:
    enum State { CLOSE, CSW, WAV, TAP };

    void closeLoadTape();
    void closeSaveTape();
    int rateToTstates(long sampleRate) const;

    Ondra* m;
    TapeSignalProc* tsp;
    int tbuff0;

    CswTapeReader* lcsw;
    CswTapeWriter* scsw;
    WavTapeReader* lwav;
    TapFile* ltap;

    int lrate;
    int srate;
    State lst;
    State sst;
    bool record;
    std::string bitDump;
};

#endif // TAPE_HPP
