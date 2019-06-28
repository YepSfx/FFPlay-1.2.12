using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Threading;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Windows.Interop;
using System.Windows.Forms;

namespace Wpf
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    /// 
    public partial class MainWindow : Window
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
		public static extern int  StartThreadPlayer();
        
        [DllImport(@".\FFP_decoder.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void StopThreadPlayer();

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
            MainWindow wnd = (MainWindow)mhWnd.Target;
            string debug = @"->> Player Exit: " + exitCode.ToString();
            Trace.WriteLine(@"->> Player Exit: " + exitCode.ToString());
            wnd.UpdateDebugString(debug);
        }

        static public void OnInfo(IntPtr sender, int infoCode, IntPtr Message)
        {
            GCHandle mhWnd = GCHandle.FromIntPtr(sender);
            MainWindow wnd = (MainWindow)mhWnd.Target;
            string msg = Marshal.PtrToStringAnsi(Message);
            string debug = @"->> Player Info: " + infoCode.ToString() + @" " + msg;
            Trace.WriteLine(@"->> Player Info: " + infoCode.ToString() + @" " + msg);
            wnd.UpdateDebugString(debug);
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
            MainWindow wnd = (MainWindow)mhWnd.Target;
            string debug = @"->> Player status: " + status.ToString();
            Trace.WriteLine(@"->> Player status: " + status.ToString());
            wnd.UpdateDebugString(debug);
        }

        private System.Windows.Forms.Panel  mYuvPanel;
        private Thread mPlayingThread;
        private GCHandle mForm;
        private Microsoft.Win32.OpenFileDialog mOpenFileDialog;
        public delegate void InvokeMethod(string msg);
        private IntPtr fileNamePtr;

        public void UpdateDebugString(string msg)
        {
            this.Dispatcher.BeginInvoke(new InvokeMethod(UpdateDebugMethod), new object[] { msg });
        }

        private void UpdateDebugMethod(string msg)
        {
            listboxDebug.Items.Add(msg);
            listboxDebug.SelectedIndex = listboxDebug.Items.Count - 1;
        }

        public MainWindow()
        {
            InitializeComponent();
        }
#if true
        private void StartPlayingThread()
        {
            mPlayingThread = new Thread(new ThreadStart(StartPlayer));
            mPlayingThread.Start();
        }

        private void StopPlayingThread()
        {
            StopPlayer();
            mPlayingThread.Join();
            Marshal.FreeHGlobal(fileNamePtr);
        }
#else
        private void StartPlayingThread()
        {
            StartThreadPlayer();
        }

        private void StopPlayingThread()
        {
            StopThreadPlayer();
        }
#endif
        private void mainForm_Loaded(object sender, RoutedEventArgs e)
        {
            int rtn = InitilizeDLL();
            if (rtn != 0)
            {
                System.Windows.MessageBox.Show(@"Cannot import DLL");
            }
            mForm = GCHandle.Alloc(this);
            mOpenFileDialog = new Microsoft.Win32.OpenFileDialog();
            mYuvPanel = new System.Windows.Forms.Panel();
            mHost.Child = mYuvPanel;
            Trace.WriteLine(@">> WPF starts!");
        }

        private void mainForm_Closed(object sender, EventArgs e)
        {
            FinalizeDLL();
            Trace.WriteLine(@">> WPF Closed!");
            mForm.Free();
            mYuvPanel.Dispose();   
        }

        private void mainForm_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (IsPlayerRunning() == 1)
                StopPlayingThread();

            Trace.WriteLine(@">> WPF Closing!");
        }

        private static OnExitCallback OnExitCB = OnExit;
        private static OnInfoCallback OnInfoCB = OnInfo;
        private static OnAudioCallback OnAudioCB = OnAudio;
        private static OnVideoCallback OnVideoCB = OnVideo;
        private static OnVideoResizeCallback OnResizeCB = OnVideoResize;
        private static OnPlayStatusCallback OnPlayStatusCB = OnPlayStatus;

        private void buttonPlay_Click(object sender, RoutedEventArgs e)
        {

            IntPtr self = (IntPtr)mForm;
            UInt32 hYuv = (UInt32)mYuvPanel.Handle;

            string FileName = "", verName = "";
            int rtn;

            if (mOpenFileDialog.ShowDialog() == true)
            {
                FileName = mOpenFileDialog.FileName;

                fileNamePtr = (IntPtr)Marshal.StringToHGlobalAnsi(FileName);
                SetPlayerFileName(fileNamePtr);
                verName = Marshal.PtrToStringAnsi(GetPlayerFileName());
                Trace.WriteLine(@">> WPF FileName: " + verName);

                rtn = SetupYuvPlayer(self, hYuv, OnInfoCB, OnExitCB, OnAudioCB, OnResizeCB, OnPlayStatusCB);
                if (rtn != 0)
                {
                    ClearPlayer();
                    Marshal.FreeHGlobal(fileNamePtr);
                    System.Windows.MessageBox.Show(@"Cannot set up the player!");
                    return;
                }

                if (OpenPlayerStream() != 0)
                {
                    ClearPlayer();
                    Marshal.FreeHGlobal(fileNamePtr);
                    System.Windows.MessageBox.Show(@"Cannot open the stream!");
                    return;

                }

                StartPlayingThread();
                SetPlayerResizeScreen(mYuvPanel.Width, mYuvPanel.Height);
            }

        }

        private void buttonStop_Click(object sender, RoutedEventArgs e)
        {
            if (IsPlayerRunning() == 1)            
                StopPlayingThread();
        }
    }
}
