#ifndef TAPFILE_H
#define TAPFILE_H

#include <stdio.h>

// Jednoduchá verze "bufferu", který obsahuje bitovì zpracovaná data
class TapBuffer {
public:
    unsigned char* byteBuffer;
    int nWBufferPosition;  // write buffer position
    int nWBitPosition;     // write bit position (0..7)
    int nRBufferPosition;  // read buffer position
    int nRBitPosition;     // read bit position
    bool bAllBufferReaded; // indikuje, že už byl pøeèten celý buffer

    // Konstruktor – alokuje buffer o dané velikosti a inicializuje hodnoty
    TapBuffer(int nSize);
    ~TapBuffer();

    // Pomocne metody pro sestaveni bitoveho prubehu TAP dat
    void fillInitPause();  // pøidá sérii nul pøed pilotní signál
    void fillPilot();      // vloží pilotní signál (sadu jednièek) do bufferu
    void putByte(unsigned char value);  // vloží jeden bajt do bufferu v "Ondrovì" notaci
    void IncWBitPosition(); // inkrementuje write bit position
    bool readBit();         // vrací další bit z bufferu
};

class TapFile {
public:
    // Statická chybová hláška – maximálnì 255 znakù
    static char strErrorMsg[256];

    // Statická metoda pro otevøení TAP souboru a syntaktickou kontrolu
    // Vstup: cesta k souboru (nulovì ukonèený øetìzec)
    // Výstup: ukazatel na novì vytvoøenou instanci TapFile, nebo vyvolá chybu
    static TapFile* openTapFile(const char* filename);

    // Vrací další bit z interního bufferu (volá fillBuffer, pokud je potøeba)
    bool readNextBit();

    // Generuje "frame" – podle interní logiky (vrací true/false)
    bool generateFrame();

    TapFile();
    ~TapFile();

    // Veøejná pøíznaková promìnná: indikuje, zda byl soubor zcela pøeèten
    bool bFinished;
    // Urèuje, zda soubor využívá CRLF formát
    bool bCrLfType;
    // Celková délka souboru
    long nMaxLen;

private:
    // Interní promìnné
    char* filename;      // kopie cesty k souboru (alokovaná dynamicky)
    FILE* streamFile;    // stream pro ètení souboru
    TapBuffer* tapeBuffer; // aktuální buffer s bitovou reprezentací dat

    int nFrameSize;      // velikost frame (napr. 70)
    int nFrameDecrement; // pomocná promìnná pro generování frame
    int nActualBit;      // aktuálnì naètený bit; výchozí hodnota 3 znamená, že ještì není nastaven

    // Interní metoda, která naète další blok dat ze souboru a naplní tapeBuffer
    // Vrací true, pokud se naèetly nìjaké nové data
    bool fillBuffer();
};

#endif // TAPFILE_H
