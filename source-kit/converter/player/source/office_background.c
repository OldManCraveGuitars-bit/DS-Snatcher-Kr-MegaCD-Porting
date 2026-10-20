#include <filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "office_background.h"
#include "resource_background.h"
static uint16_t *picture;
void office_background_end(void){free(picture);picture=NULL;}
bool office_background_draw(uint16_t output[256*192]){
    if(!output)return false;
    if(!picture){
        if(!nitroFSInit(NULL))return false;
        FILE *file=fopen("nitro:/native-office-background-r22.nsbg","rb");if(!file)return false;
        bool good=!fseek(file,0,SEEK_END);long bytes=good?ftell(file):-1;
        good=good && bytes>0 && bytes<=131072 && !fseek(file,0,SEEK_SET);
        void *data=good?malloc(bytes):NULL;
        good=data && fread(data,1,bytes,file)==(size_t)bytes && !ferror(file);
        if(fclose(file))good=false;
        if(good)picture=malloc(256*192*2);
        good=good && picture && resource_background_bind(data,bytes) && resource_background_draw(picture);
        resource_background_bind(NULL,0);free(data);
        if(!good){office_background_end();return false;}
    }
    memcpy(output,picture,256*192*2);return true;
}
