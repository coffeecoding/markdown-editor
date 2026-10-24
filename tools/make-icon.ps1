# Optional artwork tool. Normal builds use the checked-in ICO; no extra tools
# or packages are needed. The master SVG uses only filled M/L/C/Z paths.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
function Convert-IconPath([string]$data) {
    $tokens = [regex]::Matches($data, '[A-Za-z]|-?\d+(?:\.\d+)?')
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $x = $y = [single]0
    for ($i = 0; $i -lt $tokens.Count;) {
        $command = $tokens[$i++].Value
        if ($command -eq 'Z') { $path.CloseFigure(); continue }
        if ($command -notin @('M', 'L', 'C')) { throw "Unsupported SVG path command: $command" }
        $count = if ($command -eq 'C') { 6 } else { 2 }
        $n = [single[]]::new($count)
        for ($j = 0; $j -lt $count; ++$j) {
            $n[$j] = [single]::Parse($tokens[$i++].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        switch ($command) {
            'M' { $path.StartFigure() }
            'L' { $path.AddLine($x, $y, $n[0], $n[1]) }
            'C' { $path.AddBezier($x, $y, $n[0], $n[1], $n[2], $n[3], $n[4], $n[5]) }
        }
        $x = $n[$count - 2]; $y = $n[$count - 1]
    }
    return $path
}

$projectDirectory = Split-Path -Parent $PSScriptRoot
$iconPath = Join-Path $projectDirectory 'src/app.ico'
[xml]$svg = Get-Content -LiteralPath (Join-Path $projectDirectory 'src/app-icon.svg') -Raw -Encoding UTF8
$shapes = @($svg.svg.path | ForEach-Object {
    @{ Path = (Convert-IconPath $_.d); Brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($_.fill)) }
})
$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
$frames = [Collections.Generic.List[byte[]]]::new()
try {
    foreach ($size in $sizes) {
        # Supersample every frame, including the small title-bar sizes.
        $large = [Drawing.Bitmap]::new($size * 4, $size * 4)
        $graphics = [Drawing.Graphics]::FromImage($large)
        $bitmap = [Drawing.Bitmap]::new($size, $size)
        $output = [Drawing.Graphics]::FromImage($bitmap)
        $stream = [IO.MemoryStream]::new()
        try {
            $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $graphics.ScaleTransform($large.Width / 256.0, $large.Height / 256.0)
            foreach ($shape in $shapes) { $graphics.FillPath($shape.Brush, $shape.Path) }
            $output.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $output.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $output.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $output.DrawImage($large, [Drawing.Rectangle]::new(0, 0, $size, $size), 0, 0, $large.Width, $large.Height, [Drawing.GraphicsUnit]::Pixel)
            $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
            $frames.Add($stream.ToArray())
        } finally {
            $stream.Dispose(); $output.Dispose(); $bitmap.Dispose()
            $graphics.Dispose(); $large.Dispose()
        }
    }
    $writer = [IO.BinaryWriter]::new([IO.File]::Create($iconPath))
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        for ($i = 0; $i -lt $sizes.Count; ++$i) {
            $dimension = [byte]($sizes[$i] % 256)
            $writer.Write($dimension); $writer.Write($dimension)
            $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$i].Length); $writer.Write([uint32]$offset)
            $offset += $frames[$i].Length
        }
        foreach ($frame in $frames) { $writer.Write($frame) }
    } finally { $writer.Dispose() }
    Get-Item -LiteralPath $iconPath | Select-Object Name, Length
} finally {
    foreach ($shape in $shapes) { $shape.Path.Dispose(); $shape.Brush.Dispose() }
}
