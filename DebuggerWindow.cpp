#include "DebuggerWindow.h"
#include "EmbeddedResources.h"
#include "Config.h"
#include "Ondra.h"
#include "Jondra.h"
#include "Memory.h"
#include "DirtyTiles.h"
#include "Keyboard.h"
#include "cpuintf.h"
#include "z80disassembler.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <string>

/* The existing C++ disassembler uses this opcode array from z80emu.cpp. */
extern UBYTE Opcodes[65536];

namespace {
    const int WINDOW_W = 700;
    const int WINDOW_H = 684;
    const int MARGIN = 10;
    const int INPUT_H = 25;

    bool isTimelineShortcutEvent(int event) {
        int key;
        if (event != FL_KEYDOWN && event != FL_SHORTCUT) return false;
        if ((Fl::event_state() & FL_CTRL) == 0) return false;
        key = Fl::event_key();
        return key == FL_Left || key == FL_Right ||
               key == FL_Up || key == FL_Down;
    }

    /*
     * FLTK 1.1 sends keyboard events directly to the focused leaf widget.
     * Returning 0 eventually converts an unhandled key to FL_SHORTCUT, but
     * DebuggerWindow::handle() is not guaranteed to see that shortcut.
     * Fl::add_handler() is the FLTK 1.1 mechanism for window-independent
     * shortcuts, so use it to emulate Swing WHEN_IN_FOCUSED_WINDOW.
     */
    DebuggerWindow* timelineShortcutWindow = NULL;

    bool widgetBelongsToDebugger(Fl_Widget* widget, DebuggerWindow* dialog) {
        while (widget != NULL) {
            if (widget == dialog) {
                return true;
            }
            widget = widget->parent();
        }
        return false;
    }

    int timelineGlobalShortcutHandler(int event) {
        DebuggerWindow* dialog = timelineShortcutWindow;
        Fl_Widget* focus;

        if (event != FL_SHORTCUT || dialog == NULL || !dialog->visible()) {
            return 0;
        }

        /*
         * Fl::add_handler() is global to the application. Timeline shortcuts
         * must only work while the Debugger window owns keyboard focus, not
         * while the emulator's main window is active.
         */
        focus = Fl::focus();
        if (focus == NULL || !widgetBelongsToDebugger(focus, dialog)) {
            return 0;
        }

        if (!isTimelineShortcutEvent(event)) {
            return 0;
        }

        /*
         * Call the DebuggerWindow handler directly.  For a matching shortcut
         * it consumes the event before delegating to Fl_Window::handle().
         */
        return dialog->handle(event);
    }


    bool timelineFileExists(const std::string& path) {
        FILE* file = fopen(path.c_str(), "rb");
        if (file == NULL) return false;
        fclose(file);
        return true;
    }

    bool timelineHasTxtExtension(const std::string& path) {
        size_t n = path.size();
        if (n < 4) return false;
        return path[n - 4] == '.' &&
               tolower((unsigned char)path[n - 3]) == 't' &&
               tolower((unsigned char)path[n - 2]) == 'x' &&
               tolower((unsigned char)path[n - 1]) == 't';
    }

    std::string timelineBaseName() {
        std::string path = Config::strBinFilePath;
        size_t slash;
        size_t dot;

        if (path.empty()) return std::string();
        slash = path.find_last_of("/\\");
        if (slash != std::string::npos) path = path.substr(slash + 1);
        dot = path.find_last_of('.');
        if (dot != std::string::npos && dot > 0) path = path.substr(0, dot);
        return path;
    }

    std::string timelineDefaultExportPath() {
        std::string directory;
        std::string baseName = timelineBaseName();
        std::string fileName;
        char timestamp[32];
        time_t now = time(NULL);
        struct tm* local = localtime(&now);
        size_t slash;

        if (local != NULL) {
            strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", local);
        } else {
            strcpy(timestamp, "timeline");
        }

        if (!Config::strBinFilePath.empty()) {
            slash = Config::strBinFilePath.find_last_of("/\\");
            if (slash != std::string::npos) {
                directory = Config::strBinFilePath.substr(0, slash + 1);
            }
        }
        if (directory.empty()) directory = Config::getMyPath();

        if (!baseName.empty()) fileName = baseName + "-timeline-" + timestamp + ".txt";
        else fileName = std::string("timeline-") + timestamp + ".txt";

        if (!directory.empty() && directory[directory.size() - 1] != '/' &&
            directory[directory.size() - 1] != '\\') {
#ifdef _WIN32
            directory += '\\';
#else
            directory += '/';
#endif
        }
        return directory + fileName;
    }

    class TimelineSlider : public Fl_Slider {
    public:
        TimelineSlider(int x, int y, int w, int h)
            : Fl_Slider(x, y, w, h) {
        }

        virtual int handle(int event) {
            /* Ctrl+arrows belong to timeline navigation even while one of the
             * sliders has focus. */
            if (isTimelineShortcutEvent(event)) return 0;
            return Fl_Slider::handle(event);
        }
    };

    /*
     * The normal Fl_Text_Buffer highlight is theme/focus dependent.  On the
     * Windows 98 Silver theme it becomes so pale that the current instruction
     * is hard to see.  This small display subclass paints the first visible
     * ASM row itself, so the debugger always has the same blue current-line
     * marker regardless of the active Windows/FLTK theme.
     */
    class CurrentAsmTextDisplay : public Fl_Text_Display {
    public:
        typedef void (*LineDoubleClickCallback)(int line, void* data);

        CurrentAsmTextDisplay(int x, int y, int w, int h)
            : Fl_Text_Display(x, y, w, h),
              lineDoubleClickCallback(NULL), lineDoubleClickData(NULL) {
        }

        void setLineDoubleClickCallback(LineDoubleClickCallback cb, void* data) {
            lineDoubleClickCallback = cb;
            lineDoubleClickData = data;
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            int savedTopLine = mTopLineNum;
            int savedHorizOffset = mHorizOffset;
            int result = Fl_Text_Display::handle(event);

            /*
             * Keep this read-only preview fixed on a plain mouse click.
             * Some FLTK/Linux font metrics make Fl_Text_Display move the
             * viewport when the insertion position lands on the last visible
             * row.  Selection still works; only that implicit scroll is undone.
             */
            if (event == FL_PUSH && Fl::event_button() == 1 &&
                (mTopLineNum != savedTopLine ||
                 mHorizOffset != savedHorizOffset)) {
                scroll(savedTopLine, savedHorizOffset);
                redraw();
            }

            if (event == FL_PUSH && Fl::event_button() == 1 &&
                Fl::event_clicks() > 0 && lineDoubleClickCallback != NULL &&
                mMaxsize > 0) {
                int relY = Fl::event_y() - text_area.y;
                if (relY >= 0 && relY < text_area.h) {
                    int line = relY / mMaxsize;
                    lineDoubleClickCallback(line, lineDoubleClickData);
                    return 1;
                }
            }
            return result;
        }

    protected:
        virtual void draw() {
            Fl_Text_Buffer* buf;
            char* firstLine;

            Fl_Text_Display::draw();

            buf = buffer();
            if (buf == NULL || buf->length() == 0) {
                return;
            }

            firstLine = buf->line_text(0);
            if (firstLine == NULL) {
                return;
            }

            fl_push_clip(text_area.x, text_area.y, text_area.w, text_area.h);
            fl_color(fl_rgb_color(0x39, 0x69, 0x8A));
            fl_rectf(text_area.x, text_area.y, text_area.w, mMaxsize);

            fl_font(textfont(), textsize());
            fl_color(FL_WHITE);
            fl_draw(firstLine, text_area.x - mHorizOffset,
                    text_area.y + mMaxsize - fl_descent());
            fl_pop_clip();

            free(firstLine);
        }

    private:
        LineDoubleClickCallback lineDoubleClickCallback;
        void* lineDoubleClickData;
    };



    enum RegisterEditTarget {
        REGISTER_EDIT_AF = 0,
        REGISTER_EDIT_AF_ALT,
        REGISTER_EDIT_BC,
        REGISTER_EDIT_BC_ALT,
        REGISTER_EDIT_DE,
        REGISTER_EDIT_DE_ALT,
        REGISTER_EDIT_HL,
        REGISTER_EDIT_HL_ALT,
        REGISTER_EDIT_IX,
        REGISTER_EDIT_IY,
        REGISTER_EDIT_SP
    };

    /*
     * Register values are edited in-place.
     * A normal click is still handled by Fl_Text_Display (selection/copy),
     * while a double click directly on an editable four-digit value opens
     * an inline hexadecimal editor.  PC, R and I deliberately stay read-only
     * by design.
     */
    class RegisterTextDisplay : public Fl_Text_Display {
    public:
        typedef void (*RegisterDoubleClickCallback)(int target,
                                                    int x, int y, int w, int h,
                                                    void* data);

        RegisterTextDisplay(int x, int y, int w, int h)
            : Fl_Text_Display(x, y, w, h),
              registerDoubleClickCallback(NULL),
              registerDoubleClickData(NULL) {
        }

        void setRegisterDoubleClickCallback(RegisterDoubleClickCallback cb,
                                            void* data) {
            registerDoubleClickCallback = cb;
            registerDoubleClickData = data;
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            if (event == FL_PUSH && Fl::event_button() == 1 &&
                Fl::event_clicks() > 0 && registerDoubleClickCallback != NULL &&
                mMaxsize > 0) {
                int relY = Fl::event_y() - text_area.y;
                if (relY >= 0 && relY < text_area.h) {
                    int row = relY / mMaxsize;
                    int target = -1;
                    int charStart = -1;
                    double charWidth;
                    int relX;

                    fl_font(textfont(), textsize());
                    charWidth = fl_width("0");
                    if (charWidth <= 0.0) {
                        charWidth = 1.0;
                    }
                    relX = Fl::event_x() - text_area.x + mHorizOffset;

                    if (row >= 0 && row <= 3) {
                        int mainStart = (int)(4 * charWidth + 0.5);
                        int mainEnd = (int)(8 * charWidth + 0.5);
                        int altStart = (int)(14 * charWidth + 0.5);
                        int altEnd = (int)(18 * charWidth + 0.5);
                        int mainTargets[4] = {
                            REGISTER_EDIT_AF, REGISTER_EDIT_BC,
                            REGISTER_EDIT_DE, REGISTER_EDIT_HL
                        };
                        int altTargets[4] = {
                            REGISTER_EDIT_AF_ALT, REGISTER_EDIT_BC_ALT,
                            REGISTER_EDIT_DE_ALT, REGISTER_EDIT_HL_ALT
                        };

                        if (relX >= mainStart && relX < mainEnd) {
                            target = mainTargets[row];
                            charStart = 4;
                        } else if (relX >= altStart && relX < altEnd) {
                            target = altTargets[row];
                            charStart = 14;
                        }
                    } else if (row == 4 || row == 5 || row == 7) {
                        int valueStart = (int)(4 * charWidth + 0.5);
                        int valueEnd = (int)(8 * charWidth + 0.5);
                        if (relX >= valueStart && relX < valueEnd) {
                            charStart = 4;
                            if (row == 4) target = REGISTER_EDIT_IX;
                            else if (row == 5) target = REGISTER_EDIT_IY;
                            else target = REGISTER_EDIT_SP;
                        }
                    }

                    if (target >= 0 && charStart >= 0) {
                        int pixelStart = (int)(charStart * charWidth + 0.5);
                        int pixelEnd = (int)((charStart + 4) * charWidth + 0.5);
                        int editorX = text_area.x + pixelStart - mHorizOffset - 2;
                        int editorY = text_area.y + row * mMaxsize - 1;
                        int editorW = pixelEnd - pixelStart + 4;
                        int editorH = mMaxsize + 2;

                        if (buffer() != NULL) {
                            buffer()->unselect();
                            redraw();
                        }

                        registerDoubleClickCallback(target, editorX, editorY,
                                                    editorW, editorH,
                                                    registerDoubleClickData);
                        return 1;
                    }
                }
            }

            if (event == FL_PUSH && Fl::event_button() == 1) {
                int savedTopLine = mTopLineNum;
                int savedHorizOffset = mHorizOffset;
                int result = Fl_Text_Display::handle(event);

                if (mTopLineNum != savedTopLine ||
                    mHorizOffset != savedHorizOffset) {
                    scroll(savedTopLine, savedHorizOffset);
                    redraw();
                }
                return result;
            }

            return Fl_Text_Display::handle(event);
        }

    private:
        RegisterDoubleClickCallback registerDoubleClickCallback;
        void* registerDoubleClickData;
    };

    /*
     * A normal click keeps the usual text selection behaviour, while a
     * double click on one of the six flag
     * rows toggles that flag directly in register F.  Intercept the double
     * click before Fl_Text_Display handles it so it is not mistaken for a
     * word selection.
     */
    class FlagTextDisplay : public Fl_Text_Display {
    public:
        typedef void (*FlagDoubleClickCallback)(int flagIndex, void* data);

        FlagTextDisplay(int x, int y, int w, int h)
            : Fl_Text_Display(x, y, w, h),
              flagDoubleClickCallback(NULL), flagDoubleClickData(NULL) {
        }

        void setFlagDoubleClickCallback(FlagDoubleClickCallback cb, void* data) {
            flagDoubleClickCallback = cb;
            flagDoubleClickData = data;
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            if (event == FL_PUSH && Fl::event_button() == 1 &&
                Fl::event_clicks() > 0 && flagDoubleClickCallback != NULL &&
                mMaxsize > 0) {
                int relY = Fl::event_y() - text_area.y;
                if (relY >= 0 && relY < text_area.h) {
                    int row = relY / mMaxsize;
                    if (row >= 0 && row < 6) {
                        if (buffer() != NULL) {
                            buffer()->unselect();
                            redraw();
                        }
                        flagDoubleClickCallback(row, flagDoubleClickData);
                        return 1;
                    }
                }
            }
            if (event == FL_PUSH && Fl::event_button() == 1) {
                int savedTopLine = mTopLineNum;
                int savedHorizOffset = mHorizOffset;
                int result = Fl_Text_Display::handle(event);

                if (mTopLineNum != savedTopLine ||
                    mHorizOffset != savedHorizOffset) {
                    scroll(savedTopLine, savedHorizOffset);
                    redraw();
                }
                return result;
            }

            return Fl_Text_Display::handle(event);
        }

    private:
        FlagDoubleClickCallback flagDoubleClickCallback;
        void* flagDoubleClickData;
    };


    /*
     * A double click on the T:... field resets the global T-state counter.
     * Fl_Box itself ignores mouse events, therefore this
     * tiny subclass only adds double-click handling while keeping the exact
     * same visual appearance.
     */
    class TStatesBox : public Fl_Box {
    public:
        typedef void (*DoubleClickCallback)(void* data);

        TStatesBox(int x, int y, int w, int h, const char* label)
            : Fl_Box(x, y, w, h, label),
              doubleClickCallback(NULL), doubleClickData(NULL) {
        }

        void setDoubleClickCallback(DoubleClickCallback cb, void* data) {
            doubleClickCallback = cb;
            doubleClickData = data;
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            if (event == FL_PUSH && Fl::event_button() == 1) {
                if (Fl::event_clicks() > 0 && doubleClickCallback != NULL) {
                    doubleClickCallback(doubleClickData);
                }
                /* Accept the first click too so FLTK can recognize a double click. */
                return 1;
            }
            return Fl_Box::handle(event);
        }

    private:
        DoubleClickCallback doubleClickCallback;
        void* doubleClickData;
    };

    /*
     * The Memory pane does not scroll the text widget itself. Its mouse wheel
     * changes the start address and rebuilds the view.
     * Keep the same behaviour here; this also works with old FLTK 1.1.x used
     * by the Windows 98 / MSVC 6 build.
     */
    class MemoryTextDisplay : public Fl_Text_Display {
    public:
        typedef void (*WheelCallback)(int delta, void* data);
        typedef void (*ByteDoubleClickCallback)(int row, int byteIndex,
                                                int x, int y, int w, int h,
                                                void* data);

        MemoryTextDisplay(int x, int y, int w, int h)
            : Fl_Text_Display(x, y, w, h),
              wheelCallback(NULL), wheelCallbackData(NULL),
              byteDoubleClickCallback(NULL), byteDoubleClickData(NULL) {
        }

        void setWheelCallback(WheelCallback cb, void* data) {
            wheelCallback = cb;
            wheelCallbackData = data;
        }

        void setByteDoubleClickCallback(ByteDoubleClickCallback cb, void* data) {
            byteDoubleClickCallback = cb;
            byteDoubleClickData = data;
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            if (event == FL_MOUSEWHEEL && wheelCallback != NULL) {
                int delta = Fl::event_dy();
                if (delta != 0) {
                    wheelCallback(delta, wheelCallbackData);
                    return 1;
                }
            }

            if (event == FL_PUSH && Fl::event_button() == 1 &&
                Fl::event_clicks() > 0 && byteDoubleClickCallback != NULL &&
                mMaxsize > 0) {
                int relY = Fl::event_y() - text_area.y;
                if (relY >= 0 && relY < text_area.h) {
                    int row = relY / mMaxsize;
                    double charWidth;
                    int relX;
                    int byteIndex;

                    fl_font(textfont(), textsize());
                    charWidth = fl_width("0");
                    if (charWidth <= 0.0) {
                        charWidth = 1.0;
                    }

                    relX = Fl::event_x() - text_area.x + mHorizOffset;
                    for (byteIndex = 0; byteIndex < 8; ++byteIndex) {
                        int charStart = 7 + byteIndex * 3;
                        int pixelStart = (int)(charStart * charWidth + 0.5);
                        int pixelEnd = (int)((charStart + 2) * charWidth + 0.5);

                        if (relX >= pixelStart && relX < pixelEnd) {
                            /*
                             * A double click that opens
                             * the byte editor must not leave the underlying
                             * Fl_Text_Display text selected.  The first click
                             * of a double click may already have created a
                             * primary selection, so clear it before showing
                             * the inline editor.  Normal click/drag selection
                             * remains available for copying text.
                             */
                            if (buffer() != NULL) {
                                buffer()->unselect();
                                redraw();
                            }

                            int editorX = text_area.x + pixelStart - mHorizOffset - 2;
                            int editorY = text_area.y + row * mMaxsize - 1;
                            int editorW = pixelEnd - pixelStart + 4;
                            int editorH = mMaxsize + 2;
                            byteDoubleClickCallback(row, byteIndex,
                                                    editorX, editorY,
                                                    editorW, editorH,
                                                    byteDoubleClickData);
                            return 1;
                        }
                    }
                }
            }

            /*
             * Fl_Text_Display tries to keep its insertion position visible.
             * With the Linux font metrics a plain click in the last visible
             * Memory row can therefore move the internal viewport up by one
             * line, even though the debugger itself never requested a scroll.
             * Keep this pane fixed by preserving the current viewport around
             * ordinary left-click handling. Selection/copy
             * still works; only the implicit viewport movement is undone.
             */
            if (event == FL_PUSH && Fl::event_button() == 1) {
                int savedTopLine = mTopLineNum;
                int savedHorizOffset = mHorizOffset;
                int result = Fl_Text_Display::handle(event);

                if (mTopLineNum != savedTopLine ||
                    mHorizOffset != savedHorizOffset) {
                    scroll(savedTopLine, savedHorizOffset);
                    redraw();
                }
                return result;
            }

            return Fl_Text_Display::handle(event);
        }

    private:
        WheelCallback wheelCallback;
        void* wheelCallbackData;
        ByteDoubleClickCallback byteDoubleClickCallback;
        void* byteDoubleClickData;
    };

    /*
     * Address editor used by debugger breakpoint fields and the Memory
     * navigator. Breakpoint address editing ends on both
     * Enter and Escape (transferFocus()).  The generic HexInput deliberately
     * consumes those keys, so without this specialization the caret remains
     * visible in the address field after editing.
     *
     * Clearing FLTK focus causes HexInput::handle(FL_UNFOCUS) to run, which
     * invokes the normal field callback exactly once.  Thus Enter/Escape both
     * finish editing without any special callback duplication.
     */
    class DebuggerAddressInput : public HexInput {
    public:
        DebuggerAddressInput(int x, int y, int w, int h, int digits)
            : HexInput(x, y, w, h, "", digits) {
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            if (event == FL_KEYDOWN) {
                int key = Fl::event_key();
                if (key == FL_Enter || key == FL_Escape) {
                    Fl::focus(NULL);
                    return 1;
                }
            }
            return HexInput::handle(event);
        }
    };

    class InlineHexInput : public HexInput {
    public:
        typedef void (*FinishCallback)(bool save, void* data);

        InlineHexInput(int x, int y, int w, int h, int digits)
            : HexInput(x, y, w, h, "", digits),
              finishCallback(NULL), finishData(NULL), finishing(false) {
        }

        void setFinishCallback(FinishCallback cb, void* data) {
            finishCallback = cb;
            finishData = data;
        }

        void prepare() {
            finishing = false;
        }

        void dismissSilently() {
            finishing = true;
            hide();
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            if (event == FL_KEYDOWN) {
                int key = Fl::event_key();
                if (key == FL_Enter) {
                    finish(true);
                    return 1;
                }
                if (key == FL_Escape) {
                    finish(false);
                    return 1;
                }
            }

            if (event == FL_UNFOCUS) {
                int result = Fl_Input::handle(event);
                if (!finishing) {
                    finish(false);
                }
                return result;
            }

            return HexInput::handle(event);
        }

    private:
        void finish(bool save) {
            if (finishing) {
                return;
            }
            finishing = true;
            if (finishCallback != NULL) {
                finishCallback(save, finishData);
            }
        }

        FinishCallback finishCallback;
        void* finishData;
        bool finishing;
    };


    /*
     * FLTK 1.1 runs syntax/style colors through fl_contrast().  That is useful
     * for normal text, but it turns the deliberately pale #D7D7D7 bit
     * headings dark again on a white background. Paint only those three
     * heading rows after the normal text display has drawn itself, so their
     * color is exact and independent of the Windows/FLTK theme.
     */
    class PortTextDisplay : public Fl_Text_Display {
    public:
        PortTextDisplay(int x, int y, int w, int h)
            : Fl_Text_Display(x, y, w, h) {
        }

        virtual int handle(int event) {
            if (isTimelineShortcutEvent(event)) return 0;
            /*
             * Keep the Stack/Ports pane fixed when the user clicks in it.
             * On Linux, Fl_Text_Display may otherwise move its viewport by
             * one line when the last visible row receives the insertion
             * position.  Preserve ordinary text selection/copy, but undo
             * only that implicit scroll, just like in MemoryTextDisplay.
             */
            if (event == FL_PUSH && Fl::event_button() == 1) {
                int savedTopLine = mTopLineNum;
                int savedHorizOffset = mHorizOffset;
                int result = Fl_Text_Display::handle(event);

                if (mTopLineNum != savedTopLine ||
                    mHorizOffset != savedHorizOffset) {
                    scroll(savedTopLine, savedHorizOffset);
                    redraw();
                }
                return result;
            }

            return Fl_Text_Display::handle(event);
        }

    protected:
        virtual void draw() {
            static const int headingLine[3] = { 8, 11, 14 }; /* 1-based */
            static const char* heading = "7 6 5 4 3 2 1 0";
            int i;

            Fl_Text_Display::draw();

            fl_push_clip(text_area.x, text_area.y, text_area.w, text_area.h);
            fl_font(textfont(), textsize());

            for (i = 0; i < 3; ++i) {
                int visualRow = headingLine[i] - mTopLineNum;
                int y;

                if (visualRow < 0) {
                    continue;
                }

                y = text_area.y + visualRow * mMaxsize;
                if (y >= text_area.y + text_area.h) {
                    continue;
                }

                /* Erase the dark fl_contrast() result, then draw exact heading gray. */
                fl_color(color());
                fl_rectf(text_area.x, y, text_area.w, mMaxsize);
                fl_color(fl_rgb_color(215, 215, 215));
                fl_draw(heading, text_area.x - mHorizOffset,
                        y + mMaxsize - fl_descent());
            }

            fl_pop_clip();
        }
    };

    void formatTStates(char* out, uint64_t value) {
#ifdef _WIN32
        sprintf(out, "T:%019I64u", value);
#else
        sprintf(out, "T:%019llu", (unsigned long long)value);
#endif
    }

    void formatPortBits(char* out, unsigned char value) {
        int bit;
        char* p = out;
        for (bit = 7; bit >= 0; --bit) {
            *p++ = (value & (1 << bit)) ? '1' : '0';
            if (bit != 0) {
                *p++ = ' ';
            }
        }
        *p = '\0';
    }

    void markChangedStyle(std::string& style, unsigned int start,
                          unsigned int length, bool changed) {
        unsigned int i;
        if (!changed) {
            return;
        }
        for (i = start; i < start + length && i < style.size(); ++i) {
            style[i] = 'B';
        }
    }

    void appendStyledPair(std::string& text, std::string& style,
                          const char* left, bool leftChanged,
                          const char* right, bool rightChanged) {
        unsigned int start = (unsigned int)text.size();
        unsigned int leftLen = (unsigned int)strlen(left);
        unsigned int rightLen = (unsigned int)strlen(right);
        std::string line = std::string(left) + " " + right + "\n";

        text += line;
        style.append(line.size(), 'A');
        markChangedStyle(style, start, leftLen, leftChanged);
        markChangedStyle(style, start + leftLen + 1, rightLen, rightChanged);
    }

    void appendStyledSingle(std::string& text, std::string& style,
                            const char* token, bool changed) {
        unsigned int start = (unsigned int)text.size();
        unsigned int tokenLen = (unsigned int)strlen(token);
        std::string line = std::string(token) + "\n";

        text += line;
        style.append(line.size(), 'A');
        markChangedStyle(style, start, tokenLen, changed);
    }

    unsigned int debuggerOpcodeLength(Ondra* m, unsigned int address) {
        unsigned int len;
        unsigned int i;

        if (m == NULL || m->mem == NULL) {
            return 1;
        }

        address &= 0xffff;
        if (address > 0xfffb) {
            return 1;
        }

        for (i = 0; i < 5; ++i) {
            Opcodes[address + i] = m->mem->readByte((address + i) & 0xffff);
        }

        len = OpcodeLen(address);
        if (len < 1 || len > 4) {
            len = 1;
        }
        return len;
    }

    /*
     * Estimate the previous instruction boundary by starting
     * disassembly 30..39 bytes before the visible address.  The most common
     * predecessor from those ten passes is used for wheel-up in ASM mode.
     * This is intentionally preserved instead of assuming that address-1 is
     * an opcode boundary.
     */
    unsigned int debuggerPreviousInstructionStep(Ondra* m, unsigned int address) {
        unsigned int candidates[10];
        unsigned int uniqueAddress[10];
        int counts[10];
        int candidateCount = 0;
        int uniqueCount = 0;
        int shift;
        int i;
        unsigned int bestAddress;
        int bestCount;
        unsigned int step;

        address &= 0xffff;

        for (shift = 30; shift < 40; ++shift) {
            unsigned int p = (address + 0x10000UL - (unsigned int)shift) & 0xffff;
            unsigned int consumed = 0;
            unsigned int lastAddress = p;
            int guard = 0;

            while (guard < 64) {
                unsigned int len = debuggerOpcodeLength(m, p);

                if (consumed + len > (unsigned int)shift) {
                    candidates[candidateCount++] = lastAddress;
                    break;
                }

                lastAddress = p;
                p = (p + len) & 0xffff;
                consumed += len;
                ++guard;
            }

            if (guard >= 64 && candidateCount < 10) {
                candidates[candidateCount++] = (address + 0xffffUL) & 0xffff;
            }
        }

        for (i = 0; i < 10; ++i) {
            uniqueAddress[i] = 0;
            counts[i] = 0;
        }

        for (i = 0; i < candidateCount; ++i) {
            int j;
            int found = 0;
            for (j = 0; j < uniqueCount; ++j) {
                if (uniqueAddress[j] == candidates[i]) {
                    counts[j]++;
                    found = 1;
                    break;
                }
            }
            if (!found && uniqueCount < 10) {
                uniqueAddress[uniqueCount] = candidates[i];
                counts[uniqueCount] = 1;
                uniqueCount++;
            }
        }

        if (uniqueCount == 0) {
            return 1;
        }

        bestAddress = uniqueAddress[0];
        bestCount = 0;
        for (i = 0; i < uniqueCount; ++i) {
            /* With >= the later candidate wins a tie. */
            if (counts[i] >= bestCount) {
                bestCount = counts[i];
                bestAddress = uniqueAddress[i];
            }
        }

        step = (address + 0x10000UL - bestAddress) & 0xffff;
        if (step == 0) {
            step = 1;
        }
        return step;
    }


    /*
     * Format one instruction in the debugger's assembler form:
     *   #8000 3E00     LD A,#00
     *
     * The original C++ disassembler has a 64K opcode array.  Close to FFFFh
     * we deliberately fall back to a one-byte DB line rather than indexing
     * beyond that old array.  Normal addresses use the existing disassembler
     * unchanged.
     */
    unsigned int formatInstructionLine(Ondra* m, unsigned int address,
                                       char* line) {
        char instruction[1024];
        char bytes[32];
        char oneByte[8];
        unsigned int len;
        unsigned int i;

        address &= 0xffff;
        instruction[0] = '\0';
        bytes[0] = '\0';

        if (address > 0xfffb) {
            unsigned int value = m->mem->readByte(address) & 0xff;
            sprintf(line, "#%04X %02X       DB #%02X\n",
                    address, value, value);
            return 1;
        }

        len = debuggerOpcodeLength(m, address);

        for (i = 0; i < len; ++i) {
            sprintf(oneByte, "%02X", (unsigned int)Opcodes[address + i]);
            strcat(bytes, oneByte);
        }
        for (i = len; i < 4; ++i) {
            strcat(bytes, "  ");
        }

        Disassemble(address, instruction);
        sprintf(line, "#%04X %s %s\n", address, bytes, instruction);
        return len;
    }
}

DebuggerWindow::DebuggerWindow(Jondra* parent, Ondra* inMachine)
    : Fl_Window(WINDOW_W, WINDOW_H, "Debugger"),
      mainWindow(parent), machine(inMachine), resumeOnClose(false), stepOverActive(false),
      runBreakpointPollActive(false), stepOverSuppressedBreakpointMask(0),
      changeBaselineValid(false),
      btnStepInto(NULL), btnStepOver(NULL), btnRun(NULL),
      iconStepInto(NULL), iconStepOver(NULL), iconRun(NULL), iconStop(NULL),
      textAsm(NULL), textRegisters(NULL), textFlags(NULL),
      textStackPorts(NULL),
      bufferAsm(new Fl_Text_Buffer()),
      bufferRegisters(new Fl_Text_Buffer()),
      bufferFlags(new Fl_Text_Buffer()),
      bufferStackPorts(new Fl_Text_Buffer()),
      bufferStackPortStyle(new Fl_Text_Buffer()),
      bufferRegisterStyle(new Fl_Text_Buffer()),
      bufferFlagStyle(new Fl_Text_Buffer()),
      labelTStates(NULL),
      textMemory(NULL), bufferMemory(new Fl_Text_Buffer()),
      radioHex(NULL), radioAssembler(NULL),
      checkMemWrite(NULL), inputMemWrite(NULL),
      checkMemRead(NULL), inputMemRead(NULL),
      inputMemoryAddress(NULL), inlineMemoryEditor(NULL),
      inlineMemoryEditAddress(0), inlineRegisterEditor(NULL),
      inlineRegisterEditTarget(-1), timelineGroup(NULL),
      checkTimeline(NULL), sliderTimelineMain(NULL),
      sliderTimelineFine(NULL), btnTimelineBack(NULL),
      btnTimelineForward(NULL), btnTimelineExport(NULL),
      timelineUpdating(false), timelineFineLastValue(0) {

    int i;
    for (i = 0; i < 5; ++i) {
        checkBreakpoint[i] = NULL;
        inputBreakpoint[i] = NULL;
        breakpointAddress[i] = 0;
        breakpointEnabled[i] = false;
    }
    /* Five persistent slots are stored in Jondra.config. */
    breakpointEnabled[0] = Config::bBP1;
    breakpointEnabled[1] = Config::bBP2;
    breakpointEnabled[2] = Config::bBP3;
    breakpointEnabled[3] = Config::bBP4;
    breakpointEnabled[4] = Config::bBP5;
    breakpointAddress[0] = Config::nBP1Address >= 0 ? (unsigned int)Config::nBP1Address & 0xffff : 0;
    breakpointAddress[1] = Config::nBP2Address >= 0 ? (unsigned int)Config::nBP2Address & 0xffff : 0;
    breakpointAddress[2] = Config::nBP3Address >= 0 ? (unsigned int)Config::nBP3Address & 0xffff : 0;
    breakpointAddress[3] = Config::nBP4Address >= 0 ? (unsigned int)Config::nBP4Address & 0xffff : 0;
    breakpointAddress[4] = Config::nBP5Address >= 0 ? (unsigned int)Config::nBP5Address & 0xffff : 0;

    for (i = 0; i < 13; ++i) {
        asmLineAddress[i] = 0;
    }

    set_non_modal();
    callback(onClose, this);
    size_range(WINDOW_W, WINDOW_H, WINDOW_W, WINDOW_H);
    centerOnParent();

    begin();

    loadIcons();

    // ============================================================
    // EXECUTION TOOLBAR
    // ============================================================
    btnStepInto = new FlatButton(MARGIN, 10, 30, 30);
    btnStepInto->image(iconStepInto);
    btnStepInto->tooltip("Step Into");
    btnStepInto->callback(onStepInto, this);

    btnStepOver = new FlatButton(44, 10, 30, 30);
    btnStepOver->image(iconStepOver);
    btnStepOver->tooltip("Step Over");
    btnStepOver->callback(onStepOver, this);

    btnRun = new FlatButton(78, 10, 30, 30);
    btnRun->image(iconRun);
    btnRun->tooltip("Run");
    btnRun->callback(onRunStop, this);

    // ============================================================
    // MAIN DEBUGGER COLUMNS
    // ============================================================
    /*
     * Top row is deliberately balanced for the larger 15 px Win98 font.
     * AF'/BC'/DE'/HL' need more room with the 15 px Win98 font; the ASM,
     * flags and port panes have spare horizontal space.
     */
    {
        CurrentAsmTextDisplay* asmDisplay = new CurrentAsmTextDisplay(10, 48, 250, 276);
        asmDisplay->setLineDoubleClickCallback(onAsmDoubleClickLine, this);
        textAsm = asmDisplay;
    }
    textAsm->buffer(bufferAsm);
    textAsm->textfont(FL_COURIER);
    textAsm->textsize(15);
    textAsm->box(FL_DOWN_BOX);
    /* These debugger panes are fixed views. Disable FLTK auto-scrollbars,
       otherwise different
       font metrics on Linux can make them appear spuriously. */
    textAsm->scrollbar_align((Fl_Align)0);
    {
        RegisterTextDisplay* registerDisplay = new RegisterTextDisplay(270, 48, 180, 276);
        registerDisplay->buffer(bufferRegisters);
        registerDisplay->textfont(FL_COURIER);
        registerDisplay->textsize(15);
        registerDisplay->box(FL_DOWN_BOX);
        registerDisplay->scrollbar_align((Fl_Align)0);
        registerDisplay->setRegisterDoubleClickCallback(onRegisterDoubleClick, this);
        textRegisters = registerDisplay;
    }
    {
        FlagTextDisplay* flagDisplay = new FlagTextDisplay(460, 48, 45, 276);
        flagDisplay->buffer(bufferFlags);
        flagDisplay->textfont(FL_COURIER);
        flagDisplay->textsize(15);
        flagDisplay->box(FL_DOWN_BOX);
        flagDisplay->scrollbar_align((Fl_Align)0);
        flagDisplay->setFlagDoubleClickCallback(onFlagDoubleClick, this);
        textFlags = flagDisplay;
    }
    textStackPorts = new PortTextDisplay(515, 48, 165, 276);
    textStackPorts->buffer(bufferStackPorts);
    textStackPorts->textfont(FL_COURIER);
    textStackPorts->textsize(15);
    textStackPorts->box(FL_DOWN_BOX);
    textStackPorts->scrollbar_align((Fl_Align)0);

    /*
     * Changed values are emphasized in blue + bold.
     * The same style is also used for registers so a Step immediately shows
     * which CPU state actually changed.  Each display needs its own parallel
     * style buffer, but both can share this persistent style table.
     */
    changeStyleTable[0].color = FL_BLACK;
    changeStyleTable[0].font = FL_COURIER;
    changeStyleTable[0].size = 15;
    changeStyleTable[0].attr = 0;
    changeStyleTable[1].color = fl_rgb_color(0x2B, 0x6B, 0xA8);
    changeStyleTable[1].font = FL_COURIER_BOLD;
    changeStyleTable[1].size = 15;
    changeStyleTable[1].attr = 0;
    textRegisters->highlight_data(bufferRegisterStyle, changeStyleTable, 2,
                                  'A', NULL, NULL);
    textFlags->highlight_data(bufferFlagStyle, changeStyleTable, 2,
                              'A', NULL, NULL);

    /* Hidden until the user double-clicks an editable register value. */
    {
        InlineHexInput* editor = new InlineHexInput(290, 48, 42, 20, 4);
        editor->textfont(FL_COURIER);
        editor->textsize(15);
        editor->setFinishCallback(onRegisterEditFinished, this);
        editor->hide();
        inlineRegisterEditor = editor;
    }

    /*
     * Port bit-position headings are intentionally light gray (#D7D7D7),
     * while port names and actual 0/1 values stay
     * black.  A style buffer is used instead of changing the whole widget.
     */
    portStyleTable[0].color = FL_BLACK;
    portStyleTable[0].font = FL_COURIER;
    portStyleTable[0].size = 15;
    portStyleTable[0].attr = 0;
    portStyleTable[1].color = fl_rgb_color(215, 215, 215);
    portStyleTable[1].font = FL_COURIER;
    portStyleTable[1].size = 15;
    portStyleTable[1].attr = 0;
    textStackPorts->highlight_data(bufferStackPortStyle, portStyleTable, 2,
                                   'A', NULL, NULL);

    // ============================================================
    // FIVE EXECUTION BREAKPOINTS + T-STATES
    // ============================================================
    {
        const int y = 334;
        const int checkW = 20;
        const int inputW = 58;
        const int gap = 8;
        int x = 10;

        for (i = 0; i < 5; ++i) {
            checkBreakpoint[i] = new Fl_Check_Button(x, y, checkW, INPUT_H, "");
            checkBreakpoint[i]->callback(onBreakpointToggle, this);
            x += checkW;

            inputBreakpoint[i] = new DebuggerAddressInput(x, y, inputW, INPUT_H, 4);
            inputBreakpoint[i]->value("0000");
            inputBreakpoint[i]->textfont(FL_COURIER_BOLD);
            inputBreakpoint[i]->textsize(12);
            inputBreakpoint[i]->callback(onBreakpointInput, this);
            x += inputW + gap;
        }

        TStatesBox* tStatesBox = new TStatesBox(
            x + 3, y, WINDOW_W - MARGIN - x - 3,
            INPUT_H, "T:0000000000000000000");
        tStatesBox->setDoubleClickCallback(onTStatesDoubleClick, this);
        labelTStates = tStatesBox;
        labelTStates->labelfont(FL_COURIER_BOLD);
        labelTStates->labelsize(12);
        labelTStates->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        labelTStates->box(FL_DOWN_BOX);
    }

    // ============================================================
    // MEMORY VIEW
    // ============================================================
    {
        MemoryTextDisplay* memoryDisplay = new MemoryTextDisplay(10, 370, 415, 132);
        memoryDisplay->buffer(bufferMemory);
        memoryDisplay->textfont(FL_COURIER);
        memoryDisplay->textsize(15);
        memoryDisplay->box(FL_DOWN_BOX);
        memoryDisplay->scrollbar_align((Fl_Align)0);
        memoryDisplay->setWheelCallback(onMemoryWheel, this);
        memoryDisplay->setByteDoubleClickCallback(onMemoryByteDoubleClick, this);
        textMemory = memoryDisplay;
    }

    Fl_Group* memoryModeGroup = new Fl_Group(435, 370, 96, 60);
    memoryModeGroup->begin();
    radioHex = new Fl_Round_Button(435, 370, 90, 25, "Hex");
    radioHex->type(FL_RADIO_BUTTON);
    radioHex->value(Config::bShowCode ? 1 : 0);
    radioHex->callback(onMemoryMode, this);

    radioAssembler = new Fl_Round_Button(435, 400, 96, 25, "Assembler");
    radioAssembler->type(FL_RADIO_BUTTON);
    radioAssembler->value(Config::bShowCode ? 0 : 1);
    radioAssembler->callback(onMemoryMode, this);
    memoryModeGroup->end();

    Fl_Box* labelWrite = new Fl_Box(540, 368, 150, 20,
                                    "Mem Write Breakpoint");
    labelWrite->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    labelWrite->labelsize(12);
    checkMemWrite = new Fl_Check_Button(540, 391, 20, INPUT_H, "");
    checkMemWrite->value(Config::bBP6 ? 1 : 0);
    checkMemWrite->callback(onMemoryBreakpointToggle, this);
    inputMemWrite = new DebuggerAddressInput(560, 391, 65, INPUT_H, 4);
    {
        char bpValue[8];
        sprintf(bpValue, "%04X", (unsigned int)Config::nBP6Address & 0xffff);
        inputMemWrite->value(bpValue);
    }
    inputMemWrite->textfont(FL_COURIER_BOLD);
    inputMemWrite->textsize(12);
    inputMemWrite->callback(onMemoryBreakpointInput, this);

    Fl_Box* labelRead = new Fl_Box(540, 430, 150, 20,
                                   "Mem Read Breakpoint");
    labelRead->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    labelRead->labelsize(12);
    checkMemRead = new Fl_Check_Button(540, 453, 20, INPUT_H, "");
    checkMemRead->value(Config::bBP7 ? 1 : 0);
    checkMemRead->callback(onMemoryBreakpointToggle, this);
    inputMemRead = new DebuggerAddressInput(560, 453, 65, INPUT_H, 4);
    {
        char bpValue[8];
        sprintf(bpValue, "%04X", (unsigned int)Config::nBP7Address & 0xffff);
        inputMemRead->value(bpValue);
    }
    inputMemRead->textfont(FL_COURIER_BOLD);
    inputMemRead->textsize(12);
    inputMemRead->callback(onMemoryBreakpointInput, this);

    inputMemoryAddress = new DebuggerAddressInput(10, 509, 60, INPUT_H, 4);
    {
        char memoryAddress[8];
        sprintf(memoryAddress, "%04X", (unsigned int)Config::nMemAddress & 0xffff);
        inputMemoryAddress->value(memoryAddress);
    }
    inputMemoryAddress->textfont(FL_COURIER_BOLD);
    inputMemoryAddress->textsize(12);
    inputMemoryAddress->callback(onMemoryAddress, this);

    /* Hidden until the user double-clicks a byte in the Hex memory view. */
    {
        InlineHexInput* editor = new InlineHexInput(10, 370, 24, 20, 2);
        editor->textfont(FL_COURIER);
        editor->textsize(15);
        editor->setFinishCallback(onMemoryEditFinished, this);
        editor->hide();
        inlineMemoryEditor = editor;
    }

    // ============================================================
    // TIMELINE AREA
    // ============================================================
    timelineGroup = new Fl_Group(10, 542, 680, 132);
    timelineGroup->begin();

    checkTimeline = new Fl_Check_Button(10, 542, 140, 25,
                                        "Enable timeline");
    checkTimeline->callback(onTimelineEnable, this);

    Fl_Box* labelMain = new Fl_Box(10, 572, 35, 22, "Main");
    labelMain->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    sliderTimelineMain = new TimelineSlider(50, 572, 640, 22);
    sliderTimelineMain->type(FL_HOR_NICE_SLIDER);
    sliderTimelineMain->bounds(0, 1000);
    sliderTimelineMain->value(0);
    sliderTimelineMain->callback(onTimelineMain, this);

    Fl_Box* labelFine = new Fl_Box(10, 602, 35, 22, "Fine");
    labelFine->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    sliderTimelineFine = new TimelineSlider(50, 602, 640, 22);
    sliderTimelineFine->type(FL_HOR_NICE_SLIDER);
    sliderTimelineFine->bounds(-5000, 5000);
    sliderTimelineFine->value(0);
    sliderTimelineFine->callback(onTimelineFine, this);

    Fl_Box* labelStep = new Fl_Box(10, 634, 35, 27, "Step");
    labelStep->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    btnTimelineBack = new Fl_Button(50, 634, 42, 27, "<");
    btnTimelineBack->tooltip("Step back (Ctrl+Left)");
    btnTimelineBack->shortcut(FL_CTRL | FL_Left);
    btnTimelineBack->callback(onTimelineBack, this);
    btnTimelineForward = new Fl_Button(98, 634, 42, 27, ">");
    btnTimelineForward->tooltip("Step forward (Ctrl+Right)");
    btnTimelineForward->shortcut(FL_CTRL | FL_Right);
    btnTimelineForward->callback(onTimelineForward, this);

    btnTimelineExport = new Fl_Button(580, 634, 110, 27, "Export TXT");
    btnTimelineExport->tooltip("Export timeline as assembler trace");
    btnTimelineExport->callback(onTimelineExport, this);

    timelineGroup->end();

    end();

    setTimelineAvailable(true);
    updateTimelineControls(true);

    /* Exactly one DebuggerWindow exists in Jondra. */
    timelineShortcutWindow = this;
    Fl::add_handler(timelineGlobalShortcutHandler);
}

DebuggerWindow::~DebuggerWindow() {
    if (timelineShortcutWindow == this) {
        Fl::remove_handler(timelineGlobalShortcutHandler);
        timelineShortcutWindow = NULL;
    }
    Fl::remove_timeout(onStepOverPoll, this);
    Fl::remove_timeout(onUserBreakpointPoll, this);
    runBreakpointPollActive = false;
    if (stepOverActive && machine != NULL) {
        stepOverActive = false;
        machine->debugCancelStepOver();
    }
    /* Child widgets are owned by FLTK. */
}

int DebuggerWindow::handle(int event) {
    if (isTimelineShortcutEvent(event) &&
        machine != NULL && machine->isTimelineEnabled() &&
        sliderTimelineMain != NULL && visible()) {
        int delta = 0;
        switch (Fl::event_key()) {
            case FL_Left:  delta = -1; break;
            case FL_Right: delta = 1; break;
            case FL_Up:    delta = 1000; break;
            case FL_Down:  delta = -1000; break;
            default: break;
        }
        if (delta != 0) {
            int index = (int)(sliderTimelineMain->value() + 0.5) + delta;
            loadTimelinePosition(index, true);
            return 1;
        }
    }
    return Fl_Window::handle(event);
}

void DebuggerWindow::centerOnParent() {
    if (mainWindow != NULL) {
        int newX = mainWindow->x() + (mainWindow->w() - w()) / 2;
        int newY = mainWindow->y() + (mainWindow->h() - h()) / 2;
        position(newX, newY);
    }
}

void DebuggerWindow::loadIcons() {
    iconStepInto = EmbeddedResources::loadPng("images/step.png");
    iconStepOver = EmbeddedResources::loadPng("images/stepover.png");
    iconRun = EmbeddedResources::loadPng("images/rundbg.png");
    iconStop = EmbeddedResources::loadPng("images/stopdbg.png");
}

Fl_Text_Display* DebuggerWindow::createTextDisplay(int x, int y, int w, int h,
                                                   Fl_Text_Buffer* buffer) {
    Fl_Text_Display* display = new Fl_Text_Display(x, y, w, h);
    display->buffer(buffer);
    display->textfont(FL_COURIER);
    display->textsize(15);
    display->box(FL_DOWN_BOX);
    return display;
}

Ondra* DebuggerWindow::getMachine() const {
    return machine;
}

bool DebuggerWindow::readCpuVisualState(CpuVisualState& state) const {
    if (machine == NULL) {
        return false;
    }

    state.af = CPU_GetReg16(machine->cpu, REG_AF);
    state.bc = CPU_GetReg16(machine->cpu, REG_BC);
    state.de = CPU_GetReg16(machine->cpu, REG_DE);
    state.hl = CPU_GetReg16(machine->cpu, REG_HL);
    state.afAlt = CPU_GetReg16Alt(machine->cpu, REG_AF);
    state.bcAlt = CPU_GetReg16Alt(machine->cpu, REG_BC);
    state.deAlt = CPU_GetReg16Alt(machine->cpu, REG_DE);
    state.hlAlt = CPU_GetReg16Alt(machine->cpu, REG_HL);
    state.ix = CPU_GetReg16(machine->cpu, REG_IX);
    state.iy = CPU_GetReg16(machine->cpu, REG_IY);
    state.pc = CPU_GetReg16(machine->cpu, REG_PC);
    state.sp = CPU_GetReg16(machine->cpu, REG_SP);
    state.r = CPU_GetReg8(machine->cpu, REG_R);
    state.i = CPU_GetReg8(machine->cpu, REG_I);
    state.im = CPU_GetIntMode(machine->cpu);
    if (state.im > 2) {
        state.im = 0;
    }
    state.flags = CPU_GetReg8(machine->cpu, REG_F);
    return true;
}

void DebuggerWindow::captureChangeBaseline() {
    changeBaselineValid = readCpuVisualState(changeBaseline);
}

void DebuggerWindow::setRunButtonRunning(bool running) {
    if (btnRun == NULL) {
        return;
    }

    btnRun->image(running ? iconStop : iconRun);
    btnRun->tooltip(running ? "Stop" : "Run");
    btnRun->redraw();
}

void DebuggerWindow::finishDebuggerRunState() {
    restoreStepOverSuppressedBreakpoints();
    stepOverActive = false;
    setRunButtonRunning(false);
    if (btnStepInto != NULL) btnStepInto->activate();
    if (btnStepOver != NULL) btnStepOver->activate();
    if (mainWindow != NULL) mainWindow->syncPauseButton();
}

void DebuggerWindow::applyBreakpointSlot(int slot) {
    bool enabled;

    if (machine == NULL || slot < 0 || slot >= 5) {
        return;
    }

    enabled = breakpointEnabled[slot];
    if (stepOverActive &&
        (stepOverSuppressedBreakpointMask & (1U << slot)) != 0) {
        enabled = false;
    }

    CPU_SetUserBreakpointSlot(machine->cpu, slot,
                              (uint16_t)(breakpointAddress[slot] & 0xffff),
                              enabled);
}

void DebuggerWindow::applyMemoryBreakpointSlots() {
    if (machine == NULL) {
        return;
    }

    CPU_SetMemoryWriteBreakpoint(machine->cpu,
                                 (uint16_t)((unsigned int)Config::nBP6Address & 0xffff),
                                 Config::bBP6);
    CPU_SetMemoryReadBreakpoint(machine->cpu,
                                (uint16_t)((unsigned int)Config::nBP7Address & 0xffff),
                                Config::bBP7);
}

void DebuggerWindow::applyAllBreakpointSlots() {
    int i;
    for (i = 0; i < 5; ++i) {
        applyBreakpointSlot(i);
    }
    applyMemoryBreakpointSlots();
}

void DebuggerWindow::saveBreakpointConfig() {
    Config::bBP1 = breakpointEnabled[0];
    Config::nBP1Address = (int)(breakpointAddress[0] & 0xffff);
    Config::bBP2 = breakpointEnabled[1];
    Config::nBP2Address = (int)(breakpointAddress[1] & 0xffff);
    Config::bBP3 = breakpointEnabled[2];
    Config::nBP3Address = (int)(breakpointAddress[2] & 0xffff);
    Config::bBP4 = breakpointEnabled[3];
    Config::nBP4Address = (int)(breakpointAddress[3] & 0xffff);
    Config::bBP5 = breakpointEnabled[4];
    Config::nBP5Address = (int)(breakpointAddress[4] & 0xffff);
    Config::SaveConfig();
}

void DebuggerWindow::suppressCurrentStepOverBreakpoint(unsigned int address) {
    int i;

    stepOverSuppressedBreakpointMask = 0;
    if (machine == NULL) {
        return;
    }

    address &= 0xffff;
    for (i = 0; i < 5; ++i) {
        if (breakpointEnabled[i] &&
            ((breakpointAddress[i] & 0xffff) == address)) {
            stepOverSuppressedBreakpointMask |= (1U << i);
            CPU_SetUserBreakpointSlot(machine->cpu, i,
                                      (uint16_t)breakpointAddress[i], false);
        }
    }
}

void DebuggerWindow::restoreStepOverSuppressedBreakpoints() {
    unsigned int mask = stepOverSuppressedBreakpointMask;
    int i;

    stepOverSuppressedBreakpointMask = 0;
    if (mask == 0) {
        return;
    }

    for (i = 0; i < 5; ++i) {
        if ((mask & (1U << i)) != 0) {
            applyBreakpointSlot(i);
        }
    }
}

bool DebuggerWindow::hasEnabledBreakpoints() const {
    int i;
    for (i = 0; i < 5; ++i) {
        if (breakpointEnabled[i]) {
            return true;
        }
    }
    return Config::bBP6 || Config::bBP7;
}

void DebuggerWindow::showDialog(bool resumeAfterClose) {
    /*
     * Disable the emulated Ondra keyboard while the debugger
     * owns the input focus.  Without this, Keyboard::handle(FL_UNFOCUS)
     * immediately steals focus back from HexInput and the breakpoint fields
     * cannot be edited.
     */
    if (machine != NULL && machine->getKeyboard() != NULL) {
        machine->getKeyboard()->setKeyboardEnabled(false);
    }

    if (runBreakpointPollActive) {
        Fl::remove_timeout(onUserBreakpointPoll, this);
        runBreakpointPollActive = false;
    }
    if (machine != NULL) {
        CPU_ClearUserBreakpointHit(machine->cpu);
    }
    applyAllBreakpointSlots();

    /*
     * Remember the state only when the debugger is opened from a hidden state.
     * Re-triggering Ctrl+D while the window is already visible must not lose
     * the original "resume after close" decision.
     */
    if (!visible()) {
        resumeOnClose = resumeAfterClose;
        /* A freshly opened debugger starts with no "changed" values. */
        captureChangeBaseline();
        updateTimelineControls(true);
    }
    if (!stepOverActive) {
        setRunButtonRunning(false);
        if (btnStepInto != NULL) btnStepInto->activate();
        if (btnStepOver != NULL) btnStepOver->activate();
    }
    refreshData();
    centerOnParent();
    show();
    Fl::focus(this);
}

void DebuggerWindow::setTimelineAvailable(bool available) {
    if (timelineGroup == NULL) {
        return;
    }

    if (available) {
        timelineGroup->activate();
    } else {
        timelineGroup->deactivate();
    }
}

void DebuggerWindow::updateTimelineControls(bool selectLatest) {
    bool enabled;
    int count;
    int maxIndex;
    int value;

    if (checkTimeline == NULL || machine == NULL) return;

    enabled = machine->isTimelineEnabled();
    checkTimeline->value(enabled ? 1 : 0);
    count = machine->getTimelineSize();

    timelineUpdating = true;
    if (!enabled || count <= 0) {
        sliderTimelineMain->bounds(0, 0);
        sliderTimelineMain->value(0);
        sliderTimelineFine->value(0);
        timelineFineLastValue = 0;
        sliderTimelineMain->deactivate();
        sliderTimelineFine->deactivate();
        btnTimelineBack->deactivate();
        btnTimelineForward->deactivate();
    } else {
        maxIndex = count - 1;
        sliderTimelineMain->bounds(0, maxIndex);
        value = (int)(sliderTimelineMain->value() + 0.5);
        if (selectLatest || value < 0 || value > maxIndex) value = maxIndex;
        sliderTimelineMain->value(value);
        if (selectLatest) {
            sliderTimelineFine->value(0);
            timelineFineLastValue = 0;
        }
        sliderTimelineMain->activate();
        sliderTimelineFine->activate();
        btnTimelineBack->activate();
        btnTimelineForward->activate();
    }

    if (btnTimelineExport != NULL) {
        if (enabled && count >= 2) btnTimelineExport->activate();
        else btnTimelineExport->deactivate();
    }
    timelineUpdating = false;
}

bool DebuggerWindow::loadTimelinePosition(int index, bool resetFine) {
    int count;
    if (machine == NULL || !machine->isTimelineEnabled()) return false;
    count = machine->getTimelineSize();
    if (count <= 0) return false;
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;

    if (!machine->loadTimelineIndex(index)) return false;

    timelineUpdating = true;
    sliderTimelineMain->value(index);
    if (resetFine) {
        sliderTimelineFine->value(0);
        timelineFineLastValue = 0;
    }
    timelineUpdating = false;

    changeBaselineValid = false;
    captureChangeBaseline();
    refreshData();
    if (machine->til != NULL && machine->px != NULL) {
        machine->til->DispUpdate(machine->px);
    }
    return true;
}

void DebuggerWindow::prepareTimelineForExecution() {
    int index;
    if (machine == NULL || !machine->isTimelineEnabled() ||
        machine->getTimelineSize() <= 0 || sliderTimelineMain == NULL) {
        return;
    }

    index = (int)(sliderTimelineMain->value() + 0.5);
    if (machine->getTimelineLoadedIndex() != index) {
        if (!machine->loadTimelineIndex(index)) {
            return;
        }
    }
    machine->truncateTimeline(index);
    updateTimelineControls(true);
}

void DebuggerWindow::refreshData() {
    Ondra* m = getMachine();
    if (m == NULL || m->mem == NULL) {
        bufferAsm->text("");
        bufferRegisters->text("");
        bufferFlags->text("");
        bufferStackPorts->text("");
        bufferMemory->text("");
        return;
    }

    fillAsmCode();
    fillRegisters();
    fillFlags();
    fillStackPorts();
    fillBreakpoints();
    fillMemoryView();

    /*
     * FLTK 1.1.x does not reliably repaint Fl_Text_Display immediately
     * after replacing the complete Fl_Text_Buffer from a button callback.
     * Mark every read-only text view dirty explicitly.
     */
    if (textAsm != NULL) textAsm->redraw();
    if (textRegisters != NULL) textRegisters->redraw();
    if (textFlags != NULL) textFlags->redraw();
    if (textStackPorts != NULL) textStackPorts->redraw();
    if (textMemory != NULL) textMemory->redraw();
    if (labelTStates != NULL) labelTStates->redraw();
}

void DebuggerWindow::fillAsmCode() {
    Ondra* m = getMachine();
    std::string text;
    unsigned int address;
    int lineNo;
    char line[1200];

    if (m == NULL) {
        bufferAsm->text("");
        return;
    }

    address = CPU_GetReg16(m->cpu, REG_PC) & 0xffff;
    for (lineNo = 0; lineNo < 13; ++lineNo) {
        unsigned int len;
        asmLineAddress[lineNo] = address;
        len = formatInstructionLine(m, address, line);
        text += line;
        address = (address + len) & 0xffff;
    }
    bufferAsm->text(text.c_str());

    /*
     * CurrentAsmTextDisplay paints the current row with a fixed blue
     * background.  Keep the first instruction at the top after every refresh.
     */
    textAsm->scroll(1, 0);
}

void DebuggerWindow::fillRegisters() {
    Ondra* m = getMachine();
    CpuVisualState current;
    std::string text;
    std::string style;
    char left[64];
    char right[64];
    char token[64];
    char tstates[40];

    if (m == NULL || !readCpuVisualState(current)) {
        bufferRegisters->text("");
        bufferRegisterStyle->text("");
        return;
    }

    if (inlineRegisterEditor != NULL && inlineRegisterEditor->visible()) {
        static_cast<InlineHexInput*>(inlineRegisterEditor)->dismissSilently();
    }

    sprintf(left, "AF:#%04X", current.af);
    sprintf(right, "AF':#%04X", current.afAlt);
    appendStyledPair(text, style, left,
                     changeBaselineValid && current.af != changeBaseline.af,
                     right,
                     changeBaselineValid && current.afAlt != changeBaseline.afAlt);

    sprintf(left, "BC:#%04X", current.bc);
    sprintf(right, "BC':#%04X", current.bcAlt);
    appendStyledPair(text, style, left,
                     changeBaselineValid && current.bc != changeBaseline.bc,
                     right,
                     changeBaselineValid && current.bcAlt != changeBaseline.bcAlt);

    sprintf(left, "DE:#%04X", current.de);
    sprintf(right, "DE':#%04X", current.deAlt);
    appendStyledPair(text, style, left,
                     changeBaselineValid && current.de != changeBaseline.de,
                     right,
                     changeBaselineValid && current.deAlt != changeBaseline.deAlt);

    sprintf(left, "HL:#%04X", current.hl);
    sprintf(right, "HL':#%04X", current.hlAlt);
    appendStyledPair(text, style, left,
                     changeBaselineValid && current.hl != changeBaseline.hl,
                     right,
                     changeBaselineValid && current.hlAlt != changeBaseline.hlAlt);

    sprintf(token, "IX:#%04X", current.ix);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.ix != changeBaseline.ix);
    sprintf(token, "IY:#%04X", current.iy);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.iy != changeBaseline.iy);
    sprintf(token, "PC:#%04X", current.pc);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.pc != changeBaseline.pc);
    sprintf(token, "SP:#%04X", current.sp);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.sp != changeBaseline.sp);
    sprintf(token, "R:#%02X", current.r);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.r != changeBaseline.r);
    sprintf(token, "I:#%02X", current.i);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.i != changeBaseline.i);
    sprintf(token, "IM%u", current.im);
    appendStyledSingle(text, style, token,
                       changeBaselineValid && current.im != changeBaseline.im);

    /* There is intentionally no trailing newline after IMx. */
    if (!text.empty() && text[text.size() - 1] == '\n') {
        text.erase(text.size() - 1);
        style.erase(style.size() - 1);
    }

    bufferRegisters->text(text.c_str());
    bufferRegisterStyle->text(style.c_str());

    formatTStates(tstates, m->clk->getTstates());
    labelTStates->copy_label(tstates);
}

void DebuggerWindow::fillFlags() {
    Ondra* m = getMachine();
    static const char* setLabels[6] = {"M", "Z", "AC", "PE", "N1", "C"};
    static const char* clearLabels[6] = {"P", "NZ", "NA", "PO", "N0", "NC"};
    static const unsigned int masks[6] = {0x80, 0x40, 0x10, 0x04, 0x02, 0x01};
    std::string text;
    std::string style;
    unsigned int flags;
    int i;

    if (m == NULL) {
        bufferFlags->text("");
        bufferFlagStyle->text("");
        return;
    }

    flags = CPU_GetReg8(m->cpu, REG_F);
    for (i = 0; i < 6; ++i) {
        const char* label = (flags & masks[i]) ? setLabels[i] : clearLabels[i];
        bool changed = changeBaselineValid &&
                       ((flags & masks[i]) != (changeBaseline.flags & masks[i]));
        appendStyledSingle(text, style, label, changed);
    }
    bufferFlags->text(text.c_str());
    bufferFlagStyle->text(style.c_str());
}

void DebuggerWindow::fillStackPorts() {
    Ondra* m = getMachine();
    std::string text;
    std::string style;
    unsigned int sp;
    int offset;
    char line[128];
    char bits[32];
    unsigned char port;

    if (m == NULL) {
        bufferStackPorts->text("");
        bufferStackPortStyle->text("");
        return;
    }

    sp = CPU_GetReg16(m->cpu, REG_SP) & 0xffff;
    for (offset = 0; offset < 10; offset += 2) {
        unsigned int adrLo = (sp + offset) & 0xffff;
        unsigned int adrHi = (adrLo + 1) & 0xffff;
        unsigned int value = (m->mem->readByte(adrHi) << 8) |
                             m->mem->readByte(adrLo);
        sprintf(line, "#%04X #%04X\n", adrLo, value & 0xffff);
        text += line;
        style.append(strlen(line), 'A');
    }

    /* Keep the visual separator. The top debugger panes are one text row
       taller, so the separator and all
       three port value rows fit on both Win98 and Linux. */
    text += "\n";
    style += "A";

    port = m->getPortA3();
    sprintf(line, "Port03(F7h):#%02X\n", (unsigned int)port);
    text += line;
    style.append(strlen(line), 'A');
    text += "7 6 5 4 3 2 1 0\n";
    style.append(strlen("7 6 5 4 3 2 1 0\n"), 'B');
    formatPortBits(bits, port);
    text += bits;
    style.append(strlen(bits), 'A');
    text += "\n";
    style += "A";

    port = m->getPortA1();
    sprintf(line, "Port09(FDh):#%02X\n", (unsigned int)port);
    text += line;
    style.append(strlen(line), 'A');
    text += "7 6 5 4 3 2 1 0\n";
    style.append(strlen("7 6 5 4 3 2 1 0\n"), 'B');
    formatPortBits(bits, port);
    text += bits;
    style.append(strlen(bits), 'A');
    text += "\n";
    style += "A";

    port = m->getPortA0();
    sprintf(line, "Port10(FEh):#%02X\n", (unsigned int)port);
    text += line;
    style.append(strlen(line), 'A');
    text += "7 6 5 4 3 2 1 0\n";
    style.append(strlen("7 6 5 4 3 2 1 0\n"), 'B');
    formatPortBits(bits, port);
    text += bits;
    style.append(strlen(bits), 'A');

    /* No trailing newline after the final Port10 value row.  Apart from
       avoiding a needless empty logical line, this matches the fixed-height
       FLTK view more closely on Linux. */
    bufferStackPorts->text(text.c_str());
    bufferStackPortStyle->text(style.c_str());
}

void DebuggerWindow::fillBreakpoints() {
    int i;
    char value[8];

    for (i = 0; i < 5; ++i) {
        if (checkBreakpoint[i] != NULL) {
            checkBreakpoint[i]->value(breakpointEnabled[i] ? 1 : 0);
        }
        if (inputBreakpoint[i] != NULL) {
            sprintf(value, "%04X", breakpointAddress[i] & 0xffff);
            inputBreakpoint[i]->value(value);
        }
    }

    if (checkMemWrite != NULL) {
        checkMemWrite->value(Config::bBP6 ? 1 : 0);
    }
    if (inputMemWrite != NULL) {
        sprintf(value, "%04X", (unsigned int)Config::nBP6Address & 0xffff);
        inputMemWrite->value(value);
    }
    if (checkMemRead != NULL) {
        checkMemRead->value(Config::bBP7 ? 1 : 0);
    }
    if (inputMemRead != NULL) {
        sprintf(value, "%04X", (unsigned int)Config::nBP7Address & 0xffff);
        inputMemRead->value(value);
    }
}

unsigned int DebuggerWindow::getMemoryAddress() const {
    const char* value;
    unsigned long address;

    if (inputMemoryAddress == NULL) {
        return 0;
    }
    value = inputMemoryAddress->value();
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    address = strtoul(value, NULL, 16);
    return (unsigned int)(address & 0xffff);
}

void DebuggerWindow::fillMemoryView() {
    Ondra* m = getMachine();
    std::string text;
    unsigned int address;
    int row;
    char line[1200];

    if (m == NULL) {
        bufferMemory->text("");
        return;
    }

    if (inlineMemoryEditor != NULL && inlineMemoryEditor->visible()) {
        static_cast<InlineHexInput*>(inlineMemoryEditor)->dismissSilently();
    }

    address = getMemoryAddress();

    if (radioAssembler != NULL && radioAssembler->value()) {
        /*
         * With the Win98 debugger font, seven rows fit the 132 px memory
         * pane; an eighth row
         * is already outside the intended visible area.  Keep Hex and
         * Assembler modes at the same seven visible rows.
         */
        for (row = 0; row < 7; ++row) {
            unsigned int len = formatInstructionLine(m, address, line);
            text += line;
            address = (address + len) & 0xffff;
        }
    } else {
        for (row = 0; row < 7; ++row) {
            char oneLine[256];
            char byteText[8];
            char ascii[9];
            int col;
            unsigned int rowAddress = address;

            sprintf(oneLine, "#%04X  ", rowAddress);
            ascii[8] = '\0';

            for (col = 0; col < 8; ++col) {
                unsigned int value = m->mem->readByte(address) & 0xff;
                sprintf(byteText, "%02X ", value);
                strcat(oneLine, byteText);
                if (value >= 32 && value <= 128) {
                    ascii[col] = (char)value;
                } else {
                    ascii[col] = '.';
                }
                address = (address + 1) & 0xffff;
            }

            strcat(oneLine, "   ");
            strcat(oneLine, ascii);
            strcat(oneLine, "\n");
            text += oneLine;
        }
    }

    /*
     * Do not leave a newline after the last visible Memory row.
     * Fl_Text_Display treats it as an additional empty logical line.  On the
     * first click of a double-click in the last row it may then scroll that
     * empty line into view before our inline byte editor is opened, so the
     * editor appears one row below the byte being edited.
     */
    if (!text.empty() && text[text.size() - 1] == '\n') {
        text.erase(text.size() - 1);
    }

    bufferMemory->text(text.c_str());
}

void DebuggerWindow::onClose(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog != NULL) {
        bool resume = dialog->resumeOnClose;

        if (dialog->runBreakpointPollActive) {
            Fl::remove_timeout(onUserBreakpointPoll, dialog);
            dialog->runBreakpointPollActive = false;
        }

        if (dialog->stepOverActive && dialog->machine != NULL) {
            Fl::remove_timeout(onStepOverPoll, dialog);
            dialog->machine->debugCancelStepOver();
            dialog->finishDebuggerRunState();
            /* Step Over may have advanced while the timeline slider still
             * showed its starting point.  Closing during the run must keep the
             * state where Stop actually occurred, not rewind accidentally. */
            dialog->updateTimelineControls(true);
        }

        /* If the user moved back in history, closing the debugger discards the
         * future before any resumed step. */
        dialog->prepareTimelineForExecution();

        /* A memory breakpoint rolls the CPU back to the instruction which
         * caused the access.  Arm a one-instruction suppression before the
         * debugger is closed so a later Resume does not stop on the same
         * access immediately again. */
        if (dialog->machine != NULL &&
            CPU_MemoryBreakpointHit(dialog->machine->cpu) != CPU_MEM_BREAK_NONE) {
            CPU_ArmMemoryBreakpointResume(dialog->machine->cpu);
        }

        /*
         * If we are resuming from an enabled breakpoint at the current PC,
         * execute that one instruction with user BPs suspended first.  Otherwise
         * the core would stop on the same address again immediately.
         */
        if (resume && dialog->machine != NULL && dialog->machine->isPaused()) {
            unsigned int pc = CPU_GetReg16(dialog->machine->cpu, REG_PC) & 0xffff;
            int i;
            for (i = 0; i < 5; ++i) {
                if (dialog->breakpointEnabled[i] &&
                    ((dialog->breakpointAddress[i] & 0xffff) == pc)) {
                    dialog->machine->debugStepInto();
                    break;
                }
            }
        }

        dialog->resumeOnClose = false;
        dialog->hide();
        if (dialog->machine != NULL && dialog->machine->getKeyboard() != NULL) {
            dialog->machine->getKeyboard()->setKeyboardEnabled(true);
            Fl::focus((Fl_Widget*)dialog->machine->getKeyboard());
        }
        if (dialog->mainWindow != NULL) {
            dialog->mainWindow->debuggerClosed(resume);
        }

        /* Closing a running debugger must keep watching active breakpoints. */
        if (resume && dialog->machine != NULL && !dialog->machine->isPaused() &&
            dialog->hasEnabledBreakpoints()) {
            Fl::remove_timeout(onUserBreakpointPoll, dialog);
            dialog->runBreakpointPollActive = true;
            Fl::add_timeout(0.01, onUserBreakpointPoll, dialog);
        }
    }
}

void DebuggerWindow::onStepInto(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog == NULL || dialog->machine == NULL) {
        return;
    }

    /*
     * The debugger owns a stopped CPU.  Execute exactly one complete Z80
     * instruction and immediately redraw all read-only debugger fields.
     */
    if (dialog->btnStepInto != NULL) {
        dialog->btnStepInto->deactivate();
    }
    dialog->prepareTimelineForExecution();
    dialog->captureChangeBaseline();
    dialog->machine->debugStepInto();
    dialog->updateTimelineControls(true);
    dialog->refreshData();
    if (dialog->btnStepInto != NULL) {
        dialog->btnStepInto->activate();
    }
}

void DebuggerWindow::onStepOver(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    unsigned int pc;
    unsigned int len;
    unsigned int target;
    char line[1200];

    if (dialog == NULL || dialog->machine == NULL || dialog->stepOverActive) {
        return;
    }

    /*
     * Step Over uses a temporary breakpoint at the instruction directly
     * following the current one, then lets the emulator run normally.
     */
    pc = CPU_GetReg16(dialog->machine->cpu, REG_PC) & 0xffff;
    len = formatInstructionLine(dialog->machine, pc, line);
    target = (pc + len) & 0xffff;

    dialog->prepareTimelineForExecution();
    dialog->captureChangeBaseline();
    dialog->suppressCurrentStepOverBreakpoint(pc);
    if (!dialog->machine->debugStartStepOver((uint16_t)target)) {
        dialog->restoreStepOverSuppressedBreakpoints();
        return;
    }

    dialog->stepOverActive = true;
    if (dialog->btnStepInto != NULL) dialog->btnStepInto->deactivate();
    if (dialog->btnStepOver != NULL) dialog->btnStepOver->deactivate();
    dialog->setRunButtonRunning(true);
    if (dialog->mainWindow != NULL) dialog->mainWindow->syncPauseButton();

    /* Polling executes in the FLTK GUI thread; the CPU keeps running in MTimer. */
    Fl::add_timeout(0.01, onStepOverPoll, dialog);
}

void DebuggerWindow::onStepOverPoll(void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);

    if (dialog == NULL || !dialog->stepOverActive) {
        return;
    }

    if (dialog->machine != NULL &&
        dialog->machine->debugFinishMemoryBreakpoint() != CPU_MEM_BREAK_NONE) {
        /* Cancel the temporary Step Over BP, but keep the memory hit latched
         * so the next Step/Run can execute the stopped instruction once. */
        dialog->machine->debugCancelStepOver();
        dialog->finishDebuggerRunState();
        dialog->updateTimelineControls(true);
        dialog->refreshData();
        return;
    }

    if (dialog->machine != NULL && dialog->machine->debugFinishStepOver()) {
        dialog->finishDebuggerRunState();
        dialog->updateTimelineControls(true);
        dialog->refreshData();
        return;
    }

    /* The main-window Pause button may also stop a running Step Over. */
    if (dialog->machine != NULL && dialog->machine->isPaused()) {
        dialog->machine->debugCancelStepOver();
        dialog->finishDebuggerRunState();
        dialog->updateTimelineControls(true);
        dialog->refreshData();
        return;
    }

    Fl::add_timeout(0.01, onStepOverPoll, dialog);
}

void DebuggerWindow::onUserBreakpointPoll(void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);

    if (dialog == NULL || !dialog->runBreakpointPollActive) {
        return;
    }

    if (dialog->machine != NULL &&
        dialog->machine->debugFinishMemoryBreakpoint() != CPU_MEM_BREAK_NONE) {
        dialog->runBreakpointPollActive = false;
        /* Memory breakpoints reopen the debugger on the instruction which
         * attempted the watched access. */
        dialog->showDialog(true);
        if (dialog->mainWindow != NULL) {
            dialog->mainWindow->syncPauseButton();
        }
        return;
    }

    if (dialog->machine != NULL && dialog->machine->debugFinishUserBreakpoint()) {
        dialog->runBreakpointPollActive = false;
        /* The program was running before this breakpoint. */
        dialog->showDialog(true);
        if (dialog->mainWindow != NULL) {
            dialog->mainWindow->syncPauseButton();
        }
        return;
    }

    /* A manual Pause is not a debugger breakpoint; stop polling silently. */
    if (dialog->machine == NULL || dialog->machine->isPaused()) {
        dialog->runBreakpointPollActive = false;
        return;
    }

    Fl::add_timeout(0.01, onUserBreakpointPoll, dialog);
}

void DebuggerWindow::onRunStop(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog == NULL || dialog->machine == NULL) {
        return;
    }

    /*
     * During Step Over this button acts as Stop.
     * It must be able to escape a CALL which never returns (for example a
     * subroutine that enters an intentional infinite loop).
     */
    if (dialog->stepOverActive) {
        Fl::remove_timeout(onStepOverPoll, dialog);
        dialog->machine->debugCancelStepOver();
        dialog->finishDebuggerRunState();
        dialog->refreshData();
        return;
    }

    /*
     * Normal Run first executes the instruction currently
     * displayed, resume normal emulation, then hide the debugger.  The next
     * debugger invocation will stop the CPU again and show its current state.
     */
    if (dialog->machine->isPaused()) {
        dialog->prepareTimelineForExecution();
        dialog->machine->debugStepInto();

        /* If the instruction we just tried to execute hit a newly configured
         * memory BP, stay in the debugger at that instruction.  A second Run
         * or Step will use the one-instruction resume suppression. */
        if (CPU_MemoryBreakpointHit(dialog->machine->cpu) != CPU_MEM_BREAK_NONE) {
            dialog->refreshData();
            return;
        }

        dialog->machine->startEmulation();
        dialog->resumeOnClose = false;
        if (dialog->hasEnabledBreakpoints()) {
            Fl::remove_timeout(onUserBreakpointPoll, dialog);
            dialog->runBreakpointPollActive = true;
            Fl::add_timeout(0.01, onUserBreakpointPoll, dialog);
        }
        dialog->hide();
        if (dialog->machine->getKeyboard() != NULL) {
            dialog->machine->getKeyboard()->setKeyboardEnabled(true);
            Fl::focus((Fl_Widget*)dialog->machine->getKeyboard());
        }
        if (dialog->mainWindow != NULL) {
            dialog->mainWindow->syncPauseButton();
        }
    }
}

void DebuggerWindow::onBreakpointToggle(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int i;

    if (dialog == NULL) {
        return;
    }

    for (i = 0; i < 5; ++i) {
        if (widget == dialog->checkBreakpoint[i]) {
            const char* value = dialog->inputBreakpoint[i] != NULL
                              ? dialog->inputBreakpoint[i]->value() : "0000";
            dialog->breakpointAddress[i] =
                (unsigned int)(strtoul(value ? value : "0000", NULL, 16) & 0xffff);
            dialog->breakpointEnabled[i] = dialog->checkBreakpoint[i]->value() != 0;
            dialog->applyBreakpointSlot(i);
            dialog->fillBreakpoints();
            dialog->saveBreakpointConfig();
            return;
        }
    }
}

void DebuggerWindow::onBreakpointInput(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int i;

    if (dialog == NULL) {
        return;
    }

    for (i = 0; i < 5; ++i) {
        if (widget == dialog->inputBreakpoint[i]) {
            const char* value = dialog->inputBreakpoint[i]->value();
            dialog->breakpointAddress[i] =
                (unsigned int)(strtoul(value ? value : "0000", NULL, 16) & 0xffff);
            dialog->applyBreakpointSlot(i);
            dialog->fillBreakpoints();
            dialog->saveBreakpointConfig();
            return;
        }
    }
}

void DebuggerWindow::onMemoryBreakpointToggle(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog == NULL) {
        return;
    }

    if (widget == dialog->checkMemWrite) {
        const char* value = dialog->inputMemWrite != NULL
                          ? dialog->inputMemWrite->value() : "0000";
        Config::nBP6Address = (int)(strtoul(value ? value : "0000", NULL, 16) & 0xffff);
        Config::bBP6 = dialog->checkMemWrite->value() != 0;
    } else if (widget == dialog->checkMemRead) {
        const char* value = dialog->inputMemRead != NULL
                          ? dialog->inputMemRead->value() : "0000";
        Config::nBP7Address = (int)(strtoul(value ? value : "0000", NULL, 16) & 0xffff);
        Config::bBP7 = dialog->checkMemRead->value() != 0;
    } else {
        return;
    }

    dialog->applyMemoryBreakpointSlots();
    dialog->fillBreakpoints();
    Config::SaveConfig();
}

void DebuggerWindow::onMemoryBreakpointInput(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog == NULL) {
        return;
    }

    if (widget == dialog->inputMemWrite) {
        const char* value = dialog->inputMemWrite->value();
        Config::nBP6Address = (int)(strtoul(value ? value : "0000", NULL, 16) & 0xffff);
    } else if (widget == dialog->inputMemRead) {
        const char* value = dialog->inputMemRead->value();
        Config::nBP7Address = (int)(strtoul(value ? value : "0000", NULL, 16) & 0xffff);
    } else {
        return;
    }

    dialog->applyMemoryBreakpointSlots();
    dialog->fillBreakpoints();
    Config::SaveConfig();
}

void DebuggerWindow::onAsmDoubleClickLine(int line, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int slot = -1;
    int i;

    if (dialog == NULL || line < 0 || line >= 13) {
        return;
    }

    /* Use the first unchecked slot; if all five are used, replace #1. */
    for (i = 0; i < 5; ++i) {
        if (!dialog->breakpointEnabled[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
    }

    dialog->breakpointAddress[slot] = dialog->asmLineAddress[line] & 0xffff;
    dialog->breakpointEnabled[slot] = true;
    dialog->applyBreakpointSlot(slot);
    dialog->fillBreakpoints();
    dialog->saveBreakpointConfig();
}

void DebuggerWindow::onNoOperation(Fl_Widget* widget, void* data) {
    (void)widget;
    (void)data;
}

void DebuggerWindow::onTimelineEnable(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    bool enabled;
    (void)widget;
    if (dialog == NULL || dialog->machine == NULL || dialog->checkTimeline == NULL) return;

    enabled = dialog->checkTimeline->value() != 0;
    Config::bEnableTimeline = enabled;
    dialog->machine->setTimelineEnabled(enabled);
    Config::SaveConfig();
    dialog->updateTimelineControls(true);
}

void DebuggerWindow::onTimelineMain(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int index;
    (void)widget;
    if (dialog == NULL || dialog->timelineUpdating || dialog->sliderTimelineMain == NULL) return;
    index = (int)(dialog->sliderTimelineMain->value() + 0.5);
    dialog->loadTimelinePosition(index, true);
}

void DebuggerWindow::onTimelineFine(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int fineValue;
    int delta;
    int index;
    (void)widget;
    if (dialog == NULL || dialog->timelineUpdating ||
        dialog->sliderTimelineFine == NULL || dialog->sliderTimelineMain == NULL) return;

    fineValue = (int)(dialog->sliderTimelineFine->value() +
                      (dialog->sliderTimelineFine->value() >= 0 ? 0.5 : -0.5));
    delta = fineValue - dialog->timelineFineLastValue;
    if (delta == 0) return;
    index = (int)(dialog->sliderTimelineMain->value() + 0.5) + delta;
    if (dialog->loadTimelinePosition(index, false)) {
        dialog->timelineFineLastValue = fineValue;
    }
}

void DebuggerWindow::onTimelineBack(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int index;
    (void)widget;
    if (dialog == NULL || dialog->sliderTimelineMain == NULL) return;
    index = (int)(dialog->sliderTimelineMain->value() + 0.5) - 1;
    dialog->loadTimelinePosition(index, true);
}

void DebuggerWindow::onTimelineForward(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    int index;
    (void)widget;
    if (dialog == NULL || dialog->sliderTimelineMain == NULL) return;
    index = (int)(dialog->sliderTimelineMain->value() + 0.5) + 1;
    dialog->loadTimelinePosition(index, true);
}


void DebuggerWindow::onTimelineExport(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    const char* selected;
    std::string exportPath;
    std::string error;
    std::string defaultPath;
    int exported;
    int answer;
    (void)widget;

    if (dialog == NULL || dialog->machine == NULL) return;
    if (dialog->machine->getTimelineSize() < 2) {
        fl_message("Timeline does not contain any completed instruction.");
        return;
    }

    defaultPath = timelineDefaultExportPath();
    selected = fl_file_chooser("Export timeline", "*.txt", defaultPath.c_str());
    if (selected == NULL || selected[0] == '\0') return;

    exportPath = selected;
    if (!timelineHasTxtExtension(exportPath)) exportPath += ".txt";

    if (timelineFileExists(exportPath)) {
        answer = fl_choice("File already exists:\n%s\n\nOverwrite it?",
                           "Cancel", "Overwrite", 0, exportPath.c_str());
        if (answer != 1) return;
    }

    if (dialog->btnTimelineExport != NULL) {
        dialog->btnTimelineExport->deactivate();
        dialog->btnTimelineExport->label("Exporting...");
        dialog->btnTimelineExport->redraw();
        Fl::check();
    }

    exported = dialog->machine->exportTimelineToText(exportPath.c_str(), &error);

    if (dialog->btnTimelineExport != NULL) {
        dialog->btnTimelineExport->label("Export TXT");
    }
    dialog->updateTimelineControls(false);

    if (exported >= 0) {
        fl_message("Exported %d instructions to:\n%s", exported, exportPath.c_str());
    } else {
        if (error.empty()) error = "Unknown export error.";
        fl_alert("Timeline export failed:\n%s", error.c_str());
    }
}

void DebuggerWindow::onMemoryMode(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog != NULL) {
        /* bShowCode=true means hexadecimal memory view. */
        Config::bShowCode = (dialog->radioHex != NULL && dialog->radioHex->value() != 0);
        Config::nMemAddress = (int)dialog->getMemoryAddress();
        Config::SaveConfig();
        dialog->fillMemoryView();
        if (dialog->textMemory != NULL) dialog->textMemory->redraw();
    }
}

void DebuggerWindow::onMemoryAddress(Fl_Widget* widget, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    if (dialog == NULL) {
        return;
    }

    Config::nMemAddress = (int)dialog->getMemoryAddress();
    Config::bShowCode = (dialog->radioHex != NULL && dialog->radioHex->value() != 0);
    Config::SaveConfig();
    dialog->fillMemoryView();
    if (dialog->textMemory != NULL) dialog->textMemory->redraw();
}

void DebuggerWindow::onMemoryWheel(int delta, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    unsigned int address;
    unsigned int step;
    char value[8];

    if (dialog == NULL || dialog->machine == NULL || dialog->inputMemoryAddress == NULL) {
        return;
    }

    address = dialog->getMemoryAddress();

    if (dialog->radioAssembler != NULL && dialog->radioAssembler->value()) {
        if (delta < 0) {
            step = debuggerPreviousInstructionStep(dialog->machine, address);
            address = (address + 0x10000UL - step) & 0xffff;
        } else {
            step = debuggerOpcodeLength(dialog->machine, address);
            address = (address + step) & 0xffff;
        }
    } else {
        /* Hexadecimal view scrolls by one complete row (8 bytes). */
        step = 8;
        if (delta < 0) {
            address = (address + 0x10000UL - step) & 0xffff;
        } else {
            address = (address + step) & 0xffff;
        }
    }

    sprintf(value, "%04X", address);
    dialog->inputMemoryAddress->value(value);

    Config::nMemAddress = (int)address;
    Config::bShowCode = (dialog->radioHex != NULL && dialog->radioHex->value() != 0);
    Config::SaveConfig();

    dialog->fillMemoryView();
    if (dialog->textMemory != NULL) {
        dialog->textMemory->scroll(1, 0);
        dialog->textMemory->redraw();
    }
}

void DebuggerWindow::onFlagDoubleClick(int flagIndex, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    static const unsigned char masks[6] = {
        0x80, 0x40, 0x10, 0x04, 0x02, 0x01
    };
    unsigned char flags;

    if (dialog == NULL || dialog->machine == NULL ||
        flagIndex < 0 || flagIndex >= 6) {
        return;
    }

    dialog->prepareTimelineForExecution();
    flags = CPU_GetReg8(dialog->machine->cpu, REG_F);
    flags = (unsigned char)(flags ^ masks[flagIndex]);
    CPU_PutReg8(dialog->machine->cpu, REG_F, flags);
    dialog->machine->timelineInstructionBoundary();
    dialog->updateTimelineControls(true);

    /*
     * Keep changeBaseline unchanged. fillFlags() will therefore mark the
     * manually changed flag blue/bold together with other flags changed since
     * the debugger stopped. Refresh AF as well because F is its low byte.
     */
    dialog->fillRegisters();
    dialog->fillFlags();
    if (dialog->textRegisters != NULL) dialog->textRegisters->redraw();
    if (dialog->textFlags != NULL) dialog->textFlags->redraw();
}

void DebuggerWindow::onTStatesDoubleClick(void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);

    if (dialog == NULL || dialog->machine == NULL || dialog->machine->clk == NULL) {
        return;
    }

    dialog->prepareTimelineForExecution();
    dialog->machine->clk->setTstates(0);
    dialog->machine->timelineInstructionBoundary();
    dialog->updateTimelineControls(true);
    dialog->fillRegisters();
    if (dialog->labelTStates != NULL) dialog->labelTStates->redraw();
}

void DebuggerWindow::onRegisterDoubleClick(int target,
                                           int x, int y, int w, int h,
                                           void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    InlineHexInput* editor;
    unsigned int value = 0;
    char valueText[8];

    if (dialog == NULL || dialog->machine == NULL ||
        dialog->inlineRegisterEditor == NULL) {
        return;
    }

    switch (target) {
        case REGISTER_EDIT_AF:
            value = CPU_GetReg16(dialog->machine->cpu, REG_AF);
            break;
        case REGISTER_EDIT_AF_ALT:
            value = CPU_GetReg16Alt(dialog->machine->cpu, REG_AF);
            break;
        case REGISTER_EDIT_BC:
            value = CPU_GetReg16(dialog->machine->cpu, REG_BC);
            break;
        case REGISTER_EDIT_BC_ALT:
            value = CPU_GetReg16Alt(dialog->machine->cpu, REG_BC);
            break;
        case REGISTER_EDIT_DE:
            value = CPU_GetReg16(dialog->machine->cpu, REG_DE);
            break;
        case REGISTER_EDIT_DE_ALT:
            value = CPU_GetReg16Alt(dialog->machine->cpu, REG_DE);
            break;
        case REGISTER_EDIT_HL:
            value = CPU_GetReg16(dialog->machine->cpu, REG_HL);
            break;
        case REGISTER_EDIT_HL_ALT:
            value = CPU_GetReg16Alt(dialog->machine->cpu, REG_HL);
            break;
        case REGISTER_EDIT_IX:
            value = CPU_GetReg16(dialog->machine->cpu, REG_IX);
            break;
        case REGISTER_EDIT_IY:
            value = CPU_GetReg16(dialog->machine->cpu, REG_IY);
            break;
        case REGISTER_EDIT_SP:
            value = CPU_GetReg16(dialog->machine->cpu, REG_SP);
            break;
        default:
            return;
    }

    sprintf(valueText, "%04X", value & 0xffff);
    dialog->inlineRegisterEditTarget = target;
    editor = static_cast<InlineHexInput*>(dialog->inlineRegisterEditor);
    editor->dismissSilently();
    editor->resize(x, y, w, h);
    editor->value(valueText);
    editor->position(0);
    editor->prepare();
    editor->show();
    editor->take_focus();
    editor->redraw();
}

void DebuggerWindow::onRegisterEditFinished(bool save, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    InlineHexInput* editor;
    unsigned int value;
    unsigned char hi;
    unsigned char lo;
    int target;

    if (dialog == NULL || dialog->inlineRegisterEditor == NULL) {
        return;
    }

    editor = static_cast<InlineHexInput*>(dialog->inlineRegisterEditor);
    target = dialog->inlineRegisterEditTarget;

    if (save && dialog->machine != NULL) {
        dialog->prepareTimelineForExecution();
        const char* text = editor->value();
        value = (unsigned int)strtoul(text ? text : "0000", NULL, 16) & 0xffff;
        hi = (unsigned char)((value >> 8) & 0xff);
        lo = (unsigned char)(value & 0xff);

        switch (target) {
            case REGISTER_EDIT_AF:
                CPU_PutReg8(dialog->machine->cpu, REG_A, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_F, lo);
                break;
            case REGISTER_EDIT_AF_ALT:
                CPU_PutReg8Alt(dialog->machine->cpu, REG_A, hi);
                CPU_PutReg8Alt(dialog->machine->cpu, REG_F, lo);
                break;
            case REGISTER_EDIT_BC:
                CPU_PutReg8(dialog->machine->cpu, REG_B, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_C, lo);
                break;
            case REGISTER_EDIT_BC_ALT:
                CPU_PutReg8Alt(dialog->machine->cpu, REG_B, hi);
                CPU_PutReg8Alt(dialog->machine->cpu, REG_C, lo);
                break;
            case REGISTER_EDIT_DE:
                CPU_PutReg8(dialog->machine->cpu, REG_D, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_E, lo);
                break;
            case REGISTER_EDIT_DE_ALT:
                CPU_PutReg8Alt(dialog->machine->cpu, REG_D, hi);
                CPU_PutReg8Alt(dialog->machine->cpu, REG_E, lo);
                break;
            case REGISTER_EDIT_HL:
                CPU_PutReg8(dialog->machine->cpu, REG_H, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_L, lo);
                break;
            case REGISTER_EDIT_HL_ALT:
                CPU_PutReg8Alt(dialog->machine->cpu, REG_H, hi);
                CPU_PutReg8Alt(dialog->machine->cpu, REG_L, lo);
                break;
            case REGISTER_EDIT_IX:
                CPU_PutReg8(dialog->machine->cpu, REG_IXH, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_IXL, lo);
                break;
            case REGISTER_EDIT_IY:
                CPU_PutReg8(dialog->machine->cpu, REG_IYH, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_IYL, lo);
                break;
            case REGISTER_EDIT_SP:
                CPU_PutReg8(dialog->machine->cpu, REG_SPH, hi);
                CPU_PutReg8(dialog->machine->cpu, REG_SPL, lo);
                break;
            default:
                break;
        }
        dialog->machine->timelineInstructionBoundary();
        dialog->updateTimelineControls(true);
    }

    editor->dismissSilently();
    dialog->inlineRegisterEditTarget = -1;

    if (save) {
        dialog->fillRegisters();
        dialog->fillFlags();
        dialog->fillStackPorts();
        if (dialog->textRegisters != NULL) dialog->textRegisters->redraw();
        if (dialog->textFlags != NULL) dialog->textFlags->redraw();
        if (dialog->textStackPorts != NULL) dialog->textStackPorts->redraw();
    }
}

void DebuggerWindow::onMemoryByteDoubleClick(int row, int byteIndex,
                                             int x, int y, int w, int h,
                                             void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    InlineHexInput* editor;
    unsigned int address;
    unsigned int value;
    char byteText[8];

    if (dialog == NULL || dialog->machine == NULL || dialog->machine->mem == NULL ||
        dialog->inlineMemoryEditor == NULL || row < 0 || row >= 7 ||
        byteIndex < 0 || byteIndex >= 8) {
        return;
    }

    /* Editing is intentionally available only in the hexadecimal view. */
    if (dialog->radioHex == NULL || !dialog->radioHex->value()) {
        return;
    }

    address = (dialog->getMemoryAddress() +
               (unsigned int)(row * 8 + byteIndex)) & 0xffff;
    value = dialog->machine->mem->readByte(address) & 0xff;
    sprintf(byteText, "%02X", value);

    dialog->inlineMemoryEditAddress = address;
    editor = static_cast<InlineHexInput*>(dialog->inlineMemoryEditor);
    editor->dismissSilently();
    editor->resize(x, y, w, h);
    editor->value(byteText);
    editor->position(0);
    editor->prepare();
    editor->show();
    editor->take_focus();
    editor->redraw();
}

void DebuggerWindow::onMemoryEditFinished(bool save, void* data) {
    DebuggerWindow* dialog = static_cast<DebuggerWindow*>(data);
    InlineHexInput* editor;

    if (dialog == NULL || dialog->inlineMemoryEditor == NULL) {
        return;
    }

    editor = static_cast<InlineHexInput*>(dialog->inlineMemoryEditor);

    if (save && dialog->machine != NULL && dialog->machine->mem != NULL) {
        const char* text = editor->value();
        unsigned int address = dialog->inlineMemoryEditAddress & 0xffff;
        unsigned int value = (unsigned int)strtoul(text ? text : "00", NULL, 16);
        unsigned char oldValue;
        dialog->prepareTimelineForExecution();
        oldValue = dialog->machine->mem->readRam((int)address);
        if (dialog->machine->mem->writeByte((int)address,
                                            (unsigned char)(value & 0xff))) {
            dialog->machine->timelineRecordRamWrite((uint16_t)address, oldValue,
                                                    (uint8_t)(value & 0xff));
            dialog->machine->timelineInstructionBoundary();
            dialog->updateTimelineControls(true);
        }
    }

    editor->dismissSilently();

    if (save) {
        /* Refresh all dependent debugger panes. */
        dialog->fillAsmCode();
        dialog->fillStackPorts();
        dialog->fillMemoryView();
        if (dialog->textAsm != NULL) dialog->textAsm->redraw();
        if (dialog->textStackPorts != NULL) dialog->textStackPorts->redraw();
        if (dialog->textMemory != NULL) dialog->textMemory->redraw();
    }
}

