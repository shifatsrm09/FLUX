!include "WinMessages.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

; Version: pass it on the command line so it matches CMakeLists.txt, e.g.
;   makensis /DVERSION=1.5.1 installer\installer.nsi
; The self-updater downloads the release asset named  FLUX-<version>-Setup.exe
; and runs it silently:  FLUX-<version>-Setup.exe /S /NOGUI /D=<install dir>
!ifndef VERSION
  !define VERSION "2.2.3"
!endif

; General — Discord-style per-user zero-click setup:
; - CRCCheck off: eliminates the classic Windows "Verifying installer: XX%" dialog
; - SilentInstall silent: suppresses NSIS's default #32770 extracting dialog completely
; - FluxSetupSplash.exe renders the exact 300x350 rounded dark Discord-style card at 60 FPS
Name "FLUX"
Caption "FLUX Setup"
OutFile "..\dist\Release-v${VERSION}\FLUX-${VERSION}-Setup.exe"
Unicode True
CRCCheck off
SilentInstall silent
InstallDir "$LOCALAPPDATA\FLUX"
InstallDirRegKey HKCU "Software\FLUX" "Install_Dir"
RequestExecutionLevel user
AutoCloseWindow true
ShowInstDetails nevershow
XPStyle on
BrandingText " "

; Branding and icons
Icon "..\resources\flux.ico"
UninstallIcon "..\resources\flux.ico"

; Uninstaller Pages
UninstPage uninstConfirm
UninstPage instfiles

; Language
LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"

; Version Info
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "FLUX"
VIAddVersionKey /LANG=1033 "Comments" "Internal Media Streaming Client"
VIAddVersionKey /LANG=1033 "CompanyName" "FLUX"
VIAddVersionKey /LANG=1033 "LegalCopyright" "FLUX"
VIAddVersionKey /LANG=1033 "FileDescription" "FLUX Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}.0"

Var NoGuiMode
Var ProgressFile

Function WriteProgress
    Exch $R8
    StrCmp $NoGuiMode "1" wp_done
    ClearErrors
    FileOpen $9 "$ProgressFile" w
    IfErrors wp_done
    FileWrite $9 "$R8$\r$\n"
    FileClose $9
    wp_done:
    Pop $R8
FunctionEnd

!macro WriteProgress TEXT
    Push "${TEXT}"
    Call WriteProgress
!macroend

Function .onInit
    StrCpy $NoGuiMode "0"
    ${GetParameters} $R0

    ClearErrors
    ${GetOptions} $R0 "/NOGUI" $R1
    IfErrors +2
    StrCpy $NoGuiMode "1"

    ClearErrors
    ${GetOptions} $R0 "/S" $R1
    IfErrors +2
    StrCpy $NoGuiMode "1"

    StrCmp $NoGuiMode "1" skip_splash

    ; Interactive install: preserve existing per-user install location if set
    ReadRegStr $0 HKCU "Software\FLUX" "Install_Dir"
    StrCmp $0 "" +2
    StrCpy $INSTDIR $0

    ; Extract and launch the 60 FPS Discord-style setup splash popup
    InitPluginsDir
    StrCpy $ProgressFile "$PLUGINSDIR\progress.txt"
    !insertmacro WriteProgress "12|INSTALLING UPDATE 1 OF 1"

    SetOutPath "$PLUGINSDIR"
    File "..\build\Release\FluxSetupSplash.exe"
    Exec '"$PLUGINSDIR\FluxSetupSplash.exe" "$ProgressFile" "${VERSION}"'

    skip_splash:
FunctionEnd

Section "FLUX" SecMain
    SectionIn RO

    !insertmacro WriteProgress "20|INSTALLING UPDATE 1 OF 1"

    ; Close any running FLUX instance so files are unlocked
    nsExec::ExecToStack 'taskkill /F /IM FLUX.exe'

    !insertmacro WriteProgress "35|INSTALLING UPDATE 1 OF 1"
    SetOutPath "$INSTDIR"
    File /r "..\dist\FLUX-${VERSION}\*.*"

    ; Write uninstaller
    !insertmacro WriteProgress "88|INSTALLING UPDATE 1 OF 1"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Per-user Registry keys (no admin elevation required)
    WriteRegStr HKCU "Software\FLUX" "Install_Dir" "$INSTDIR"

    ; Windows Installed Apps / Add or Remove Programs (per-user)
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "DisplayName" "FLUX"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "Publisher" "FLUX"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "DisplayIcon" "$INSTDIR\resources\flux.ico"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX" "NoRepair" 1

    ; Shortcuts
    !insertmacro WriteProgress "94|INSTALLING UPDATE 1 OF 1"
    CreateDirectory "$SMPROGRAMS\FLUX"
    CreateShortcut "$SMPROGRAMS\FLUX\FLUX.lnk" "$INSTDIR\FLUX.exe" "" "$INSTDIR\resources\flux.ico" 0
    CreateShortcut "$SMPROGRAMS\FLUX\Uninstall FLUX.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    CreateShortcut "$DESKTOP\FLUX.lnk" "$INSTDIR\FLUX.exe" "" "$INSTDIR\resources\flux.ico" 0

    !insertmacro WriteProgress "100|STARTING..."

    StrCmp $NoGuiMode "1" skip_launch
    Sleep 450
    !insertmacro WriteProgress "DONE|STARTING..."
    Exec '"$INSTDIR\FLUX.exe"'
    Sleep 550
    skip_launch:
SectionEnd

Section "Uninstall"
    nsExec::ExecToStack 'taskkill /F /IM FLUX.exe'

    ; Remove Shortcuts
    Delete "$DESKTOP\FLUX.lnk"
    Delete "$SMPROGRAMS\FLUX\FLUX.lnk"
    Delete "$SMPROGRAMS\FLUX\Uninstall FLUX.lnk"
    RMDir "$SMPROGRAMS\FLUX"

    ; Remove Registry keys
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FLUX"
    DeleteRegKey HKCU "Software\FLUX"

    ; Remove Files and Directories
    RMDir /r "$INSTDIR"
SectionEnd
