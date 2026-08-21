; Inno Setup script for the Progressions installer.
;
;   iscc /DAppVersion=1.0.0 /DPluginDir=<folder holding Progressions.vst3> ^
;        /DOutputDir=<where to write the exe> packaging\windows\progressions.iss
;
; The plugin goes into the vendor folder inside the shared VST3 directory,
; which is where hosts scan and where the macOS installer puts it too.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef PluginDir
  #define PluginDir "..\..\build-plugin\plugin\Progressions_artefacts\Release\VST3"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
AppId={{A6F2C9E4-8B3D-4E17-9C55-2D0F7B1A64E3}
AppName=Progressions
AppVersion={#AppVersion}
AppPublisher=Nowhr Dynamics
AppPublisherURL=https://github.com/andrebeltrame/claudeapp
DefaultDirName={commoncf64}\VST3\Nowhr Dynamics
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=Progressions
UninstallFilesDir={commoncf64}\VST3\Nowhr Dynamics\Progressions-uninstall
OutputDir={#OutputDir}
OutputBaseFilename=Progressions-{#AppVersion}-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; The shared VST3 folder is under Program Files, so this needs elevation.
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Messages]
WelcomeLabel2=This will install Progressions by Nowhr Dynamics.%n%nThe VST3 goes into the shared plug-in folder, so any DAW on this machine will find it. Restart your DAW and rescan your plug-ins afterwards.

[Files]
; A VST3 on Windows is a folder, same as on macOS - copy the whole tree.
Source: "{#PluginDir}\Progressions.vst3\*"; DestDir: "{app}\Progressions.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{app}"; Description: "Open the plug-in folder"; \
    Flags: postinstall shellexec skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Progressions.vst3"
