#pragma once

#include <stdint.h>
#include <winsock2.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS


#define ERROR_CODE_RECV (0x0101)

#define ERROR_CODE_START_SING (0x0102)

//_READY回复错误
#define ERROR_CODE_READY_PEPLY (0x0103)

#define WARNING_CODE_TYPE_NOT_SCREEN (0x0104)

#define ERROR_CODE_DECODE (0x0201)
#define ERROR_CODE_DECODE_PACKET_ERROR (0x0202)


#define ERROR_CODE_CREATE_SDLWINDOW (0x0301)
#define ERROR_CODE_CREATE_SDLRENDERER (0x0302)




//正式开始控制回调，参数为视频大小
typedef  void(_stdcall *StartCall) (int, int);

//断开回调函数
typedef  void(_stdcall *DisconnectCall) ();

//错误函数
typedef  void (_stdcall *ErrorMessageCall) (int,const char*);

struct Context;

//程序启动调用
 int _stdcall DLLInit();
 
 /*创建一个上下文
 HWND hwnd, 窗口句柄
 SOCKET socket_con, socket 句柄

 StartCall startCall, 开始回调，传入视频高宽
 DisconnectCall disconnectCall, 断开回调
 ErrorMessageCall errorMessageCall 错误消息回调
 */
 void*_stdcall NewContext(HWND hwnd,SOCKET socket_con, 
	 StartCall startCall, DisconnectCall disconnectCall, ErrorMessageCall errorMessageCall);

 //删除并停止这个上下文线程
 void _stdcall DeleteStopContext(Context* context);

 //开始线程
 int _stdcall StartWork(Context* context);


 /*
 该函数作用是，是否只解码 而不显示图像

 原因如下，winform窗口最小化的时候，绘制会阻塞，第二次还原窗口时候  画面会从最小化时刻开始绘制，

 所以，最小化的时候，设置ishow为0，保证最小化时候，绘制线程不会阻塞，从最小化还原窗口，画面不会延迟！！

 endraw 0标识不绘制， 大于0 绘制
 */
 void  _stdcall EnableDraw(Context* context, int endraw);

 //发送鼠标位置
 int _stdcall SendCursorPos(Context* context, int x, int y);

 //发送鼠标点击事件
 int _stdcall SendMouseEvent(Context* context, unsigned int action, int wheel);

 //发送键盘事件
 int _stdcall SendKeyEvent(Context* context, int VK, bool down);


