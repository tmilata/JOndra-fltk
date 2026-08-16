#ifndef CONFIG_H
#define CONFIG_H

#include <string>

class Config {
public:
    static std::string strBinFilePath;
    static int nBeginBinAddress;
    static bool bRunBin;
    static int nRunBinAddress;
    static bool bAllRam;
    static bool bHeaderOn;
    static bool bBP1;
    static int nBP1Address;
    static bool bBP2;
    static int nBP2Address;
    static bool bBP3;
    static int nBP3Address;
    static bool bBP4;
    static int nBP4Address;
    static bool bBP5;
    static int nBP5Address;
    static bool bBP6;
    static int nBP6Address;
    static bool bBP7;
    static int nBP7Address;
    static int nMemAddress;
    static bool bShowCode;
    static bool bEnableTimeline;
    static bool bAudio;
    static bool bMelodik;
    static bool bFullscreen;
    static bool bScanlines;
    static std::string strSaveBinFilePath;
    static int nSaveFromAddress;
    static int nSaveToAddress;
    static std::string strTapFilePath;
    static std::string strSnapFilePath;
    static std::string strShotFilePath;
    static std::string strRomAFilePath;
    static std::string strRomBFilePath;
    static std::string strRomDirectory;
    static int nRomType;
	
    static std::string strBasicA;
    static std::string strBasicB;
    static std::string strTeslaA;
    static std::string strTeslaB;
    static std::string strViLiA;
    static std::string strViLiB;


    static void SaveConfig();
    static void LoadConfig();
	

    static bool getAudio();
    static void setAudio(bool value);
    static bool getMelodik();
    static void setMelodik(bool value);
    static bool getFullscreen();
    static void setFullscreen(bool value);
    static bool getScanlines();
    static void setScanlines(bool value);
    static void setRomType(int type);
    static int getRomType();
    static void setRomA(const std::string& path);
    static std::string getRomA();
    static void setRomB(const std::string& path);
    static std::string getRomB();
    static std::string getRomsDirectory();
    static std::string getMyPath();
	static std::string correctFullPath(const char* filename);
	static bool atob(const char* str);
	static std::string getDirectoryFromPath(const std::string& filePath);
};

#endif // CONFIG_H
