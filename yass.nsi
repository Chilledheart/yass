!include "MUI.nsh"

!define MUI_ABORTWARNING # This will warn the user if they exit from the installer.
!define MUI_ICON "..\src\win32\yass.ico"
!define MUI_UNICON "..\src\win32\yass.ico"

!insertmacro MUI_PAGE_WELCOME # Welcome to the installer page.
!insertmacro MUI_PAGE_LICENSE "..\GPL-2.0.rtf"
!insertmacro MUI_PAGE_DIRECTORY # In which folder install page.
!insertmacro MUI_PAGE_INSTFILES # Installing page.
!insertmacro MUI_PAGE_FINISH # Finished installation page.

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"

# Name of the installer (usually the name of the application to install).
Name "Yet Another Shadow Socket"

# define name of installer
OutFile "yass-installer.exe"

# define installation directory
InstallDir $LOCALAPPDATA\YASS

# For removing Start Menu shortcut in Windows 7
RequestExecutionLevel user

# start default section
Section "Yet Another Shadow Socket"

    # set the installation directory as the destination for the following actions
    SetOutPath $INSTDIR

    # create the uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    # point the new shortcut at the program uninstaller
    CreateShortcut "$SMPROGRAMS\YASS.lnk" "$INSTDIR\yass.exe"
    CreateShortcut "$SMPROGRAMS\YASS Uninstall.lnk" "$INSTDIR\uninstall.exe"

    File "yass.exe"
    File /nonfatal "crashpad_handler.exe"
    File /nonfatal "icudtl.dat"
    # DLL PLACEHOLDER
    File "LICENSE"

SectionEnd

# uninstaller section start
Section "uninstall"
    # kill current process
    ExecWait "TASKKILL /F /IM yass.exe /T"

    # first, delete the uninstaller
    Delete "$INSTDIR\uninstall.exe"

    # second, remove the link from the start menu
    Delete "$SMPROGRAMS\YASS.lnk"
    Delete "$SMPROGRAMS\YASS Uninstall.lnk"

    # delete the remaining files
    Rmdir /r $INSTDIR

# uninstaller section end
SectionEnd
