object frmMain: TfrmMain
  Left = 254
  Height = 441
  Top = 221
  Width = 1030
  Caption = 'lazPlayer'
  ClientHeight = 441
  ClientWidth = 1030
  OnCloseQuery = FormCloseQuery
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  OnResize = FormResize
  Position = poScreenCenter
  LCLVersion = '6.6'
  object Panel1: TPanel
    Left = 8
    Height = 376
    Top = 8
    Width = 794
    Anchors = [akTop, akLeft, akRight, akBottom]
    ClientHeight = 376
    ClientWidth = 794
    Color = clBlack
    ParentColor = False
    TabOrder = 0
    OnResize = Panel1Resize
    object ImageRGB: TImage
      Left = 16
      Height = 360
      Top = 8
      Width = 504
      Center = True
      Proportional = True
      Stretch = True
    end
    object PanelYUV: TGroupBox
      Left = 16
      Height = 369
      Top = 0
      Width = 505
      Color = clBlack
      ParentColor = False
      TabOrder = 0
      OnMouseMove = PanelYUVMouseMove
      OnResize = PanelYUVResize
    end
  end
  object ScrollBar1: TScrollBar
    Left = 8
    Height = 13
    Top = 394
    Width = 793
    Anchors = [akLeft, akRight, akBottom]
    PageSize = 0
    TabOrder = 1
  end
  object ButtonPlay: TButton
    Left = 808
    Height = 33
    Top = 8
    Width = 211
    Anchors = [akTop, akRight]
    Caption = 'Play'
    OnClick = ButtonPlayClick
    TabOrder = 2
  end
  object ButtonStop: TButton
    Left = 808
    Height = 34
    Top = 48
    Width = 211
    Anchors = [akTop, akRight]
    Caption = 'Stop'
    OnClick = ButtonStopClick
    TabOrder = 3
  end
  object ButtonPause: TButton
    Left = 808
    Height = 35
    Top = 88
    Width = 210
    Anchors = [akTop, akRight]
    Caption = 'Pause'
    OnClick = ButtonPauseClick
    TabOrder = 4
  end
  object Label1: TLabel
    Left = 8
    Height = 17
    Top = 424
    Width = 45
    Anchors = [akLeft, akBottom]
    Caption = 'Label1'
    ParentColor = False
  end
  object Memo1: TMemo
    Left = 808
    Height = 287
    Top = 152
    Width = 214
    Anchors = [akTop, akRight, akBottom]
    Font.Height = -8
    Font.Name = 'Sans'
    Lines.Strings = (
      'Memo1'
    )
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssAutoBoth
    TabOrder = 5
  end
  object Label2: TLabel
    Left = 804
    Height = 17
    Top = 134
    Width = 109
    Anchors = [akTop, akRight]
    Caption = 'Debug Message'
    ParentColor = False
  end
  object Timer1: TTimer
    Enabled = False
    OnTimer = Timer1Timer
    Left = 592
    Top = 320
  end
  object OpenDialog: TOpenDialog
    Left = 736
    Top = 320
  end
end
