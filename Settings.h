#ifndef SETTINGS_H
#define SETTINGS_H

#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>

class Settings : public Fl_Window {
private:
	int result;
    

    static void onRomTypeChanged(Fl_Widget *w, void *data);
    static void onBrowseRomA(Fl_Widget *w, void *data);
    static void onBrowseRomB(Fl_Widget *w, void *data);
    static void onOk(Fl_Widget *w, void *data);
	static void onClose(Fl_Widget* w, void* data);
	

    void setCustomControlsEnabled(bool enabled);
	static Fl_Double_Window* mainWindow;

public:
	Fl_Round_Button *radioBasic, *radioTesla, *radioVili, *radioCustom;
    Fl_Input *textRomA, *textRomB;
    Fl_Button *buttonRomA, *buttonRomB, *buttonOk;
    Fl_Check_Button *checkSound, *checkMelodik, *checkFullscreen, *checkScanlines;
    Settings(Fl_Double_Window* parent);
    void showDialog();
	int showModal();
	static int blockMainWindowEvents(int event);
};

#endif // SETTINGS_H
