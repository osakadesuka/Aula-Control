#ifndef AppVersion
  #error Build this installer using packaging/build-setup.ps1.
#endif
#ifndef PayloadDir
  #error PayloadDir is required.
#endif
#ifndef OutputDir
  #error OutputDir is required.
#endif
#define AppName "AULA Control"
#define AppExe "aula_control.exe"

[Setup]
AppId={{B67295C2-0F10-4A2C-B113-225FC0C0075A}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=AULA Control
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
DefaultDirName={localappdata}\Programs\AULA Control
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible and not arm64
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
SetupIconFile=..\resources\aula_control.ico
WizardStyle=modern dynamic
WizardSizePercent=110
DisableProgramGroupPage=yes
DisableWelcomePage=yes
DisableStartupPrompt=yes
Compression=lzma2
SolidCompression=yes
OutputDir={#OutputDir}
OutputBaseFilename=AULA-Control-{#AppVersion}-Setup-x64
AppMutex=Local\AulaControl.Desktop.1
SetupMutex=Local\AulaControl.Setup.1
CloseApplications=no
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Messages]
english.SetupAppRunningError=AULA Control is open.%n%nChoose Exit AULA Control from its tray menu or Settings, then click OK. Closing the window may only hide it.
spanish.SetupAppRunningError=AULA Control está abierta.%n%nElige Salir de AULA Control desde la bandeja o Preferencias y pulsa Aceptar. Cerrar la ventana puede ocultarla solamente.
english.UninstallAppRunningError=AULA Control is open.%n%nChoose Exit AULA Control from its tray menu or Settings, then click OK.
spanish.UninstallAppRunningError=AULA Control está abierta.%n%nElige Salir de AULA Control desde la bandeja o Preferencias y pulsa Aceptar.

[CustomMessages]
english.DesktopShortcut=Create a desktop shortcut
spanish.DesktopShortcut=Crear un acceso directo en el escritorio
english.LaunchApp=Open AULA Control
spanish.LaunchApp=Abrir AULA Control
english.StartupMigrationFailed=The Windows startup shortcut could not be updated. Open Settings in AULA Control and turn Start with Windows off and on again.
spanish.StartupMigrationFailed=No se pudo actualizar el inicio con Windows. Abre Preferencias en AULA Control y desactiva y vuelve a activar Iniciar con Windows.

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopShortcut}"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\aula_control.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\rgb_prueba.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\bateria_f75.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\assets\keyboard-actual-v1.png"; DestDir: "{app}\assets"; Flags: ignoreversion
Source: "{#PayloadDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "{#PayloadDir}\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\release-manifest.json"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\AULA Control"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"
Name: "{autodesktop}\AULA Control"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchApp}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Code]
const
  StartupKey = 'Software\Microsoft\Windows\CurrentVersion\Run';
  StartupValue = 'AULA Control';

function IsAulaStartup(Command: String): Boolean;
var
  ClosingQuote: Integer;
  Executable, Arguments, Name: String;
begin
  Result := False;
  Command := Trim(Command);
  if (Length(Command) < 3) or (Copy(Command, 1, 1) <> '"') then exit;
  Delete(Command, 1, 1);
  ClosingQuote := Pos('"', Command);
  if ClosingQuote = 0 then exit;
  Executable := Copy(Command, 1, ClosingQuote - 1);
  Arguments := Trim(Copy(Command, ClosingQuote + 1, Length(Command)));
  Name := ExtractFileName(Executable);
  Result := ((CompareText(Name, 'aula_control.exe') = 0) or
    (CompareText(Name, 'aula_control_glass.exe') = 0)) and
    (Arguments = '--background');
end;

function InstalledStartupCommand: String;
begin
  Result := '"' + ExpandConstant('{app}\{#AppExe}') + '" --background';
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Command: String;
begin
  // Preserve the existing choice, including the development-folder build.
  // Never create startup on a first installation or overwrite another command.
  if (CurStep = ssPostInstall) and
      RegQueryStringValue(HKCU, StartupKey, StartupValue, Command) and
      IsAulaStartup(Command) then begin
    if not RegWriteStringValue(HKCU, StartupKey, StartupValue, InstalledStartupCommand) then begin
      Log('Could not migrate the existing AULA Control startup value.');
      SuppressibleMsgBox(CustomMessage('StartupMigrationFailed'), mbError, MB_OK, IDOK);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  // Remove only our own path; a later registration at a different path wins.
  if (CurUninstallStep = usUninstall) and
      RegQueryStringValue(HKCU, StartupKey, StartupValue, Command) and
      (CompareText(Command, InstalledStartupCommand) = 0) then
    RegDeleteValue(HKCU, StartupKey, StartupValue);
  // User data in LocalAppData\AulaControl is deliberately never removed.
end;
