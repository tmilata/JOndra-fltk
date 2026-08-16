#ifndef SCREEN_H
#define SCREEN_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_RGB_Image.H>

#if defined(_MSC_VER) && (_MSC_VER < 1300)
    #include "vs_stdint.h"
#else
    #include <stdint.h>
#endif

#define MAX_RECTS 320

struct Rect {
    unsigned int tileID;
    int x1, y1, x2, y2;
};

class Jondra;
class JScreen : public Fl_Widget {
private:
    int width, height;              // logical Ondra image size (320x256)
    float scale;                    // windowed default scale (2x)
    int renderWidth, renderHeight;  // actually generated image size
    int imageOffsetX, imageOffsetY; // black-border offset inside widget
    bool fullscreenMode;
    bool scanlines;
    bool geometryDirty;

    uint8_t* imageBuffer;
    uint8_t* backBuffer;
    int* xStart;
    int* xEnd;
    int* yStart;
    int* yEnd;

    Rect redrawRects[MAX_RECTS];

    void allocateBuffers(int newRenderWidth, int newRenderHeight);
    void rebuildScaleMaps();

public:
    JScreen(Jondra* mainWin, int X, int Y, int W, int H, float scale);
    ~JScreen();

    void draw();
    bool bDrawingNow;
    volatile bool needsRedraw;
    int rectCount;
    Jondra* mainWin;

    int GetWidth() { return width; }
    int GetHeight() { return height; }
    int GetRenderWidth() { return renderWidth; }
    int GetRenderHeight() { return renderHeight; }

    void addRedrawRect(unsigned int id, int x1, int y1, int x2, int y2);
    void triggerRedraw();
    bool isNeedRedraw();
    void updateTile(const uint8_t* framebuffer, unsigned int id,
                    int x1, int y1, int x2, int y2);

    /* Changes only presentation geometry. The emulator framebuffer remains
       320x256. In fullscreen the image is aspect-correct and centered. */
    void setDisplayArea(int X, int Y, int W, int H, bool fullscreen);
    void setScanlines(bool enabled);
};

#endif // SCREEN_H
