#include "Settings.h"
#include "Config.h"
#include "Jondra.h"
#include "Ondra.h"
#include "DirtyTiles.h"
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#include "Settings.h"

Fl_Double_Window* Settings::mainWindow = NULL;

Settings::Settings(Fl_Double_Window* parent) 
    : Fl_Window(450, 400, "Settings") {
	set_non_modal();
	mainWindow = parent;
	result = 0;
    callback(onClose, this);  // Zavírací funkce
    
	if (mainWindow) {
        int parent_x = mainWindow->x();
        int parent_y = mainWindow->y();
        int parent_w = mainWindow->w();
        int parent_h = mainWindow->h();
		
        // **Vypocítáme souradnice pro centrování**
        int new_x = parent_x + (parent_w - w()) / 2;
        int new_y = parent_y + (parent_h - h()) / 2;
		
        // **Nastavíme nové souradnice okna**
        position(new_x, new_y);
    }
	
    begin();

    // --- Radio buttony pro ROM typy ---
    radioBasic = new Fl_Round_Button(20, 20, 100, 25, "Basic ROM");
    radioBasic->type(FL_RADIO_BUTTON);
    radioBasic->callback(onRomTypeChanged, this);

    radioTesla = new Fl_Round_Button(20, 50, 100, 25, "Tesla ROM");
    radioTesla->type(FL_RADIO_BUTTON);
    radioTesla->callback(onRomTypeChanged, this);

    radioVili = new Fl_Round_Button(20, 80, 120, 25, "SSM (ViLi) ROM");
    radioVili->type(FL_RADIO_BUTTON);
    radioVili->callback(onRomTypeChanged, this);

    radioCustom = new Fl_Round_Button(20, 110, 120, 25, "Custom ROM");
    radioCustom->type(FL_RADIO_BUTTON);
    radioCustom->callback(onRomTypeChanged, this);

    // --- ROM Slot A ---
    textRomA = new Fl_Input(130, 150, 250, 25, "ROM Slot A:");
    buttonRomA = new Fl_Button(390, 150, 30, 25, "...");
    buttonRomA->callback(onBrowseRomA, this);

    // --- ROM Slot B ---
    textRomB = new Fl_Input(130, 180, 250, 25, "ROM Slot B:");
    buttonRomB = new Fl_Button(390, 180, 30, 25, "...");
    buttonRomB->callback(onBrowseRomB, this);

    // --- Checkboxy ---
    checkSound = new Fl_Check_Button(20, 220, 380, 25, "Enable Sound (Disable if emulation is slow)");
    checkMelodik = new Fl_Check_Button(20, 250, 200, 25, "Enable Melodik Module");
    checkFullscreen = new Fl_Check_Button(20, 280, 250, 25, "Launch in Fullscreen (Toggle F12)");
    checkScanlines = new Fl_Check_Button(20, 310, 200, 25, "Enable Scanlines");

    // --- OK tlacítko ---
    buttonOk = new Fl_Button(160, 350, 100, 30, "OK");
    buttonOk->callback(onOk, this);

    end();

    // Nactení hodnot z konfigurace
    textRomA->value(Config::getRomA().c_str());
    textRomB->value(Config::getRomB().c_str());
    switch(Config::getRomType()) {
        case 0: radioBasic->set(); break;
        case 1: radioTesla->set(); break;
        case 2: radioVili->set(); break;
        case 3: radioCustom->set(); setCustomControlsEnabled(true); break;
    }
    checkSound->value(Config::getAudio());
    checkMelodik->value(Config::getMelodik());
    checkFullscreen->value(Config::getFullscreen());
    checkScanlines->value(Config::getScanlines());

    setCustomControlsEnabled(radioCustom->value()!=0);
}

int Settings::showModal() {
    
    if (mainWindow) {
	//	((Jondra*)mainWindow)->m->til->DirtyTilesAll();
	//	((Jondra*)mainWindow)->m->til->DispUpdate(((Jondra*)mainWindow)->m->px);
        mainWindow->deactivate();  // Deaktivujeme hlavní okno
    }
 //Fl::add_handler(blockMainWindowEvents); // Blokovat kliknutí na hlavní okno
    show();
    
    while (visible()) {
        Fl::wait();
    }

    if (mainWindow) {
        mainWindow->activate();
    }

//    Fl::remove_handler(blockMainWindowEvents); // Po zavrení zrušíme blokaci
	return result;
}

int Settings::blockMainWindowEvents(int event) {
    if (mainWindow && Fl::first_window() == mainWindow) {
        return 1; // Vrátíme 1 = FLTK nebude zpracovávat klikání do hlavního okna
    }
    return 0;
}

void Settings::onClose(Fl_Widget* w, void* data) {
    Settings* settings = static_cast<Settings*>(data);
    settings->hide();  // Skryje okno (stejné jako zavrení)
}

void Settings::setCustomControlsEnabled(bool enabled) {
    textRomA->deactivate();
    buttonRomA->deactivate();
    textRomB->deactivate();
    buttonRomB->deactivate();
    if (enabled) {
        textRomA->activate();
        buttonRomA->activate();
        textRomB->activate();
        buttonRomB->activate();
    }
}

void Settings::onRomTypeChanged(Fl_Widget *w, void *data) {
    Settings *dlg = static_cast<Settings*>(data);
    dlg->setCustomControlsEnabled(dlg->radioCustom->value()!=0);
}

void Settings::onBrowseRomA(Fl_Widget *w, void *data) {
    Settings *dlg = static_cast<Settings*>(data);
    const char *filename = fl_file_chooser("Choose ROM file for slot A", "*.*", NULL);
    if (filename) dlg->textRomA->value(filename);
}

void Settings::onBrowseRomB(Fl_Widget *w, void *data) {
    Settings *dlg = static_cast<Settings*>(data);
    const char *filename = fl_file_chooser("Choose ROM file for slot B", "*.*", NULL);
    if (filename) dlg->textRomB->value(filename);
}

void Settings::onOk(Fl_Widget *w, void *data) {
    Settings *dlg = static_cast<Settings*>(data);

	dlg->result = 1;
    dlg->hide();
}

void Settings::showDialog() {
    show();
}
