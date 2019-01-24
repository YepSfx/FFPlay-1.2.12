unit umain;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs, StdCtrls, ExtCtrls;

//{$DEFINE DEF_RGB}
//{$DEFINE DEF_DEL7}

const WM_USER_RESIZE          = WM_USER + $01;
      WM_USER_REFRESH         = WM_USER + $02;
      WM_USER_STATUS          = WM_USER + $03;

type
  TfrmMain = class(TForm)
    mButtonPlay: TButton;
    OpenDialog: TOpenDialog;
    mButtonStop: TButton;
    ScrollBar1: TScrollBar;
    Timer1: TTimer;
    Label1: TLabel;
    mButtonPause: TButton;
    Panel1: TPanel;
    Panel2: TPanel;
    Image1: TImage;
    Button1: TButton;
    procedure mButtonPlayClick(Sender: TObject);
    procedure mButtonStopClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
    procedure mButtonPauseClick(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
  private
    { Private declarations }
    FCurrentTime_Sec   : double;
    FDurationTime_mSec : Int64;
    FThreadPlayingID   : Integer;
    FStaticCmd         : Integer;
  public
    { Public declarations }
    procedure OnResizeScreen(var Msg : TMessage); message WM_USER_RESIZE;
    procedure OnRefreshScreen(var Msg : TMessage); message WM_USER_REFRESH;
    procedure OnStatus(var Msg : TMessage); message WM_USER_STATUS;
    procedure updateScreen( Buffer : pointer ; w,h,Bpp : Integer);
    procedure resizeScreen( w , h : Integer );
    procedure updateTime(Current : double );
    procedure StopPlaying();
    procedure StartPlaying();
  end;

var
  frmMain: TfrmMain;

implementation

{$R *.dfm}

uses FFPlay;

type TPlayData = record
  w            : Integer;
  h            : Integer;
  Bpp          : Integer;
  status       : TFFP_PLAY_STATUS;
end;
type PPlayData = ^TPlayData;

const TestArcv : AnsiString = '.\1.asf';

var ffp_events : TFFP_EVENTS;
    RGBBuffer  : pointer;

function ConvertFileName( unicodeName : String ) : AnsiString;
  var L       : Cardinal;
      utf8Str : UTF8String;
begin
  L := Length(unicodeName);
  SetLength(utf8Str, (L * sizeOf(Char) + 1));
  L := UnicodeToUtf8(PAnsiChar(utf8Str), Length(utf8Str), PWideChar(unicodeName), L);
  Result := AnsiString(utf8Str);
end;

procedure EventExit( sender : pointer ; exitCode : Integer ) ; cdecl;
begin
  Application.Terminate();
end;

procedure EventInfo( sender : pointer; infoCode : Integer ; msg : PFFP_CHAR ) ; cdecl;
  var dbgMsg : String;
begin
  dbgMsg := String(msg);
  OutputDebugString(PChar(dbgMsg));
end;

procedure EventAudio( sender : pointer ; buff : PByte ; BuffLenInByte : Integer ) ; cdecl;
begin

end;

procedure EventPlayStatus( sender : pointer ; status : TFFP_PLAY_STATUS ) ; cdecl;
  var handle : HWND;
      msg    : String;
      pPlay  : PPlayData;
begin
  handle := TfrmMain(sender).Handle;

  msg := Format('>>Status: %d',[Integer(status)]);
  OutputDebugString(PChar(msg));

  GetMem( pPlay , sizeof(TPlayData) );
  FillChar( pPlay^,Sizeof(TPlayData), 0);
  pPlay.status := status;
  PostMessage( handle, WM_USER_STATUS, Integer(pPlay), 0 );

end;


procedure EventVideo( sender : pointer ; YuvData : PFFP_YUV_DATA ) ; cdecl;
  var handle : HWND;
      msg    : String;
      pPlay  : PPlayData;
begin
  handle := TfrmMain(sender).Handle;
  multimedia_yuv420p_to_rgb24( YuvData,  RGBBuffer);

  GetMem( pPlay , sizeof(TPlayData) );
  FillChar( pPlay^,Sizeof(TPlayData), 0);
  pPlay.w   := YuvData.width;
  pPlay.h   := YuvData.height;
  pPlay.Bpp := 3;
  PostMessage( handle, WM_USER_REFRESH, Integer(pPlay), 0 );
end;

procedure EventResize(sender : pointer ; w, h : Integer) ; cdecl;
  var msg    : String;
      handle : HWND;
      pPlay  : PPlayData;
begin
  handle := TfrmMain(sender).Handle;
  GetMem( pPlay , sizeof(TPlayData) );
  FillChar( pPlay^,Sizeof(TPlayData), 0);
  pPlay.w := w;
  pPlay.h := h;
  PostMessage( handle, WM_USER_RESIZE, Integer(pPlay), 0 );
end;

function ThreadPlaying(Param : Pointer) : Integer;
begin
  multimedia_stream_start();

  EndThread(0);
end;

procedure TfrmMain.OnStatus(var Msg: TMessage);
  var pPlay : PPlayData;
begin
  pPlay := PPlayData(Msg.WParam);

  case pPlay.status of
  FFP_PAUSED: begin
                   mButtonPause.Caption := 'Resume';
                   end;
  FFP_RESUMED:begin
                   mButtonPause.Caption := 'Pause';
              end;
  FFP_STOP:   begin
                   StopPlaying();
              end;
  FFP_EOF:    begin
                   StopPlaying();
              end;
  end;

  FreeMem(pPlay);
end;

procedure TfrmMain.OnRefreshScreen(var Msg : TMessage);
  var pPlay : PPlayData;
begin
  pPlay := PPlayData(Msg.WParam);
  updateScreen(RGBBuffer, pPlay.w, pPlay.h, pPlay.Bpp);
  FreeMem(pPlay);
end;

procedure TfrmMain.OnResizeScreen(var Msg: TMessage);
  var pPlay : PPlayData;
      w, h  : Integer;
      dmsg   : String;
begin
  pPlay := PPlayData(Msg.WParam);
  resizeScreen(pPlay.w, pPlay.h);
  w :=  pPlay.w;
  h :=  pPlay.h;
  FreeMem(pPlay);
  dmsg := Format('--> Resize Screen message %d %d',[w,h]);
  OutputDebugString(PChar(dmsg));
end;

procedure TfrmMain.updateTime(Current: double);
begin
  FCurrentTime_Sec := Current;
end;

procedure TfrmMain.resizeScreen(w , h : Integer );
  var center_x, center_y : Integer;
begin
{$IFDEF DEF_RGB}
  Panel2.Width := w;
  Panel2.Height:= h;

  Image1.Width := w;
  Image1.Height:= h;
  Image1.Picture.Bitmap.Width  := w;
  Image1.Picture.Bitmap.Height := h;
  Image1.Picture.Bitmap.PixelFormat := pf24Bit;

  center_x := Panel1.Width div 2;
  center_y := Panel1.Height div 2;

  Image1.Top  := center_y - (h div 2);
  Image1.Left := center_x - (w div 2);

  Image1.Invalidate();
{$ELSE}
  Panel2.Width := w;
  Panel2.Height:= h;

  center_x := Panel1.Width div 2;
  center_y := Panel1.Height div 2;

  Panel2.Top  := center_y - (h div 2);
  Panel2.Left := center_x - (w div 2);

{$ENDIF}
end;

procedure TfrmMain.Timer1Timer(Sender: TObject);
  var msg : String;
begin
  FCurrentTime_Sec := ffp_events.current_in_s;
  FDurationTime_mSec := ffp_events.duration_in_us div 1000000;
  msg := Format('%f / %d',[FCurrentTime_Sec, FDurationTime_mSec]);
  Label1.Caption := msg;
  ScrollBar1.Max := FDurationTime_mSec ;
  ScrollBar1.Position := Round(FCurrentTime_Sec);
end;

procedure TfrmMain.updateScreen(Buffer: Pointer; w: Integer; h: Integer; Bpp: Integer);
  var pSrc, pDst : PByte;
      lineLen : Integer;
begin
  pSrc := Buffer;
  lineLen := w * Bpp;
  try
    for h := 0 to h - 1 do
    begin
      pDst := Image1.Picture.Bitmap.ScanLine[h];
      CopyMemory( pDst, pSrc, lineLen);
      Inc(pSrc,lineLen);
    end;
    Image1.Invalidate();
  except
    ShowMessage('Error occured! Stop playing...');
    mButtonStopClick(self);
  end;
end;

procedure TfrmMain.FormCreate(Sender: TObject);
begin
  Image1.Picture.Bitmap.PixelFormat := pf24Bit;
  GetMem(RGBBuffer, (1024*768*4));
  FCurrentTime_Sec := 0;
  FDurationTime_mSec:= 0;
  Timer1.Enabled := False;

  mButtonPlay.Enabled := True;
  mButtonStop.Enabled := False;
  mButtonPause.Enabled:= False;
end;

procedure TfrmMain.FormDestroy(Sender: TObject);
begin
  FreeMem(RGBBuffer);
  OutputDebugString('Destory');
end;

procedure TfrmMain.mButtonPauseClick(Sender: TObject);
begin
  multimedia_pause_resume();
end;

procedure TfrmMain.mButtonPlayClick(Sender: TObject);
  var argv  : TFFP_CHAR;
      argc  : Integer;
      rtnStr: TFFP_CHAR;
      mediaFile : AnsiString;
      hWin  : HWND;
      mms   : AnsiString;
begin
  ffp_events.sender           := self;
  ffp_events.screenID         := Panel2.Handle;
  ffp_events.uiType           := FFP_GUI;
  ffp_events.eventInfo        := EventInfo;
  ffp_events.eventExit        := EventExit;
  ffp_events.eventAudio       := EventAudio;
  ffp_events.eventResize      := EventResize;
  ffp_events.eventStatus      := EventPlayStatus;
  ffp_events.playStatus       := FFP_STOP;

{$IFDEF DEF_RGB}
  ffp_events.eventVideo := EventVideo;
  Panel2.Enabled        := False;
  Panel2.Visible        := False;
{$ELSE}
  ffp_events.eventVideo := nil;
  Panel2.Enabled        := True;
  Panel2.Visible        := True;
{$ENDIF}

  if OpenDialog.Execute() then
  begin
{$IFDEF DEF_DEL7}
    mediaFile := AnsiString(OpenDialog.FileName);
{$ELSE}
    mediaFile := ConvertFileName(OpenDialog.FileName);
{$ENDIF}
    multimedia_set_filename( PFFP_CHAR(mediaFile) );
    if multimedia_init_device( @ffp_events ) <> 0 then
    begin
      multimedia_exit();
      ShowMessage('Fail to Init!');
      Exit;
    end;
    if ( multimedia_stream_open( ) = FFP_FALSE ) then
    begin
      multimedia_exit();
      ShowMessage('Fail to open file!');
      Exit;
    end;
    Timer1.Enabled := True;
    ScrollBar1.Max := 100;

    try
      mButtonPlay.Enabled := False;
      mButtonStop.Enabled := True;
      mButtonPause.Enabled:= True;

      StartPlaying();
    except
      ShowMessage('Have a problem to play!');
    end;
  end;
end;

procedure TfrmMain.mButtonStopClick(Sender: TObject);
begin
   multimedia_stream_stop();
   FCurrentTime_Sec := 0;
   FDurationTime_mSec:= 0;
   ScrollBar1.Position := 0;
   Timer1.Enabled := False;
   mButtonPlay.Enabled := True;
   mButtonStop.Enabled := False;
   mButtonPause.Enabled:= False;

   mButtonPause.Caption := 'Pause';
end;

procedure TfrmMain.StopPlaying();
begin
  mButtonStopClick(self);
end;

procedure TfrmMain.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
begin
  if multimedia_event_loop_alive() = 1 then
  begin
    multimedia_stream_stop();
    CanClose := True;
  end;
end;

procedure TfrmMain.StartPlaying();
  var id : LongWord;
      pData : Integer;
begin
  CloseHandle( FThreadPlayingID );
  FThreadPlayingID := BeginThread(nil, 0, @ThreadPlaying, @pData, 0, id);
end;

end.
