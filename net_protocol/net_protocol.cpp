
#include "net_protocol.h"

#include <stdio.h>

bool Packet_SendCursorPos(SOCKET con, int x, int y)
{
	struct PacketMouseEventData {
		PacketHead head;
		MouseEventData data;
	};


	PacketMouseEventData pkg;
	pkg.head.type = PACKET_TYPE_MOUSE_EVENT;
	pkg.head.len = sizeof(MouseEventData);
	pkg.head.start_sign = PACKET_START_SING;

	pkg.data.dx = x;
	pkg.data.dy = y;

	pkg.data.action = WM_MOUSEMOVE;
	if (SendN(con, (const char *)&pkg, sizeof(PacketMouseEventData), 0)
		== SOCKET_ERROR) {
		return false;
	}

	return true;
}

bool Packet_SendMouseEvent(SOCKET con, unsigned int action, int wheel)
{
	struct PacketMouseEventData {
		PacketHead head;
		MouseEventData data;
	};

	PacketMouseEventData pkg;
	pkg.head.type = PACKET_TYPE_MOUSE_EVENT;
	pkg.head.len = sizeof(MouseEventData);
	pkg.head.start_sign = PACKET_START_SING;

	pkg.data.wheel = wheel;
	pkg.data.action = action;

	if (SendN(con, (const char *)&pkg, sizeof(PacketMouseEventData), 0)
		== SOCKET_ERROR) {
		return false;
	}

	return true;
}

bool Packet_SendKeyEvent(SOCKET con, int VK, bool down)
{
	struct PacketKeybdEventData {
		PacketHead head;
		KeybdEventData data;
	};


	PacketKeybdEventData pkg;

	pkg.head.type = PACKET_TYPE_KEY_EVENT;
	pkg.head.len = sizeof(KeybdEventData);
	pkg.head.start_sign = PACKET_START_SING;

	pkg.data.VK = VK;
	pkg.data.down = down;
	if (SendN(con, (const char *)&pkg, sizeof(PacketKeybdEventData), 0)
		== SOCKET_ERROR) {
		return false;
	}

	return true;
}




bool Packet_SendScreenPacket(SOCKET con, uint8_t* data, uint32_t size)
{
	PacketHead head;
	head.type = PACKET_TYPE_SCREEN;
	head.len = size;
	head.start_sign = PACKET_START_SING;
	if (SendN(con, (const char *)&head, sizeof(PacketHead), 0) == SOCKET_ERROR) {
		return false;
	}
	if (SendN(con, (const char *)data, size, 0) == SOCKET_ERROR) {
		return false;
	}
	return true;
}

int SendN(SOCKET s, const char *buf, int len, int flags)
{
	int result, old_len = len;
	do {
		result = send(s, buf, len, flags);
		if (result == SOCKET_ERROR) {
			//DebugWSAError();
			return SOCKET_ERROR;
		}
		len -= result;
		buf += result;
	} while (len);
	return old_len;
}

int RecvN(SOCKET s, char *buf, int len, int flags)
{
	int result, old_len = len;
	do {
		result = recv(s, buf, len, flags);
		if (result == SOCKET_ERROR) {
			//DebugWSAError();
			return SOCKET_ERROR;
		}

		len -= result;
		buf += result;
	} while (len);
	return old_len;
}

void DebugWSAError()
{
	int	id = WSAGetLastError();
	switch (id)
	{
	case WSANOTINITIALISED: OutputDebugStringA("not initialized\n"); break;
	case WSASYSNOTREADY: OutputDebugStringA("sub sys not ready\n"); break;
	case WSAHOST_NOT_FOUND: OutputDebugStringA("name server not found\n"); break;
	case WSATRY_AGAIN: OutputDebugStringA("server fail\n"); break;
	case WSANO_RECOVERY: OutputDebugStringA("no recovery\n"); break;
	case WSAEINPROGRESS: OutputDebugStringA("socket blocked by other prog\n"); break;
	case WSANO_DATA: OutputDebugStringA("no data record\n"); break;
	case WSAEINTR: OutputDebugStringA("blocking call canciled\n"); break;
	case WSAEPROCLIM: OutputDebugStringA("limit exceeded\n");
	case WSAEFAULT: OutputDebugStringA("lpWSAData in startup not valid\n");
	default: {
		CHAR sOut[512];
		sprintf_s(sOut, "unknown error %d \n", id);
		OutputDebugStringA(sOut);
	}; break;
	};
}