$robot = 'ubuntu@192.168.222.135'
$dest = '~/catkin_ws/src/TABDK'
$src = Resolve-Path (Join-Path $PSScriptRoot '..')

# Miroir complet : le dossier distant est vidé puis remplacé par le contenu local.
cmd /c "tar -C `"$src`" --exclude=.git --exclude=.vscode -cf - . | ssh -o BatchMode=yes $robot `"mkdir -p $dest && find $dest -mindepth 1 -delete && tar -xf - -C $dest`""
if ($LASTEXITCODE -eq 0) { Write-Host "Sync OK -> $robot`:$dest" } else { Write-Host "Sync ECHEC (code $LASTEXITCODE)" }
