#include "HexInput.h"
#include <FL/Fl.H>
#include <ctype.h>  // Pro MSVC 6.0
#include <string>
#include <string.h>

HexInput::HexInput(int X, int Y, int W, int H, const char *L, int maxLen)
    : Fl_Input(X, Y, W, H, L), maxLength(maxLen) {
}

int HexInput::handle(int event) {
    switch (event) {
        case FL_KEYDOWN: {
            int key = Fl::event_key();
            int cursorPos = this->position();  // Aktuální pozice kurzoru
            std::string currentText = this->value() ? this->value() : "";

			// **Povolit Ctrl+V, aby mohl být vyvolán FL_PASTE**
            if (Fl::event_state(FL_CTRL) && key == 'v') {
                return Fl_Input::handle(event);
            }

            // **Povolit Ctrl+C pro kopírování celého obsahu**
            if (Fl::event_state(FL_CTRL) && key == 'c') {
                Fl::copy(this->value(), strlen(this->value()), 1);  // 1 = výchozí clipboard
                return 1;
            }


            // **Povolené klávesy**: 0-9, A-F
            if ((key >= '0' && key <= '9') ||
                (key >= 'a' && key <= 'f') ||
                (key >= 'A' && key <= 'F')) {

                if (cursorPos >= maxLength) {
                    return 1;  // Zabránení prekrocení délky
                }

                if (key >= 'a' && key <= 'f') {
                    key = toupper(key);  // Konverze na velká písmena
                }

                // Prepisování znaku na aktuální pozici kurzoru
                if (cursorPos < currentText.length()) {
                    currentText[cursorPos] = key;
                } else {
                    currentText += key;
                }

                this->value(currentText.c_str());
                this->position(cursorPos + 1);  // Posun kurzoru doprava
                return 1;
            }

            // **Backspace: nahrazuje znak PRED kurzorem nulou a posune kurzor DOLEVA**
            if (key == FL_BackSpace && cursorPos > 0) {
                currentText[cursorPos - 1] = '0';
                this->value(currentText.c_str());
                this->position(cursorPos - 1);  // Posun DOLEVA
                return 1;
            }

            // **Delete: nahrazuje znak NA pozici kurzoru nulou a posune kurzor DOPRAVA**
            if (key == FL_Delete && cursorPos < currentText.length()) {
                currentText[cursorPos] = '0';
                this->value(currentText.c_str());
                if (cursorPos < currentText.length() - 1) {
                    this->position(cursorPos + 1);  // Posun DOPRAVA
                }
                return 1;
            }

            // **Šipky vlevo/vpravo zustávají uvnitr inputboxu**
            if (key == FL_Left) {
                if (cursorPos > 0) {
                    this->position(cursorPos - 1);
                }
                return 1;  // Zamezí presunu fokusu ven
            }

            if (key == FL_Right) {
                if (cursorPos < currentText.length()) {
                    this->position(cursorPos + 1);
                }
                return 1;  // Zamezí presunu fokusu ven
            }

            // **Klávesa Home - skok na zacátek textu**
            if (key == FL_Home) {
                this->position(0);
                return 1;
            }

            // **Klávesa End - skok na konec textu**
            if (key == FL_End) {
                this->position(currentText.length());
                return 1;
            }

            // **Povolit Tab a Shift+Tab**
            if (key == FL_Tab) {
                return Fl_Input::handle(event);
            }
            if (Fl::event_state(FL_SHIFT) && key == FL_Tab) {
                return Fl_Input::handle(event);
            }


            return 1;  // **Vše ostatní zahodíme**
        }

        case FL_PASTE: {
            /* Match JNumberTextField: optional '#', hex only, fixed width. */
            const char* clipboard = Fl::event_text();
            std::string pasted;
            unsigned int i;

            if (!clipboard) return 1;
            pasted = clipboard;
            if (!pasted.empty() && pasted[0] == '#') {
                pasted.erase(0, 1);
            }
            if (pasted.empty() || pasted.length() > (unsigned int)maxLength) {
                return 1;
            }

            for (i = 0; i < pasted.length(); ++i) {
                char ch = pasted[i];
                if (!((ch >= '0' && ch <= '9') ||
                      (ch >= 'A' && ch <= 'F') ||
                      (ch >= 'a' && ch <= 'f'))) {
                    return 1;
                }
                if (ch >= 'a' && ch <= 'f') {
                    pasted[i] = (char)toupper(ch);
                }
            }

            while (pasted.length() < (unsigned int)maxLength) {
                std::string padded("0");
                padded.append(pasted);
                pasted = padded;
            }
            this->value(pasted.c_str());
            this->position((int)pasted.length());
            return 1;
        }

        case FL_UNFOCUS: {
            int result = Fl_Input::handle(event);
            /* Breakpoint inputs commit on focus loss. */
            do_callback();
            return result;
        }
    }
    return Fl_Input::handle(event);  // Predání ostatních eventu dál
}
