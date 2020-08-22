using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WindowsFormsApp
{
    public partial class RemoteDesktop : Form
    {
        [DllImport("user32.dll", EntryPoint = "ShowCursor")]
        public static extern void ShowCursor(bool status);

        public RemoteDesktop(IPAddress ip, int port)
        {
            InitializeComponent();

            // this.ctrl_panel ，用来显示和接受输入事件

            
            //设置ctrl_panel外观位置
            this.ctrl_pic.BackColor = System.Drawing.SystemColors.Desktop;
            this.ctrl_pic.Location = new System.Drawing.Point(0, 0);
            this.ctrl_pic.Margin = new System.Windows.Forms.Padding(0);


            //鼠标事件
            this.ctrl_pic.MouseWheel += Ctrl_pic_MouseWheel;

            this.ctrl_pic.MouseDown += Ctrl_pic_MouseDown;
            this.ctrl_pic.MouseUp += Ctrl_pic_MouseUp;

            this.ctrl_pic.MouseMove += Ctrl_pic_MouseMove;

            this.ctrl_pic.MouseEnter += Ctrl_pic_MouseEnter;
            this.ctrl_pic.MouseLeave += Ctrl_pic_MouseLeave;

            //键盘事件
            this.KeyDown += RemoteDesktop_KeyDown;
            this.KeyUp += RemoteDesktop_KeyUp;


            //取消多线程检查
            Control.CheckForIllegalCrossThreadCalls = false;

            this.ip = ip;
            this.port = port;

            this.startCall = thisStartCall;
            this.disconnectCall = thisDisconnectCall;
            this.errorMessageCall = thisErrorMessageCall;

            this.Load += RemoteDesktop_Load;
            this.FormClosing += RemoteDesktop_FormClosing;


        }

        bool is_close = false;
        private static readonly object lock_object = new object();
        private void RemoteDesktop_FormClosing(object sender, FormClosingEventArgs e)
        {
            socket.Close();
            socket.Dispose();


            Task.Factory.StartNew(() =>
            {

                lock (lock_object)
                {
                    if (context != System.IntPtr.Zero)
                    {
                        ControlRemote.DeleteStopContext(context);
                        context = System.IntPtr.Zero;
                    }
                    is_close = true;
                }
            });

            if (!is_close) {
                e.Cancel = true;
            }
        }

        private void RemoteDesktop_Load(object sender, EventArgs e)
        {
            this.BeginInvoke(new Action(() => { Init(); }));
        }

        private System.IntPtr context;
        private IPAddress ip;
        private int port;

        /// <summary>
        /// 标记是否已经开始
        /// </summary>
        bool is_start = false;

        /// <summary>
        /// 这个对象一定是存在类里的持久对象，不能是函数里临时变量，会被释放！！
        /// </summary>
        private Socket socket;

        #region 回调函数
        ControlRemote.StartCall startCall;
        ControlRemote.DisconnectCall disconnectCall;
        ControlRemote.ErrorMessageCall errorMessageCall;

        public void thisStartCall(int video_w, int video_h)
        {

            SetMessage("" + video_w + "x" + video_h);

            this.ctrl_pic.Width = video_w;
            this.ctrl_pic.Height = video_h;

            this.SetClientSizeCore(video_w, video_h + this.statusStrip1.Height);
            is_start = true;

        }

        public void thisDisconnectCall()
        {

            SetMessage("已断开");

            //MessageBox.Show("断开");


            //不能直接执行this.Close();，需要异步 因为调用thisDisconnectCall函数的不是ui线程，会造成死锁
            System.Threading.Tasks.Task.Factory.StartNew(() =>
            {
                this.Close();
            });

        }

        public void thisErrorMessageCall(int error_code, string message)
        {
            SetMessage("错误：" + message);
        }

        #endregion


        private void SetMessage(string text)
        {
            this.BeginInvoke(new Action(() =>
            {
                this.toolStripStatusLabel1.Text = this.Text = text;
            }));

        }


        void Init()
        {
            //创建socket
            IPEndPoint ipEnd = new IPEndPoint(this.ip, this.port);
            socket = new Socket(AddressFamily.InterNetwork,
               SocketType.Stream, ProtocolType.Tcp);
            try
            {
                socket.Connect(ipEnd);
                socket.SetSocketOption(SocketOptionLevel.Tcp, SocketOptionName.NoDelay, true);
            }
            catch (SocketException e)
            {
                SetMessage("链接远端失败 " + e.Message);
                return;
            }


            context = ControlRemote.NewContext(this.ctrl_pic.Handle, socket.Handle,
                this.startCall, this.disconnectCall, this.errorMessageCall);
            if (context == System.IntPtr.Zero)
            {
                SetMessage("创建上下文失败！");
                return;
            }

            SetMessage("链接远端成功…… ");

            ControlRemote.StartWork(context);
        }

        #region 鼠标键盘事件处理

        private void Ctrl_pic_MouseLeave(object sender, EventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            ShowCursor(true);
        }

        private void Ctrl_pic_MouseEnter(object sender, EventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            ShowCursor(false);
        }

        private void Ctrl_pic_MouseMove(object sender, MouseEventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            ControlRemote.SendCursorPos(context, e.X, e.Y);

            SetMessage("MouseMove " + e.X + " x " + e.Y);

            //  Debug.WriteLine("MouseMove " + e.X + " x " + e.Y);
        }

        private void Ctrl_pic_MouseUp(object sender, MouseEventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            switch (e.Button)
            {
                case MouseButtons.Left:
                    ControlRemote.SendMouseEvent(context, ControlRemote.WM_LBUTTONUP, 0);
                    break;
                case MouseButtons.None:
                    break;
                case MouseButtons.Right:
                    ControlRemote.SendMouseEvent(context, ControlRemote.WM_RBUTTONUP, 0);
                    break;
                case MouseButtons.Middle:
                    ControlRemote.SendMouseEvent(context, ControlRemote.WM_MBUTTONUP, 0);
                    break;
                case MouseButtons.XButton1:
                    break;
                case MouseButtons.XButton2:
                    break;
                default:
                    break;
            }
        }

        void Re()
        {           
            //重连步骤
            //启动一个非UI线程
            System.Threading.Tasks.Task.Factory.StartNew(() =>
            {
                is_start = false;
                socket.Close();
                socket.Dispose();

                if (context != System.IntPtr.Zero)
                {
                    //用非ui线程去关闭DeleteStopContext
                    ControlRemote.DeleteStopContext(context);
                    context = System.IntPtr.Zero;
                }


                //ui线程 然后用this.Invoke 去创建   this.ctrl_pic
                this.Invoke(new Action(()=> {

                    //加一句
                    this.ctrl_pic.Dispose();

                    this.ctrl_pic = new PictureBox();
                    this.Controls.Add(this.ctrl_pic);
                    //设置ctrl_panel外观位置
                    this.ctrl_pic.BackColor = System.Drawing.SystemColors.Desktop;
                    this.ctrl_pic.Location = new System.Drawing.Point(0, 0);
                    this.ctrl_pic.Margin = new System.Windows.Forms.Padding(0);

                    //鼠标事件
                    this.ctrl_pic.MouseWheel += Ctrl_pic_MouseWheel;

                    this.ctrl_pic.MouseDown += Ctrl_pic_MouseDown;
                    this.ctrl_pic.MouseUp += Ctrl_pic_MouseUp;

                    this.ctrl_pic.MouseMove += Ctrl_pic_MouseMove;

                    this.ctrl_pic.MouseEnter += Ctrl_pic_MouseEnter;
                    this.ctrl_pic.MouseLeave += Ctrl_pic_MouseLeave;

                }));

                //最后init 去链接
                Init();
            });
          

        }

        private void Ctrl_pic_MouseDown(object sender, MouseEventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            switch (e.Button)
            {
                case MouseButtons.Left:
                    ControlRemote.SendMouseEvent(context, ControlRemote.WM_LBUTTONDOWN, 0);
                    break;
                case MouseButtons.None:
                    break;
                case MouseButtons.Right:
                    ControlRemote.SendMouseEvent(context, ControlRemote.WM_RBUTTONDOWN, 0);
                    break;
                case MouseButtons.Middle:
                    ControlRemote.SendMouseEvent(context, ControlRemote.WM_MBUTTONDOWN, 0);
                    break;
                case MouseButtons.XButton1:
                    break;
                case MouseButtons.XButton2:
                    break;
                default:
                    break;
            }

         
        }

        private void Ctrl_pic_MouseWheel(object sender, MouseEventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            ControlRemote.SendMouseEvent(context, ControlRemote.WM_MOUSEWHEEL, e.Delta);


        }

        private void RemoteDesktop_KeyUp(object sender, KeyEventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            ControlRemote.SendKeyEvent(context, (int)e.KeyCode, false);
            SetMessage("KeyUp " + e.KeyCode.ToString() + "(" + (int)e.KeyCode + ")");

            //Debug.WriteLine("KeyUp " + e.KeyCode.ToString() + "(" + (int)e.KeyCode + ")");

            e.Handled = true;

            if (e.KeyCode == Keys.F2)
            {
                Re();
            }

        }

        private void RemoteDesktop_KeyDown(object sender, KeyEventArgs e)
        {
            if (!is_start)
                return;

            if (context == System.IntPtr.Zero)
                return;

            ControlRemote.SendKeyEvent(context, (int)e.KeyCode, true);

            SetMessage("KeyDown " + e.KeyCode.ToString() + "(" + (int)e.KeyCode + ")");

            // Debug.WriteLine("KeyDown " + e.KeyCode.ToString() + "(" + (int)e.KeyCode + ")");

            e.Handled = true;
        }

        #endregion



        const int WM_SYSCOMMAND = 0x112;
        const int SC_CLOSE = 0xF060;
        const int SC_MINIMIZE = 0xF020;
        const int SC_MAXIMIZE = 0xF030;
        const int SC_RESTORE = 61728;
        //拦截最小化事件
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_SYSCOMMAND)
            {
                if (m.WParam.ToInt32() == SC_RESTORE)
                {

                    if (is_start && context != System.IntPtr.Zero)
                    {
                        ControlRemote.EnableDraw(context, 1);//启用绘制
                    }
                    //窗口还原！
                }
                if (m.WParam.ToInt32() == SC_MINIMIZE)
                {
                    if (is_start && context != System.IntPtr.Zero)
                    {
                        ControlRemote.EnableDraw(context, 0);//取消绘制，让线程不阻塞，第二次还原窗口画面不延迟
                    }
                    //"点击最小化按钮！");
                }
                if (m.WParam.ToInt32() == SC_MAXIMIZE)
                {
                    //"点击最大化按钮！");
                }
                if (m.WParam.ToInt32() == SC_CLOSE)
                {

                    //点击窗口关闭按钮！");
                }

            }
            base.WndProc(ref m);
        }
    }
}
