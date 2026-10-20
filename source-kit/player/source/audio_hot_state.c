#include "audio_hot_state.h"
#if defined(__NDS__) && defined(ARM9)
audio_hot_scratch audio_hot_state __attribute__((section(".dtcm")));
#else
audio_hot_scratch audio_hot_state;
#endif
_Static_assert(sizeof(FM_CH) <= sizeof(resource_pcm_output), "Reuse existing PCM scratch budget");
