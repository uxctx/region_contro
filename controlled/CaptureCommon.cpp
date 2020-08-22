#include "controlled.h"
#include "CaptureCommon.h"
#include <Windows.h>


BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor,
	HDC      hdcMonitor,
	LPRECT   lprcMonitor,
	LPARAM   dwData)
{

	auto ret= (std::vector<Monitor>*)dwData;

	Monitor monitor = {};
	monitor.width =std::abs(lprcMonitor->right - lprcMonitor->left);
	monitor.height = std::abs(lprcMonitor->bottom - lprcMonitor->top);
	monitor.left = lprcMonitor->left;
	monitor.top = lprcMonitor->top;

	//monitor.hdcMonitor = hdcMonitor;

	MONITORINFOEX  info;
	info.cbSize = sizeof(info);
	if (!GetMonitorInfo(hMonitor, (LPMONITORINFO)&info)){
		ret->push_back(monitor);
		return TRUE;
	}

	lstrcpyW(monitor.name,info.szDevice);
	ret->push_back(monitor);

	return TRUE;  
}

std::vector<Monitor>  GetMonitors()
{
	std::vector<Monitor> ret;
	//HDC hDesktopDC = GetDC(NULL);
	//EnumDisplayMonitors(hDesktopDC, NULL, MonitorEnumProc, (LPARAM)&ret);
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&ret);
	//DeleteDC(hDesktopDC);
	return ret;
}

