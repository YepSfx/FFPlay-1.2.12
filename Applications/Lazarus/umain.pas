unit umain;

{$mode objfpc}{$H+}

//{$DEFINE DEF_RGB}

interface

uses
  {$ifdef fpc}
  LCLType,
  {$endif}
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, ExtCtrls,
  StdCtrls, LCLIntf,  LMessages, LazLogger;

const WM_USER_RESIZE          = WM_USER + $01;
      WM_USER_REFRESH         = WM_USER + $02;
      WM_USER_STATUS          = WM_USER + $03;
      WM_USER_EVENT_UPDATE    = WM_USER + $04;
      WM_USER_EVENT_LOG       = WM_USER + $05;
type
  { TfrmMain }
  TfrmMain = class(TForm)
    ButtonPause : TButton;
    ButtonStop  : TButton;
    ButtonPlay  : TButton;
    Label1      : TLabel;
    Label2      : TLabel;
    Memo1       : TMemo;
    Panel1      : TPanel;
    ImageRGB    : TImage;
    PanelYUV    : TGroupBox;
    OpenDialog  : TOpenDialog;
    ScrollBar1  : TScrollBar;
    Timer1      : TTimer;
    procedure ButtonPauseClick(Sender: TObject);
    procedure ButtonPlayClick(Sender: TObject);
    procedure ButtonStopClick(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: boolean);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure FormResize(Sender: TObject);
    procedure Panel1Resize(Sender: TObject);
    procedure PanelYUVMouseMove(Sender: TObject; Shift: TShiftState; X,
      Y: Integer);
    procedure PanelYUVResize(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
  private
    { private declarations }
    FCurrentTime_Sec   : double;
    FDurationTime_mSec : Int64;
    FThreadPlayingID   : TThreadID;
    FinitW, FInitH     : Integer;
    FresImage          : TBitmap;
  public
    { public declarations }
    procedure OnResizeScreen(var Msg : TLMessage); message WM_USER_RESIZE;
    procedure OnRefreshScreen(var Msg : TLMessage); message WM_USER_REFRESH;
    procedure OnStatus(var Msg : TLMessage); message WM_USER_STATUS;
    procedure OnUserEvent(var Msg : TLMessage); message WM_USER_EVENT_UPDATE;
    procedure OnUserLog(var Msg : TLMessage); message WM_USER_EVENT_LOG;
    procedure stopPlaying();
    procedure startPlaying();
    procedure updateScreen( Buffer : pointer ; w,h,Bpp : Integer);
    procedure updateTime(Current : double);
    procedure resizeScreen(w, h : Integer);
  end;

var
  frmMain: TfrmMain;

implementation

{$R *.frm}

{ TfrmMain }

uses FFPlay, LCLProc
     {$IFDEF DEF_OUTPUT_WIN}
     ;
     {$ELSE}
     ,Gtk2, Gdk2, Gdk2x, Glib2, xlib, x, Gtk2Proc;
     {$ENDIF}

type TVideoData = record
     Buffer : Pointer;
     w      : Integer;
     h      : Integer;
     Bpp    : Integer;
end;
type PVideoData = ^TVideoData;

type TPlayData = record
  w            : Integer;
  h            : Integer;
  Bpp          : Integer;
  status       : TFFP_PLAY_STATUS;
end;
type PPlayData = ^TPlayData;

var sti_events : TFFP_EVENTS;
    RGBBuffer  : pointer ;

procedure PrintDebugMessage( msg : String );
  Var TS   : TTimeStamp;
      DT   : TDateTime;
      dMsg : String;
      pMsg : PChar;
begin
  TS:=DateTimeToTimeStamp(Now);
  DT:=TimeStampToDateTime(TS);
  dMsg := DateTimeToStr(DT) + ': ' + msg;
  pMsg := StrNew(PChar(dMsg));
  PostMessage( frmMain.Handle, WM_USER_EVENT_LOG, PtrInt(pMsg), 0);
end;

procedure EventHandler( sender : pointer ) ; cdecl;
  var handle : HWND;
begin
  handle := TfrmMain(sender).Handle;
  PostMessage( handle, WM_USER_EVENT_UPDATE, 0, 0 );
end;

procedure EventExit( sender : pointer ; exitCode : Integer ) ; cdecl;
   var msg : String;
begin
   msg := Format('[Exit signal : %d]',[exitCode]);
   PrintDebugMessage( msg );
end;

procedure EventInfo( sender : pointer; infoCode : Integer ; msg : PFFP_CHAR ) ; cdecl;
  var dbgMsg : String;
begin
  if (infocode <> Integer(FFP_INFO_DEBUG)) then
  begin
    dbgMsg := String(msg);
    PrintDebugMessage(dbgMsg);
  end;
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

     msg := Format('[Status event: %d]',[Integer(status)]);
     PrintDebugMessage((msg));

     GetMem( pPlay , sizeof(TPlayData) );
     FillChar( pPlay^,Sizeof(TPlayData), 0);
     pPlay^.status := status;
     PostMessage( handle, WM_USER_STATUS, PtrInt(pPlay), 0 );
end;

procedure EventVideo( sender : pointer ; YuvData : PFFP_YUV_DATA ) ; cdecl;
  var handle : HWND;
      msg    : String;
      pPlay  : PPlayData;
begin
     handle := TfrmMain(sender).Handle;
     multimedia_yuv420p_to_rgb32( YuvData,  RGBBuffer);

     GetMem( pPlay , sizeof(TPlayData) );
     FillChar( pPlay^,Sizeof(TPlayData), 0);
     pPlay^.w   := YuvData^.width;
     pPlay^.h   := YuvData^.height;
     pPlay^.Bpp := 4;

     PostMessage( handle, WM_USER_REFRESH, PtrInt(pPlay), 0 );
end;

procedure EventResize(sender : pointer ; w, h : Integer) ; cdecl;
  var msg    : String;
      handle : HWND;
      pPlay  : PPlayData;
begin
     msg := Format('[Resize Event] %d %d',[w,h]);
     PrintDebugMessage(msg);
     handle := TfrmMain(sender).Handle;
     GetMem( pPlay , sizeof(TPlayData) );
     FillChar( pPlay^,Sizeof(TPlayData), 0);
     pPlay^.w := w;
     pPlay^.h := h;
     PostMessage( handle, WM_USER_RESIZE, PtrInt(pPlay), 0 );
end;
{$IFDEF  DEF_OUTPUT_WIN}
function ThreadPlaying(Param : Pointer) : Integer;
{$ELSE}
function ThreadPlaying(Param : Pointer) : PtrInt;
{$ENDIF}
begin
{$IFDEF  DEF_OUTPUT_WIN}
       multimedia_stream_start();
{$ELSE}
       multimedia_stream_start();
{$ENDIF}
  EndThread(0);
end;

procedure TfrmMain.OnUserLog(var Msg : TLMessage);
  var pMsg : PChar;
begin
  pMsg := PChar(Msg.WParam);
  Memo1.Lines.Add(String(pMsg));
  StrDispose(pMsg);
end;

procedure TfrmMain.OnUserEvent(var Msg: TLMessage);
begin
end;

procedure TfrmMain.OnStatus(var Msg: TLMessage);
  var pPlay : PPlayData;
begin
  pPlay := PPlayData(Msg.WParam);

  case pPlay^.status of
  FFP_PAUSED: begin
                   ButtonPause.Caption := 'Resume';
                   end;
  FFP_RESUMED:begin
                   ButtonPause.Caption := 'Pause';
                   end;
  FFP_STOP:   begin
                   StopPlaying();
                   end;
  FFP_EOF:    begin
                   StopPlaying();
                   end;
  else begin
       end;
  end;

  FreeMem(pPlay);
end;

procedure TfrmMain.OnRefreshScreen(var Msg : TLMessage);
  var pPlay : PPlayData;
begin
  pPlay := PPlayData(Msg.WParam);
  updateScreen(RGBBuffer, pPlay^.w, pPlay^.h, pPlay^.Bpp);
  FreeMem(pPlay);
end;

procedure TfrmMain.OnResizeScreen(var Msg: TLMessage);
  var pPlay : PPlayData;
      dMsg   : String;
begin
  pPlay := PPlayData(Msg.WParam);
  resizeScreen(pPlay^.w, pPlay^.h);
  FreeMem(pPlay);
end;

procedure TfrmMain.Timer1Timer(Sender: TObject);
  var msg : String;
begin
  FCurrentTime_Sec := sti_events.current_in_s;
  FDurationTime_mSec := sti_events.duration_in_us div 1000000;
  msg := Format('%f / %d',[FCurrentTime_Sec, FDurationTime_mSec]);
  Label1.Caption := msg;
  ScrollBar1.Max := FDurationTime_mSec ;
  ScrollBar1.Position := Round(FCurrentTime_Sec);
end;

procedure TfrmMain.FormCreate(Sender: TObject);
begin
  ImageRGB.Picture.Bitmap.PixelFormat:=pf32Bit;
  GetMem(RGBBuffer, (4096*1024*4));
  FCurrentTime_Sec := 0;
  FDurationTime_mSec:= 0;
  Timer1.Enabled := False;

  ButtonPlay.Enabled := True;
  ButtonStop.Enabled := False;
  ButtonPause.Enabled:= False;

  FresImage := TBitmap.Create();
  {$IFDEF DEF_OUTPUT_WIN}
  Self.Caption := 'Win32 LazFFPlayer';
  ImageRGB.Picture.Bitmap.PixelFormat := pf32Bit;
  FresImage.PixelFormat := pf32Bit;
  {$ELSE}
  Self.Caption := 'Linux lazPlayer';
  ImageRGB.Picture.Bitmap.PixelFormat := pf24Bit;
  FresImage.PixelFormat := pf24Bit;
  {$ENDIF}

  Memo1.Clear();

  FinitW := Panel1.Width;
  FinitH := Panel1.Height;

  resizeScreen(FinitW, FinitH);
end;

procedure TfrmMain.FormDestroy(Sender: TObject);
begin
  FresImage.Free();
  FreeMem(RGBBuffer);
  PrintDebugMessage('Application Terminated safely!');
end;

procedure TfrmMain.PanelYUVMouseMove(Sender: TObject; Shift: TShiftState; X,
  Y: Integer);
begin
  multimedia_reset_pointer();
end;

procedure TfrmMain.FormResize(Sender: TObject);
begin

end;

procedure TfrmMain.Panel1Resize(Sender: TObject);
begin
  FinitW := Panel1.Width  ;
  FinitH := Panel1.Height ;
  if multimedia_event_loop_alive() = 1 then
  begin
    multimedia_resize_screen(FinitW,FinitH);
  end
  else
  begin
    resizeScreen(FinitW, FinitH);
  end;
end;

procedure TfrmMain.PanelYUVResize(Sender: TObject);
begin

end;

procedure TfrmMain.FormCloseQuery(Sender: TObject; var CanClose: boolean);
begin
    {$IFDEF  DEF_OUTPUT_WIN}
    if multimedia_event_loop_alive() = 1 then
    begin
      multimedia_stream_stop();
      //ShowMessage('Still in playing, terminating...');
      Sleep(500);
      CanClose := True;
    end;
    {$ELSE}
    if multimedia_event_loop_alive() = 1 then
    begin
      multimedia_stream_stop();
      //ShowMessage('Still in playing, terminating...');
      Sleep(500);
      CanClose := True;
    end;
    {$ENDIF}
end;

procedure TfrmMain.ButtonPlayClick(Sender: TObject);
  var mediaFile : AnsiString;
      rtn       : Integer;
      msg       : String;
{$IFNDEF DEF_OUTPUT_WIN}
      XWinID    : TXID;
{$ENDIF}
begin
  sti_events.sender           := self;
{$IFDEF DEF_OUTPUT_WIN}
  sti_events.screenID         := PanelYUV.Handle;
  sti_events.uiType           := FFP_GUI;
{$ELSE}

  XWinID := GDK_WINDOW_XWINDOW(PGtkWidget(PtrUInt(PanelYUV.Handle))^.window);
  sti_events.screenID         := XWinID;
  sti_events.uiType           := FFP_GUI;
{$ENDIF}
  sti_events.eventInfo        := @EventInfo;
  sti_events.eventExit        := @EventExit;
  sti_events.eventAudio       := @EventAudio;
  sti_events.eventResize      := @EventResize;
  sti_events.eventStatus      := @EventPlayStatus;
  sti_events.playStatus       := FFP_STOP;

{$IFDEF DEF_RGB}
  sti_events.eventVideo       := @EventVideo;
  PanelYUV.Enabled            := False;
  PanelYUV.Visible            := False;
  ImageRGB.Enabled            := True;
  ImageRGB.Visible            := True;
{$ELSE}
  sti_events.eventVideo       := nil;
  PanelYUV.Enabled            := True;
  PanelYUV.Visible            := True;
  ImageRGB.Enabled            := False;
  ImageRGB.Visible            := False;
{$ENDIF}

{$IFDEF  DEF_OUTPUT_WIN}
  if OpenDialog.Execute() then
  begin
    mediaFile := AnsiString(OpenDialog.FileName);
    multimedia_set_filename( PFFP_CHAR(mediaFile) );
    if multimedia_init_device( @sti_events ) <> 0 then
    begin
      multimedia_exit();
      ShowMessage('Fail to Init!');
      Exit();
    end;
    if ( multimedia_stream_open( ) = FFP_FALSE ) then
    begin
      multimedia_exit();
      ShowMessage('Fail to open file!');
      Exit();
    end;
    Timer1.Enabled := True;
    ScrollBar1.Max := 100;
    try
      ButtonPlay.Enabled := False;
      ButtonStop.Enabled := True;
      ButtonPause.Enabled:= True;

      StartPlaying();
    except
      ShowMessage('Have a problem to play!');
    end;
  end;
{$ELSE}
       if OpenDialog.Execute() then
       begin
         mediaFile := AnsiString(OpenDialog.FileName);
         multimedia_set_filename( PFFP_CHAR(mediaFile) );

         multimedia_clear_screen( sti_events.screenID, PanelYUV.Width, PanelYUV.Height );

         rtn := multimedia_init_device( @sti_events );
         if  rtn <> 0 then
         begin
              multimedia_exit();
              msg := Format('Fail to init: %d',[rtn]);
              ShowMessage(msg);
              Exit();
         end;

         if ( multimedia_stream_open() = FFP_FALSE ) then
         begin
              multimedia_exit();
              ShowMessage('Fail to open file!');
              Exit();
         end;
         Timer1.Enabled := True;
         ScrollBar1.Max := 100;
         try
            ButtonPlay.Enabled := False;
            ButtonStop.Enabled := True;
            ButtonPause.Enabled:= True;

            startPlaying();
         except
            ShowMessage('Have a problem to play!');
         end;
end;
{$ENDIF}
end;

procedure TfrmMain.ButtonPauseClick(Sender: TObject);
begin
  multimedia_pause_resume();
end;

procedure TfrmMain.ButtonStopClick(Sender: TObject);
begin
  multimedia_stream_stop();
  FCurrentTime_Sec    := 0;
  FDurationTime_mSec  := 0;
  ScrollBar1.Position := 0;
  Timer1.Enabled      := False;
  ButtonPlay.Enabled  := True;
  ButtonStop.Enabled  := False;
  ButtonPause.Enabled := False;
  ButtonPause.Caption := 'Pause';
end;

procedure TfrmMain.stopPlaying();
begin
  ButtonStopClick(self);
end;

procedure TfrmMain.updateTime(Current : double);
begin
  FCurrentTime_Sec := Current;
end;

procedure TfrmMain.resizeScreen(w, h : Integer );
  var center_x, center_y : Integer;
      dMsg : String;
begin
  center_x := Panel1.Width div 2;
  center_y := Panel1.Height div 2;

  PanelYUV.Top := center_y - (h div 2);
  PanelYUV.Left:= center_x - (w div 2);
  ImageRGB.Top := PanelYUV.Top;
  ImageRGB.Left:= PanelYUV.Left;

{$IFDEF DEF_RGB}
  if (w <> FinitW) or (h <> FinitH) then
  begin
    PanelYUV.Width  := 0;
    PanelYUV.Height := 0;
    multimedia_resize_screen( FinitW , FinitH );
  end
  else
  begin
    FinitW := w;
    FinitH := h;
    ImageRGB.Width  := FinitW;
    ImageRGB.Height := FinitH;
    ImageRGB.Picture.Bitmap.Width  := FinitW;
    ImageRGB.Picture.Bitmap.Height := FinitH;
    ImageRGB.Invalidate();

    PanelYUV.Visible:= False;
    PanelYUV.Enabled:= False;
  end;
{$ELSE}
  if (w <> FinitW) or (h <> FinitH) then
  begin
    PanelYUV.Width  := 0;
    PanelYUV.Height := 0;
    multimedia_resize_screen( FinitW , FinitH );
  end
  else
  begin
    FinitW := w;
    FinitH := h;
    PanelYUV.Width  := FinitW;
    PanelYUV.Height := FinitH;
    ImageRGB.Visible:= False;
    ImageRGB.Enabled:= False;
  end;
{$ENDIF}
end;

procedure TfrmMain.UpdateScreen(Buffer : pointer ; w, h, BPP : Integer);
  var
    pSrc, pDst : PByte;
    size : Integer;
begin

  FresImage.BeginUpdate(False);
  FresImage.SetSize(w,h);
  size := w * h * Bpp;
  pSrc := Buffer;
  pDst := FresImage.RawImage.Data;
  Move( pSrc^, pDst^, size );
  FresImage.EndUpdate(False);

  ImageRGB.Picture.Assign(FresImage);

end;

procedure TfrmMain.startPlaying();
  var Data : Integer;
begin
  FThreadPlayingID := BeginThread(@ThreadPlaying, @Data);
  multimedia_resize_screen(FinitW, FinitH);
end;

end.

