param([string]$ArenaPath = 'C:\Program Files\Resolume Arena\Arena.exe')
$ErrorActionPreference = 'Stop'
if (Get-Process Arena -ErrorAction SilentlyContinue) {
    throw 'Arena is running. Save your work and close Arena before using this launcher.'
}
if (-not (Test-Path -LiteralPath $ArenaPath -PathType Leaf)) {
    throw "Arena executable not found: $ArenaPath"
}
$logDirectory = Join-Path $PSScriptRoot '../logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$logFile = Join-Path (Resolve-Path -LiteralPath $logDirectory).Path ('control-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.jsonl')
$previousLog = $env:RLC_WS_LOG
try {
    $env:RLC_WS_LOG = $logFile
    Start-Process -FilePath $ArenaPath -WindowStyle Hidden
    Write-Output "Plugin log destination: $logFile"
} finally {
    $env:RLC_WS_LOG = $previousLog
}
