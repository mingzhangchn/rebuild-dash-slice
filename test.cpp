#include "mp4_rebuild.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 

int main(int argc, char* argv[])
{
    if (argc < 4){
        printf("input error\n");
        return -1;
    }
    char *dir = argv[1];
    int count = atoi(argv[2]);
    char *name = argv[3];
    
    if (access(dir, F_OK) < 0 ){
        printf("src file not exist!\n");
        return -1;
    }
    
    printf("dir:%s, count:%d\n", dir, count);
    
    char destName[1024] = {0};
    snprintf(destName, sizeof(destName), "%s/%s", dir, name);
    char item[1024] = {0};
    snprintf(item, sizeof(item), "%s/init.mp4", dir);
    mp4_init(item, destName);
    
    for(int i = 0; i < count; ++i){
        memset(item, 0, sizeof(item));
        snprintf(item, sizeof(item), "%s/%d.m4s", dir, i);
        mp4_add_frame(item);
    }
    
    mp4_end();
    
    return 0;
}