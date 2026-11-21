param(
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\icon_image\app-icon-v2.png'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\resources\aula_control.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sourceFile = (Resolve-Path -LiteralPath $SourcePath).Path
$outputFile = [IO.Path]::GetFullPath($OutputPath)
if ($sourceFile -eq $outputFile) { throw 'La salida debe ser distinta de la imagen original.' }

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$frames = [Collections.Generic.List[byte[]]]::new()
$sourceImage = [Drawing.Image]::FromFile($sourceFile)
try {
    foreach ($size in $sizes) {
        $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $png = [IO.MemoryStream]::new()
        try {
            $graphics.Clear([Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $ratio = [Math]::Min($size / $sourceImage.Width, $size / $sourceImage.Height)
            $scaledWidth = [Math]::Max(1, [int][Math]::Round($sourceImage.Width * $ratio))
            $scaledHeight = [Math]::Max(1, [int][Math]::Round($sourceImage.Height * $ratio))
            $left = [int][Math]::Floor(($size - $scaledWidth) / 2)
            $top = [int][Math]::Floor(($size - $scaledHeight) / 2)
            $rect = [Drawing.Rectangle]::new($left, $top, $scaledWidth, $scaledHeight)
            $graphics.DrawImage($sourceImage, $rect, 0, 0, $sourceImage.Width, $sourceImage.Height, [Drawing.GraphicsUnit]::Pixel)
            $bitmap.Save($png, [Drawing.Imaging.ImageFormat]::Png)
            $frames.Add($png.ToArray())
        } finally {
            $png.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
        }
    }
} finally {
    $sourceImage.Dispose()
}

$iconStream = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($iconStream)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
        $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$i].Length); $writer.Write([uint32]$offset)
        $offset += $frames[$i].Length
    }
    foreach ($frame in $frames) { $writer.Write([byte[]]$frame) }
    $writer.Flush()
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputFile)) | Out-Null
    [IO.File]::WriteAllBytes($outputFile, $iconStream.ToArray())
} finally {
    $writer.Dispose(); $iconStream.Dispose()
}
Write-Output "Icono creado: $outputFile"
Write-Output "Tamanos: $($sizes -join ', ') px. Imagen completa, centrada y sin recorte."
