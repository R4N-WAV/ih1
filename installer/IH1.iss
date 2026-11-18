; IH1 setup wizard (Inno Setup 6). Built by CI: ISCC.exe /DAppVersion=x.y.z /DSourceDir=<stage> installer\IH1.iss
; Installs the VST3 into the standard system location (C:\Program Files\Common Files\VST3), which every
; VST3 host scans (FL Studio, Cubase, ...). The standalone app is optional.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\stage"
#endif

[Setup]
AppId={{7B3E8C51-6A0D-4E7F-9B1C-1A2D3E4F5061}
AppName=IH1
AppVersion={#AppVersion}
AppVerName=IH1 {#AppVersion}
AppPublisher=R4N
DefaultDirName={autopf}\IH1
DisableDirPage=no
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\THIRD_PARTY_NOTICES.txt
InfoBeforeFile={#SourceDir}\INSTALL_NOTES.txt
OutputDir=..\dist
OutputBaseFilename=IH1-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName=IH1 {#AppVersion}
UninstallDisplayIcon={app}\IH1.exe
SetupLogging=yes

[Types]
Name: "full"; Description: "VST3 plug-in and standalone app"
Name: "vst3"; Description: "VST3 plug-in only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "IH1 VST3 plug-in (installed to Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "IH1 standalone app (for trying IH1 without a DAW)"; Types: full

[Files]
Source: "{#SourceDir}\VST3\IH1.vst3\*"; DestDir: "{commoncf64}\VST3\IH1.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SourceDir}\Standalone\IH1.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\IH1"; Filename: "{app}\IH1.exe"; Components: standalone
Name: "{autoprograms}\IH1 README"; Filename: "{app}\README.md"

[Run]
Filename: "{app}\IH1.exe"; Description: "Launch the IH1 standalone app"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

[Messages]
FinishedLabel=IH1 has been installed.%n%nThe VST3 plug-in is in C:\Program Files\Common Files\VST3\IH1.vst3. In FL Studio use Options > Manage plugins > Find more plugins; in Cubase open Studio > VST Plug-in Manager and rescan if IH1 does not appear.
