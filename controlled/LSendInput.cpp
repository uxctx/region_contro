#include "controlled.h"
#include "LSendInput.h"

static bool shift_down = false;
void LMouseEvent(MouseEventData& data)
{
	INPUT inp;
	inp.type = INPUT_MOUSE;
	inp.mi.mouseData = 0;
	//auto scx = (float)GetSystemMetrics(SM_CXSCREEN);
	//auto scy = (float)GetSystemMetrics(SM_CYSCREEN);

	//auto divl = (float)h.pos.left;
	//auto divt = (float)h.pos.top;
	//inp.mi.dx = (LONG)((65536.0f / scx)*divl);//x being coord in pixels
	//inp.mi.dy = (LONG)((65536.0f / scy)*divt);//y being coord in pixels

	if (data.action == WM_MOUSEMOVE) {
		//inp.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE; 

		//鼠标移动，不用SendInput处理了，因为需要计算上面的浮点数计算
		SetCursorPos(data.dx, data.dy);
		return;
	}
	else if (data.action == WM_LBUTTONDOWN) inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	else if (data.action == WM_LBUTTONUP) inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	else if (data.action == WM_RBUTTONDOWN) inp.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	else if (data.action == WM_RBUTTONUP) inp.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	else if (data.action == WM_MBUTTONDOWN) inp.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
	else if (data.action == WM_MBUTTONUP) inp.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
	else if (data.action == WM_MOUSEWHEEL) {
		inp.mi.dwFlags = MOUSEEVENTF_WHEEL;
		inp.mi.mouseData = data.wheel;
	}

	//双击，发送两次
	if (data.action == WM_LBUTTONDBLCLK) {
		inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
		//mouse_event(inp.mi.dwFlags, inp.mi.dx, inp.mi.dy, 0, 0);
		SendInput(1, &inp, sizeof(inp));
	}
	else if (data.action == WM_RBUTTONDBLCLK) {
		inp.mi.dwFlags = MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_RIGHTDOWN;
		//mouse_event(inp.mi.dwFlags, inp.mi.dx, inp.mi.dy, 0, 0);
		SendInput(1, &inp, sizeof(inp));
	}
	//mouse_event(inp.mi.dwFlags, inp.mi.dx, inp.mi.dy, inp.mi.mouseData, 0);
	SendInput(1, &inp, sizeof(inp));

}
void LKeyEvent(KeybdEventData& data)
{
	//INPUT inp;
    //inp.type = INPUT_KEYBOARD;
    //inp.ki.wVk = data.VK;
    //inp.ki.dwFlags = data.down ? 0 : KEYEVENTF_KEYUP;
    //SendInput(1, &inp, sizeof(INPUT));

	if (data.VK == VK_SHIFT) {
		shift_down = data.down;
		keybd_event(data.VK, MapVirtualKey(data.VK, 0), data.down ? 0 : KEYEVENTF_KEYUP, 0);
		return;
	}

	if (shift_down){
		keybd_event(data.VK, MapVirtualKey(data.VK, 0), data.down ? (KEYEVENTF_EXTENDEDKEY | 0) : (KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP), 0);
	}
	else {
		keybd_event(data.VK, MapVirtualKey(data.VK, 0), data.down ? 0 : KEYEVENTF_KEYUP, 0);
	}

}