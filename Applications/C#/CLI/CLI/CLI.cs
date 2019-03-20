using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace CLI
{
    class CLI
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
        public static extern int SetupCLIPlayer(IntPtr sender,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnInfoCallback eventInfo,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnExitCallback eventExit,
                      [MarshalAs(UnmanagedType.FunctionPtr)]OnAudioCallback eventAudio,
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


        static public void OnExit(IntPtr sender, int exitCode)
        {
            Trace.WriteLine(@"->> Player Exit: " + exitCode.ToString());
        }

        static public void OnInfo(IntPtr sender, int infoCode, IntPtr Message)
        {
            string msg = Marshal.PtrToStringAnsi(Message);
            Trace.WriteLine(@"->> Player Info: " + infoCode.ToString() + @" " + msg);
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
            Trace.WriteLine(@"->> Player status: " + status.ToString());
        }

         static void Main(string[] args)
        {
            OnExitCallback OnExitCB             = OnExit;
            OnInfoCallback OnInfoCB             = OnInfo;
            OnAudioCallback OnAudioCB           = OnAudio;
            OnVideoCallback OnvideoCB           = OnVideo;
            OnVideoResizeCallback OnResizeCB    = OnVideoResize;
            OnPlayStatusCallback OnPlayStatusCB = OnPlayStatus;

            string FileName = "", verName = "" ;
            IntPtr fileNamePtr;
            int rtn;

            InitilizeDLL();

            IntPtr self = Marshal.GetFunctionPointerForDelegate(OnExitCB);
            {
                FileName = @".\1.asf";
                fileNamePtr = (IntPtr)Marshal.StringToHGlobalAnsi(FileName); 
                SetPlayerFileName(fileNamePtr);
                verName = Marshal.PtrToStringAnsi(GetPlayerFileName());
                Trace.WriteLine(@">> WinForm FileName: " + verName);

                rtn = SetupCLIPlayer(self, OnInfoCB, OnExitCB, OnAudioCB, OnResizeCB, OnPlayStatusCB);
                if (rtn != 0)
                {
                    ClearPlayer();
                    Trace.WriteLine(@">> Cannot set up the player!");                    
                    return;
                }
                if (OpenPlayerStream() != 0)
                {
                    ClearPlayer();
                    Trace.WriteLine(@"Cannot open the stream!");
                    return;
                }

                try
                {
                    StartPlayer();
                }
                catch
                {
                    Trace.WriteLine(@"Cannot run the stream!");
                }
                Marshal.FreeHGlobal(fileNamePtr);
            }
            FinalizeDLL();
        }
    }
}
