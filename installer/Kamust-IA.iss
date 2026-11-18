; Inno Setup script for the Windows installer.
; Build with: ISCC.exe installer\Kamust-IA.iss
; Paths are relative to this file, so it expects the CMake output in ..\build.

#define MyAppName "Kamust IA"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Kamust"

[Setup]
AppId={{8F4A2C1E-6B3D-4E7A-9C2F-1D5E8B0A7C31}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; Installs into Common Files\VST3, the standard folder that Studio One and other hosts scan.
DefaultDirName={commoncf}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
OutputBaseFilename=KamustIA-Setup-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\build\Kamust_IA_artefacts\Release\VST3\Kamust IA.vst3\*"; DestDir: "{app}\Kamust IA.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Kamust IA.vst3"

[Messages]
FinishedLabel=Kamust IA was installed to the VST3 folder. Restart your DAW and rescan plugins.
