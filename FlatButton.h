#ifndef FLAT_H
#define FLAT_H

#include <FL/Fl_Button.H>
#include <FL/Fl.H>        // Potrebné pro Fl::belowmouse()
#include <FL/Fl_Image.H>  // Oprava chyby s Fl_Image
#include <FL/fl_draw.H>

class FlatButton : public Fl_Button {
public:
    FlatButton(int X, int Y, int W, int H, const char *L = 0)
        : Fl_Button(X, Y, W, H, L), imageOffsetX(0), imageOffsetY(0) {
        box(FL_FLAT_BOX);  // Ploché tlaèítko
        color(FL_BACKGROUND_COLOR);

    }


    int handle(int event);
	void updateState();

    void draw(); // Správnì deklarovaná metoda
private:
    int imageOffsetX, imageOffsetY; // Posun obrázku pøi stisku
};

#endif
