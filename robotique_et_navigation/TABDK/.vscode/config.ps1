$cfg = @(Get-Content (Join-Path $PSScriptRoot 'robot.config.json') -Raw | ConvertFrom-Json)[0]
$robot = "$($cfg.username)@$($cfg.host)"
$remoteDir = "$($cfg.catkinSrc)/$($cfg.package)"
$env:DISPLAY = $cfg.display
