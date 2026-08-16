#include "Keyboard.h"
#include "Jondra.h"
#include <FL/Fl.H>   // FLTK Eventy
#include <FL/Enumerations.H>
#include <string.h>


// Konstanty pro bity
const unsigned char sb0 = 0x01;
const unsigned char sb1 = 0x02;
const unsigned char sb2 = 0x04;
const unsigned char sb3 = 0x08;
const unsigned char sb4 = 0x10;

const unsigned char rb0 = ~sb0 & 0xFF;
const unsigned char rb1 = ~sb1 & 0xFF;
const unsigned char rb2 = ~sb2 & 0xFF;
const unsigned char rb3 = ~sb3 & 0xFF;
const unsigned char rb4 = ~sb4 & 0xFF;

Keyboard::Keyboard(unsigned char* iovect)
: Fl_Widget(0, 0, 0, 0), io(iovect), isEnabled(true), mainWindow(NULL) {
    Reset();
}

Keyboard::~Keyboard() {}

void Keyboard::Reset() {
    if (io) memset(io, 0xFF, 256);
}

void Keyboard::setKeyboardEnabled(bool enabled) {
    isEnabled = enabled;
    if (!enabled) clearKeyboardBuffer();
}

void Keyboard::clearKeyboardBuffer() {
    if (io) memset(io, 0xFF, 256);
}

void Keyboard::setMainWindow(Jondra* window) {
    mainWindow = window;
}

// Obsluha událostí klávesnice v FLTK
int Keyboard::handle(int event) {
    if (!isEnabled) return Fl_Widget::handle(event);

    int keyCode = Fl::event_key();
    bool bOndraKey = false;

    /* The Keyboard widget owns focus during normal emulation, so FLTK menu
       accelerators would otherwise never receive these key presses. */
    if (event == FL_KEYDOWN && mainWindow) {
        if (mainWindow->handleMenuShortcut(keyCode, Fl::event_state())) {
            return 1;
        }
    }

    switch (event) {
	case FL_FOCUS:
            return 1;
	case FL_UNFOCUS:
            /* Keep the keyboard widget focused during normal emulation so
               emulator shortcuts (F2/F5/F8/F12, Ctrl+D, Escape) continue
               to work.  Do not steal focus from a modal dialog such as the
               FLTK file chooser; it must receive Escape itself. */
            clearKeyboardBuffer();
            if (Fl::modal() == NULL) {
                Fl::focus(this);
            }
            return 1;
	case FL_PUSH:
	case FL_RELEASE:
		Fl::focus(this);
            return 1;
	case FL_KEYDOWN:
		keyCode = toupper(keyCode);
		switch (keyCode) {
		case 'Q': io[0] &= rb4; bOndraKey = true; break;
		case 'T': io[0] &= rb3; bOndraKey = true; break;
		case 'W': io[0] &= rb2; bOndraKey = true; break;
		case 'E': io[0] &= rb1; bOndraKey = true; break;
		case 'R': io[0] &= rb0; bOndraKey = true; break;

		case 'A': io[1] &= rb4; bOndraKey = true; break;
		case 'G': io[1] &= rb3; bOndraKey = true; break;
		case 'S': io[1] &= rb2; bOndraKey = true; break;
		case 'D': io[1] &= rb1; bOndraKey = true; break;
		case 'F': io[1] &= rb0; bOndraKey = true; break;

		case FL_Alt_L: io[2] &= rb4; bOndraKey = true; break;
		case 'V': io[2] &= rb3; bOndraKey = true; break;
		case 'Z': io[2] &= rb2; bOndraKey = true; break;
		case 'X': io[2] &= rb1; bOndraKey = true; break;
		case 'C': io[2] &= rb0; bOndraKey = true; break;

		case ' ': io[3] &= rb0; bOndraKey = true; break;
		case FL_Shift_L: io[4] &= rb4; bOndraKey = true; break;
		case FL_Tab: io[4] &= rb1; bOndraKey = true; break;
		case FL_Enter: io[5] &= rb4;bOndraKey = true; break;
		case 'H': io[5] &= rb3; bOndraKey = true; break;
		case 'L': io[5] &= rb2; bOndraKey = true; break;
		case 'K': io[5] &= rb1; bOndraKey = true; break;
		case 'J': io[5] &= rb0; bOndraKey = true; break;

		case 'P': io[6] &= rb4; bOndraKey = true; break;
		case 'Y': io[6] &= rb3; bOndraKey = true; break;
		case 'O': io[6] &= rb2; bOndraKey = true; break;
		case 'I': io[6] &= rb1; bOndraKey = true; break;
		case 'U': io[6] &= rb0; bOndraKey = true; break;

		case FL_Control_L: io[7] &= rb4; bOndraKey = true; break;
		case 'B': io[7] &= rb3; bOndraKey = true; break;
		case FL_Up: io[7] &= rb2; bOndraKey = true; break;
		case 'M': io[7] &= rb1; bOndraKey = true; break;
		case 'N': io[7] &= rb0; bOndraKey = true; break;

		case FL_Right: io[8] &= rb4; bOndraKey = true; break;
		case FL_Down: io[8] &= rb2; bOndraKey = true; break;
		case FL_Left: io[8] &= rb1; bOndraKey = true; break;

		case '0': io[9] &= rb4; bOndraKey = true; break;
		case '2': io[9] &= rb3; bOndraKey = true; break;
		case '8': io[9] &= rb2; bOndraKey = true; break;
		case '4': io[9] &= rb1; bOndraKey = true; break;
		case '6': io[9] &= rb0; bOndraKey = true; break;

		case 0x3D: io[4] &= rb2; bOndraKey = true; break; // '=' mapuje CS
		}
        if (bOndraKey && mainWindow) {
            mainWindow->processKeyboardPicturePress(keyCode);
        }
		return 1;

        case FL_KEYUP:
			keyCode = toupper(keyCode);
            switch (keyCode) {
			case 'Q': io[0] |= sb4; break;
			case 'T': io[0] |= sb3; break;
			case 'W': io[0] |= sb2; break;
			case 'E': io[0] |= sb1; break;
			case 'R': io[0] |= sb0; break;

			case 'A': io[1] |= sb4; break;
			case 'G': io[1] |= sb3; break;
			case 'S': io[1] |= sb2; break;
			case 'D': io[1] |= sb1; break;
			case 'F': io[1] |= sb0; break;

			case FL_Alt_L: io[2] |= sb4; bOndraKey = true; break;
			case 'V': io[2] |= sb3; bOndraKey = true; break;
			case 'Z': io[2] |= sb2; bOndraKey = true; break;
			case 'X': io[2] |= sb1; bOndraKey = true; break;
			case 'C': io[2] |= sb0; bOndraKey = true; break;

			case ' ': io[3] |= sb0; bOndraKey = true; break;
			case FL_Shift_L: io[4] |= sb4; bOndraKey = true; break;
			case FL_Tab: io[4] |= sb1; bOndraKey = true; break;
			case FL_Enter: io[5] |= sb4; bOndraKey = true; break;
			case 'H': io[5] |= sb3; bOndraKey = true; break;
			case 'L': io[5] |= sb2; bOndraKey = true; break;
			case 'K': io[5] |= sb1; bOndraKey = true; break;
			case 'J': io[5] |= sb0; bOndraKey = true; break;

			case 'P': io[6] |= sb4; bOndraKey = true; break;
			case 'Y': io[6] |= sb3; bOndraKey = true; break;
			case 'O': io[6] |= sb2; bOndraKey = true; break;
			case 'I': io[6] |= sb1; bOndraKey = true; break;
			case 'U': io[6] |= sb0; bOndraKey = true; break;

			case FL_Control_L: io[7] |= sb4; bOndraKey = true; break;
			case 'B': io[7] |= sb3; bOndraKey = true; break;
			case FL_Up: io[7] |= sb2; bOndraKey = true; break;
			case 'M': io[7] |= sb1; bOndraKey = true; break;
			case 'N': io[7] |= sb0; bOndraKey = true; break;

			case FL_Right: io[8] |= sb4; bOndraKey = true; break;
			case FL_Down: io[8] |= sb2; bOndraKey = true; break;
			case FL_Left: io[8] |= sb1; bOndraKey = true; break;

			case '0': io[9] |= sb4; bOndraKey = true; break;
			case '2': io[9] |= sb3; bOndraKey = true; break;
			case '8': io[9] |= sb2; bOndraKey = true; break;
			case '4': io[9] |= sb1; bOndraKey = true; break;
			case '6': io[9] |= sb0; bOndraKey = true; break;

			case 0x3D: io[4] |= sb2; bOndraKey = true; break;
            }
            if (mainWindow) {
                mainWindow->processKeyboardPictureRelease(keyCode);
            }
            return 1;
    }

    return Fl_Widget::handle(event);
}

void Keyboard::draw() {
    // Tento widget nic nekreslí, takze metoda je prazdna
}
