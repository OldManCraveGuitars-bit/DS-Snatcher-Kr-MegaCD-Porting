#include "resource_vdp_history.h"
#include <stdlib.h>
#include <string.h>
static unsigned be16(const uint8_t *p){return p[0]*256u+p[1];}
static unsigned le16(const uint8_t *p){return p[0]+p[1]*256u;}
static void put16(uint8_t *p,unsigned x){p[0]=x;p[1]=x>>8;}
static bool same_band(const resource_opening_state *s,unsigned a,unsigned b){
    return s->raster_visible[a]==s->raster_visible[b] &&
        s->raster_vertical[0][a]==s->raster_vertical[0][b] &&
        s->raster_vertical[1][a]==s->raster_vertical[1][b] &&
        s->raster_horizontal[0][a]==s->raster_horizontal[0][b] &&
        s->raster_horizontal[1][a]==s->raster_horizontal[1][b];
}
static unsigned encode(const resource_opening_state *s,uint8_t *out){
    if(s->front_sprites>64)return 0;
    memset(out,0,16);put16(out,s->plane_b);out[2]=s->front_sprites;
    memcpy(out+4,s->cards.vscroll,4);memcpy(out+8,s->cards.hscroll,4);
    memcpy(out+12,s->cards.cached_registers+6,2);memcpy(out+14,s->cards.main+0x6c,2);
    unsigned at=16+s->front_sprites*8;
    memcpy(out+16,s->front_sat,s->front_sprites*8);
    for(unsigned y=0;y<224;){
        unsigned n=1;while(y+n<224 && same_band(s,y,y+n))n++;
        out[at]=n;out[at+1]=s->raster_visible[y];
        put16(out+at+2,(uint16_t)s->raster_vertical[0][y]);
        put16(out+at+4,(uint16_t)s->raster_vertical[1][y]);
        put16(out+at+6,(uint16_t)s->raster_horizontal[0][y]);
        put16(out+at+8,(uint16_t)s->raster_horizontal[1][y]);
        at+=10;y+=n;
    }
    return at;
}
static bool valid_meta(const uint8_t *p,unsigned bytes){
    if(bytes<26 || bytes>RESOURCE_VDP_META_MAX || p[2]>64 || le16(p)>61440 || (le16(p)&1))return false;
    unsigned at=16+p[2]*8,y=0;
    if(at>bytes || (bytes-at)%10)return false;
    while(at<bytes){
        unsigned n=p[at];if(!n || n>224-y)return false;
        y+=n;at+=10;
    }
    return y==224;
}
static bool visible_sprite(const resource_background_sprite *s,unsigned y,unsigned rows){
    unsigned w=((s->size>>10&3)+1)*8,h=((s->size>>8&3)+1)*8;
    return s->x<224 && s->x+(int)w>32 && s->y<(int)(y+rows) && s->y+(int)h>(int)y;
}
static resource_background_sprite read_sprite(const uint8_t *p){
    resource_background_sprite s;
    s.x=(be16(p+6)&511)-128;s.y=(be16(p)&511)-128;
    s.size=be16(p+2);s.attribute=be16(p+4);return s;
}
/* Bit0 changes output; bit1 changes plane-to-pattern dependencies too. */
static unsigned visible_metadata_changed(const resource_vdp_view *v,const uint8_t *p,unsigned bytes){
    bool line_scroll=(be16(p+12)&3)==3 || be16(p+14)==9;
    if(v->plane_b!=le16(p) || v->line_scroll!=line_scroll)return 3;
    for(unsigned plane=0;plane<2;plane++)
        if(!line_scroll && v->scroll_x[plane]!=be16(p+4+plane*2))return 3;
    /* Invisible SAT entries cannot own a pixel in this viewport. Preserve
     * the order and every attribute of all entries that can overlap it. */
    unsigned old=0,changed=0;
    for(unsigned i=0;i<p[2];i++){
        resource_background_sprite s=read_sprite(p+16+i*8);
        if(!visible_sprite(&s,v->visible_y,v->visible_rows))continue;
        while(old<v->sprite_count && !visible_sprite(v->sprites+old,v->visible_y,v->visible_rows))old++;
        if(old==v->sprite_count){changed=1;break;}
        const resource_background_sprite *before=v->sprites+old++;
        if(s.x!=before->x || s.y!=before->y || s.size!=before->size || s.attribute!=before->attribute){changed=1;break;}
    }
    while(old<v->sprite_count)
        if(visible_sprite(v->sprites+old++,v->visible_y,v->visible_rows)){changed=1;break;}
    unsigned y=0,last=v->visible_y+v->visible_rows;
    for(unsigned at=16+p[2]*8;at<bytes;at+=10){
        unsigned end=y+p[at];
        for(;y<end;y++)if(y>=v->visible_y && y<last){
            if(v->visible[y]!=p[at+1])changed=1;
            for(unsigned plane=0;plane<2;plane++){
                if(v->vertical[plane][y]!=(int16_t)le16(p+at+2+plane*2))return 3;
                if(line_scroll && v->horizontal[plane][y]!=(int16_t)le16(p+at+6+plane*2))return 3;
            }
        }
    }
    return changed;
}
static void mark_visible_tile(uint32_t bits[64],unsigned tile){
    bits[tile>>5]|=1u<<(tile&31);
}
volatile unsigned resource_vdp_dependency_builds,resource_vdp_dependency_reuses;
static void remember_visible_tiles(resource_vdp_view *v,unsigned y,unsigned rows){
    /* SAT motion and pattern ink do not change which patterns the planes
     * reference. Rebuild that dependency set only after map/scroll changes. */
    if(!v->visible_ready || v->visible_plane_dirty || v->visible_y!=y || v->visible_rows!=rows){
    /* Fine scrolling often exposes the same map cells. First compare their
     * exact union; their pattern dependencies only change when that union,
     * a referenced map word or the plane base changes. Pixel composition
     * still uses every original scroll value and every original packet. */
    static uint32_t cells[2][64];memset(cells,0,sizeof(cells));
    for(unsigned plane=0;plane<2;plane++){
        for(unsigned line=y;line<y+rows;line++){
            unsigned row=((line+v->vertical[plane][line])&511)>>3;
            int scroll=v->line_scroll?v->horizontal[plane][line]:(int16_t)v->scroll_x[plane];
            unsigned native=(32-scroll)&255,first=native>>3,count=24+((native&7)!=0);
            uint32_t columns=(1u<<count)-1;
            if(first)columns=(columns<<first)|(columns>>(32-first));
            cells[plane][row]|=columns;
        }
    }
    if(!v->visible_ready || v->visible_maps_dirty ||
       memcmp(cells,v->visible_map_cells,sizeof(cells))){
        resource_vdp_dependency_builds++;
        memset(v->visible_plane_tiles,0,sizeof(v->visible_plane_tiles));
        memset(v->visible_map_tiles,0,sizeof(v->visible_map_tiles));
        for(unsigned plane=0;plane<2;plane++)for(unsigned row=0;row<64;row++){
            unsigned base=plane?v->plane_b:0xe000;
            uint32_t pending=cells[plane][row];
            while(pending){
                unsigned col=__builtin_ctz(pending);pending&=pending-1;
                unsigned address=base+(row*32+col)*2;
                mark_visible_tile(v->visible_map_tiles,address>>5);
                mark_visible_tile(v->visible_plane_tiles,address>>5);
                mark_visible_tile(v->visible_plane_tiles,be16(v->vram+address)&2047);
            }
        }
        memcpy(v->visible_map_cells,cells,sizeof(cells));
    }else resource_vdp_dependency_reuses++;
    }
    memcpy(v->visible_tiles,v->visible_plane_tiles,sizeof(v->visible_tiles));
    for(unsigned i=0;i<v->sprite_count;i++){
        const resource_background_sprite *s=v->sprites+i;
        if(!visible_sprite(s,y,rows))continue;
        unsigned tile=s->attribute&2047,n=((s->size>>10&3)+1)*((s->size>>8&3)+1);
        for(unsigned j=0;j<n;j++)mark_visible_tile(v->visible_tiles,tile+j);
    }
    v->visible_y=y;v->visible_rows=rows;v->visible_ready=true;
    v->visible_dirty=v->visible_plane_dirty=v->visible_maps_dirty=false;
}
static void decode(resource_vdp_view *v,const uint8_t *p,unsigned bytes){
    v->plane_b=le16(p);v->sprite_count=p[2];
    v->line_scroll=(be16(p+12)&3)==3 || be16(p+14)==9;
    for(unsigned i=0;i<2;i++){v->scroll_x[i]=be16(p+4+i*2);v->scroll_y[i]=be16(p+8+i*2);}
    for(unsigned i=0;i<v->sprite_count;i++){
        const uint8_t *sat=p+16+i*8;resource_background_sprite *s=&v->sprites[i];
        s->x=(be16(sat+6)&511)-128;s->y=(be16(sat)&511)-128;
        s->size=be16(sat+2);s->attribute=be16(sat+4);
    }
    unsigned y=0;
    for(unsigned at=16+v->sprite_count*8;at<bytes;at+=10){
        unsigned end=y+p[at];
        for(;y<end;y++){
            v->visible[y]=p[at+1];
            v->vertical[0][y]=(int16_t)le16(p+at+2);v->vertical[1][y]=(int16_t)le16(p+at+4);
            v->horizontal[0][y]=(int16_t)le16(p+at+6);v->horizontal[1][y]=(int16_t)le16(p+at+8);
        }
    }
}
bool resource_vdp_history_begin(resource_vdp_encoder *e,resource_vdp_view *v,const resource_opening_state *s){
    if(!e || !v || !s)return false;
    unsigned bytes=encode(s,e->metadata);
    if(!bytes || !valid_meta(e->metadata,bytes))return false;
    e->bytes=bytes;e->publication=s->portrait_publication;
    v->visible_ready=false;v->visible_dirty=v->visible_plane_dirty=v->visible_maps_dirty=true;
    memcpy(v->vram,s->front_vram,sizeof(v->vram));decode(v,e->metadata,bytes);return true;
}
bool resource_vdp_history_capture(resource_vdp_encoder *e,const resource_opening_state *s,resource_vdp_packet **out){
    if(!e || !s || !out || !e->bytes)return false;
    *out=NULL;
    /* Losing a publication would lose its tile writes; fail instead of hiding
     * the gap behind a later scene or a fabricated full picture. */
    if(s->portrait_publication<e->publication || s->portrait_publication-e->publication>1)return false;
    uint8_t *meta=e->scratch;unsigned bytes=encode(s,meta),count=0;
    if(!bytes || !valid_meta(meta,bytes))return false;
    bool published=s->portrait_publication!=e->publication;
    if(published)for(unsigned i=0;i<64;i++)count+=__builtin_popcount(s->display_changed_tiles[i]);
    bool same=bytes==e->bytes && !memcmp(meta,e->metadata,bytes);
    if(count || !same){
        unsigned size=sizeof(resource_vdp_packet)+count*sizeof(resource_background_patch)+(same?0:bytes);
        resource_vdp_packet *p=malloc(size);if(!p)return false;
        p->bytes=size;p->tiles=count;p->metadata=same?0:bytes;
        unsigned n=0;
        if(published)for(unsigned w=0;w<64;w++){
            uint32_t bits=s->display_changed_tiles[w];
            while(bits){
                unsigned tile=w*32+__builtin_ctz(bits);bits&=bits-1;
                p->patches[n].tile=tile;memcpy(p->patches[n].pixels,s->front_vram+tile*32,32);n++;
            }
        }
        if(!same){memcpy(p->patches+count,meta,bytes);memcpy(e->metadata,meta,bytes);e->bytes=bytes;}
        *out=p;
    }
    e->publication=s->portrait_publication;return true;
}
bool resource_vdp_history_bind(resource_vdp_view *v){
    static const uint16_t dummy[64]={0};
    if(v)v->visible_ready=false;
    return v && resource_background_bind_vram(v->vram,dummy,32,64,0xe000,v->plane_b,0,0,256,224);
}
bool resource_vdp_history_apply(resource_vdp_view *v,const resource_vdp_packet *p){
    if(!v)return false;
    if(!p)return true;
    if(p->tiles>2048 || p->metadata>RESOURCE_VDP_META_MAX ||
       p->bytes!=sizeof(*p)+p->tiles*sizeof(*p->patches)+p->metadata)return false;
    const uint8_t *meta=(const uint8_t*)(p->patches+p->tiles);
    if(p->metadata && !valid_meta(meta,p->metadata))return false;
    if(!resource_background_patch_vram(v->vram,p->patches,p->tiles))return false;
    if(v->visible_ready){
        for(unsigned i=0;i<p->tiles;i++){
            unsigned tile=p->patches[i].tile;
            uint32_t mask=1u<<(tile&31);
            if(v->visible_tiles[tile>>5]&mask)v->visible_dirty=true;
            if(v->visible_map_tiles[tile>>5]&mask)v->visible_plane_dirty=v->visible_maps_dirty=true;
        }
        if(p->metadata && !v->visible_plane_dirty){
            unsigned changed=visible_metadata_changed(v,meta,p->metadata);
            v->visible_dirty|=(changed&1)!=0;v->visible_plane_dirty|=(changed&2)!=0;
        }
    }
    if(p->metadata){
        if(v->plane_b!=le16(meta))v->visible_maps_dirty=true;
        decode(v,meta,p->metadata);
    }
    return true;
}
bool resource_vdp_history_changed(const resource_vdp_view *v,unsigned y,unsigned rows){
    return !v || !v->visible_ready || v->visible_dirty || v->visible_y!=y || v->visible_rows!=rows;
}
bool resource_vdp_history_draw(resource_vdp_view *v,uint8_t *out,unsigned y,unsigned rows,
    const uint8_t *previous,resource_background_picture *picture){
    if(!v || !out || !rows || rows>192 || y>32)return false;
    uint16_t indices[64];for(unsigned i=0;i<64;i++)indices[i]=0x8000|i;
    if(!resource_background_planes(0xe000,v->plane_b) || !resource_background_view(0,y,256,192) ||
       !resource_background_palette_rgb555(indices))return false;
    for(unsigned i=0;i<2;i++)if(!resource_background_scroll(i,v->scroll_x[i],-v->scroll_y[i]))return false;
    bool drawn=resource_background_raster(v->vertical,v->visible) &&
        resource_background_line_scroll(v->line_scroll?v->horizontal:NULL) &&
        resource_background_sprites(v->sprites,v->sprite_count) &&
        resource_background_draw_native_packed_retained(out,rows,previous,picture);
    if(drawn)remember_visible_tiles(v,y,rows);
    return drawn;
}
