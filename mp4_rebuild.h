#ifndef mp4_rebuild_H
#define mp4_rebuild_H

#ifdef __cplusplus
extern "C"{
#endif

int mp4_init(const char *initFile, const char* destFile);
int mp4_add_frame(const char * m4sFile);
int mp4_end();

#ifdef __cplusplus
}
#endif

#endif