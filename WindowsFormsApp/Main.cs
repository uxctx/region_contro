using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Net;
using System.Text;
using System.Windows.Forms;

namespace WindowsFormsApp
{
    public partial class Main : Form
    {
        public Main()
        {
            InitializeComponent();

            this.txt_ip.Text = "localhost";

           
        }

        private void btn_connect_Click(object sender, EventArgs e)
        {
            IPAddress ip;
            int port;

            //ip转换
            if (!IPAddress.TryParse(this.txt_ip.Text.Trim(), out ip))
            {
                //ip失败就开始解析域名
                IPHostEntry hostinfo;
                try
                {
                    hostinfo = Dns.GetHostEntry(this.txt_ip.Text.Trim());
                }
                catch (Exception)
                {
                    MessageBox.Show("域名或ip错误");
                    return;
                }

                IPAddress[] aryIP = hostinfo.AddressList;
                if (aryIP.Length <= 0)
                {
                    MessageBox.Show("域名解析失败");
                    return;
                }

                for (int i = 0; i < aryIP.Length; i++)
                {
                    if (aryIP[i].ToString().StartsWith("::"))
                        continue;

                    ip = aryIP[i];
                }

                if (ip == null) {
                    MessageBox.Show("解析域名得到错误IP");
                    return;
                }
            }


            if (!Int32.TryParse(this.txt_port.Text.Trim(), out port))
            {
                MessageBox.Show("错误port");
                return;
            }



            var form = new RemoteDesktop(ip, port);
            this.Hide();
            form.ShowDialog();
            this.WindowState = FormWindowState.Normal;
            this.Show();

        }
    }
}
