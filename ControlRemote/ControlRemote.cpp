// ControlRemote.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "ControlRemote.h"

#include <Windows.h>

extern "C"
{
#include <sdl/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"SDL2.lib")


#include <Winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#include "net_protocol.h"

struct Context
{

	HWND hwnd;

	//ffmpeg相关参数
	AVCodec *codec;
	AVCodecContext *codecCtx;
	AVPixelFormat    g_AVFix;

	HANDLE work_thread;
	volatile bool run_mark;

	SOCKET socket_con;

	StartCall startCall;
	DisconnectCall disconnectCall;
	ErrorMessageCall errorMessageCall;

	//如果为true 标识无效了
	bool invalid;

	volatile bool enableDraw;
};

bool Init(Context *context)
{
	int ret = 0;
	context->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (context->codec == NULL) {
		return false;
	}
	context->codecCtx = avcodec_alloc_context3(context->codec);
	if (context->codecCtx == NULL) {
		return false;
	}

	ret = avcodec_open2(context->codecCtx, context->codec, NULL);
	if (ret != 0) {
		avcodec_free_context(&context->codecCtx);
		return false;
	}
	return true;
}


unsigned int __stdcall RemoteThread(void *args);

int _stdcall DLLInit()
{
	SDL_Init(SDL_INIT_VIDEO);
	avcodec_register_all();
	return 1;
}

void* _stdcall NewContext(HWND hwnd, SOCKET socket_con,
	StartCall startCall, DisconnectCall disconnectCall, ErrorMessageCall errorMessageCall)
{
	Context *context = new Context();

	context->socket_con = socket_con;
	context->hwnd = hwnd;
	context->startCall = startCall;
	context->disconnectCall = disconnectCall;
	context->errorMessageCall = errorMessageCall;
	context->invalid = true;

	context->enableDraw = true;

	if (!Init(context)) {
		delete context;
		return NULL;
	}
	return context;
}

int _stdcall StartWork(Context *context)
{
	context->run_mark = true;
	context->invalid = true;
	context->work_thread = (HANDLE)_beginthreadex(NULL, 0, RemoteThread, (void*)context, CREATE_SUSPENDED, NULL);
	ResumeThread(context->work_thread);
	return 0;
}

void  _stdcall EnableDraw(Context* context, int endraw)
{
	context->enableDraw = endraw > 0 ? true : false;
}

void _stdcall DeleteStopContext(Context* context)
{
	if (context == NULL)
		return;
	context->run_mark = false;
	//closesocket(context->socket_con); winform自己关闭socket
	WaitForSingleObject(context->work_thread, INFINITE);
	CloseHandle(context->work_thread);

	avcodec_close(context->codecCtx);
	avcodec_free_context(&context->codecCtx);


	delete context;
}

int _stdcall SendCursorPos(Context* context, int x, int y)
{
	if (context->invalid)
		return -1;

	if (!Packet_SendCursorPos(context->socket_con, x, y)) {
		return -1;
	}
	return 0;
}

int _stdcall SendMouseEvent(Context* context, unsigned int action, int wheel)
{
	if (context->invalid)
		return -1;

	if (!Packet_SendMouseEvent(context->socket_con, action, wheel)) {
		return -1;
	}
	return 0;
}

int _stdcall SendKeyEvent(Context* context, int VK, bool down)
{
	if (context->invalid)
		return -1;

	if (!Packet_SendKeyEvent(context->socket_con, VK, down)) {
		return -1;
	}
	return 0;
}


unsigned int __stdcall RemoteThread(void *args)
{
	Context* context = (Context*)args;

	SDL_Texture* sdlTexture = NULL;
	SDL_Rect sdlRect;

	int pkg_size = 0;
	int window_w = 0, window_h = 0;

	AVFrame  *yuvFrame = av_frame_alloc();

	PacketHead pkghead;
	int rn = 0;
	SDL_Renderer* renderer = NULL;
	SDL_Window *window = NULL;

	AVPacket packet;
	int on = 1;
	ReadyData readyData;

	//接收缓冲区，大一点
	int rcv_size = 3 * 1024 * 1024;

	window = SDL_CreateWindowFrom(context->hwnd);
	if (window == NULL) {
		context->errorMessageCall(
			ERROR_CODE_CREATE_SDLWINDOW, "SDL_CreateWindowFrom error");
		OutputDebugStringA(SDL_GetError());
		goto WORK_END;
	}

	renderer = SDL_CreateRenderer(window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		renderer = SDL_CreateRenderer(window, -1, 0);
	}
	if (!renderer) {
		context->errorMessageCall(
			ERROR_CODE_CREATE_SDLRENDERER, "SDL_CreateRenderer error");
		goto WORK_END;
	}

	//setsockopt(context->socket_con, SOL_SOCKET, SO_KEEPALIVE, (const char *)&on, sizeof(on));
	setsockopt(context->socket_con, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(on));


	setsockopt(context->socket_con, SOL_SOCKET, SO_RCVBUF, (char *)&rcv_size, sizeof(int));


	//接收一个PACKET_TYPE_READY
	rn = RecvN(context->socket_con, (char *)&pkghead, sizeof(PacketHead), 0);
	if (rn != sizeof(PacketHead)) {
		context->errorMessageCall(ERROR_CODE_RECV, "recv error");
		goto WORK_END;
	}

	//
	if (pkghead.start_sign != PACKET_START_SING) {
		context->errorMessageCall(ERROR_CODE_START_SING, "start_sign error");
		goto WORK_END;
	}

	if (pkghead.type != PACKET_TYPE_READY) {
		context->errorMessageCall(ERROR_CODE_READY_PEPLY, "READY_PEPLY error");
		goto WORK_END;
	}

	//接收PACKET_TYPE_READY主体
	rn = RecvN(context->socket_con, (char *)&readyData, sizeof(ReadyData), 0);
	if (pkghead.len != rn || rn != sizeof(ReadyData)) {
		DebugWSAError();
		context->errorMessageCall(ERROR_CODE_RECV, "recv error");
		goto WORK_END;
	}


	sdlTexture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, readyData.video_w, readyData.video_h);

	if (!sdlTexture) {
		context->errorMessageCall(
			ERROR_CODE_CREATE_SDLRENDERER, "SDL_CreateTexture error");
		goto WORK_END;
	}

	//回复一个PACKET_TYPE_READY_PEPLY
	pkghead.type = PACKET_TYPE_READY_PEPLY;
	pkghead.len = 0;
	pkghead.start_sign = PACKET_START_SING;
	if (SendN(context->socket_con, (const char *)&pkghead, sizeof(PacketHead), 0)
		== SOCKET_ERROR) {
		goto WORK_END;
	}


	context->startCall(readyData.video_w, readyData.video_h);
	context->invalid = false;
	//开始接收视频数据
	while (context->run_mark)
	{
		//packet_head
		rn = RecvN(context->socket_con, (char *)&pkghead, sizeof(PacketHead), 0);
		if (rn != sizeof(PacketHead)) {
			DebugWSAError();

			context->errorMessageCall(ERROR_CODE_RECV, "recv error");
			goto WORK_END;
		}

		//
		if (pkghead.start_sign != PACKET_START_SING) {
			context->errorMessageCall(ERROR_CODE_START_SING, "start_sign error");
			goto WORK_END;
		}


		if (pkghead.type != PACKET_TYPE_SCREEN) {
			context->errorMessageCall(WARNING_CODE_TYPE_NOT_SCREEN, "TYPE NOT IS SCREEN");
			continue;
		}

		pkg_size = pkghead.len;


		av_init_packet(&packet);
		rn=av_new_packet(&packet, pkg_size);
		//接收正文包
		rn = RecvN(context->socket_con, (char *)packet.data, pkg_size, 0);
		if (rn != pkg_size) {
			DebugWSAError();
			context->errorMessageCall(ERROR_CODE_RECV, "recv error");
			goto WORK_END;
		}
		
		//开始解码显示
		rn = avcodec_send_packet(context->codecCtx, &packet);
		av_packet_unref(&packet);

		if (rn != 0) {
			context->errorMessageCall(ERROR_CODE_DECODE_PACKET_ERROR, "avcodec_send_packet != 0");
			continue;
		}

		//获取解码结果显示
		while (avcodec_receive_frame(context->codecCtx, yuvFrame) == 0) {

			if (context->enableDraw) {
				SDL_UpdateYUVTexture(sdlTexture, NULL, yuvFrame->data[0], yuvFrame->linesize[0],
					yuvFrame->data[1], yuvFrame->linesize[1],
					yuvFrame->data[2], yuvFrame->linesize[2]);

				sdlRect.x = 0;
				sdlRect.y = 0;
				sdlRect.w = yuvFrame->width;
				sdlRect.h = yuvFrame->height;

				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, sdlTexture, NULL, &sdlRect);
				//SDL_RenderCopy(renderer, pTexture, NULL, &sdl_rect_pointer);
				SDL_RenderPresent(renderer);
			}
		}

	}

WORK_END:
	context->invalid = true;
	context->run_mark = false;
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	av_frame_free(&yuvFrame);

	context->disconnectCall();

	return 0;
}
