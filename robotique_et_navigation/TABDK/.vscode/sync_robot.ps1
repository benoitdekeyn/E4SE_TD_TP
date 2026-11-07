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

    # Miroir complet : le dossier distant est vidé puis remplacé par le contenu local.
    cmd /c "tar -C `"$src`" --exclude=.git --exclude=.vscode -cf - . | ssh -o BatchMode=yes $robot `"mkdir -p $remoteDir && find $remoteDir -mindepth 1 -delete && tar -xf - -C $remoteDir`""
    if ($LASTEXITCODE -ne 0) {
        throw "Sync ECHEC (code $LASTEXITCODE)"
    }
    Write-Host "Sync OK -> $robot`:$remoteDir"
} finally {
    if ($locked) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
