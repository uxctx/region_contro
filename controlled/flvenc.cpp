#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "flv.h"

#undef NDEBUG
#include <assert.h>



static void la_flush(flv_t *a)
{
	fwrite(a->buff,1,a->buff_index, a->f);
	fflush(a->f);
	a->buff_index = 0;
}

static size_t la_fwrite( const void* buf, size_t size, size_t count, flv_t* a)
{
	//write to buff, if buff is full , flush it
	long length = size*count;
	if(length>BUFFMAX)
	{// if length > buff max length, write it to file directly.
		la_flush(a);
		return fwrite(buf, size, count,a->f);		
	}

	if(a->buff_index+length >= BUFFMAX)
		la_flush(a);
	//cout<<"flush ok."<<endl;
	//cout<<length<<endl;
	if (length >= BUFFMAX)
	{
		fwrite(buf, 1, length, a->f);
	}
	else
	{
		memcpy(a->buff + a->buff_index, buf, length);
		a->buff_index += length;
	}
	return length;
}

static void la_fputc( int ch, flv_t* a)
{
	if(a->buff_index+1 >= BUFFMAX)
		la_flush(a);
	a->buff[a->buff_index] = (char)ch;
	a->buff_index++;
}

static long la_ftell( flv_t* a)
{
	la_flush(a);
	return ftell(a->f);
}

static int la_fseek(flv_t* a, long _Offset, int _Origin)
{
	la_flush(a);
	return fseek(a->f, _Offset, _Origin);
}
//end add by lostangel
/***********************************************************************/


static void put_byte( flv_t *a, int w )
{
	la_fputc( ( w      ) & 0xff, a ); 
}

static void put_be16( flv_t *a, uint32_t w )
{
    //fputc( ( w      ) & 0xff, a->f );
    //fputc( ( w >> 8 ) & 0xff, a->f );

	la_fputc( ( w >> 8 ) & 0xff, a );
	la_fputc( ( w      ) & 0xff, a ); 
}

static void put_be24(flv_t *a, uint32_t w)
{
    put_be16(a, w >> 8);
    put_byte(a, w);
}

static void put_be32( flv_t *a, uint32_t dw )
{
	la_fputc( ( dw >> 24) & 0xff, a );
	la_fputc( ( dw >> 16) & 0xff, a );
	la_fputc( ( dw >> 8 ) & 0xff, a );
	la_fputc( ( dw      ) & 0xff, a );
}

static void put_be64( flv_t *a, uint64_t dw )
{
	put_be32(a, (uint32_t)(dw >> 32));
    put_be32(a, (uint32_t)(dw & 0xffffffff));
}

static void put_tag(flv_t *a, const char *tag)
{
    while (*tag) {
        put_byte(a, *tag++);
    }
}

static void put_buffer(flv_t *a,const unsigned char *buf, int size)
{
	la_fwrite(buf,1,size,a);
}

static void put_amf_string(flv_t *a, const char *str)
{
    size_t len = strlen(str);
    put_be16(a, len);
//    put_buffer(pb, str, len);
	la_fwrite(str,len,1,a);
}


static int64_t av_dbl2int(double d)
{
    int e;
    if     ( !d) return 0;
    else if(d-d) return 0x7FF0000000000000LL + ((int64_t)(d<0)<<63) + (d!=d);
    d= frexp(d, &e);
    return (int64_t)(d<0)<<63 | (e+1022LL)<<52 | (int64_t)((fabs(d)-0.5)*(1LL<<53));
}


static void put_amf_double(flv_t *a, double d)
{
    put_byte(a, AMF_DATA_TYPE_NUMBER);
    put_be64(a, av_dbl2int(d));
}

static void put_amf_bool(flv_t *a, int b) {
    put_byte(a, AMF_DATA_TYPE_BOOL);
    put_byte(a, !!b);
}

//typedef   int   intptr_t;
/*************************************************************************/
static const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for( end -= 3; p < a && p < end; p++ ) {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
            return p;
    }

    for( end -= 3; p < end; p += 4 ) {
        uint32_t x = *(const uint32_t*)p;
//      if( (x - 0x01000100) & (~x) & 0x80008000 ) // little endian
//      if( (x - 0x00010001) & (~x) & 0x00800080 ) // big endian
        if( (x - 0x01010101) & (~x) & 0x80808080 ) { // generic
            if( p[1] == 0 ) {
                if( p[0] == 0 && p[2] == 1 )
                    return p-1;
                if( p[2] == 0 && p[3] == 1 )
                    return p;
            }
            if( p[3] == 0 ) {
                if( p[2] == 0 && p[4] == 1 )
                    return p+1;
                if( p[4] == 0 && p[5] == 1 )
                    return p+2;
            }
        }
    }

    for( end += 3; p < end; p++ ) {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
            return p;
    }

    return end + 3;
}

static int ff_avc_parse_nal_units(const uint8_t *buf_in, uint8_t *buf_out, int size)
{
    const uint8_t *p = buf_in;
    const uint8_t *end = p + size;
    const uint8_t *nal_start, *nal_end;
	int nal_size;

	uint8_t *p_out = buf_out;

    size = 0;
    nal_start = ff_avc_find_startcode(p, end);
    while (nal_start < end) {
        while(!*(nal_start++));
        nal_end = ff_avc_find_startcode(nal_start, end);
//        put_be32(pb, nal_end - nal_start);
//        put_buffer(pb, nal_start, nal_end - nal_start);
		nal_size = nal_end - nal_start;
		p_out[0]=( nal_size >> 24) & 0xff;
		p_out[1]=( nal_size >> 16) & 0xff;
		p_out[2]=( nal_size >>  8) & 0xff;
		p_out[3]=( nal_size      ) & 0xff;
		memcpy(&p_out[4],nal_start,nal_size);
		p_out += 4 + nal_end - nal_start;

        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }
    return size;
}

static int ff_avc_parse_nal_units_buf(const uint8_t *buf_in, uint8_t *buf_out, int *size)
{

    int ret=ff_avc_parse_nal_units(buf_in, buf_out, *size);
	*size = ret;

    return 0;
}

#define AV_RB32(x)  ((((const uint8_t*)(x))[0] << 24) | \
                     (((const uint8_t*)(x))[1] << 16) | \
                     (((const uint8_t*)(x))[2] <<  8) | \
                      ((const uint8_t*)(x))[3])

#define AV_RB24(x)  ((((const uint8_t*)(x))[0] << 16) | \
                     (((const uint8_t*)(x))[1] <<  8) | \
                      ((const uint8_t*)(x))[2])


static int ff_isom_write_avcc(flv_t *pb, const uint8_t *data, int len)
{
    if (len > 6) {
        /* check for h264 start code */
        if (AV_RB32(data) == 0x00000001 ||
            AV_RB24(data) == 0x000001) {
            uint8_t *buf=NULL, *end, *start;
            uint32_t sps_size=0, pps_size=0;
            uint8_t *sps=0, *pps=0;

			buf = new unsigned char[len+10];
            int ret = ff_avc_parse_nal_units_buf(data, buf, &len);
            if (ret < 0)
                return ret;
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end) {
                unsigned int size;
                uint8_t nal_type;
                size = AV_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7) { /* SPS */
                    sps = buf + 4;
                    sps_size = size;
                } else if (nal_type == 8) { /* PPS */
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }
            assert(sps);
            assert(pps);

            put_byte(pb, 1); /* version */
            put_byte(pb, sps[1]); /* profile */
            put_byte(pb, sps[2]); /* profile compat */
            put_byte(pb, sps[3]); /* level */
            put_byte(pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
            put_byte(pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

            put_be16(pb, sps_size);
            put_buffer(pb, sps, sps_size);
            put_byte(pb, 1); /* number of pps */
            put_be16(pb, pps_size);
            put_buffer(pb, pps, pps_size);
            
			delete start;
//			buf=NULL;
        } else {
            put_buffer(pb, data, len);
        }
    }
    return 0;
}
/*************************************************************************/

static int flv_write_header(flv_t *pb)
{
/*    ByteIOContext *pb = s->pb;
    FLVContext *flv = s->priv_data;
    AVCodecContext *audio_enc = NULL, *video_enc = NULL;
    int i;
    double framerate = 0.0;
    int metadata_size_pos, data_size;

    for(i=0; i<s->nb_streams; i++){
        AVCodecContext *enc = s->streams[i]->codec;
        if (enc->codec_type == CODEC_TYPE_VIDEO) {
            if (s->streams[i]->r_frame_rate.den && s->streams[i]->r_frame_rate.num) {
                framerate = av_q2d(s->streams[i]->r_frame_rate);
            } else {
                framerate = 1/av_q2d(s->streams[i]->codec->time_base);
            }
            video_enc = enc;
            if(enc->codec_tag == 0) {
                av_log(enc, AV_LOG_ERROR, "video codec not compatible with flv\n");
                return -1;
            }
        } else {
            audio_enc = enc;
            if(get_audio_flags(enc)<0)
                return -1;
        }
        av_set_pts_info(s->streams[i], 32, 1, 1000); // 32 bit pts in ms
    }
*/
	int metadata_size_pos, data_size;

    put_tag(pb,"FLV");
    put_byte(pb,1);
    put_byte(pb,   FLV_HEADER_FLAG_HASAUDIO * !!pb->audioCodec
                 + FLV_HEADER_FLAG_HASVIDEO * !!pb->videoCodec);
    put_be32(pb,9);
    put_be32(pb,0);


    // write meta_tag 
    put_byte(pb, 18);         // tag type META
    metadata_size_pos= la_ftell(pb);
    put_be24(pb, 0);          // size of data part (sum of all parts below)
    put_be24(pb, 0);          // time stamp
    put_be32(pb, 0);          // reserved

    // now data of data_size size 

    // first event name as a string 
    put_byte(pb, AMF_DATA_TYPE_STRING);
    put_amf_string(pb, "onMetaData"); // 12 bytes

    // mixed array (hash) with size and string/type/data tuples 
    put_byte(pb, AMF_DATA_TYPE_MIXEDARRAY);
    put_be32(pb, 5*!!pb->videoCodec + 4*!!pb->audioCodec + 2); // +2 for duration and file size

    put_amf_string(pb, "duration");
    pb->duration_offset= la_ftell(pb);
    put_amf_double(pb, 0); // delayed write

    if(pb->videoCodec){
        put_amf_string(pb, "width");
        put_amf_double(pb, pb->i_width);

        put_amf_string(pb, "height");
        put_amf_double(pb, pb->i_height);

//        put_amf_string(pb, "videodatarate");
//        put_amf_double(pb, video_enc->bit_rate / 1024.0);

        put_amf_string(pb, "framerate");
        put_amf_double(pb, pb->i_fps);

        put_amf_string(pb, "videocodecid");
        put_amf_double(pb, pb->videoCodec);
    }

    if(pb->audioCodec){
//        put_amf_string(pb, "audiodatarate");
//        put_amf_double(pb, audio_enc->bit_rate / 1024.0);

        put_amf_string(pb, "audiosamplerate");
        put_amf_double(pb, 44100/*audio_enc->sample_rate*/);

        put_amf_string(pb, "audiosamplesize");
        put_amf_double(pb, 16/*audio_enc->codec_id == CODEC_ID_PCM_S8 ? 8 : 16*/);

        put_amf_string(pb, "stereo");
        put_amf_bool(pb, 1/*audio_enc->channels == 2*/);

        put_amf_string(pb, "audiocodecid");
		put_byte(pb, AMF_DATA_TYPE_STRING);
		put_amf_string(pb, "mp4a");
//        put_amf_double(pb, pb->audioCodec);
    }

    put_amf_string(pb, "filesize");
    pb->filesize_offset= la_ftell(pb);
    put_amf_double(pb, 0); // delayed write

    put_amf_string(pb, "");
    put_byte(pb, AMF_END_OF_OBJECT);

    // write total size of tag 
    data_size= la_ftell(pb) - metadata_size_pos - 10;
    la_fseek(pb, metadata_size_pos, SEEK_SET);
    put_be24(pb, data_size);
    la_fseek(pb, data_size + 10 - 3, SEEK_CUR);
    put_be32(pb, data_size + 11);


	long pos;
	if(pb->videoCodec!=0) //写入video extra数据
	{
		put_byte(pb,FLV_TAG_TYPE_VIDEO);
        put_be24(pb, 0); // size patched later
        put_be24(pb, 0); // ts
        put_byte(pb, 0); // ts ext
        put_be24(pb, 0); // streamid
        pos = la_ftell(pb);
        
        put_byte(pb, pb->videoCodec | FLV_FRAME_KEY); // flags
        put_byte(pb, 0); // AVC sequence header
        put_be24(pb, 0); // composition time

		if(pb->video_extradata_size>0)
			ff_isom_write_avcc(pb, pb->video_extradata, pb->video_extradata_size);

        data_size = la_ftell(pb) - pos;
        la_fseek(pb, -data_size - 10, SEEK_CUR);
        put_be24(pb, data_size);
        la_fseek(pb, data_size + 10 - 3, SEEK_CUR);
        put_be32(pb, data_size + 11); // previous tag size
	}

	if(pb->audioCodec!=0) //写入audio extra数据
	{
		put_byte(pb,FLV_TAG_TYPE_AUDIO);
        put_be24(pb, 0); // size patched later
        put_be24(pb, 0); // ts
        put_byte(pb, 0); // ts ext
        put_be24(pb, 0); // streamid
        pos = la_ftell(pb);
        
        put_byte(pb, FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO); // flags
        put_byte(pb, 0); // // AAC sequence headerr

		if(pb->audio_extradata_size>0)
			put_buffer(pb, pb->audio_extradata, pb->audio_extradata_size);

        data_size = la_ftell(pb) - pos;
        la_fseek(pb, -data_size - 10, SEEK_CUR);
        put_be24(pb, data_size);
        la_fseek(pb, data_size + 10 - 3, SEEK_CUR);
        put_be32(pb, data_size + 11); // previous tag size
	}

    return 0;
}

flv_t* flv_init( const char *filename, int fps,int width,int height)
{
	flv_t *a = new flv_t();
	memset(a, 0, sizeof(flv_t));
	//add by lostangel 2008.7.24
	a->buff = new char[BUFFMAX];
	a->buff_index = 0;
	//end add
	a->f = fopen(filename,"wb");
	a->i_fps = fps;
	a->i_width = width;
	a->i_height = height;
	a->first_video_ts = 0;
	a->first_audio_ts = 0;
	a->duration = 0;

	a->videoCodec=7;
	a->audioCodec=10;

	flv_write_header( a );

	return a;
}

int flv_write_trailer(flv_t *pb)
{

    int64_t file_size;

//    ByteIOContext *pb = s->pb;
//    FLVContext *flv = s->priv_data;

    file_size = la_ftell(pb);

    // update informations 
    la_fseek(pb, pb->duration_offset, SEEK_SET);
    put_amf_double(pb, pb->duration / (double)1000);
    la_fseek(pb, pb->filesize_offset, SEEK_SET);
    put_amf_double(pb, file_size);

    la_fseek(pb, file_size, SEEK_SET);

	la_flush(pb);
	if(pb->buff)
		delete[] pb->buff;

	fclose(pb->f);

	delete pb;

    return 0;
}

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))

int flv_write_video_packet(flv_t *pb,int isKeyFrame,unsigned char* packetData,int size,unsigned int ts)
{
   
    uint8_t *data= NULL;
    int flags, flags_size;

	if(pb->first_video_ts==0)
		pb->first_video_ts = ts;

//	if(pb->first_audio_ts>0 && pb->first_video_ts > pb->first_audio_ts)
//		pb->first_video_ts = pb->first_audio_ts;

	unsigned int interval_ts = ts-pb->first_video_ts;

//    av_log(s, AV_LOG_DEBUG, "type:%d pts: %"PRId64" size:%d\n", enc->codec_type, timestamp, size);

     flags_size= 5;
     put_byte(pb, FLV_TAG_TYPE_VIDEO);

     flags = pb->videoCodec; //7;
     flags |= isKeyFrame ? FLV_FRAME_KEY : FLV_FRAME_INTER; //interframe:27  keyfame:17

     /* check if extradata looks like mp4 formated */
	 data = new unsigned char[size+10];
     //if (ff_avc_parse_nal_units_buf(packetData, data, &size) < 0)
     //   return -1;
/*
     if (!flv->delay && pkt->dts < 0)
         flv->delay = -pkt->dts;
*/
//    int ts = 0;//pkt->dts + flv->delay; // add delay to force positive dts
    put_be24(pb,size + flags_size);
    put_be24(pb,interval_ts);
    put_byte(pb,(interval_ts >> 24) & 0x7F); // timestamps are 32bits _signed_
    put_be24(pb, 0/*flv->reserved*/);
    put_byte(pb,flags);
    
    put_byte(pb,1); // AVC NALU
    put_be24(pb,0/*pkt->pts - pkt->dts*/);

//    put_buffer(pb, data ? data : pkt->data, size);
	//put_buffer(pb,data,size); //将视频数据写入文件
	put_buffer(pb, packetData, size); //将视频数据写入文件

    put_be32(pb,size+flags_size+11); // previous tag size

	la_flush(pb);

	delete data;
	data=NULL;

    pb->duration = FFMAX(pb->duration, ts - pb->first_video_ts);

    return 0;
}

int flv_write_audio_packet(flv_t *pb,unsigned char* packetData,int size,unsigned int ts)
{
	uint8_t *data= NULL;
    int flags, flags_size;

	data=packetData;

	if(pb->first_audio_ts==0)
		pb->first_audio_ts = ts;

	unsigned int interval_ts = ts - pb->first_audio_ts;

	flags_size= 2;
    put_byte(pb, FLV_TAG_TYPE_AUDIO);

	flags=FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO; //0xAF

	put_be24(pb,size + flags_size);
    put_be24(pb,interval_ts);
    put_byte(pb,(interval_ts >> 24) & 0x7F); // timestamps are 32bits _signed_
    put_be24(pb, 0/*flv->reserved*/);
    put_byte(pb,flags);

	put_byte(pb,1); // AAC raw

	put_buffer(pb,data,size); //将音频数据写入文件
    put_be32(pb,size+flags_size+11); // previous tag size

//	pb->duration = FFMAX(pb->duration, ts - pb->first_audio_ts);

	la_flush(pb);

	return 0;
}