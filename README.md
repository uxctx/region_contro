#### regioncontro 是一个以前做的一个 自定义远程桌面控制项目，开源出来。自定义通讯协议，将桌面数据编码为h264，同时控制端发出鼠标键盘信号在被控制端重现，默认工作端口tcp9090。rgioncontro.exe支持xp系统。

```
如何运行：

被控制端启动，最好是两台主机或者将gioncontro.exe运行在虚拟机
\bin\Release\regioncontro.exe --x 0 --y 0 --w 200 --h 200

 --x 0 --y 0 --w 200 --h 200 代表被控制端可见的的桌面区域


控制端
\Release\WindowsFormsApp.exe
```

## regioncontro 项目文档




####  原理

被控端 录屏 编码h264 发送到控制端 =》 控制端接收解码显示

控制端接收并模拟鼠标键盘消息  《= 控制端发送鼠标键盘消息  


####  源码项目描述

controlled 被控制端核心静态lib库，录屏负责编码 处理鼠标键盘消息

regioncontro 调用controlled传参，exe程序

ControlRemote 控制端库，负责接收h264解码，并发送鼠标键盘消息

WindowsFormsApp winfrom控制端，调用ControlRemote.dll

---

#### 被控制端controlled.lib 静态库详情


整个库就一个核心接口是`class Work` 

```

	//const Monitor&, 屏幕数据，其中包括录屏的区域
	//int fps, 每秒帧数
	//bool capture_cursor,  是否显示鼠标
	//int bitrate, 比特率
	//std::string& x264_preset, 
	//std::string& x264_tune, 
	//check_key ckcall,键盘事件验证，返回false则不处理消息，用与拦截某些按键
	//std::string& record_file 录制文件

	//初始化函数
	bool Init(const Monitor&,int fps, bool capture_cursor, int bitrate, std::string& x264_preset, std::string& x264_tune, check_key ckcall,std::string& record_file);
	
	//开始函数
	void Start(SOCKET socket_con);

	//手动结束清理
	void StopClear();

	//等待一个结束信号
	void WaitStop();
```

调用流程
```
Init 初始化 -> Start 开始 ->  StopClear 结束

WaitStop 函数可以等待recv线程结束

```

引用库：

`libx264-148.dll` 此库为x264编码库

`libyuv` 此静态库是yuv和rgba快速转换库，为谷歌开源项目


源码解析：

`CaptureCommon.cpp` 为获取屏幕大小

`Capture.cpp` 截屏功能 核心函数是BitBlt

`flvenc.cpp` 保存h264为flv文件的功能

`LSendInput.cpp` 向本机发送鼠标键盘消息

`net_protocol.cpp` 简易通讯相关

`pkt_queue.cpp` 一个简单的cas锁队列

`Work.cpp` 核心代码 这个类会开启`四个线程`来工作，分别是，
如下

```
	void CaptureWork();截屏线程
	void EncoderWork();编码线程
	void OutPutWork();发送h264到远端&保存文件线程
	void RecvCtr(); 接收远端鼠标键盘线程 并发送到本地

```

线程之间用event和pkt_queue队列通讯

---

#### 控制端ControlRemote.dll


此库共提供一下几个接口

```
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


 //发送鼠标位置
 int _stdcall SendCursorPos(Context* context, int x, int y);

 //发送鼠标点击事件
 int _stdcall SendMouseEvent(Context* context, unsigned int action, int wheel);

 //发送键盘事件
 int _stdcall SendKeyEvent(Context* context, int VK, bool down);
```

调用流程

```

DLLInit   

NewContext
    |
StartWork 
    |
[SendCursorPos SendMouseEvent SendKeyEvent]  
    |
DeleteStopContext

```

引用库：

`ffmpeg` 解码

`SDL` sdl显示库


源码解析：

核心就一个`ControlRemote.cpp`

接收h264并解码显示

---

#### WindowsFormsApp 说明


控制端ControlRemote.dll只是最底层的功能，实现完整的控制必须依赖前段界面

ControlRemote.dll的线程只负责接收h264解码显示，

发送鼠标键盘消息，是winfrom调用ControlRemote.dll完成，并不在ControlRemote.dll创建的线程里

winform会把自己的某个空间句柄传给ControlRemote.dll 当做显示界面，示例代码是用一个 PictureBox控件来显示

更多请参见源码

---

#### 通讯协议简单描述

简单的通讯协议定义为 `net_protocol.h cpp`文件

一个包分为 包头和正文

包头：起始标志+包类型+正文长度正文

```
struct PacketHead
{
	//起始标志位0xffffffff  uint32 max
	uint32_t start_sign;
	//类型
	uint32_t type;
	//正文长度
	uint32_t len;
};
```

接收这个固定长度头部，开始接收正文长度`uint32_t len;`


`uint32_t type;`是类型，大体分为，屏幕数据包，鼠标键盘消息包等等

在通讯开始的时候会有一个read包发送和接收，参见`Work::ReadyCheck` 

---

##### ffmpeg编译

ffmpeg 和libx264都是开源项目，可以自己百度搜索如何编译，


ffmpeg 编译参数为：

```
./configure --enable-shared --prefix=/ffmpeg/  --disable-everything   --enable-decoder=h264 --enable-parser=h264 --enable-demuxer=h264 --disable-programs --disable-iconv --extra-ldlibflags="-static-libgcc"   --disable-avdevice  --disable-swresample --disable-swscale --disable-postproc --disable-avfilter --disable-avformat 
```

注意，mingw编译的ffmpeg和x264 的lib文件在vs2010下没法用，必须用工具通过dll重新生成lib，否则在vs2010的release模式下编译运行会出错！！



