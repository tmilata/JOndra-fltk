#ifndef DEBUGGERWINDOW_H
#define DEBUGGERWINDOW_H

#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_RGB_Image.H>

#include "FlatButton.h"
#include "HexInput.h"

class Ondra;
class Jondra;

/*
 * Debugger window: CPU/memory state, Step Into, Step Over, Run/Stop and five
 * persistent execution breakpoint slots. Timeline uses delta records with
 * periodic copy-on-write RAM keyframes.
 */
class DebuggerWindow : public Fl_Window {
public:
    DebuggerWindow(Jondra* parent, Ondra* machine);
    virtual ~DebuggerWindow();
    virtual int handle(int event);

    void showDialog(bool resumeAfterClose);
    void setTimelineAvailable(bool available);
    void refreshData();

private:
    Jondra* mainWindow;
    Ondra* machine;
    bool resumeOnClose;
    bool stepOverActive;
    bool runBreakpointPollActive;
    unsigned int stepOverSuppressedBreakpointMask;

    struct CpuVisualState {
        unsigned int af, bc, de, hl;
        unsigned int afAlt, bcAlt, deAlt, hlAlt;
        unsigned int ix, iy, pc, sp;
        unsigned int r, i, im, flags;
    };

    CpuVisualState changeBaseline;
    bool changeBaselineValid;

    // Top execution controls.
    FlatButton* btnStepInto;
    FlatButton* btnStepOver;
    FlatButton* btnRun;

    Fl_RGB_Image* iconStepInto;
    Fl_RGB_Image* iconStepOver;
    Fl_RGB_Image* iconRun;
    Fl_RGB_Image* iconStop;

    // Main information columns.
    Fl_Text_Display* textAsm;
    Fl_Text_Display* textRegisters;
    Fl_Text_Display* textFlags;
    Fl_Text_Display* textStackPorts;

    Fl_Text_Buffer* bufferAsm;
    Fl_Text_Buffer* bufferRegisters;
    Fl_Text_Buffer* bufferFlags;
    Fl_Text_Buffer* bufferStackPorts;
    Fl_Text_Buffer* bufferStackPortStyle;
    Fl_Text_Buffer* bufferRegisterStyle;
    Fl_Text_Buffer* bufferFlagStyle;
    Fl_Text_Display::Style_Table_Entry changeStyleTable[2];
    Fl_Text_Display::Style_Table_Entry portStyleTable[2];

    // Five normal execution breakpoints.
    Fl_Check_Button* checkBreakpoint[5];
    HexInput* inputBreakpoint[5];
    unsigned int breakpointAddress[5];
    bool breakpointEnabled[5];
    unsigned int asmLineAddress[13];
    Fl_Box* labelTStates;

    // Memory view and memory breakpoints.
    Fl_Text_Display* textMemory;
    Fl_Text_Buffer* bufferMemory;
    Fl_Round_Button* radioHex;
    Fl_Round_Button* radioAssembler;
    Fl_Check_Button* checkMemWrite;
    HexInput* inputMemWrite;
    Fl_Check_Button* checkMemRead;
    HexInput* inputMemRead;
    HexInput* inputMemoryAddress;
    HexInput* inlineMemoryEditor;
    unsigned int inlineMemoryEditAddress;
    HexInput* inlineRegisterEditor;
    int inlineRegisterEditTarget;

    // Timeline controls.
    Fl_Group* timelineGroup;
    Fl_Check_Button* checkTimeline;
    Fl_Slider* sliderTimelineMain;
    Fl_Slider* sliderTimelineFine;
    Fl_Button* btnTimelineBack;
    Fl_Button* btnTimelineForward;
    Fl_Button* btnTimelineExport;
    bool timelineUpdating;
    int timelineFineLastValue;

    void centerOnParent();
    void loadIcons();
    Fl_Text_Display* createTextDisplay(int x, int y, int w, int h,
                                       Fl_Text_Buffer* buffer);

    Ondra* getMachine() const;
    void fillAsmCode();
    void fillRegisters();
    void fillFlags();
    void fillStackPorts();
    void fillBreakpoints();
    void fillMemoryView();
    bool readCpuVisualState(CpuVisualState& state) const;
    void captureChangeBaseline();
    void setRunButtonRunning(bool running);
    void finishDebuggerRunState();
    void applyBreakpointSlot(int slot);
    void applyMemoryBreakpointSlots();
    void applyAllBreakpointSlots();
    void saveBreakpointConfig();
    void suppressCurrentStepOverBreakpoint(unsigned int address);
    void restoreStepOverSuppressedBreakpoints();
    bool hasEnabledBreakpoints() const;
    unsigned int getMemoryAddress() const;
    void updateTimelineControls(bool selectLatest);
    bool loadTimelinePosition(int index, bool resetFine);
    void prepareTimelineForExecution();

    static void onClose(Fl_Widget* widget, void* data);
    static void onStepInto(Fl_Widget* widget, void* data);
    static void onStepOver(Fl_Widget* widget, void* data);
    static void onStepOverPoll(void* data);
    static void onUserBreakpointPoll(void* data);
    static void onRunStop(Fl_Widget* widget, void* data);
    static void onBreakpointToggle(Fl_Widget* widget, void* data);
    static void onBreakpointInput(Fl_Widget* widget, void* data);
    static void onMemoryBreakpointToggle(Fl_Widget* widget, void* data);
    static void onMemoryBreakpointInput(Fl_Widget* widget, void* data);
    static void onAsmDoubleClickLine(int line, void* data);
    static void onFlagDoubleClick(int flagIndex, void* data);
    static void onTStatesDoubleClick(void* data);
    static void onNoOperation(Fl_Widget* widget, void* data);
    static void onTimelineEnable(Fl_Widget* widget, void* data);
    static void onTimelineMain(Fl_Widget* widget, void* data);
    static void onTimelineFine(Fl_Widget* widget, void* data);
    static void onTimelineBack(Fl_Widget* widget, void* data);
    static void onTimelineForward(Fl_Widget* widget, void* data);
    static void onTimelineExport(Fl_Widget* widget, void* data);
    static void onRegisterDoubleClick(int target,
                                      int x, int y, int w, int h, void* data);
    static void onRegisterEditFinished(bool save, void* data);
    static void onMemoryMode(Fl_Widget* widget, void* data);
    static void onMemoryAddress(Fl_Widget* widget, void* data);
    static void onMemoryWheel(int delta, void* data);
    static void onMemoryByteDoubleClick(int row, int byteIndex,
                                        int x, int y, int w, int h, void* data);
    static void onMemoryEditFinished(bool save, void* data);
};

#endif
