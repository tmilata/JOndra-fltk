#include "SoundSample.h"
#include <cstdio>
#include "Config.h"
#include <string>
#include <string.h>

SoundSample::SoundSample(const char* inData, int length)
: nLen(length), nPos(0)
{
    if (length > 0 && inData != 0) {
        sample = new char[length];
        for (int i = 0; i < length; i++) {
            sample[i] = inData[i];
        }
    } else {
        sample = 0;
        nLen = 0;
    }
}

SoundSample::SoundSample(const char* filePath)
: sample(0), nLen(0), nPos(0)
{
    loadFromFile(filePath);
}

SoundSample::~SoundSample() {
    if (sample) {
        delete[] sample;
    }
}

void SoundSample::loadFromFile(const char* filePath) {
	std::string fullPath = Config::getMyPath() + filePath;
	FILE* fp = fopen(fullPath.c_str(), "rb");
    if (!fp) {
        // Soubor se nepodaøilo otevøít – mùžeš sem pøípadnì doplnit chybovou hlášku.
        return;
    }
    // Zjištìní velikosti souboru.
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
	
    if (fileSize <= 0) {
        fclose(fp);
        return;
    }
	
    // Uvolnìní pøedchozích dat, pokud existují.
    if (sample) {
        delete[] sample;
        sample = 0;
    }
    nLen = (int)fileSize;
    sample = new char[nLen];
	
    size_t readSize = fread(sample, 1, nLen, fp);
    if (readSize != (size_t)nLen) {
        // Pokud došlo k chybì pøi ètení, uvolníme pamì.
        delete[] sample;
        sample = 0;
        nLen = 0;
    }
    fclose(fp);
    nPos = 0;
}

void SoundSample::resetPosition() {
    nPos = 0;
}

char SoundSample::getNextByte() {
    if (nLen == 0 || sample == 0) {
        return 0;
    }
    char bRet = sample[nPos];
    nPos++;
    if (nPos >= nLen) {
        nPos = 0;
    }
    return bRet;
}
