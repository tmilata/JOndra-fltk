#include "FlatButton.h"
#include "Ondra.h"
#include "Screen.h"
#include "FL/Fl_Tooltip.H"
#include "DirtyTiles.h"


int FlatButton::handle(int event) {
    switch (event) {
	case FL_ENTER: // Myš najede na tlacítko
		if (type() != FL_TOGGLE_BUTTON || !value()) {
			color(FL_LIGHT1);
			box(FL_UP_BOX);
		}
		redraw();
		//return 1;
		return Fl_Button::handle(event);

	case FL_LEAVE: // Myš opustí tlacítko
		if (type() == FL_TOGGLE_BUTTON && value()) {
			box(FL_DOWN_BOX);  // Zachovat zamácknutý stav pri toggle režimu
			color(FL_LIGHT1);
		} else {
			box(FL_FLAT_BOX);
			color(FL_BACKGROUND_COLOR);
		}
		if (Fl_Tooltip::current()) {
		if (Ondra::machine) {
			Ondra::machine->til->DispDirtyRectTiles(0, 0, 320, 15);
		}
		}
		redraw();
		//return 1;
		return Fl_Button::handle(event);

	case FL_PUSH: // Stisknutí tlacítka
		imageOffsetX = 1; // Posuneme obrázek dolu a doprava
		imageOffsetY = 1;
		box(FL_DOWN_BOX);
		redraw();
		if (type() == FL_TOGGLE_BUTTON) {
			value(!value()); // Prepnutí stavu pomocí nativní metody
		}
		if (Ondra::machine) {
			Ondra::machine->til->DispDirtyRectTiles(0, 0, 320, 15);
		}
		//	redraw();
		return Fl_Button::handle(event);

	case FL_RELEASE: // Uvolnení tlacítka
		imageOffsetX = 0; // Vrátíme obrázek zpet na puvodní pozici
		imageOffsetY = 0;

		if (type() == FL_TOGGLE_BUTTON) {
			if (value()) {
				box(FL_DOWN_BOX);  // Stisknutý stav
				//color(FL_WHITE);

			} else {
				box(FL_FLAT_BOX);  // Normální stav
				//color(FL_BACKGROUND_COLOR);
			}
		} else {
			// Normální tlacítko (netoggle)
			if (Fl::belowmouse() == this) {
				box(FL_UP_BOX);
				color(FL_LIGHT1);
			} else {
				box(FL_FLAT_BOX);
				color(FL_BACKGROUND_COLOR);
			}
		}
		//redraw();
		return Fl_Button::handle(event);

	case FL_FOCUS:
	case FL_UNFOCUS:
		return 0;
	default:
		return Fl_Button::handle(event);
    }
}

void FlatButton::updateState() {
    if (type() == FL_TOGGLE_BUTTON) {
        if (value()) {
			box(FL_DOWN_BOX);  // Stisknutý stav
			//color(FL_WHITE);

		} else {
			box(FL_FLAT_BOX);  // Normální stav
			//color(FL_BACKGROUND_COLOR);
		}
        redraw();
        handle(FL_RELEASE);  // Simulace uvolnení tlacítka pro správný vizuální stav
    }
}


void FlatButton::draw() {
    Fl_Image* img = image();  // Ulo×Ýme obrßzek
    image(0);                 // Docasne odstranÝme obrßzek z tlacÝtka
    Fl_Button::draw();        // VykreslÝme tlacÝtko bez obrßzku
    image(img);               // VrßtÝme obrßzek do tlacÝtka

    if (img) {
        // Korekce pozice obrßzku rucne
        int offsetX = x() + imageOffsetX + (w() - img->w()) / 2;
        int offsetY = y() + imageOffsetY + (h() - img->h()) / 2;

        img->draw(offsetX, offsetY);
    }
}

