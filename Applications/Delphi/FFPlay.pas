unit FFPlay;

interface

{$IFDEF DEF_OUTPUT_WIN}
       const LIBNAME = '../Bin/FFPlayLib.dll';
{$ELSE}
       const LIBNAME = 'libFFPlay.so';
{$ENDIF}

       const FFP_AUDIO_U8	    = $0008;
       const FFP_AUDIO_S8	    = $8008;
       const FFP_AUDIO_U16LSB	= $0010;
       const FFP_AUDIO_S16LSB	= $8010;
       const FFP_AUDIO_U16MSB	= $1010;
       const FFP_AUDIO_S16MSB	= $9010;
       const FFP_AUDIO_U16	  = FFP_AUDIO_U16LSB;
       const FFP_AUDIO_S16	  = FFP_AUDIO_S16LSB;

       type TFFP_UITYPE = ( FFP_CLI = 0, FFP_GUI = 1 );
       type TFFP_BOOL   = ( FFP_FALSE = 0, FFP_TRUE = 1);
       type TFFP_INFO   = ( FFP_INFO_NONE = 0, FFP_INFO_ERROR = 1, FFP_INFO_WARNING = 2, FFP_INFO_STREAM_ERROR = 3, FFP_INFO_DEBUG = 4);
       type TFFP_PLAY_STATUS = ( FFP_STOP    = 0,
                                 FFP_PLAY    = 1,
                                 FFP_PAUSED  = 2,
                                 FFP_RESUMED = 3,
                                 FFP_EOF     = 4);
       type TFFP_CHAR = AnsiChar;
       type PFFP_CHAR = ^TFFP_CHAR;

       type TFFP_AUD_PARAMS = record
            Freq           : Integer;
            Channels       : Byte;
            Format         : Word;
            SampsInBuff    : Word;
            BuffSizeinByte : Cardinal;
       end;
       type PFFP_AUD_PARAMS = ^TFFP_AUD_PARAMS;

       type TFFP_VID_PARAMS = record
            Width         : Integer;
            Height        : Integer;
            BytesPerPixel : Integer;
       end;
       type PFFP_VID_PARAMS = ^TFFP_VID_PARAMS;

       type TFFP_YUV_DATA = record
           width  : Integer;
           height : Integer;
           pixels : pointer;
       end;
       type PFFP_YUV_DATA = ^TFFP_YUV_DATA;

       type TFFP_EVENTEXIT         = procedure( sender : pointer ; exitCode : Integer ) ; cdecl ;
       type TFFP_EVENTINFO         = procedure( sender : pointer ; infoCode : Integer ; msg : PFFP_CHAR ) ; cdecl ;
       type TFFP_EVENTAUDIO        = procedure( sender : pointer ; buff : PByte ; BuffLenInByte : Integer ) ; cdecl ;
       type TFFP_EVENTVIDEO        = procedure( sender : pointer ; YuvData :  PFFP_YUV_DATA) ; cdecl ;
       type TFFP_EVENTVIDEORESIZE  = procedure( sender : pointer ; w , h : Integer ) ; cdecl;
       type TFFP_EVENTPLAYSTATUS   = procedure( sender : pointer ; status : TFFP_PLAY_STATUS ) ; cdecl;

       type TFFP_EVENTS = record
           sender          : Pointer;
           screenID        : Cardinal;
           duration_in_us  : Int64;
           current_in_s    : double;
           uiType          : TFFP_UITYPE;
           eventInfo       : TFFP_EVENTINFO;
           eventExit       : TFFP_EVENTEXIT;
           eventAudio      : TFFP_EVENTAUDIO;
           eventVideo      : TFFP_EVENTVIDEO;
           eventResize     : TFFP_EVENTVIDEORESIZE;
           eventStatus     : TFFP_EVENTPLAYSTATUS;
           playStatus      : TFFP_PLAY_STATUS;
       end;
       type PFFP_EVENTS = ^TFFP_EVENTS;

       function  multimedia_init_device( events : PFFP_EVENTS ) : Integer ; cdecl ; external LIBNAME;
       function  multimedia_stream_open()  : TFFP_BOOL ; cdecl ; external LIBNAME;
       procedure multimedia_stream_start() ; cdecl ; external LIBNAME;
       procedure multimedia_exit() ; cdecl ;  external LIBNAME;
       function  multimedia_get_filename() : PFFP_CHAR ; cdecl ; external LIBNAME;
       procedure multimedia_parse_options( argc : Integer ; argv : PFFP_CHAR ) ; cdecl ;external LIBNAME;
       procedure multimedia_set_filename( filename : PFFP_CHAR ) ; cdecl ; external LIBNAME;
       procedure multimedia_stream_stop() ; cdecl ; external LIBNAME;
       function  multimedia_get_audioformat() : PFFP_AUD_PARAMS ; cdecl ; external LIBNAME;
       function  multimedia_get_videoformat() : PFFP_VID_PARAMS ; cdecl ; external LIBNAME;
       function  multimedia_get_duration_in_mSec() : Int64 ; cdecl ; external LIBNAME;
       procedure multimedia_pause_resume() ; cdecl ; external LIBNAME;
       function  multimedia_event_loop_alive() : Integer ; cdecl ; external LIBNAME;
       procedure multimedia_resize_screen(w,h : Integer) ; cdecl ; external LIBNAME;
       procedure multimedia_clear_screen( screenID, width, height : Integer ) ; cdecl ; external LIBNAME;
       procedure multimedia_reset_pointer(); cdecl ; external LIBNAME;

       procedure multimedia_yuv420p_to_rgb24( YuvData : PFFP_YUV_DATA ; RGBBuff : pointer) ; cdecl ; external LIBNAME;
       procedure multimedia_yuv420p_to_rgb32( YuvData : PFFP_YUV_DATA ; RGBBuff : pointer) ; cdecl ; external LIBNAME;

       function  multimedia_test_xwin_draw_start(XWinID, width, height : Integer) : Integer ; cdecl ; external LIBNAME;
       procedure multimedia_test_xwin_draw_stop(); cdecl ; external LIBNAME;

implementation

initialization

finalization


end.
