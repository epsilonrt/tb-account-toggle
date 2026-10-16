<#
    Shared logic for enabling/disabling Thunderbird accounts (dot-source this file).

    A "disabled" account is removed from mail.accountmanager.accounts, so
    Thunderbird does not load it at all (no connection, not displayed).
    Its settings (mail.account.* / mail.server.* / mail.identity.*) and its
    downloaded messages stay untouched, so re-enabling restores it as it was.

    Reactivation data is kept in prefs.js itself (Thunderbird preserves
    unknown prefs):
      mail.accountmanager.disabled_accounts  disabled account ids
      mail.accountmanager.accounts_order     original order of all accounts
    Both prefs are removed once no account is disabled.

    The default account cannot be disabled.
    Thunderbird must be closed: it rewrites prefs.js on exit.
#>

$script:PrefAccounts = 'mail.accountmanager.accounts'
$script:PrefDefault  = 'mail.accountmanager.defaultaccount'
$script:PrefDisabled = 'mail.accountmanager.disabled_accounts'
$script:PrefOrder    = 'mail.accountmanager.accounts_order'
$script:Utf8NoBom    = New-Object System.Text.UTF8Encoding($false)

function Get-TbDefaultProfilePath {
    $root = Join-Path $env:APPDATA 'Thunderbird'
    $ini = Get-Content (Join-Path $root 'profiles.ini')
    $rel = ($ini | Where-Object { $_ -match '^Default=Profiles/' } | Select-Object -First 1) -replace '^Default=', ''
    if (-not $rel) { throw 'Profil par défaut introuvable dans profiles.ini' }
    Join-Path $root $rel
}

function Test-TbRunning { [bool](Get-Process thunderbird -ErrorAction SilentlyContinue) }

# Read a pref value from prefs.js lines, or $null when absent
function Get-TbPref($Lines, [string]$Name) {
    $pattern = '^user_pref\("' + [regex]::Escape($Name) + '", (.*)\);$'
    foreach ($l in $Lines) {
        if ($l -match $pattern) { return $Matches[1].Trim('"') }
    }
    return $null
}

# Replace (or add) a string pref
function Set-TbStringPref($Lines, [string]$Name, [string]$Value) {
    $new = 'user_pref("' + $Name + '", "' + $Value + '");'
    $pattern = '^user_pref\("' + [regex]::Escape($Name) + '",'
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match $pattern) { $Lines[$i] = $new; return }
    }
    $Lines.Add($new)
}

function Remove-TbPref($Lines, [string]$Name) {
    $pattern = '^user_pref\("' + [regex]::Escape($Name) + '",'
    $null = $Lines.RemoveAll([Predicate[string]] { param($l) $l -match $pattern })
}

function Split-TbIds([string]$Value) { @($Value -split ',' | Where-Object { $_ }) }

# Load prefs.js and describe every mail account (enabled or disabled)
function Read-TbState([string]$ProfilePath) {
    if (-not $ProfilePath) { $ProfilePath = Get-TbDefaultProfilePath }
    $prefsFile = Join-Path $ProfilePath 'prefs.js'
    $legacyStateFile = Join-Path $ProfilePath 'tb-accounts-state.json'
    if (-not (Test-Path $prefsFile)) { throw "prefs.js introuvable : $prefsFile" }

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.AddRange([IO.File]::ReadAllLines($prefsFile, $script:Utf8NoBom))

    $enabledIds  = Split-TbIds (Get-TbPref $lines $script:PrefAccounts)
    $disabledIds = Split-TbIds (Get-TbPref $lines $script:PrefDisabled)
    $defaultId   = Get-TbPref $lines $script:PrefDefault

    # Master list = every known account in its original order (enabled + disabled)
    $master = [System.Collections.Generic.List[string]]::new()
    $savedOrder = Split-TbIds (Get-TbPref $lines $script:PrefOrder)
    if (-not $savedOrder -and (Test-Path $legacyStateFile)) {
        # Migrate the order saved by an earlier version of the script
        $savedOrder = @((Get-Content $legacyStateFile -Raw | ConvertFrom-Json).accounts)
    }
    foreach ($id in @($savedOrder) + $enabledIds + $disabledIds) {
        if ($id -and -not $master.Contains($id)) { $master.Add($id) }
    }

    $accounts = @(foreach ($acc in $master) {
        $srv = Get-TbPref $lines "mail.account.$acc.server"
        if (-not $srv) { continue }
        if ((Get-TbPref $lines "mail.server.$srv.type") -eq 'none') { continue }   # skip Local Folders
        $idents = (Get-TbPref $lines "mail.account.$acc.identities") -split ','
        [pscustomobject]@{
            Account = $acc
            Name    = Get-TbPref $lines "mail.server.$srv.name"
            Email   = Get-TbPref $lines "mail.identity.$($idents[0]).useremail"
            Active  = $enabledIds -contains $acc
            Default = $acc -eq $defaultId
        }
    })

    [pscustomobject]@{
        PrefsFile       = $prefsFile
        LegacyStateFile = $legacyStateFile
        Lines           = $lines
        Master          = $master
        EnabledIds      = $enabledIds
        Accounts        = $accounts
    }
}

# Write prefs.js so that exactly $ActiveIds (among $State.Accounts) are enabled.
# Returns the backup file path.
function Save-TbState($State, [string[]]$ActiveIds) {
    if (Test-TbRunning) { throw 'Thunderbird est ouvert. Fermez-le avant de modifier les comptes.' }

    $default = $State.Accounts | Where-Object Default
    if ($default -and $ActiveIds -notcontains $default.Account) {
        throw "Le compte par défaut ($($default.Name)) ne peut pas être désactivé. Aucune modification effectuée."
    }

    $backup = "$($State.PrefsFile).bak-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    Copy-Item $State.PrefsFile $backup

    # Rebuild the active list in the original order; Local Folders etc. keep their state
    $known = @($State.Accounts | ForEach-Object Account)
    $activeList = [System.Collections.Generic.List[string]]::new()
    $disabledList = [System.Collections.Generic.List[string]]::new()
    foreach ($id in $State.Master) {
        $isActive = if ($known -contains $id) { $ActiveIds -contains $id } else { $State.EnabledIds -contains $id }
        if ($isActive) { $activeList.Add($id) } else { $disabledList.Add($id) }
    }

    $lines = $State.Lines
    Set-TbStringPref $lines $script:PrefAccounts ($activeList -join ',')
    if ($disabledList.Count -gt 0) {
        Set-TbStringPref $lines $script:PrefDisabled ($disabledList -join ',')
        Set-TbStringPref $lines $script:PrefOrder ($State.Master -join ',')
    } else {
        # Nothing disabled anymore: leave no trace in prefs.js
        Remove-TbPref $lines $script:PrefDisabled
        Remove-TbPref $lines $script:PrefOrder
    }

    [IO.File]::WriteAllLines($State.PrefsFile, $lines, $script:Utf8NoBom)
    if (Test-Path $State.LegacyStateFile) { Remove-Item $State.LegacyStateFile }
    foreach ($a in $State.Accounts) { $a.Active = $ActiveIds -contains $a.Account }
    return $backup
}
