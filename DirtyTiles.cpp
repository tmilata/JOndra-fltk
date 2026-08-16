#include "DirtyTiles.h"
#include "Debug.h"

void DirtyTiles::InitDirtyTiles(){
	int xStep = SCR_WIDTH / DIRTYTILESX;
	int yStep = SCR_HEIGHT / DIRTYTILESY;
	unsigned int tileId=1;
	for (int i = 0; i < DIRTYTILESX; i++) {
		for (int j = 0; j < DIRTYTILESY; j++) {
			dirtyTiles[i][j].id=tileId++;			
			dirtyTiles[i][j].minX = i * xStep;
			dirtyTiles[i][j].maxX = (i + 1) * xStep - 1;
			dirtyTiles[i][j].minY = j * yStep;
			dirtyTiles[i][j].maxY = (j + 1) * yStep - 1;
		}
	}
	DirtyTilesNone(); 
}

void DirtyTiles::DirtyTilesNone(){
	for (int i = 0; i < DIRTYTILESX; i++) {
		for (int j = 0; j < DIRTYTILESY; j++) {
			dirtyTiles[i][j].X1 = dirtyTiles[i][j].maxX;
			dirtyTiles[i][j].X2 = dirtyTiles[i][j].minX;
			dirtyTiles[i][j].Y1 = dirtyTiles[i][j].maxY;
			dirtyTiles[i][j].Y2 = dirtyTiles[i][j].minY;
		}
	}
	nDispDirtyX1 = SCR_WIDTH;
	nDispDirtyX2 = 0;
	nDispDirtyY1 = SCR_HEIGHT;
	nDispDirtyY2 = 0; 
}

void DirtyTiles::DirtyTilesAll(){
	for (int i = 0; i < DIRTYTILESX; i++) {
		for (int j = 0; j < DIRTYTILESY; j++) {
			dirtyTiles[i][j].X1 = dirtyTiles[i][j].minX;
			dirtyTiles[i][j].X2 = dirtyTiles[i][j].maxX;
			dirtyTiles[i][j].Y1 = dirtyTiles[i][j].minY;
			dirtyTiles[i][j].Y2 = dirtyTiles[i][j].maxY;
		}
	}
	nDispDirtyX1 = 0;
	nDispDirtyX2 = SCR_WIDTH; //DispWidth;
	nDispDirtyY1 = 0;
	nDispDirtyY2 = SCR_HEIGHT;
}

void DirtyTiles::DispDirtyPointTiles(int x, int y){
	if (((uint32_t) x < (uint32_t) SCR_WIDTH) && ((uint32_t) y < (uint32_t) SCR_HEIGHT)) {
		int xStep = SCR_WIDTH / DIRTYTILESX;
		int yStep = SCR_HEIGHT / DIRTYTILESY;
		
		int tileX1 = (x + 1) / xStep + ((x + 1) % xStep > 0 ? 1 : 0) - 1;
		int tileY1 = (y + 1) / yStep + ((y + 1) % yStep > 0 ? 1 : 0) - 1;
	
		
		if (x < dirtyTiles[tileX1][tileY1].X1)
			dirtyTiles[tileX1][tileY1].X1 = x;
		if (x + 1 > dirtyTiles[tileX1][tileY1].X2)
			dirtyTiles[tileX1][tileY1].X2 = x + 1;
		if (y < dirtyTiles[tileX1][tileY1].Y1)
			dirtyTiles[tileX1][tileY1].Y1 = y;
		if (y + 1 > dirtyTiles[tileX1][tileY1].Y2)
			dirtyTiles[tileX1][tileY1].Y2 = y + 1;
		
		if (x < nDispDirtyX1)
			nDispDirtyX1 = x;
		if (x + 1 > nDispDirtyX2)
			nDispDirtyX2 = x + 1;
		if (y < nDispDirtyY1)
			nDispDirtyY1 = y;
		if (y + 1 > nDispDirtyY2)
			nDispDirtyY2 = y + 1;
		
	}
}

void DirtyTiles::DispDirtyRectTiles(int x, int y, int w, int h){
	int xStep = SCR_WIDTH / DIRTYTILESX;
	int yStep = SCR_HEIGHT / DIRTYTILESY;
	
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (x + w > SCR_WIDTH)
		w = SCR_WIDTH - x;
	if (w <= 0)
		return;
	
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (y + h > SCR_HEIGHT)
		h = SCR_HEIGHT - y;
	if (h <= 0)
		return;
	
	int tileX1 = (x + 1) / xStep + ((x + 1) % xStep > 0 ? 1 : 0) - 1;
	int tileY1 = (y + 1) / yStep + ((y + 1) % yStep > 0 ? 1 : 0) - 1;
	
	int tileX2 = (x + w + 1) / xStep + ((x + w + 1) % xStep > 0 ? 1 : 0) - 1;
	int tileY2 = (y + h + 1) / yStep + ((y + h + 1) % yStep > 0 ? 1 : 0) - 1;
	
	if (tileX2 >= DIRTYTILESX) {
		tileX2 = DIRTYTILESX - 1;
	}
	if (tileY2 >= DIRTYTILESY) {
		tileY2 = DIRTYTILESY - 1;
	}
	for (int i = tileX1; i <= tileX2; i++) {
		for (int j = tileY1; j <= tileY2; j++) {
			
			if (i == tileX1) {
				//jsem v pocatecnim tile X
				if (x < dirtyTiles[i][j].X1)
					dirtyTiles[i][j].X1 = x;
				if ((x + w) > dirtyTiles[i][j].maxX)
					dirtyTiles[i][j].X2 = dirtyTiles[i][j].maxX + 1;
			}
			if (i == tileX2) {
				//jsem v koncovem tile X
				if ((x + w) > dirtyTiles[i][j].X2)
					dirtyTiles[i][j].X2 = x + w;
				if (x < dirtyTiles[i][j].minX)
					dirtyTiles[i][j].X1 = dirtyTiles[i][j].minX;
			}
			if ((i != tileX1) && (i != tileX2)) {
				
				//jsem v prostrednim tile X
				dirtyTiles[i][j].X1 = dirtyTiles[i][j].minX;
				dirtyTiles[i][j].X2 = dirtyTiles[i][j].maxX + 1;
			}
			
			if (j == tileY1) {
				//jsem v pocatecnim tile Y
				if (y < dirtyTiles[i][j].Y1)
					dirtyTiles[i][j].Y1 = y;
				if ((y + h) > dirtyTiles[i][j].maxY)
					dirtyTiles[i][j].Y2 = dirtyTiles[i][j].maxY + 1;
				
			}
			if (j == tileY2) {
				//jsem v koncovem tile Y
				if ((y + h) > dirtyTiles[i][j].Y2)
					dirtyTiles[i][j].Y2 = y + h;
				if (y < dirtyTiles[i][j].minY)
					dirtyTiles[i][j].Y1 = dirtyTiles[i][j].minY;
			}
			if ((j != tileY1) && (j != tileY2)) {
				//jsem v prostrednim tile Y
				dirtyTiles[i][j].Y1 = dirtyTiles[i][j].minY;
				dirtyTiles[i][j].Y2 = dirtyTiles[i][j].maxY + 1;
			}
			
		}
		
	}
	
	if (x < nDispDirtyX1)
		nDispDirtyX1 = x;
	if (x + w > nDispDirtyX2)
		nDispDirtyX2 = x + w;
	if (y < nDispDirtyY1)
		nDispDirtyY1 = y;
	if (y + h > nDispDirtyY2)
		nDispDirtyY2 = y + h; 	
}

void DirtyTiles::DispUpdate(uint8_t* framebuffer){
	//zkusim zjistit, jestli jde o 1 celistvy blok, pak je vyhodnejsi prekreslit cely najednou a nikoliv pomoci dlazdic
	int nSolid = 0;
	if ((nDispDirtyX1 < nDispDirtyX2) && (nDispDirtyY1 < nDispDirtyY2)) {
		int xStep = SCR_WIDTH / DIRTYTILESX;
		int yStep = SCR_HEIGHT / DIRTYTILESY;
		int tileX1 = (nDispDirtyX1 + 1) / xStep + ((nDispDirtyX1 + 1) % xStep > 0 ? 1 : 0) - 1;
		int tileY1 = (nDispDirtyY1 + 1) / yStep + ((nDispDirtyY1 + 1) % yStep > 0 ? 1 : 0) - 1;
		
		int tileX2 = (nDispDirtyX2 + 1) / xStep + ((nDispDirtyX2 + 1) % xStep > 0 ? 1 : 0) - 1;
		int tileY2 = (nDispDirtyY2 + 1) / yStep + ((nDispDirtyY2 + 1) % yStep > 0 ? 1 : 0) - 1;
		
		if (tileX2 >= DIRTYTILESX) {
			tileX2 = DIRTYTILESX - 1;
		}
		if (tileY2 >= DIRTYTILESY) {
			tileY2 = DIRTYTILESY - 1;
		}
		tileX1++;
		tileY1++;
		tileX2--;
		tileY2--;
		
		if ((tileX1 < tileX2) && (tileY1 < tileY2)) {
			nSolid = 1;
			for (int i = tileX1; (i <= tileX2) && (nSolid == 1); i++) {
				for (int j = tileY1; (j <= tileY2) && (nSolid == 1); j++) {
					if (!((dirtyTiles[i][j].X1 <= dirtyTiles[i][j].minX) && (dirtyTiles[i][j].X2 >= dirtyTiles[i][j].maxX) && (dirtyTiles[i][j].Y1 <= dirtyTiles[i][j].minY)
						&& (dirtyTiles[i][j].Y2 >= dirtyTiles[i][j].maxY))) {
						nSolid = 0;
					}
				}
			}
		}
	}
	if (nSolid == 1) {
		//jedna se o solid blok, prekreslim ho cely najednou
		scr->updateTile(framebuffer,0,nDispDirtyX1, nDispDirtyY1,  nDispDirtyX2, nDispDirtyY2);		
		scr->triggerRedraw();
	
	} else {
		//nejedna se o solid blok, proto kreslim po dlazdicich
		for (int j = 0; j < DIRTYTILESY; j++) {
			for (int i = 0; i < DIRTYTILESX; i++) {
				if ((dirtyTiles[i][j].X1 < dirtyTiles[i][j].X2) && (dirtyTiles[i][j].Y1 < dirtyTiles[i][j].Y2)) {					
					// set draw window	
					scr->updateTile(framebuffer,dirtyTiles[i][j].id,dirtyTiles[i][j].X1, dirtyTiles[i][j].Y1, dirtyTiles[i][j].X2, dirtyTiles[i][j].Y2);
				}
				
			}
		}
		scr->triggerRedraw();
	}
	// set dirty none
	DirtyTilesNone();	
}
