#include <string.h>
#include "combat.h"
#include "combat_damage.h"
#include "hq_scene.h"
#include "../generated/battle_rules_r21.h"
#include "../generated/battle_layouts_r22.h"
static const CombatLayout *layout=&combat_layouts[0];
int combat_cell_x(unsigned cell){return cell<9?layout->x[cell]:0;}
int combat_cell_y(unsigned cell){return cell<9?layout->y[cell]:0;}
CombatState combat_state;
/* SUBCODE FD96 and12030. The port keeps this arithmetic and probability map;
   the source's asynchronously consumed global random stream is not reproduced. */
static unsigned random_cell(void){
    CombatState *s=&combat_state;
    s->random_a=(s->random_a&255)|((((s->random_a>>8)+(s->random_a&255))&255)<<8);
    s->random_a=(uint16_t)(s->random_a+s->random_b+((s->random_b<<8)|(s->random_b>>8)));
    unsigned top=(s->random_b>>8)+(s->random_b&255);
    s->random_b=(s->random_b&255)|((top&255)<<8);
    if(top>255)s->random_a++;
    if(++s->random_b==0)s->random_a++;
    return battle_random_map[s->random_a&31];
}
/* K1 phase markers0204070F1216181D and its actual sprite command list.
   Low nibble is the frame hold;10 repeats the pose until the AI marker gate.
   81 redirects to a command index. No envelope or fabricated sprite poses. */
static const unsigned char markers[8]={2,4,7,15,18,22,24,29};
static const unsigned char pictures[31]={0,0,0,1,1,0x81,2,2,0x81,3,2,3,2,3,2,3,0x81,3,3,0x81,1,0,0,0x81,2,3,4,5,6,6,0x81};
static const unsigned char times[31]={0,0,8,8,17,4,2,17,7,32,6,6,6,6,6,17,15,2,17,18,8,8,17,22,2,2,4,4,4,17,29};
static void animate(CombatEnemy *enemy){
    if(!enemy->phase)return;
    if(enemy->clock){enemy->clock--;return;}
    for(unsigned budget=0;budget<4;budget++){
        if(enemy->cursor>=31){enemy->phase=0;return;}
        unsigned cursor=enemy->cursor;
        if(pictures[cursor]==0x81){enemy->cursor=times[cursor];continue;}
        enemy->image=pictures[cursor];
        enemy->clock=(times[cursor]&15)?(times[cursor]&15)-1:0;
        if(!(times[cursor]&16))enemy->cursor++;
        return;
    }
}
static void next_phase(CombatEnemy *enemy,unsigned phase){
    enemy->cursor=markers[phase-1]+2;enemy->phase=phase;
    enemy->timer=(int)(2*random_cell()+combat_state.delay);
}
bool combat_begin(unsigned stage,unsigned scene,uint32_t seed){
    if(stage!=1)return false;
    const CombatLayout *selected=NULL;
    for(unsigned i=0;i<COMBAT_LAYOUT_COUNT;i++)
        if(scene==HQ_SCENE(combat_layouts[i].scene))selected=&combat_layouts[i];
    if(!selected)return false;
    layout=selected;
    memset(&combat_state,0,sizeof(combat_state));
    combat_state.delay=BATTLE_INITIAL_DELAY;combat_state.spawn_timer=BATTLE_INITIAL_SPAWN;
    combat_state.random_a=seed>>16;combat_state.random_b=seed;combat_state.aim=4;
    return true;
}
unsigned combat_sprite_frame(unsigned cell){
    return cell<9?layout->frame_base[cell]+combat_state.enemies[cell].image:0;
}
int combat_touch_cell(int x,int y){
    if(x<0 || x>=256 || y<0 || y>=192)return -1;
    unsigned col=(unsigned)x*3/256,row=(unsigned)y/60;
    if(row>2)row=2;
    return (int)(row*3+col);
}
unsigned combat_effects(void){unsigned events=combat_state.effects;combat_state.effects=0;return events;}
unsigned combat_tick(unsigned aim,bool fire,bool toggle_weapon){
    CombatState *s=&combat_state;
    if(s->status!=COMBAT_ACTIVE)return s->status;
    s->ticks++;s->aim=aim<9?aim:4;
    if(s->recoil)s->recoil--;
    if(toggle_weapon && !s->recoil){s->weapon=!s->weapon;if(s->weapon)s->effects|=8;}
    if(fire && s->weapon && !s->recoil && !toggle_weapon && !s->shot_phase){s->shot_cell=s->aim;s->shots++;s->shot_phase=1;s->shot_clock=0;s->effects|=2;}
    /* Source4->5->1->2->3 pipeline resolves in10-12 VBlanks in the native
       stage1 trace. Keep the latched cell while touch/aim changes afterward. */
    if(s->shot_phase && ++s->shot_clock>=11){
        CombatEnemy *target=&s->enemies[s->shot_cell];
        if(target->phase>=2 && target->phase<=4){
            target->phase=7;target->cursor=markers[6];
            if((s->kills&7)==7 && s->delay>=2)s->delay-=2;
            s->kills++;s->effects|=1;
        }else s->misses++;
        s->shot_phase=0;
    }
    unsigned active=0;
    for(unsigned cell=0;cell<9;cell++){
        CombatEnemy *enemy=&s->enemies[cell];if(!enemy->phase)continue;
        active++;
        if(enemy->phase==1 && enemy->cursor==markers[1])next_phase(enemy,2);
        else if(enemy->phase>=2 && enemy->phase<=4){
            if(--enemy->timer<0){
                unsigned phase=enemy->phase;
                if(enemy->cursor==markers[phase]){
                    next_phase(enemy,phase+1);
                    if(phase==2 && s->kills>BATTLE_TARGET_KILLS)next_phase(enemy,5);
                    if(phase==3 && !s->beam_phase && !s->recoil){s->beam_cell=cell;s->beam_phase=1;s->beam_clock=0;s->effects|=4;}
                    if(phase==4 && s->kills>BATTLE_TARGET_KILLS && random_cell()<6)next_phase(enemy,3);
                }else enemy->timer=0;
            }
        }else if(enemy->phase==5 && enemy->cursor==markers[5])enemy->phase=0;
        else if(enemy->phase==7 && enemy->cursor==markers[7])enemy->phase=0;
        animate(enemy);
    }
    if(s->beam_phase && ++s->beam_clock>=16){
        s->beam_phase=0;s->damage++;s->recoil=56;s->effects|=16;
    }
    if(s->spawn_phase==0){
        if(--s->spawn_timer<0 && s->kills<BATTLE_TARGET_KILLS && active<BATTLE_MAX_ACTIVE){
            s->spawn_timer=(int)(2*random_cell()+16+s->delay);
            if(random_cell()>=7)s->spawn_timer=5;
            s->spawn_phase=1;
        }
    }else if(s->spawn_phase==1){
        unsigned cell=random_cell();
        if((layout->allowed_mask&(1u<<cell)) && !s->enemies[cell].phase){s->spawn_cell=cell;s->spawn_phase=2;}
    }else{
        CombatEnemy *enemy=&s->enemies[s->spawn_cell];
        enemy->phase=1;enemy->cursor=2;enemy->clock=0;enemy->image=0;s->spawn_phase=0;
    }
    active=0;for(unsigned cell=0;cell<9;cell++)if(s->enemies[cell].phase)active++;
    if(s->damage>=BATTLE_DAMAGE_LIMIT)s->status=COMBAT_LOST;
    else if(s->kills>=BATTLE_TARGET_KILLS && !active && !s->shot_phase && !s->beam_phase)s->status=COMBAT_WON;
    return s->status;
}
static void blit(uint16_t *pixels,const uint16_t *sprite,int nx,int ny,const CombatDamageRow *rows){
    /* Same nearest-neighbor coordinates as the full192x128 original viewport. */
    int left=(nx-32)*4,right=nx*4,top=(ny-8)*3,bottom=(ny+24)*3;
    left=left>0?(left+2)/3:0;right=right>0?(right+2)/3:0;
    top=top>0?(top+1)/2:0;bottom=bottom>0?(bottom+1)/2:0;
    if(right>256)right=256;if(bottom>192)bottom=192;
    for(int y=top;y<bottom;y++){
        int sy=8+y*128/192-ny;
        int first=left,last=right;
        if(rows){if(first<rows[y].left)first=rows[y].left;if(last>rows[y].right)last=rows[y].right;}
        for(int x=first;x<last;x++){
            int sx=32+x*192/256-nx;
            uint16_t color=sprite[sy*32+sx];if(color)pixels[y*256+x]=color;
        }
    }
}
void combat_picture_key(uint32_t key[14]){
    for(unsigned c=0;c<9;c++)key[c]=combat_state.enemies[c].phase?combat_sprite_frame(c)+1:0;
    key[9]=combat_state.beam_phase?29+combat_state.beam_clock*7/16:0;
    key[10]=combat_state.beam_phase?combat_state.beam_cell:0;
    key[11]=combat_state.damage;key[12]=combat_state.weapon;
    key[13]=combat_state.weapon?combat_state.aim:0;
}
void combat_render(uint16_t *pixels,const uint16_t *scene,const uint16_t *sprites){
    const int16_t *x=layout->x,*y=layout->y;
    memcpy(pixels,scene,256*192*2);
    for(unsigned cell=0;cell<9;cell++)if(combat_state.enemies[cell].phase)
        blit(pixels,sprites+1024*combat_sprite_frame(cell),x[cell],y[cell],NULL);
    if(combat_state.beam_phase){
        unsigned frame=28+(combat_state.beam_clock*7/16);
        unsigned cell=combat_state.beam_cell;
        blit(pixels,sprites+1024*frame,x[cell],y[cell],NULL);
    }
}

void combat_render_rows(uint16_t *pixels,const uint16_t *scene,const uint16_t *sprites,const CombatDamageRow rows[192]){
    const int16_t *x=layout->x,*y=layout->y;
    for(unsigned y=0;y<192;y++)if(rows[y].left<rows[y].right)
        memcpy(pixels+y*256+rows[y].left,scene+y*256+rows[y].left,(rows[y].right-rows[y].left)*2);
    for(unsigned cell=0;cell<9;cell++)if(combat_state.enemies[cell].phase)
        blit(pixels,sprites+1024*combat_sprite_frame(cell),x[cell],y[cell],rows);
    if(combat_state.beam_phase){unsigned frame=28+combat_state.beam_clock*7/16,cell=combat_state.beam_cell;
        blit(pixels,sprites+1024*frame,x[cell],y[cell],rows);}
}
