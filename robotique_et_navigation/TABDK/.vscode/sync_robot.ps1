. (Join-Path $PSScriptRoot 'config.ps1')
$src = Resolve-Path (Join-Path $PSScriptRoot '..')

# Miroir complet : le dossier distant est vidé puis remplacé par le contenu local.
cmd /c "tar -C `"$src`" --exclude=.git --exclude=.vscode -cf - . | ssh -o BatchMode=yes $robot `"mkdir -p $remoteDir && find $remoteDir -mindepth 1 -delete && tar -xf - -C $remoteDir`""
if ($LASTEXITCODE -eq 0) { Write-Host "Sync OK -> $robot`:$remoteDir" } else { Write-Host "Sync ECHEC (code $LASTEXITCODE)" }
