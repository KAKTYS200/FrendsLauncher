; Frends Launcher installer.
;
; Build with: ISCC.exe /Qp frends.iss
; The portable zip is produced separately, this one writes a normal Windows
; installation with a Start Menu group, an uninstaller and an optional desktop
; shortcut. Player data stays in %APPDATA%\FrendsLauncher and is never touched.

[Setup]
AppId={{8E4C1F62-7B1A-4E0D-9C2B-FRENDS0001}
AppName=Frends Launcher
AppVersion=1.2
AppVerName=Frends Launcher 1.2
AppPublisher=Frends Launcher
AppPublisherURL=https://github.com/KAKTYS200/FrendsLauncher
AppSupportURL=https://github.com/KAKTYS200/FrendsLauncher
AppUpdatesURL=https://github.com/KAKTYS200/FrendsLauncher/releases
DefaultDirName={autopf}\Frends Launcher
DefaultGroupName=Frends Launcher
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=FrendsLauncher-1.2-setup
SetupIconFile=..\program_info\frendslauncher.ico
UninstallDisplayIcon={app}\frendslauncher.exe
UninstallDisplayName=Frends Launcher 1.2
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\dist\FrendsLauncher\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Frends Launcher"; Filename: "{app}\frendslauncher.exe"
Name: "{group}\{cm:UninstallProgram,Frends Launcher}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Frends Launcher"; Filename: "{app}\frendslauncher.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\frendslauncher.exe"; Description: "{cm:LaunchProgram,Frends Launcher}"; Flags: nowait postinstall skipifsilent
