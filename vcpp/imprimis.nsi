Name "Imprimis"
Icon icon.ico
OutFile "imprimis_windows.exe"

InstallDir $PROGRAMFILES64\Imprimis

InstallDirRegKey HKLM "Software\Imprimis" "Install_Dir"

RequestExecutionLevel admin
SetCompressor /SOLID lzma
XPStyle on

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Imprimis (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR\config
  File /r "..\config\*.*"
  SetOutPath $INSTDIR\bin64
  File /r "..\bin64\*.*"
  SetOutPath $INSTDIR\media
  File /r "..\media\*.*"
  SetOutPath $INSTDIR\doc
  File /r "..\doc\*.*"
  SetOutPath $INSTDIR
  File "..\imprimis.bat"
  File "icon.ico"

  WriteRegStr HKLM SOFTWARE\Imprimis "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Imprimis" "DisplayName" "Imprimis"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Imprimis" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Imprimis" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Imprimis" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  IfFileExists "$DOCUMENTS\My Games\Imprimis\config\saved.cfg" ConfigFound NoConfig  
  ConfigFound:
     Delete "$DOCUMENTS\My Games\Imprimis\config\old-saved.cfg"
     Rename "$DOCUMENTS\My Games\Imprimis\config\saved.cfg" "$DOCUMENTS\My Games\Imprimis\config\old-saved.cfg"
  NoConfig:

SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Imprimis"

  CreateDirectory "$DOCUMENTS\My Games\Imprimis"
 
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$INSTDIR\Imprimis.lnk"                "$INSTDIR\imprimis.bat" "" "$INSTDIR\bin64\Imprimis.exe" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Imprimis\Imprimis.lnk"   "$INSTDIR\imprimis.bat" "" "$INSTDIR\bin64\Imprimis.exe" 0 SW_SHOWMINIMIZED
;  CreateShortCut "$SMPROGRAMS\Imprimis\README.lnk"      "$INSTDIR\README.html"   "" "$INSTDIR\README.html" 0

  CreateShortCut "$INSTDIR\User Data.lnk"                "$DOCUMENTS\My Games\Imprimis"
  CreateShortCut "$SMPROGRAMS\Imprimis\User Data.lnk"   "$DOCUMENTS\My Games\Imprimis"  

  CreateShortCut "$SMPROGRAMS\Imprimis\Uninstall.lnk"   "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Imprimis"
  DeleteRegKey HKLM SOFTWARE\Imprimis

  RMDir /r "$SMPROGRAMS\Imprimis"
  RMDir /r "$INSTDIR"

SectionEnd
