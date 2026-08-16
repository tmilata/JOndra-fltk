#include "Config.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#endif
#include "Debug.h"

std::string Config::strBinFilePath = "";
int Config::nBeginBinAddress = 0;
bool Config::bRunBin = false;
int Config::nRunBinAddress = 0;
bool Config::bAllRam = true;
bool Config::bHeaderOn = true;
bool Config::bBP1 = false;
int Config::nBP1Address = 0;
bool Config::bBP2 = false;
int Config::nBP2Address = 0;
bool Config::bBP3 = false;
int Config::nBP3Address = 0;
bool Config::bBP4 = false;
int Config::nBP4Address = 0;
bool Config::bBP5 = false;
int Config::nBP5Address = 0;
bool Config::bBP6 = false;
int Config::nBP6Address = 0;
bool Config::bBP7 = false;
int Config::nBP7Address = 0;
int Config::nMemAddress = 0;
bool Config::bShowCode = false;
bool Config::bEnableTimeline = false;
bool Config::bAudio = true;
bool Config::bMelodik = true;
bool Config::bFullscreen = false;
bool Config::bScanlines = false;
std::string Config::strSaveBinFilePath = "";
int Config::nSaveFromAddress = 0;
int Config::nSaveToAddress = 0;
std::string Config::strTapFilePath = "";
std::string Config::strSnapFilePath = "";
std::string Config::strShotFilePath = "";
std::string Config::strRomAFilePath = "";
std::string Config::strRomBFilePath = "";
std::string Config::strRomDirectory = "roms/";
int Config::nRomType = 2;
std::string Config::strBasicA = "Ondra_BASICEXP_V5_a.rom";
std::string Config::strBasicB = "Ondra_BASICEXP_V5_b.rom";
std::string Config::strTeslaA = "Ondra_TESLA_V5_a.rom";
std::string Config::strTeslaB = "Ondra_TESLA_V5_b.rom";
std::string Config::strViLiA = "Ondra_ViLi_v27_a.rom";
std::string Config::strViLiB = "Ondra_ViLi_v27_b.rom";

bool Config::getAudio() { return bAudio; }
void Config::setAudio(bool value) { bAudio = value; }

bool Config::getMelodik() { return bMelodik; }
void Config::setMelodik(bool value) { bMelodik = value; }

bool Config::getFullscreen() { return bFullscreen; }
void Config::setFullscreen(bool value) { bFullscreen = value; }

bool Config::getScanlines() { return bScanlines; }
void Config::setScanlines(bool value) { bScanlines = value; }

void Config::setRomType(int type) { nRomType = type; }
int Config::getRomType() { return nRomType; }

void Config::setRomA(const std::string& path) { strRomAFilePath = path; }
std::string Config::getRomA() { return strRomAFilePath; }

void Config::setRomB(const std::string& path) { strRomBFilePath = path; }
std::string Config::getRomB() { return strRomBFilePath; }

std::string Config::getRomsDirectory() { return strRomDirectory; }

bool Config::atob(const char* str) {
    if (!str) return false;
	
#ifdef _MSC_VER  // MSVC 6.0 (Windows)
    return (str[0] == '1' || _stricmp(str, "true") == 0);
#else  // Linux (GCC, Clang)
    return (str[0] == '1' || strcasecmp(str, "true") == 0);
#endif
}

std::string Config::getDirectoryFromPath(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return filePath.substr(0, lastSlash);
    }
    return filePath;
}

std::string Config::getMyPath() {
    char buffer[1024];
	
#ifdef _WIN32
    GetModuleFileNameA(NULL, buffer, sizeof(buffer));
#else
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
    }
#endif
	
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
	return (pos == std::string::npos) ? "" : path.substr(0, pos)+'/';
}

std::string Config::correctFullPath(const char* filename) {
    if (!filename || filename[0] == '\0') {
        return "";
    }
	
    std::string filePath(filename);
	
    if (filePath.find('\\') != std::string::npos || filePath.find('/') != std::string::npos) {
        return filePath;
    }
	
    std::string fullPath = Config::getMyPath() + filePath;
    return fullPath;
}

void Config::SaveConfig() {
    FILE* file = fopen(std::string(getMyPath() + "Jondra.config").c_str(), "w");
    if (!file) return;
	
    fprintf(file, "BINFILEPATH=%s\n", strBinFilePath.c_str());
    fprintf(file, "BEGINBINADDRESS=%d\n", nBeginBinAddress);
    fprintf(file, "BRUNBIN=%d\n", bRunBin);
    fprintf(file, "RUNBINADDRESS=%d\n", nRunBinAddress);
    fprintf(file, "BALLRAM=%d\n", bAllRam);
    fprintf(file, "BHEADER=%d\n", bHeaderOn);
    fprintf(file, "BP1CHCK=%d\n", bBP1);
    fprintf(file, "BP1ADDRESS=%d\n", nBP1Address);
    fprintf(file, "BP2CHCK=%d\n", bBP2);
    fprintf(file, "BP2ADDRESS=%d\n", nBP2Address);
    fprintf(file, "BP3CHCK=%d\n", bBP3);
    fprintf(file, "BP3ADDRESS=%d\n", nBP3Address);
    fprintf(file, "BP4CHCK=%d\n", bBP4);
    fprintf(file, "BP4ADDRESS=%d\n", nBP4Address);
    fprintf(file, "BP5CHCK=%d\n", bBP5);
    fprintf(file, "BP5ADDRESS=%d\n", nBP5Address);
    fprintf(file, "BP6CHCK=%d\n", bBP6);
    fprintf(file, "BP6ADDRESS=%d\n", nBP6Address);
    fprintf(file, "BP7CHCK=%d\n", bBP7);
    fprintf(file, "BP7ADDRESS=%d\n", nBP7Address);
    fprintf(file, "MEMADDRESS=%d\n", nMemAddress);
    fprintf(file, "BSHOWCODE=%d\n", bShowCode);
    fprintf(file, "BTIMELINE=%d\n", bEnableTimeline);
    fprintf(file, "AUDIO=%d\n", bAudio);
    fprintf(file, "MELODIK=%d\n", bMelodik);
    fprintf(file, "FULLSCREEN=%d\n", bFullscreen);
    fprintf(file, "SCANLINES=%d\n", bScanlines);
    fprintf(file, "BINSAVEFILEPATH=%s\n", strSaveBinFilePath.c_str());
    fprintf(file, "BINSAVEADDRESSFROM=%d\n", nSaveFromAddress);
    fprintf(file, "BINSAVEADDRESSTO=%d\n", nSaveToAddress);
    fprintf(file, "TAPFILEPATH=%s\n", strTapFilePath.c_str());
    fprintf(file, "SNAFILEPATH=%s\n", strSnapFilePath.c_str());
    fprintf(file, "SHOTFILEPATH=%s\n", strShotFilePath.c_str());
    fprintf(file, "ROMAFILEPATH=%s\n", strRomAFilePath.c_str());
    fprintf(file, "ROMBFILEPATH=%s\n", strRomBFilePath.c_str());
    fprintf(file, "ROMTYPE=%d\n", nRomType);
	
    fclose(file);
}

void Config::LoadConfig() {
    std::string configPath = getMyPath() + "Jondra.config";
    FILE* file = fopen(configPath.c_str(), "r");
    
    if (!file) {
       
        return;
    }

    
    char line[512];  // Radek pro cteni ze souboru
    char key[64], value[256];

    while (fgets(line, sizeof(line), file)) {
        // Odstraneni noveho radku na konci
        line[strcspn(line, "\r\n")] = 0;
        
        if (sscanf(line, "%63[^=]=%255s", key, value) == 2) {
    
			
            if (strcmp(key, "BINFILEPATH") == 0) strBinFilePath = value;
            else if (strcmp(key, "BEGINBINADDRESS") == 0) nBeginBinAddress = atoi(value);
            else if (strcmp(key, "BRUNBIN") == 0) bRunBin = atob(value);
            else if (strcmp(key, "RUNBINADDRESS") == 0) nRunBinAddress = atoi(value);
            else if (strcmp(key, "BALLRAM") == 0) bAllRam = atob(value);
            else if (strcmp(key, "BHEADER") == 0) bHeaderOn = atob(value);
            else if (strcmp(key, "BP1CHCK") == 0) bBP1 = atob(value);
            else if (strcmp(key, "BP1ADDRESS") == 0) nBP1Address = atoi(value);
            else if (strcmp(key, "BP2CHCK") == 0) bBP2 = atob(value);
            else if (strcmp(key, "BP2ADDRESS") == 0) nBP2Address = atoi(value);
            else if (strcmp(key, "BP3CHCK") == 0) bBP3 = atob(value);
            else if (strcmp(key, "BP3ADDRESS") == 0) nBP3Address = atoi(value);
            else if (strcmp(key, "BP4CHCK") == 0) bBP4 = atob(value);
            else if (strcmp(key, "BP4ADDRESS") == 0) nBP4Address = atoi(value);
            else if (strcmp(key, "BP5CHCK") == 0) bBP5 = atob(value);
            else if (strcmp(key, "BP5ADDRESS") == 0) nBP5Address = atoi(value);
            else if (strcmp(key, "BP6CHCK") == 0) bBP6 = atob(value);
            else if (strcmp(key, "BP6ADDRESS") == 0) nBP6Address = atoi(value);
            else if (strcmp(key, "BP7CHCK") == 0) bBP7 = atob(value);
            else if (strcmp(key, "BP7ADDRESS") == 0) nBP7Address = atoi(value);
            else if (strcmp(key, "MEMADDRESS") == 0) nMemAddress = atoi(value);
            else if (strcmp(key, "BSHOWCODE") == 0) bShowCode = atob(value);
            else if (strcmp(key, "BTIMELINE") == 0) bEnableTimeline = atob(value);
            else if (strcmp(key, "AUDIO") == 0) bAudio = atob(value);
            else if (strcmp(key, "MELODIK") == 0) bMelodik = atob(value);
            else if (strcmp(key, "FULLSCREEN") == 0) bFullscreen = atob(value);
            else if (strcmp(key, "SCANLINES") == 0) bScanlines = atob(value);
            else if (strcmp(key, "BINSAVEFILEPATH") == 0) strSaveBinFilePath = value;
            else if (strcmp(key, "BINSAVEADDRESSFROM") == 0) nSaveFromAddress = atoi(value);
            else if (strcmp(key, "BINSAVEADDRESSTO") == 0) nSaveToAddress = atoi(value);
            else if (strcmp(key, "TAPFILEPATH") == 0) strTapFilePath = value;
            else if (strcmp(key, "SNAFILEPATH") == 0) strSnapFilePath = value;
            else if (strcmp(key, "SHOTFILEPATH") == 0) strShotFilePath = value;
            else if (strcmp(key, "ROMAFILEPATH") == 0) strRomAFilePath = value;
            else if (strcmp(key, "ROMBFILEPATH") == 0) strRomBFilePath = value;
            else if (strcmp(key, "ROMTYPE") == 0) nRomType = atoi(value);
        } 
    }
	
    fclose(file);
}

