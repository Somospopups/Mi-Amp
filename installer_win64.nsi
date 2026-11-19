Unicode True
!include "MUI2.nsh"

Name "MI AMP"
OutFile "Instalar_MI_AMP_Windows_x64.exe"
InstallDir "$PROGRAMFILES64\Popups Audio\MI AMP"
RequestExecutionLevel admin

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\MI AMP.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir MI AMP (Standalone)"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Spanish"

Section "Plugin VST3 (64-bit para DAWs)" SecVST3
  SectionIn RO
  SetOutPath "$COMMONFILES64\VST3"
  File /r "build\MiAmp_artefacts\Release\VST3\MI AMP.vst3"
SectionEnd

Section "App Independiente Standalone (.exe)" SecStandalone
  SetOutPath "$INSTDIR"
  File "build\MiAmp_artefacts\Release\Standalone\MI AMP.exe"
  File "build\MiAmp_artefacts\Release\Standalone\onnxruntime.dll"

  CreateDirectory "$SMPROGRAMS\MI AMP"
  CreateShortcut "$SMPROGRAMS\MI AMP\MI AMP.lnk" "$INSTDIR\MI AMP.exe"
  CreateShortcut "$SMPROGRAMS\MI AMP\Desinstalar MI AMP.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\MI AMP.lnk" "$INSTDIR\MI AMP.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MiAmpBass" "DisplayName" "MI AMP Bass Edition (VST3 + Standalone)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MiAmpBass" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MiAmpBass" "Publisher" "Popups Audio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MiAmpBass" "DisplayVersion" "1.0.0"
SectionEnd

LangString DESC_SecVST3 ${LANG_SPANISH} "Instala MI AMP.vst3 en C:\Program Files\Common Files\VST3 para Reaper, Ableton Live, FL Studio, Cubase y Studio One."
LangString DESC_SecStandalone ${LANG_SPANISH} "Instala la aplicacion independiente MI AMP.exe con Cabezal 8x10, Pedalera de 5 pedales (XV Bass, Compresor, Chorus, Phaser, Reverb), Drop Tuner y Afinador YIN."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} $(DESC_SecVST3)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStandalone} $(DESC_SecStandalone)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\MI AMP.vst3"
  Delete "$INSTDIR\MI AMP.exe"
  Delete "$INSTDIR\onnxruntime.dll"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\Popups Audio"

  Delete "$DESKTOP\MI AMP.lnk"
  RMDir /r "$SMPROGRAMS\MI AMP"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MiAmpBass"
SectionEnd
