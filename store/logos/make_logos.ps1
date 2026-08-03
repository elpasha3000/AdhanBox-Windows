# Store logos: 9:16 posters + 1:1 box art. System.Drawing shapes Arabic correctly (PIL does not).
Add-Type -AssemblyName System.Drawing
$root = 'D:\WORK\AdhanBox-Windows'
$out  = "$root\store\logos"
$icon = [System.Drawing.Image]::FromFile("$root\assets\adhanbox-512.png")

function New-Logo {
    param($W, $H, $Out, $Square)

    $bmp = New-Object System.Drawing.Bitmap($W, $H)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.TextRenderingHint = 'AntiAliasGridFit'

    # gradient background
    $rect = New-Object System.Drawing.Rectangle(0, 0, $W, $H)
    $c1 = [System.Drawing.Color]::FromArgb(255, 10, 16, 28)
    $c2 = [System.Drawing.Color]::FromArgb(255, 13, 44, 50)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $c1, $c2, 90.0)
    $g.FillRectangle($bg, $rect); $bg.Dispose()

    # soft glow behind icon
    $icY = if ($Square) { [int]($H*0.42) } else { [int]($H*0.34) }
    $gp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $gr = [int]($W*0.45)
    $gp.AddEllipse([int]($W/2-$gr), [int]($icY-$gr), $gr*2, $gr*2)
    $pgb = New-Object System.Drawing.Drawing2D.PathGradientBrush($gp)
    $pgb.CenterColor = [System.Drawing.Color]::FromArgb(80, 34, 175, 110)
    $pgb.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 10, 16, 28))
    $g.FillPath($pgb, $gp); $pgb.Dispose(); $gp.Dispose()

    # icon
    $icS = if ($Square) { [int]($W*0.46) } else { [int]($W*0.52) }
    $g.DrawImage($icon, [int](($W-$icS)/2), [int]($icY-$icS/2), $icS, $icS)

    $s = $W / 720.0
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = 'Center'
    $wTitle = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,236,243,252))
    $wSub   = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,150,176,208))
    $wFoot  = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,95,224,160))

    if ($Square) {
        $fT = New-Object System.Drawing.Font('Segoe UI', [float](72*$W/1080.0), [System.Drawing.FontStyle]::Bold)
        $fS = New-Object System.Drawing.Font('Segoe UI', [float](30*$W/1080.0))
        $g.DrawString('AdhanBox', $fT, $wTitle, (New-Object System.Drawing.RectangleF([float](0),[float]($H*0.685),[float]($W),[float]($H*0.12))), $fmt)
        $g.DrawString([char]0x0645+[char]0x0648+[char]0x0627+[char]0x0642+[char]0x064A+[char]0x062A+' '+[char]0x0627+[char]0x0644+[char]0x0635+[char]0x0644+[char]0x0627+[char]0x0629+' '+[char]0x0648+[char]0x0627+[char]0x0644+[char]0x0623+[char]0x0630+[char]0x0627+[char]0x0646, $fS, $wSub, (New-Object System.Drawing.RectangleF([float](0),[float]($H*0.815),[float]($W),[float]($H*0.08))), $fmt)
    } else {
        $fT = New-Object System.Drawing.Font('Segoe UI', [float](56*$s), [System.Drawing.FontStyle]::Bold)
        $fS = New-Object System.Drawing.Font('Segoe UI', [float](24*$s))
        $fF = New-Object System.Drawing.Font('Segoe UI', [float](19*$s))
        $g.DrawString('AdhanBox', $fT, $wTitle, (New-Object System.Drawing.RectangleF([float](0),[float]($H*0.60),[float]($W),[float]($H*0.10))), $fmt)
        $g.DrawString('Prayer times and adhan', $fS, $wSub, (New-Object System.Drawing.RectangleF([float](0),[float]($H*0.695),[float]($W),[float]($H*0.05))), $fmt)
        $g.DrawString([char]0x0645+[char]0x0648+[char]0x0627+[char]0x0642+[char]0x064A+[char]0x062A+' '+[char]0x0627+[char]0x0644+[char]0x0635+[char]0x0644+[char]0x0627+[char]0x0629+' '+[char]0x0648+[char]0x0627+[char]0x0644+[char]0x0623+[char]0x0630+[char]0x0627+[char]0x0646, $fS, $wSub, (New-Object System.Drawing.RectangleF([float](0),[float]($H*0.745),[float]($W),[float]($H*0.05))), $fmt)
        $g.DrawString('Free '+[char]0x00B7+' Offline '+[char]0x00B7+' Open source', $fF, $wFoot, (New-Object System.Drawing.RectangleF([float](0),[float]($H*0.865),[float]($W),[float]($H*0.05))), $fmt)
    }

    $g.Dispose()
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $Out"
}

New-Logo -W 720  -H 1080 -Out "$out\Poster_720x1080.png"   -Square $false
New-Logo -W 1440 -H 2160 -Out "$out\Poster_1440x2160.png"  -Square $false
New-Logo -W 1080 -H 1080 -Out "$out\BoxArt_1080.png"       -Square $true
New-Logo -W 2160 -H 2160 -Out "$out\BoxArt_2160.png"       -Square $true
$icon.Dispose()
