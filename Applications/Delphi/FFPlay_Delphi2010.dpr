program FFPlay_Delphi2010;

uses
  Forms,
  umain in 'umain.pas' {frmMain},
  FFPlay in 'FFPlay.pas';

{$R *.res}

begin
  ReportMemoryLeaksOnShutDown := True;

  Application.Initialize;
  Application.MainFormOnTaskbar := True;
  Application.CreateForm(TfrmMain, frmMain);
  Application.Run;
end.
