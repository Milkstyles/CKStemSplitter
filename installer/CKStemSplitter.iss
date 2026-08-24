#define MyAppName "CK Stem Splitter"
#define MyAppVersion "0.7.0"
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
Source: "..\staging\audition-extension\*"; DestDir: "{commoncf32}\Adobe\CEP\extensions\com.commercialkings.ckstemsplitter"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\staging\bridge\CKStemBridge.exe"; DestDir: "{commoncf32}\Adobe\CEP\extensions\com.commercialkings.ckstemsplitter\bin"; Flags: ignoreversion

[Dirs]
Name: "{commonappdata}\Commercial Kings\CK Stem Splitter\engine"

[Registry]
; Audition still hosts CEP panels. The beta is installed unpacked, so enable local
; extensions for the installing user across supported CEP runtime generations.
Root: HKCU; Subkey: "Software\Adobe\CSXS.9"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"; Flags: preservestringtype
Root: HKCU; Subkey: "Software\Adobe\CSXS.10"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"; Flags: preservestringtype
Root: HKCU; Subkey: "Software\Adobe\CSXS.11"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"; Flags: preservestringtype
Root: HKCU; Subkey: "Software\Adobe\CSXS.12"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"; Flags: preservestringtype
Root: HKCU; Subkey: "Software\Adobe\CSXS.13"; ValueType: string; ValueName: "PlayerDebugMode"; ValueData: "1"; Flags: preservestringtype

[Run]
Filename: "{cmd}"; Parameters: "/C echo CK Stem Splitter installed successfully."; Flags: runhidden nowait
