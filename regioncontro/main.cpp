#include "app.h"
#include <controlled.h>
#include "cmdline.h"
#include "keyenum.h"

#pragma comment(lib,"WINMM.LIB")

#pragma comment(lib,"x264.lib")
#pragma comment(lib,"libmp4v2.lib")
#pragma comment(lib,"yuv.lib")


int main(int argc, char *argv[])
{
	WORD Version;
	WSADATA wsaData;
	int err;
	Version = MAKEWORD(1, 1);
	err = WSAStartup(Version, &wsaData);
	if (err != 0) {
		printf("WSAStartup ERROR!\n");
		WSACleanup();
		return 0;
	}


	//准备参数

	int capture_x;
	int capture_y;
	int capture_w;
	int capture_h;

	int fps = 24;
	int bitrate = 1300;
	std::string x264_preset;
	std::string x264_tune;

	int net_port = 9090;
	std::string record_file;

	SOCKET socket_con = NULL;


	cmdline::parser a;


	//坐标
	a.add<int>("x", 'x', "x", true);
	a.add<int>("y", 'y', "y", true);
	a.add<int>("w", 'w', "w", true);
	a.add<int>("h", 'h', "h", true);


	a.add<int>("fps", 'f', "fps(帧率 可选项)", false, fps);

	a.add<int>("bitrate", 'b', "bitrate(码率 可选项)", false, bitrate);

	a.add<std::string>("record", 'r', "录制文件 可选项", false, "out.flv");


	a.add<std::string>("x264_preset", 'p', "264编码的preset参数,可选项", false, "ultrafast",
		cmdline::oneof<std::string>("ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow", "placebo"));

	a.add<std::string>("x264_tune", 't', "264编码的tune参数,可选项", false, "zerolatency",
		cmdline::oneof<std::string>("film", "animation", "grain", "stillimage", "psnr", "ssim", "fastdecode", "zerolatency"));


	a.add("help", 0, "参见文档");

	a.parse_check(argc, argv);


	if (!a.exist("x") ||
		!a.exist("y") ||
		!a.exist("w") ||
		!a.exist("h")
		) {
		printf("没有指定区域[x,y,w,h]\n");
		return 0;
	}


	capture_x = a.get<int>("x");
	capture_y = a.get<int>("y");
	capture_w = a.get<int>("w");
	capture_h = a.get<int>("h");


	if (a.exist("fps")) {
		fps = a.get<int>("fps");

		if (fps < 8)
			fps = 8;
		else if (fps > 35)
			fps = 35;
	}
	if (a.exist("bitrate")) {
		bitrate = a.get<int>("bitrate");
		if (bitrate < 300)
			bitrate = 300;
		else if (bitrate > 9000)
			bitrate = 9000;
	}
	if (a.exist("record")) {
		record_file = a.get<std::string>("record");
	}

	x264_preset = a.get<std::string>("x264_preset");


	x264_tune = a.get<std::string>("x264_tune");



	std::vector<Monitor> monitors = GetMonitors();
	if (monitors.size() <= 0) {
		printf("屏幕信息获取失败!\n");
		return 0;
	}
	std::cout << "Monitor：" << std::endl;

	int count = monitors.size();
	for (int i = 0; i < count; i++) {
		auto &m = monitors[i];
		std::cout << "\t[" << i << "] ";
		std::wcout << m.name;
		std::cout << " " << m.width << " x " << m.height << std::endl;
	}


	Monitor capture_monitor = monitors[0];
	printf("\tselect monitor [0]!\n");

	//设置要采集的区域
	capture_monitor.capture_x = capture_x;
	capture_monitor.capture_y = capture_y;
	capture_monitor.capture_w = capture_w;
	capture_monitor.capture_h = capture_h;

	if ((capture_monitor.capture_x + capture_monitor.capture_w) > capture_monitor.width
		|| (capture_monitor.capture_y + capture_monitor.capture_h) > capture_monitor.height) {

		printf("\tRECT[x,y,w,h] 已经超出显示器范围！！\n");

		printf("x:%d y:%d w:%d h:%d => monitor_width:%d monitor_height:%d \n",
			capture_monitor.capture_x,
			capture_monitor.capture_y,
			capture_monitor.capture_w,
			capture_monitor.capture_h,
			capture_monitor.width,
			capture_monitor.height
		);

		return 0;
	}

	printf("(%d,%d)----------------------------\n", capture_monitor.capture_x, capture_monitor.capture_y);
	printf("     |                             |\n");
	printf("     |                             |\n");
	printf("     |                             |(%d)\n", capture_monitor.capture_h);
	printf("     |                             |\n");
	printf("     |                             |\n");
	printf("     -------------(%d)-------------\n", capture_monitor.capture_w);

	printf("\t fps : %d \n", fps);
	printf("\t bitrate : %d \n", bitrate);

	if (!record_file.empty())
		printf("\t record : %s \n", record_file.c_str());

	printf("\t x264_preset : %s \n", x264_preset.c_str());
	printf("\t x264_tune : %s \n", x264_tune.c_str());


RE_ACCEPT:

	SOCKET  socketser = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in saddr;
	saddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(net_port);
	if (bind(socketser, (sockaddr*)& saddr, sizeof(sockaddr)) != 0) {
		printf("bind失败，端口%d可能被占用\n", net_port);
		return 0;
	}
	listen(socketser, 1);

	sockaddr_in saddrclient;
	int len = sizeof(sockaddr);

	printf("listen port %d ……\n", net_port);

	int on = 1;
	printf("accept ……\n");
	socket_con = accept(socketser, (sockaddr*)&saddrclient, &len);

	//链接成功，直接关闭服务器，下次goto RE_ACCEPT重启服务器
	closesocket(socketser);

	setsockopt(socket_con, SOL_SOCKET, SO_KEEPALIVE, (const char *)&on, sizeof(on));
	setsockopt(socket_con, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(on));//关闭Nagle算法


	if (!Work::ReadyCheck(socket_con, capture_monitor.capture_w , capture_monitor.capture_h)) {
		closesocket(socket_con);
		goto RE_ACCEPT;
	}

	printf("connection succee,start work ……\n");

	Work *work = new Work();

	if (!work->Init(capture_monitor, fps, true, bitrate, x264_preset, x264_tune, record_file)) {
		printf("Work.Init False\n");
		delete work;
		return 0;
	}

	work->AddBlockKey(Keys::LWin);

	work->AddBlockKey(Keys::F1);

	work->Start(socket_con);

	//等待结束
	work->WaitStop();

	work->StopClear();
	delete work;

	closesocket(socket_con);
	goto RE_ACCEPT;

	return 0;
}