; Localized names used by the Inno Setup installer (included by CPack)

[CustomMessages]
english.AppTitle=Thunderbird Accounts
french.AppTitle=Comptes Thunderbird

; Declared here rather than with CPACK_INNOSETUP_RUN_EXECUTABLES, which escapes
; the nested {cm:AppTitle} constant
[Run]
Filename: "{app}\tb-account-toggle.exe"; Description: "{cm:LaunchProgram,{cm:AppTitle}}"; Flags: nowait postinstall skipifsilent

; Remove what the uninstaller did not log (or could not delete), then {app} itself
[UninstallDelete]
Type: filesandordirs; Name: "{app}"
