#define MyAppName "CK AI Voice Insert"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Commercial Kings"
#define MyAppId "{{E1DF22D5-A315-48A7-B727-65B8478D6601}"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={userappdata}\Adobe\CEP\extensions\CKAIVoiceInsert
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist\voice-insert
OutputBaseFilename=CK-AI-Voice-Insert-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=yes
CreateUninstallRegKey=yes
SetupLogging=yes
CloseApplications=no
RestartApplications=no

[Files]
Source: "..\staging\CKAIVoiceInsert\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; The first test build is unsigned, so CEP debug mode must be enabled for the
; Audition/CEP runtime versions that may host the panel.
[Registry]
Root: HKCU; Subkey: "Software\Adobe\CSXS.7"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"
Root: HKCU; Subkey: "Software\Adobe\CSXS.8"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"
Root: HKCU; Subkey: "Software\Adobe\CSXS.9"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"
Root: HKCU; Subkey: "Software\Adobe\CSXS.10"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"
Root: HKCU; Subkey: "Software\Adobe\CSXS.11"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"
Root: HKCU; Subkey: "Software\Adobe\CSXS.12"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"

[Run]
Filename: "{cmd}"; Parameters: "/C echo CK AI Voice Insert installed successfully."; Flags: runhidden nowait

[Code]
function InitializeSetup(): Boolean;
begin
  MsgBox(
    'Close Adobe Audition before continuing. After installation, restart Audition and open Window > Extensions > CK AI Voice Insert.',
    mbInformation,
    MB_OK
  );
  Result := True;
end;
