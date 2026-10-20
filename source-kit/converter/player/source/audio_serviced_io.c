#include <stdio.h>
#include <stddef.h>
#include "music_player.h"
static int reading;
size_t audio_serviced_fread(void *destination,size_t size,size_t count,FILE *file){
 if(reading || !size || !count || size>4096)return fread(destination,size,count,file);
 reading=1;size_t done=0;unsigned char *out=destination;
 while(done<count){
  /* A complete catch-up refill can consume many display frames while
   * the DAC advances. Keep reading the original bytes between bounded
   * shared/interactive slices, as story audio streaming already does. */
  music_player_service_io();
  size_t part=count-done,limit=4096/size;if(part>limit)part=limit;
  size_t n=fread(out+done*size,size,part,file);done+=n;
  if(n!=part)break;
 }
 reading=0;return done;
}
