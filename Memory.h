// Memory.h
#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <stdio.h>

// Forward declarations
class Ondra;
class Config;

#define PAGE_SIZE 2048
#define PAGE_MASK (PAGE_SIZE - 1)
#define PAGE_BIT 11

class Memory {
public:
    Memory(Ondra* machine, Config* cnf);
    void setIOVect(unsigned char* iov);
    void setTapeIn(int x);
    void Reset(int dirty);
    unsigned char readRam(int address);
    unsigned char readByte(int address);
    bool writeByte(int address, unsigned char value);
    void mapIO(int state);
    void mapRom(int state);
    int loadSnapshotRam(const char* filename);
    int saveSnapshotRam(const char* filename);
    int loadSnapshotCustomRom(const char* filename);
    int saveSnapshotCustomRom(const char* filename);
    int loadSnapshotRam(FILE* file);
    int saveSnapshotRam(FILE* file);
    int loadSnapshotCustomRom(FILE* file);
    int saveSnapshotCustomRom(FILE* file);
    void copyRamToByteArray(unsigned char* dest);
    void copyRawRamPage(int page, unsigned char* dest);
    void restoreRawRamFromByteArray(const unsigned char* src);
    void getRamPages(unsigned char* pages[32]);
	std::string getFullPath(const char* filename);

private:
    void loadCustomRom(const char* name, int page);
    void loadRoms();    
    int loadRomAsResource(const char* filename, unsigned char rom[][PAGE_SIZE], int page, int size);
	bool loadRomAsFile(const char* filename, unsigned char rom[8][PAGE_SIZE], int page, int size);

    unsigned char Ram[32][PAGE_SIZE];
    unsigned char Basic[8][PAGE_SIZE];
    unsigned char Tesla[2][PAGE_SIZE];
    unsigned char Vili[2][PAGE_SIZE];
    unsigned char Cust[8][PAGE_SIZE];
    unsigned char* readPages[32];
    unsigned char* writePages[32];
    unsigned char fakeROM[PAGE_SIZE];
    unsigned char* IOVect;
    /* Tape input is bit 7 of memory-mapped I/O reads. Keep it
     * separately instead of rewriting the whole 2 KB I/O vector for
     * every tape sample. */
    bool tapeIn;
    Ondra* m;
    Config* cf;
};

#endif // MEMORY_H