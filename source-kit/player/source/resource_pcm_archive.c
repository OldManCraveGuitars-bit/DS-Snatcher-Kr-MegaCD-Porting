#include "resource_pcm_archive.h"
#include "resource_archive.h"
#include <stdlib.h>
extern size_t audio_serviced_fread(void *,size_t,size_t,FILE *);
/* The original bank may need64KiB or more, but not a contiguous temporary
 * allocation. Finish all reads before changing the live PCM device, as in
 * the prior contiguous path. Never service sound between load and feeds. */
bool resource_pcm_archive_load(resource_pcm_driver *driver,FILE *file,const char *name,unsigned id){
    size_t bytes=0;
    if(!driver || !resource_archive_find(file,name,&bytes) || !bytes || bytes%2048 || bytes>106496)return false;
    unsigned count=bytes/2048;
    unsigned char *blocks[52]={0};bool good=true;
    for(unsigned i=0;i<count;i++){
        blocks[i]=malloc(2048);
        if(!blocks[i] || audio_serviced_fread(blocks[i],1,2048,file)!=2048 || ferror(file)){good=false;break;}
    }
    if(good)good=resource_pcm_driver_load(driver,id);
    if(good){
        unsigned sectors=(unsigned)driver->ram[0x30]<<24|(unsigned)driver->ram[0x31]<<16|
                         (unsigned)driver->ram[0x32]<<8|driver->ram[0x33];
        good=sectors==count;
    }
    for(unsigned i=0;i<count;i++){
        if(good)good=resource_pcm_driver_feed_load(driver,blocks[i],2048);
        free(blocks[i]);
    }
    return good;
}
