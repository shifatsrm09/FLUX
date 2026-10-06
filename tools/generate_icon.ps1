Add-Type -AssemblyName System.Drawing

function Create-FluxIcon {
    param([string]$OutPath)

    $sizes = @(16, 24, 32, 48, 64, 128, 256)
    $pngBytesList = @()

    foreach ($size in $sizes) {
        $bmp = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)

        $pad = [float]($size * 0.05)
        $rect = [System.Drawing.RectangleF]::new($pad, $pad, [float]($size - 2 * $pad), [float]($size - 2 * $pad))

        # Background rounded dark disc
        $bgPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
        $corner = [float]($size * 0.22)
        $bgPath.AddArc($rect.X, $rect.Y, $corner * 2, $corner * 2, 180, 90)
        $bgPath.AddArc($rect.Right - $corner * 2, $rect.Y, $corner * 2, $corner * 2, 270, 90)
        $bgPath.AddArc($rect.Right - $corner * 2, $rect.Bottom - $corner * 2, $corner * 2, $corner * 2, 0, 90)
        $bgPath.AddArc($rect.X, $rect.Bottom - $corner * 2, $corner * 2, $corner * 2, 90, 90)
        $bgPath.CloseFigure()

        $bgBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 15, 23, 42)) # Deep obsidian navy #0F172A
        $g.FillPath($bgBrush, $bgPath)
        $bgBrush.Dispose()

        # Subtle glowing cyan/blue border
        $borderPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(140, 56, 189, 248), [Math]::Max(1.0, [float]($size * 0.035)))
        $g.DrawPath($borderPen, $bgPath)
        $borderPen.Dispose()
        $bgPath.Dispose()

        # Sleek "F" / Flux glyph in Electric Cyan & Blue
        $cyan = [System.Drawing.Color]::FromArgb(255, 0, 229, 255)      # #00E5FF
        $blue = [System.Drawing.Color]::FromArgb(255, 59, 130, 246)     # #3B82F6
        $gradBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
            [System.Drawing.PointF]::new([float]($size * 0.2), [float]($size * 0.2)),
            [System.Drawing.PointF]::new([float]($size * 0.8), [float]($size * 0.8)),
            $cyan,
            $blue
        )

        # Draw sleek geometric F
        $barW = [float]($size * 0.14)
        $x0 = [float]($size * 0.28)
        $y0 = [float]($size * 0.22)
        $h = [float]($size * 0.56)
        $topW = [float]($size * 0.44)
        $midW = [float]($size * 0.32)

        # Vertical spine
        $spineRect = [System.Drawing.RectangleF]::new($x0, $y0, $barW, $h)
        $g.FillRectangle($gradBrush, $spineRect)

        # Top horizontal bar
        $topRect = [System.Drawing.RectangleF]::new($x0, $y0, $topW, $barW)
        $g.FillRectangle($gradBrush, $topRect)

        # Middle horizontal bar
        $midY = $y0 + [float]($h * 0.38)
        $midRect = [System.Drawing.RectangleF]::new($x0, $midY, $midW, [float]($barW * 0.9))
        $g.FillRectangle($gradBrush, $midRect)

        # Play accent triangle attached to the right of top bar
        $playPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
        $px = $x0 + $topW
        $py = $y0
        [System.Drawing.PointF[]]$pts = @(
            [System.Drawing.PointF]::new($px, $py),
            [System.Drawing.PointF]::new([float]($px + $barW * 0.7), [float]($py + $barW * 0.5)),
            [System.Drawing.PointF]::new($px, [float]($py + $barW))
        )
        $playPath.AddPolygon($pts)
        $cyanBrush = [System.Drawing.SolidBrush]::new($cyan)
        $g.FillPath($cyanBrush, $playPath)
        $cyanBrush.Dispose()
        $playPath.Dispose()

        $gradBrush.Dispose()
        $g.Dispose()

        $ms = [System.IO.MemoryStream]::new()
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $pngBytesList += ,$ms.ToArray()
        $ms.Dispose()
    }

    # Write multi-image ICO format (PNG payloads)
    $fs = [System.IO.File]::Create($OutPath)
    $bw = [System.IO.BinaryWriter]::new($fs)

    # ICONHEADER: 6 bytes
    $bw.Write([uint16]0) # Reserved
    $bw.Write([uint16]1) # Type 1 = ICO
    $bw.Write([uint16]$sizes.Count) # Image count

    # Directory entries (16 bytes each)
    $dataOffset = 6 + (16 * $sizes.Count)
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $s = $sizes[$i]
        $w = if ($s -ge 256) { [byte]0 } else { [byte]$s }
        $h = if ($s -ge 256) { [byte]0 } else { [byte]$s }
        $bw.Write([byte]$w)
        $bw.Write([byte]$h)
        $bw.Write([byte]0)   # Color count (0 for >=8bpp)
        $bw.Write([byte]0)   # Reserved
        $bw.Write([uint16]1) # Color planes
        $bw.Write([uint16]32) # Bits per pixel
        $bw.Write([uint32]$pngBytesList[$i].Length) # Size of image data
        $bw.Write([uint32]$dataOffset) # Offset of image data
        $dataOffset += $pngBytesList[$i].Length
    }

    # Image data
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $bw.Write($pngBytesList[$i])
    }

    $bw.Close()
    $fs.Close()
    Write-Host "Created ICO at: $OutPath with $($sizes.Count) resolutions."
}

Create-FluxIcon -OutPath "resources/flux.ico"
