#include "controlled.h"
#include "Work.h"
#include "LSendInput.h"

//#include "flv.h"


#pragma comment(lib,"x264.lib")

#include <libyuv.h>
#pragma comment(lib,"WINMM.LIB")
#pragma comment(lib,"yuv.lib")


//封装mp4

#include "mp4record.h"


#include <process.h>


#define compare_and_swap(Destination,Comperand,ExChange) \
 (InterlockedCompareExchange(Destination, ExChange, Comperand) == Comperand)

Work::Work()
{
	sps_data = NULL;
	pps_data = NULL;
	sps_len = 0;
	pps_len = 0;
}

Work::~Work()
{

}


void Work::AddBlockKey(int vk)
{
	blockkeylist.push_back(vk);
}


void Work::WaitStop()
{
	WaitForSingleObject((HANDLE)ctrlth_end_event, INFINITE);
}

bool Work::Init(const Monitor& monitor, int fps, bool capture_cursor, int bitrate, std::string& x264_preset, std::string& x264_tune, std::string& record_file)
{
	this->monitor = monitor;
	this->fps = fps;
	this->bitrate = bitrate;
	this->record_file = record_file;

	this->x264_preset = x264_preset;
	this->x264_tune = x264_tune;

	if (!InitH264Encoder()) {
		log_error("InitH264Encoder Failed");
		return false;
	}

	if (capture.PartialInit(monitor, capture_cursor)
		!= RETURN_SUCCESS) {
		log_error("capture.PartialInit Failed");
		return false;
	}

	yuv_queue_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	h264_queue_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	init_queue(&yuv_queue);
	init_queue(&h264_queue);

	ctrlth_end_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	return true;
}

void Work::Start(SOCKET socket_con)
{
	this->socket_con = socket_con;
	run_mark = true;

	thread_capture = (HANDLE)_beginthreadex(NULL, 0, CaptureWork, this, CREATE_SUSPENDED, NULL);
	thread_encoder = (HANDLE)_beginthreadex(NULL, 0, EncoderWork, this, CREATE_SUSPENDED, NULL);
	thread_push = (HANDLE)_beginthreadex(NULL, 0, OutPutWork, this, CREATE_SUSPENDED, NULL);
	thread_recvctr = (HANDLE)_beginthreadex(NULL, 0, RecvCtr, this, CREATE_SUSPENDED, NULL);

	ResumeThread(thread_capture);
	ResumeThread(thread_encoder);
	ResumeThread(thread_push);
	ResumeThread(thread_recvctr);
}


void Work::StopClear()
{
	run_mark = false;
	SetEvent(yuv_queue_event);
	SetEvent(h264_queue_event);

	WaitForSingleObject(thread_capture, INFINITE);
	WaitForSingleObject(thread_encoder, INFINITE);
	WaitForSingleObject(thread_push, INFINITE);
	WaitForSingleObject(thread_recvctr, INFINITE);

	CloseHandle(thread_capture);
	CloseHandle(thread_encoder);
	CloseHandle(thread_push);
	CloseHandle(thread_recvctr);

	YUV_FRAME* yuv_frame = NULL;
	while ((yuv_frame = (YUV_FRAME*)queue_get(yuv_queue)) != NULL) {
		free(yuv_frame->frame_buffer);
		delete yuv_frame;
	}
	H264_FRAME* h264_frame = NULL;
	while ((h264_frame = (H264_FRAME*)queue_get(h264_queue)) != NULL) {
		free(h264_frame->free_buffer);
		delete h264_frame;
	}

	capture.PartialFree();

	CloseHandle(yuv_queue_event);
	CloseHandle(h264_queue_event);

	CloseHandle(ctrlth_end_event);

	free_queue(yuv_queue);
	free_queue(h264_queue);

	x264_encoder_close(x264);
}


unsigned int __stdcall Work::CaptureWork(void* arg)
{
	Work* work = (Work*)arg;
	int video_width = work->monitor.capture_w;
	int video_height = work->monitor.capture_h;

	int  inbrga_size = video_width * video_height * 4;
	uint8_t* inbrga = (uint8_t*)malloc(inbrga_size);

	int uv_w = (video_width + 1) / 2;
	int uv_h = (video_height + 1) / 2;

	int  outi420_size = video_width * video_height + uv_w * uv_h * 2;
	uint8_t * outi420 = NULL;

	unsigned long interval_ms = 1000 / work->fps;
	unsigned long cur_time = timeGetTime();
	unsigned long nextf_time = cur_time + interval_ms;

	unsigned long start_ms = cur_time;
	unsigned long sleep_ms;

	//interval_ms += 4; //误差很大，只有更延时了
	while (work->run_mark) {

		cur_time = timeGetTime();

		if (nextf_time > cur_time) {
			sleep_ms = nextf_time - cur_time;

			if (sleep_ms > 1) {
				if (sleep_ms <= interval_ms) {	/*检测，防止虚拟机上时间函数出错*/
					Sleep(sleep_ms);
				}
				else {
					Sleep(interval_ms);
				}
			}
			else
				Sleep(0);
		}

		nextf_time = cur_time + interval_ms;

		YUV_FRAME* yuv_frame = new YUV_FRAME();
		yuv_frame->ms = cur_time - start_ms;

		if (work->capture.PartialProcessFrame(inbrga)
			!= RETURN_SUCCESS) {
			log_error("capture.PartialProcessFrame Failed");
			break;
		}

		outi420 = (uint8_t*)malloc(outi420_size);
		uint8_t* dst_y = outi420;
		int dst_stride_y = video_width;

		uint8_t* dst_u = dst_y + video_width * video_height;
		int dst_stride_u = uv_w;

		uint8_t* dst_v = dst_u + uv_w * uv_h;
		int dst_stride_v = uv_w;

		int ret = libyuv::ARGBToI420(inbrga, video_width * 4,
			dst_y, dst_stride_y,
			dst_u, dst_stride_u,
			dst_v, dst_stride_v,
			video_width, video_height
		);

		yuv_frame->frame_buffer = outi420;

		x264_picture_init(&yuv_frame->pic);

		//yuv_frame->pic.i_pts = total_frames;
		yuv_frame->pic.img.i_csp = X264_CSP_I420;

		yuv_frame->pic.img.i_plane = 3;

		yuv_frame->pic.img.i_stride[0] = dst_stride_y;
		yuv_frame->pic.img.plane[0] = outi420;

		yuv_frame->pic.img.i_stride[1] = dst_stride_u;
		yuv_frame->pic.img.plane[1] = dst_u;

		yuv_frame->pic.img.i_stride[2] = dst_stride_v;
		yuv_frame->pic.img.plane[2] = dst_v;

		queue_add(work->yuv_queue, yuv_frame);
		SetEvent(work->yuv_queue_event);
	}

	free(inbrga);
	log_info("Capture Thread END. ");
	return 0;
}

unsigned int __stdcall Work::EncoderWork(void* arg)
{
	Work* work = (Work*)arg;

	YUV_FRAME* yuv_frame;

	x264_picture_t pic_out;
	x264_picture_init(&pic_out);

	x264_nal_t* nals;
	int             nal_count;
	int             payload_size;

	H264_FRAME* h264_frame = NULL;

	uint8_t* start_ptr;
	while (work->run_mark) {

	REGET:

		yuv_frame = (YUV_FRAME*)queue_get(work->yuv_queue);
		if (yuv_frame == NULL)
			goto WAIT_TAG;

		x264_picture_init(&pic_out);
		payload_size = x264_encoder_encode(work->x264, &nals, &nal_count, &yuv_frame->pic, &pic_out);
		if (payload_size < 0) {
			log_error("x264_encoder_encode error");
		}

		if (nal_count > 0) {

			h264_frame = new H264_FRAME();

			h264_frame->ms = yuv_frame->ms;

			h264_frame->i_dts = pic_out.i_dts;
			h264_frame->i_pts = pic_out.i_pts;
			h264_frame->b_keyframe = pic_out.b_keyframe;

			h264_frame->payload_size = payload_size;

			h264_frame->nal_count = nal_count;

			//+4 保存mp4
			h264_frame->free_buffer = (uint8_t*)malloc(h264_frame->payload_size + 4);
			h264_frame->frame_buffer = h264_frame->free_buffer + 4;


			start_ptr = nals[0].p_payload;

			memcpy(h264_frame->frame_buffer, start_ptr, h264_frame->payload_size);

			for (int i = 0; i < nal_count; i++) {
				x264_nal_t* nal = nals + i;

				h264_frame->nalt_s[i] = nals[i];
				int offset = nals[i].p_payload - start_ptr;
				h264_frame->nalt_s[i].p_payload = h264_frame->frame_buffer + offset;

			}


			queue_add(work->h264_queue, h264_frame);
			SetEvent(work->h264_queue_event);

		}

		free(yuv_frame->frame_buffer);
		delete yuv_frame;

		goto REGET;
	WAIT_TAG:
		WaitForSingleObject((HANDLE)work->yuv_queue_event, INFINITE);
	}

	while ((yuv_frame = (YUV_FRAME*)queue_get(work->yuv_queue)) != NULL) {
		//
		free(yuv_frame->frame_buffer);
		delete yuv_frame;
	}
	log_info("Encoder Thread END. ");
	return 0;
}

unsigned int __stdcall Work::OutPutWork(void* arg)
{
	Work* work = (Work*)arg;


	H264_FRAME* h264_frame = NULL;
	int ret = 0;

	MP4_CONFIG* mp4_context = NULL;
	if (!work->record_file.empty()) {
		mp4_context = CreateMP4File(work->record_file.c_str(),
			work->fps + 3,
			work->monitor.capture_w, work->monitor.capture_h);
	}


	int queue_size = 0;

	while (work->run_mark) {
	REGET:

		h264_frame = (H264_FRAME*)queue_get(work->h264_queue);
		if (h264_frame == NULL)
			goto WAIT_TAG;

		if (!Packet_SendScreenPacket(work->socket_con, h264_frame->frame_buffer, h264_frame->payload_size)) {
			log_error("SendPacket SocketError,Disconnect...........");

			work->run_mark = false;
			break;
		}

		if (mp4_context != NULL) {
			WriteH264_Data(mp4_context, h264_frame->nalt_s, h264_frame->nal_count,
				h264_frame->payload_size, true,
				MP4_INVALID_DURATION, 0);
		}

		free(h264_frame->free_buffer);
		delete h264_frame;

		goto REGET;

	WAIT_TAG:
		WaitForSingleObject((HANDLE)work->h264_queue_event, INFINITE);
	}

	if (mp4_context != NULL)
		CloseMP4File(mp4_context);

	while ((h264_frame = (H264_FRAME*)queue_get(work->h264_queue)) != NULL) {
		free(h264_frame->free_buffer);
		delete h264_frame;
	}

	log_info("OutPut Thread END. ");
	return 0;
}

unsigned int __stdcall Work::RecvCtr(void* arg)
{
	Work* work = (Work*)arg;
	PacketHead pkghead;

	MouseEventData me_data;
	KeybdEventData ke_data;
	int rn;
	while (work->run_mark) {

		//packet_head
		rn = RecvN(work->socket_con, (char*)& pkghead, sizeof(PacketHead), 0);
		if (rn != sizeof(PacketHead)) {
			OutLogWSAError();
			log_error("recv_n error %d", rn);
			work->run_mark = false;
			break;
		}

		//检查头
		if (pkghead.start_sign != PACKET_START_SING) {
			log_error("start_sign != PACKET_Startsign");
			work->run_mark = false;
			break;
		}

		switch (pkghead.type)
		{
		case  PACKET_TYPE_MOUSE_EVENT:
			rn = RecvN(work->socket_con, (char*)& me_data, sizeof(MouseEventData), 0);
			if (rn != sizeof(MouseEventData)) {
				OutLogWSAError();

				log_error("recv_n error %d", rn);
				work->run_mark = false;
				break;
			}

			work->Handle_MouseEvent(me_data);
			; break;
		case  PACKET_TYPE_KEY_EVENT:
			rn = RecvN(work->socket_con, (char*)& ke_data, sizeof(KeybdEventData), 0);
			if (rn != sizeof(KeybdEventData)) {
				OutLogWSAError();

				log_error("recv_n error %d", rn);
				work->run_mark = false;
				break;
			}
			work->Handle_KeyEvent(ke_data);
			break;
		default:
			break;
		}
	}

	log_info("RecvCtr Thread END. ");

	SetEvent(work->ctrlth_end_event);
	return 0;
}

void Work::Handle_MouseEvent(MouseEventData& data)
{
	data.dx = data.dx + monitor.capture_x;
	data.dy = data.dy + monitor.capture_y;

	if (data.action == WM_MOUSEMOVE)
	{
		//负数
		if (data.dx < monitor.capture_x || data.dy < monitor.capture_y) {
			//log_error("鼠标超出区域，取消响应~");
			return;
		}

		if (((monitor.capture_x + monitor.capture_w) < data.dx
			||
			(monitor.capture_y + monitor.capture_h) < data.dy)) {
			//log_error("鼠标超出区域，取消响应~");
			return;
		}
	}


	LMouseEvent(data);
}

void Work::Handle_KeyEvent(KeybdEventData& data)
{
	size_t len = blockkeylist.size();
	for (size_t i = 0; i < len; i++) {
		if (blockkeylist[i] == data.VK)
			return;
	}

	LKeyEvent(data);

}


bool Work::ReadyCheck(SOCKET socket_con, int w, int h)
{
	struct PacketReadyData
	{
		PacketHead head;
		ReadyData data;
	};

	PacketReadyData readpkg;
	readpkg.head.type = PACKET_TYPE_READY;
	readpkg.head.len = sizeof(ReadyData);
	readpkg.head.start_sign = PACKET_START_SING;

	readpkg.data.video_w = w;
	readpkg.data.video_h = h;
	//发送一个read包
	if (SendN(socket_con, (const char*)& readpkg, sizeof(PacketReadyData), 0)
		!= sizeof(PacketReadyData)) {
		log_error("SendN ERROR");
		return false;
	}

	//接收一个ready 回复包
	PacketHead pkghead;
	if (RecvN(socket_con, (char*)& pkghead, sizeof(PacketHead), 0)
		!= sizeof(PacketHead)) {
		log_error("RecvN ERROR");
		return false;
	}

	//回复包不正确
	if (pkghead.start_sign != PACKET_START_SING) {
		log_error("Recv READY_PEPLY START_SING ERROR");
		return false;
	}
	if (pkghead.type != PACKET_TYPE_READY_PEPLY) {
		log_error("Recv PACKET_TYPE_READY_PEPLY ERROR");
		return false;
	}
	return true;
}

static void log_x264(void* param, int level, const char* format, va_list args)
{
	struct obs_x264* obsx264 = (obs_x264*)param;
	char str[1024];
	vsnprintf(str, 1024, format, args);
	log_debug(str);
}

bool Work::InitH264Encoder()
{
	int ret;

	x264_param_default(&param);

	//int ret = x264_param_default_preset(&param, "veryfast", "zerolatency");
	//ret = x264_param_default_preset(&param, "ultrafast", "zerolatency");

	ret = x264_param_default_preset(&param, x264_preset.c_str(), x264_tune.c_str());

	ret = x264_param_apply_profile(&param, "high");

	param.i_keyint_min = 50;//关键帧最小间隔
	param.i_keyint_max = 500;//关键帧最大间隔

	param.i_width = monitor.capture_w;
	param.i_height = monitor.capture_h;

	param.b_vfr_input = 0;

	param.i_csp = X264_CSP_I420;

	param.rc.i_vbv_max_bitrate = bitrate;

	param.rc.i_vbv_buffer_size = bitrate;
	param.rc.i_bitrate = bitrate;

	param.i_fps_num = fps;//* 帧率分子   
	param.i_fps_den = 1; //* 帧率分母 

	param.pf_log = log_x264;
	param.p_log_private = &param;
	param.i_log_level = X264_LOG_WARNING;

	param.b_deterministic = false;

	param.rc.i_rc_method = X264_RC_ABR;

	param.i_timebase_num = 1;
	param.i_timebase_den = 1000;

	param.rc.f_rf_constant = 0;

	this->x264 = x264_encoder_open(&param);

	if (this->x264 == NULL) {
		log_info("x264_encoder_open == NULL");
		return false;
	}
	return true;
}