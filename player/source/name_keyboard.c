#include "name_keyboard.h"
#include <string.h>
#include <stdlib.h>
void name_keyboard_reset(NameKeyboard *k){memset(k,0,sizeof(*k));}
char name_keyboard_key(unsigned key){
    static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ-.'";
    return key<29?chars[key]:key==30?' ':0;
}
bool name_keyboard_character(NameKeyboard *k,char ch){
    return name_keyboard_character_for(k,NAME_INPUT_PERSON,ch);
}
bool name_keyboard_character_for(NameKeyboard *k,unsigned kind,char ch){
    unsigned limit=name_keyboard_limit(kind);
    if(kind>NAME_INPUT_PASSWORD || k->cursor>=limit || !name_keyboard_accepts(kind,(unsigned char)ch))return false;
    /* Original11868 keeps the insertion cursor on the final cell. A further
     * character replaces that cell; it does not grow beyond17 characters. */
    k->cells[k->cursor]=ch;
    if(k->cursor+1<limit)k->cursor++;
    k->cells[limit]=0;return true;
}
bool name_keyboard_delete(NameKeyboard *k){
    /* Source11A66 deletes the last occupied cell, including a full final slot. */
    if(!k->cursor)return false;
    unsigned i=k->cursor;if(!k->cells[i])i--;
    k->cells[i]=0;k->cursor=i;return true;
}
void name_keyboard_box(unsigned key,int *x,int *y,int *w,int *h){
    if(key<29){*x=8+(key%10)*24;*y=58+(key/10)*26;*w=22;*h=23;}
    else{*x=8+(key-29)*80;*y=144;*w=78;*h=30;}
}
int name_keyboard_hit(int x,int y){
    for(unsigned i=0;i<32;i++){int a,b,w,h;name_keyboard_box(i,&a,&b,&w,&h);if(x>=a && x<a+w && y>=b && y<b+h)return i;}
    return -1;
}
void name_keyboard_move(NameKeyboard *k,int dx,int dy){
    if(k->selected>=32)k->selected=0;
    unsigned row=k->selected<29?k->selected/10:3;
    unsigned first=row*10,count=row<2?10:row==2?9:3;
    if(row==3)first=29;
    if(dx){k->selected=first+(k->selected-first+count+(dx>0?1:-1))%count;return;}
    if(!dy)return;
    int x,y,w,h;name_keyboard_box(k->selected,&x,&y,&w,&h);int center=x+w/2;
    row=(row+4+(dy>0?1:-1))%4;first=row==3?29:row*10;count=row<2?10:row==2?9:3;
    int distance=1000;
    for(unsigned i=first;i<first+count;i++){
        name_keyboard_box(i,&x,&y,&w,&h);int d=abs(x+w/2-center);
        if(d<distance){distance=d;k->selected=i;}
    }
}
unsigned name_keyboard_count(unsigned kind){return kind==NAME_INPUT_PHONE?13:32;}
/* Original116A2 ignores START in dial mode. Its confirm index is outside
 * the active key set: the source length test alone completes a number. */
unsigned name_keyboard_confirm(unsigned kind){return kind==NAME_INPUT_PHONE?13:31;}
unsigned name_keyboard_erase(unsigned kind){return kind==NAME_INPUT_PHONE?12:29;}
char name_keyboard_key_for(unsigned kind,unsigned key){
    static const char dial[]="123456789*0#";
    return kind==NAME_INPUT_PHONE?(key<12?dial[key]:0):name_keyboard_key(key);
}
void name_keyboard_box_for(unsigned kind,unsigned key,int *x,int *y,int *w,int *h){
    if(kind!=NAME_INPUT_PHONE){name_keyboard_box(key,x,y,w,h);return;}
    if(key<12){*x=8+(key%3)*80;*y=57+(key/3)*25;*w=78;*h=23;}
    else{*x=8;*y=159;*w=238;*h=25;}
}
int name_keyboard_hit_for(unsigned kind,int x,int y){
    for(unsigned i=0;i<name_keyboard_count(kind);i++){
        int a,b,w,h;name_keyboard_box_for(kind,i,&a,&b,&w,&h);
        if(x>=a && x<a+w && y>=b && y<b+h)return i;
    }
    return -1;
}
void name_keyboard_move_for(NameKeyboard *k,unsigned kind,int dx,int dy){
    if(kind!=NAME_INPUT_PHONE){name_keyboard_move(k,dx,dy);return;}
    if(k->selected>=13)k->selected=0;
    unsigned row=k->selected<12?k->selected/3:4,first=row*3,count=row==4?1:3;
    if(dx){k->selected=first+(k->selected-first+count+(dx>0?1:-1))%count;return;}
    if(!dy)return;
    int x,y,w,h;name_keyboard_box_for(kind,k->selected,&x,&y,&w,&h);int center=x+w/2;
    row=(row+5+(dy>0?1:-1))%5;first=row*3;count=row==4?1:3;
    int distance=1000;
    for(unsigned i=first;i<first+count;i++){
        name_keyboard_box_for(kind,i,&x,&y,&w,&h);int d=abs(x+w/2-center);
        if(d<distance){distance=d;k->selected=i;}
    }
}
bool name_keyboard_ready(const NameKeyboard *k,unsigned kind){
    if(kind!=NAME_INPUT_PHONE)return false;
    unsigned n=name_keyboard_dial_length((unsigned char)k->cells[0]);
    return n && k->cells[n-1]!=0;
}
int name_keyboard_dial_actor(char ch){
    if(ch=='*')return 2;
    if(ch=='#')return 3;
    return ch>='0' && ch<='9'?(ch&31):-1;
}
