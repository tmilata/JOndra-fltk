#include "Screen.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <string.h>
#include "Debug.h"
#include "MTimer.h"
#include "Jondra.h"
#include "Ondra.h"
#include <stdio.h>
#include "Config.h"

JScreen::JScreen(Jondra* mainWin, int X, int Y, int W, int H, float inScale)
    : Fl_Widget(X, Y, (int)(W * inScale), (int)(H * inScale)),
      width(W), height(H), scale(inScale) {
    imageBuffer = NULL;
    backBuffer = NULL;
    xStart = new int[width];
    xEnd = new int[width];
    yStart = new int[height];
    yEnd = new int[height];

    renderWidth = (int)(width * scale);
    renderHeight = (int)(height * scale);
    imageOffsetX = 0;
    imageOffsetY = 0;
    fullscreenMode = false;
    scanlines = false;
    geometryDirty = true;
    this->mainWin = mainWin;
    rectCount = 0;
    needsRedraw = false;
    bDrawingNow = false;

    allocateBuffers(renderWidth, renderHeight);
    rebuildScaleMaps();
}

JScreen::~JScreen() {
    delete[] imageBuffer;
    delete[] backBuffer;
    delete[] xStart;
    delete[] xEnd;
    delete[] yStart;
    delete[] yEnd;
}

void JScreen::allocateBuffers(int newRenderWidth, int newRenderHeight) {
    if (newRenderWidth < 1) newRenderWidth = 1;
    if (newRenderHeight < 1) newRenderHeight = 1;

    if (imageBuffer && backBuffer &&
        renderWidth == newRenderWidth && renderHeight == newRenderHeight) {
        memset(imageBuffer, 0, renderWidth * renderHeight * 3);
        memset(backBuffer, 0, renderWidth * renderHeight * 3);
        return;
    }

    delete[] imageBuffer;
    delete[] backBuffer;

    renderWidth = newRenderWidth;
    renderHeight = newRenderHeight;
    imageBuffer = new uint8_t[renderWidth * renderHeight * 3];
    backBuffer = new uint8_t[renderWidth * renderHeight * 3];
    memset(imageBuffer, 0, renderWidth * renderHeight * 3);
    memset(backBuffer, 0, renderWidth * renderHeight * 3);
}

void JScreen::rebuildScaleMaps() {
    int i;

    for (i = 0; i < width; ++i) {
        xStart[i] = (i * renderWidth) / width;
        xEnd[i] = (((i + 1) * renderWidth) / width) - 1;
        if (xEnd[i] < xStart[i]) xEnd[i] = xStart[i];
        if (xEnd[i] >= renderWidth) xEnd[i] = renderWidth - 1;
    }

    for (i = 0; i < height; ++i) {
        yStart[i] = (i * renderHeight) / height;
        yEnd[i] = (((i + 1) * renderHeight) / height) - 1;
        if (yEnd[i] < yStart[i]) yEnd[i] = yStart[i];
        if (yEnd[i] >= renderHeight) yEnd[i] = renderHeight - 1;
    }
}

void JScreen::setDisplayArea(int X, int Y, int W, int H, bool fullscreen) {
    int newRenderWidth;
    int newRenderHeight;

    if (W < 1) W = 1;
    if (H < 1) H = 1;

    fullscreenMode = fullscreen;
    resize(X, Y, W, H);

    if (fullscreen) {
        /* Preserve the Ondra 5:4 pixel aspect ratio and use the largest image
           that fits. Scaling is done into our own RGB buffer; GDI/X11 only
           receives a 1:1 image, so there is no StretchBlt-style scaling in
           the platform drawing backend. */
        if ((W * height) <= (H * width)) {
            newRenderWidth = W;
            newRenderHeight = (W * height) / width;
        } else {
            newRenderHeight = H;
            newRenderWidth = (H * width) / height;
        }
    } else {
        newRenderWidth = (int)(width * scale);
        newRenderHeight = (int)(height * scale);
    }

    if (newRenderWidth < 1) newRenderWidth = 1;
    if (newRenderHeight < 1) newRenderHeight = 1;

    allocateBuffers(newRenderWidth, newRenderHeight);
    rebuildScaleMaps();

    imageOffsetX = (W - renderWidth) / 2;
    imageOffsetY = (H - renderHeight) / 2;
    rectCount = 0;
    geometryDirty = true;
    needsRedraw = true;
    redraw();
}

void JScreen::setScanlines(bool enabled) {
    scanlines = enabled;
}

void JScreen::addRedrawRect(unsigned int tileID, int x1, int y1, int x2, int y2) {
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= renderWidth) x2 = renderWidth - 1;
    if (y2 >= renderHeight) y2 = renderHeight - 1;
    if (x2 < x1 || y2 < y1) return;

    if (x1 == 0 && y1 == 0 &&
        x2 == renderWidth - 1 && y2 == renderHeight - 1) {
        rectCount = 0;
    }

    if (rectCount < MAX_RECTS) {
        redrawRects[rectCount].tileID = tileID;
        redrawRects[rectCount].x1 = x1;
        redrawRects[rectCount].y1 = y1;
        redrawRects[rectCount].x2 = x2;
        redrawRects[rectCount].y2 = y2;
        rectCount++;
    }
}

void JScreen::triggerRedraw() {
    needsRedraw = true;
}

bool JScreen::isNeedRedraw() {
    return needsRedraw;
}

void JScreen::updateTile(const uint8_t* framebuffer, unsigned int id,
                         int x1, int y1, int x2, int y2) {
    int sy, sx, dy;

    if (!backBuffer || !framebuffer) return;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > width - 1) x2 = width - 1;
    if (y2 > height - 1) y2 = height - 1;
    if (x2 < x1 || y2 < y1) return;

    /* Scale only dirty source pixels. Source-to-destination boundaries are
       precomputed when the window mode changes, avoiding divisions in the
       hot drawing path (important on a 486). */
    for (sy = y1; sy <= y2; ++sy) {
        int sourceRow = sy * 40;
        int dy1 = yStart[sy];
        int dy2 = yEnd[sy];

        for (dy = dy1; dy <= dy2; ++dy) {
            uint8_t* dstLine = backBuffer + (dy * renderWidth * 3);
            uint8_t whiteValue = 255;

            /* In fullscreen, overlay two darker rows out of every four. With
               a monochrome source, alpha-black 50/255 changes white 255 to
               approximately 205 while black remains black. Calculate this
               once per destination row, not once per pixel (486 friendly). */
            if (fullscreenMode && scanlines && ((dy & 3) < 2)) {
                whiteValue = 205;
            }

            for (sx = x1; sx <= x2; ++sx) {
                uint8_t pixelByte = framebuffer[sourceRow + (sx >> 3)];
                uint8_t value = (pixelByte & (1 << (7 - (sx & 7))))
                              ? whiteValue : 0;
                int dx1 = xStart[sx];
                int dx2 = xEnd[sx];

                if (dx2 >= dx1) {
                    memset(dstLine + dx1 * 3, value, (dx2 - dx1 + 1) * 3);
                }
            }
        }
    }

    addRedrawRect(id, xStart[x1], yStart[y1], xEnd[x2], yEnd[y2]);
}

void JScreen::draw() {
    int i;

    if (bDrawingNow) return;
    bDrawingNow = true;

    if (!imageBuffer) {
        bDrawingNow = false;
        return;
    }

    if (geometryDirty) {
        fl_color(FL_BLACK);
        fl_rectf(x(), y(), w(), h());
        geometryDirty = false;
    }

    if (!needsRedraw || rectCount == 0) {
        bDrawingNow = false;
        return;
    }

    {
        uint8_t* temp = imageBuffer;
        imageBuffer = backBuffer;
        backBuffer = temp;
    }

    for (i = 0; i < rectCount; ++i) {
        int x1 = redrawRects[i].x1;
        int y1 = redrawRects[i].y1;
        int x2 = redrawRects[i].x2;
        int y2 = redrawRects[i].y2;
        int drawX = x() + imageOffsetX + x1;
        int drawY = y() + imageOffsetY + y1;
        int drawWidth = x2 - x1 + 1;
        int drawHeight = y2 - y1 + 1;
        int imageStride = renderWidth * 3;

        fl_push_clip(drawX, drawY, drawWidth, drawHeight);
        fl_draw_image(
            imageBuffer + (y1 * imageStride) + (x1 * 3),
            drawX, drawY, drawWidth, drawHeight, 3, imageStride);
        fl_pop_clip();
    }

    rectCount = 0;
    bDrawingNow = false;
}
