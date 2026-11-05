param([ValidateSet('shell', 'run')][string]$Mode = 'shell')
. (Join-Path $PSScriptRoot 'config.ps1')

if ($Mode -eq 'run') {
    ssh -t $robot 'bash -ic ./run_robot.sh; exec bash -l'
} else {
    ssh -Y -o ServerAliveInterval=30 $robot
}
