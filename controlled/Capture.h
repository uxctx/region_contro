#pragma once
#include "CaptureCommon.h"
#include <functional>

enum CAPTURE_RETURN
{
	RETURN_SUCCESS,
	CREATEDC_FAILED,
	BITBLT_FAILED,
};

class Capture
{
private:
	HDC src_dc;
	HDC capture_dc;
	HBITMAP capture_bitmap;
	HGDIOBJ old_bmp;

	Monitor monitor;

	bool capture_cursor;
	CURSORINFO cursor_info;
	BOOL cursor_captured;
	RECT capt_rect;


public:
	Capture();
	~Capture();
	
	CAPTURE_RETURN PartialInit(const Monitor&,bool capture_cursor);

	CAPTURE_RETURN PartialProcessFrame(void* frame_buffer);

	void PartialFree();

};

