#include "combat_damage.h"
#include "combat.h"

static void include(CombatDamageRow rows[192],int left,int top,int right,int bottom){
    if(left<0)left=0;if(top<0)top=0;
    if(right>256)right=256;if(bottom>192)bottom=192;
    if(left>=right || top>=bottom)return;
    /* 32-bit aligned transfers also cover all original 16-bit pixels. */
    left&=~1;right=(right+1)&~1;
    for(int y=top;y<bottom;y++){
        if(left<rows[y].left)rows[y].left=left;
        if(right>rows[y].right)rows[y].right=right;
    }
}
static void sprite(CombatDamageRow rows[192],unsigned cell){
    int x=combat_cell_x(cell),y=combat_cell_y(cell);
    if(cell>=9)return;
    /* Exact preimage of source coordinates32+x*192/256,8+y*128/192
     * intersecting the original32x32 sprite. Keep clipping identical. */
    int l=(x-32)*4,r=x*4,t=(y-8)*3,b=(y+24)*3;
    include(rows,l>0?(l+2)/3:0,t>0?(t+1)/2:0,r>0?(r+2)/3:0,b>0?(b+1)/2:0);
}
static void aim(CombatDamageRow rows[192],unsigned cell){
    if(cell>=9)return;
    int x=43+85*(cell%3),y=36+60*(cell/3);
    include(rows,x-6,y-6,x+7,y+7);
}
void combat_damage_rows(const uint32_t before[14],const uint32_t after[14],CombatDamageRow rows[192]){
    for(unsigned y=0;y<192;y++){rows[y].left=256;rows[y].right=0;}
    for(unsigned c=0;c<9;c++)if(before[c]!=after[c])sprite(rows,c);
    if(before[9]!=after[9] || before[10]!=after[10]){
        if(before[9])sprite(rows,before[10]);
        if(after[9])sprite(rows,after[10]);
    }
    if(before[11]!=after[11])include(rows,0,0,256,17);
    if(before[12]!=after[12] || before[13]!=after[13]){
        if(before[12])aim(rows,before[13]);
        if(after[12])aim(rows,after[13]);
    }
}
