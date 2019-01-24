object frmMain: TfrmMain
  Left = 356
  Top = 248
  BorderIcons = [biSystemMenu, biMaximize]
  BorderStyle = bsDialog
  Caption = 'Delphi FFPlay'
  ClientHeight = 501
  ClientWidth = 1074
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  OnCloseQuery = FormCloseQuery
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 507
    Top = 47
    Width = 31
    Height = 13
    Caption = 'Label1'
  end
  object mButtonPlay: TButton
    Left = 24
    Top = 24
    Width = 75
    Height = 25
    Caption = 'Play'
    TabOrder = 0
    OnClick = mButtonPlayClick
  end
  object mButtonStop: TButton
    Left = 105
    Top = 24
    Width = 75
    Height = 25
    Caption = 'Stop'
    TabOrder = 1
    OnClick = mButtonStopClick
  end
  object ScrollBar1: TScrollBar
    Left = 568
    Top = 24
    Width = 481
    Height = 17
    LargeChange = 10
    PageSize = 0
    TabOrder = 2
  end
  object mButtonPause: TButton
    Left = 186
    Top = 24
    Width = 75
    Height = 25
    Caption = 'Pause'
    TabOrder = 3
    OnClick = mButtonPauseClick
  end
  object Panel1: TPanel
    Left = 24
    Top = 72
    Width = 1025
    Height = 417
    Caption = 'Panel1'
    TabOrder = 4
    object Image1: TImage
      Left = 8
      Top = 8
      Width = 1009
      Height = 361
    end
    object Panel2: TPanel
      Left = 8
      Top = 8
      Width = 1009
      Height = 401
      Caption = 'Panel2'
      TabOrder = 0
    end
  end
  object Button1: TButton
    Left = 320
    Top = 24
    Width = 75
    Height = 25
    Caption = 'Button1'
    TabOrder = 5
  end
  object OpenDialog: TOpenDialog
    Left = 480
    Top = 24
  end
  object Timer1: TTimer
    Enabled = False
    OnTimer = Timer1Timer
    Left = 528
    Top = 24
  end
end
