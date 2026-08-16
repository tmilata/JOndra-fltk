#ifndef TILES_H
#define TILES_H
#include "Screen.h"
#if defined(_MSC_VER) && (_MSC_VER < 1300)
    #include "vs_stdint.h"
#else
    #include <stdint.h>
#endif


#define SCR_WIDTH 320
#define SCR_HEIGHT 256
#define TILE_SIZE 16
#define DIRTYTILESX (SCR_WIDTH/TILE_SIZE)
#define DIRTYTILESY (SCR_HEIGHT/TILE_SIZE)

typedef struct dirty_tile {
	unsigned int id;
	int X1;
	int X2;
	int Y1;
	int Y2;
	int minX;
	int maxX;
	int minY;
	int maxY;
} dirty_tile;


class DirtyTiles{
public:
    DirtyTiles(JScreen* inscr){scr=inscr;};
    ~DirtyTiles();

	JScreen* scr;
    
	void InitDirtyTiles();
	void DirtyTilesNone();
	void DirtyTilesAll();
	void DispDirtyPointTiles(int x, int y);
	void DispDirtyRectTiles(int x, int y, int w, int h);
	void DispUpdate(uint8_t* framebuffer);
	dirty_tile dirtyTiles[DIRTYTILESX][DIRTYTILESY];
	int nDispDirtyX1;
	int nDispDirtyX2;
	int nDispDirtyY1;
	int nDispDirtyY2;
};

#endif //TILES_H
