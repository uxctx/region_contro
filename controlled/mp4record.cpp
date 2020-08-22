#include "controlled.h"

#include "mp4record.h"
#include "string.h"
#include <stdlib.h>

MP4_CONFIG* CreateMP4File(const char* pFileName, int fps, int width, int height)
{
	MP4_CONFIG* config = new MP4_CONFIG();
	config->videoId = MP4_INVALID_TRACK_ID;

	config->timeScale = 1000;

	config->fps = fps;
	config->width = width;
	config->height = height;

	config->video_track_configured = false;

	config->hMp4file = MP4Create(pFileName, 0);
	if (config->hMp4file == MP4_INVALID_FILE_HANDLE)
	{
		printf("open file fialed.\n");
		return NULL;
	}

	MP4SetTimeScale(config->hMp4file, config->timeScale);    

	return config;
}

void CloseMP4File(MP4_CONFIG* config)
{
	if (config->hMp4file)
	{
		MP4Close(config->hMp4file, 0);
		config->hMp4file = NULL;
	}

}

void WriteH264_SPS(MP4_CONFIG* config, unsigned char* data, int length)
{
	data = data + 4;
	length = length - 4;

	config->videoId = MP4AddH264VideoTrack
	(config->hMp4file,
		config->timeScale,                               //timeScale
		(config->timeScale / config->fps),           //sampleDuration    timeScale/fps
		config->width,     								// width  
		config->height,    								// height    

		data[5], // sps[1] AVCProfileIndication
		data[6], // sps[2] profile_compat
		data[7], // sps[3] AVCLevelIndication

		//0x42,//n->buf[1], // sps[1] AVCProfileIndication    
		//0,// n->buf[2], // sps[2] profile_compat    
		//0x1f,//n->buf[3], // sps[3] AVCLevelIndication    
		3);           // 4 bytes length before each NAL unit 


	if (config->videoId == MP4_INVALID_TRACK_ID)
	{
		printf("add video track failed.\n");
		return;
	}

	//MP4SetVideoProfileLevel(config->hMp4file, 0x0f); //  Simple Profile @ Level 3 
	MP4AddH264SequenceParameterSet(config->hMp4file, config->videoId, data + 4, length - 4);
}

void WriteH264_PPS(MP4_CONFIG * config, unsigned char* data, int length)
{
	MP4AddH264PictureParameterSet(config->hMp4file, config->videoId, data + 4, length - 4);
}

void WriteH264_Data(MP4_CONFIG * config, x264_nal_t * nals, int nal_count, size_t  payload_size, bool b_keyframe, int64_t decode_delta, int64_t composition_offset)
{
	unsigned char* pdata = NULL;
	int datalen = 0;

	x264_nal_t* nal_ptr = NULL;
	for (int i = 0; i < nal_count; i++) {
		nal_ptr = &nals[i];
		switch (nal_ptr->i_type) {
		case NAL_SPS:

			if (config->videoId == MP4_INVALID_TRACK_ID) {


				config->videoId = MP4AddH264VideoTrack
				(config->hMp4file,
					//config->timeScale,                               //timeScale
					//(config->timeScale / config->fps),           //sampleDuration    timeScale/fps
					config->timeScale,
					config->timeScale / config->fps,
					config->width,     								// width  
					config->height,    								// height    
					nal_ptr->p_payload[5],
					nal_ptr->p_payload[6],
					nal_ptr->p_payload[7],

					3);

				MP4SetVideoProfileLevel(config->hMp4file, 0x0f);
				config->video_track_configured = true;
			}

			MP4AddH264SequenceParameterSet(config->hMp4file, config->videoId, nal_ptr->p_payload + 4, nal_ptr->i_payload - 4);
			break;
		case NAL_PPS:

			MP4AddH264PictureParameterSet(config->hMp4file, config->videoId, nal_ptr->p_payload + 4, nal_ptr->i_payload - 4);
			break;
		case NAL_FILLER:

			break;
		case NALU_TYPE_SEI:

			break;
			//case NALU_TYPE_IDR:
			//case NALU_TYPE_SLICE:
		default:
		{

			int remaining_nals = nal_count - i;
			uint8_t* start_ptr = nals[i].p_payload;
			int size = payload_size - (start_ptr - (nals[0].p_payload));

			pdata = start_ptr - 4;

			pdata[0] = size >> 24;
			pdata[1] = size >> 16;
			pdata[2] = size >> 8;
			pdata[3] = size & 0xff;


			if (MP4WriteSample(config->hMp4file, config->videoId, pdata, size + 4, decode_delta, composition_offset, b_keyframe) != true)
				fprintf(stderr, "enc_mp4_write_video_sample: MP4WriteSample (NAL %d) failed\n", i);


			i += remaining_nals;
			break;
		}

		break;
		}
	}

}
