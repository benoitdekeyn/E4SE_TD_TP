. (Join-Path $PSScriptRoot 'config.ps1')
$src = Resolve-Path (Join-Path $PSScriptRoot '..')

$mutex = New-Object System.Threading.Mutex($false, 'Local\TABDKRobotSync')
$locked = $false
try {
    try {
        $locked = $mutex.WaitOne([TimeSpan]::FromMinutes(5))
    } catch [System.Threading.AbandonedMutexException] {
        $locked = $true
    }
    if (-not $locked) {
        throw "Synchronisation impossible : une autre synchronisation est toujours en cours."
    }

    # Miroir complet de chaque package : le dossier distant est vidé puis remplacé par le contenu local.
    foreach ($pkg in @($cfg.package, 'AStar-ROS')) {
        $pkgSrc = Join-Path $src $pkg
        if (-not (Test-Path $pkgSrc)) { continue }
        $pkgDir = "$($cfg.catkinSrc)/$pkg"
        cmd /c "tar -C `"$pkgSrc`" --exclude=.git -cf - . | ssh -o BatchMode=yes $robot `"mkdir -p $pkgDir && find $pkgDir -mindepth 1 -delete && tar -xf - -C $pkgDir`""
        if ($LASTEXITCODE -ne 0) {
            throw "Sync ECHEC de $pkg (code $LASTEXITCODE)"
        }
        Write-Host "Sync OK -> $robot`:$pkgDir"
    }
} finally {
    if ($locked) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
