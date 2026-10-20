#include "resource_cards.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static void putword(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
static void clear_secondary(resource_cards_state *s){
    memset(s->main+0x70,0,8);
    putword(s->cached_registers+6,word(s->cached_registers+6)&0xfff8);
    memset(s->vscroll,0,4);memset(s->hscroll,0,4);
}
bool resource_cards_primary(resource_cards_state *s,unsigned request,resource_cards_port port,void *ctx){
    if(!s || !port || request>255)return false;
    if(request && request!=254 && request!=15 && (request<23 || request>29) && (request<8 || request>12))return false;
    uint8_t *m=s->main;
    if(request)m[0x60]=request;
    unsigned mode=m[0x60];
    if(mode==m[0x61] && mode)return true;
    if(mode==254){
        if(m[0x61]==23){
            memset(s->hscroll,0,4);
            if(word(m+0x6a)){
                memset(m+0x60,0,16);m[0x60]=m[0x61]=15;return true;
            }
        }
        if(!port(ctx,0xc00004,2,word(s->cached_registers)))return false;
        memset(m+0x60,0,10);memset(m+0x6c,0,4);return true;
    }
    if(!mode)return true;
    if(mode==15 && m[0x61]==23){m[0x60]=23;putword(m+0x6a,1);return true;}
    m[0x61]=mode;memset(m+0x62,0,8);return true;
}
bool resource_cards_tick(resource_cards_state *s,resource_cards_port port,void *ctx){
    if(!s || !port)return false;
    /* Telephone portrait slides through the original backgroundB window.
     * FF3B3E uses a separate phase at E066, preserving the caller's mode15. */
    if(s->main[0x60]==23){
        unsigned phase=word(s->main+0x66);
        if(phase==0){putword(s->main+0x68,48);putword(s->main+0x66,1);return true;}
        if(phase==1){
            uint16_t offset=word(s->main+0x68)+8;
            if((int16_t)(offset-96)>=0){offset=96;putword(s->main+0x66,2);}
            putword(s->main+0x68,offset);
        }else if(phase!=2)return false;
        putword(s->main+0x6c,17);putword(s->main+0x32,0x1194);return true;
    }
    /* Main FF3B82/FF3BC0/FF3BDC: phone reveal, hold and close.
     * These are original VSRAM windows, not a replacement bitmap fade. */
    if(s->main[0x60]>=24 && s->main[0x60]<=26){
        uint8_t *m=s->main;unsigned mode=m[0x60],phase=word(m+0x62);
        if(mode==25){
            if(!phase){putword(m+0x6c,19);putword(m+0x32,0x11f4);
                putword(m+0x64,96);putword(m+0x62,1);}
            return true;
        }
        if(!phase){
            putword(m+0x6c,mode==24?18:20);putword(m+0x32,0x11f4);
            putword(m+0x64,mode==24?0xffc0:96);putword(m+0x62,1);return true;
        }
        if(phase==1){
            uint16_t value=word(m+0x64)+(mode==24?8:-8);
            if(mode==24?(int16_t)value>=0:(int16_t)(value-48)<0){
                value=mode==24?0:48;putword(m+0x62,2);
            }
            putword(m+0x64,value);return true;
        }
        if(phase!=2)return false;
        if(mode==26){memset(s->hscroll,0,4);putword(m+0x6a,0);}
        memset(m+0x60,0,10);memset(m+0x6c,0,4);return true;
    }
    /* Original FF3C22: mode27 has only initialization and a hold phase.
     * Its lower portrait window uses the same FF11F4 raster ISR as18..20. */
    if(s->main[0x60]==27){
        uint8_t *m=s->main;unsigned phase=word(m+0x62);
        if(!phase){putword(m+0x6c,21);putword(m+0x32,0x11f4);
            putword(m+0x64,0xffc0);putword(m+0x62,1);return true;}
        return phase==1;
    }
    /* Original FF3C48/FF3C64: the S2 phone window holds or closes.
     * Its ISR123A counts scanlines, unlike the older eight-line ISR bands. */
    if(s->main[0x60]==28 || s->main[0x60]==29){
        uint8_t *m=s->main;unsigned mode=m[0x60],phase=word(m+0x62);
        if(!phase){putword(m+0x6c,mode==28?22:23);putword(m+0x32,0x123a);
            putword(m+0x64,96);putword(m+0x62,1);return true;}
        if(mode==28)return true;
        if(phase==1){
            uint16_t value=word(m+0x64)-8;
            if((int16_t)(value-48)<0){value=48;putword(m+0x62,2);}
            putword(m+0x64,value);return true;
        }
        if(phase!=2)return false;
        memset(s->hscroll,0,4);putword(m+0x6a,0);
        memset(m+0x60,0,10);memset(m+0x6c,0,4);return true;
    }
    if(s->main[0x60]==15){
        if(!word(s->main+0x62)){
            putword(s->main+0x6c,9);putword(s->main+0x32,0x0efa);
            putword(s->main+0x64,256);putword(s->main+0x62,1);
        }
        return true;
    }
    if(s->main[0x60]<8 || s->main[0x60]>12)return false;
    if((int16_t)word(s->main+0x62)<1){
        putword(s->main+0x64,23);putword(s->main+0x6c,s->main[0x60]-6);
        putword(s->main+0x32,0xd6e);putword(s->main+0x62,word(s->main+0x62)+1);
        clear_secondary(s);
    }else{
        s->main[0x65]--;
        if((int8_t)s->main[0x65]<0){
            putword(s->main+0x64,0);
            if(s->main[0x60]!=12)return true;
            if(!port(ctx,0xc00004,2,word(s->cached_registers)))return false;
            putword(s->main+0x78,0);memset(s->main+0x86,0,6);
            memset(s->main+0x60,0,16);clear_secondary(s);return true;
        }
    }
    s->main[0x64]=s->main[0x65]>>2;return true;
}
static bool visible(resource_cards_state *s,bool enabled,resource_cards_port port,void *ctx){
    if(enabled)s->cached_registers[3]|=64;else s->cached_registers[3]&=~64;
    return port(ctx,0xc00004,2,word(s->cached_registers+2));
}
static bool vertical(resource_cards_state *s,bool negative,resource_cards_port port,void *ctx){
    uint16_t value=s->main[0x64];if(negative)value=-value;
    return port(ctx,0xc00004,2,0x8f02) && port(ctx,0xc00004,4,0x40000010) && port(ctx,0xc00000,2,value);
}
bool resource_cards_hblank(resource_cards_state *s,unsigned counter,resource_cards_port port,void *ctx){
    if(!s || !port)return false;
    unsigned effect=word(s->main+0x6c);
    if(effect==22 || effect==23){
        /* Original picture-only viewport is scanlines0..127. At136 the
         * original ISR uploads the excluded lower text/HUD palette. That
         * region is rendered by the DS lower UI, not this picture kernel. */
        if(counter>=128)return false;
        if(counter!=72)return true;
        uint32_t value=(-word(s->main+0x64))&1023;
        return port(ctx,0xc00004,2,0x8f02) && port(ctx,0xc00004,4,0x40000010) &&
            port(ctx,0xc00000,4,value);
    }
    if(effect>=18 && effect<=21){
        if(counter!=9)return true;
        uint32_t value=(-word(s->main+0x64))&1023;
        return port(ctx,0xc00004,2,0x8f02) && port(ctx,0xc00004,4,0x40000010) &&
            port(ctx,0xc00000,4,value) && port(ctx,0xc00004,2,word(s->cached_registers));
    }
    if(effect==17){
        if(counter!=9 && counter!=17)return true;
        uint32_t value=counter==9?(-word(s->main+0x68))&1023:
            ((uint32_t)word(s->hscroll)<<16)|word(s->hscroll+2);
        if(!port(ctx,0xc00004,2,0x8f02) || !port(ctx,0xc00004,4,0x40000010) ||
           !port(ctx,0xc00000,4,value))return false;
        return counter!=17 || port(ctx,0xc00004,2,word(s->cached_registers));
    }
    if(effect==9){
        if(counter!=17)return true;
        uint32_t value=word(s->main+0x64);value|=value<<16;
        return port(ctx,0xc00004,2,0x8f02) && port(ctx,0xc00004,4,0x40000010) &&
            port(ctx,0xc00000,4,value) && port(ctx,0xc00004,2,word(s->cached_registers)) &&
            port(ctx,0xc00004,4,0x78000003) && port(ctx,0xc00000,4,0);
    }
    if(effect<2 || effect>6)return false;
    unsigned first=effect==2?0:9+(effect-3)*3,last=first+(effect==2?7:1);
    if(counter==0 && !visible(s,true,port,ctx))return false;
    if(counter==first)return vertical(s,true,port,ctx);
    if(counter==last)return vertical(s,false,port,ctx);
    if(counter==last+1)return visible(s,false,port,ctx) && port(ctx,0xc00004,2,word(s->cached_registers));
    return true;
}
