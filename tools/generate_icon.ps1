Add-Type -AssemblyName System.Drawing

function Create-FluxIcon {
    param(
        [string]$SourceImage = "resources/v1-icon.png",
        [string]$OutPath = "resources/flux.ico"
    )

    if (-not (Test-Path $SourceImage)) {
        Write-Error "Source image not found: $SourceImage"
        return
    }

    $src = [System.Drawing.Image]::FromFile((Resolve-Path $SourceImage).Path)
    $sizes = @(16, 20, 24, 32, 48, 64, 128, 256)
    $pngBytesList = @()

    foreach ($size in $sizes) {
        $bmp = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.DrawImage($src, 0, 0, $size, $size)
        $g.Dispose()

        $ms = [System.IO.MemoryStream]::new()
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $pngBytesList += ,$ms.ToArray()
        $ms.Dispose()
    }
    $src.Dispose()

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
    Write-Host "Created ICO at: $OutPath with $($sizes.Count) resolutions from $SourceImage."
}

Create-FluxIcon -SourceImage "resources/v1-icon.png" -OutPath "resources/flux.ico"
