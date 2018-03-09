#include "mp4_rebuild.h"
#include <stdio.h>


int main()
{
    printf("Hello world!\n");
    mp4_init("test-stream/init.mp4", "test-stream/dest.mp4");
    
    mp4_add_frame("test-stream/1.m4s");
    
    mp4_end();
    
    return 0;
}