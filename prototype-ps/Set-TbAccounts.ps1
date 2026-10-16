<#
.SYNOPSIS
    Temporarily enable/disable Thunderbird mail accounts (command line).

.DESCRIPTION
    See TbAccounts.Core.ps1 for how accounts are disabled.
    For a graphical window, use TbAccounts-Gui.ps1.

.EXAMPLE
    .\Set-TbAccounts.ps1            # interactive menu
    .\Set-TbAccounts.ps1 5          # toggle account 5
    .\Set-TbAccounts.ps1 3,5        # toggle accounts 3 and 5
    .\Set-TbAccounts.ps1 -List      # list only
#>
[CmdletBinding()]
param(
    # Account numbers to toggle (as shown by -List); prompts when omitted
    [Parameter(Position = 0)]
    [int[]]$Toggle,
    [switch]$List,
    # Profile folder; defaults to the default profile found in profiles.ini
    [string]$ProfilePath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TbAccounts.Core.ps1')

$state = Read-TbState $ProfilePath
$accounts = $state.Accounts

function Show-Accounts {
    $i = 1
    foreach ($a in $accounts) {
        $st = if ($a.Active) { 'ACTIF  ' } else { 'inactif' }
        $mark = if ($a.Default) { '  (compte par défaut)' } else { '' }
        '{0,2}. [{1}] {2,-14} {3}{4}' -f $i, $st, $a.Name, $a.Email, $mark
        $i++
    }
}

Show-Accounts
if ($List) { return }

if (Test-TbRunning) { throw 'Thunderbird est ouvert. Fermez-le avant de modifier les comptes.' }

if ($Toggle) {
    $selected = $Toggle
} else {
    $answer = Read-Host "`nNuméros à basculer (ex: 1,3), vide pour quitter"
    if ([string]::IsNullOrWhiteSpace($answer)) { return }
    $selected = $answer -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { [int]$_ }
}
foreach ($n in $selected) {
    if ($n -lt 1 -or $n -gt $accounts.Count) { throw "Numéro invalide : $n" }
}

$toggled = @($selected | ForEach-Object { $accounts[$_ - 1].Account })
$activeIds = @($accounts | Where-Object { $_.Active -xor ($toggled -contains $_.Account) } | ForEach-Object Account)

$backup = Save-TbState $state $activeIds
Write-Host "`nprefs.js mis à jour (sauvegarde : $backup)`n"
Show-Accounts
