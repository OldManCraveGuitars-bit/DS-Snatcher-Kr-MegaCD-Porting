#ifndef RESOURCE_VOICE_STREAM_H
#define RESOURCE_VOICE_STREAM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RESOURCE_VOICE_CHUNK 2048u
#define RESOURCE_VOICE_GUARD 2048u
typedef bool (*resource_voice_read)(void *context,void *output,size_t bytes);
typedef void (*resource_voice_publish)(void *context,void *output,size_t bytes);
typedef void (*resource_voice_sink)(void *context,unsigned offset,void *input,size_t bytes);

typedef struct {
    uint8_t *buffer;
    unsigned capacity,total,written,played,refills,min_lead,error;
    resource_voice_read read;
    resource_voice_publish publish;
    resource_voice_sink sink;
    void *context;
} resource_voice_stream;
bool resource_voice_stream_begin(resource_voice_stream *,void *,unsigned,unsigned,
                                 resource_voice_read,resource_voice_publish,void *);
bool resource_voice_stream_begin_sink(resource_voice_stream *,void *,unsigned,unsigned,unsigned,
                                      resource_voice_read,resource_voice_sink,void *);
bool resource_voice_stream_update(resource_voice_stream *,unsigned played);
#endif
