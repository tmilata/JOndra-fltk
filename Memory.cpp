#include "Memory.h"
#include "Config.h"
#include "Debug.h"
#include "Screen.h"
#include "Ondra.h"
#include "EmbeddedResources.h"
#include <stdio.h>
#include <string>
#include <string.h>




Memory::Memory(Ondra* machine, Config* cnf)
: IOVect(NULL), tapeIn(true), m(machine), cf(cnf) {
    memset(fakeROM, 0, PAGE_SIZE*sizeof(unsigned char));
    for (int i = 0; i < 32; i++) {
        readPages[i] = NULL;
        writePages[i] = NULL;
    }
    loadRoms();
}

void Memory::setIOVect(unsigned char* iov) {
    IOVect = iov;
}

void Memory::setTapeIn(int x) {
    /* TAP changes this signal extremely often. Store only the current
     * level; readByte() merges it into bit 7 of the I/O value. */
    tapeIn = (x != 0);
}

void Memory::Reset(int dirty) {
    tapeIn = true;
	if (dirty) {
		uint8_t character = 0;
		int index = 0;
		for (int i = 0; i < 32; i++) {
			for (int j = 0; j < PAGE_SIZE; j++) {				
				//int reconstructedAddress = (i << PAGE_BIT) | (j & PAGE_MASK);				
				Ram[i][j] = (uint8_t)character;
				index = (index + 1) & 127;
				if (index == 0) {
					character ^= 255;
				}
			}
		}
	}

    if (cf->getRomType() == 3) {
        for (int i = 0; i < PAGE_SIZE; i++) {
            for (int page = 0; page <= 7; page++) {
                Cust[page][i] = 255;
            }
        }
        loadCustomRom(cf->getRomA().c_str(), 0);
        loadCustomRom(cf->getRomB().c_str(), 4);
    }
    for (int i = 0; i < 32; i++) {
        readPages[i] = Ram[i];
        writePages[i] = Ram[i];
    }	
}

unsigned char Memory::readRam(int address) {
    return Ram[(static_cast<unsigned int>(address) >> PAGE_BIT)][address & PAGE_MASK];
}

unsigned char Memory::readByte(int address) {
    if(readPages[(static_cast<unsigned int>(address) >> PAGE_BIT)] == IOVect){
        unsigned char value = readPages[(static_cast<unsigned int>(address) >> PAGE_BIT)][address & 0xFF];
        if (tapeIn) {
            value = (unsigned char)(value | 0x80);
        } else {
            value = (unsigned char)(value & 0x7f);
        }
        return value;
    }
    return readPages[(static_cast<unsigned int>(address) >> PAGE_BIT)][address & PAGE_MASK];
}

bool Memory::writeByte(int address, unsigned char value) {
    if (writePages[address >> PAGE_BIT] == fakeROM) {
        return false;
    }
    if (writePages[address >> PAGE_BIT][address & PAGE_MASK] == value) {
        return false;
    }

    writePages[address >> PAGE_BIT][address & PAGE_MASK] = value;
    if (address >= 0xd800) {
        m->processVram(address);
    }
    return true;
}

void Memory::mapIO(int state) {
	if (!state) {
		readPages[28] = writePages[28] = Ram[28];
		readPages[29] = writePages[29] = Ram[29];
		readPages[30] = writePages[30] = Ram[30];
		readPages[31] = writePages[31] = Ram[31];

	}
	else {
		writePages[28] = fakeROM;
		writePages[29] = fakeROM;
		writePages[30] = fakeROM;
		writePages[31] = fakeROM;

		readPages[28] = IOVect;
		readPages[29] = IOVect;
		readPages[30] = IOVect;
		readPages[31] = IOVect;
	}
	
}

void Memory::mapRom(int state) {
	if (!state) {
		readPages[0] = writePages[0] = Ram[0];
		readPages[1] = writePages[1] = Ram[1];
		readPages[2] = writePages[2] = Ram[2];
		readPages[3] = writePages[3] = Ram[3];
		readPages[4] = writePages[4] = Ram[4];
		readPages[5] = writePages[5] = Ram[5];
		readPages[6] = writePages[6] = Ram[6];
		readPages[7] = writePages[7] = Ram[7];
	}
	else {
		writePages[0] = fakeROM;
		writePages[1] = fakeROM;
		writePages[2] = fakeROM;
		writePages[3] = fakeROM;
		writePages[4] = fakeROM;
		writePages[5] = fakeROM;
		writePages[6] = fakeROM;
		writePages[7] = fakeROM;
		switch(Config::nRomType) {
		case 0: {               // BASIC
			readPages[0] = Basic[0];
			readPages[1] = Basic[1];
			readPages[2] = Basic[2];
			readPages[3] = Basic[3];
			readPages[4] = Basic[4];
			readPages[5] = Basic[5];
			readPages[6] = Basic[6];
			readPages[7] = Basic[7];
			break;
				}

		case 1: {               // TESLA
			readPages[0] = Tesla[0];
			readPages[1] = Tesla[0];
			readPages[2] = Tesla[0];
			readPages[3] = Tesla[0];
			readPages[4] = Tesla[1];
			readPages[5] = Tesla[1];
			readPages[6] = Tesla[1];
			readPages[7] = Tesla[1];
			break;
				}

		case 2: {               // ViLi
			readPages[0] = Vili[0];
			readPages[1] = Vili[0];
			readPages[2] = Vili[0];
			readPages[3] = Vili[0];
			readPages[4] = Vili[1];
			readPages[5] = Vili[1];
			readPages[6] = Vili[1];
			readPages[7] = Vili[1];
			break;
				}

		default: {               // Custom
			readPages[0] = Cust[0];
			readPages[1] = Cust[1];
			readPages[2] = Cust[2];
			readPages[3] = Cust[3];
			readPages[4] = Cust[4];
			readPages[5] = Cust[5];
			readPages[6] = Cust[6];
			readPages[7] = Cust[7];
				 }
		}
	}
}

std::string Memory::getFullPath(const char* filename) {
    if (!filename || filename[0] == '\0') {
        return "";
    }

    std::string filePath(filename);

    if (filePath.find('\\') != std::string::npos || filePath.find('/') != std::string::npos) {
        return filePath;
    }

    std::string fullPath = Config::getMyPath() + "roms/" + filePath;
    return fullPath;
}

int Memory::loadSnapshotRam(const char* filename) {
    std::string fullPath = getFullPath(filename);
    FILE* file = fopen(fullPath.c_str(), "rb");
    if (!file) return 0;

    for (int i = 0; i < 32; i++) {
        if (fread(Ram[i], 1, PAGE_SIZE, file) != PAGE_SIZE) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

int Memory::saveSnapshotRam(const char* filename) {
    std::string fullPath = getFullPath(filename);
    FILE* file = fopen(fullPath.c_str(), "wb");
    if (!file) return 0;

    for (int i = 0; i < 32; i++) {
        if (fwrite(Ram[i], 1, PAGE_SIZE, file) != PAGE_SIZE) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

int Memory::loadSnapshotCustomRom(const char* filename) {
    std::string fullPath = getFullPath(filename);
    FILE* file = fopen(fullPath.c_str(), "rb");
    if (!file) return 0;

    for (int i = 0; i < 8; i++) {
        if (fread(Cust[i], 1, PAGE_SIZE, file) != PAGE_SIZE) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

int Memory::saveSnapshotCustomRom(const char* filename) {
    std::string fullPath = getFullPath(filename);
    FILE* file = fopen(fullPath.c_str(), "wb");
    if (!file) return 0;

    for (int i = 0; i < 8; i++) {
        if (fwrite(Cust[i], 1, PAGE_SIZE, file) != PAGE_SIZE) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}


int Memory::loadSnapshotRam(FILE* file) {
    if (!file) return 0;
    for (int i = 0; i < 32; i++) {
        if (fread(Ram[i], 1, PAGE_SIZE, file) != PAGE_SIZE) return 0;
    }
    return 1;
}

int Memory::saveSnapshotRam(FILE* file) {
    if (!file) return 0;
    for (int i = 0; i < 32; i++) {
        if (fwrite(Ram[i], 1, PAGE_SIZE, file) != PAGE_SIZE) return 0;
    }
    return 1;
}

int Memory::loadSnapshotCustomRom(FILE* file) {
    if (!file) return 0;
    for (int i = 0; i < 8; i++) {
        if (fread(Cust[i], 1, PAGE_SIZE, file) != PAGE_SIZE) return 0;
    }
    return 1;
}

int Memory::saveSnapshotCustomRom(FILE* file) {
    if (!file) return 0;
    for (int i = 0; i < 8; i++) {
        if (fwrite(Cust[i], 1, PAGE_SIZE, file) != PAGE_SIZE) return 0;
    }
    return 1;
}

void Memory::copyRamToByteArray(unsigned char* dest) {
    for (int pageIndex = 0; pageIndex < 32; pageIndex++) {
        memcpy(dest + pageIndex * PAGE_SIZE, Ram[pageIndex], PAGE_SIZE);
    }
}

void Memory::copyRawRamPage(int page, unsigned char* dest) {
    if (dest == NULL || page < 0 || page >= 32) return;
    memcpy(dest, Ram[page], PAGE_SIZE);
}

void Memory::restoreRawRamFromByteArray(const unsigned char* src) {
    int pageIndex;
    if (src == NULL) return;
    for (pageIndex = 0; pageIndex < 32; ++pageIndex) {
        memcpy(Ram[pageIndex], src + pageIndex * PAGE_SIZE, PAGE_SIZE);
    }
}

void Memory::getRamPages(unsigned char* pages[32]) {
    memcpy(pages, readPages, sizeof(readPages));
}

void Memory::loadCustomRom(const char* name, int page) {
    std::string fullPath = getFullPath(name);
    FILE* file = fopen(fullPath.c_str(), "rb");
    if (!file) return;

    fread(Cust[page], 1, PAGE_SIZE * 4, file);
    fclose(file);
}


void Memory::loadRoms() {
    if (!loadRomAsResource(Config::strBasicA.c_str(), Basic, 0, PAGE_SIZE * 4)) {
        fprintf(stderr, "Failed to load embedded Basic A ROM: %s\n", Config::strBasicA.c_str());
    }
    if (!loadRomAsResource(Config::strBasicB.c_str(), Basic, 4, PAGE_SIZE * 4)) {
        fprintf(stderr, "Failed to load embedded Basic B ROM: %s\n", Config::strBasicB.c_str());
    }

    if (!loadRomAsResource(Config::strTeslaA.c_str(), Tesla, 0, PAGE_SIZE)) {
        fprintf(stderr, "Failed to load embedded Tesla A ROM: %s\n", Config::strTeslaA.c_str());
    }
    if (!loadRomAsResource(Config::strTeslaB.c_str(), Tesla, 1, PAGE_SIZE)) {
        fprintf(stderr, "Failed to load embedded Tesla B ROM: %s\n", Config::strTeslaB.c_str());
    }

    if (!loadRomAsResource(Config::strViLiA.c_str(), Vili, 0, PAGE_SIZE)) {
        fprintf(stderr, "Failed to load embedded ViLi A ROM: %s\n", Config::strViLiA.c_str());
    }
    if (!loadRomAsResource(Config::strViLiB.c_str(), Vili, 1, PAGE_SIZE)) {
        fprintf(stderr, "Failed to load embedded ViLi B ROM: %s\n", Config::strViLiB.c_str());
    }
}

int Memory::loadRomAsResource(const char* filename,
                              unsigned char rom[][PAGE_SIZE],
                              int page,
                              int size) {
    std::string resourceName("roms/");
    resourceName += filename;

    if (!EmbeddedResources::copyData(resourceName.c_str(), rom[page],
                                     (unsigned long)size)) {
        return 0;
    }
    return 1;
}

bool Memory::loadRomAsFile(const char* filename,
                           unsigned char rom[8][PAGE_SIZE],
                           int page,
                           int size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return false;
    }

    size_t bytesRead = fread(rom[page], 1, size, file);
    fclose(file);
    return bytesRead == (size_t)size;
}


