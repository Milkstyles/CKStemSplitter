#define MyAppName "CK Stem Splitter"
#define MyAppVersion "1.0.8"
#define MyAppPublisher "Commercial Kings"
#define MyAppURL "https://commercialkings.com"

[Setup]
AppId={{9A98B982-3A8D-4A89-A7DF-FF2762A4E0F1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\Common Files\VST3
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=CK-Stem-Splitter-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Uninstallable=yes
CreateUninstallRegKey=yes
SetupLogging=yes

[Files]
Source: "..\staging\CK Stem Splitter.vst3\*"; DestDir: "{app}\CK Stem Splitter.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\staging\engine\ckstem-engine\*"; DestDir: "{commonappdata}\Commercial Kings\CK Stem Splitter\engine\ckstem-engine"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\staging\engine\models\*"; DestDir: "{commonappdata}\Commercial Kings\CK Stem Splitter\engine\models"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\staging\companion\CKStemBridge.exe"; DestDir: "{commonappdata}\Commercial Kings\CK Stem Splitter\companion"; Flags: ignoreversion

[Dirs]
Name: "{commonappdata}\Commercial Kings\CK Stem Splitter\engine"
Name: "{commonappdata}\Commercial Kings\CK Stem Splitter\companion"

[InstallDelete]
Type: files; Name: "{userappdata}\Commercial Kings\CK Stem Splitter\automation-process.txt"
Type: files; Name: "{userappdata}\Commercial Kings\CK Stem Splitter\automation-ready.txt"
; Remove the discontinued CEP export panel so Audition shows one CK interface.
Type: filesandordirs; Name: "{commoncf32}\Adobe\CEP\extensions\com.commercialkings.ckstemsplitter"

[Run]
Filename: "{cmd}"; Parameters: "/C echo CK Stem Splitter installed successfully."; Flags: runhidden nowait

