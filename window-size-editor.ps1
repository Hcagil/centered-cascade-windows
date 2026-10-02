param([switch]$Probe)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'window-size-editor-core.ps1')

if (-not $Probe) {
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $admin) {
        $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"'
        Start-Process 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' `
            -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden
        return
    }
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$modKey = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@cascade-windows-personal'
$settingsKey = Join-Path $modKey 'Settings'
$mod = Get-ItemProperty -LiteralPath $modKey
$saved = Get-ItemProperty -LiteralPath $settingsKey
if ($mod.Version -ne '2.2') { throw 'The installed Centered Cascade Windows mod has changed. Open its Windhawk settings instead.' }

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$monitorWidth = $screen.Width
$monitorHeight = $screen.Height

$form = New-Object System.Windows.Forms.Form
$form.Text = 'Centered Cascade Windows - Size Editor'
$form.ClientSize = New-Object System.Drawing.Size(370, 248)
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.StartPosition = 'CenterScreen'
$form.Font = New-Object System.Drawing.Font('Segoe UI', 10)

function Add-Label([string]$text, [int]$x, [int]$y, [int]$width) {
    $label = New-Object System.Windows.Forms.Label
    $label.Text = $text
    $label.Location = New-Object System.Drawing.Point($x, $y)
    $label.Size = New-Object System.Drawing.Size($width, 28)
    $form.Controls.Add($label)
}

Add-Label 'Width (px)' 20 24 130
Add-Label 'Height (px)' 20 68 130
Add-Label 'Screen ratio' 20 112 130

$widthBox = New-Object System.Windows.Forms.TextBox
$widthBox.Location = New-Object System.Drawing.Point(165, 20)
$widthBox.Size = New-Object System.Drawing.Size(180, 30)
$widthBox.Text = [string]$saved.width
$form.Controls.Add($widthBox)

$heightBox = New-Object System.Windows.Forms.TextBox
$heightBox.Location = New-Object System.Drawing.Point(165, 64)
$heightBox.Size = New-Object System.Drawing.Size(180, 30)
$heightBox.Text = [string]$saved.height
$form.Controls.Add($heightBox)

$ratioBox = New-Object System.Windows.Forms.ComboBox
$ratioBox.Location = New-Object System.Drawing.Point(165, 108)
$ratioBox.Size = New-Object System.Drawing.Size(180, 30)
$ratioBox.DropDownStyle = 'DropDownList'
[void]$ratioBox.Items.AddRange(@('Current monitor', '3:2', '16:10', '16:9'))
$ratioBox.SelectedItem = if ($saved.screenRatio -in @('3:2', '16:10', '16:9')) {
    [string]$saved.screenRatio
} else { 'Current monitor' }
$form.Controls.Add($ratioBox)

$status = New-Object System.Windows.Forms.Label
$status.Location = New-Object System.Drawing.Point(20, 158)
$status.Size = New-Object System.Drawing.Size(330, 42)
$status.ForeColor = [System.Drawing.Color]::DimGray
$form.Controls.Add($status)

$saveButton = New-Object System.Windows.Forms.Button
$saveButton.Text = 'Save to Windhawk'
$saveButton.Location = New-Object System.Drawing.Point(190, 204)
$saveButton.Size = New-Object System.Drawing.Size(155, 32)
$form.Controls.Add($saveButton)
$form.AcceptButton = $saveButton

$script:updating = $false
$script:driver = 'width'

function Get-SelectedRatio {
    if ($ratioBox.SelectedItem -eq 'Current monitor') { return 'monitor' }
    return [string]$ratioBox.SelectedItem
}

function Update-Pair {
    if ($script:updating) { return }
    $width = 0
    $height = 0
    if (-not [int]::TryParse($widthBox.Text, [ref]$width) -or
        -not [int]::TryParse($heightBox.Text, [ref]$height) -or
        $width -lt 300 -or $height -lt 200 -or
        $width -gt 10000 -or $height -gt 10000) {
        $status.Text = 'Enter width 300-10000 and height 200-10000.'
        $saveButton.Enabled = $false
        return
    }
    $pair = Get-LinkedSize -Width $width -Height $height -Ratio (Get-SelectedRatio) `
        -Driver $script:driver -MonitorWidth $monitorWidth -MonitorHeight $monitorHeight
    if ($pair.Width -gt 10000 -or $pair.Height -gt 10000) {
        $status.Text = 'Calculated size exceeds 10000 px. Enter a smaller number.'
        $saveButton.Enabled = $false
        return
    }
    $script:updating = $true
    try {
        if ($script:driver -eq 'width') { $heightBox.Text = [string]$pair.Height }
        else { $widthBox.Text = [string]$pair.Width }
    } finally { $script:updating = $false }
    $status.Text = "Window size: $($pair.Width) x $($pair.Height) px"
    $saveButton.Enabled = $true
}

$widthBox.Add_TextChanged({
    if (-not $script:updating) { $script:driver = 'width'; Update-Pair }
})
$heightBox.Add_TextChanged({
    if (-not $script:updating) { $script:driver = 'height'; Update-Pair }
})
$ratioBox.Add_SelectedIndexChanged({ Update-Pair })

$saveButton.Add_Click({
    try {
        $width = [int]$widthBox.Text
        $height = [int]$heightBox.Text
        $ratio = Get-SelectedRatio
        Save-LinkedSettings -ModKey $modKey -Width $width -Height $height -Ratio $ratio
        $status.Text = "Saved $width x $height px to Windhawk."
    } catch {
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Could not save', 'OK', 'Error') | Out-Null
    }
})

Update-Pair
if ($Probe) {
    $widthBox.Text = '2500'
    $ratioBox.SelectedItem = '16:9'
    if ($heightBox.Text -ne '1406') { throw "Changing width or ratio did not update height: $($heightBox.Text)" }
    $heightBox.Text = '1600'
    if ($widthBox.Text -ne '2844') { throw "Changing height did not update width: $($widthBox.Text)" }
    'Editor live-update probe passed'
    $form.Dispose()
    return
}
[void]$form.ShowDialog()
$form.Dispose()
