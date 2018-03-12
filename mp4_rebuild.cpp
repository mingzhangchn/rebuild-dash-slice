#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <unistd.h>  

#include "mp4_rebuild.h"
#include "mp4v2/mp4v2.h"

MP4FileHandle file_handle;
MP4TrackId video_trackId;
MP4TrackId audio_trackId;

typedef struct {
    std::string type;
    long pos;
    long size;
}BoxInfo;

static unsigned int byteToUint32(unsigned char buf[])
{
    return (unsigned int)buf[0] << 24 | (unsigned int)buf[1] << 16 | (unsigned int)buf[2]<<8 | buf[3];
}

static void saveBoxInfo(FILE *fp, long size, std::vector<BoxInfo> &info)
{
    int len = 0;
    while(size > 0 ? len < size: 1){
        unsigned char buf[16] = {0};
        if (fread(buf, 8, 1, fp) <= 0){
            printf("read finish\n");
            break;
        }

        BoxInfo tempInfo;
        std::string temp((const char*)&buf[4], 4);
        tempInfo.type = temp;
        tempInfo.pos = ftell (fp) - 8;
        tempInfo.size = byteToUint32(buf);
        info.push_back(tempInfo);
        
        printf("type:%s, size:%ld\n", tempInfo.type.c_str(), tempInfo.size);    
        if (fseek(fp, tempInfo.size - 8, SEEK_CUR) != 0){
            printf("seek error\n");
            break;
        }
        
        len += tempInfo.size;
    }    
}

int mp4_init(const char *initFile, const char* destFile)
{
    if (!initFile || !destFile){
        printf("Input error\n");
        return -1;
    }

    //create mp4
    file_handle = MP4CreateEx(destFile);
    if (file_handle == MP4_INVALID_FILE_HANDLE){
        printf("open file_handle fialed.\n");
        return -1;
    }
    
    MP4SetTimeScale(file_handle, 1000);
    
    MP4FileHandle srcFile = MP4Modify(initFile);
    
    //int tCount = MP4GetNumberOfTracks(srcFile);
    //printf("---------trackCount:%d\n", tCount);

    video_trackId =  MP4CopyTrack(srcFile, MP4FindTrackId(srcFile,0), file_handle);     
    audio_trackId =  MP4CopyTrack(srcFile, MP4FindTrackId(srcFile,1), file_handle);    

    printf("mp4 init finish\n");
    
    return 0;
}

int mp4_add_frame(const char * m4sFile)
{
    if (!m4sFile){
        printf("mp4_add_frame input error\n");
    }
    
    if (access(m4sFile, R_OK) != 0){
        printf("file:%s not exist\n", m4sFile);
        return -1;
    }
    
    FILE *fp = fopen(m4sFile, "r");
    std::vector<BoxInfo> rootBoxInfo;
    saveBoxInfo(fp, 0, rootBoxInfo);
    
    int moofPos = 0;
    std::vector<BoxInfo> moofBoxInfo;
    for(int i = 0; i < rootBoxInfo.size(); ++i){
        if (rootBoxInfo[i].type.compare("moof") == 0){
            moofPos = rootBoxInfo[i].pos;
            if (fseek(fp, rootBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            saveBoxInfo(fp, rootBoxInfo[i].size - 8, moofBoxInfo);        
        }
    }  
    
    for(int i = 0; i < moofBoxInfo.size(); ++i){
        std::vector<BoxInfo> trafBoxInfo;
        if (moofBoxInfo[i].type.compare("traf") == 0){
            if (fseek(fp, moofBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            saveBoxInfo(fp, moofBoxInfo[i].size - 8, trafBoxInfo);  
            
            int trackId = 0;
            for(int i = 0; i < trafBoxInfo.size(); ++i){
                if (trafBoxInfo[i].type.compare("tfhd") == 0){
                    if (fseek(fp, trafBoxInfo[i].pos + 8 + 4, SEEK_SET) != 0){
                        printf("seek error\n");
                        break;
                    }
                    unsigned char buf[4] = {0};
                    fread(buf, 4, 1, fp);
                    trackId = byteToUint32(buf);
                    printf("trackId:%d\n", trackId);
                }
            }
            
            for(int i = 0; i < trafBoxInfo.size(); ++i){    
                if (trafBoxInfo[i].type.compare("trun") == 0){
                    if (fseek(fp, trafBoxInfo[i].pos + 8 + 4, SEEK_SET) != 0){
                        printf("seek error\n");
                        break;
                    }
                    unsigned char buf[8] = {0};
                    fread(buf, 8, 1, fp);
                    int sampleCount = byteToUint32(buf);  
                    int dataOffset = byteToUint32(&(buf[4])); 
                    //printf("sampleCount:%d, dataOffset:%d\n", sampleCount, dataOffset);
                    int sampleListSize = sampleCount * 8;
                    unsigned char sampleListBuf[sampleListSize];
                    fread(sampleListBuf, sampleListSize, 1, fp);
                    int dataTotalSize = 0;
                    for(int s = 0; s < sampleCount; ++s){
                        int duration = byteToUint32(sampleListBuf+s*8);
                        int size = byteToUint32(sampleListBuf+s*8 + 4);
                        //printf("duration:%d, size:%d\n", duration,size);
                        int sampleOffset = moofPos + dataOffset + dataTotalSize; 
                        fseek(fp, sampleOffset, SEEK_SET);
                        unsigned char buf[size];
                        fread(buf, size, 1, fp);
                        if (trackId == 1){
                            MP4WriteSample(file_handle, video_trackId, (uint8_t*)buf, size , duration, 0, 1);
                        }
                        else if(trackId == 2){
                            MP4WriteSample(file_handle, audio_trackId, (uint8_t*)buf, size , duration, 0, 1);
                        }
                        dataTotalSize += size;
                    }
                }                
            }
        }
        trafBoxInfo.clear();
    }  

    return 0;
}

int mp4_end()
{
    MP4Close(file_handle); 
    return 0;
}
