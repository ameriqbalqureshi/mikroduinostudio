; Inno Setup script for MikroDuino Studio.
; Packages the PyInstaller onedir build (dist\MikroDuinoStudio\) — which already
; contains the bundled avr-gcc/avrdude toolchains and SDK — into a single
; Windows installer, no separate toolchain download or PATH edits required.
;
; Build order:
;   1. powershell -File packaging\build.ps1     (produces dist\MikroDuinoStudio\)
;   2. ISCC packaging\installer.iss             (produces installer\MikroDuinoStudio-Setup-<ver>.exe)
;
; Keep AppVersion in sync with main.py's app.setApplicationVersion(...).

#define AppName "MikroDuino Studio"
#define AppVersion "0.2.0"
#define AppPublisher "MikroDuino"
#define DistDir "..\dist\MikroDuinoStudio"

[Setup]
AppId={{B7D9F3D2-6C3E-4E4F-9B7C-6B4C1B0A6B8E}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\MikroDuino Studio
DefaultGroupName=MikroDuino Studio
DisableProgramGroupPage=yes
OutputDir=..\installer
OutputBaseFilename=MikroDuinoStudio-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\MikroDuinoStudio.exe
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\MikroDuino Studio"; Filename: "{app}\MikroDuinoStudio.exe"
Name: "{group}\Uninstall MikroDuino Studio"; Filename: "{uninstallexe}"
Name: "{autodesktop}\MikroDuino Studio"; Filename: "{app}\MikroDuinoStudio.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MikroDuinoStudio.exe"; Description: "Launch MikroDuino Studio"; Flags: nowait postinstall skipifsilent
