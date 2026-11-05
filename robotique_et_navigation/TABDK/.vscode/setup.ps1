$robot = 'ubuntu@192.168.222.135'
$sshOpts = @('-o', 'StrictHostKeyChecking=accept-new')

# VcXsrv (serveur X pour rviz)
if (-not (Test-Path 'C:\Program Files\VcXsrv\vcxsrv.exe')) {
    Write-Host 'Installation de VcXsrv...'
    winget install --id marha.VcXsrv -e --accept-source-agreements --accept-package-agreements
}

# Extensions VS Code
if (Get-Command code -ErrorAction SilentlyContinue) {
    $installed = code --list-extensions
    foreach ($ext in 'kelvin.vscode-sshfs', 'emeraldwalk.RunOnSave') {
        if ($installed -notcontains $ext) { code --install-extension $ext | Out-Null }
    }
}

# Cle SSH
$key = Join-Path $HOME '.ssh\id_ed25519'
if (-not (Test-Path $key)) {
    New-Item -ItemType Directory -Force (Split-Path $key) | Out-Null
    ssh-keygen -t ed25519 -N '""' -f $key
}

# Copie de la cle sur le robot (mot de passe demande une seule fois)
ssh @sshOpts -o BatchMode=yes $robot true 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Copie de la cle SSH sur le robot (mot de passe : turtlebot)"
    Get-Content "$key.pub" | ssh @sshOpts $robot 'mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys'
}
