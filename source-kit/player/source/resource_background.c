#include "resource_background.h"
#include <string.h>
#include <stdlib.h>
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
#include <nds.h>
uint32_t resource_background_profile[6];
#endif
#ifdef __NDS__
#define BG_FAST __attribute__((noinline))
#define BG_DATA
#define BG_HOT_DATA __attribute__((section(".dtcm")))
#else
#define BG_FAST
#define BG_DATA
#define BG_HOT_DATA
#endif
typedef uint32_t alias_word __attribute__((may_alias));
typedef uint16_t alias_half __attribute__((may_alias));
#ifdef __NDS__
extern void resource_background_plane_mix_arm(const uint8_t *,const uint8_t *,uint8_t *,unsigned);
extern void resource_background_sprite_mix_arm(const uint8_t *,const uint8_t *,uint8_t *,unsigned,uint32_t);
#endif
static const uint8_t *bank;
static bool ui_backdrop;
static unsigned ui_blank;
void resource_background_ui_backdrop(bool enabled){ui_backdrop=enabled;}
bool resource_background_has_ui_backdrop(void){return ui_backdrop;}
void resource_background_ui_blank(unsigned palette_index){ui_blank=palette_index<64?palette_index:0;}
static size_t bytes;
static uint32_t vram_offset,sprite_offset;
static unsigned sprite_count,width,height,crop_x,crop_y,crop_w,crop_h;
static unsigned plane_base[2];
static int horizontal[2],vertical[2];
static const int16_t (*raster_vertical)[224];
static const int16_t (*raster_horizontal)[224];
static const uint8_t *raster_visible;
static int16_t raster_words_cache[2][224];
static int16_t horizontal_words_cache[2][224];
static uint8_t raster_visible_cache[224];
static uint16_t colors[64] BG_DATA;
static const resource_background_sprite *live_sprites;
static unsigned live_count;
static uint16_t *plane_pixels;
static uint16_t source_x[256] BG_DATA,source_y[192] BG_DATA;
static uint32_t sprite_owned[192][8];
static uint32_t low_visible[192][8];
/* Every native byte contains two4-bit dots. All eight combinations of
 * palette/priority fit in4KiB, so decode directly in fast data memory.
 * Zero nibbles stay transparent even when palette/priority is nonzero. */
static uint16_t dot_pairs[8][256];
static bool dot_pairs_ready;
static bool dirty,indexed_mode;
static unsigned indexed_first,indexed_last=256,indexed_rows=192;
/* Source map cells are decoded on demand. A slot may be reused only
 * after its owner stamp changes, so no evicted or partially revised pixels
 * can be mistaken for a complete source tile. All original maps stay live. */
#define MAPPED_CACHE_SLOTS 1024
static uint8_t mapped_cache[MAPPED_CACHE_SLOTS][64] __attribute__((aligned(4)));
static uint16_t mapped_slot[2][4096],mapped_owner[MAPPED_CACHE_SLOTS];
static unsigned mapped_cursor;
static uint8_t valid[2][4096];
static unsigned cached_slot(unsigned plane,unsigned cell){
    unsigned slot=mapped_slot[plane][cell];
    return slot && mapped_owner[slot-1]==plane*4096+cell+1?slot:0;
}
static uint8_t *cache_cell(unsigned plane,unsigned cell){
    unsigned slot=cached_slot(plane,cell);
    if(!slot){
        slot=mapped_cursor+1;mapped_cursor=(mapped_cursor+1)&(MAPPED_CACHE_SLOTS-1);
        mapped_slot[plane][cell]=slot;mapped_owner[slot-1]=plane*4096+cell+1;
        valid[plane][cell]=0;
    }
    return mapped_cache[slot-1];
}
/* A pattern can change without its map cell changing. Remember its revision
 * per decoded cell instead of rescanning both complete maps on every DMA. */
static uint8_t pattern_revision[2048];
static uint8_t pattern_last_rows[2048],pending_pattern_rows[2048];
static uint32_t plane_patterns[2][64];
static bool plane_high[2],plane_nonzero[2];
static bool plane_pattern_updates[2];
static uint64_t pending_map_rows[2];
static uint8_t flat_rows[2][4096];
static uint8_t screen_codes[2][256*192] __attribute__((aligned(4)));
/* Keep each original plane colour and its priority in the same byte until
 * the final output. A separate full-screen priority page duplicated every
 * composition write and every sprite/plane read. Bits0..5 are colour;
 * bit6 is the original VDP priority, never part of the output palette. */
static uint8_t composite_codes[256*192] __attribute__((aligned(4)));
static bool screen_dirty[2],reuse_ready[2];
static int16_t reuse_horizontal_rows[2][192];
static uint16_t reuse_source_rows[2][192];
static int16_t reuse_rows[2][512];
static uint8_t previous_codes[256*192] __attribute__((aligned(4)));
static uint8_t indexed_sprites[256*192] __attribute__((aligned(4)));
static uint16_t indexed_sprite_bounds[192][2];
static resource_background_sprite sprite_state_cache[80];
static unsigned sprite_state_count;
static bool indexed_sprites_ready,sprite_state_live;
static bool indexed_damage_full=true,indexed_damage_rows[192],merge_rows[192],merge_visible[192];
static uint16_t indexed_damage_left[192],indexed_damage_right[192];
static unsigned edge(int coordinate,unsigned origin,unsigned extent,unsigned screen);
static void invalidate_indexed_sprites(void){indexed_sprites_ready=false;indexed_damage_full=true;}
static void damage_indexed_sprite(const resource_background_sprite *sprite){
    if(!indexed_mode){invalidate_indexed_sprites();return;}
    if(indexed_damage_full){indexed_sprites_ready=false;return;}
    unsigned w=((sprite->size>>10&3)+1)*8,h=((sprite->size>>8&3)+1)*8;
    unsigned left=edge(sprite->x,crop_x,crop_w,256),right=edge(sprite->x+(int)w,crop_x,crop_w,256);
    unsigned first=edge(sprite->y,crop_y,crop_h,192),last=edge(sprite->y+(int)h,crop_y,crop_h,192);
    if(left<indexed_first)left=indexed_first;
    if(right>indexed_last)right=indexed_last;
    if(last>indexed_rows)last=indexed_rows;
    if(left>=right || first>=last)return;
    /* Word boundaries match the four-dot final compositor. Both old and
     * new SAT extents are included, so moving/removing an owner reveals
     * all lower-priority sprites and planes exactly. */
    left&=~3u;right=(right+3)&~3u;
    indexed_sprites_ready=false;
    for(unsigned y=first;y<last;y++){
        if(!indexed_damage_rows[y]){indexed_damage_left[y]=left;indexed_damage_right[y]=right;}
        else{
            if(left<indexed_damage_left[y])indexed_damage_left[y]=left;
            if(right>indexed_damage_right[y])indexed_damage_right[y]=right;
        }
        indexed_damage_rows[y]=true;
    }
}
static uint32_t *color_pairs;
static bool pairs_dirty;
static bool low_planes,base_ready,empty_planes[2];
static uint16_t *base_pixels,(*solid_rows)[256];
static void *rgb_storage;
typedef struct {
    uint8_t pixels[256*192];
    uint16_t rgb[256*192];
    uint32_t palette_generation,draw_generation;
} indexed_rgb_buffer;
static indexed_rgb_buffer *indexed_rgb;
static uint32_t indexed_draw_generation;
/* Never reset on bind/free: a caller can still retain an older output. */
static uint64_t rgb_output_revision;
typedef struct {uint8_t pixels[256*192];uint32_t generation;} opaque_index_buffer;
static opaque_index_buffer *opaque_index;
static uint64_t solid_ready;
/* Original sprite shapes contain at most4x4 native tiles. Decode a shape
 * once per VRAM change, then reuse its opaque rows at moving SAT positions. */
typedef struct {
    uint16_t attribute,size;
    uint32_t palette;
    uint8_t valid,w,h,opaque[32];
    uint8_t codes[32*32] __attribute__((aligned(4)));

} sprite_shape;
static sprite_shape sprite_shapes[64];
static uint16_t (*sprite_rgb)[32*32];
static uint8_t sprite_lookup[1024] BG_DATA;
static unsigned sprite_next;
static uint32_t palette_generation;
static uint16_t le16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t le32(const uint8_t *p){return le16(p)|(uint32_t)le16(p+2)<<16;}
static uint16_t be16(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static bool range(uint32_t at,size_t n){return at<=bytes && n<=bytes-at;}
static void init_dot_pairs(void){
    if(dot_pairs_ready)return;
    for(unsigned group=0;group<8;group++)for(unsigned value=0;value<256;value++){
        unsigned first=value>>4,second=value&15,bias=group*16;
        dot_pairs[group][value]=(first?first|bias:0)|((second?second|bias:0)<<8);
    }
    dot_pairs_ready=true;
}
static uint16_t color(unsigned value){
    unsigned r=value&15,g=value>>4&15,b=value>>8&15;
    r=r*2|(r>>3);g=g*4|(g>>2);b=b*2|(b>>3);
    return 0x8000|((r*255/31)>>3)|((g*255/63)>>3)<<5|((b*255/31)>>3)<<10;
}
bool resource_background_view(unsigned x,unsigned y,unsigned w,unsigned h){
    if(!bank || !w || !h || x+w>(width==32?256u:320u) || y+h>224)return false;
    if(x==crop_x && y==crop_y && w==crop_w && h==crop_h)return true;
    crop_x=x;crop_y=y;crop_w=w;crop_h=h;
    invalidate_indexed_sprites();
    reuse_ready[0]=reuse_ready[1]=false;
    for(unsigned i=0;i<256;i++)source_x[i]=x+i*w/256;
    for(unsigned i=0;i<192;i++)source_y[i]=y+i*h/192;
    screen_dirty[0]=screen_dirty[1]=dirty=true;base_ready=false;
    return true;
}
const uint16_t *resource_background_palette_colors(void){return bank?colors:NULL;}
bool resource_background_palette_rgb555(const uint16_t words[64]){
    if(!bank || !words)return false;
    bool changed=false;
    for(unsigned i=0;i<64;i++){
        uint16_t value=words[i]|0x8000;
        if(ui_backdrop && (!i || i==ui_blank) && !(value&32767))value&=32767;
        if(colors[i]!=value){colors[i]=value;changed=true;}
    }
    if(changed){pairs_dirty=dirty=true;solid_ready=0;palette_generation++;}
    return true;
}
bool resource_background_bind(const void *data,size_t size){
    init_dot_pairs();
    if(!data){free(rgb_storage);rgb_storage=NULL;sprite_rgb=NULL;plane_pixels=base_pixels=NULL;solid_rows=NULL;color_pairs=NULL;free(indexed_rgb);indexed_rgb=NULL;free(opaque_index);opaque_index=NULL;}
    bank=NULL;ui_blank=0;live_sprites=NULL;live_count=0;raster_vertical=NULL;raster_horizontal=NULL;raster_visible=NULL;if(!data || size<40)return false;
    const uint8_t *p=data;
    unsigned version=le16(p+4);
    if(memcmp(p,"NSBG",4) || (version!=1 && version!=2) || (version==2 && size<44))return false;
    bytes=size;sprite_count=le16(p+6);vram_offset=le32(p+8);
    unsigned palette_offset=le32(p+12);sprite_offset=le32(p+16);
    width=le16(p+20);height=le16(p+22);crop_x=le16(p+24);crop_y=le16(p+26);
    crop_w=le16(p+28);crop_h=le16(p+30);
    plane_base[0]=version==1?0xe000:le16(p+40);plane_base[1]=version==1?0xc000:le16(p+42);
    if(sprite_count>64 || (width!=32 && width!=64) || height!=64 || !crop_w || !crop_h
       || crop_x+crop_w>(width==32?256:320) || crop_y+crop_h>224 || !range(vram_offset,65536)
       || !range(palette_offset,128) || !range(sprite_offset,sprite_count*8))return false;
    if(plane_base[0]+width*height*2>65536 || plane_base[1]+width*height*2>65536)return false;
    horizontal[0]=(int16_t)le16(p+32);horizontal[1]=(int16_t)le16(p+34);
    vertical[0]=(int16_t)le16(p+36);vertical[1]=(int16_t)le16(p+38);
    for(unsigned i=0;i<64;i++)colors[i]=color(be16(p+palette_offset+i*2));
    if(ui_backdrop && !(colors[0]&32767))colors[0]&=32767;
    for(unsigned i=0;i<sprite_count;i++){
        const uint8_t *s=p+sprite_offset+i*8;unsigned size_word=le16(s+4),tile=le16(s+6)&2047;
        unsigned tiles=((size_word>>10&3)+1)*((size_word>>8&3)+1);
        if(tile+tiles>2048)return false;
    }
    for(unsigned x=0;x<256;x++)source_x[x]=crop_x+x*crop_w/256;
    for(unsigned y=0;y<192;y++)source_y[y]=crop_y+y*crop_h/192;
    bank=p;pairs_dirty=true;palette_generation++;resource_background_invalidate();return true;
}
bool resource_background_bind_vram(const uint8_t v[65536],const uint16_t palette[64],
    unsigned w,unsigned h,unsigned a,unsigned b,unsigned x,unsigned y,unsigned cw,unsigned ch){
    init_dot_pairs();
    if(!v || !palette || (w!=32 && w!=64) || (h!=32 && h!=64) || !cw || !ch || x+cw>(w==32?256u:320u)
       || y+ch>224 || a+w*h*2>65536 || b+w*h*2>65536)return false;
    bank=v;ui_blank=0;bytes=65536;vram_offset=sprite_offset=sprite_count=0;live_sprites=NULL;live_count=0;
    width=w;height=h;plane_base[0]=a;plane_base[1]=b;crop_x=x;crop_y=y;crop_w=cw;crop_h=ch;
    horizontal[0]=horizontal[1]=vertical[0]=vertical[1]=0;raster_vertical=NULL;raster_horizontal=NULL;raster_visible=NULL;
    for(unsigned i=0;i<64;i++)colors[i]=color(palette[i]);
    if(ui_backdrop && !(colors[0]&32767))colors[0]&=32767;
    for(unsigned i=0;i<256;i++)source_x[i]=x+i*cw/256;
    for(unsigned i=0;i<192;i++)source_y[i]=y+i*ch/192;
    pairs_dirty=true;palette_generation++;resource_background_invalidate();return true;
}
bool resource_background_raster(const int16_t words[2][224],const uint8_t visible[224]){
    if(!bank || (!words)!=(!visible))return false;
    if(!words && !raster_vertical)return true;
    if(words && raster_vertical && !memcmp(words,raster_words_cache,sizeof(raster_words_cache)) &&
       !memcmp(visible,raster_visible_cache,sizeof(raster_visible_cache)))return true;
    if(words){memcpy(raster_words_cache,words,sizeof(raster_words_cache));memcpy(raster_visible_cache,visible,sizeof(raster_visible_cache));}
    raster_vertical=words?raster_words_cache:NULL;raster_visible=visible?raster_visible_cache:NULL;
    screen_dirty[0]=screen_dirty[1]=dirty=true;return true;
}
static unsigned source_row(unsigned plane,unsigned row){
    return (source_y[row]+(raster_vertical?raster_vertical[plane][source_y[row]]:-vertical[plane]))&(height*8-1);
}
bool resource_background_line_scroll(const int16_t words[2][224]){
    if(!bank)return false;
    if(!words && !raster_horizontal)return true;
    for(unsigned plane=0;plane<2;plane++){
        if(!words || !raster_horizontal || memcmp(words[plane],horizontal_words_cache[plane],sizeof(horizontal_words_cache[plane]))){
            screen_dirty[plane]=dirty=true;base_ready=false;
        }
    }
    if(words)memcpy(horizontal_words_cache,words,sizeof(horizontal_words_cache));
    raster_horizontal=words?horizontal_words_cache:NULL;
    return true;
}
static void map_summary(unsigned plane){
    memset(plane_patterns[plane],0,sizeof(plane_patterns[plane]));
    plane_high[plane]=plane_nonzero[plane]=false;
    if(bank)for(unsigned cell=0;cell<width*height;cell++){
        unsigned attr=be16(bank+vram_offset+plane_base[plane]+cell*2),tile=attr&2047;
        plane_patterns[plane][tile>>5]|=1u<<(tile&31);
        if(attr&0x8000)plane_high[plane]=true;
        if(tile)plane_nonzero[plane]=true;
    }
}
static void plane_summary(void){
    bool blank=bank!=NULL;
    if(bank)for(unsigned i=0;i<32;i++)if(bank[vram_offset+i])blank=false;
    low_planes=bank && !plane_high[0] && !plane_high[1];
    for(unsigned p=0;p<2;p++)empty_planes[p]=blank && !plane_nonzero[p];
}
void resource_background_invalidate(void){
    invalidate_indexed_sprites();
    reuse_ready[0]=reuse_ready[1]=false;
    memset(valid,0,sizeof(valid));screen_dirty[0]=screen_dirty[1]=dirty=true;
    memset(pattern_revision,1,sizeof(pattern_revision));
    memset(pending_pattern_rows,0,sizeof(pending_pattern_rows));
    plane_pattern_updates[0]=plane_pattern_updates[1]=false;
    pending_map_rows[0]=pending_map_rows[1]=0;
    for(unsigned i=0;i<64;i++)sprite_shapes[i].valid=0;
    memset(sprite_lookup,0,sizeof(sprite_lookup));
    sprite_next=0;
    base_ready=false;
    map_summary(0);map_summary(1);plane_summary();
}
bool resource_background_map_changed(unsigned plane,unsigned x,unsigned y,unsigned w,unsigned h){
    if(!bank || plane>1 || x>width || w>width-x || y>height || h>height-y)return false;
    for(unsigned row=y;row<y+h;row++)memset(valid[plane]+row*width+x,0,w);
    map_summary(plane);plane_summary();
    reuse_ready[plane]=false;screen_dirty[plane]=dirty=true;return true;
}
static bool changed_pattern(const uint32_t changed[64],unsigned tile){
    return (changed[tile>>5]&(1u<<(tile&31)))!=0;
}
static void changed_vram(const uint32_t changed[64]){
    for(unsigned word=0;word<64;word++){
        uint32_t pending=changed[word];
        while(pending){
            unsigned bit=__builtin_ctz(pending),tile=word*32+bit;pending&=pending-1;
            if(!++pattern_revision[tile]){
                /* Zero means undecoded. On wrap, no old stamp may alias the
                 * new revision, including cached cells outside the view. */
                memset(valid,0,sizeof(valid));pattern_revision[tile]=1;
            }
        }
    }
    for(unsigned plane=0;plane<2;plane++){
        bool map_changed=false,affected=false;
        unsigned map_edits=0;
        unsigned first=plane_base[plane]/32,last=(plane_base[plane]+width*height*2-1)/32;
        for(unsigned tile=first;tile<=last;tile++)if(changed_pattern(changed,tile)){
            map_changed=true;
            /* Each changed four-byte map word owns two cells. Invalidate
             * those cells and their eight source rows, retaining the other
             * decoded rows while a small animated map region changes. */
            for(unsigned row=0;row<8;row++)if(pattern_last_rows[tile]&(1u<<row)){
                unsigned begin=tile*32+row*4,end=begin+4;
                unsigned map_begin=plane_base[plane],map_end=map_begin+width*height*2;
                if(begin<map_begin)begin=map_begin;
                if(end>map_end)end=map_end;
                if(begin>=end)continue;
                for(unsigned cell=(begin-map_begin)/2;cell<=(end-1-map_begin)/2;cell++){
                    valid[plane][cell]=0;
                    pending_map_rows[plane]|=UINT64_C(1)<<(cell/width);
                    /* The summaries are only conservative dependency/fast-
                     * path guards, never output colours. Adding new owners
                     * while retaining old ones cannot miss a changed dot.
                     * Large map updates and rebinds rebuild exact summaries. */
                    unsigned attr=be16(bank+vram_offset+plane_base[plane]+cell*2),pattern=attr&2047;
                    plane_patterns[plane][pattern>>5]|=1u<<(pattern&31);
                    if(attr&0x8000)plane_high[plane]=true;
                    if(pattern)plane_nonzero[plane]=true;
                    map_edits++;
                }
            }
        }
        if(map_changed){
            if(map_edits>64)map_summary(plane);
            affected=true;
        }
        for(unsigned word=0;word<64;word++)if(changed[word]&plane_patterns[plane][word])affected=true;
        if(affected){plane_pattern_updates[plane]=true;screen_dirty[plane]=dirty=true;base_ready=false;}
    }
    plane_summary();
    for(unsigned i=0;i<64;i++){
        sprite_shape *shape=&sprite_shapes[i];if(!shape->valid)continue;
        unsigned tile=shape->attribute&2047,n=shape->w*shape->h/64;
        for(unsigned j=0;j<n;j++)if(changed_pattern(changed,tile+j)){shape->valid=0;break;}
    }
    /* A displayed SAT shape may already have left the64-entry decode cache.
     * Check the actual SAT too, so its composed picture cannot stay stale. */
    if(live_sprites)for(unsigned i=0;i<live_count;i++){
        unsigned tile=live_sprites[i].attribute&2047,size=live_sprites[i].size;
        unsigned n=((size>>10&3)+1)*((size>>8&3)+1);
        for(unsigned j=0;j<n;j++)if(changed_pattern(changed,tile+j)){damage_indexed_sprite(&live_sprites[i]);break;}
    }
}
static unsigned copy_pattern(uint8_t *to,const uint8_t *from){
    unsigned rows=0;
    if(!(((uintptr_t)to|(uintptr_t)from)&3)){
        alias_word *dst=(alias_word*)to;const alias_word *src=(const alias_word*)from;
        for(unsigned y=0;y<8;y++)if(dst[y]!=src[y]){dst[y]=src[y];rows|=1u<<y;}
    }else for(unsigned y=0;y<8;y++)if(memcmp(to+y*4,from+y*4,4)){
        memcpy(to+y*4,from+y*4,4);rows|=1u<<y;
    }
    return rows;
}
__attribute__((noinline,noclone)) bool resource_background_publish_vram(uint8_t destination[65536],
    const uint8_t source[65536],const uint32_t written_tiles[64],uint32_t observed[64],bool deferred){
    if(!destination || !source || (!deferred && (bank!=destination || vram_offset || bytes!=65536)))return false;
    uint32_t local[64],*changed=observed?observed:local;unsigned count=0;
    memset(changed,0,64*sizeof(*changed));
    for(unsigned word=0;word<64;word++){
        uint32_t pending=written_tiles?written_tiles[word]:~0u;
        while(pending){
            unsigned bit=__builtin_ctz(pending),tile=word*32+bit;pending&=pending-1;
            unsigned rows=copy_pattern(destination+tile*32,source+tile*32);
            if(!rows)continue;
            if(!deferred){pattern_last_rows[tile]=rows;pending_pattern_rows[tile]|=rows;}
            changed[word]|=1u<<bit;count++;
        }
    }
    if(count && !deferred)changed_vram(changed);
    return true;
}
bool resource_background_update_vram_tiles(uint8_t destination[65536],const uint8_t source[65536],const uint32_t written_tiles[64]){
    return resource_background_publish_vram(destination,source,written_tiles,NULL,false);
}
bool resource_background_patch_vram(uint8_t destination[65536],const resource_background_patch *patches,unsigned count){
    if(bank!=destination || vram_offset || bytes!=65536 || count>2048 || (count && !patches))return false;
    /* Reject malformed packets before touching either the bank or its caches. */
    for(unsigned i=0;i<count;i++)if(patches[i].tile>=2048 || (i && patches[i-1].tile>=patches[i].tile))return false;
    uint32_t changed[64]={0};unsigned edits=0;
    for(unsigned i=0;i<count;i++){
        unsigned tile=patches[i].tile,rows=copy_pattern(destination+tile*32,patches[i].pixels);
        if(!rows)continue;
        pattern_last_rows[tile]=rows;pending_pattern_rows[tile]|=rows;
        changed[tile>>5]|=1u<<(tile&31);edits++;
    }
    if(edits)changed_vram(changed);
    return true;
}

bool resource_background_update_vram(uint8_t destination[65536],const uint8_t source[65536]){
    return resource_background_update_vram_tiles(destination,source,NULL);
}
bool resource_background_scroll(unsigned plane,int h,int v){
    if(!bank || plane>1)return false;
    if(horizontal[plane]!=(int16_t)h || vertical[plane]!=(int16_t)v)screen_dirty[plane]=dirty=true;
    horizontal[plane]=(int16_t)h;vertical[plane]=(int16_t)v;return true;
}
bool resource_background_palette(unsigned first,const uint16_t *words,unsigned count){
    if(!bank || !words || first>64 || count>64-first)return false;
    bool changed=false;
    for(unsigned i=0;i<count;i++){
        uint16_t value=color(words[i]);if(ui_backdrop && (first+i==0 || first+i==ui_blank) && !(value&32767))value&=32767;
        if(colors[first+i]!=value){pairs_dirty=dirty=true;changed=true;}
        colors[first+i]=value;
    }
    if(changed)palette_generation++;
    return true;
}
bool resource_background_sprites(const resource_background_sprite *sprites,unsigned count){
    if(!bank || count>80 || (!sprites && count))return false;
    for(unsigned i=0;i<count;i++){
        unsigned n=((sprites[i].size>>10&3)+1)*((sprites[i].size>>8&3)+1);
        if((sprites[i].attribute&2047)+n>2048)return false;
    }
    if((sprites!=NULL)!=sprite_state_live || count!=sprite_state_count ||
       (sprites && memcmp(sprite_state_cache,sprites,count*sizeof(*sprites)))){
        if(!sprites || !sprite_state_live)invalidate_indexed_sprites();
        else{
            unsigned n=count>sprite_state_count?count:sprite_state_count;
            for(unsigned i=0;i<n;i++)if(i>=count || i>=sprite_state_count ||
                memcmp(sprite_state_cache+i,sprites+i,sizeof(*sprites))){
                if(i<sprite_state_count)damage_indexed_sprite(sprite_state_cache+i);
                if(i<count)damage_indexed_sprite(sprites+i);
            }
        }
        if(sprites)memcpy(sprite_state_cache,sprites,count*sizeof(*sprites));
        sprite_state_count=count;sprite_state_live=sprites!=NULL;
    }
    live_sprites=sprites;live_count=count;return true;
}
bool resource_background_geometry(unsigned w,unsigned h){
    if(!bank || (w!=32 && w!=64) || (h!=32 && h!=64) ||
       plane_base[0]+w*h*2>65536 || plane_base[1]+w*h*2>65536)return false;
    if(width==w && height==h)return true;
    width=w;height=h;resource_background_invalidate();return true;
}
bool resource_background_planes(unsigned a,unsigned b){
    if(!bank || a>65536-width*height*2 || b>65536-width*height*2)return false;
    if(plane_base[0]==a && plane_base[1]==b)return true;
    plane_base[0]=a;plane_base[1]=b;resource_background_invalidate();return true;
}
static BG_FAST sprite_shape *sprite_decoded(unsigned attribute,unsigned size){
    unsigned shape=size&0xf00; /* Width bits are10/11; height bits8/9. */
    unsigned hash=(attribute*0x9e3779b1u+(shape>>8)*0x85ebca6bu)>>22;
    unsigned cached=sprite_lookup[hash];
    sprite_shape *s=cached?sprite_shapes+cached-1:NULL;
    if(s && (!s->valid || s->attribute!=attribute || s->size!=shape))s=NULL;
    if(!s){
        unsigned slot=sprite_next++&63;s=sprite_shapes+slot;sprite_lookup[hash]=slot+1;
        s->valid=1;s->attribute=attribute;s->size=shape;s->palette=palette_generation-1;
        s->w=((size>>10&3)+1)*8;s->h=((size>>8&3)+1)*8;
        memset(s->opaque,1,s->h);
        const uint16_t *pairs=dot_pairs[attribute>>13];
        /* Native SAT shapes store tiles by column. Expand eight dots at
         * once, then apply the whole-shape flips to their destination.
         * Keep transparent dots zero for original SAT ownership tests. */
        for(unsigned tx=0;tx<s->w;tx+=8)for(unsigned ty=0;ty<s->h;ty+=8){
            unsigned tile=(attribute&2047)+(tx/8)*(s->h/8)+ty/8;
            const uint8_t *pixels=bank+vram_offset+tile*32;
            unsigned x=attribute&0x800?s->w-tx-8:tx;
            for(unsigned row=0;row<8;row++){
                const uint8_t *p=pixels+row*4;
                uint32_t a=pairs[p[0]]|((uint32_t)pairs[p[1]]<<16);
                uint32_t b=pairs[p[2]]|((uint32_t)pairs[p[3]]<<16);
                if(attribute&0x800){uint32_t t=a;a=__builtin_bswap32(b);b=__builtin_bswap32(t);}
                uint32_t nz_a=(a+0x7f7f7f7fu)&0x80808080u,nz_b=(b+0x7f7f7f7fu)&0x80808080u;
                unsigned y=attribute&0x1000?s->h-ty-row-1:ty+row;
                if((nz_a&nz_b)!=0x80808080u)s->opaque[y]=0;
                *(alias_word*)(s->codes+y*32+x)=a;*(alias_word*)(s->codes+y*32+x+4)=b;
            }
        }
    }
    if(!indexed_mode && s->palette!=palette_generation){
        for(unsigned y=0;y<s->h;y++)for(unsigned x=0;x<s->w;x++)sprite_rgb[s-sprite_shapes][y*32+x]=colors[s->codes[y*32+x]&63];
        s->palette=palette_generation;
    }
    return s;
}
static BG_FAST const uint8_t *decode_mapped_tile(unsigned plane,unsigned cell,unsigned attr){
    uint8_t *out=cache_cell(plane,cell);
    unsigned rows=valid[plane][cell] && valid[plane][cell]+1==pattern_revision[attr&2047]?pattern_last_rows[attr&2047]:255;
    unsigned flat=flat_rows[plane][cell];
    const uint16_t *pairs=dot_pairs[attr>>13];
    const uint8_t *pixels=bank+vram_offset+(attr&2047)*32;
    for(unsigned y=0;y<8;y++){
        if(!(rows&(1u<<(attr&0x1000?7-y:y))))continue;
        const uint8_t *p=pixels+(attr&0x1000?7-y:y)*4;
        uint32_t a=pairs[p[0]]|((uint32_t)pairs[p[1]]<<16);
        uint32_t b=pairs[p[2]]|((uint32_t)pairs[p[3]]<<16);
        if(attr&0x800){uint32_t t=a;a=__builtin_bswap32(b);b=__builtin_bswap32(t);}
        *(alias_word*)(out+y*8)=a;*(alias_word*)(out+y*8+4)=b;
        flat&=~(1u<<y);if(a==b && a==(a&255)*0x01010101u)flat|=1u<<y;
    }
    flat_rows[plane][cell]=flat;
    valid[plane][cell]=pattern_revision[attr&2047];return out;
}
static inline __attribute__((always_inline)) const uint8_t *mapped_tile(unsigned plane,unsigned cell){
    unsigned attr=be16(bank+vram_offset+plane_base[plane]+cell*2);
    unsigned slot=cached_slot(plane,cell);
    if(slot && valid[plane][cell]==pattern_revision[attr&2047])return mapped_cache[slot-1];
    return decode_mapped_tile(plane,cell,attr);
}
static inline __attribute__((always_inline)) void copy_codes(uint8_t *out,const uint8_t *in,unsigned n){
    if(n==8){
        if(!((uintptr_t)in&3)){
            uint32_t a=*(const alias_word*)in,b=*(const alias_word*)(in+4);
            switch((uintptr_t)out&3){
            case 0:*(alias_word*)out=a;*(alias_word*)(out+4)=b;break;
            case 1:out[0]=a;*(alias_half*)(out+1)=a>>8;*(alias_word*)(out+3)=(a>>24)|(b<<8);out[7]=b>>24;break;
            case 2:*(alias_half*)out=a;*(alias_word*)(out+2)=(a>>16)|(b<<16);*(alias_half*)(out+6)=b>>16;break;
            case 3:out[0]=a;*(alias_word*)(out+1)=(a>>8)|(b<<24);*(alias_half*)(out+5)=b>>8;out[7]=b>>24;break;
            }
        }else{
            out[0]=in[0];out[1]=in[1];out[2]=in[2];out[3]=in[3];
            out[4]=in[4];out[5]=in[5];out[6]=in[6];out[7]=in[7];
        }
    }else for(unsigned i=0;i<n;i++)out[i]=in[i];
}
static void pairs(void){
    if(!pairs_dirty)return;
    for(unsigned high=0;high<64;high++)for(unsigned low=0;low<64;low++)
        color_pairs[low+high*64]=colors[low]|(uint32_t)colors[high]<<16;
    pairs_dirty=false;solid_ready=0;
}
static void solid_span(unsigned at,unsigned length,unsigned index){
    if(!(solid_ready&(UINT64_C(1)<<index))){
        for(unsigned i=0;i<256;i++)solid_rows[index][i]=colors[index];
        solid_ready|=UINT64_C(1)<<index;
    }
    memcpy(plane_pixels+at,solid_rows[index],length*2);
}
static BG_FAST void low_draw(void){
    reuse_ready[0]=reuse_ready[1]=false;
    bool palette_changed=pairs_dirty;
    pairs();
    if(screen_dirty[1]){
        unsigned sx[256],first[64],last[64],tile_x[64],segments=0;
        for(unsigned x=0;x<256;x++){
            sx[x]=(source_x[x]-horizontal[1])&(width*8-1);
            if(!x || (sx[x]>>3)!=(sx[x-1]>>3)){
                if(segments)last[segments-1]=x;
                first[segments]=x;tile_x[segments++]=sx[x]>>3;
            }
        }
        last[segments-1]=256;
        for(unsigned y=0;y<192;y++){
            unsigned sy=source_row(1,y);
            for(unsigned s=0;s<segments;s++){
                const uint8_t *line=mapped_tile(1,(sy/8)*width+tile_x[s])+(sy&7)*8;
                unsigned at=y*256+first[s];
                if(crop_w==256)memcpy(screen_codes[1]+at,line+(sx[first[s]]&7),last[s]-first[s]);
                else for(unsigned x=first[s];x<last[s];x++,at++)screen_codes[1][at]=line[sx[x]&7];
            }
        }
        screen_dirty[1]=false;base_ready=false;
    }
    if(!base_ready || palette_changed){
        for(unsigned i=0;i<256*192;i+=2){
            uint32_t value=color_pairs[screen_codes[1][i]+(unsigned)screen_codes[1][i+1]*64];
            *(alias_word*)(base_pixels+i)=value;
        }
        base_ready=true;
    }
    memcpy(plane_pixels,base_pixels,256*192*2);
    unsigned first[64],last[64],tile_x[64],segments=0,sx[256];
    for(unsigned x=0;x<256;x++){
        sx[x]=(source_x[x]-horizontal[0])&(width*8-1);
        if(!x || (sx[x]>>3)!=(sx[x-1]>>3)){
            if(segments)last[segments-1]=x;
            first[segments]=x;tile_x[segments++]=sx[x]>>3;
        }
    }
    last[segments-1]=256;
    /* An opaque flat native tile row can cover many adjacent cells. Copy a
     * solid span once instead of reading/compositing every identical pixel. */
    for(unsigned y=0;y<192;y++){
        unsigned sy=source_row(0,y),run_start=0,run_end=0,run_color=0;
        for(unsigned s=0;s<segments;s++){
            unsigned start=first[s],end=last[s],cell=(sy/8)*width+tile_x[s];
            const uint8_t *line=mapped_tile(0,cell)+(sy&7)*8;
            if(flat_rows[0][cell]&(1u<<(sy&7))){
                unsigned index=line[0];
                if(run_end==start && run_color==index){run_end=end;continue;}
                if(run_color)solid_span(y*256+run_start,run_end-run_start,run_color);
                run_start=start;run_end=end;run_color=index;
            }else{
                if(run_color)solid_span(y*256+run_start,run_end-run_start,run_color);
                run_start=run_end=end;run_color=0;
                for(unsigned i=start;i<end;i++){
                    unsigned index=line[sx[i]&7];
                    if(index)plane_pixels[y*256+i]=colors[index];
                }
            }
        }
        if(run_color)solid_span(y*256+run_start,run_end-run_start,run_color);
    }
    screen_dirty[0]=false;
}
static BG_FAST void code_planes(void){
#ifdef __NDS__
    extern void fm_observe_stack(void);fm_observe_stack();
#endif

#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t begin=cpuGetTiming();
#endif
    /* The renderer already has exclusive ownership of the global plane/SAT
     * caches. Reuse its bounded working arrays too: deferred presentation may
     * interrupt the sound producer on the same small ARM9 user stack. */
    static bool changed_rows[192];
    static unsigned sx[256],first[64],last[64],tile_x[64];
    static uint8_t changed_map_rows[64];
    bool compose=false;memset(changed_rows,0,sizeof(changed_rows));
    unsigned end_y=indexed_mode?indexed_rows:192;
    unsigned start_x=indexed_mode?indexed_first:0,end_x=indexed_mode?indexed_last:256;
    for(int plane=1;plane>=0;plane--){
        if(!screen_dirty[plane])continue;
        compose=true;screen_dirty[plane]=false;
        if(empty_planes[plane]){
            memset(screen_codes[plane],0,sizeof(screen_codes[plane]));
            memset(changed_rows,1,sizeof(changed_rows));
            reuse_ready[plane]=false;continue;
        }
        unsigned segments=0;
        bool reuse=indexed_mode && crop_w==256 && reuse_ready[plane];
        int period=width*8;
        /* The previous source image remains in screen_codes until a row
         * is overwritten. Preserve only those rows and only the active span.
         * Consumers of later/unchanged rows read the original page directly. */
        uint32_t previous_saved[6]={0};
        unsigned previous_row=~0u;int previous_horizontal=65536;
        uint64_t checked_map_rows=0;
        for(unsigned y=0;y<end_y;y++){
            unsigned sy=source_row(plane,y);
            int current=raster_horizontal?raster_horizontal[plane][source_y[y]]:horizontal[plane];
            bool dots_changed=(pending_map_rows[plane]&(UINT64_C(1)<<(sy>>3)))!=0;
            if(reuse && plane_pattern_updates[plane]){
                unsigned row=sy>>3;
                if(!(checked_map_rows&(UINT64_C(1)<<row))){
                    unsigned mask=0;
                    for(unsigned col=0;col<width;col++){
                        unsigned attr=be16(bank+vram_offset+plane_base[plane]+(row*width+col)*2);
                        unsigned rows=pending_pattern_rows[attr&2047];
                        if(attr&0x1000){
                            rows=((rows&0x55)<<1)|((rows&0xaa)>>1);
                            rows=((rows&0x33)<<2)|((rows&0xcc)>>2);
                            rows=(rows<<4)|(rows>>4);
                        }
                        mask|=rows;
                    }
                    changed_map_rows[row]=mask;checked_map_rows|=UINT64_C(1)<<row;
                }
                dots_changed|=(changed_map_rows[row]&(1u<<(sy&7)))!=0;
            }
            if(reuse && !dots_changed && reuse_source_rows[plane][y]==sy && reuse_horizontal_rows[plane][y]==current){
                previous_row=~0u;continue;
            }
            changed_rows[y]=true;
            if(reuse){
                memcpy(previous_codes+y*256+start_x,screen_codes[plane]+y*256+start_x,end_x-start_x);
                previous_saved[y>>5]|=1u<<(y&31);
            }
            if(sy==previous_row && current==previous_horizontal){memcpy(screen_codes[plane]+y*256,screen_codes[plane]+(y-1)*256,256);continue;}
            previous_row=sy;
            if(current!=previous_horizontal){
                segments=0;
                if(crop_w==256){
                    /* Unit source steps: skip to the next exact tile edge,
                     * including unaligned first/last spans and map wrap. */
                    unsigned x=start_x;
                    while(x<end_x){
                        unsigned native=(source_x[x]-current)&(width*8-1);
                        unsigned next=x+8-(native&7);if(next>end_x)next=end_x;
                        first[segments]=x;last[segments]=next;
                        tile_x[segments++]=native>>3;x=next;
                    }
                }else{
                    for(unsigned x=start_x;x<end_x;x++){
                        sx[x]=(source_x[x]-current)&(width*8-1);
                        if(x==start_x || (sx[x]>>3)!=(sx[x-1]>>3)){
                            if(segments)last[segments-1]=x;
                            first[segments]=x;tile_x[segments++]=sx[x]>>3;
                        }
                    }
                    last[segments-1]=end_x;
                }
                previous_horizontal=current;
            }
            int old=reuse && !dots_changed?reuse_rows[plane][sy]:-1;
            int delta=old>=0?reuse_horizontal_rows[plane][old]-current:0;
            delta=((delta+period/2)&(period-1))-period/2;
            if(delta<-(int)(end_x-start_x) || delta>(int)(end_x-start_x))old=-1;
            if(old>=0){
                unsigned left=delta<0?start_x-delta:start_x,right=delta>0?end_x-delta:end_x;
                const uint8_t *before=(previous_saved[(unsigned)old>>5]&(1u<<(old&31)))?
                    previous_codes:screen_codes[plane];
                memcpy(screen_codes[plane]+y*256+left,before+old*256+left+delta,right-left);
                for(unsigned edge=0;edge<2;edge++)for(unsigned x=edge?right:start_x;x<(edge?end_x:left);x++){
                    unsigned native=(source_x[x]-current)&(width*8-1);
                    unsigned cell=(sy/8)*width+(native>>3);
                    const uint8_t *tile=mapped_tile(plane,cell);
                    screen_codes[plane][y*256+x]=tile[(sy&7)*8+(native&7)];
                }
                continue;
            }
            for(unsigned s=0;s<segments;s++){
                unsigned cell=(sy/8)*width+tile_x[s];
                const uint8_t *line=mapped_tile(plane,cell)+(sy&7)*8;
                unsigned at=y*256+first[s];
                if(crop_w==256)copy_codes(screen_codes[plane]+at,line+((source_x[first[s]]-current)&7),last[s]-first[s]);
                else for(unsigned x=first[s];x<last[s];x++,at++)screen_codes[plane][at]=line[sx[x]&7];
            }
        }
        reuse_ready[plane]=indexed_mode && crop_w==256;
        if(reuse_ready[plane]){
            memset(reuse_rows[plane],255,sizeof(reuse_rows[plane]));
            for(unsigned y=0;y<end_y;y++){
                reuse_rows[plane][source_row(plane,y)]=y;
                reuse_source_rows[plane][y]=source_row(plane,y);
                reuse_horizontal_rows[plane][y]=raster_horizontal?raster_horizontal[plane][source_y[y]]:horizontal[plane];
            }
        }
    }
    for(unsigned y=0;y<end_y;y++)merge_rows[y]|=changed_rows[y];
    memset(pending_pattern_rows,0,sizeof(pending_pattern_rows));
    plane_pattern_updates[0]=plane_pattern_updates[1]=false;
    pending_map_rows[0]=pending_map_rows[1]=0;
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t mapped_end=cpuGetTiming();resource_background_profile[4]=mapped_end-begin;
#endif
    if(compose && !indexed_mode)memset(low_visible,0,sizeof(low_visible));
    if(compose && indexed_mode && (empty_planes[0] || empty_planes[1])){
      /* A validated blank native plane contributes no priority or colour.
       * Keep the other plane intact instead of blending two full pages. */
      const uint8_t *codes=screen_codes[empty_planes[0]?1:0];
      for(unsigned row=0;row<end_y;row++){
        if(!changed_rows[row])continue;
        if(row && source_y[row]==source_y[row-1])continue;
        for(unsigned i=row*256+start_x;i<row*256+end_x;i+=4){
            uint32_t value=*(const alias_word*)(codes+i);
            *(alias_word*)(composite_codes+i)=value;
        }
      }
    }else if(compose)for(unsigned row=0;row<end_y;row++){
      if(!changed_rows[row])continue;
      if(indexed_mode && row && source_y[row]==source_y[row-1])continue;
#ifdef __NDS__
      if(indexed_mode){
        resource_background_plane_mix_arm(screen_codes[0]+row*256+start_x,
            screen_codes[1]+row*256+start_x,composite_codes+row*256+start_x,end_x-start_x);
        continue;
      }
#endif
      for(unsigned i=row*256+start_x;i<row*256+end_x;i+=4){
        /* Four independent7-bit pixels per word. Addition cannot carry
         * between bytes:127+127<256. Keep original plane/sprite priority. */
        uint32_t a=*(alias_word*)(screen_codes[0]+i),b=*(alias_word*)(screen_codes[1]+i);
        uint32_t nonzero=(a+0x7f7f7f7fu)&0x80808080u;
        uint32_t high_a=(a&0x40404040u)<<1,high_b=(b&0x40404040u)<<1;
        uint32_t choose=((nonzero&(~high_b|high_a))>>7)*255;
        uint32_t value=(a&choose)|(b&~choose);
        *(alias_word*)(composite_codes+i)=value;
        if(!indexed_mode){
            uint32_t bits=(value>>6)&0x01010101u;
            unsigned hidden=(bits&1)|((bits>>7)&2)|((bits>>14)&4)|((bits>>21)&8);
            low_visible[i/256][(i&255)>>5]|=((~hidden)&15)<<(i&31);
        }
      }
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    resource_background_profile[5]=cpuGetTiming()-mapped_end;
#endif
}
static BG_FAST void planes(void){
    if(low_planes && !raster_horizontal){low_draw();return;}
    code_planes();
    pairs();
    for(unsigned i=0;i<256*192;i+=2){
        uint32_t value=color_pairs[(composite_codes[i]&63)+(unsigned)(composite_codes[i+1]&63)*64];
        *(alias_word*)(plane_pixels+i)=value;
    }
}
static unsigned edge(int coordinate,unsigned origin,unsigned extent,unsigned screen){
    if(coordinate<=(int)origin)return 0;
    if(coordinate>=(int)(origin+extent))return screen;
    return ((coordinate-origin)*screen+extent-1)/extent;
}
BG_FAST bool resource_background_draw(uint16_t output[256*192]){
    if(!bank || !output)return false;
    if(!rgb_storage){
        /* Indexed scene playback does not need these 240KiB RGB caches. */
        rgb_storage=malloc(256*192*4+64*256*2+64*64*4+64*32*32*2);
        if(!rgb_storage)return false;
        plane_pixels=rgb_storage;base_pixels=plane_pixels+256*192;
        solid_rows=(uint16_t(*)[256])(base_pixels+256*192);color_pairs=(uint32_t*)(solid_rows+64);
        sprite_rgb=(uint16_t(*)[32*32])(color_pairs+64*64);
        base_ready=false;pairs_dirty=dirty=true;solid_ready=0;
    }
    if(indexed_mode){reuse_ready[0]=reuse_ready[1]=false;indexed_mode=false;screen_dirty[0]=screen_dirty[1]=dirty=true;base_ready=false;}
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t profile_begin=cpuGetTiming();
#endif
    if(dirty){
        planes();
        dirty=false;
    }
    memcpy(output,plane_pixels,256*192*2);
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t profile_planes=cpuGetTiming();
#endif
    unsigned count=live_sprites?live_count:sprite_count;
    bool direct=true;unsigned sprite_level=99;
    for(unsigned i=0;i<count;i++){
        unsigned attr=live_sprites?live_sprites[i].attribute:le16(bank+sprite_offset+i*8+6);
        unsigned level=attr&0x8000?6:2;
        if(sprite_level==99)sprite_level=level;
        else if(sprite_level!=level)direct=false;
    }
    /* Equal-priority sprites can composite directly in reverse SAT order.
     * A low-priority group only needs the already cached plane mask; a high
     * group always beats both planes. This avoids a full owner-buffer pass. */
    bool plane_mask=direct && !low_planes && sprite_level==2;
    /* SAT order resolves sprite ownership before testing plane priority.
     * An earlier low-priority sprite still owns an overlapping later high-
     * priority sprite's pixels, even where a high plane hides that owner. */
    if(!direct)memset(sprite_owned,0,sizeof(sprite_owned));
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t profile_setup=cpuGetTiming();
#endif
    for(unsigned step=0;step<count;step++){
        unsigned i=direct?count-1-step:step;
        const uint8_t *s=bank+sprite_offset+i*8;
        int sx=live_sprites?live_sprites[i].x:(int16_t)le16(s);
        int sy=live_sprites?live_sprites[i].y:(int16_t)le16(s+2);
        unsigned size=live_sprites?live_sprites[i].size:le16(s+4);
        unsigned attr=live_sprites?live_sprites[i].attribute:le16(s+6);
        unsigned w=((size>>10&3)+1)*8,h=((size>>8&3)+1)*8;
        unsigned left=edge(sx,crop_x,crop_w,256),right=edge(sx+(int)w,crop_x,crop_w,256);
        unsigned top=edge(sy,crop_y,crop_h,192),bottom=edge(sy+(int)h,crop_y,crop_h,192);
        if(left>=right || top>=bottom)continue;
        sprite_shape *decoded=sprite_decoded(attr,size);
        bool high=(attr&0x8000)!=0;
        for(unsigned y=top;y<bottom;y++){
            unsigned row=source_y[y]-sy,at=y*256+left;
            const uint8_t *line=decoded->codes+row*32;
            if(!direct){
                for(unsigned x=left;x<right;){
                    unsigned end=(x&~31u)+32;if(end>right)end=right;
                    unsigned slot=x>>5;
                    uint32_t old=sprite_owned[y][slot],owned=old;
                    uint32_t mask=(~0u<<(x&31))&(~0u>>(32-(((end-1)&31)+1)));
                    uint32_t visible=high || low_planes?~0u:low_visible[y][slot];
                    if((old&mask)!=mask){
                        if(crop_w==256 && decoded->opaque[row]){
                            uint32_t available=mask&~old&visible;owned|=mask;
                            if(available==mask)memcpy(output+y*256+x,sprite_rgb[decoded-sprite_shapes]+row*32+source_x[x]-sx,(end-x)*2);
                            else while(available){
                                unsigned bit=__builtin_ctz(available),pos=slot*32+bit;
                                output[y*256+pos]=sprite_rgb[decoded-sprite_shapes][row*32+source_x[pos]-sx];available&=available-1;
                            }
                        }else for(unsigned pos=x;pos<end;pos++){
                            uint32_t bit=1u<<(pos&31);
                            if(old&bit)continue;
                            unsigned code=line[source_x[pos]-sx];
                            if(code){owned|=bit;if(visible&bit)output[y*256+pos]=colors[code&63];}
                        }
                    }
                    sprite_owned[y][slot]=owned;x=end;
                }
                continue;
            }
            if(crop_w==256 && decoded->opaque[row] && !plane_mask){
                unsigned from=source_x[left]-sx;
                memcpy(output+at,sprite_rgb[decoded-sprite_shapes]+row*32+from,(right-left)*2);
                continue;
            }
            for(unsigned x=left;x<right;x++,at++){
                unsigned code=line[source_x[x]-sx];
                if(code){
                    if(!plane_mask || !(composite_codes[at]&64))output[at]=colors[code&63];
                }
            }
        }
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t profile_sprites=cpuGetTiming();
#endif
    if(raster_visible)for(unsigned y=0;y<192;y++)if(!raster_visible[source_y[y]])
        for(unsigned x=0;x<256;x++)output[y*256+x]=colors[0];
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    resource_background_profile[0]=profile_planes-profile_begin;
    resource_background_profile[1]=profile_setup-profile_planes;
    resource_background_profile[2]=profile_sprites-profile_setup;
    resource_background_profile[3]=cpuGetTiming()-profile_sprites;
#endif
    return true;
}

static inline void copy_indexed_span(uint8_t *out,const uint8_t *in,unsigned n,uint32_t bias){
    for(unsigned x=0;x<n;x+=4)*(alias_word*)(out+x)=(*(const alias_word*)(in+x)&0x3f3f3f3fu)|bias;
}
static bool indexed_last_changed,indexed_last_rows[192];
bool resource_background_last_row_changed(unsigned y){return y>=192 || indexed_last_rows[y];}
bool resource_background_last_indices_changed(void){return indexed_last_changed;}
static __attribute__((noinline,noclone)) bool draw_indexed8(uint8_t output[256*192],unsigned first,unsigned last,unsigned rows,uint32_t bias,resource_background_picture *picture,bool packed,const uint8_t *previous){
    if(!bank || !output || first>=last || last>256 || !rows || rows>192 || ((first|last)&3))return false;
    bool retained=picture && picture->destination && picture->destination==(const uint16_t*)(packed?previous:output) &&
        picture->revision==indexed_draw_generation && indexed_mode &&
        first==indexed_first && last==indexed_last && rows==indexed_rows && !indexed_damage_full;
    memset(merge_rows,!retained,sizeof(merge_rows));
    if(first!=indexed_first || last!=indexed_last || rows!=indexed_rows){invalidate_indexed_sprites();reuse_ready[0]=reuse_ready[1]=false;indexed_first=first;indexed_last=last;indexed_rows=rows;screen_dirty[0]=screen_dirty[1]=dirty=true;}
    if(!indexed_mode){reuse_ready[0]=reuse_ready[1]=false;indexed_mode=true;screen_dirty[0]=screen_dirty[1]=dirty=true;}
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t begin=cpuGetTiming();
#endif
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    resource_background_profile[4]=resource_background_profile[5]=0;
#endif
    if(dirty){code_planes();dirty=false;}
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t drawn=cpuGetTiming();
#endif
    uint8_t *destination=output;output=indexed_sprites;
    if(!indexed_sprites_ready){
    for(unsigned y=0;y<rows;y++)if(indexed_damage_full || indexed_damage_rows[y]){
        if(indexed_damage_full){
            indexed_damage_left[y]=first;indexed_damage_right[y]=last;
            indexed_sprite_bounds[y][0]=256;indexed_sprite_bounds[y][1]=0;
        }
        unsigned left=indexed_damage_left[y],right=indexed_damage_right[y];
        memset(indexed_sprites+y*256+left,0,right-left);
        indexed_damage_rows[y]=true;
    }
    unsigned count=live_sprites?live_count:sprite_count;
    /* Reverse nontransparent overwrite is identical to forward first
     * owner, including mixed priorities. Keep the winning priority byte
     * for the later plane/SAT compositor. No source sprite is omitted. */
    const bool direct=true;
    for(unsigned step=0;step<count;step++){
        unsigned i=direct?count-1-step:step;
        const uint8_t *s=bank+sprite_offset+i*8;
        int sx=live_sprites?live_sprites[i].x:(int16_t)le16(s);
        int sy=live_sprites?live_sprites[i].y:(int16_t)le16(s+2);
        unsigned size=live_sprites?live_sprites[i].size:le16(s+4);
        unsigned attr=live_sprites?live_sprites[i].attribute:le16(s+6);
        unsigned w=((size>>10&3)+1)*8,h=((size>>8&3)+1)*8;
        unsigned left=edge(sx,crop_x,crop_w,256),right=edge(sx+(int)w,crop_x,crop_w,256);
        unsigned top=edge(sy,crop_y,crop_h,192),bottom=edge(sy+(int)h,crop_y,crop_h,192);
        if(bottom>rows)bottom=rows;
        if(left<first)left=first;
        if(right>last)right=last;
        if(left>=right || top>=bottom)continue;
        sprite_shape *decoded=sprite_decoded(attr,size);
        for(unsigned y=top;y<bottom;y++){
            if(!indexed_damage_full && !indexed_damage_rows[y])continue;
            /* A128/160-row source repeats rows in the192-row staging page.
             * Composite each source row once, then duplicate its final
             * plane+SAT result below instead of repeating all sprite work. */
            if(y && source_y[y]==source_y[y-1])continue;
            if(left<indexed_sprite_bounds[y][0])indexed_sprite_bounds[y][0]=left;
            if(right>indexed_sprite_bounds[y][1])indexed_sprite_bounds[y][1]=right;
            unsigned row_left=left,row_right=right;
            if(row_left<indexed_damage_left[y])row_left=indexed_damage_left[y];
            if(row_right>indexed_damage_right[y])row_right=indexed_damage_right[y];
            if(row_left>=row_right)continue;
            unsigned left=row_left,right=row_right;
            unsigned row=source_y[y]-sy,at=y*256+left;
            const uint8_t *line=decoded->codes+row*32;
            if(direct && crop_w==256 && decoded->opaque[row]){
                memcpy(output+at,line+source_x[left]-sx,right-left);continue;
            }
            if(crop_w==256){
                unsigned x=left;
                while(x<right && (x&3)){
                    unsigned code=line[source_x[x]-sx];
                    if(code && (direct || !output[y*256+x]))output[y*256+x]=code;
                    x++;
                }
                for(;x+4<=right;x+=4){
                    const uint8_t *p=line+source_x[x]-sx;
                    uint32_t codes;
                    if(!((uintptr_t)p&3))codes=*(const alias_word*)p;
                    else codes=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
                    uint32_t visible=(codes+0x7f7f7f7fu)&0x80808080u;
                    unsigned pos=y*256+x;
                    alias_word *dest=(alias_word*)(output+pos);
                    uint32_t prior=*dest;
                    /* A nonzero cached SAT colour is already owned by an
                     * earlier sprite, regardless of its plane priority.
                     * Resolve four owners at once; original transparent
                     * pixels are exactly zero in this intermediate page. */
                    if(!direct)visible&=~((prior+0x7f7f7f7fu)&0x80808080u);
                    uint32_t mask=(visible>>7)*255;
                    *dest=(prior&~mask)|(codes&mask);
                }
                for(;x<right;x++){
                    unsigned code=line[source_x[x]-sx];
                    if(code && (direct || !output[y*256+x]))output[y*256+x]=code;
                }
                continue;
            }
            for(unsigned x=left;x<right;x++,at++){
                unsigned code=line[source_x[x]-sx];
                if(!code)continue;
                if(direct || !output[at])output[at]=code;
            }
        }
    }
    indexed_sprites_ready=true;indexed_damage_full=false;
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t sprites_end=cpuGetTiming();
#endif
    output=destination;indexed_last_changed=false;memset(indexed_last_rows,0,sizeof(indexed_last_rows));
    /* Write each output word once. Resolve SAT ownership only inside its
     * row bounds and add the opaque palette bias in the same pass. */
    for(unsigned y=0;y<rows;y++){
        bool visible=!raster_visible || raster_visible[source_y[y]];
        merge_rows[y]|=visible!=merge_visible[y];merge_visible[y]=visible;
        if(y && source_y[y]==source_y[y-1])merge_rows[y]|=merge_rows[y-1] || indexed_damage_rows[y-1];
        if(!merge_rows[y] && !indexed_damage_rows[y]){
            if(packed && output!=previous)memcpy(output+y*(last-first),previous+y*(last-first),last-first);
            continue;
        }
        indexed_last_changed=true;indexed_last_rows[y]=true;
        unsigned stride=packed?last-first:256;
        unsigned offset=packed?first:0;
        uint8_t *rowout=output+y*stride;
        if(raster_visible && !raster_visible[source_y[y]]){memset(rowout+first-offset,bias&255,last-first);continue;}
        if(y && source_y[y]==source_y[y-1]){
            memcpy(rowout+first-offset,output+(y-1)*stride+first-offset,last-first);continue;
        }
        unsigned full_first=first,full_last=last;
        if(!merge_rows[y]){
            /* Packed histories are immutable distinct buffers. Copy only
             * the unchanged edges from the previous valid picture. */
            if(packed && output!=previous){
                memcpy(rowout,previous+y*stride,indexed_damage_left[y]-first);
                memcpy(rowout+indexed_damage_right[y]-offset,
                       previous+y*stride+indexed_damage_right[y]-offset,
                       last-indexed_damage_right[y]);
            }
            full_first=indexed_damage_left[y];full_last=indexed_damage_right[y];
        }
        unsigned first=full_first,last=full_last;
        unsigned left=indexed_sprite_bounds[y][0]&~3u,right=(indexed_sprite_bounds[y][1]+3)&~3u;
        if(left<first)left=first;
        if(right>last)right=last;
        if(left>=right){copy_indexed_span(rowout+first-offset,composite_codes+y*256+first,last-first,bias);continue;}
        copy_indexed_span(rowout+first-offset,composite_codes+y*256+first,left-first,bias);
#ifdef __NDS__
        resource_background_sprite_mix_arm(indexed_sprites+y*256+left,composite_codes+y*256+left,
            rowout+left-offset,right-left,bias);
#else
        for(unsigned at=y*256+left;at<y*256+right;at+=4){
            uint32_t sprite=*(alias_word*)(indexed_sprites+at);
            uint32_t visible=(sprite+0x7f7f7f7fu)&0x80808080u;
            uint32_t plane=*(alias_word*)(composite_codes+at);
            visible&=(sprite<<1)|~(plane<<1);
            uint32_t mask=(visible>>7)*255;
            *(alias_word*)(rowout+(at-y*256)-offset)=(((sprite&mask)|(plane&~mask))&0x3f3f3f3fu)|bias;
        }
#endif
        copy_indexed_span(rowout+right-offset,composite_codes+y*256+right,last-right,bias);
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    resource_background_profile[0]=drawn-begin;resource_background_profile[1]=sprites_end-drawn;
    resource_background_profile[2]=cpuGetTiming()-sprites_end;resource_background_profile[3]=0;
#endif
    memset(indexed_damage_rows,0,sizeof(indexed_damage_rows));
    indexed_draw_generation++;
    if(picture){picture->destination=(const uint16_t*)output;picture->revision=indexed_draw_generation;}
    return true;
}
bool resource_background_draw_indexed8(uint8_t output[256*192],unsigned first,unsigned last){
    return draw_indexed8(output,first,last,192,0,NULL,false,NULL);
}

void resource_background_picture_invalidate(resource_background_picture *picture){
    if(picture)picture->destination=NULL;
}
bool resource_background_draw_indexed_rgb555(uint16_t output[256*192]){
    return resource_background_draw_indexed_rgb555_cached(output,NULL,NULL);
}
BG_FAST bool resource_background_draw_indexed_rgb555_cached(uint16_t output[256*192],
    resource_background_picture *picture,bool *copied){
    if(copied)*copied=false;
    if(!bank || !output)return false;
    if(!indexed_rgb){
        indexed_rgb=malloc(sizeof(*indexed_rgb));if(!indexed_rgb)return false;
        indexed_rgb->palette_generation=palette_generation-1;
        indexed_rgb->draw_generation=indexed_draw_generation-1;
    }
    /* Palette changes do not alter native dot ownership or priority. Keep
     * the indexed plane/SAT caches alive through fades, then expand the
     * final colour numbers in pairs instead of recolouring every sprite. */
    bool changed=!indexed_mode || indexed_first || indexed_last!=256 || indexed_rows!=192 ||
        screen_dirty[0] || screen_dirty[1] || !indexed_sprites_ready ||
        indexed_rgb->draw_generation!=indexed_draw_generation;
    if(changed){
        if(!resource_background_draw_indexed8(indexed_rgb->pixels,0,256))return false;
        indexed_rgb->draw_generation=indexed_draw_generation;
    }
    if(changed || indexed_rgb->palette_generation!=palette_generation){
        /* The64-colour table fits fast memory. A4096-entry pair table
         * thrashes ARM9's data cache on normal source pictures. */
        for(unsigned i=0;i<256*192;i+=4){
            uint32_t dots=*(const alias_word*)(indexed_rgb->pixels+i);
            *(alias_word*)(indexed_rgb->rgb+i)=colors[dots&63]|(uint32_t)colors[(dots>>8)&63]<<16;
            *(alias_word*)(indexed_rgb->rgb+i+2)=colors[(dots>>16)&63]|(uint32_t)colors[dots>>24]<<16;
        }
        indexed_rgb->palette_generation=palette_generation;
        rgb_output_revision++;
    }
    if(picture && picture->destination==output && picture->revision==rgb_output_revision)return true;
    memcpy(output,indexed_rgb->rgb,256*192*2);
    if(picture){picture->destination=output;picture->revision=rgb_output_revision;}
    if(copied)*copied=true;
    return true;
}


static unsigned opaque_first,opaque_last,opaque_rows;
static uint32_t opaque_palette;
static uint64_t opaque_revision;
/* Row damage describes the immediately preceding owned picture only. A
 * missed/external draw or a new destination must use full source comparison. */
static bool opaque_relative_rows;
const bool *resource_background_last_changed_rows(void){return opaque_relative_rows?indexed_last_rows:NULL;}
BG_FAST bool resource_background_draw_indexed8_area_cached(uint8_t output[256*192],
    resource_background_picture *picture,bool *copied,unsigned first,unsigned last,unsigned rows){
    if(copied)*copied=false;
    opaque_relative_rows=false;
    if(!bank || !output || first>=last || last>256 || !rows || rows>192 || ((first|last)&3))return false;
    bool fresh=!opaque_index;
    if(fresh){
        opaque_index=malloc(sizeof(*opaque_index));if(!opaque_index)return false;
        opaque_index->generation=indexed_draw_generation-1;
    }
    bool bounds=fresh || first!=opaque_first || last!=opaque_last || rows!=opaque_rows;
    bool owned=!bounds && opaque_index->generation==indexed_draw_generation && picture && picture->destination==(const uint16_t*)output && picture->revision==opaque_revision;
    bool changed=bounds || !indexed_mode || indexed_first!=first || indexed_last!=last || indexed_rows!=rows ||
        screen_dirty[0] || screen_dirty[1] || !indexed_sprites_ready || opaque_index->generation!=indexed_draw_generation;
    if(bounds)memset(opaque_index->pixels,0,sizeof(opaque_index->pixels));
    if(changed){
        /* The internal token describes only this owned index page. External
         * draws, dimension changes and rebinds invalidate it normally. */
        resource_background_picture native={0};
        if(!bounds){native.destination=(const uint16_t*)opaque_index->pixels;native.revision=opaque_index->generation;}
        if(!draw_indexed8(opaque_index->pixels,first,last,rows,0x80808080u,&native,false,NULL))return false;
        opaque_index->generation=indexed_draw_generation;
        opaque_first=first;opaque_last=last;opaque_rows=rows;
        opaque_relative_rows=owned;
    }
    bool palette_changed=fresh || opaque_palette!=palette_generation;
    if(changed || palette_changed){opaque_revision++;opaque_palette=palette_generation;}
    if(picture && picture->destination==(const uint16_t*)output && picture->revision==opaque_revision)return true;
    memcpy(output,opaque_index->pixels,256*192);
    if(picture){picture->destination=(const uint16_t*)output;picture->revision=opaque_revision;}
    if(copied)*copied=true;
    return true;
}
BG_FAST bool resource_background_draw_indexed8_opaque(uint8_t output[256*192]){
    return resource_background_draw_indexed8_area_cached(output,NULL,NULL,0,256,192);
}

bool resource_background_draw_area8_retained(uint8_t output[256*192],resource_background_picture *picture){
    return draw_indexed8(output,32,224,192,0,picture,false,NULL);
}

bool resource_background_draw_native_rows_retained(uint8_t output[256*192],unsigned rows,resource_background_picture *picture){
    return draw_indexed8(output,32,224,rows,0,picture,false,NULL);
}

bool resource_background_draw_native_packed(uint8_t *output,unsigned rows){
    /* Every slot receives a complete immutable source picture. Cache reuse
     * applies only to plane/SAT intermediates, never an older history slot. */
    return draw_indexed8(output,32,224,rows,0x80808080u,NULL,true,NULL);
}

bool resource_background_draw_native_packed_retained(uint8_t *output,unsigned rows,
    const uint8_t *previous,resource_background_picture *picture){
    return draw_indexed8(output,32,224,rows,0x80808080u,picture,true,previous);
}
