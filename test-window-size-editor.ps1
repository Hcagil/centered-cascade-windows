$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'window-size-editor-core.ps1')

function Assert-Size($actual, $width, $height) {
    if ($actual.Width -ne $width -or $actual.Height -ne $height) {
        throw "Expected ${width}x${height}, got $($actual.Width)x$($actual.Height)"
    }
}

Assert-Size (Get-LinkedSize -Width 2600 -Height 1850 -Ratio '3:2' -Driver width -MonitorWidth 3000 -MonitorHeight 2000) 2600 1733
Assert-Size (Get-LinkedSize -Width 2600 -Height 1850 -Ratio '3:2' -Driver height -MonitorWidth 3000 -MonitorHeight 2000) 2775 1850
Assert-Size (Get-LinkedSize -Width 2600 -Height 1850 -Ratio '16:9' -Driver width -MonitorWidth 3000 -MonitorHeight 2000) 2600 1463
Assert-Size (Get-LinkedSize -Width 2600 -Height 1850 -Ratio 'monitor' -Driver width -MonitorWidth 3000 -MonitorHeight 2000) 2600 1733
Assert-Size (Get-LinkedSize -Width 2600 -Height 1850 -Ratio '16:10' -Driver height -MonitorWidth 3000 -MonitorHeight 2000) 2960 1850

$testKey = "HKCU:\Software\CenteredCascadeEditorTest_$PID"
try {
    New-Item -Path $testKey -Force | Out-Null
    New-Item -Path (Join-Path $testKey 'Settings') -Force | Out-Null
    New-ItemProperty -Path $testKey -Name SettingsChangeTime -PropertyType DWord -Value 1 | Out-Null
    Save-LinkedSettings -ModKey $testKey -Width 2600 -Height 1733 -Ratio '3:2'
    $stored = Get-ItemProperty -LiteralPath (Join-Path $testKey 'Settings')
    $timestamp = (Get-ItemProperty -LiteralPath $testKey).SettingsChangeTime
    if ($stored.width -ne 2600 -or $stored.height -ne 1733 -or
        $stored.screenRatio -ne '3:2' -or $timestamp -le 1) {
        throw 'Save did not persist linked dimensions and notify Windhawk.'
    }
} finally {
    if (Test-Path -LiteralPath $testKey) { Remove-Item -LiteralPath $testKey -Recurse -Force }
}
'5 linked-size cases and registry save passed'
