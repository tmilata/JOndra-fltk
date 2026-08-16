#ifndef JONDRA_H
#define JONDRA_H

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Tooltip.H>
#include "FlatButton.h"
#include "CustomMenuBar.h"
#include "Screen.h"

#ifdef _WIN32
 #ifndef DWORD_PTR
	#define DWORD_PTR DWORD
 #endif
#endif

class Ondra;
class Keyboard;
class DebuggerWindow;
class Jondra;

class KeyboardPicture : public Fl_Double_Window {
private:
    Jondra *parentWindow;
    Fl_RGB_Image *keyboardImage;
    Fl_RGB_Image *symPressedImage;
    Fl_RGB_Image *shiftPressedImage;
    Fl_RGB_Image *csPressedImage;
    Fl_RGB_Image *numPressedImage;
    Fl_RGB_Image *ctrlPressedImage;
    bool symPressed;
    bool shiftPressed;
    bool csPressed;
    bool numPressed;
    bool ctrlPressed;
    bool csKeySet;

    static void OnClose(Fl_Widget *w, void *data);
    void keepAboveMainWindow();

public:
    KeyboardPicture(Jondra *parent);
    ~KeyboardPicture();

    void hideAllPressedKeys();
    void processKeyPress(int keyCode);
    void processKeyRelease(int keyCode);
    void showDialog();
    void hideDialog();
    int handle(int event);
    void draw();
};

class Jondra : public Fl_Double_Window {
private:
    CustomMenuBar *menuBar;
	//Fl_Menu_Bar *menuBar;
    Fl_Group *toolBar;
    Fl_Box *menuSeparator;
    Fl_Box *toolbarBackground;
	//Fl_Button  *btnReset;
	FlatButton  *btnReset;
    FlatButton  *btnPause, *btnNMI, *btnLoadTape, *btnSaveTape, *btnRecTape;
    FlatButton *btnOpenSnap, *btnSaveSnap, *btnLoadMem, *btnSaveMem, *btnDebugger;
    FlatButton *btnSettings, *btnKeyboard;
	Fl_Box* separator1,* separator2,* separator3,* separator4;
    Fl_Box *statusPanel;
    Fl_Box *greenLedBox, *yellowLedBox;
    Fl_RGB_Image *iconReset, *iconPause, *iconNMI, *iconLoadTape, *iconSaveTape, *iconPlayTape, *iconRecTape;
    Fl_RGB_Image *iconOpenSnap, *iconSaveSnap, *iconLoadMem, *iconSaveMem, *iconDebugger;
    Fl_RGB_Image *iconSettings, *iconKeyboard;
    Fl_RGB_Image *iconGreenLed, *iconYellowLed, *iconGreenLedOff, *iconYellowLedOff;
    int greenLedState, yellowLedState;
	Fl_RGB_Image* iconOndra;
	DebuggerWindow* debuggerWindow;
	KeyboardPicture* keyboardPicture;
	bool firstKeyboardShow;
	bool fullscreenMode;
	int windowedX, windowedY, windowedW, windowedH;
#ifdef _WIN32
    long windowedStyle;
    long windowedExStyle;
    bool windowedStyleSaved;
#endif
	std::string snapshotNameProposal;
	std::string screenshotNameProposal;

    void loadIcons();
    void setFullscreenMode(bool fullscreen);

    static void redrawTimerCallback(void* userdata);
    static void ledTimerCallback(void* userdata);
    void updateLeds();
    static void OnExit(Fl_Widget *w, void *data);
	static void OnReset(Fl_Widget *w, void *data);
	static void OnPause(Fl_Widget *w, void *data);
	static void OnNMI(Fl_Widget *w, void *data);
    static void OnCpuSpeed(Fl_Widget *w, void *data);
	static void OnSettings(Fl_Widget *w, void *data);
	static void OnDebugger(Fl_Widget *w, void *data);
	static void OnKeyboard(Fl_Widget *w, void *data);
	static void OnAbout(Fl_Widget *w, void *data);
	static void OnLoadMemoryBlock(Fl_Widget *w, void *data);
	static void OnSaveMemoryBlock(Fl_Widget *w, void *data);
	static void close_about(Fl_Widget *w, void *win);
	static void OnOpenTap(Fl_Widget *w, void *data);
	static void OnSaveTap(Fl_Widget *w, void *data);
	static void OnRecordTape(Fl_Widget *w, void *data);
	static void OnOpenSnapshot(Fl_Widget *w, void *data);
	static void OnSaveSnapshot(Fl_Widget *w, void *data);
	static void OnSaveScreenshot(Fl_Widget *w, void *data);
	
	
	

public:
	Ondra *m;
	JScreen* m_screen; // Hlavni obrazovka emulatoru


    Jondra(int w, int h, const char* title);
	int last_x,last_y;
    void initEmulator();
	static void initEmulator_cb(void *userdata);//callback pro zavolani z timeru	
	void sizeWindowToFit();
	static void setWindowIcon(Fl_Window *win);
	static void startRedrawTimer(JScreen* screen);
	static void stopRedrawTimer();
	void LoadBinaryAndRun();
	void LoadBinSilently(std::string strFile);
	bool LoadArgumentImage(const std::string& strFile);
	void debuggerClosed(bool resumeEmulation);
	void syncPauseButton();
	void toggleKeyboardPicture();
	void processKeyboardPicturePress(int keyCode);
	void processKeyboardPictureRelease(int keyCode);
	void restoreKeyboardFocus();
	void toggleFullscreen();
	bool handleMenuShortcut(int keyCode, int state);
	void setSnapshotNameProposalFromFile(const std::string& fileName);

#ifdef _WIN32
	static void CALLBACK screenRefreshCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
	static UINT timerID;
#endif

};

#endif
