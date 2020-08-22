#pragma once

#include "Capture.h"
#include <stdint.h>
extern "C"
{
#include <x264.h>
#include <x264_config.h>
}

#include "pkt_queue.h"
#include "net_protocol.h"

#include <vector>

struct YUV_FRAME
{
	uint8_t *frame_buffer;
	unsigned long ms;
	x264_picture_t pic;
};

struct H264_FRAME
{
	//x264_picture_t pic;

	int     b_keyframe;
	__int64 i_pts;
	__int64 i_dts;

	size_t payload_size;

	x264_nal_t nalt_s[24];
	int nal_count;

	uint8_t* free_buffer;
	uint8_t *frame_buffer;

	unsigned long ms;

};

typedef bool(*check_key) (int VK);

class Work
{
private:
	Monitor monitor;
	Capture capture;
	int fps;

	HANDLE thread_capture;//采集
	HANDLE thread_encoder;//编码
	HANDLE thread_push;//输出
	HANDLE thread_recvctr;//接收控制

	volatile bool run_mark;

	queue_root *yuv_queue;//yuv队列
	HANDLE yuv_queue_event;

	queue_root *h264_queue;
	HANDLE h264_queue_event;


	std::string record_file;

	std::string x264_preset;
	std::string x264_tune;

	int bitrate;

	x264_t* x264;
	x264_param_t param;

	//socket句柄
	SOCKET socket_con;

	HANDLE ctrlth_end_event;

	std::vector<int> blockkeylist;
	//CRITICAL_SECTION lock_blockkeylist;

	void* sps_data;
	int sps_len;
	void* pps_data;
	int pps_len;

public:
	Work();
	~Work();

	/*添加一个拦截按键,不是线程安全函数，请一定要在Start函数之前调用*/
	void AddBlockKey(int vk);

	/*
	const Monitor&, 屏幕数据，其中包括录屏的区域
	int fps, 每秒帧数
	bool capture_cursor,  是否显示鼠标
	int bitrate, 比特率
	std::string& x264_preset,
	std::string& x264_tune,
	check_key ckcall,键盘事件验证，返回false则不处理消息
	std::string& record_file 录制文件
	*
	*/
	bool Init(const Monitor&, int fps, bool capture_cursor, int bitrate, std::string& x264_preset, std::string& x264_tune, std::string& record_file);

	void Start(SOCKET socket_con);

	//手动结束清理
	void StopClear();

	//等待一个结束信号
	void WaitStop();

private:
	static unsigned int __stdcall  CaptureWork(void* arg);
	static unsigned int __stdcall  EncoderWork(void* arg);
	static unsigned int __stdcall  OutPutWork(void* arg);
	static unsigned int __stdcall  RecvCtr(void* arg);

	bool InitH264Encoder();


	void Handle_MouseEvent(MouseEventData& data);
	void Handle_KeyEvent(KeybdEventData& data);
public:

	//ready 交互
	static bool ReadyCheck(SOCKET socket_con, int w, int h);
};

