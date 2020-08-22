//
// Created by M君 on 2019/4/29.
//

#ifndef HHMP4V2TEST_MP4RECORD_H
#define HHMP4V2TEST_MP4RECORD_H

#include <mp4v2/mp4v2.h>


extern "C"
{
#include <x264.h>
#include <x264_config.h>
}

#define _NALU_SPS_  7
#define _NALU_PPS_  8
#define _NALU_IDR_  5
#define _NALU_BP_   1

typedef enum {
	NALU_TYPE_SLICE = 1,
	NALU_TYPE_DPA = 2,
	NALU_TYPE_DPB = 3,
	NALU_TYPE_DPC = 4,
	NALU_TYPE_IDR = 5,
	NALU_TYPE_SEI = 6,
	NALU_TYPE_SPS = 7,
	NALU_TYPE_PPS = 8,
	NALU_TYPE_AUD = 9,
	NALU_TYPE_EOSEQ = 10,
	NALU_TYPE_EOSTREAM = 11,
	NALU_TYPE_FILL = 12,
} NaluType;



typedef struct Mp4_Config
{
	MP4FileHandle hMp4file;
	MP4TrackId videoId;
	int timeScale;        //视频每秒的ticks数,如90000
	int fps;              //视频帧率
	int width;          //视频宽
	int height;         //视频高

	bool video_track_configured;
}MP4_CONFIG;



MP4_CONFIG* CreateMP4File(const char* pFileName, int fps, int width, int height);

void CloseMP4File(MP4_CONFIG* config);

void WriteH264_SPS(MP4_CONFIG* config, unsigned char* data, int length);

void WriteH264_PPS(MP4_CONFIG* config, unsigned char* data, int length);

void WriteH264_Data(MP4_CONFIG* config, x264_nal_t* nals, int nal_count, size_t  payload_size, bool b_keyframe, int64_t decode_delta, int64_t composition_offset);


#endif //HHMP4V2TEST_MP4RECORD_H
