; Alpha Monster - Windows installer (Inno Setup 6)
#ifndef Version
  #define Version "1.0.0"
#endif

[Setup]
AppId={{6E1F2A9C-3B7D-4C55-9A0E-AB12CD34EF56}
AppName=Alpha Monster
AppVersion={#Version}
AppVerName=Alpha Monster {#Version}
AppPublisher=Alpha Monster
AppPublisherURL=https://github.com/djfabiobachata/Alpha-Monster
DefaultDirName={autopf}\Alpha Monster
DefaultGroupName=Alpha Monster
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=AlphaMonster-{#Version}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayName=Alpha Monster {#Version}

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Installazione completa"
Name: "custom"; Description: "Personalizzata"; Flags: iscustom

[Components]
Name: "mix"; Description: "Alpha Monster Mix (app di mixing con modello IA)"; Types: full custom
Name: "vst3"; Description: "Alpha Monster Master - plugin VST3"; Types: full custom
Name: "standalone"; Description: "Alpha Monster Master - app standalone"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Crea un'icona sul desktop per Alpha Monster Mix"; Components: mix

[Files]
Source: "{#Stage}\Apps\Alpha Monster Mix.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: mix
Source: "{#Stage}\Apps\Models\*"; DestDir: "{app}\Models"; Flags: ignoreversion; Components: mix
Source: "{#Stage}\Apps\Alpha Monster Master.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#Stage}\VST3\Alpha Monster Master.vst3\*"; DestDir: "{commoncf64}\VST3\Alpha Monster Master.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#Stage}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\LEGGIMI.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Alpha Monster Mix"; Filename: "{app}\Alpha Monster Mix.exe"; Components: mix
Name: "{group}\Alpha Monster Master"; Filename: "{app}\Alpha Monster Master.exe"; Components: standalone
Name: "{group}\Disinstalla Alpha Monster"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Alpha Monster Mix"; Filename: "{app}\Alpha Monster Mix.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Alpha Monster Mix.exe"; Description: "Avvia Alpha Monster Mix"; Flags: nowait postinstall skipifsilent; Components: mix
