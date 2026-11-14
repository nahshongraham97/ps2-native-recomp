# Capture the client area of a running ps2run window to a PNG.
#
# The native OpenGL path renders into a visible SDL window, so a screenshot of
# that window is direct evidence of what the display contains. PrintWindow is
# not used: an OpenGL surface usually reads back black through it. The window is
# raised first so a log-viewer console cannot cover it.
#
#   powershell -NoProfile -File scripts/capture_ps2run_window.ps1 -Out run.png
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Process = "ps2run"
)

Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Ps2RunWindow {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int command);
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }
}
"@

$game = Get-Process -Name $Process -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $game) { throw "no $Process process with a window" }

$handle = $game.MainWindowHandle
[void][Ps2RunWindow]::ShowWindow($handle, 9)   # SW_RESTORE
[void][Ps2RunWindow]::SetForegroundWindow($handle)
Start-Sleep -Milliseconds 600

$client = New-Object Ps2RunWindow+RECT
[void][Ps2RunWindow]::GetClientRect($handle, [ref]$client)
$origin = New-Object Ps2RunWindow+POINT
[void][Ps2RunWindow]::ClientToScreen($handle, [ref]$origin)
$width = $client.Right - $client.Left
$height = $client.Bottom - $client.Top
if ($width -le 0 -or $height -le 0) { throw "window has no client area" }

$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
$graphics.Dispose()
$bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
Write-Output "$Out ${width}x${height}"
