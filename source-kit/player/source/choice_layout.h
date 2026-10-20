#ifndef CHOICE_LAYOUT_H
#define CHOICE_LAYOUT_H

enum { CHOICE_CAPACITY=16, CHOICE_VIEW_HEIGHT=156, CHOICE_TEXT_WIDTH=104 };
typedef struct { int x,y,width,height,text_width; } ChoiceCell;
typedef struct { ChoiceCell cells[CHOICE_CAPACITY]; unsigned count; int height; } ChoiceLayout;

/* Widths use the same 12px glyph / 6px Korean space advances as rendering.
   Wide labels retain a full-width row and their complete wrapped text. */
void choice_layout(ChoiceLayout *layout,unsigned count,const int *widths,const int *full_lines);
int choice_hit(const ChoiceLayout *layout,int x,int y);
int choice_step(const ChoiceLayout *layout,int selected,int dx,int dy);
int choice_scroll(const ChoiceLayout *layout,int scroll);
int choice_focus(const ChoiceLayout *layout,int selected,int scroll);

#endif
