#include "TapFile.h"
#include <stdlib.h>
#include <string.h>
#include "Debug.h"

// Definice statické promìnné pro chybovou zprávu
char TapFile::strErrorMsg[256] = "";

// --- Implementace tøídy TapBuffer ---

TapBuffer::TapBuffer(int nSize) {
    byteBuffer = (unsigned char*)malloc(nSize);
    if(byteBuffer) {
        memset(byteBuffer, 0, nSize);
    }
    nWBufferPosition = 0;
    nWBitPosition = 0;
    nRBufferPosition = 0;
    nRBitPosition = 0;
    bAllBufferReaded = false;
}

TapBuffer::~TapBuffer() {
    if (byteBuffer) {
        free(byteBuffer);
        byteBuffer = NULL;
    }
}

void TapBuffer::fillInitPause() {
    // Jednoduše posuneme write pozici o 100 bajtù (což odpovídá pauze)
    nWBufferPosition += 100;
}

void TapBuffer::fillPilot() {
    // Naplníme pilotní signál: 200 bajtù s hodnotou 255
    for (int i = nWBufferPosition; i < nWBufferPosition + 200; i++) {
        byteBuffer[i] = 255;
    }
    nWBufferPosition += 200;
    // Pilotní signál zakonèíme jedním bajtem s hodnotou 127
    byteBuffer[nWBufferPosition] = 127;
    nWBufferPosition++;
}

void TapBuffer::putByte(unsigned char value) {
    // Nejprve uložíme negaci 1. bitu (v Ondrovì notaci)
    unsigned char bit = (((value & 1) ^ 1) << nWBitPosition) & 0xFF;
    byteBuffer[nWBufferPosition] |= bit;
    IncWBitPosition();
    // Následnì postupnì vložíme všechny bity bajtu
    for (int i = 0; i < 8; i++) {
        int mask = (1 << i);
        if (value & mask) {
            bit = (1 << nWBitPosition);
            byteBuffer[nWBufferPosition] |= bit;
        }
        IncWBitPosition();
    }
}

void TapBuffer::IncWBitPosition() {
    nWBitPosition++;
    if(nWBitPosition > 7) {
        nWBitPosition = 0;
        nWBufferPosition++;
    }
}

bool TapBuffer::readBit() {
    bool bBit = false;
    int mask = (1 << nRBitPosition);
    if ((byteBuffer[nRBufferPosition] & mask) != 0) {
        bBit = true;
    }
    // Posun ètecí pozice – pokud jsme u posledního bajtu (èásteènì doplnìného)
    if (nRBufferPosition == nWBufferPosition) {
        nRBitPosition++;
        if(nRBitPosition > nWBitPosition) {
            bAllBufferReaded = true;
        }
    } else {
        nRBitPosition++;
        if(nRBitPosition > 7) {
            nRBitPosition = 0;
            nRBufferPosition++;
        }
    }
    return bBit;
}

// --- Implementace tøídy TapFile ---

TapFile::TapFile()
    : bFinished(false),
      bCrLfType(false),
      nMaxLen(0),
      filename(NULL),
      streamFile(NULL),
      tapeBuffer(NULL),
      nFrameSize(70),
      nFrameDecrement(0),
      nActualBit(3)
{
}

TapFile::~TapFile() {
    if (filename) {
        free(filename);
        filename = NULL;
    }
    if (streamFile) {
        fclose(streamFile);
        streamFile = NULL;
    }
    if (tapeBuffer) {
        delete tapeBuffer;
        tapeBuffer = NULL;
    }
}

// Statická metoda pro otevøení TAP souboru
TapFile* TapFile::openTapFile(const char* fname) {
    TapFile* tap = new TapFile();
    // Uložíme si kopii cesty
    tap->filename = (char*)malloc(strlen(fname) + 1);
    strcpy(tap->filename, fname);

    FILE* f = fopen(fname, "rb");
    if (!f) {
        strcpy(strErrorMsg, "Cannot open file");
        delete tap;
        return NULL;
    }
    // Zjistíme délku souboru
    fseek(f, 0, SEEK_END);
    long nMaxLen = ftell(f);
    rewind(f);
    tap->nMaxLen = nMaxLen;

    bool bCrlf = true;
    unsigned char crlfArray[2] = {0, 0};
    long nCounter = 0;

    // Smycka pro syntaktickou kontrolu celého souboru
    while (nCounter < nMaxLen) {
        unsigned char byteArray[25];
        if (!bCrlf) {
            // Pokud CRLF nebylo nalezeno, doplníme první dva bajty z predchozího crlfArray
            byteArray[0] = crlfArray[0];
            byteArray[1] = crlfArray[1];
            if (fread(byteArray + 2, 1, 23, f) != 23) {
                fclose(f);
                strcpy(strErrorMsg, "Unexpected EOF reading header");
                delete tap;
                return NULL;
            }
        } else {
            if (fread(byteArray, 1, 25, f) != 25) {
                fclose(f);
                strcpy(strErrorMsg, "Unexpected EOF reading header");
                delete tap;
                return NULL;
            }
        }

        if (byteArray[0] != 'H') {
            fclose(f);
            strcpy(strErrorMsg, "Wrong header flagbyte! Maybe not an ondra tape.");
            delete tap;
            return NULL;
        }
        bCrlf = false;
        if (fread(crlfArray, 1, 2, f) != 2) {
            // Ignorujeme, pokud nelze precíst CRLF
        }

        // Zkontrolujeme obe varianty poradí CRLF: Windows (0x0D,0x0A) a alternativní (0x0A,0x0D)
        if ((crlfArray[0] == 0x0D && crlfArray[1] == 0x0A) ||
            (crlfArray[0] == 0x0A && crlfArray[1] == 0x0D)) {
            bCrlf = true;
            nCounter += 2;            
        } else {
            // Pokud neodpovídají CRLF, uložíme je do crlfArray pro použití v další iteraci
            bCrlf = false;            
        }
        int tmp_size = 256 * (byteArray[22] & 0xff) + (byteArray[21] & 0xff);

        if (byteArray[16] != 'D') {
            fclose(f);
            strcpy(strErrorMsg, "Wrong header byte! Maybe not an ondra tape.");
            delete tap;
            return NULL;
        }
        unsigned char nDataByte = 0;
        // Pokud CRLF bylo nalezeno, precteme 1 bajt, jinak použijeme crlfArray[0]
        if (bCrlf) {
            if (fread(&nDataByte, 1, 1, f) != 1) {
                fclose(f);
                strcpy(strErrorMsg, "Unexpected EOF reading data byte");
                delete tap;
                return NULL;
            }
        } else {
            nDataByte = crlfArray[0];
        }
        if (nDataByte != 'D') {
            fclose(f);
            strcpy(strErrorMsg, "Wrong data byte! Maybe not an ondra tape.");
            delete tap;
            return NULL;
        }
        // Nacteme datový blok
        unsigned char* byteArrayTmp = (unsigned char*)malloc(tmp_size);
        if (bCrlf) {
            if (fread(byteArrayTmp, 1, tmp_size, f) != (size_t)tmp_size) {
                free(byteArrayTmp);
                fclose(f);
                strcpy(strErrorMsg, "Unexpected EOF reading data block");
                delete tap;
                return NULL;
            }
        } else {
            byteArrayTmp[0] = crlfArray[1];
            if (fread(byteArrayTmp + 1, 1, tmp_size - 1, f) != (size_t)(tmp_size - 1)) {
                free(byteArrayTmp);
                fclose(f);
                strcpy(strErrorMsg, "Unexpected EOF reading data block");
                delete tap;
                return NULL;
            }
        }
        // Nacteme CRC jeden bajt pomocí fread
        unsigned char nReadCRC = 0;
        if (fread(&nReadCRC, 1, 1, f) != 1) {
            free(byteArrayTmp);
            fclose(f);
            strcpy(strErrorMsg, "Unexpected EOF reading CRC");
            delete tap;
            return NULL;
        }
        int nComputeCRC = 0;
        for (int i = 0; i < tmp_size; i++) {
            nComputeCRC = (nComputeCRC + byteArrayTmp[i]) & 0xff;
        }
        if (nReadCRC != (unsigned char)nComputeCRC) {
            free(byteArrayTmp);
            fclose(f);
            strcpy(strErrorMsg, "Wrong CRC of binary block");
            delete tap;
            return NULL;
        }
        free(byteArrayTmp);
        bCrlf = false;
        unsigned char tempCrlf[2] = {0, 0};
        if (fread(tempCrlf, 1, 2, f) == 2) {
            if ((tempCrlf[0] == 0x0D && tempCrlf[1] == 0x0A) ||
                (tempCrlf[0] == 0x0A && tempCrlf[1] == 0x0D)) {
                bCrlf = true;
                nCounter += 2;
            } else {
                // Pokud nejsou CRLF, uložíme je do crlfArray pro príští iteraci
                bCrlf = false;
                crlfArray[0] = tempCrlf[0];
                crlfArray[1] = tempCrlf[1];
            }
        }
        nCounter += 27 + tmp_size;     
    }
    fclose(f);
    tap->bCrLfType = bCrlf;    
    return tap;
}




bool TapFile::fillBuffer() {
    if (!bFinished) {
        if (streamFile == NULL) {
            streamFile = fopen(filename, "rb");
            if (!streamFile) {
                return false;
            }
            // Posuneme se na pøíslušnou pozici, pokud je tøeba – zde se pøedpokládá, že èteme od zaèátku
        }
        int nCrLfCorrect = bCrLfType ? 2 : 0;
        int headerSize = 25 + nCrLfCorrect;
        unsigned char* byteArrayHeader = (unsigned char*)malloc(headerSize);
        if (fread(byteArrayHeader, 1, headerSize, streamFile) != (size_t)headerSize) {
            free(byteArrayHeader);
            bFinished = true;
            fclose(streamFile);
            streamFile = NULL;
            return false;
        }
        int nBlockSize = 256 * (byteArrayHeader[22] & 0xff) + (byteArrayHeader[21] & 0xff);

        // byteArrayHeader obsahuje prave nactenou 25bajtovou hlavicku
        // a pripadne dva CR/LF bajty za ni.
        if (tapeBuffer) {
            delete tapeBuffer;
        }
        tapeBuffer = new TapBuffer(1024 + nBlockSize);
        if (!tapeBuffer || !tapeBuffer->byteBuffer) {
            free(byteArrayHeader);
            if (tapeBuffer) {
                delete tapeBuffer;
                tapeBuffer = NULL;
            }
            bFinished = true;
            return false;
        }

        // Poradi bloku: pauza -> pilot -> header -> final byte -> pauza -> pilot -> data.
        tapeBuffer->fillInitPause();
        tapeBuffer->fillPilot();
        for (int i = 0; i < 25; i++) {
            tapeBuffer->putByte(byteArrayHeader[i]);
        }
        free(byteArrayHeader);
        byteArrayHeader = NULL;
        tapeBuffer->putByte(0);
        // Pauza mezi bloky
        tapeBuffer->nWBitPosition = 0;
        tapeBuffer->nWBufferPosition += 2;
        tapeBuffer->fillPilot();
        // Èteme data
        int dataSize = nBlockSize + 2 + nCrLfCorrect;
        unsigned char* byteArrayData = (unsigned char*)malloc(dataSize);
        memset(byteArrayData, 0, dataSize);
        if (fread(byteArrayData, 1, dataSize, streamFile) != (size_t)dataSize) {
            free(byteArrayData);
            bFinished = true;
            fclose(streamFile);
            streamFile = NULL;
            return false;
        }
        for (int ix = 0; ix < nBlockSize + 2; ix++) {
            tapeBuffer->putByte(byteArrayData[ix]);
        }
        tapeBuffer->putByte(0);
        tapeBuffer->nWBitPosition = 0;
        tapeBuffer->nWBufferPosition += 2;
        free(byteArrayData);
    }
    return !bFinished;
}

bool TapFile::readNextBit() {
    bool bRet = false;
    if (tapeBuffer == NULL) {
        if (fillBuffer()) {
            bRet = tapeBuffer->readBit();
        }
    } else {
        bRet = tapeBuffer->readBit();
    }
    if (tapeBuffer && tapeBuffer->bAllBufferReaded) {
        fillBuffer();
    }
    return bRet;
}

bool TapFile::generateFrame() {
    bool bRet = false;
    if(nActualBit == 3) {
        // První spuštìní – naèteme první bit
        nActualBit = readNextBit() ? 1 : 0;
        nFrameDecrement = 2 * nFrameSize;
    }
    if(nFrameDecrement > nFrameSize) {
        // Druhá polovina frame
        bRet = (nActualBit != 0);
    } else {
        // První polovina frame
        bRet = !(nActualBit != 0);
    }
    nFrameDecrement--;
    if(nFrameDecrement == 0) {
        nActualBit = readNextBit() ? 1 : 0;
        nFrameDecrement = 2 * nFrameSize;
    }
    return bRet;
}
