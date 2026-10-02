function Get-LinkedSize {
    param(
        [int]$Width,
        [int]$Height,
        [ValidateSet('monitor', '3:2', '16:10', '16:9')][string]$Ratio,
        [ValidateSet('width', 'height')][string]$Driver,
        [int]$MonitorWidth,
        [int]$MonitorHeight
    )
    if ($Width -le 0 -or $Height -le 0 -or $MonitorWidth -le 0 -or $MonitorHeight -le 0) {
        throw 'Dimensions must be positive.'
    }
    switch ($Ratio) {
        '3:2' { $ratioWidth = 3; $ratioHeight = 2 }
        '16:10' { $ratioWidth = 16; $ratioHeight = 10 }
        '16:9' { $ratioWidth = 16; $ratioHeight = 9 }
        default { $ratioWidth = $MonitorWidth; $ratioHeight = $MonitorHeight }
    }
    if ($Driver -eq 'width') {
        $Height = [int][Math]::Round($Width * $ratioHeight / $ratioWidth, [MidpointRounding]::AwayFromZero)
    } else {
        $Width = [int][Math]::Round($Height * $ratioWidth / $ratioHeight, [MidpointRounding]::AwayFromZero)
    }
    [pscustomobject]@{ Width = $Width; Height = $Height }
}

function Save-LinkedSettings {
    param(
        [string]$ModKey,
        [int]$Width,
        [int]$Height,
        [ValidateSet('monitor', '3:2', '16:10', '16:9')][string]$Ratio
    )
    $settingsKey = Join-Path $ModKey 'Settings'
    Set-ItemProperty -LiteralPath $settingsKey -Name width -Value $Width
    Set-ItemProperty -LiteralPath $settingsKey -Name height -Value $Height
    Set-ItemProperty -LiteralPath $settingsKey -Name screenRatio -Value $Ratio
    $previous = [long](Get-ItemProperty -LiteralPath $ModKey).SettingsChangeTime
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    Set-ItemProperty -LiteralPath $ModKey -Name SettingsChangeTime -Value ([int][Math]::Max($now, $previous + 1))
}
