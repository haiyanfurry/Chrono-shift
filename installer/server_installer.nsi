; Chrono-shift 服务端安装脚本
; NSIS v3.12

Name "Chrono-shift Server"
OutFile "Chrono-shift-Server-Setup.exe"
InstallDir "$PROGRAMFILES64\Chrono-shift\Server"
RequestExecutionLevel admin

Section "Install"
    SetOutPath "$INSTDIR"
    
    ; 复制服务端文件
    File /r "..\server\build\Release\*.exe"
    File /r "..\server\build\Release\*.dll"
    
    ; 创建数据目录
    CreateDirectory "$INSTDIR\data\db"
    CreateDirectory "$INSTDIR\data\storage"
    
    ; 创建配置文件
    FileOpen $0 "$INSTDIR\server_config.ini" w
    FileWrite $0 "[Server]$\r$\n"
    FileWrite $0 "port=8080$\r$\n"
    FileWrite $0 "db_path=./data/db/chrono.db$\r$\n"
    FileWrite $0 "storage_path=./data/storage$\r$\n"
    FileClose $0
    
    ; 创建开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\Chrono-shift"
    CreateShortCut "$SMPROGRAMS\Chrono-shift\Chrono-shift Server.lnk" "$INSTDIR\chrono-server.exe"
    CreateShortCut "$SMPROGRAMS\Chrono-shift\卸载 Server.lnk" "$INSTDIR\uninstall.exe"
    
    ; 写入卸载信息
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftServer" \
                     "DisplayName" "Chrono-shift Server"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftServer" \
                     "UninstallString" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    RMDir /r "$INSTDIR"
    RMDir /r "$SMPROGRAMS\Chrono-shift"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftServer"
SectionEnd
