#include "controlled.h"

#include <Windows.h>

#include <tchar.h>
#include <time.h>

static const char *level_names[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void default_log_call(int level, const char *file, int line, const char* fmt, va_list vl)
{
	time_t t = time(NULL);
	tm lt;
	localtime_s(&lt, &t);


	char buf[16];
	buf[strftime(buf, sizeof(buf), "%H:%M:%S", &lt)] = '\0';

	fprintf(stderr, "%s %-5s %s:%d: ", buf, level_names[level], file, line);

	vfprintf(stderr, fmt, vl);

	fprintf(stderr, "\n");
	fflush(stderr);
}

void log_call(int level, const char *file, int line, const char *fmt, ...)
{
	if (log_callback == NULL)
		return;

	va_list args;
	va_start(args, fmt);
	log_callback(level, file, line, fmt, args);
	va_end(args);

}



WCHAR* _FormatErrorMessage(DWORD dwErrCode)
{
	static TCHAR szErrMsg[1024];
	memset(szErrMsg, 0, sizeof(szErrMsg));

	//DWORD dwErrCode = GetLastError(); //之前的错误代码

	LPVOID lpMsgBuf = NULL;
	DWORD dwLen = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&lpMsgBuf,
		0,
		NULL
	);
	if (dwLen == 0)
	{
		DWORD dwFmtErrCode = GetLastError(); //FormatMessage 引起的错误代码
		_stprintf_s(szErrMsg, _T("FormatMessage failed with %u\n"), dwFmtErrCode);
	}

	if (lpMsgBuf)
	{
		_stprintf_s(szErrMsg, _T("Code = %u, Mean = %s"),
			dwErrCode, (LPCTSTR)lpMsgBuf);

		wprintf(szErrMsg);
	}

	if (lpMsgBuf)
	{
		// Free the buffer.
		LocalFree(lpMsgBuf);
		lpMsgBuf = NULL;
	}

	return szErrMsg;
}

void OutLogWSAError()
{
	int	id = WSAGetLastError();
	switch (id)
	{
	case WSANOTINITIALISED:log_debug("not initialized\n"); break;
	case WSASYSNOTREADY: log_debug("sub sys not ready\n"); break;
	case WSAHOST_NOT_FOUND: log_debug("name server not found\n"); break;
	case WSATRY_AGAIN: log_debug("server fail\n"); break;
	case WSANO_RECOVERY:log_debug("no recovery\n"); break;
	case WSAEINPROGRESS:log_debug("socket blocked by other prog\n"); break;
	case WSANO_DATA: log_debug("no data record\n"); break;
	case WSAEINTR:log_debug("blocking call canciled\n"); break;
	case WSAEPROCLIM: log_debug("limit exceeded\n");
	case WSAEFAULT:log_debug("lpWSAData in startup not valid\n");
	default: {
		log_debug("unknown error %d \n", id);
	}; break;
	};
}