#include "BinOpen.h"
#include "Config.h"
#include "Jondra.h"
#include "Ondra.h"
#include "DirtyTiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <FL/x.H>  // Header pro X11 a Windows specifické funkce FLTK


Fl_Window* BinOpen::mainWindow = NULL;
BinOpen* BinOpen::thisDialog=NULL;

// Konstruktor dialogu
BinOpen::BinOpen(Fl_Double_Window* parent) 
    : Fl_Window(350, 270, "Upload file into memory") {
	thisDialog=this;
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
	show();
	begin();

    textBinFile = new Fl_Input(10, 10, 250, 25, "");
    btnBrowse = new Fl_Button(270, 10, 50, 25, "...");
    btnBrowse->callback(BrowseCB, this);

    checkHeaderOn = new Fl_Check_Button(10, 45, 190, 25, "The file contains a header");
    checkHeaderOn->callback(HeaderChangedCB, this);

	labelInsert = new Fl_Box(140, 65, 100, 30, "Insert from address:");
	labelInsert->align(FL_ALIGN_LEFT);

    textSavAdr = new HexInput(10, 90, 60, 25, "",4);
    textSavAdr->value("0000");

    checkRunBin = new Fl_Check_Button(9, 120, 140, 25, "Run from address:");
    textRunAdr = new HexInput(10, 140, 60, 25,"",4);
    textRunAdr->value("0000");

    checkAllRam = new Fl_Check_Button(10, 180, 75, 25, "All RAM");

    btnOK = new Fl_Button(270, 220, 60, 30, "OK");
    btnOK->callback(OKCB, this);


    resizable(this);
    end();

    // Nastavení hodnot z konfigurace
    checkHeaderOn->value(Config::bHeaderOn);
    checkRunBin->value(Config::bRunBin);
    checkAllRam->value(Config::bAllRam);
    EnableHeaderOn();
    textBinFile->value(Config::strBinFilePath.c_str());
    char buf[5];
    sprintf(buf,"%04X", Config::nBeginBinAddress);
    textSavAdr->value(buf);
    sprintf(buf,"%04X", Config::nRunBinAddress);
    textRunAdr->value(buf);
}

// Destruktor
BinOpen::~BinOpen() {}

// Otevrení souborového dialogu
void BinOpen::BrowseCB(Fl_Widget* w, void* data) {
    BinOpen* self = (BinOpen*)data;
    Fl_File_Chooser chooser(Config::getDirectoryFromPath(self->textBinFile->value()).c_str(), "*.*", Fl_File_Chooser::SINGLE, "Open Binary File");
    chooser.show();
    
    while (chooser.shown()) Fl::wait();
    
    if (chooser.value() != NULL) {
        self->textBinFile->value(chooser.value());
    }
}

void BinOpen::onClose(Fl_Widget* w, void* data) {
    BinOpen* binopen = static_cast<BinOpen*>(data);
    binopen->hide();  // Skryje okno (stejné jako zavrení)
}

// Potvrzení a zavrení dialogu
void BinOpen::OKCB(Fl_Widget* w, void* data) {
    BinOpen* self = (BinOpen*)data;
	self->result = 1;
    self->hide();
}

// Povolení/zakázání prvku podle stavu checkboxu
void BinOpen::HeaderChangedCB(Fl_Widget* w, void* data) {
    BinOpen* self = (BinOpen*)data;
    self->EnableHeaderOn();
}

void BinOpen::EnableHeaderOn() {
    bool enabled = !checkHeaderOn->value();
    textSavAdr->deactivate();
    textRunAdr->deactivate();
    checkRunBin->deactivate();
	labelInsert->deactivate();
    
    if (enabled) {
        textSavAdr->activate();
        textRunAdr->activate();
        checkRunBin->activate();
		labelInsert->activate();
    }
}

int BinOpen::showModal() {
    if (mainWindow) {
	//	((Jondra*)mainWindow)->m->til->DirtyTilesAll();
	//	((Jondra*)mainWindow)->m->til->DispUpdate(((Jondra*)mainWindow)->m->px);
        mainWindow->deactivate();  // Deaktivujeme hlavní okno
    }

    //Fl::add_handler(blockMainWindowEvents); // Blokovat kliknutí na hlavní okno
    

    show();	
    Fl::focus(this);  // Zameríme dialog


    while (visible()) {
        Fl::wait();
       
    }

   // Fl::remove_handler(blockMainWindowEvents); // Zrušíme blokaci
    

    if (mainWindow) {
        mainWindow->activate();  // Po zavrení reaktivujeme hlavní okno
    }
    
    return result;
}




int BinOpen::blockMainWindowEvents(int event) {   
    if (mainWindow && Fl::first_window() == mainWindow) {
        
        return 1; // Blokujeme event pro hlavní okno
    }
    return 0;
}


#include "Jondra.h"