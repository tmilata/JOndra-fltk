#ifndef SOUNDSAMPLE_H
#define SOUNDSAMPLE_H

class SoundSample {
public:
	char* sample;   // Ukazatel na naètená data (sample)
    int nLen;       // Délka naèteného vzorku
    int nPos;       // Aktuální pozice v poli
    // Konstruktor, který pøijme data jako pole bajtù a jeho délku.
    SoundSample(const char* inData, int length);

    // Konstruktor, který naète data ze souboru (místo zdroje).
    SoundSample(const char* filePath);

    ~SoundSample();

    // Metoda pro naètení dat ze souboru.
    void loadFromFile(const char* filePath);

    // Resetuje pozici na zaèátek.
    void resetPosition();

    // Vrací další bajt a posouvá pozici; pokud dosáhne konce, vrací se na zaèátek.
    char getNextByte();

    // Nepovinná metoda pro získání délky vzorku.
    int getLength() const { return nLen; }
};

#endif // SOUNDSAMPLE_H
