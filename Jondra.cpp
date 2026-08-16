#include "Jondra.h"
#include <FL/fl_ask.H>
#include <string>
#include "Config.h"
#include "Ondra.h"
#include "Keyboard.h"
#include "MTimer.h"
#include "Settings.h"
#include "BinOpen.h"
#include "HexInput.h"
#include <FL/Fl_Input.H>
#include "DebuggerWindow.h"
#include "Memory.h"
#include "EmbeddedResources.h"
#include "DirtyTiles.h"
#include "z80emu.h"
#include "cpuintf.h"
#include <FL/x.H>
#include <FL/Fl.H>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <unistd.h>
#include <limits.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif


#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
UINT Jondra::timerID = 0;
#endif




// -----------------------------------------------------------------------------
// Obrazek skutecne klavesnice Ondry.
// Zakladni obrazek a prekryvne PNG pro specialni klavesy SHIFT, NUM, CS,
// CTRL a SYM.
// -----------------------------------------------------------------------------
KeyboardPicture::KeyboardPicture(Jondra *parent)
: Fl_Double_Window(600, 169, "Keyboard") {
    parentWindow = parent;
    keyboardImage = NULL;
    symPressedImage = NULL;
    shiftPressedImage = NULL;
    csPressedImage = NULL;
    numPressedImage = NULL;
    ctrlPressedImage = NULL;
    symPressed = false;
    shiftPressed = false;
    csPressed = false;
    numPressed = false;
    ctrlPressed = false;
    csKeySet = false;

    keyboardImage = EmbeddedResources::loadPng("images/keyboard.png");
    symPressedImage = EmbeddedResources::loadPng("images/KBD_symbPress.png");
    shiftPressedImage = EmbeddedResources::loadPng("images/KBD_shiftPress.png");
    csPressedImage = EmbeddedResources::loadPng("images/KBD_csPress.png");
    numPressedImage = EmbeddedResources::loadPng("images/KBD_numPress.png");
    ctrlPressedImage = EmbeddedResources::loadPng("images/KBD_ctrlPress.png");

    color(FL_BLACK);
    callback(OnClose, this);
    set_non_modal();
    end();
}

KeyboardPicture::~KeyboardPicture() {
    delete keyboardImage;
    delete symPressedImage;
    delete shiftPressedImage;
    delete csPressedImage;
    delete numPressedImage;
    delete ctrlPressedImage;
}

void KeyboardPicture::OnClose(Fl_Widget *w, void *data) {
    KeyboardPicture *kbd = (KeyboardPicture*)data;
    if (kbd) {
        kbd->hideDialog();
    }
}

void KeyboardPicture::hideAllPressedKeys() {
    symPressed = false;
    shiftPressed = false;
    csPressed = false;
    numPressed = false;
    ctrlPressed = false;
    csKeySet = false;
    redraw();
}

void KeyboardPicture::processKeyPress(int keyCode) {
    switch (keyCode) {
    case FL_Tab:
        numPressed = true;
        break;
    case FL_Shift_L:
        shiftPressed = true;
        break;
    case 0x3D: /* '=' - CS funguje jako prepinac. */
        if (csKeySet) {
            csPressed = false;
            csKeySet = false;
        } else {
            csPressed = true;
            csKeySet = true;
        }
        break;
    case FL_Control_L:
        ctrlPressed = true;
        break;
    case FL_Alt_L:
        symPressed = true;
        break;
    default:
        /* Stisk bezne klavesy ukonci aktivni CS lock. */
        csPressed = false;
        csKeySet = false;
        break;
    }
    redraw();
}

void KeyboardPicture::processKeyRelease(int keyCode) {
    switch (keyCode) {
    case FL_Tab:
        numPressed = false;
        break;
    case FL_Shift_L:
        shiftPressed = false;
        break;
    case FL_Control_L:
        ctrlPressed = false;
        break;
    case FL_Alt_L:
        symPressed = false;
        break;
    default:
        return;
    }
    redraw();
}

void KeyboardPicture::draw() {
    Fl_Double_Window::draw();

    if (keyboardImage && keyboardImage->w() > 0) {
        keyboardImage->draw(0, 0);
    }
    if (symPressed && symPressedImage && symPressedImage->w() > 0) {
        symPressedImage->draw(2, 89);
    }
    if (shiftPressed && shiftPressedImage && shiftPressedImage->w() > 0) {
        shiftPressedImage->draw(0, 132);
    }
    if (csPressed && csPressedImage && csPressedImage->w() > 0) {
        csPressedImage->draw(57, 132);
    }
    if (numPressed && numPressedImage && numPressedImage->w() > 0) {
        numPressedImage->draw(112, 131);
    }
    if (ctrlPressed && ctrlPressedImage && ctrlPressedImage->w() > 0) {
        ctrlPressedImage->draw(516, 91);
    }
}

void KeyboardPicture::keepAboveMainWindow() {
#ifdef _WIN32
    HWND hwnd = fl_xid(this);
    if (hwnd) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#else
    Display *disp = fl_display;
    Window xwin = fl_xid(this);
    if (disp && xwin) {
        Atom wmState = XInternAtom(disp, "_NET_WM_STATE", False);
        Atom above = XInternAtom(disp, "_NET_WM_STATE_ABOVE", False);
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.xclient.type = ClientMessage;
        ev.xclient.window = xwin;
        ev.xclient.message_type = wmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1; /* _NET_WM_STATE_ADD */
        ev.xclient.data.l[1] = (long)above;
        XSendEvent(disp, DefaultRootWindow(disp), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        XRaiseWindow(disp, xwin);
        XFlush(disp);
    }
#endif
}

void KeyboardPicture::showDialog() {
    hideAllPressedKeys();
    show();
    keepAboveMainWindow();
    if (parentWindow) {
        parentWindow->restoreKeyboardFocus();
    }
}

void KeyboardPicture::hideDialog() {
    hide();
    if (parentWindow) {
        parentWindow->restoreKeyboardFocus();
    }
}

int KeyboardPicture::handle(int event) {
    /* FL_CLOSE posila FLTK pri stisku krizku v titulku okna.
       Okno klavesnice nechceme rusit - pouze ho schovame, aby ho bylo
       mozne znovu otevrit klavesou Escape. */
    if (event == FL_CLOSE) {
        hideDialog();
        return 1;
    }

    /* Kdyby window manager prece jen nechal focus na tomto okne, preposleme
       klavesy rovnou do emulovane klavesnice. */
    if ((event == FL_KEYDOWN || event == FL_KEYUP) &&
        parentWindow && parentWindow->m && parentWindow->m->getKeyboard()) {
        return parentWindow->m->getKeyboard()->handle(event);
    }
    if (event == FL_PUSH && parentWindow) {
        parentWindow->restoreKeyboardFocus();
        return 1;
    }
    return Fl_Double_Window::handle(event);
}


// -----------------------------------------------------------------------------
// Save Memory Block dialog.
// Uklada prosty binarni blok od prvni do posledni adresy vcetne.
// Soubor zamerne nema zadnou hlavicku.
// The class lives in this source file so the old VC6 project does not need a
// new source file added to it.
// -----------------------------------------------------------------------------
class MemorySaveDialog : public Fl_Double_Window {
private:
    Jondra *mainWindow;
    int result;
    Fl_Button *btnBrowse;
    Fl_Button *btnOK;

    static void BrowseCB(Fl_Widget *w, void *data) {
        MemorySaveDialog *self = (MemorySaveDialog*)data;
        const char *initial = self->textFile->value();
        const char *filename = fl_file_chooser("Select file for save data", "*.*", initial);
        if (filename) {
            self->textFile->value(filename);
        }
    }

    static void OKCB(Fl_Widget *w, void *data) {
        MemorySaveDialog *self = (MemorySaveDialog*)data;
        self->result = 1;
        self->hide();
    }

    static void CloseCB(Fl_Widget *w, void *data) {
        MemorySaveDialog *self = (MemorySaveDialog*)data;
        self->result = 0;
        self->hide();
    }

public:
    Fl_Input *textFile;
    HexInput *textFrom;
    HexInput *textTo;

    MemorySaveDialog(Jondra *parent)
        : Fl_Double_Window(370, 155, "Save memory block to file") {
        mainWindow = parent;
        result = 0;
        callback(CloseCB, this);
        /* Same window style as the existing Load Memory Block dialog.
           The parent is deactivated manually in showModal(), so FLTK's
           set_modal() is not needed here.  On old FLTK/Win32 set_modal()
           creates a dialog caption without the normal system close button. */
        set_non_modal();

        begin();
        Fl_Box *labelFile = new Fl_Box(10, 5, 60, 18, "File");
        labelFile->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        textFile = new Fl_Input(10, 25, 295, 25);
        btnBrowse = new Fl_Button(315, 25, 45, 25, "...");
        btnBrowse->callback(BrowseCB, this);

        Fl_Box *labelFrom = new Fl_Box(10, 60, 100, 18, "From address:");
        labelFrom->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Box *labelTo = new Fl_Box(125, 60, 100, 18, "To address:");
        labelTo->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        textFrom = new HexInput(10, 80, 70, 25, "", 4);
        textTo = new HexInput(125, 80, 70, 25, "", 4);

        btnOK = new Fl_Button(290, 115, 70, 30, "OK");
        btnOK->callback(OKCB, this);
        end();

        textFile->value(Config::strSaveBinFilePath.c_str());
        {
            char buf[5];
            sprintf(buf, "%04X", Config::nSaveFromAddress & 0xffff);
            textFrom->value(buf);
            sprintf(buf, "%04X", Config::nSaveToAddress & 0xffff);
            textTo->value(buf);
        }

        if (mainWindow) {
            position(mainWindow->x() + (mainWindow->w() - w()) / 2,
                     mainWindow->y() + (mainWindow->h() - h()) / 2);
        }
    }

    int showModal() {
        if (mainWindow) mainWindow->deactivate();
        show();
        Fl::focus(textFile);
        while (shown()) Fl::wait();
        if (mainWindow) mainWindow->activate();
        return result;
    }

    int handle(int event) {
        if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape) {
            result = 0;
            hide();
            return 1;
        }
        return Fl_Double_Window::handle(event);
    }
};

std::string strArgFile="";
int32_t nStartAddress=-1;

Jondra::Jondra(int w, int h, const char* title) : Fl_Double_Window(w, h, title) {
	color(FL_BLACK);
    debuggerWindow = NULL;
    keyboardPicture = NULL;
    firstKeyboardShow = true;
    fullscreenMode = false;
    greenLedBox = NULL;
    yellowLedBox = NULL;
    iconGreenLed = NULL;
    iconYellowLed = NULL;
    iconGreenLedOff = NULL;
    iconYellowLedOff = NULL;
    greenLedState = -1;
    yellowLedState = -1;
    windowedX = windowedY = 0;
    windowedW = windowedH = 0;
#ifdef _WIN32
    windowedStyle = 0;
    windowedExStyle = 0;
    windowedStyleSaved = false;
#endif
    menuBar = new CustomMenuBar(0, 0, w, 25);
	//menuBar = new Fl_Menu_Bar(0, 0, w, 25);
	
    menuBar->add("File/Open Tape for Load",0, OnOpenTap,this);
    menuBar->add("File/Open Tape for Save",0, OnSaveTap,this, FL_MENU_DIVIDER);
    
    menuBar->add("File/Open snapshot", FL_F+8, OnOpenSnapshot, this);
    menuBar->add("File/Save snapshot", FL_F+5, OnSaveSnapshot, this, FL_MENU_DIVIDER);
    menuBar->add("File/Load Memory Block", 0, OnLoadMemoryBlock, this);
    menuBar->add("File/Save Memory Block", 0, OnSaveMemoryBlock, this, FL_MENU_DIVIDER);
    menuBar->add("File/Save screenshot", FL_F+2, OnSaveScreenshot, this, FL_MENU_DIVIDER);
    menuBar->add("File/Exit", 0, OnExit, this);
    
    menuBar->add("Control/Reset",0, OnReset, this);
    menuBar->add("Control/Pause", 0, OnPause, this);
    menuBar->add("Control/NMI", 0, OnNMI,this, FL_MENU_DIVIDER);
    menuBar->add("Control/Speed/0.5x", 0, OnCpuSpeed, this, FL_MENU_RADIO);
    menuBar->add("Control/Speed/1x", 0, OnCpuSpeed, this, FL_MENU_RADIO | FL_MENU_VALUE);
    menuBar->add("Control/Speed/2x", 0, OnCpuSpeed, this, FL_MENU_RADIO);
    menuBar->add("Control/Speed/4x", 0, OnCpuSpeed, this, FL_MENU_RADIO);
    menuBar->add("Control/Speed/10x", 0, OnCpuSpeed, this, FL_MENU_RADIO);
    menuBar->add("Control/Speed/40x", 0, OnCpuSpeed, this, FL_MENU_RADIO);
    
    menuBar->add("Tools/Debugger", FL_CTRL+'d', OnDebugger, this);
    menuBar->add("Tools/Settings", 0, OnSettings, this);
    menuBar->add("Tools/Keyboard", FL_Escape, OnKeyboard, this);
    
    menuBar->add("About", 0, OnAbout,this);
    menuBar->textsize(13);
    menuBar->box(FL_FLAT_BOX); // Ploche menu bez 3D efektu
	
    // === Simulace spodni cary pod menu ===
    menuSeparator = new Fl_Box(0, 25, w, 1);
    menuSeparator->box(FL_FLAT_BOX);
    menuSeparator->color(FL_DARK3);  // Tmavsi seda, aby cara byla viditelna
    
	toolbarBackground = new Fl_Box(0, 26, w, 29);
	toolbarBackground->box(FL_FLAT_BOX);
	toolbarBackground->color(FL_GRAY);

    toolBar = new Fl_Group(0, 25, w, 30);
	
    toolBar->begin();
	
    loadIcons();
    
    int x = 5;  // Pocatecni pozice tlacitek
    int btnSize = 24;  // Velikost tlacitek
    int spacing = 28;  // Rozestupy mezi tlacitky
    
    int separatorWidth = 3;  // Sirka separatoru
    int separatorSpacing = 0; // Prostor kolem separatoru
    
    btnReset = new FlatButton(x, 28, btnSize, btnSize);
    btnReset->image(iconReset);
    btnReset->tooltip("Reset emulator");
    btnReset->callback(OnReset,this);
    x += spacing;
    
    btnPause = new FlatButton(x, 28, btnSize, btnSize);
    btnPause->image(iconPause);
    btnPause->type(FL_TOGGLE_BUTTON);
    btnPause->tooltip("Pause emulation");
    btnPause->callback(OnPause,this);
    x += spacing;
    
    btnNMI = new FlatButton(x, 28, btnSize, btnSize);
    btnNMI->image(iconNMI);
    btnNMI->tooltip("Trigger NMI interrupt");
    btnNMI->callback(OnNMI,this);
    x += spacing;
    
    btnLoadTape = new FlatButton(x, 28, btnSize, btnSize);
    btnLoadTape->image(iconLoadTape);
    btnLoadTape->tooltip("Open TAPE for loading");
	btnLoadTape->callback(OnOpenTap,this);
	
    x += spacing;
    
    btnSaveTape = new FlatButton(x, 28, btnSize, btnSize);
    btnSaveTape->image(iconSaveTape);
    btnSaveTape->tooltip("Open TAPE for saving to CSW");
    btnSaveTape->callback(OnSaveTap, this);
    x += spacing;

    btnRecTape = new FlatButton(x, 28, btnSize, btnSize);
    btnRecTape->image(iconPlayTape);
    btnRecTape->type(FL_TOGGLE_BUTTON);
    btnRecTape->tooltip("Play / Record switch");
    btnRecTape->callback(OnRecordTape, this);
    x += spacing;
    
    btnOpenSnap = new FlatButton(x, 28, btnSize, btnSize);
    btnOpenSnap->image(iconOpenSnap);
    btnOpenSnap->tooltip("Open snapshot");
    btnOpenSnap->callback(OnOpenSnapshot, this);
    x += spacing;
    
    btnSaveSnap = new FlatButton(x, 28, btnSize, btnSize);
    btnSaveSnap->image(iconSaveSnap);
    btnSaveSnap->tooltip("Save snapshot");
    btnSaveSnap->callback(OnSaveSnapshot, this);
    x += spacing;
    
    btnLoadMem = new FlatButton(x, 28, btnSize, btnSize);
    btnLoadMem->image(iconLoadMem);
    btnLoadMem->tooltip("Load memory block");
    btnLoadMem->callback(OnLoadMemoryBlock,this);
    x += spacing;
    
    btnSaveMem = new FlatButton(x, 28, btnSize, btnSize);
    btnSaveMem->image(iconSaveMem);
    btnSaveMem->tooltip("Save memory block");
    btnSaveMem->callback(OnSaveMemoryBlock, this);
    x += spacing;
    
    btnDebugger = new FlatButton(x, 28, btnSize, btnSize);
    btnDebugger->image(iconDebugger);
    btnDebugger->tooltip("Start debugger");
    btnDebugger->callback(OnDebugger,this);
    x += spacing;
    
    btnSettings = new FlatButton(x, 28, btnSize, btnSize);
    btnSettings->image(iconSettings);
    btnSettings->tooltip("Open settings");
    btnSettings->callback(OnSettings,this);
    x += spacing;
    
    btnKeyboard = new FlatButton(x, 28, btnSize, btnSize);
    btnKeyboard->image(iconKeyboard);
    btnKeyboard->tooltip("Show keyboard");
    btnKeyboard->callback(OnKeyboard,this);
    
    toolBar->end();

	m=NULL;
    
    m_screen = new JScreen(this,0, menuBar->h()+toolBar->h(), 320, 256, 2.0f);
    
    statusPanel = new Fl_Box(FL_DOWN_BOX, 0, h - 30, w, 30, "");
    sizeWindowToFit();
    statusPanel->resize(0, this->h() - 30, this->w(), 30);

    /* The real Ondra has two software-controlled LEDs on output port A0.
     * D0 controls the green LED and D1 the yellow LED; both are active low.
     * Keep only those two real-machine indicators here -- no emulator-only
     * tape LED. */
    greenLedBox = new Fl_Box(6, this->h() - 23, 16, 16);
    greenLedBox->image(iconGreenLedOff != NULL ? iconGreenLedOff : iconGreenLed);
    greenLedBox->tooltip("Green LED");

    yellowLedBox = new Fl_Box(28, this->h() - 23, 16, 16);
    yellowLedBox->image(iconYellowLedOff != NULL ? iconYellowLedOff : iconYellowLed);
    yellowLedBox->tooltip("Yellow LED");

    windowedX = this->x();
    windowedY = this->y();
    windowedW = this->w();
    windowedH = this->h();
    end();
	last_x=this->x();
	last_y=this->y();
    Fl_Tooltip::delay(0.2F);
}

void Jondra::initEmulator_cb(void *userdata) {
	Jondra* win = (Jondra*)userdata;
	win->initEmulator();
    }

#ifdef _WIN32
static HICON createEmbeddedWindowIcon(Fl_RGB_Image* source, int targetW, int targetH) {
    Fl_Image* scaledBase = NULL;
    Fl_RGB_Image* image = source;
    HDC screenDC = NULL;
    HBITMAP colorBitmap = NULL;
    HBITMAP maskBitmap = NULL;
    void* dibBits = NULL;
    BITMAPINFO bitmapInfo;
    ICONINFO iconInfo;
    HICON icon = NULL;
    unsigned char* maskBits = NULL;
    int maskStride;
    int x, y;

    if (source == NULL || source->w() <= 0 || source->h() <= 0 || source->d() < 3) {
        return NULL;
    }

    if (source->w() != targetW || source->h() != targetH) {
        scaledBase = source->copy(targetW, targetH);
        image = (Fl_RGB_Image*)scaledBase;
        if (image == NULL || image->w() != targetW || image->h() != targetH || image->d() < 3) {
            delete scaledBase;
            return NULL;
        }
    }

    memset(&bitmapInfo, 0, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = targetW;
    bitmapInfo.bmiHeader.biHeight = targetH; /* bottom-up DIB for Win9x compatibility */
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    screenDC = GetDC(NULL);
    colorBitmap = CreateDIBSection(screenDC, &bitmapInfo, DIB_RGB_COLORS,
                                   &dibBits, NULL, 0);
    if (screenDC != NULL) {
        ReleaseDC(NULL, screenDC);
    }
    if (colorBitmap == NULL || dibBits == NULL) {
        delete scaledBase;
        return NULL;
    }

    maskStride = ((targetW + 15) / 16) * 2;
    maskBits = new unsigned char[maskStride * targetH];
    memset(maskBits, 0, maskStride * targetH);

    {
        const unsigned char* src = (const unsigned char*)image->data()[0];
        unsigned char* dst = (unsigned char*)dibBits;
        int depth = image->d();
        int srcStride = image->ld();
        if (srcStride == 0) {
            srcStride = targetW * depth;
        }

        for (y = 0; y < targetH; ++y) {
            const unsigned char* srcRow = src + y * srcStride;
            unsigned char* dstRow = dst + (targetH - 1 - y) * targetW * 4;
            for (x = 0; x < targetW; ++x) {
                const unsigned char* pixel = srcRow + x * depth;
                unsigned char alpha = (depth >= 4) ? pixel[3] : 255;
                dstRow[x * 4 + 0] = pixel[2];
                dstRow[x * 4 + 1] = pixel[1];
                dstRow[x * 4 + 2] = pixel[0];
                dstRow[x * 4 + 3] = alpha;

                if (alpha < 128) {
                    maskBits[(targetH - 1 - y) * maskStride + (x >> 3)] |= (unsigned char)(0x80 >> (x & 7));
                }
            }
        }
    }

    maskBitmap = CreateBitmap(targetW, targetH, 1, 1, maskBits);
    delete[] maskBits;

    if (maskBitmap != NULL) {
        memset(&iconInfo, 0, sizeof(iconInfo));
        iconInfo.fIcon = TRUE;
        iconInfo.hbmColor = colorBitmap;
        iconInfo.hbmMask = maskBitmap;
        icon = CreateIconIndirect(&iconInfo);
        DeleteObject(maskBitmap);
    }
    DeleteObject(colorBitmap);
    delete scaledBase;
    return icon;
}
#endif

void Jondra::setWindowIcon(Fl_Window* win) {
    Fl_RGB_Image* pngIcon = EmbeddedResources::loadPng("images/ondra.png");
    if (pngIcon == NULL || pngIcon->w() <= 0 || pngIcon->h() <= 0 || pngIcon->d() < 3) {
        delete pngIcon;
        return;
    }

#ifdef _WIN32
    static HICON hIconBig = NULL;
    static HICON hIconSmall = NULL;

    if (hIconBig == NULL) {
        hIconBig = createEmbeddedWindowIcon(pngIcon, 32, 32);
    }
    if (hIconSmall == NULL) {
        hIconSmall = createEmbeddedWindowIcon(pngIcon, 16, 16);
    }

    {
        HWND hwnd = fl_xid(win);
        if (hwnd != NULL) {
            if (hIconBig != NULL) {
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
            }
            if (hIconSmall != NULL) {
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }
#else
    Fl_Color flColor = win->color();
    unsigned char r_bg, g_bg, b_bg;
    int w = pngIcon->w();
    int h = pngIcon->h();
    int d = pngIcon->d();
    int stride = pngIcon->ld();
    unsigned long* iconData = new unsigned long[(w * h) + 2];
    const unsigned char* imgData = (const unsigned char*)pngIcon->data()[0];
    int i;

    if (stride == 0) {
        stride = w * d;
    }

    Fl::get_color(flColor, r_bg, g_bg, b_bg);
    iconData[0] = (unsigned long)w;
    iconData[1] = (unsigned long)h;

    for (i = 0; i < w * h; ++i) {
        int x = i % w;
        int y = i / w;
        const unsigned char* pixel = imgData + y * stride + x * d;
        unsigned char r = pixel[0];
        unsigned char g = pixel[1];
        unsigned char b = pixel[2];
        unsigned char a = (d >= 4) ? pixel[3] : 0xff;

        if (a == 0) {
            r = r_bg;
            g = g_bg;
            b = b_bg;
            a = 255;
        }
        iconData[i + 2] = ((unsigned long)a << 24) |
                          ((unsigned long)r << 16) |
                          ((unsigned long)g << 8) |
                          (unsigned long)b;
    }

    {
        Display* disp = fl_display;
        Window xWin = fl_xid(win);
        Atom netWmIcon = XInternAtom(disp, "_NET_WM_ICON", False);
        Atom cardinal = XInternAtom(disp, "CARDINAL", False);
        Atom themeAtom = XInternAtom(disp, "_GTK_THEME_VARIANT", False);
        const char* themeValue = "dark";

        XChangeProperty(disp, xWin, netWmIcon, cardinal, 32, PropModeReplace,
                        (unsigned char*)iconData, (w * h) + 2);
        XChangeProperty(disp, xWin, themeAtom, XA_STRING, 8, PropModeReplace,
                        (const unsigned char*)themeValue, strlen(themeValue));
        XFlush(disp);
    }

    /* XChangeProperty copies the data, so the temporary array can be freed. */
    delete[] iconData;
#endif

    delete pngIcon;
}

void Jondra::sizeWindowToFit() {
    int screenW = m_screen->w();  // Sirka obrazovky emulatoru
    int screenH = m_screen->h();  // Vyska obrazovky emulatoru
	
    int newWidth = screenW; // Pouzijeme sirku emulatoru
    int newHeight = screenH + menuBar->h() + toolBar->h() + statusPanel->h(); // Pricteme vysku toolbaru a status panelu
	
    size(newWidth, newHeight); // Nastavime novou velikost okna
    redraw(); // Prekreslime okno

}


void Jondra::setFullscreenMode(bool enableFullscreen) {
    bool wasRunning = false;
    int screenX = 0;
    int screenY = 0;
    int screenW = 0;
    int screenH = 0;
#ifdef _WIN32
    bool restartRedrawTimer = (timerID != 0);
#endif

    if (fullscreenMode == enableFullscreen) {
        return;
    }

    if (m && !m->isPaused()) {
        wasRunning = true;
        m->stopEmulation();
    }

#ifdef _WIN32
    /* The Win98 build uses a multimedia timer callback for GUI refresh.
       Stop it while the screen buffers are reallocated. */
    if (restartRedrawTimer) {
        stopRedrawTimer();
    }
    while (m_screen && m_screen->bDrawingNow) {
        Sleep(1);
    }
#endif

    if (enableFullscreen) {
        windowedX = x();
        windowedY = y();
        windowedW = w();
        windowedH = h();

        fullscreenMode = true;
        menuBar->hide();
        menuSeparator->hide();
        toolbarBackground->hide();
        toolBar->hide();
        statusPanel->hide();
        if (greenLedBox != NULL) greenLedBox->hide();
        if (yellowLedBox != NULL) yellowLedBox->hide();

        /* Do not use w()/h() immediately after fullscreen().  On X11 the
           window manager processes the fullscreen request asynchronously, so
           they can still contain the old windowed size.  That was the reason
           Linux showed only a small black rectangle in the upper-left corner.
           Obtain the physical screen size directly and force both the FLTK
           window and the native window to that geometry. */
#ifdef _WIN32
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
#else
        if (fl_display) {
            int scrNo = DefaultScreen(fl_display);
            screenW = DisplayWidth(fl_display, scrNo);
            screenH = DisplayHeight(fl_display, scrNo);
        } else {
            screenW = Fl::w();
            screenH = Fl::h();
        }
#endif
        if (screenW < 1) screenW = windowedW;
        if (screenH < 1) screenH = windowedH;

#ifdef _WIN32
        /* Do not use Fl_Window::fullscreen() on the Win98/FLTK 1.1 path.
           Depending on the FLTK build it can leave the native caption behind.
           Keep FLTK's normal-window state intact and switch only the HWND to a
           borderless popup.  This also makes returning to the exact old window
           geometry deterministic. */
        {
            HWND hwnd = fl_xid(this);
            if (hwnd) {
                long style;
                long exStyle;

                windowedStyle = GetWindowLong(hwnd, GWL_STYLE);
                windowedExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
                windowedStyleSaved = true;

                style = windowedStyle;
                style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
                           WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER | WS_DLGFRAME);
                style |= WS_POPUP;

                exStyle = windowedExStyle;
                exStyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME);

                SetWindowLong(hwnd, GWL_STYLE, style);
                SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
                resize(screenX, screenY, screenW, screenH);
                SetWindowPos(hwnd, HWND_TOPMOST,
                             screenX, screenY, screenW, screenH,
                             SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                Fl::flush();
            } else {
                resize(screenX, screenY, screenW, screenH);
                Fl::flush();
            }
        }
#else
        fullscreen();
        resize(screenX, screenY, screenW, screenH);
        Fl::flush();

        /* Some X11 window managers do not resize an old FLTK fullscreen
           request synchronously.  Force the native geometry as well and ask
           the WM for both FULLSCREEN and ABOVE so desktop panels cannot stay
           in front during presentations. */
        {
            Display *disp = fl_display;
            Window xwin = fl_xid(this);
            if (disp && xwin) {
                Atom wmState = XInternAtom(disp, "_NET_WM_STATE", False);
                Atom fsState = XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", False);
                Atom aboveState = XInternAtom(disp, "_NET_WM_STATE_ABOVE", False);
                XEvent ev;

                XMoveResizeWindow(disp, xwin,
                                  screenX, screenY,
                                  (unsigned int)screenW, (unsigned int)screenH);

                memset(&ev, 0, sizeof(ev));
                ev.xclient.type = ClientMessage;
                ev.xclient.window = xwin;
                ev.xclient.message_type = wmState;
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = 1; /* _NET_WM_STATE_ADD */
                ev.xclient.data.l[1] = (long)fsState;
                ev.xclient.data.l[2] = (long)aboveState;
                ev.xclient.data.l[3] = 1; /* normal application */
                XSendEvent(disp, DefaultRootWindow(disp), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &ev);
                XRaiseWindow(disp, xwin);
                XFlush(disp);
            }
        }
#endif

        m_screen->setDisplayArea(0, 0, screenW, screenH, true);
    } else {
        fullscreenMode = false;

#ifdef _WIN32
        /* Windows fullscreen above is purely native, so there is no FLTK
           fullscreen state to unwind.  Restore the original HWND styles first;
           then FLTK can restore its saved client geometry normally. */
        {
            HWND hwnd = fl_xid(this);
            if (hwnd) {
                if (windowedStyleSaved) {
                    SetWindowLong(hwnd, GWL_STYLE, windowedStyle);
                    SetWindowLong(hwnd, GWL_EXSTYLE, windowedExStyle);
                }
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE |
                             SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                resize(windowedX, windowedY, windowedW, windowedH);
                Fl::flush();
            } else {
                resize(windowedX, windowedY, windowedW, windowedH);
            }
        }
#else
        /* We forced the X11 native window to screen size on entry, therefore
           fullscreen_off() alone is not enough with some WMs: FLTK clears its
           own flag, but the already resized X window can remain screen-sized.
           Explicitly remove both EWMH states and finally restore the native
           geometry saved before fullscreen. */
        {
            Display *disp = fl_display;
            Window xwin = fl_xid(this);
            if (disp && xwin) {
                Atom wmState = XInternAtom(disp, "_NET_WM_STATE", False);
                Atom fsState = XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", False);
                Atom aboveState = XInternAtom(disp, "_NET_WM_STATE_ABOVE", False);
                XEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.xclient.type = ClientMessage;
                ev.xclient.window = xwin;
                ev.xclient.message_type = wmState;
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = 0; /* _NET_WM_STATE_REMOVE */
                ev.xclient.data.l[1] = (long)fsState;
                ev.xclient.data.l[2] = (long)aboveState;
                ev.xclient.data.l[3] = 1;
                XSendEvent(disp, DefaultRootWindow(disp), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &ev);
                XFlush(disp);
            }
        }

        fullscreen_off(windowedX, windowedY, windowedW, windowedH);
        resize(windowedX, windowedY, windowedW, windowedH);
        Fl::flush();

        {
            Display *disp = fl_display;
            Window xwin = fl_xid(this);
            if (disp && xwin) {
                XMoveResizeWindow(disp, xwin,
                                  windowedX, windowedY,
                                  (unsigned int)windowedW, (unsigned int)windowedH);
                XRaiseWindow(disp, xwin);
                XFlush(disp);
            }
        }
#endif

        menuBar->resize(0, 0, windowedW, 25);
        menuSeparator->resize(0, 25, windowedW, 1);
        toolbarBackground->resize(0, 26, windowedW, 29);
        toolBar->resize(0, 25, windowedW, 30);
        statusPanel->resize(0, windowedH - 30, windowedW, 30);
        if (greenLedBox != NULL) greenLedBox->resize(6, windowedH - 23, 16, 16);
        if (yellowLedBox != NULL) yellowLedBox->resize(28, windowedH - 23, 16, 16);

        menuBar->show();
        menuSeparator->show();
        toolbarBackground->show();
        toolBar->show();
        statusPanel->show();
        if (greenLedBox != NULL) greenLedBox->show();
        if (yellowLedBox != NULL) yellowLedBox->show();

        m_screen->setDisplayArea(0, menuBar->h() + toolBar->h(),
                                 m_screen->GetWidth() * 2,
                                 m_screen->GetHeight() * 2, false);
    }

    /* Rebuild the complete presentation buffer immediately. This is also
       needed if F12 is pressed while emulation is paused. */
    if (m && m->til) {
        m->til->DirtyTilesAll();
        m->til->DispUpdate(m->px);
    }

    redraw();
    m_screen->redraw();
    Fl::flush();
    m_screen->needsRedraw = false;

#ifdef _WIN32
    if (restartRedrawTimer) {
        startRedrawTimer(m_screen);
    }
#endif

    if (wasRunning && m) {
        m->startEmulation();
    }
    restoreKeyboardFocus();
}

void Jondra::toggleFullscreen() {
    setFullscreenMode(!fullscreenMode);
}


static Fl_RGB_Image* createLedOffImage(Fl_RGB_Image* source) {
    int w, h, d, srcStride;
    const unsigned char* src;
    unsigned char* dst;
    Fl_RGB_Image* result;
    int x, y;

    if (source == NULL || source->w() <= 0 || source->h() <= 0 || source->d() < 3) {
        return NULL;
    }

    w = source->w();
    h = source->h();
    d = source->d();
    srcStride = source->ld();
    if (srcStride == 0) {
        srcStride = w * d;
    }

    src = (const unsigned char*)source->data()[0];
    dst = new unsigned char[w * h * d];

    for (y = 0; y < h; ++y) {
        const unsigned char* srcRow = src + y * srcStride;
        unsigned char* dstRow = dst + y * w * d;

        for (x = 0; x < w; ++x) {
            const unsigned char* s = srcRow + x * d;
            unsigned char* p = dstRow + x * d;

            /* Dark coloured glass when the LED is off. */
            p[0] = (unsigned char)((s[0] * 28) / 100);
            p[1] = (unsigned char)((s[1] * 28) / 100);
            p[2] = (unsigned char)((s[2] * 28) / 100);
            if (d >= 4) {
                p[3] = s[3];
            }
            if (d > 4) {
                int c;
                for (c = 4; c < d; ++c) {
                    p[c] = s[c];
                }
            }
        }
    }

    result = new Fl_RGB_Image(dst, w, h, d);
    if (result == NULL) {
        delete[] dst;
        return NULL;
    }

    result->alloc_array = 1;
    return result;
}

void Jondra::loadIcons() {
    iconReset = EmbeddedResources::loadPng("images/reset.png");
    iconPause = EmbeddedResources::loadPng("images/pause.png");
    iconNMI = EmbeddedResources::loadPng("images/nmi.png");
    iconLoadTape = EmbeddedResources::loadPng("images/open.png");
    iconSaveTape = EmbeddedResources::loadPng("images/save.png");
    iconPlayTape = EmbeddedResources::loadPng("images/player_play.png");
    iconRecTape = EmbeddedResources::loadPng("images/player_rec.png");
    iconOpenSnap = EmbeddedResources::loadPng("images/opensn.png");
    iconSaveSnap = EmbeddedResources::loadPng("images/savesn.png");
    iconLoadMem = EmbeddedResources::loadPng("images/binaryopn.png");
    iconSaveMem = EmbeddedResources::loadPng("images/binarysav.png");
    iconDebugger = EmbeddedResources::loadPng("images/debugger.png");
    iconSettings = EmbeddedResources::loadPng("images/settings.png");
    iconKeyboard = EmbeddedResources::loadPng("images/keyico.png");
    iconGreenLed = EmbeddedResources::loadPng("images/green.png");
    iconYellowLed = EmbeddedResources::loadPng("images/yellow.png");
    iconGreenLedOff = createLedOffImage(iconGreenLed);
    iconYellowLedOff = createLedOffImage(iconYellowLed);

    if (iconReset == NULL) {
        fl_alert("Chyba: Vestavene ikonky se nepodarilo nacist.");
    }
}

void Jondra::initEmulator() {
    Config::LoadConfig();
    m_screen->setScanlines(Config::getScanlines());
    m = new Ondra();
	m->nStartAddress=nStartAddress;
	m->strArgFile=strArgFile;
    m->setFrame(this);
    m->getKeyboard()->setMainWindow(this);
    m->setScreen(m_screen);
    m->setRecButton(btnRecTape);
    m->setTapeMode(btnRecTape && btnRecTape->value() != 0);
    if (Config::getFullscreen()) {
        setFullscreenMode(true);
    }
    m->startEmulation();
	startRedrawTimer(m_screen);
    updateLeds();
    Fl::add_timeout(0.02, ledTimerCallback, this);

}

// Windows: precise 1ms timer using timeSetEvent().
#ifdef _WIN32
void CALLBACK Jondra::screenRefreshCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    JScreen* screen = reinterpret_cast<JScreen*>(dwUser);
	Jondra* mainWin=(Jondra*)screen->mainWin;
	if (mainWin->x() != mainWin->last_x || mainWin->y() != mainWin->last_y) {
		if(mainWin->m!=NULL){
			mainWin->m->til->DirtyTilesAll();
		}
		mainWin->last_x = mainWin->x();
		mainWin->last_y = mainWin->y();
	}
	if (screen->needsRedraw) {
		if(!screen->bDrawingNow){
			screen->redraw();
			if (Fl::ready()){				
				Fl::lock();
				Fl::flush();
				Fl::unlock();
			}
			screen->needsRedraw=false;			
		}
	}
}

void Jondra::startRedrawTimer(JScreen* screen) {
    if (timerID == 0) {
        timerID = timeSetEvent(1, 1, screenRefreshCallback, reinterpret_cast<DWORD_PTR>(screen), TIME_PERIODIC);
    }
}

void Jondra::stopRedrawTimer() {
    if (timerID) {
        timeKillEvent(timerID);
        timerID = 0;
    }
}
#endif

// **Linux: `Fl::add_timeout()` s intervalem 5 ms**
#ifndef _WIN32
void Jondra::redrawTimerCallback(void* userdata) {
    JScreen* screen = static_cast<JScreen*>(userdata);
    Jondra* mainWin = screen ? static_cast<Jondra*>(screen->mainWin) : NULL;

    /*
     * This callback runs in the FLTK main thread. Apply window-title changes
     * here; the emulation pthread must never call Fl_Window::label() directly.
     */
    if (mainWin && mainWin->m) {
        int speedPercent = 0;
        if (mainWin->m->takePendingWindowSpeed(&speedPercent)) {
            char title[64];
            sprintf(title, "Ondra SPO 186 - %d%%", speedPercent);
            mainWin->label(title);
        }
    }

    if (screen && screen->needsRedraw) {
        screen->redraw();
        Fl::flush();
        screen->needsRedraw = false;
    }
    Fl::add_timeout(0.005, redrawTimerCallback, userdata);
}

void Jondra::startRedrawTimer(JScreen* screen) {
    Fl::add_timeout(0.005, redrawTimerCallback, screen);
}

void Jondra::stopRedrawTimer() {
    // FLTK handles its own timeouts here; nothing to stop.
}
#endif


void Jondra::ledTimerCallback(void* userdata) {
    Jondra* me = (Jondra*)userdata;

    if (me == NULL) {
        return;
    }

    me->updateLeds();
    Fl::add_timeout(0.02, ledTimerCallback, userdata);
}

void Jondra::updateLeds() {
    unsigned char port;
    int newGreen;
    int newYellow;

    if (m == NULL) {
        return;
    }

    port = m->getPortA0();

    /* Original hardware is active low:
     *   D0 = 0 -> green LED on
     *   D1 = 0 -> yellow LED on */
    newGreen = ((port & 0x01) == 0) ? 1 : 0;
    newYellow = ((port & 0x02) == 0) ? 1 : 0;

    if (greenLedBox != NULL && newGreen != greenLedState) {
        greenLedState = newGreen;
        greenLedBox->image(newGreen
            ? iconGreenLed
            : (iconGreenLedOff != NULL ? iconGreenLedOff : iconGreenLed));
        greenLedBox->redraw();
    }

    if (yellowLedBox != NULL && newYellow != yellowLedState) {
        yellowLedState = newYellow;
        yellowLedBox->image(newYellow
            ? iconYellowLed
            : (iconYellowLedOff != NULL ? iconYellowLedOff : iconYellowLed));
        yellowLedBox->redraw();
    }
}

void Jondra::OnExit(Fl_Widget *w, void *data) {
    Jondra* me = ((Jondra*)data);
    Fl::remove_timeout(ledTimerCallback, me);
    if (me->debuggerWindow) {
        me->debuggerWindow->hide();
        delete me->debuggerWindow;
        me->debuggerWindow = NULL;
    }
    if (me->keyboardPicture) {
        me->keyboardPicture->hide();
        delete me->keyboardPicture;
        me->keyboardPicture = NULL;
    }
    me->hide();
//	stopRedrawTimer();
    if (me->m) {
        // Bezpecne zastaveni casovace
        if (me->m->task) {
            me->m->task->StopTimer(); // Zastav casovac
        }
        delete me->m; // Uvolni pamet
        me->m = NULL; // Prevence dvojiteho mazani
    }
	
    if (me->m_screen) {
        delete me->m_screen; // Uvolni obrazovku
        me->m_screen = NULL; // Prevence dvojiteho mazani
    }
}



void Jondra::OnReset(Fl_Widget *w, void *data) {
	Jondra* me=((Jondra*)data);

    /*
     * Reset must cross the emulation-thread barrier only once. The old code
     * stopped the worker, started it again and immediately stopped it a
     * second time, which was unnecessary and made the intermittent
     * stop/redraw deadlock much easier to hit.
     */
	me->m->stopEmulation();
	me->btnPause->value(0);
	me->btnPause->updateState();
	me->m->setClockSpeed(20);
	me->snapshotNameProposal = "";
	me->screenshotNameProposal = "";
	me->m->Reset(false);
    me->m->startEmulation();
}

void Jondra::OnPause(Fl_Widget *w, void *data) {
	Jondra* me=((Jondra*)data);
	if (me->m->isPaused()) {
		me->btnPause->value(0);
		me->m->startEmulation();
	} else {
		me->btnPause->value(1);
		me->m->stopEmulation();
	}
}


void Jondra::OnNMI(Fl_Widget *w, void *data) {
	Jondra* me=((Jondra*)data);
	me->m->Nmi();
}

void Jondra::OnCpuSpeed(Fl_Widget *w, void *data) {
    Jondra* me = (Jondra*)data;
    const Fl_Menu_Item* item;
    int percent = 100;
    bool wasPaused;

    if (me == NULL || me->m == NULL || me->menuBar == NULL) {
        return;
    }

    item = me->menuBar->mvalue();
    if (item == NULL || item->text == NULL) {
        return;
    }

    if (strcmp(item->text, "0.5x") == 0) {
        percent = 50;
    } else if (strcmp(item->text, "1x") == 0) {
        percent = 100;
    } else if (strcmp(item->text, "2x") == 0) {
        percent = 200;
    } else if (strcmp(item->text, "4x") == 0) {
        percent = 400;
    } else if (strcmp(item->text, "10x") == 0) {
        percent = 1000;
    } else if (strcmp(item->text, "40x") == 0) {
        percent = 4000;
    }

    /* Speed changes touch audio timing state.  Perform the change while the
     * emulation thread is stopped, then restore the previous run/pause state. */
    wasPaused = me->m->isPaused();
    if (!wasPaused) {
        me->m->stopEmulation();
    }
    me->m->setCpuSpeedPercent(percent);
    if (!wasPaused) {
        me->m->startEmulation();
    }

    me->restoreKeyboardFocus();
}

void Jondra::OnKeyboard(Fl_Widget *w, void *data) {
    Jondra *me = (Jondra*)data;
    if (me) {
        me->toggleKeyboardPicture();
    }
}

/*
 * The zero-size Keyboard widget normally owns the FLTK focus and consumes
 * FL_KEYDOWN events before the menu bar sees them.  Therefore menu
 * accelerators that must also work while the emulator is being used are
 * dispatched explicitly from Keyboard::handle().  Keep these values in sync
 * with the accelerators displayed in the menu above.
 */
bool Jondra::handleMenuShortcut(int keyCode, int state) {
    if (keyCode == FL_F + 2) {
        OnSaveScreenshot(NULL, this);
        return true;
    }
    if (keyCode == FL_F + 5) {
        OnSaveSnapshot(NULL, this);
        return true;
    }
    if (keyCode == FL_F + 8) {
        OnOpenSnapshot(NULL, this);
        return true;
    }
    if (keyCode == FL_F + 12) {
        toggleFullscreen();
        return true;
    }
    if (keyCode == FL_Escape) {
        OnKeyboard(NULL, this);
        return true;
    }
    if ((state & FL_CTRL) && (keyCode == 'd' || keyCode == 'D')) {
        /* Ctrl is an Ondra key too.  Opening the debugger changes focus, so
           release the emulated keyboard first; otherwise Ctrl/D could remain
           logically pressed after returning from the debugger. */
        if (m && m->getKeyboard()) {
            m->getKeyboard()->clearKeyboardBuffer();
        }
        if (keyboardPicture && keyboardPicture->shown()) {
            keyboardPicture->hideAllPressedKeys();
        }
        OnDebugger(NULL, this);
        return true;
    }
    return false;
}

void Jondra::restoreKeyboardFocus() {
    if (m && m->getKeyboard()) {
        take_focus();
        Fl::focus((Fl_Widget*)m->getKeyboard());
    }
}

void Jondra::toggleKeyboardPicture() {
    if (!m || !m->getKeyboard()) {
        return;
    }

    if (!keyboardPicture) {
        keyboardPicture = new KeyboardPicture(this);
    }

    if (keyboardPicture->shown()) {
        keyboardPicture->hideDialog();
        return;
    }

    if (firstKeyboardShow) {
        int px = x() + (w() - keyboardPicture->w()) / 2;
        int py = y() + (h() - keyboardPicture->h()) / 2;
        keyboardPicture->position(px, py);
        firstKeyboardShow = false;
    }
    keyboardPicture->showDialog();
}

void Jondra::processKeyboardPicturePress(int keyCode) {
    if (keyboardPicture && keyboardPicture->shown()) {
        keyboardPicture->processKeyPress(keyCode);
    }
}

void Jondra::processKeyboardPictureRelease(int keyCode) {
    if (keyboardPicture && keyboardPicture->shown()) {
        keyboardPicture->processKeyRelease(keyCode);
    }
}

void Jondra::OnOpenTap(Fl_Widget *w, void *data) {
	Jondra* me=((Jondra*)data);
	const char* filename = fl_file_chooser("Open tape for LOAD", "Tape files (*.{tap,csw,wav})", Config::getDirectoryFromPath(Config::strTapFilePath).c_str());
    if (filename) {
	    me->m->openLoadTape(filename);
		me->setSnapshotNameProposalFromFile(filename);
		me->m->setClockSpeed(20);
		Config::strTapFilePath = filename;
		Config::SaveConfig();
	}
	me->restoreKeyboardFocus();
}

static bool tapeHasCswExtension(const std::string& fileName) {
    size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos) return false;
    if (fileName.length() - dot != 4) return false;

    char c1 = fileName[dot + 1];
    char c2 = fileName[dot + 2];
    char c3 = fileName[dot + 3];
    return (c1 == 'c' || c1 == 'C') &&
           (c2 == 's' || c2 == 'S') &&
           (c3 == 'w' || c3 == 'W');
}

static bool tapeFileExists(const std::string& fileName) {
    FILE* f = fopen(fileName.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void Jondra::OnSaveTap(Fl_Widget *w, void *data) {
    Jondra* me = (Jondra*)data;
    bool wasRunning;
    const char* filename;
    std::string saveName;
    (void)w;

    if (!me || !me->m) return;

    /* Opening/replacing the writer must not race with the emulation timer,
       which may currently be writing a CSW sample. */
    wasRunning = !me->m->isPaused();
    if (wasRunning) me->m->stopEmulation();

    filename = fl_file_chooser("Open tape for SAVE", "*.csw",
                               Config::getDirectoryFromPath(Config::strTapFilePath).c_str());
    if (filename) {
        saveName = filename;
        if (!tapeHasCswExtension(saveName)) {
            saveName += ".csw";
        }

        bool mayWrite = true;
        if (tapeFileExists(saveName)) {
            int answer = fl_choice("File already exists:\n%s\n\nOverwrite it?",
                                   "Cancel", "Overwrite", 0, saveName.c_str());
            mayWrite = (answer == 1);
        }

        if (mayWrite) {
            /* Save vzdy nahrazuje existujici soubor; CSW writer pouziva wb+ a nikdy
               nepridava data na konec stare nahravky. */
            if (me->m->openSaveTape(saveName)) {
                Config::strTapFilePath = saveName;
                Config::SaveConfig();
            } else {
                fl_alert("Cannot create CSW file:\n%s", saveName.c_str());
            }
        }
    }

    me->m->setClockSpeed(20);
    if (wasRunning) me->m->startEmulation();
    me->restoreKeyboardFocus();
}

void Jondra::OnRecordTape(Fl_Widget *w, void *data) {
    Jondra* me = (Jondra*)data;
    (void)w;
    if (!me || !me->m || !me->btnRecTape) return;

    me->m->setTapeMode(me->btnRecTape->value() != 0);
    me->btnRecTape->image(me->btnRecTape->value() ? me->iconRecTape : me->iconPlayTape);
    me->btnRecTape->redraw();
    me->restoreKeyboardFocus();
}


static std::string snapshotFileNameOnly(const std::string& fileName) {
    size_t slash = fileName.find_last_of("/\\");
    if (slash == std::string::npos) return fileName;
    return fileName.substr(slash + 1);
}

static std::string snapshotRemoveExtension(const std::string& fileName) {
    size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return fileName;
    return fileName.substr(0, dot);
}

static std::string snapshotDirectoryOnly(const std::string& fileName) {
    size_t slash = fileName.find_last_of("/\\");
    if (slash == std::string::npos) return "";
    return fileName.substr(0, slash);
}

static std::string snapshotJoinPath(const std::string& directory, const std::string& fileName) {
    if (directory.empty()) return fileName;
    char last = directory[directory.length() - 1];
    if (last == '/' || last == '\\') return directory + fileName;
#ifdef _WIN32
    return directory + "\\" + fileName;
#else
    return directory + "/" + fileName;
#endif
}

/* Navrh dalsiho jmena:
   game -> game01, game01 -> game02, game009 -> game010.
   Pokud neni predchozi navrh, zacina se jmenem snap01. */
static std::string snapshotGetNextBaseName(const std::string& currentFileName) {
    std::string shortName = snapshotFileNameOnly(currentFileName);
    std::string noExt;
    size_t digitStart;

    if (shortName.empty()) return "snap01";

    noExt = snapshotRemoveExtension(shortName);
    if (noExt.empty()) return "snap01";

    digitStart = noExt.length();
    while (digitStart > 0 && noExt[digitStart - 1] >= '0' && noExt[digitStart - 1] <= '9') {
        digitStart--;
    }

    if (digitStart > 0 && digitStart < noExt.length()) {
        std::string digits = noExt.substr(digitStart);
        int width = (int)digits.length();
        long value = atol(digits.c_str()) + 1;
        char number[64];
        sprintf(number, "%0*ld", width, value);
        return noExt.substr(0, digitStart) + number;
    }

    return noExt + "01";
}

void Jondra::setSnapshotNameProposalFromFile(const std::string& fileName) {
    std::string proposal = snapshotRemoveExtension(snapshotFileNameOnly(fileName));
    snapshotNameProposal = proposal;
    screenshotNameProposal = proposal;
}

static std::string snapshotEnsureOsnExtension(const char* filename) {
    std::string result = filename ? filename : "";
    size_t dot = result.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = result.substr(dot + 1);
        for (size_t i = 0; i < ext.length(); i++) {
            if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] - 'A' + 'a');
        }
        if (ext == "osn") return result;
    }
    result += ".osn";
    return result;
}

void Jondra::OnOpenSnapshot(Fl_Widget *w, void *data) {
    Jondra* me = (Jondra*)data;
    bool wasPaused;
    const char* filename;
    std::string defaultPath;

    if (!me || !me->m) return;
    wasPaused = me->m->isPaused();
    me->m->stopEmulation();

    defaultPath = Config::strSnapFilePath;
    if (defaultPath.empty()) {
        defaultPath = Config::getMyPath();
    }
    filename = fl_file_chooser("Open snapshot", "*.osn", defaultPath.c_str());
    if (filename) {
        std::string snapName = snapshotEnsureOsnExtension(filename);
        int oldRomType = Config::getRomType();
        if (me->m->loadSnapshot(snapName)) {
            int snapshotRomType = Config::getRomType();
            me->m->setClockSpeed(20);
            Config::strSnapFilePath = snapName;

            /* ROM vybrana snapshotem plati jen pro bezici stroj. Ulozeni posledni
               cesty ke snapshotu nesmi prepsat ROM zvolenou v konfiguraci. */
            Config::setRomType(oldRomType);
            Config::SaveConfig();
            Config::setRomType(snapshotRomType);
        } else {
            fl_alert("Cannot load snapshot. The file is invalid, truncated, or uses the unsupported PLUS ROM.");
        }
    }

    if (!wasPaused) me->m->startEmulation();
    me->syncPauseButton();
    me->restoreKeyboardFocus();
}

void Jondra::OnSaveSnapshot(Fl_Widget *w, void *data) {
    Jondra* me = (Jondra*)data;
    bool wasPaused;
    const char* filename;
    std::string defaultPath;

    if (!me || !me->m) return;
    wasPaused = me->m->isPaused();
    me->m->stopEmulation();

    {
        std::string baseForName;
        std::string directory;
        std::string suggestedBase;

        if (!me->snapshotNameProposal.empty()) {
            baseForName = me->snapshotNameProposal;
        } else {
            baseForName = Config::strSnapFilePath;
        }

        suggestedBase = snapshotGetNextBaseName(baseForName);
        directory = snapshotDirectoryOnly(Config::strSnapFilePath);
        if (directory.empty()) {
            directory = Config::getMyPath();
        }
        defaultPath = snapshotJoinPath(directory, suggestedBase);
    }
    filename = fl_file_chooser("Save snapshot", "*.osn", defaultPath.c_str());
    if (filename) {
        std::string snapName = snapshotEnsureOsnExtension(filename);
        if (me->m->saveSnapshot(snapName)) {
            me->snapshotNameProposal = "";
            Config::strSnapFilePath = snapName;
            Config::SaveConfig();
        } else {
            fl_alert("Cannot save snapshot.");
        }
    }

    if (!wasPaused) me->m->startEmulation();
    me->syncPauseButton();
    me->restoreKeyboardFocus();
}


/* -------------------------------------------------------------------------
   Save screenshot

   Obrazovka je v Ondra::px ulozena jako 1bitovy obraz 320x256
   (40 bajtu na radek, MSB = levy pixel).

   To keep this compatible with VC6/Win98 and avoid adding another source file
   or an extra library dependency, the small PNG writer below emits an 8-bit
   grayscale PNG with uncompressed DEFLATE blocks. The resulting file is a
   perfectly ordinary PNG; for a 320x256 black/white image it is about 82 kB.
   ------------------------------------------------------------------------- */
static uint32_t screenshotCrc32Update(uint32_t crc, const unsigned char* data, size_t length) {
    size_t i;
    int bit;
    for (i = 0; i < length; ++i) {
        crc ^= (uint32_t)data[i];
        for (bit = 0; bit < 8; ++bit) {
            if (crc & 1U) crc = (crc >> 1) ^ 0xEDB88320UL;
            else crc >>= 1;
        }
    }
    return crc;
}

static void screenshotWriteU32BE(FILE* f, uint32_t value) {
    unsigned char b[4];
    b[0] = (unsigned char)((value >> 24) & 0xff);
    b[1] = (unsigned char)((value >> 16) & 0xff);
    b[2] = (unsigned char)((value >> 8) & 0xff);
    b[3] = (unsigned char)(value & 0xff);
    fwrite(b, 1, 4, f);
}

static bool screenshotWriteChunk(FILE* f, const char type[4], const unsigned char* data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    if (!f) return false;

    screenshotWriteU32BE(f, length);
    if (fwrite(type, 1, 4, f) != 4) return false;
    crc = screenshotCrc32Update(crc, (const unsigned char*)type, 4);

    if (length > 0) {
        if (!data || fwrite(data, 1, length, f) != length) return false;
        crc = screenshotCrc32Update(crc, data, length);
    }

    crc ^= 0xFFFFFFFFUL;
    screenshotWriteU32BE(f, crc);
    return ferror(f) == 0;
}

static uint32_t screenshotAdler32(const unsigned char* data, size_t length) {
    const uint32_t MOD_ADLER = 65521UL;
    uint32_t a = 1;
    uint32_t b = 0;
    size_t i;

    for (i = 0; i < length; ++i) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

static bool screenshotSavePng(const char* filename, const uint8_t* ondraScreen) {
    const int width = 320;
    const int height = 256;
    const int bytesPerOndraLine = 40;
    const size_t rowSize = (size_t)width + 1; /* PNG filter byte + pixels */
    const size_t rawSize = rowSize * (size_t)height;
    const size_t blockCount = (rawSize + 65534U) / 65535U;
    const size_t zlibSize = 2U + rawSize + blockCount * 5U + 4U;
    unsigned char* raw = NULL;
    unsigned char* zdata = NULL;
    size_t rawPos = 0;
    size_t zpos = 0;
    size_t remaining;
    size_t offset;
    int y, x;
    FILE* f = NULL;
    unsigned char ihdr[13];
    uint32_t adler;
    bool ok = false;

    if (!filename || !ondraScreen) return false;

    raw = new unsigned char[rawSize];
    zdata = new unsigned char[zlibSize];
    if (!raw || !zdata) {
        delete[] raw;
        delete[] zdata;
        return false;
    }

    /* Decode Ondra's packed 1-bit framebuffer to PNG grayscale pixels. */
    for (y = 0; y < height; ++y) {
        raw[rawPos++] = 0; /* filter type: None */
        for (x = 0; x < width; ++x) {
            unsigned char src = ondraScreen[y * bytesPerOndraLine + (x >> 3)];
            raw[rawPos++] = (src & (0x80 >> (x & 7))) ? 255 : 0;
        }
    }

    /* zlib header: deflate, 32K window, fastest/no compression. */
    zdata[zpos++] = 0x78;
    zdata[zpos++] = 0x01;

    remaining = rawSize;
    offset = 0;
    while (remaining > 0) {
        unsigned int len = (remaining > 65535U) ? 65535U : (unsigned int)remaining;
        unsigned int nlen = (~len) & 0xffffU;
        bool finalBlock = (remaining <= 65535U);

        /* Stored DEFLATE block. At a byte boundary BFINAL is bit 0. */
        zdata[zpos++] = finalBlock ? 0x01 : 0x00;
        zdata[zpos++] = (unsigned char)(len & 0xff);
        zdata[zpos++] = (unsigned char)((len >> 8) & 0xff);
        zdata[zpos++] = (unsigned char)(nlen & 0xff);
        zdata[zpos++] = (unsigned char)((nlen >> 8) & 0xff);
        memcpy(zdata + zpos, raw + offset, len);
        zpos += len;
        offset += len;
        remaining -= len;
    }

    adler = screenshotAdler32(raw, rawSize);
    zdata[zpos++] = (unsigned char)((adler >> 24) & 0xff);
    zdata[zpos++] = (unsigned char)((adler >> 16) & 0xff);
    zdata[zpos++] = (unsigned char)((adler >> 8) & 0xff);
    zdata[zpos++] = (unsigned char)(adler & 0xff);

    f = fopen(filename, "wb");
    if (f) {
        static const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        memset(ihdr, 0, sizeof(ihdr));
        ihdr[0] = 0; ihdr[1] = 0; ihdr[2] = 1; ihdr[3] = 64; /* 320 */
        ihdr[4] = 0; ihdr[5] = 0; ihdr[6] = 1; ihdr[7] = 0;  /* 256 */
        ihdr[8] = 8; /* bit depth */
        ihdr[9] = 0; /* grayscale */

        if (fwrite(signature, 1, 8, f) == 8 &&
            screenshotWriteChunk(f, "IHDR", ihdr, 13) &&
            screenshotWriteChunk(f, "IDAT", zdata, (uint32_t)zpos) &&
            screenshotWriteChunk(f, "IEND", NULL, 0)) {
            ok = true;
        }
        fclose(f);
        if (!ok) remove(filename);
    }

    delete[] raw;
    delete[] zdata;
    return ok;
}

static std::string screenshotGetNextBaseName(const std::string& currentFileName) {
    if (currentFileName.empty()) return "screen01";
    return snapshotGetNextBaseName(currentFileName);
}

static std::string screenshotEnsurePngExtension(const char* filename) {
    std::string result = filename ? filename : "";
    size_t dot = result.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = result.substr(dot + 1);
        size_t i;
        for (i = 0; i < ext.length(); ++i) {
            if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] - 'A' + 'a');
        }
        if (ext == "png") return result;
    }
    result += ".png";
    return result;
}

void Jondra::OnSaveScreenshot(Fl_Widget *w, void *data) {
    Jondra* me = (Jondra*)data;
    bool wasPaused;
    const char* filename;
    std::string baseForName;
    std::string directory;
    std::string suggestedBase;
    std::string defaultPath;

    if (!me || !me->m || !me->m->px) return;

    wasPaused = me->m->isPaused();
    me->m->stopEmulation();

    if (!me->screenshotNameProposal.empty()) baseForName = me->screenshotNameProposal;
    else baseForName = Config::strShotFilePath;

    suggestedBase = screenshotGetNextBaseName(baseForName);
    directory = snapshotDirectoryOnly(Config::strShotFilePath);
    if (directory.empty()) directory = Config::getMyPath();
    defaultPath = snapshotJoinPath(directory, suggestedBase);

    filename = fl_file_chooser("Save screenshot", "*.png", defaultPath.c_str());
    if (filename) {
        std::string shotName = screenshotEnsurePngExtension(filename);
        if (screenshotSavePng(shotName.c_str(), me->m->px)) {
            me->screenshotNameProposal = "";
            Config::strShotFilePath = shotName;
            Config::SaveConfig();
        } else {
            fl_alert("Cannot save screenshot.");
        }
    }

    if (!wasPaused) me->m->startEmulation();
    me->syncPauseButton();
    me->restoreKeyboardFocus();
}

void Jondra::close_about(Fl_Widget *w, void *win) {
	((Fl_Window*)win)->hide();  // Zavre okno
	if (Ondra::machine) {              		
		Ondra::machine->til->DirtyTilesAll();
	}
}

void Jondra::OnAbout(Fl_Widget *w, void *data) {
    Jondra* me = ((Jondra*)data);
    int win_w = 260, win_h = 130;
    int center_x = me->x() + (me->w() - win_w) / 2;
    int center_y = me->y() + (me->h() - win_h) / 2;
	
    // Vytvoreni okna
    Fl_Window *about_win = new Fl_Window(win_w, win_h, "About application");
    about_win->position(center_x, center_y);  // Zarovnani doprostred hlavniho okna
    about_win->begin();
	
    // Textove pole s informacemi
    Fl_Box *info = new Fl_Box(10, 20, 240, 50, "Ondra emulator\nOriginally written in Java,\nconverted to pure C++\nBuilt: 15.8.2026");
    info->labelsize(12);
    info->align(FL_ALIGN_CENTER);
	
    // Tlacitko OK
    Fl_Button *ok_btn = new Fl_Button(80, 90, 100, 25, "OK");
    ok_btn->callback(close_about, about_win);
	
    about_win->end();
    about_win->set_modal();
    about_win->show();
}



void Jondra::OnDebugger(Fl_Widget *w, void *data) {
    Jondra* me = ((Jondra*)data);
    bool wasRunning = false;

    if (me->m) {
        /* The debugger must read one fully stopped, consistent CPU state. */
        wasRunning = !me->m->isPaused();
        me->m->stopEmulation();
        me->btnPause->value(1);
        me->btnPause->updateState();
    }
    if (!me->debuggerWindow) {
        me->debuggerWindow = new DebuggerWindow(me, me->m);
    }
    me->debuggerWindow->showDialog(wasRunning);
}

void Jondra::syncPauseButton() {
    if (m == NULL || btnPause == NULL) {
        return;
    }
    btnPause->value(m->isPaused() ? 1 : 0);
    btnPause->updateState();
}

void Jondra::debuggerClosed(bool resumeEmulation) {
    if (m == NULL) {
        return;
    }

    /* Restore exactly the run/pause state that existed before opening it. */
    if (resumeEmulation) {
        m->startEmulation();
    }
    syncPauseButton();
}

void Jondra::OnSettings(Fl_Widget *w, void *data){
	Jondra* me=((Jondra*)data);
	me->m->stopEmulation();
	Settings *dlg = new Settings(me);
    bool ResetNeeded = false;
    bool ApplyFullscreen = false;
    bool ScanlinesChanged = false;
	
    if(dlg->showModal()==1){
		
		int nLastType=Config::getRomType();
		if(dlg->radioBasic->value()){
			Config::setRomType(0);
		}
		if(dlg->radioTesla->value()){
			Config::setRomType(1);
		}
		if(dlg->radioVili->value()){
			Config::setRomType(2);
		}
		if(dlg->radioCustom->value()){
			Config::setRomType(3);
			
			
		}
		if(nLastType!=Config::getRomType()) {
			ResetNeeded=true;
		}
		if(Config::getRomType()==3){
			if(dlg->textRomA->value()!=Config::getRomA()){
				Config::setRomA(dlg->textRomA->value());
				ResetNeeded=true;
			}
			if(dlg->textRomB->value()!=Config::getRomB()){
				Config::setRomB(dlg->textRomB->value());
				ResetNeeded=true;
			}
		}
		
		if
			(dlg->checkSound->value()){
			if(!Config::getAudio()) ResetNeeded=true;
			Config::setAudio(true);
		}else{
			if(Config::getAudio()) ResetNeeded=true;
			Config::setAudio(false);
		}
		if(dlg->checkMelodik->value()){
			if(!Config::getMelodik()) ResetNeeded=true;
			Config::setMelodik(true);
		}else{
			if(Config::getMelodik()) ResetNeeded=true;
			Config::setMelodik(false);
		}
		if(dlg->checkFullscreen->value()){
			Config::setFullscreen(true);
		}else{
			Config::setFullscreen(false);
		}
        {
            bool newScanlines = (dlg->checkScanlines->value() != 0);
            if (newScanlines != Config::getScanlines()) {
                ScanlinesChanged = true;
            }
            Config::setScanlines(newScanlines);
        }
		Config::SaveConfig();
        ApplyFullscreen = true;
		
		if(ResetNeeded) me->m->Reset(false);
	}
	
    delete dlg;
    if (ApplyFullscreen) {
        me->setFullscreenMode(Config::getFullscreen());
    }
    me->m_screen->setScanlines(Config::getScanlines());
    if (ScanlinesChanged && me->m && me->m->til) {
        /* The effect is presentation-only. Rebuild the complete frame once so
           enabling/disabling it is visible immediately (also while paused). */
        me->m->til->DirtyTilesAll();
        me->m->til->DispUpdate(me->m->px);
        me->m_screen->redraw();
        Fl::flush();
    }
	Fl::focus((Fl_Widget*)me->m->getKeyboard());
	me->m->startEmulation();
}




void Jondra::OnSaveMemoryBlock(Fl_Widget *w, void *data) {
    Jondra *me = (Jondra*)data;
    MemorySaveDialog *dlg;

    if (!me || !me->m) return;

    me->m->stopEmulation();

    /* MemorySaveDialog is created from a menu/toolbar callback.  FLTK 1.1
       creates Fl_Window as a subwindow if Fl_Group::current() is non-NULL.
       A subwindow has no window-manager decoration (and therefore no close
       button).  Force this dialog to be a real top-level window. */
    {
        Fl_Group *oldCurrent = Fl_Group::current();
        Fl_Group::current(0);
        dlg = new MemorySaveDialog(me);
        Fl_Group::current(oldCurrent);
    }

    if (dlg->showModal() == 1) {
        char *endptr = 0;
        long fromAddress;
        long toAddress;

        Config::strSaveBinFilePath = dlg->textFile->value() ? dlg->textFile->value() : "";

        fromAddress = strtol(dlg->textFrom->value(), &endptr, 16);
        if (!endptr || *endptr != '\0' || fromAddress < 0 || fromAddress > 0xffff) {
            fromAddress = 0;
        }

        endptr = 0;
        toAddress = strtol(dlg->textTo->value(), &endptr, 16);
        if (!endptr || *endptr != '\0' || toAddress < 0 || toAddress > 0xffff) {
            toAddress = 0;
        }

        Config::nSaveFromAddress = (int)fromAddress;
        Config::nSaveToAddress = (int)toAddress;
        Config::SaveConfig();

        if (Config::strSaveBinFilePath.empty()) {
            fl_alert("Please select a file.");
        } else if (Config::nSaveFromAddress > Config::nSaveToAddress) {
            fl_alert("The start address must not be greater than the end address.");
        } else {
            std::string fullPath = Config::correctFullPath(Config::strSaveBinFilePath.c_str());
            FILE *fOut = fopen(fullPath.c_str(), "wb");
            if (!fOut) {
                fl_alert("Can't create file:\n%s", fullPath.c_str());
            } else {
                unsigned char buffer[1024];
                int address = Config::nSaveFromAddress;
                bool writeOK = true;

                while (address <= Config::nSaveToAddress) {
                    int remaining = Config::nSaveToAddress - address + 1;
                    int count = (remaining > (int)sizeof(buffer)) ? (int)sizeof(buffer) : remaining;
                    int i;

                    for (i = 0; i < count; ++i) {
                        buffer[i] = me->m->mem->readByte(address + i);
                    }

                    if (fwrite(buffer, 1, count, fOut) != (size_t)count) {
                        writeOK = false;
                        break;
                    }
                    address += count;
                }

                if (fclose(fOut) != 0) writeOK = false;
                if (!writeOK) {
                    fl_alert("Can't save data to file:\n%s", fullPath.c_str());
                }
            }
        }
    }

    delete dlg;
    Fl::focus((Fl_Widget*)me->m->getKeyboard());
    me->m->startEmulation();
}

void Jondra::OnLoadMemoryBlock(Fl_Widget *w, void *data){
	
    // Nacteni bloku pameti.
	Jondra* me=((Jondra*)data);
	me->m->stopEmulation();
	BinOpen *dlg = new BinOpen(me);
	
	if (dlg->showModal() == 1) {
		Config::strBinFilePath=dlg->textBinFile->value();
		Config::bHeaderOn=dlg->checkHeaderOn->value()!=0;
		Config::bAllRam = dlg->checkAllRam->value()!=0;
		Config::bRunBin = dlg->checkRunBin->value()!=0;
		char* endptr=0;
		long nSavAdr = strtol(dlg->textSavAdr->value(), &endptr, 16);
		if (*endptr != '\0') {
			nSavAdr=0;
		}
		Config::nBeginBinAddress =nSavAdr;
		endptr=0;
		long nRunAdr = strtol(dlg->textRunAdr->value(), &endptr, 16);
		if (*endptr != '\0') {
			nRunAdr=0;
		}
		Config::nRunBinAddress =nRunAdr;
		Config::SaveConfig();
		me->LoadBinaryAndRun();
		
	}
	delete dlg;
	Fl::focus((Fl_Widget*)me->m->getKeyboard());
	me->m->startEmulation();
	
	
}

void Jondra::LoadBinSilently(std::string strFile){
	std::string fullPath = Config::correctFullPath(strFile.c_str());
	FILE* fIn = fopen(fullPath.c_str(), "rb");
    if (!fIn) {		
        return;
    }
		
    if (Config::bAllRam) {
        m->mem->mapRom(false);
    }
	
    if (Config::bHeaderOn) {
        // S hlavickou
        bool bFinish = false;
        while (!bFinish) {
            unsigned char bType;
            if (fread(&bType, 1, 1, fIn) != 1) {
                bFinish = true;
                break;
            }
			
            int nMemAdr = 0;
            int bBlockLen = 0;
			
            if (bType == 1) {
                // Blok dat k ulozeni do RAM
                unsigned char adrLo, adrHi, lenLo, lenHi;
                if (fread(&adrLo, 1, 1, fIn) != 1 || fread(&adrHi, 1, 1, fIn) != 1 ||
                    fread(&lenLo, 1, 1, fIn) != 1 || fread(&lenHi, 1, 1, fIn) != 1) {
                    break;
                }
				
                nMemAdr = adrLo + (adrHi << 8);
                bBlockLen = lenLo + (lenHi << 8);
				
                if (bBlockLen > 0) {
                    unsigned char* contents = new unsigned char[bBlockLen];
                    size_t bytesRead = fread(contents, 1, bBlockLen, fIn);
					
                    if (bytesRead > 0) {
                        for (size_t i = 0; i < bytesRead; i++) {
                            m->mem->writeByte(nMemAdr++, contents[i]);
                        }
                    }
                    delete[] contents;
                }
            } else if (bType == 2) {
                // Blok ke spusteni
                unsigned char adrLo, adrHi;
                if (fread(&adrLo, 1, 1, fIn) != 1 || fread(&adrHi, 1, 1, fIn) != 1) {
                    break;
                }
                nMemAdr = adrLo + (adrHi << 8);                
				CPU_SetPC(m->cpu, nMemAdr);
                bFinish = true;
                break;
            } else {
                m->Reset(true);
                break;
            }
        }
    } else {
        // Bez hlavicky
        int nMemAdr = Config::nBeginBinAddress;
        unsigned char* buffer = new unsigned char[1024];
		
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, 1024, fIn)) > 0) {
            for (size_t i = 0; i < bytesRead; i++) {
                m->mem->writeByte(nMemAdr++, buffer[i]);
            }
        }
		
        delete[] buffer;
		
        if (Config::bRunBin) {
			CPU_SetPC(m->cpu, Config::nRunBinAddress);            
        }
    }
	
    fclose(fIn);
    setSnapshotNameProposalFromFile(fullPath);
	
    // Nastavimme Ondru na spravnou rychlost a spustime emulaci
    m->setClockSpeed(20);
	m->til->DirtyTilesAll();
}


/*
 * Load the command-line image in the Ondra block format.
 * Command-line loading is deliberately independent of the settings in the
 * Load Memory Block dialog: it always expects the Ondra block header
 * (01=data block, 02=run address) and makes the whole RAM writable.
 */
bool Jondra::LoadArgumentImage(const std::string& strFile) {
    std::string fullPath = strFile;
    FILE* fIn = fopen(fullPath.c_str(), "rb");

    /* Relativni cesta se nejdriv zkusi v aktualnim pracovnim adresari a
     * potom v adresari emulatoru. */
    if (!fIn) {
        fullPath = Config::getMyPath() + strFile;
        fIn = fopen(fullPath.c_str(), "rb");
    }
    if (!fIn) {
        return false;
    }

    m->mem->mapRom(false);

    bool ok = true;
    bool runBlockFound = false;
    while (ok && !runBlockFound) {
        unsigned char blockType;
        if (fread(&blockType, 1, 1, fIn) != 1) {
            ok = false;
            break;
        }

        if (blockType == 1) {
            unsigned char header[4];
            if (fread(header, 1, 4, fIn) != 4) {
                ok = false;
                break;
            }

            int address = header[0] | (header[1] << 8);
            int remaining = header[2] | (header[3] << 8);
            unsigned char buffer[1024];

            while (remaining > 0) {
                int wanted = remaining > (int)sizeof(buffer) ? (int)sizeof(buffer) : remaining;
                size_t got = fread(buffer, 1, wanted, fIn);
                if (got != (size_t)wanted) {
                    ok = false;
                    break;
                }

                for (int i = 0; i < wanted; ++i) {
                    m->mem->writeByte(address, buffer[i]);
                    address = (address + 1) & 0xffff;
                }
                remaining -= wanted;
            }
        } else if (blockType == 2) {
            unsigned char runAddress[2];
            if (fread(runAddress, 1, 2, fIn) != 2) {
                ok = false;
                break;
            }
            CPU_SetPC(m->cpu, runAddress[0] | (runAddress[1] << 8));
            runBlockFound = true;
        } else {
            ok = false;
        }
    }

    fclose(fIn);

    if (!ok || !runBlockFound) {
        /* Do not leave the machine with ROM unmapped and a half-loaded image. */
        m->Reset(true);
        return false;
    }

    setSnapshotNameProposalFromFile(fullPath);
    m->setClockSpeed(20);
    if (m->til != NULL) {
        m->til->DirtyTilesAll();
    }
    return true;
}

void Jondra::LoadBinaryAndRun() {
	LoadBinSilently(Config::strBinFilePath);
}

std::string toLower(const std::string &s)
{
    std::string result = s;
    for (unsigned int i = 0; i < result.size(); ++i)
    {
        result[i] = (char)tolower(result[i]);
    }
    return result;
}

std::string replaceAll(const std::string& source, const std::string& from, const std::string& to)
{
    std::string result = source;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos)
    {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

int main(int argc, char **argv) {	
	if (argc > 1) {
		if (argc == 2) {
			/* File only: load the command-line image after the emulator starts. */
			strArgFile = argv[1];
		} else if (argc == 3) {
			/* File + hexadecimal address: let ROM run to this address first. */
			strArgFile = argv[1];
			nStartAddress = -1;

			if (strlen(argv[2]) > 0) {
				std::string strAddress = toLower(argv[2]);
				strAddress = replaceAll(strAddress, "0x", "");
				strAddress = replaceAll(strAddress, "h", "");

				if (!strAddress.empty()) {
					char* endptr = 0;
					long parsedAddress = strtol(strAddress.c_str(), &endptr, 16);
					if (endptr != strAddress.c_str() && *endptr == '\0' &&
						parsedAddress >= 0 && parsedAddress <= 0xffffL) {
						nStartAddress = (int32_t)parsedAddress;
					}
				}
			}
		}
	}
	
    Jondra *wnd = new Jondra(800, 600, "Ondra SPO 186");
    wnd->show(1, argv);
	//wnd->show();
	Jondra::setWindowIcon(wnd);
	Fl::add_timeout(0.1, Jondra::initEmulator_cb, wnd);
    return Fl::run();
	
}
