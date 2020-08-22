using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace WindowsFormsApp
{
    class ControlRemote
    {
        /// <summary>
        /// 接收socket数据错误
        /// </summary>
        public const int ERROR_CODE_RECV = (0x0101);

        /// <summary>
        /// 包开始标志错误
        /// </summary>
        public const int ERROR_CODE_START_SING = (0x0102);

        //_READY回复错误
        public const int ERROR_CODE_READY_PEPLY = (0x0103);

        public const int WARNING_CODE_TYPE_NOT_SCREEN = (0x0104);

        public const int ERROR_CODE_DECODE = (0x0201);
        public const int ERROR_CODE_DECODE_PACKET_ERROR = (0x0202);

        /// <summary>
        /// 创建sdl错误
        /// </summary>
        public const int ERROR_CODE_CREATE_SDLWINDOW = (0x0301);
        /// <summary>
        /// 创建sdl错误
        /// </summary>
        public const int ERROR_CODE_CREATE_SDLRENDERER = (0x0302);


        /// <summary>
        /// 开始控制的回调
        /// </summary>
        /// <param name="video_w">视频宽</param>
        /// <param name="video_h">视频高</param>
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void StartCall(int video_w, int video_h);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void DisconnectCall();

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ErrorMessageCall(int error_code, string message);

        /// <summary>
        /// 程序启动时候调用
        /// </summary>
        /// <returns></returns>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int DLLInit();

        /// <summary>
        /// 创建一个上下文
        /// </summary>
        /// <param name="hwnd">窗口句柄</param>
        /// <param name="socket">socket 句柄</param>
        /// <param name="startCall">开始回调</param>
        /// <param name="disconnectCall">断开回调</param>
        /// <param name="errorMessageCall">错误消息回调</param>
        /// <returns></returns>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern System.IntPtr NewContext(IntPtr hwnd, IntPtr socket, 
            StartCall startCall, DisconnectCall disconnectCall, ErrorMessageCall errorMessageCall);

        /// <summary>
        /// 删除
        /// </summary>
        /// <param name="context"></param>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void DeleteStopContext(System.IntPtr context);


        /// <summary>
        /// 开始线程
        /// </summary>
        /// <param name="context"></param>
        /// <returns></returns>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int StartWork(System.IntPtr context);


        /// <summary>
        ///  该函数作用是，是否只解码 而不显示图像
        ///  原因如下，winform窗口最小化的时候，绘制会阻塞，第二次还原窗口时候 画面会从最小化时刻的画面开始绘制，
        ///  所以，最小化的时候，设置ishow为0，保证最小化时候，绘制线程不会阻塞，从最小化还原窗口，画面不会延迟！！
        ///  endraw 0标识不绘制， 大于0 绘制
        /// </summary>
        /// <param name="context"></param>
        /// <param name="endraw"></param>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void EnableDraw(System.IntPtr context,int endraw);


        /// <summary>
        /// 发送鼠标位置
        /// </summary>
        /// <param name="context"></param>
        /// <param name="x"></param>
        /// <param name="y"></param>
        /// <returns></returns>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int SendCursorPos(System.IntPtr context, int x, int y);

        /// <summary>
        /// 
        /// </summary>
        /// <param name="context"></param>
        /// <param name="action">动作</param>
        /// <param name="wheel">鼠标滚轮数值</param>
        /// <returns></returns>
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int SendMouseEvent(System.IntPtr context, UInt32 action, int wheel);

        //发送键盘位置
        [DllImport("ControlRemote.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern int SendKeyEvent(System.IntPtr context, int VK, bool down);


        //鼠标移动
        public const UInt32 WM_MOUSEMOVE = 0x0200;

        //左键抬起按下
        public const UInt32 WM_LBUTTONDOWN = 0x0201;
        public const UInt32 WM_LBUTTONUP = 0x0202;

        //右键抬起按下
        public const UInt32 WM_RBUTTONDOWN = 0x0204;
        public const UInt32 WM_RBUTTONUP = 0x0205;

        //滚轮
        public const UInt32 WM_MBUTTONDOWN = 0x0207;
        public const UInt32 WM_MBUTTONUP = 0x0208;

        //滚轮
        public const UInt32 WM_MOUSEWHEEL = 0x020A;


        //双击
        public const UInt32 WM_LBUTTONDBLCLK = 0x0203;
        public const UInt32 WM_RBUTTONDBLCLK = 0x0206;



    }
}
