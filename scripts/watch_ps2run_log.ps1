[CmdletBinding()]
param(
    [string]$Path,
    [ValidateRange(1, 5000)]
    [int]$Tail = 80,
    [switch]$Once
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Path)) {
    $logDirectory = Join-Path $env:LOCALAPPDATA 'Temp\logs'
    if (-not (Test-Path -LiteralPath $logDirectory -PathType Container)) {
        throw "ps2run log directory not found: $logDirectory"
    }

    $latest = Get-ChildItem -LiteralPath $logDirectory -Filter 'ps2run_*.log' -File |
        Sort-Object -Property LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $latest) {
        throw "No ps2run logs found in: $logDirectory"
    }
    $Path = $latest.FullName
}

if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "ps2run log file not found: $Path"
}

Write-Host "ps2run log: $Path"
$Host.UI.RawUI.WindowTitle = "ps2run live log - $([System.IO.Path]::GetFileName($Path))"
if ($Once) {
    Get-Content -LiteralPath $Path -Tail $Tail
} else {
    Get-Content -LiteralPath $Path -Tail $Tail -Wait
}
