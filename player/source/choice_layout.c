#include "choice_layout.h"

void choice_layout(ChoiceLayout *layout,unsigned count,const int *widths,const int *full_lines){
    if(count>CHOICE_CAPACITY)count=CHOICE_CAPACITY;
    layout->count=count;
    int y=0;
    for(unsigned i=0;i<count;){
        if(widths[i]>CHOICE_TEXT_WIDTH){
            int lines=full_lines[i]>0?full_lines[i]:1;
            layout->cells[i++]=(ChoiceCell){8,y,240,26+16*(lines-1),216};
            y+=26+16*(lines-1)+4;
        }else{
            layout->cells[i++]=(ChoiceCell){8,y,116,26,CHOICE_TEXT_WIDTH};
            if(i<count && widths[i]<=CHOICE_TEXT_WIDTH)
                layout->cells[i++]=(ChoiceCell){132,y,116,26,CHOICE_TEXT_WIDTH};
            y+=30;
        }
    }
    layout->height=y?y-4:0;
}

int choice_hit(const ChoiceLayout *layout,int x,int y){
    for(unsigned i=0;i<layout->count;i++){
        const ChoiceCell *cell=&layout->cells[i];
        if(x>=cell->x && x<cell->x+cell->width && y>=cell->y && y<cell->y+cell->height)return (int)i;
    }
    return -1;
}

int choice_scroll(const ChoiceLayout *layout,int scroll){
    int maximum=layout->height>CHOICE_VIEW_HEIGHT?layout->height-CHOICE_VIEW_HEIGHT:0;
    return scroll<0?0:scroll>maximum?maximum:scroll;
}

int choice_focus(const ChoiceLayout *layout,int selected,int scroll){
    if(selected>=0 && (unsigned)selected<layout->count){
        const ChoiceCell *cell=&layout->cells[selected];
        if(cell->y<scroll)scroll=cell->y;
        else if(cell->y+cell->height>scroll+CHOICE_VIEW_HEIGHT)scroll=cell->y+cell->height-CHOICE_VIEW_HEIGHT;
    }
    return choice_scroll(layout,scroll);
}

int choice_step(const ChoiceLayout *layout,int selected,int dx,int dy){
    if(selected<0 || (unsigned)selected>=layout->count)return 0;
    const ChoiceCell *from=&layout->cells[selected];
    int center=from->x+from->width/2,best=selected,best_distance=100000;
    for(unsigned i=0;i<layout->count;i++){
        const ChoiceCell *to=&layout->cells[i];
        int delta_x=to->x+to->width/2-center,delta_y=to->y-from->y;
        int distance;
        if(dx){
            if(delta_y || delta_x*dx<=0)continue;
            distance=delta_x*dx;
        }else{
            if(delta_y*dy<=0)continue;
            distance=delta_y*dy*256+(delta_x<0?-delta_x:delta_x);
        }
        if(distance<best_distance){best=(int)i;best_distance=distance;}
    }
    return best;
}
