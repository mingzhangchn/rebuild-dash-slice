#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <unistd.h>  

#include "mp4_rebuild.h"
#include "mp4v2/mp4v2.h"

int sequenceParameterSetLength = 0;
unsigned char *sequenceParameterSet = NULL;

int pictureParameterSetLength = 0;
unsigned char *pictureParameterSet = NULL;

int video_w = 0;
int video_h = 0;

MP4FileHandle file_handle;
MP4TrackId video_trackId;
MP4TrackId audio_trackId;


typedef struct {
    std::string type;
    long pos;
    long size;
}BoxInfo;

typedef struct{
    unsigned int first_chunk;
    unsigned int samples_per_chunk;
    unsigned int sample_description_index;
}StscInfo;

typedef struct{
    unsigned int chunk_index;
    unsigned int offset;
    unsigned int sample_count;
    unsigned int sample_description_index;
}ChunkInfo;

typedef struct{
    unsigned int sample_index;
    unsigned int offset;
    unsigned int size;
    unsigned int delta;
    unsigned int chunk_index;
    unsigned int sample_description_index;
}SampleInfo;

typedef struct{
    unsigned int start;
    unsigned int size;
}SliceInfo;

static unsigned int byteToUint32(unsigned char buf[])
{
    return (unsigned int)buf[0] << 24 | (unsigned int)buf[1] << 16 | (unsigned int)buf[2]<<8 | buf[3];
}

static int set_box_size(int offset, int size, FILE *f){
    fseek(f, offset, SEEK_SET);
    unsigned char size_buf[4] = {0};
    size_buf[0] = (size & 0xff000000) >> 24;
    size_buf[1] = (size & 0x00ff0000) >> 16;
    size_buf[2] = (size & 0x0000ff00) >> 8;
    size_buf[3] = (size & 0x000000ff);
    fwrite(size_buf, 1, 4, f);           
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
    
    FILE *fp = fopen(initFile, "r");
    std::vector<BoxInfo> rootBoxInfo;
    saveBoxInfo(fp, 0, rootBoxInfo);

    int media_duration_s = 0;
    int trackCount = 0;
    int vide_track = 0;
    int soun_track = 1;
    int v_timescale = 0;
    int s_timescale = 0;
    int vs_timescale[2] =  {0};
    
    std::vector<BoxInfo> moovBoxInfo;
    for(int i = 0; i < rootBoxInfo.size(); ++i){
        if (rootBoxInfo[i].type.compare("moov") == 0){
            if (fseek(fp, rootBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            saveBoxInfo(fp, rootBoxInfo[i].size - 8, moovBoxInfo);        
        }
    }
    
    for(int i = 0; i < moovBoxInfo.size(); ++i){
        if (moovBoxInfo[i].type.compare("trak") == 0){   
            trackCount++;        
        } 
    } 
    std::cout<<"trak count:"<<trackCount<<std::endl;
    
    int trackIndex = 0;
    std::vector<BoxInfo> trakBoxInfo[trackCount];
    for(int i = 0; i < moovBoxInfo.size(); ++i){
        if (moovBoxInfo[i].type.compare("trak") == 0){
            if (fseek(fp, moovBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            saveBoxInfo(fp, moovBoxInfo[i].size - 8, trakBoxInfo[trackIndex]);   
            trackIndex++;        
        } 
        
        if (moovBoxInfo[i].type.compare("mvhd") == 0){
            if (fseek(fp, moovBoxInfo[i].pos + 8, SEEK_SET) != 0){
                printf("seek error\n");
                break;
            }
            unsigned char tBuf[20] = {0};
            fread(tBuf, 1 , 20, fp);
            
            int scale = byteToUint32(&(tBuf[12]));
            int dural = byteToUint32(&(tBuf[16]));
            
            media_duration_s = dural/scale;
            printf("media_duration_s:%d\n", media_duration_s);
        }         
    }    

    std::vector<BoxInfo> mdiaBoxInfo[trackCount];
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < trakBoxInfo[index].size(); ++i){
            if (trakBoxInfo[index][i].type.compare("mdia") == 0){
                if (fseek(fp, trakBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                saveBoxInfo(fp, trakBoxInfo[index][i].size - 8, mdiaBoxInfo[index]);   
            }  
            if (trakBoxInfo[index][i].type.compare("tkhd") == 0){
                if (index == 0){
                    if (fseek(fp, trakBoxInfo[index][i].pos + 82, SEEK_SET) != 0){
                        printf("seek error\n");
                        break;
                    }
                    unsigned char buf[8] = {0};
                    fread(buf, 8, 1, fp);
                    //printf("%02x%02x%02x%02x%02x%02x%02x%02x\n", buf[0], buf[1],buf[2],buf[3],buf[4], buf[5],buf[6],buf[7]);
                    video_w = byteToUint32(buf);
                    video_h = byteToUint32(&(buf[4]));
                }
            }             
        }        
    }
    
    std::vector<BoxInfo> minfBoxInfo[trackCount];
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < mdiaBoxInfo[index].size(); ++i){
            if (mdiaBoxInfo[index][i].type.compare("minf") == 0){
                if (fseek(fp, mdiaBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                saveBoxInfo(fp, mdiaBoxInfo[index][i].size - 8, minfBoxInfo[index]);   
            }  
            if (mdiaBoxInfo[index][i].type.compare("hdlr") == 0){
                if (fseek(fp, mdiaBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 8, SEEK_CUR);
                char buf[4] = {0};
                fread(buf, 4, 1, fp);
                printf("track[%d]:%s\n", index, buf);
                if (strncmp(buf, "vide", 4) == 0){
                    vide_track = index;
                }
                if (strncmp(buf, "soun", 4) == 0){
                    soun_track = index;
                }                
            }  
        }        
    }
    
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < mdiaBoxInfo[index].size(); ++i){  
            if (mdiaBoxInfo[index][i].type.compare("mdhd") == 0){
                if (fseek(fp, mdiaBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 12, SEEK_CUR);
                unsigned char buf[4] = {0};
                fread(buf, 4, 1, fp);
                if (index == vide_track){ //can only be used when vide and soun be set properly
                    v_timescale = byteToUint32(buf);
                    printf("v_timescale:%d\n", v_timescale);
                    vs_timescale[0] = v_timescale;
                }
                if (index == soun_track){
                    s_timescale = byteToUint32(buf);
                    printf("s_timescale:%d\n", s_timescale);
                    vs_timescale[1] = s_timescale;
                }                
            }  
        }        
    }    
    
    std::vector<BoxInfo> stblBoxInfo[trackCount];
    for(int index = 0; index < trackCount; ++index){
        for(int i = 0; i < minfBoxInfo[index].size(); ++i){
            if (minfBoxInfo[index][i].type.compare("stbl") == 0){
                if (fseek(fp, minfBoxInfo[index][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                saveBoxInfo(fp, minfBoxInfo[index][i].size - 8, stblBoxInfo[index]);   
            }  
        }        
    }    
    
    std::vector<StscInfo> stscList[trackCount];
    std::vector<unsigned int> stcoList[trackCount];
    std::vector<unsigned int> stszList[trackCount];
    unsigned int chunkTotalCount[trackCount];
    unsigned int sampleTotalCount[trackCount];
    ChunkInfo *chunkList[trackCount];
    SampleInfo *sampleList[trackCount];
    for(int i = 0; i < trackCount; ++i){//init
        chunkList[i] = NULL;
        sampleList[i] = NULL;
    }    
    
    std::vector<SliceInfo> slice_list;
    
    //int tIndex = 0;
    for(int tIndex = 0; tIndex < trackCount; ++tIndex)
    {
        int sampleCount = 0;
        int chunkCount = 0;
        printf("\n-------Track %d------\n", tIndex);
        for(int i = 0; i < stblBoxInfo[tIndex].size(); ++i){
            if (stblBoxInfo[tIndex][i].type.compare("stsd") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[4] = {0};
                int ret = fread(buf, 4, 1, fp);

                int entry_count = byteToUint32(buf);
                std::cout<<"stsd entry count:"<<entry_count<<std::endl;
                if (tIndex == 0){
                    unsigned char tempBuf[8] = {0};
                    fread(tempBuf, 8, 1, fp);   
                    int blen = byteToUint32(tempBuf);
                    printf("%c%c%c%c:len:%d\n", tempBuf[4],tempBuf[5],tempBuf[6],tempBuf[7], blen);
#if 0                    
                    if (memcmp(&(tempBuf[4]), "avc1", 4) == 0){
                        printf("-----------------\n");
                        fseek(fp, 78, SEEK_CUR);
                        unsigned char buf[8] = {0};
                        int ret = fread(buf, 8, 1, fp);
                        int avccLen = byteToUint32(buf);
                        
                        if (memcmp(&(buf[4]), "avcC", 4) == 0){     
                            unsigned char avccBuf[avccLen];
                            fread(avccBuf, avccLen - 8, 1, fp);
                            if ( (avccBuf[5] & 0x1f) == 0x01){
                                sequenceParameterSetLength = ((int)avccBuf[6]) << 8 | avccBuf[7];
                                sequenceParameterSet = (unsigned char *)malloc(sequenceParameterSetLength);
                                memcpy(sequenceParameterSet, &avccBuf[5+2 + 1], sequenceParameterSetLength);
                                printf("\n");
                                for(int s = 0; s < sequenceParameterSetLength; ++s){
                                    printf("%02x ", sequenceParameterSet[s]);
                                }
                                printf("\n");
                                
                                
                                int p_offset = 5 + 2 + sequenceParameterSetLength + 1;
                                int numOfPictureParameterSets = avccBuf[p_offset];
                                pictureParameterSetLength = ((int)avccBuf[p_offset+1]) << 8 | avccBuf[p_offset+2];
                                pictureParameterSet = (unsigned char*)malloc(pictureParameterSetLength);
                                memcpy(pictureParameterSet, &avccBuf[p_offset+2 + 1], pictureParameterSetLength);
                                printf("\n");
                                for(int s = 0; s < pictureParameterSetLength; ++s){
                                    printf("%02x ", pictureParameterSet[s]);
                                }
                                printf("\n");                                
                                
                            }
                            else{
                                printf("numOfSequenceParameterSets more than one, todo\n");
                            }
                            printf(" %x %x %x\n", avccBuf[5], avccBuf[6], avccBuf[7]);
                            
                        } 
                    }   
#endif                    
                }
            }
            
            if (stblBoxInfo[tIndex][i].type.compare("stsc") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[4] = {0};
                int ret = fread(buf, 4, 1, fp);

                int entry_count = byteToUint32(buf);
                std::cout<<"stsc entry count:"<<entry_count<<std::endl;
                int count = 0;
                while(count < entry_count){
                    unsigned char tempBuf[12] = {0};
                    fread(tempBuf, 12, 1, fp);
                    StscInfo info;
                    info.first_chunk = byteToUint32(tempBuf);
                    info.samples_per_chunk = byteToUint32(tempBuf + 4);
                    info.sample_description_index = byteToUint32(tempBuf + 8);
                    stscList[tIndex].push_back(info);
                    //printf("%d:%d:%d\n", info.first_chunk, info.samples_per_chunk, info.sample_description_index);
                    count++;
                }
            }  
         
            if (stblBoxInfo[tIndex][i].type.compare("stco") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[4] = {0};
                int ret = fread(buf, 4, 1, fp);

                int entry_count = byteToUint32(buf);
                std::cout<<"stco entry count:"<<entry_count<<std::endl;
                int count = 0;
                while(count < entry_count){
                    unsigned char tempBuf[4] = {0};
                    fread(tempBuf, 4, 1, fp);

                    unsigned int offset = byteToUint32(tempBuf);

                    stcoList[tIndex].push_back(offset);
                    //printf("Index : %d, offset:%d\n", count, offset);
                    count++;
                }
                chunkTotalCount[tIndex] = stcoList[tIndex].size();
                chunkCount = chunkTotalCount[tIndex];
                chunkList[tIndex] = (ChunkInfo*)malloc(sizeof(ChunkInfo)*chunkCount);
            }  
            
            if (stblBoxInfo[tIndex][i].type.compare("stsz") == 0){
                if (fseek(fp, stblBoxInfo[tIndex][i].pos + 8, SEEK_SET) != 0){
                    printf("seek error\n");
                    break;
                }
                fseek(fp, 4, SEEK_CUR);
                unsigned char buf[8] = {0};
                int ret = fread(buf, 8, 1, fp);
                int sample_size = byteToUint32(buf);
                int entry_count = byteToUint32(buf + 4);
                std::cout<<"stsz sample size:"<<sample_size<< ", entry count:"<<entry_count<<std::endl;
                int count = 0;
                while(count < entry_count){
                    unsigned char tempBuf[4] = {0};
                    fread(tempBuf, 4, 1, fp);

                    unsigned int entry_size = byteToUint32(tempBuf);
                    stszList[tIndex].push_back(entry_size);
                    
                    //printf("Index: %d, entry_size:%d\n", count, entry_size);
                    count++;
                }
                sampleTotalCount[tIndex] = stszList[tIndex].size();
                sampleCount = sampleTotalCount[tIndex];
                sampleList[tIndex] = (SampleInfo*)malloc(sizeof(SampleInfo)*sampleCount);
            }  
        }         
    }
    
    for(int i = 0; i < trackCount; ++i){
        if (chunkList[i]){
            free(chunkList[i]);
            chunkList[i] = NULL;
        }
        if (sampleList[i]){
            free(sampleList[i]);
            sampleList[i] = NULL;
        }
    }
    fclose(fp);    
    
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
                            //printf("video_trackId:%d\n", video_trackId);
                            MP4WriteSample(file_handle, video_trackId, (uint8_t*)buf, size , duration, 0, 1);
                        }
                        else if(trackId == 2){
                            //printf("audio_trackId:%d\n", audio_trackId);
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
