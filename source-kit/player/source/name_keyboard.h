#pragma once
#include <stdbool.h>
#define NAME_KEYBOARD_LIMIT 17
#define NAME_KEYBOARD_KEYS 32
typedef struct {char cells[18];unsigned cursor,selected;} NameKeyboard;
enum { NAME_INPUT_PERSON,NAME_INPUT_PHONE,NAME_INPUT_PASSWORD,NAME_INPUT_NUMBER,NAME_INPUT_ANSWER };
static inline bool name_keyboard_numeric(unsigned kind){return kind==NAME_INPUT_PHONE || kind==NAME_INPUT_NUMBER;}
static inline unsigned name_keyboard_limit(unsigned kind){
    return (kind==NAME_INPUT_NUMBER || kind==NAME_INPUT_ANSWER)?13:kind==NAME_INPUT_PHONE?10:kind==NAME_INPUT_PASSWORD?12:17;
}
static inline bool name_keyboard_accepts(unsigned kind,unsigned ch){
    return name_keyboard_numeric(kind)?(ch>='0' && ch<='9') || ch=='*' || ch=='#':
        (ch>='A' && ch<='Z') || ch=='-' || ch=='.' || ch=='\'' || ch==' ';
}
static inline unsigned name_keyboard_dial_length(unsigned first){
    static const unsigned char lengths[10]={10,3,6,6,6,6,6,6,6,3};
    if(first=='*' || first=='#')return 2;
    return first>='0' && first<='9'?lengths[first-'0']:0;
}
unsigned name_keyboard_count(unsigned kind);
unsigned name_keyboard_confirm(unsigned kind);
unsigned name_keyboard_erase(unsigned kind);
bool name_keyboard_character_for(NameKeyboard *k,unsigned kind,char ch);
char name_keyboard_key_for(unsigned kind,unsigned key);
void name_keyboard_box_for(unsigned kind,unsigned key,int *x,int *y,int *w,int *h);
int name_keyboard_hit_for(unsigned kind,int x,int y);
void name_keyboard_move_for(NameKeyboard *k,unsigned kind,int dx,int dy);
/* Source118F4/11938: complete a telephone number automatically. */
bool name_keyboard_ready(const NameKeyboard *k,unsigned kind);
/* Source1189E: original actor whose control byte is cleared on dial input. */
int name_keyboard_dial_actor(char ch);
void name_keyboard_reset(NameKeyboard *k);
bool name_keyboard_character(NameKeyboard *k,char ch);
bool name_keyboard_delete(NameKeyboard *k);
void name_keyboard_box(unsigned key,int *x,int *y,int *w,int *h);
int name_keyboard_hit(int x,int y);
void name_keyboard_move(NameKeyboard *k,int dx,int dy);
char name_keyboard_key(unsigned key);
