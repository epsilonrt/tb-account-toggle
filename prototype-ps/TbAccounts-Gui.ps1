<#
.SYNOPSIS
    Small window with one switch per Thunderbird account (default account excluded).

.DESCRIPTION
    Uses the same logic as Set-TbAccounts.ps1 (TbAccounts.Core.ps1).
    If Thunderbird is open when applying, it offers to close it gracefully,
    applies the changes, then restarts it.
    Launch with:  pwsh -STA -WindowStyle Hidden -File TbAccounts-Gui.ps1
#>
param(
    # Profile folder; defaults to the default profile found in profiles.ini
    [string]$ProfilePath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TbAccounts.Core.ps1')

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$title = 'Comptes Thunderbird'

function Show-Error([string]$msg) {
    [void][System.Windows.Forms.MessageBox]::Show($msg, $title, 'OK', 'Error')
}

try { $state = Read-TbState $ProfilePath } catch { Show-Error $_.Exception.Message; return }

# Ask Thunderbird to close like a click on its close button; never kill it.
# Returns the executable path for the restart, or $null when it did not close.
function Close-Thunderbird {
    $procs = @(Get-Process thunderbird -ErrorAction SilentlyContinue)
    $exe = ($procs | Where-Object Path | Select-Object -First 1).Path
    foreach ($p in $procs) { if ($p.MainWindowHandle -ne 0) { [void]$p.CloseMainWindow() } }
    $deadline = (Get-Date).AddSeconds(30)
    while ((Test-TbRunning) -and (Get-Date) -lt $deadline) {
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 300
    }
    if (Test-TbRunning) { return $null }
    if (-not $exe) { $exe = Join-Path $env:ProgramFiles 'Mozilla Thunderbird\thunderbird.exe' }
    return $exe
}

# --- Window layout ---
$form = New-Object System.Windows.Forms.Form
$form.Text = $title
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.StartPosition = 'CenterScreen'
$form.AutoSize = $true
$form.AutoSizeMode = 'GrowAndShrink'
$form.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$form.Padding = New-Object System.Windows.Forms.Padding(12)

$layout = New-Object System.Windows.Forms.FlowLayoutPanel
$layout.FlowDirection = 'TopDown'
$layout.AutoSize = $true
$layout.WrapContents = $false
$form.Controls.Add($layout)

$default = $state.Accounts | Where-Object Default
if ($default) {
    $lblDefault = New-Object System.Windows.Forms.Label
    $lblDefault.AutoSize = $true
    $lblDefault.ForeColor = [System.Drawing.SystemColors]::GrayText
    $lblDefault.Text = "Compte par défaut (toujours actif) : $($default.Name) – $($default.Email)"
    $lblDefault.Margin = New-Object System.Windows.Forms.Padding(3, 0, 3, 10)
    $layout.Controls.Add($lblDefault)
}

$checks = @{}
foreach ($a in $state.Accounts | Where-Object { -not $_.Default }) {
    $cb = New-Object System.Windows.Forms.CheckBox
    $cb.AutoSize = $true
    $cb.Text = "$($a.Name)  –  $($a.Email)"
    $cb.Checked = $a.Active
    $cb.Tag = $a.Account
    $layout.Controls.Add($cb)
    $checks[$a.Account] = $cb
}

$status = New-Object System.Windows.Forms.Label
$status.AutoSize = $true
$status.Margin = New-Object System.Windows.Forms.Padding(3, 10, 3, 3)
$layout.Controls.Add($status)

$buttons = New-Object System.Windows.Forms.FlowLayoutPanel
$buttons.AutoSize = $true
$buttons.Anchor = 'Right'
$layout.Controls.Add($buttons)

$btnApply = New-Object System.Windows.Forms.Button
$btnApply.Text = 'Appliquer'
$btnApply.AutoSize = $true
$btnApply.Enabled = $false
$buttons.Controls.Add($btnApply)

$btnClose = New-Object System.Windows.Forms.Button
$btnClose.Text = 'Fermer'
$btnClose.AutoSize = $true
$btnClose.Add_Click({ $form.Close() })
$buttons.Controls.Add($btnClose)
$form.CancelButton = $btnClose

# Enable "Apply" only when a switch differs from the saved state
function Update-Pending {
    $pending = @($state.Accounts | Where-Object { -not $_.Default -and $checks[$_.Account].Checked -ne $_.Active }).Count
    $btnApply.Enabled = $pending -gt 0
    $status.Text = if ($pending) { "$pending modification(s) en attente" } else { 'Cochée = compte actif' }
}
foreach ($cb in $checks.Values) { $cb.Add_CheckedChanged({ Update-Pending }) }
Update-Pending

$btnApply.Add_Click({
    $restartExe = $null
    if (Test-TbRunning) {
        $answer = [System.Windows.Forms.MessageBox]::Show(
            "Thunderbird doit être fermé pour appliquer les changements.`n`nLe fermer maintenant ? Il sera relancé ensuite.",
            $title, 'YesNo', 'Question')
        if ($answer -ne 'Yes') { return }
        $form.UseWaitCursor = $true
        $status.Text = 'Fermeture de Thunderbird…'
        $restartExe = Close-Thunderbird
        $form.UseWaitCursor = $false
        if (-not $restartExe) {
            Show-Error "Thunderbird ne s'est pas fermé (fenêtre de confirmation ouverte, ou réduit dans la barre des tâches ?).`n`nFermez-le vous-même puis cliquez à nouveau sur Appliquer."
            Update-Pending
            return
        }
    }

    try {
        $activeIds = @($state.Accounts | Where-Object { $_.Default -or $checks[$_.Account].Checked } | ForEach-Object Account)
        [void](Save-TbState $state $activeIds)
        Update-Pending
        $status.Text = 'Modifications appliquées.'
    } catch {
        Show-Error $_.Exception.Message
    }

    if ($restartExe -and (Test-Path $restartExe)) {
        $status.Text = 'Modifications appliquées, Thunderbird relancé.'
        Start-Process $restartExe
    }
})

[void]$form.ShowDialog()
