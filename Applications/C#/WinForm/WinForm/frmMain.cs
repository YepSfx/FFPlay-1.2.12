using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Threading;
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace WinForm
{
    public partial class frmMain : Form
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnExitCallback(IntPtr sender, int exitCode);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnInfoCallback(IntPtr sender, int infoCode, IntPtr Message);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnAudioCallback(IntPtr sender, IntPtr AudioBuffer, int BufferLengthInByte);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnVideoCallback(IntPtr sender, IntPtr yuvData);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnVideoResizeCallback(IntPtr sender, int width, int height);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnPlayStatusCallback(IntPtr sender, int status);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int InitilizeDLL();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void FinalizeDLL();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int SetupYuvPlayer(IntPtr sender,
                      [MarshalAs(UnmanagedType.U4)]UInt32 yuvHandle,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnInfoCallback eventInfo,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnExitCallback eventExit,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnAudioCallback eventAudio,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnVideoResizeCallback eventResize,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnPlayStatusCallback eventStatus);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int SetupRgbPlayer(IntPtr sender,
                      [MarshalAs(UnmanagedType.U4)]UInt32 yuvHandle,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnInfoCallback eventInfo,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnExitCallback eventExit,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnAudioCallback eventAudio,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnVideoCallback eventVideo,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnVideoResizeCallback eventResize,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnPlayStatusCallback eventStatus);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetPlayerFileName(IntPtr fileName);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr GetPlayerFileName();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int OpenPlayerStream();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void StartPlayer();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void StopPlayer();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void PauseResumePlayer();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ClearPlayer();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int IsPlayerRunning();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetPlayerStatus();

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void GetPlayerVideoFormat(ref int width, ref int height);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void GetPlayerAudioFormat(ref int freq, ref int channels, ref ushort format, ref ushort samplesinbuffer, ref ulong buffersizeinbyte);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ConvertYUV420pToRGB32(
                                  IntPtr yuvData,
                                  IntPtr rgbData,
                                  ref int width, ref int height);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ConvertYUV420pToRGB24(
                                  IntPtr yuvData,
                                  IntPtr rgbData,
                                  ref int width, ref int height);

        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetPlayerResizeScreen(int width, int height);

        static public void OnExit(IntPtr sender, int exitCode)
        {
            GCHandle mhWnd = GCHandle.FromIntPtr(sender);
            frmMain frm = (frmMain)mhWnd.Target;
            string debug = @"->> Player Exit: " + exitCode.ToString();
            Trace.WriteLine(@"->> Player Exit: " + exitCode.ToString());
            frm.UpdateDebugString(debug);
        }

        static public void OnInfo(IntPtr sender, int infoCode, IntPtr Message)
        {
            GCHandle mhWnd = GCHandle.FromIntPtr(sender);
            frmMain frm = (frmMain)mhWnd.Target;
            string msg = Marshal.PtrToStringAnsi(Message);
            string debug = @"->> Player Info: " + infoCode.ToString() + @" " + msg;
            Trace.WriteLine(@"->> Player Info: " + infoCode.ToString() + @" " + msg);
            frm.UpdateDebugString(debug);
        }

        static public void OnAudio(IntPtr sender, IntPtr AudioBuffer, int BufferLengthInByte)
        {
            //Trace.WriteLine(@"->> Audio event");
        }

        static public void OnVideo(IntPtr sender, IntPtr yuvData)
        {
            //Trace.WriteLine(@"->> Video event");
        }

        static public void OnVideoResize(IntPtr sender, int width, int height)
        {

        }

        static public void OnPlayStatus(IntPtr sender, int status)
        {
            GCHandle mhWnd = GCHandle.FromIntPtr(sender);
            frmMain frm = (frmMain)mhWnd.Target;
            string debug = @"->> Player status: " + status.ToString();
            Trace.WriteLine(@"->> Player status: " + status.ToString());
            frm.UpdateDebugString(debug);
        }

        private Thread mPlayingThread;
        private GCHandle mForm;
        public frmMain()
        {
            InitializeComponent();
            mPanelYUV.Enabled = true;
            mPanelYUV.Visible = true;
            mForm = GCHandle.Alloc(this);
        }

        public delegate void InvokeMethod(string msg);

        public void UpdateDebugString(string msg)
        {
            listBoxDebug.BeginInvoke(new InvokeMethod(UpdateDebugMethod), new object[] { msg });
        }

        private void UpdateDebugMethod(string msg)
        {
            listBoxDebug.Items.Add(msg);
            listBoxDebug.SelectedIndex = listBoxDebug.Items.Count - 1;
        }
        
        private void frmMain_Load(object sender, EventArgs e)
        {
            int rtn = InitilizeDLL();
            if (rtn != 0)
            {
                MessageBox.Show(@"Cannot import DLL");
            }
            Trace.WriteLine(@">> WinForm starts!");
        }

        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (IsPlayerRunning() == 1)
                        StopPlayingThread();

            Trace.WriteLine(@">> WinForm Closing!");
        }

        private void frmMain_FormClosed(object sender, FormClosedEventArgs e)
        {
            FinalizeDLL();
            Trace.WriteLine(@">> WinForm Closed!");
            mForm.Free();
        }

        private void StartPlayingThread()
        {
            mPlayingThread = new Thread(new ThreadStart(StartPlayer));
            mPlayingThread.Start();
        }
        
        private void StopPlayingThread()
        {
            StopPlayer();
            mPlayingThread.Join();
        }

        OnExitCallback OnExitCB = OnExit;
        OnInfoCallback OnInfoCB = OnInfo;
        OnAudioCallback OnAudioCB = OnAudio;
        OnVideoCallback OnVideoCB = OnVideo;
        OnVideoResizeCallback OnResizeCB = OnVideoResize;
        OnPlayStatusCallback OnPlayStatusCB = OnPlayStatus;

        private void mButtonPlay_Click(object sender, EventArgs e)
        {
            IntPtr self = (IntPtr)mForm;
            UInt32 hYuv = (UInt32)mPanelYUV.Handle;

            string FileName = "", verName = "";
            int rtn;

            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                FileName = openFileDialog.FileName;
                IntPtr fileNamePtr;
                fileNamePtr = (IntPtr)Marshal.StringToHGlobalAnsi(FileName);
                SetPlayerFileName(fileNamePtr);
                verName = Marshal.PtrToStringAnsi( GetPlayerFileName() );
                Trace.WriteLine(@">> WinForm FileName: " + verName);

                rtn = SetupYuvPlayer(self, hYuv, OnInfoCB, OnExitCB, OnAudioCB, OnResizeCB, OnPlayStatusCB);
                if (rtn != 0)
                {
                    ClearPlayer();
                    MessageBox.Show(@"Cannot set up the player!");
                    return;
                }

                if (OpenPlayerStream() != 0)
                {
                    ClearPlayer();
                    MessageBox.Show(@"Cannot open the stream!");
                    return;

                }
                StartPlayingThread();
                SetPlayerResizeScreen(mPanelYUV.Width, mPanelYUV.Height);
            }
        }

        private void mButtonStop_Click(object sender, EventArgs e)
        {
            if (IsPlayerRunning() == 1)
                StopPlayingThread();
        }

        private void mButtonPause_Click(object sender, EventArgs e)
        {
            PauseResumePlayer();
        }
    }
}
