#define INT64				__int64

extern "C"
{

	typedef void(*FFP_EVENT_EXIT)(void *sender, int exitCode);
	typedef void(*FFP_EVENT_INFO)(void *sender, int infoCode, char *Message);
	typedef void(*FFP_EVENT_AUDIO)(void *sender, unsigned char *AudBuffer, int BufferLengthInByte);
	typedef void(*FFP_EVENT_VIDEO)(void *sender, void *YuvData);
	typedef void(*FFP_EVENT_VIDEORESIZE)(void *sender, int width, int height);
	typedef void(*FFP_EVENT_PLAYSTATUS)(void *sender, int status);

		__declspec(dllexport) int  InitilizeDLL();
		__declspec(dllexport) void FinalizeDLL();
		__declspec(dllexport) int  SetupYuvPlayer(void *sender, unsigned int yuvHandle, void *eventInfo, void *eventExit, void *eventAudio, void *eventResize, void *eventStatus);
		__declspec(dllexport) int  SetupRgbPlayer(void *sender, unsigned int yuvHandle, void *eventInfo, void *eventExit, void *eventAudio, void *eventVideo, void *eventResize, void *eventStatus);
		__declspec(dllexport) void SetPlayerFileName(const char *FileName);
		__declspec(dllexport) const char* GetPlayerFileName();
		__declspec(dllexport) int  StartPlayer();
		__declspec(dllexport) void StopPlayer();
		__declspec(dllexport) void PauseResumePlayer();
		__declspec(dllexport) void ClearPlayer();
		__declspec(dllexport) int  IsPlayerRunning();
		__declspec(dllexport) void ConvertYUV420pToRGB32(void *yuvData, void *rgbData);
		__declspec(dllexport) void ConvertYUV420pToRGB24(void *yuvData, void *rgbData);
		__declspec(dllexport) void GetPlayerTimes(INT64	*duration_in_us, double *current_in_s);
		__declspec(dllexport) int  GetPlayerStatus();
		__declspec(dllexport) void GetPlayerAudioFormat(int *freq, unsigned char *channels, unsigned short *format, unsigned short *samplesinbuffer, unsigned long *buffersizeinbyte);
		__declspec(dllexport) void GetPlayerVideoFormat(int *width, int *height);
		__declspec(dllexport) void SetPlayerResizeScreen(int width, int height);
		__declspec(dllexport) void SetPlayerResetPointer();
		__declspec(dllexport) void SetPlayerClearScreen(int screenID, int width, int height);

}