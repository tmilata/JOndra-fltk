#ifndef HEXINPUT_H
#define HEXINPUT_H

#include <FL/Fl_Input.H>

class HexInput : public Fl_Input {
public:
    HexInput(int X, int Y, int W, int H, const char *L = 0, int maxLen = 8);
    
    void setMaxLength(int len) { maxLength = len; }
    int handle(int event);  // Pøepsání metody pro vlastní zpracování vstupu

private:
    int maxLength;
};

#endif
