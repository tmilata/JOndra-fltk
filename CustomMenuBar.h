#ifndef CUSTOM_MENUBAR_H
#define CUSTOM_MENUBAR_H

#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "Debug.h"
#include "DirtyTiles.h"
#include "Ondra.h"

class CustomMenuBar : public Fl_Menu_Bar {
private:
    const Fl_Menu_Item* highlighted; // Aktuálne zvýraznená položka
	const Fl_Menu_Item* last_highlighted;
	int lastMenuX, lastMenuY, lastMenuW, lastMenuH;
public:
    CustomMenuBar(int X, int Y, int W, int H) : Fl_Menu_Bar(X, Y, W, H) {
        highlighted = NULL;
		last_highlighted=NULL;
    }
	
	int handle(int event) {
		static const Fl_Menu_Item* last_highlighted = NULL;
		
		switch (event) {
        case FL_ENTER: {
            int mx = Fl::event_x();
            int my = Fl::event_y();
            const Fl_Menu_Item* menu_items = this->menu();
            if (!menu_items) return 0;
			
            int xpos = x();
            highlighted = NULL;
			for (const Fl_Menu_Item* m=menu()->first(); m->text; m = m->next()) {
                int text_width = (int)fl_width(m->text) + 20;
                int text_height = (int)h();
				
                if (mx >= xpos && mx < xpos + text_width && my >= y() && my < y() + text_height) {						
					highlighted = m;
					if (Ondra::machine && Ondra::machine->til) {              					
					 Ondra::machine->til->DirtyTilesAll();
					}
					redraw();
					return 1;
					
                }
                xpos += text_width;
            }
            return 1;
					   }
			
        case FL_MOVE: { 
            int mx = Fl::event_x();
            int my = Fl::event_y();
            const Fl_Menu_Item* menu_items = this->menu();
            if (!menu_items) return 0;
			
            int xpos = x();
            const Fl_Menu_Item* hovered = NULL;
			for (const Fl_Menu_Item* m=menu()->first(); m->text; m = m->next()) {
                int text_width = (int)fl_width(m->text) + 20;
                int text_height = (int)h();
				
                if (mx >= xpos && mx < xpos + text_width && my >= y() && my < y() + text_height) {
					hovered = m;
                    break;
                }
                xpos += text_width;
            }
			
            if (hovered != last_highlighted) {  // Redraw jen pri zmene položky				
                last_highlighted = hovered;
                highlighted = hovered;
				if (Ondra::machine && Ondra::machine->til) {              
					//Ondra::machine->til->DispDirtyRectTiles(0, 0, 320, 66);
					Ondra::machine->til->DirtyTilesAll();
				}
                redraw();
            }
			
            return 1;
					  }
			
        case FL_LEAVE:
            highlighted = NULL;
			if (Ondra::machine && Ondra::machine->til) {              
				//Ondra::machine->til->DispDirtyRectTiles(0, 0, 320, 66);
				Ondra::machine->til->DirtyTilesAll();
			}
            redraw();
            return 1;
			
        case FL_PUSH:
            highlighted = NULL;
            redraw();
            //Fl::flush();
            return Fl_Menu_Bar::handle(event);
			
        default:
            return Fl_Menu_Bar::handle(event);
		}
	}
	
	
	
	void draw() {
		draw_box();
		if (!menu() || !menu()->text) return;
		const Fl_Menu_Item* m;
		int X = x()+6;
		for (m=menu()->first(); m->text; m = m->next()) {			
			int text_width = 0, text_height = 0;
			int W = m->measure(0,this) + 16;
			if (m == highlighted) {						
				fl_color(selection_color());
				fl_rectf(X-3, y()+1, W-4, h()-2);								
				fl_color(fl_contrast((Fl_Color)(FL_COLOR_CUBE-1),selection_color()));				
				fl_draw(m->text, X+3, y() + (h() + fl_height())/2 - fl_descent());
			}else{
				
				m->draw(X, y(), W, h(), this);
			}
			
			X += W;
			
			if (m->flags & FL_MENU_DIVIDER) {
				int y1 = y() + Fl::box_dy(box());
				int y2 = y1 + h() - Fl::box_dh(box()) - 1;
				
				// Draw a vertical divider between menus...
				fl_color(FL_DARK3);
				fl_yxline(X - 6, y1, y2);
				fl_color(FL_LIGHT3);
				fl_yxline(X - 5, y1, y2);
			}
			
		}
		if (Ondra::machine && Ondra::machine->til) {              
					//Ondra::machine->til->DispDirtyRectTiles(0, 0, 320, 66);
					Ondra::machine->til->DirtyTilesAll();
		}
	}
	
	
	
	
	
};

#endif
