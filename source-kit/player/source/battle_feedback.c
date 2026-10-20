#include "battle_feedback.h"
static uint16_t tint(uint16_t old,unsigned alpha){
    unsigned r=old&31,g=(old>>5)&31,b=(old>>10)&31;
    return 0x8000|((r*(16-alpha)+24*alpha+8)>>4)|
        (((g*(16-alpha)+28*alpha+8)>>4)<<5)|(((b*(16-alpha)+31*alpha+8)>>4)<<10);
}
static void dot(uint16_t *p,int x,int y,unsigned alpha){
    if((unsigned)x<256 && (unsigned)y<192)p[y*256+x]=tint(p[y*256+x],alpha);
}
static void line(uint16_t *p,int x,int y,int endx,int endy,unsigned alpha){
    int dx=endx>x?endx-x:x-endx,sx=x<endx?1:-1;
    int dy=endy>y?y-endy:endy-y,sy=y<endy?1:-1,error=dx+dy;
    for(;;){
        dot(p,x,y,alpha);if(x==endx && y==endy)break;
        int twice=error*2;
        if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}
    }
}
static void crack(uint16_t *p,unsigned hit){
    static const int centers[6][2]={{47,43},{209,143},{43,145},{209,45},{124,27},{132,167}};
    static const int spokes[12][2]={{30,2},{21,17},{12,31},{-3,26},{-19,27},{-29,11},
        {-32,-4},{-20,-18},{-8,-32},{8,-25},{21,-29},{31,-12}};
    int cx=centers[hit][0],cy=centers[hit][1];
    for(unsigned i=0;i<12;i++){
        unsigned next=(i+1)%12;int dx=spokes[i][0],dy=spokes[i][1];
        int bend=(int)((i*7+hit*3)%5)-2;
        int x1=cx+dx/7,y1=cy+dy/7,x2=cx+dx/2+bend,y2=cy+dy/2-bend;
        line(p,x1,y1,x2,y2,9);line(p,x2,y2,cx+dx,cy+dy,5);
        line(p,x1,y1,cx+spokes[next][0]/7,cy+spokes[next][1]/7,11);
        if((i+hit)%3){
            line(p,x2,y2,cx+spokes[next][0]/2,cy+spokes[next][1]/2,4);
            line(p,x2,y2,cx+dx+dy/5,cy+dy-dx/5,3);
        }
    }
}
unsigned battle_feedback_face(unsigned damage){return !damage?0:damage<=2?1:damage<=4?2:3;}
/* Brightness coefficients3/16,2/16,1/16: a six-display flash, peak18.75%. */
unsigned battle_feedback_flash(unsigned elapsed){return elapsed<6?3-elapsed/2:0;}
void battle_feedback_draw(uint16_t pixels[256*192],const uint16_t faces[4*4096],unsigned damage){
    const uint16_t *face=faces+battle_feedback_face(damage)*4096;
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
        uint16_t color=face[y*64+x];
        if(damage>=6){unsigned gray=((color&31)*77+((color>>5)&31)*150+((color>>10)&31)*29+128)>>8;
            color=0x8000|gray|(gray<<5)|(gray<<10);}
        pixels[(y+64)*256+x+96]=color;
    }
    for(unsigned hit=0;hit<damage && hit<6;hit++)crack(pixels,hit);
}
