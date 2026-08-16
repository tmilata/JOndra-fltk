#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <FL/Fl_Widget.H>  // FLTK widgety
#include <string>         // memset
#include <ctype.h>          // toupper

class Jondra;

class Keyboard : public Fl_Widget {
public:
    Keyboard(unsigned char* iovect);
    ~Keyboard();

    void Reset();
    void setKeyboardEnabled(bool enabled);
    void clearKeyboardBuffer();
    void setMainWindow(Jondra* window);

    int handle(int event);  // Prepsání FLTK událostí
	//povinna metoda
	void draw();

private:
    unsigned char* io;
    bool isEnabled;
    Jondra* mainWindow;
};

#endif // KEYBOARD_H
