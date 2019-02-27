// FFP_decoder.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <stdio.h>
#include <process.h>

#include <tchar.h>

#define INT64				__int64

	typedef struct {
		int               Freq;
		unsigned char     Channels;
		unsigned short    Format;
		unsigned short    SamplesInBuffer;
		unsigned long     BufferSizeInBytes;
	} FFP_AUD_PARAMS;

	typedef struct{
		int   width;
		int   height;
		int   Bpp;
	} FFP_VID_PARAMS;

	typedef struct {
		int           w;
		int           h;
		unsigned char **pixels;
	}FFP_YUV420P_DATA;

#define FFP_AUDIO_U8	    0x0008	                  /**< Unsigned 8-bit samples */
#define FPP_AUDIO_S8	    0x8008	                  /**< Signed 8-bit samples */
#define FFP_AUDIO_U16LSB	0x0010	                  /**< Unsigned 16-bit samples */
#define FFP_AUDIO_S16LSB	0x8010	                  /**< Signed 16-bit samples */
#define FFP_AUDIO_U16MSB	0x1010	                  /**< As above, but big-endian byte order */
#define FFP_AUDIO_S16MSB	0x9010	                  /**< As above, but big-endian byte order */
#define FFP_AUDIO_U16	    FFP_AUDIO_U16LSB
#define FFP_AUDIO_S16	    FFP_AUDIO_S16LSB

	typedef enum {
		FFP_CLI = 0,
		FFP_GUI
	}FFP_UI_TYPE;

	typedef enum {
		FFP_FALSE = 0,
		FFP_TRUE = 1
	}FFP_BOOL;

	typedef enum {
		FFP_INFO_NONE = 0,
		FFP_INFO_WARNING = 1,
		FFP_INFO_ERROR = 2,
		FFP_INFO_STREAM_ERROR = 3,
		FFP_INFO_DEBUG = 4
	}FFP_INFO;

	typedef enum {
		FFP_STATUS_STOP    = 0,
		FFP_STATUS_PLAY    = 1,
		FFP_STATUS_PAUSED  = 2,
		FFP_STATUS_RESUMED = 3,
		FFP_STATUS_EOF     = 4
	}FFP_PLAY_STATUS;

	typedef void(*FFP_EVENT_EXIT)(void *sender, int exitCode);
	typedef void(*FFP_EVENT_INFO)(void *sender, int infoCode, char *Message);
	typedef void(*FFP_EVENT_AUDIO)(void *sender, unsigned char *AudBuffer, int BufferLengthInByte);
	typedef void(*FFP_EVENT_VIDEO)(void *sender, FFP_YUV420P_DATA *YuvData);
	typedef void(*FFP_EVENT_VIDEORESIZE)(void *sender, int width, int height);
	typedef void(*FFP_EVENT_PLAYSTATUS)(void *sender, FFP_PLAY_STATUS status);

	typedef struct {
		void*                  sender;
		unsigned int           screenID;
		INT64                  duration_in_us;
		double                 current_in_s;
		FFP_UI_TYPE            ui_type;
		FFP_EVENT_INFO         event_info;
		FFP_EVENT_EXIT         event_exit;
		FFP_EVENT_AUDIO        event_audio;
		FFP_EVENT_VIDEO        event_video;
		FFP_EVENT_VIDEORESIZE  event_video_resize;
		FFP_EVENT_PLAYSTATUS   event_play_status;
		FFP_PLAY_STATUS        playstatus;
	}FFP_EVENTS;

	typedef int(*FFP_INIT)(FFP_EVENTS *pEvents);
	typedef FFP_BOOL(*FFP_OPEN)();
	typedef void(*FFP_START)();
	typedef void(*FFP_STOP)();
	typedef void(*FFP_PAUSE)();
	typedef void(*FFP_EXIT)();
	typedef const char*(*FFP_GETNAME)();
	typedef void(*FFP_SETNAME)(const char* pName);
	typedef FFP_AUD_PARAMS*(*FFP_GETAUDFMT)();
	typedef FFP_VID_PARAMS*(*FFP_GETVIDFMT)();
	typedef INT64(*FFP_GETDURATION)();
	typedef int(*FFP_LOOPALIVE)();
	typedef void(*FFP_YUVTORGB24)(FFP_YUV420P_DATA *yuvData, unsigned char* rgbbuffer);
	typedef void(*FFP_YUVTORGB32)(FFP_YUV420P_DATA *yuvData, unsigned char* rgbbuffer);
	typedef void(*FFP_RESIZESCREEN)(int w, int h);
	typedef void(*FFP_CLEARSCREEN)(int screenID, int width, int height);
	typedef void(*FFP_RESETPOINTER)();

#define DEF_FFP_INIT			"multimedia_init_device"
#define DEF_FFP_OPEN			"multimedia_stream_open"
#define DEF_FFP_START			"multimedia_stream_start"
#define DEF_FFP_STOP			"multimedia_stream_stop"
#define DEF_FFP_PAUSE			"multimedia_pause_resume"
#define DEF_FFP_EXIT			"multimedia_exit"
#define DEF_FFP_GETNAME			"multimedia_get_filename"
#define DEF_FFP_SETNAME			"multimedia_set_filename"
#define DEF_FFP_GETAUDFMT		"multimedia_get_audioformat"
#define DEF_FFP_GETVIDFMT		"multimedia_get_videoformat"
#define DEF_FFP_GETDURATION		"multimedia_get_duration_in_mSec"
#define DEF_FFP_LOOPALIVE		"multimedia_event_loop_alive"
#define DEF_FFP_YUVTORGB24		"multimedia_yuv420p_to_rgb24"
#define DEF_FFP_YUVTORGB32		"multimedia_yuv420p_to_rgb32"
#define DEF_FFP_RESIZESCREEN    "multimedia_resize_screen"
#define DEF_FFP_CLEARSCREEN     "multimedia_clear_screen"
#define DEF_FFP_RESETPOINTER    "multimedia_reset_pointer"

	static FFP_INIT         multimedia_init_device = NULL;
	static FFP_OPEN         multimedia_stream_open = NULL;
	static FFP_START        multimedia_stream_start = NULL;
	static FFP_EXIT         multimedia_exit = NULL;
	static FFP_GETNAME      multimedia_get_filename = NULL;
	static FFP_SETNAME      multimedia_set_filename = NULL;
	static FFP_STOP         multimedia_stream_stop = NULL;
	static FFP_GETAUDFMT    multimedia_get_audioformat = NULL;
	static FFP_GETVIDFMT    multimedia_get_videoformat = NULL;
	static FFP_GETDURATION  multimedia_get_duration_in_mSec = NULL;
	static FFP_PAUSE        multimedia_pause_resume = NULL;
	static FFP_LOOPALIVE    multimedia_event_loop_alive = NULL;

	static FFP_YUVTORGB24   multimedia_yuv420p_to_rgb24 = NULL;
	static FFP_YUVTORGB32   multimedia_yuv420p_to_rgb32 = NULL;

	static FFP_RESIZESCREEN	multimedia_resize_screen = NULL;
	static FFP_CLEARSCREEN  multimedia_clear_screen = NULL;
	static FFP_RESETPOINTER multimedia_reset_pointer = NULL;


	static HMODULE	hMod = NULL;

	static FFP_EVENTS		FFP_events;

	static FFP_EVENT_EXIT	OnExit = NULL;
	static FFP_EVENT_INFO   OnInfo = NULL;
	static FFP_EVENT_AUDIO  OnAudio = NULL;
	static FFP_EVENT_VIDEO  OnVideo = NULL;
	static FFP_EVENT_VIDEORESIZE OnResize = NULL;
	static FFP_EVENT_PLAYSTATUS	 OnStatus = NULL;

	static void OnEventExit(void *sender, int exitCode)
	{
		if (OnExit != NULL)
			OnExit(sender, exitCode);
	}

	static void OnEventInfo(void *sender, int infoCode, char *Message)
	{
		if (OnInfo != NULL)
			OnInfo(sender, infoCode, Message);
	}

	static void OnEventAudio(void *sender, unsigned char *AudBuffer, int BufferLengthInByte)
	{
		if (OnAudio != NULL)
			OnAudio(sender, AudBuffer, BufferLengthInByte);
	}

	static void OnEventVideo(void *sender, FFP_YUV420P_DATA *YuvData)
	{
		if (OnVideo != NULL)
			OnVideo(sender, YuvData);
	}

	static void OnEventResize(void *sender, int width, int height)
	{
		if (OnResize != NULL)
			OnResize(sender, width, height);
	}

	static void OnEventStatus(void *sender, FFP_PLAY_STATUS status)
	{
		if (OnStatus != NULL)
			OnStatus(sender, status);
	}

	static int ImportDLL()
	{
		int rtn = -1;

		multimedia_init_device = (FFP_INIT)GetProcAddress(hMod, DEF_FFP_INIT);
		if (multimedia_init_device == NULL)
		{
			return 1;
		}

		multimedia_stream_open = (FFP_OPEN)GetProcAddress(hMod, DEF_FFP_OPEN);
		if (multimedia_stream_open == NULL)
		{
			return 2;
		}

		multimedia_stream_start = (FFP_START)GetProcAddress(hMod, DEF_FFP_START);
		if (multimedia_stream_start == NULL)
		{
			return 3;
		}

		multimedia_exit = (FFP_EXIT)GetProcAddress(hMod, DEF_FFP_EXIT);
		if (multimedia_exit == NULL)
		{
			return 4;
		}

		multimedia_get_filename = (FFP_GETNAME)GetProcAddress(hMod, DEF_FFP_GETNAME);
		if (multimedia_get_filename == NULL)
		{
			return 5;
		}

		multimedia_set_filename = (FFP_SETNAME)GetProcAddress(hMod, DEF_FFP_SETNAME);
		if (multimedia_set_filename == NULL)
		{
			return 6;
		}

		multimedia_stream_stop = (FFP_STOP)GetProcAddress(hMod, DEF_FFP_STOP);
		if (multimedia_stream_stop == NULL)
		{
			return 7;
		}

		multimedia_get_audioformat = (FFP_GETAUDFMT)GetProcAddress(hMod, DEF_FFP_GETAUDFMT);
		if (multimedia_get_audioformat == NULL)
		{
			return 8;
		}

		multimedia_get_videoformat = (FFP_GETVIDFMT)GetProcAddress(hMod, DEF_FFP_GETVIDFMT);
		if (multimedia_get_videoformat == NULL)
		{
			return 9;
		}

		multimedia_get_duration_in_mSec = (FFP_GETDURATION)GetProcAddress(hMod, DEF_FFP_GETDURATION);
		if (multimedia_get_duration_in_mSec == NULL)
		{
			return 10;
		}

		multimedia_pause_resume = (FFP_PAUSE)GetProcAddress(hMod, DEF_FFP_PAUSE);
		if (multimedia_pause_resume == NULL)
		{
			return 11;
		}

		multimedia_event_loop_alive = (FFP_LOOPALIVE)GetProcAddress(hMod, DEF_FFP_LOOPALIVE);
		if (multimedia_event_loop_alive == NULL)
		{
			return 12;
		}

		multimedia_yuv420p_to_rgb24 = (FFP_YUVTORGB24)GetProcAddress(hMod, DEF_FFP_YUVTORGB24);
		if (multimedia_yuv420p_to_rgb24 == NULL)
		{
			return 13;
		}

		multimedia_yuv420p_to_rgb32 = (FFP_YUVTORGB32)GetProcAddress(hMod, DEF_FFP_YUVTORGB32);
		if (multimedia_yuv420p_to_rgb32 == NULL)
		{
			return 14;
		}

		multimedia_resize_screen = (FFP_RESIZESCREEN)GetProcAddress(hMod, DEF_FFP_RESIZESCREEN);
		if (multimedia_resize_screen == NULL)
		{
			return 15;
		}

		multimedia_clear_screen = (FFP_CLEARSCREEN)GetProcAddress(hMod, DEF_FFP_CLEARSCREEN);
		if (multimedia_clear_screen == NULL)
		{
			return 16;
		}

		multimedia_reset_pointer = (FFP_RESETPOINTER)GetProcAddress(hMod, DEF_FFP_RESETPOINTER);
		if (multimedia_reset_pointer == NULL)
		{
			return 17;
		}

		rtn = 0;

		return rtn;
	}

#if 1
	extern "C"
	{
		__declspec(dllexport) int  InitilizeDLL();
		__declspec(dllexport) void FinalizeDLL();
		__declspec(dllexport) int  SetupYuvPlayer(void *sender, unsigned int yuvHandle, void *eventInfo, void *eventExit, void *eventAudio, void *eventResize, void *eventStatus);
		__declspec(dllexport) int  SetupRgbPlayer(void *sender, unsigned int yuvHandle, void *eventInfo, void *eventExit, void *eventAudio, void *eventVideo, void *eventResize, void *eventStatus);
		__declspec(dllexport) int  SetupCLIPlayer(void *sender, void *eventInfo, void *eventExit, void *eventAudio, void *eventResize, void *eventStatus);
		__declspec(dllexport) void SetPlayerFileName(const char *FileName);
		__declspec(dllexport) const char* GetPlayerFileName();
		__declspec(dllexport) int  StartThreadPlayer();
		__declspec(dllexport) void StopThreadPlayer();
		__declspec(dllexport) void PauseResumePlayer();
		__declspec(dllexport) int  OpenPlayerStream();
		__declspec(dllexport) void StartPlayer();
		__declspec(dllexport) void StopPlayer();
		__declspec(dllexport) void ClearPlayer();
		__declspec(dllexport) int  IsPlayerRunning();
		__declspec(dllexport) void ConvertYUV420pToRGB32(void *yuvData, void *rgbData, int *pWidth, int *pHeight);
		__declspec(dllexport) void ConvertYUV420pToRGB24(void *yuvData, void *rgbData, int *pWidth, int *pHeight);
		__declspec(dllexport) void GetPlayerTimes(INT64	*duration_in_us, double *current_in_s);
		__declspec(dllexport) int  GetPlayerStatus();
		__declspec(dllexport) void GetPlayerAudioFormat(int *freq, unsigned char *channels, unsigned short *format, unsigned short *samplesinbuffer, unsigned long *buffersizeinbyte);
		__declspec(dllexport) void GetPlayerVideoFormat(int *width, int *height);
		__declspec(dllexport) void SetPlayerResizeScreen(int width, int height);
		__declspec(dllexport) void SetPlayerResetPointer();
		__declspec(dllexport) void SetPlayerClearScreen(int screenID, int width, int height);
	}
#endif
		int InitilizeDLL()
		{
			hMod = LoadLibrary(_T("./FFPlayLib.dll"));

			if (hMod != NULL)
			{
				OutputDebugString(_T("-> Load library!\n"));
				return ImportDLL();
			}
			else
			{
				hMod = NULL;
				OutputDebugString(_T("-> Fail to load library!\n"));
				return 1;
			}
		}

	void FinalizeDLL()
	{
		if (hMod)
		{
			FreeLibrary(hMod);
			OutputDebugString(_T("-> Unload library!\n"));
		}
		else
		{
			OutputDebugString(_T("-> Fail to unload library!\n"));
		}
	}

	int SetupCLIPlayer(void *sender, void *eventInfo, void *eventExit, void *eventAudio, void *eventResize, void *eventStatus)
	{
		FFP_events.sender				= sender;
		FFP_events.current_in_s			= 0;
		FFP_events.duration_in_us		= 0;
		FFP_events.event_audio			= OnEventAudio;
		FFP_events.event_exit			= OnEventExit;
		FFP_events.event_play_status	= OnEventStatus;
		FFP_events.event_info			= OnEventInfo;
		FFP_events.event_video			= NULL;// OnEventVideo;
		FFP_events.event_video_resize	= OnEventResize;
		FFP_events.screenID				= 0;
		FFP_events.ui_type				= FFP_CLI;
		FFP_events.playstatus			= FFP_STATUS_STOP;

		OnExit = (FFP_EVENT_EXIT)eventExit;
		OnInfo = (FFP_EVENT_INFO)eventInfo;
		OnAudio = (FFP_EVENT_AUDIO)eventAudio;
		OnVideo = NULL;
		OnResize = (FFP_EVENT_VIDEORESIZE)eventResize;
		OnStatus = (FFP_EVENT_PLAYSTATUS)eventStatus;

		return multimedia_init_device(&FFP_events);
	}

	int SetupYuvPlayer(void *sender, unsigned int yuvHandle, void *eventInfo, void *eventExit, void *eventAudio, void *eventResize, void *eventStatus)
	{
		FFP_events.sender = sender;
		FFP_events.current_in_s = 0;
		FFP_events.duration_in_us = 0;
		FFP_events.event_audio = OnEventAudio;
		FFP_events.event_exit  = OnEventExit;
		FFP_events.event_play_status = OnEventStatus;
		FFP_events.event_info  = OnEventInfo;
		FFP_events.event_video = NULL;// OnEventVideo;
		FFP_events.event_video_resize = OnEventResize;
		FFP_events.screenID    = yuvHandle;
		FFP_events.ui_type     = FFP_GUI;
		FFP_events.playstatus  = FFP_STATUS_STOP;

		OnExit   = (FFP_EVENT_EXIT)eventExit;
		OnInfo   = (FFP_EVENT_INFO)eventInfo;
		OnAudio  = (FFP_EVENT_AUDIO)eventAudio;
		OnVideo  = NULL;
		OnResize = (FFP_EVENT_VIDEORESIZE)eventResize;
		OnStatus = (FFP_EVENT_PLAYSTATUS)eventStatus;

		return multimedia_init_device(&FFP_events);
	}

	int SetupRgbPlayer(void *sender, unsigned int yuvHandle, void *eventInfo, void *eventExit, void *eventAudio, void *eventVideo, void *eventResize, void *eventStatus)
	{
		FFP_events.sender = sender;
		FFP_events.current_in_s   = 0;
		FFP_events.duration_in_us = 0;
		FFP_events.event_audio    = OnEventAudio;
		FFP_events.event_exit     = OnEventExit;
		FFP_events.event_play_status = OnEventStatus;
		FFP_events.event_info  = OnEventInfo;
		FFP_events.event_video = OnEventVideo;
		FFP_events.event_video_resize = OnEventResize;
		FFP_events.screenID   = yuvHandle;
		FFP_events.ui_type    = FFP_GUI;
		FFP_events.playstatus = FFP_STATUS_STOP;

		OnExit   = (FFP_EVENT_EXIT)eventExit;
		OnInfo   = (FFP_EVENT_INFO)eventInfo;
		OnAudio  = (FFP_EVENT_AUDIO)eventAudio;
		OnVideo  = (FFP_EVENT_VIDEO)eventVideo;
		OnResize = (FFP_EVENT_VIDEORESIZE)eventResize;
		OnStatus = (FFP_EVENT_PLAYSTATUS)eventStatus;

		return multimedia_init_device(&FFP_events);
	}

	void SetPlayerFileName(const char *FileName)
	{
		multimedia_set_filename(FileName);
	}

	const char* GetPlayerFileName()
	{
		return multimedia_get_filename();
	}

	static void ThreadPlaying(void *pParam)
	{
		multimedia_stream_start();
	}

	int StartThreadPlayer()
	{
		if (multimedia_stream_open() == FFP_FALSE)
			return 1;

		_beginthread(ThreadPlaying, 0, NULL);

		return 0;
	}

	void StopThreadPlayer()
	{
		multimedia_stream_stop();
		FFP_events.playstatus   = FFP_STATUS_STOP;
		FFP_events.current_in_s = 0;
		FFP_events.duration_in_us = 0;
	}

	int OpenPlayerStream()
	{
		if (multimedia_stream_open() == FFP_FALSE)
			return 1;
		else
			return 0;
	}

	void StartPlayer()
	{
		multimedia_stream_start();
	}
	
	void StopPlayer()
	{
		StopThreadPlayer();
	}
	
	void PauseResumePlayer()
	{
		multimedia_pause_resume();
	}

	void ClearPlayer()
	{
		multimedia_exit();
	}

	int IsPlayerRunning()
	{
		return multimedia_event_loop_alive();
	}

	void ConvertYUV420pToRGB32(void *yuvData, void *rgbData, int *pWidth, int *pHeight)
	{
		FFP_YUV420P_DATA *pYUV;
		unsigned char    *pRGB;
		pYUV = (FFP_YUV420P_DATA*)yuvData;
		pRGB = (unsigned char*)rgbData;
		multimedia_yuv420p_to_rgb32(pYUV, pRGB);
		*pWidth  = pYUV->w;
		*pHeight = pYUV->h;
	}

	void ConvertYUV420pToRGB24(void *yuvData, void *rgbData, int *pWidth, int *pHeight)
	{
		FFP_YUV420P_DATA *pYUV;
		unsigned char    *pRGB;
		pYUV = (FFP_YUV420P_DATA*)yuvData;
		pRGB = (unsigned char*)rgbData;
		multimedia_yuv420p_to_rgb24(pYUV, pRGB);
		*pWidth  = pYUV->w;
		*pHeight = pYUV->h;
	}

	int GetPlayerStatus()
	{
		return FFP_events.playstatus;
	}
	
	void GetPlayerTimes(INT64	*duration_in_us, double *current_in_s)
	{
		*duration_in_us = FFP_events.duration_in_us;
		*current_in_s = FFP_events.current_in_s;
	}

	void GetPlayerAudioFormat(int *freq, unsigned char *channels, unsigned short *format, unsigned short *samplesinbuffer, unsigned long *buffersizeinbyte)
	{
		FFP_AUD_PARAMS *pLocalFmt = multimedia_get_audioformat();

		*freq = pLocalFmt->Freq;
		*channels = pLocalFmt->Channels;
		*format = pLocalFmt->Format;
		*samplesinbuffer = pLocalFmt->SamplesInBuffer;
		*buffersizeinbyte = pLocalFmt->BufferSizeInBytes;
	}

	void GetPlayerVideoFormat(int *width, int *height)
	{
		FFP_VID_PARAMS *pLocalVideoFmt = multimedia_get_videoformat();

		*width = pLocalVideoFmt->width;
		*height = pLocalVideoFmt->height;
	}

	void SetPlayerResizeScreen(int width, int height)
	{
		multimedia_resize_screen(width, height);
	}
	
	void SetPlayerClearScreen(int screenID, int width, int height)
	{
		multimedia_clear_screen(screenID, width, height);
	}
	
	void SetPlayerResetPointer()
	{
		multimedia_reset_pointer();
	}
