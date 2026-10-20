/* Host-only file adapter for portable resource conversion/verification.
 * There is no asynchronous NDS audio device to service on this host path.
 * The NDS player must link source/audio_serviced_io.c instead. */
#ifdef __NDS__
#error The host resource I/O adapter must never replace NDS audio servicing
#endif
#include <stdio.h>

size_t audio_serviced_fread(void *destination,size_t size,size_t count,FILE *file){
    return fread(destination,size,count,file);
}
