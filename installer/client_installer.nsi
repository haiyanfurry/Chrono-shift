; Chrono-shift 客户端安装脚本
; NSIS v3.12

Name "Chrono-shift Client"
OutFile "Chrono-shift-Client-Setup.exe"
InstallDir "$LOCALAPPDATA\Chrono-shift"
RequestExecutionLevel user

Section "Install"
    SetOutPath "$INSTDIR"
    
    ; 复制客户端文件
    File /r "..\client\build\Release\*.exe"
    File /r "..\client\build\Release\*.dll"
    
    ; 复制前端资源
    SetOutPath "$INSTDIR\ui"
    File /r "..\client\ui\*.*"
    
    ; 创建数据目录
    CreateDirectory "$INSTDIR\data\config"
    CreateDirectory "$INSTDIR\data\cache"
    CreateDirectory "$INSTDIR\data\themes"
    CreateDirectory "$INSTDIR\data\secure"
    
    ; 创建开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\Chrono-shift"
    CreateShortCut "$SMPROGRAMS\Chrono-shift\Chrono-shift.lnk" "$INSTDIR\chrono-client.exe"
    CreateShortCut "$SMPROGRAMS\Chrono-shift\卸载 Client.lnk" "$INSTDIR\uninstall.exe"
    
    ; 桌面快捷方式
    CreateShortCut "$DESKTOP\Chrono-shift.lnk" "$INSTDIR\chrono-client.exe"
    
    ; 写入卸载信息
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "DisplayName" "Chrono-shift Client"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "UninstallString" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    RMDir /r "$INSTDIR"
    RMDir /r "$SMPROGRAMS\Chrono-shift"
    Delete "$DESKTOP\Chrono-shift.lnk"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient"
SectionEnd
