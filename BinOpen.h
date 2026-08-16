#ifndef BINOPEN_H
#define BINOPEN_H

#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_File_Chooser.H>
#include "HexInput.h"

class BinOpen : public Fl_Window {
private:
	int result;
    static Fl_Window* mainWindow;

    Fl_Button* btnOK;
    
	
    static void BrowseCB(Fl_Widget* w, void* data);
    static void OKCB(Fl_Widget* w, void* data);
	static void onClose(Fl_Widget* w, void* data);
    static void HeaderChangedCB(Fl_Widget* w, void* data);
	
    void EnableHeaderOn();
	
public:
	Fl_Input* textBinFile;
    Fl_Button* btnBrowse;
    Fl_Check_Button* checkHeaderOn;
    HexInput* textSavAdr;
	Fl_Box *labelInsert;
    Fl_Check_Button* checkRunBin;
    HexInput* textRunAdr;
    Fl_Check_Button* checkAllRam;
    
    BinOpen(Fl_Double_Window* parent);
    ~BinOpen();
    int showModal();
	static int blockMainWindowEvents(int event);
	static BinOpen* thisDialog;
	
};

#endif // BINOPEN_H
