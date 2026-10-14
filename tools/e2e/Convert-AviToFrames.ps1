# Extracts BGB uncompressed 24bpp DIB AVI → PNG frame sequence for HTML playback.
param(
    [Parameter(Mandatory = $true)][string]$AviPath,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [int]$FrameStep = 0,   # 0 = auto: spread MaxFrames over the WHOLE video
    [int]$MaxFrames = 300
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Find-Ascii([byte[]]$bytes, [string]$needle, [int]$start, [int]$end) {
    $n = [Text.Encoding]::ASCII.GetBytes($needle)
    for ($i = $start; $i -le ($end - $n.Length); $i++) {
        $ok = $true
        for ($j = 0; $j -lt $n.Length; $j++) {
            if ($bytes[$i + $j] -ne $n[$j]) { $ok = $false; break }
        }
        if ($ok) { return $i }
    }
    return -1
}

if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$bytes = [IO.File]::ReadAllBytes($AviPath)
$strf = Find-Ascii $bytes "strf" 0 ([Math]::Min(2000, $bytes.Length))
if ($strf -lt 0) { throw "No strf in $AviPath" }
$w = [BitConverter]::ToInt32($bytes, $strf + 12)
$h = [BitConverter]::ToInt32($bytes, $strf + 16)
$bit = [BitConverter]::ToInt16($bytes, $strf + 22)
$comp = [BitConverter]::ToInt32($bytes, $strf + 24)
if ($bit -ne 24 -or $comp -ne 0) {
    throw "Only uncompressed 24bpp AVI supported (got bit=$bit comp=$comp)"
}
$absH = [Math]::Abs($h)
$rowStride = [int](([Math]::Floor(($w * 3 + 3) / 4)) * 4)
$frameBytes = $rowStride * $absH
$movi = Find-Ascii $bytes "movi" 0 $bytes.Length
if ($movi -lt 0) { throw "No movi list in $AviPath" }

if ($FrameStep -le 0) {
    $est = [Math]::Max(1, [int][Math]::Floor(($bytes.Length - $movi) / ($frameBytes + 8)))
    $FrameStep = [Math]::Max(1, [int][Math]::Ceiling($est / $MaxFrames))
}

$pos = $movi + 4
$idx = 0
$count = 0
while ($pos + 8 -lt $bytes.Length -and $count -lt $MaxFrames) {
    $id = [Text.Encoding]::ASCII.GetString($bytes, $pos, 4)
    $size = [BitConverter]::ToInt32($bytes, $pos + 4)
    $data = $pos + 8
    if ($size -lt 0 -or ($data + $size) -gt $bytes.Length) { break }

    if (($id -eq "00db" -or $id -eq "00dc") -and $size -ge $frameBytes) {
        if (($idx % $FrameStep) -eq 0) {
            $bmp = New-Object System.Drawing.Bitmap $w, $absH, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
            $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $absH
            $bits = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $bmp.PixelFormat)
            # Clear stride padding then copy BGR rows (DIB is bottom-up).
            $zero = New-Object byte[] ($bits.Stride * $absH)
            [Runtime.InteropServices.Marshal]::Copy($zero, 0, $bits.Scan0, $zero.Length)
            for ($y = 0; $y -lt $absH; $y++) {
                $srcY = $absH - 1 - $y
                $src = $data + ($srcY * $rowStride)
                $destPtr = [IntPtr]::Add($bits.Scan0, $y * $bits.Stride)
                [Runtime.InteropServices.Marshal]::Copy($bytes, $src, $destPtr, ($w * 3))
            }
            $bmp.UnlockBits($bits)
            $name = ("{0:D4}.png" -f $count)
            $bmp.Save((Join-Path $OutDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
            $bmp.Dispose()
            $count++
        }
        $idx++
    }

    $pos = $data + $size
    if (($pos % 2) -ne 0) { $pos++ }
}

if ($count -eq 0) { throw "No frames extracted from $AviPath" }

$meta = @{ count = $count; width = $w; height = $absH; fps = 20 } | ConvertTo-Json -Compress
Set-Content -Path (Join-Path $OutDir "meta.json") -Value $meta -Encoding ascii
Write-Host "Frames $count -> $OutDir"
