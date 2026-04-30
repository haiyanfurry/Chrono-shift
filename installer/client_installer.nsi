; Chrono-shift 客户端安装脚本
; NSIS v3.12+
; 功能: 完整的 UI 资源显式列表、注册表键、卸载程序

!define PRODUCT_NAME "Chrono-shift Client"
!define PRODUCT_VERSION "0.1.0"
!define PRODUCT_PUBLISHER "Chrono-shift Team"
!define PRODUCT_WEB_SITE "https://chrono-shift.example.com"

Name "${PRODUCT_NAME}"
OutFile "Chrono-shift-Client-Setup-${PRODUCT_VERSION}.exe"
InstallDir "$LOCALAPPDATA\Chrono-shift"
RequestExecutionLevel user

; ============================================================
; 版本信息
; ============================================================
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1033 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "© ${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=1033 "FileDescription" "${PRODUCT_NAME} Installer"

; ============================================================
; 安装程序
; ============================================================
Section "Install" SecMain

    ; ---- 主程序文件 ----
    SetOutPath "$INSTDIR"
    File "..\client\build\Release\chrono-client.exe"
    File /r "..\client\build\Release\*.dll"
    File /r "..\client\security\target\release\chrono_client_security.dll"

    ; ---- UI HTML ----
    SetOutPath "$INSTDIR\ui"
    File "..\client\ui\index.html"

    ; ---- UI CSS ----
    SetOutPath "$INSTDIR\ui\css"
    File "..\client\ui\css\variables.css"
    File "..\client\ui\css\global.css"
    File "..\client\ui\css\login.css"
    File "..\client\ui\css\main.css"
    File "..\client\ui\css\chat.css"
    File "..\client\ui\css\community.css"

    ; ---- UI CSS 主题 ----
    SetOutPath "$INSTDIR\ui\css\themes"
    File "..\client\ui\css\themes\default.css"

    ; ---- UI JavaScript ----
    SetOutPath "$INSTDIR\ui\js"
    File "..\client\ui\js\app.js"
    File "..\client\ui\js\api.js"
    File "..\client\ui\js\auth.js"
    File "..\client\ui\js\chat.js"
    File "..\client\ui\js\community.js"
    File "..\client\ui\js\contacts.js"
    File "..\client\ui\js\ipc.js"
    File "..\client\ui\js\theme_engine.js"
    File "..\client\ui\js\utils.js"

    ; ---- UI 资源 ----
    SetOutPath "$INSTDIR\ui\assets\images"
    File "..\client\ui\assets\images\default_avatar.png"

    ; ---- 数据目录 ----
    CreateDirectory "$INSTDIR\data\config"
    CreateDirectory "$INSTDIR\data\cache"
    CreateDirectory "$INSTDIR\data\themes"
    CreateDirectory "$INSTDIR\data\secure"
    CreateDirectory "$INSTDIR\logs"

    ; ---- 开始菜单快捷方式 ----
    CreateDirectory "$SMPROGRAMS\Chrono-shift"
    CreateShortCut "$SMPROGRAMS\Chrono-shift\Chrono-shift.lnk" \
                  "$INSTDIR\chrono-client.exe" "" "$INSTDIR\chrono-client.exe" 0
    CreateShortCut "$SMPROGRAMS\Chrono-shift\卸载 Client.lnk" \
                  "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

    ; ---- 桌面快捷方式 ----
    CreateShortCut "$DESKTOP\Chrono-shift.lnk" \
                  "$INSTDIR\chrono-client.exe" "" "$INSTDIR\chrono-client.exe" 0

    ; ---- 注册表: 卸载信息 ----
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "DisplayIcon" "$INSTDIR\chrono-client.exe,0"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                     "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                      "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient" \
                      "NoRepair" 1

    ; ---- 注册表: 应用设置 (默认值) ----
    WriteRegStr HKCU "Software\Chrono-shift\Client" "ServerHost" "127.0.0.1"
    WriteRegDWORD HKCU "Software\Chrono-shift\Client" "ServerPort" 8080
    WriteRegStr HKCU "Software\Chrono-shift\Client" "DataDir" "$INSTDIR\data"
    WriteRegStr HKCU "Software\Chrono-shift\Client" "LogDir" "$INSTDIR\logs"

SectionEnd

; ============================================================
; 卸载程序
; ============================================================
Section "Uninstall"

    ; ---- 终止正在运行的进程 ----
    ; 使用 ExecWait 不是最佳方案，改用 nsProcess 插件 (如有)
    ; 简单实现: 告知用户先关闭应用
    ; nsProcess::_FindProcess "chrono-client.exe" 可选的插件方式
    ; 这里不做进程终止，以免产生副作用

    ; ---- 删除快捷方式 ----
    Delete "$DESKTOP\Chrono-shift.lnk"
    RMDir /r "$SMPROGRAMS\Chrono-shift"

    ; ---- 删除安装目录 (保留用户数据？默认全部删除) ----
    ; 注意: RMDir /r 是递归删除，如有用户聊天记录等需保留
    ; 可改为选择性删除:
    RMDir /r "$INSTDIR\ui"
    RMDir /r "$INSTDIR\logs"
    Delete "$INSTDIR\chrono-client.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\uninstall.exe"
    ; 询问是否同时删除用户数据
    ; MessageBox MB_YESNO|MB_ICONQUESTION "是否同时删除用户数据 (聊天记录、配置等)?" IDNO skip_data
    RMDir /r "$INSTDIR\data"
    ; skip_data:
    RMDir "$INSTDIR" ; 仅在目录为空时删除

    ; ---- 删除注册表 ----
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChronoShiftClient"
    DeleteRegKey HKCU "Software\Chrono-shift\Client"

SectionEnd
