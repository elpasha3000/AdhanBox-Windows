# Builds 1920x1080 Microsoft Store screenshots from captured window PNGs.
# Captions live in captions.txt (UTF-8, no BOM) -> read with -Encoding UTF8 so Arabic survives.
param([string]$Root = 'D:\WORK\AdhanBox-Windows')

Add-Type -AssemblyName System.Drawing

$W = 1920; $H = 1080
$outDir = Join-Path $Root 'store'
$shots = @{
    'main'  = @{ File = 'p_main.png';  FileEn = 'p_main_en.png';  Scale = 1.55 }
    'set'   = @{ File = 'p_set.png';   FileEn = 'p_set_en.png';   Scale = 1.15 }
    'about' = @{ File = 'p_about.png'; FileEn = 'p_about_en.png'; Scale = 1.60 }
}

function New-Shot {
    param($WinPng, $Head, $Sub, $Rtl, $Scale, $Out)

    $bmp = New-Object System.Drawing.Bitmap($W, $H)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.TextRenderingHint = 'ClearTypeGridFit'

    # background gradient (brand navy -> deep teal)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $W, $H)
    $c1 = [System.Drawing.Color]::FromArgb(255, 10, 16, 28)
    $c2 = [System.Drawing.Color]::FromArgb(255, 12, 40, 48)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $c1, $c2, 60.0)
    $g.FillRectangle($bg, $rect)
    $bg.Dispose()

    # soft brand glow behind the window
    $glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $gx = if ($Rtl) { 60 } else { 900 }
    $glowPath.AddEllipse($gx, 120, 940, 840)
    $gb = New-Object System.Drawing.Drawing2D.PathGradientBrush($glowPath)
    $gb.CenterColor = [System.Drawing.Color]::FromArgb(70, 34, 175, 110)
    $gb.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 26, 86, 219))
    $g.FillPath($gb, $glowPath)
    $gb.Dispose(); $glowPath.Dispose()

    # window image
    $raw = [System.Drawing.Image]::FromFile($WinPng)
    # trim the invisible resize border Windows includes in the captured rect
    $pad = 8
    $crop = New-Object System.Drawing.Rectangle($pad, 0, ($raw.Width - 2 * $pad), ($raw.Height - $pad))
    $img = (New-Object System.Drawing.Bitmap($raw)).Clone($crop, $raw.PixelFormat)
    $raw.Dispose()
    $iw = [int]($img.Width * $Scale); $ih = [int]($img.Height * $Scale)
    $ix = if ($Rtl) { 150 } else { $W - 150 - $iw }
    $iy = [int](($H - $ih) / 2)

    # fake drop shadow: stacked translucent rounded rects
    for ($k = 26; $k -ge 2; $k -= 4) {
        $a = [int](26 - $k)
        if ($a -lt 4) { $a = 4 }
        $sh = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb($a, 0, 0, 0))
        $g.FillRectangle($sh, ($ix - $k), ($iy - $k + 10), ($iw + 2 * $k), ($ih + 2 * $k))
        $sh.Dispose()
    }
    $g.DrawImage($img, $ix, $iy, $iw, $ih)
    $img.Dispose()

    # text column
    $tx = if ($Rtl) { 150 + $iw + 70 } else { 150 }
    $tw = if ($Rtl) { $W - $tx - 150 } else { $ix - 70 - $tx }
    $fmt = New-Object System.Drawing.StringFormat
    if ($Rtl) { $fmt.FormatFlags = [System.Drawing.StringFormatFlags]::DirectionRightToLeft }

    $icoPath = Join-Path $Root 'assets\adhanbox-512.png'
    if (Test-Path $icoPath) {
        $ico = [System.Drawing.Image]::FromFile($icoPath)
        $ax = if ($Rtl) { $tx + $tw - 92 } else { $tx }
        $g.DrawImage($ico, $ax, 268, 92, 92)
        $ico.Dispose()
    }

    $fHead = New-Object System.Drawing.Font('Segoe UI', 40, [System.Drawing.FontStyle]::Bold)
    $fSub  = New-Object System.Drawing.Font('Segoe UI', 21, [System.Drawing.FontStyle]::Regular)
    $fFoot = New-Object System.Drawing.Font('Segoe UI', 18, [System.Drawing.FontStyle]::Bold)
    $bHead = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 236, 243, 252))
    $bSub  = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 150, 176, 208))
    $bFoot = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 95, 224, 160))

    $rHead = New-Object System.Drawing.RectangleF($tx, 392, $tw, 200)
    $g.DrawString($Head, $fHead, $bHead, $rHead, $fmt)
    $rSub = New-Object System.Drawing.RectangleF($tx, 560, $tw, 240)
    $g.DrawString($Sub, $fSub, $bSub, $rSub, $fmt)
    $rFoot = New-Object System.Drawing.RectangleF($tx, 760, $tw, 60)
    $g.DrawString('magicweb.win', $fFoot, $bFoot, $rFoot, $fmt)

    $g.Dispose()
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $Out"
}

$lines = Get-Content (Join-Path $outDir 'captions.txt') -Encoding UTF8
foreach ($ln in $lines) {
    if (-not $ln.Trim()) { continue }
    $p = $ln.Split('|')
    $lang = $p[0]; $key = $p[1]; $head = $p[2]; $sub = $p[3]
    $meta = $shots[$key]
    $src = if ($lang -eq 'ar') { $meta.File } else { $meta.FileEn }
    $srcPath = Join-Path $Root $src
    if (-not (Test-Path $srcPath)) { Write-Host "MISSING $srcPath"; continue }
    $out = Join-Path $outDir ("store-{0}-{1}.png" -f $lang, $key)
    New-Shot -WinPng $srcPath -Head $head -Sub $sub -Rtl ($lang -eq 'ar') -Scale $meta.Scale -Out $out
}
