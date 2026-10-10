!include "MUI2.nsh"
!include "x64.nsh"

; Version: pass it on the command line so it matches CMakeLists.txt, e.g.
;   makensis /DVERSION=0.0.4 installer\installer.nsi
; The self-updater downloads the release asset named  FLUX-<version>-Setup.exe
; and runs it silently:  FLUX-<version>-Setup.exe /S /D=<install dir>
!ifndef VERSION
  !define VERSION "0.0.5"
!endif

; General
Name "FLUX"
OutFile "..\dist\Release-v${VERSION}\FLUX-${VERSION}-Setup.exe"
Unicode True
InstallDir "$PROGRAMFILES64\FLUX"
InstallDirRegKey HKLM "Software\FLUX" "Install_Dir"
RequestExecutionLevel admin

; Branding and visual
!define MUI_ICON "..\resources\flux.ico"
!define MUI_UNICON "..\resources\flux.ico"
!define MUI_ABORTWARNING

; Modern UI Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\FLUX.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch FLUX"
!insertmacro MUI_PAGE_FINISH

; Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

; Version Info
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "FLUX"
VIAddVersionKey "Comments" "Internal Media Streaming Client"
VIAddVersionKey "CompanyName" "FLUX"
VIAddVersionKey "LegalCopyright" "FLUX"
VIAddVersionKey "FileDescription" "FLUX \x2014 Internal Media Streaming Client Installer"
VIAddVersionKey "FileVersion" "${VERSION}.0"
VIAddVersionKey "ProductVersion" "${VERSION}.0"

; The installer is 32-bit but the install location is written to the 64-bit registry
; view, so read it from there. A silent update passes /D=<dir> explicitly; keep that.
Function .onInit
    IfSilent done
    SetRegView 64
    ReadRegStr $0 HKLM "Software\FLUX" "Install_Dir"
    StrCmp $0 "" done
    StrCpy $INSTDIR $0
    done:
FunctionEnd

Section "FLUX (required)" SecMain
    SectionIn RO
    SetRegView 64

    SetOutPath "$INSTDIR"
    File /r "..\dist\FLUX-${VERSION}\*.*"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Registry keys
    WriteRegStr HKLM "Software\FLUX" "Install_Dir" "$INSTDIR"

    ; Windows Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "DisplayName" "FLUX"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "Publisher" "FLUX"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "DisplayIcon" "$INSTDIR\resources\flux.ico"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "NoRepair" 1

    ; Shortcuts
    CreateDirectory "$SMPROGRAMS\FLUX"
    CreateShortcut "$SMPROGRAMS\FLUX\FLUX.lnk" "$INSTDIR\FLUX.exe" "" "$INSTDIR\resources\flux.ico" 0
    CreateShortcut "$SMPROGRAMS\FLUX\Uninstall FLUX.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    CreateShortcut "$DESKTOP\FLUX.lnk" "$INSTDIR\FLUX.exe" "" "$INSTDIR\resources\flux.ico" 0
SectionEnd

Section "Uninstall"
    SetRegView 64

    ; Remove Shortcuts
    Delete "$DESKTOP\FLUX.lnk"
    Delete "$SMPROGRAMS\FLUX\FLUX.lnk"
    Delete "$SMPROGRAMS\FLUX\Uninstall FLUX.lnk"
    RMDir "$SMPROGRAMS\FLUX"

    ; Remove Registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX"
    DeleteRegKey HKLM "Software\FLUX"

    ; Remove Files and Directories
    RMDir /r "$INSTDIR"
SectionEnd
