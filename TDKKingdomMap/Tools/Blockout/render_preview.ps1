$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$base=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../Design/Blockout'))
$d=Get-Content (Join-Path $base 'Generated/layout_compiled.json') -Raw | ConvertFrom-Json
$b=Get-Content (Join-Path $base 'Generated/buildings.json') -Raw | ConvertFrom-Json
$bitmap=[Drawing.Bitmap]::new(1500,1500)
$graphics=[Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([Drawing.ColorTranslator]::FromHtml('#24343a'))
$font=[Drawing.Font]::new('Arial',10)
$titleFont=[Drawing.Font]::new('Arial',19)
$colors=@{Medieval='#589bc9';Hybrid='#65a278';Gothic='#b45150'}
function Point($p) { [Drawing.PointF]::new([single](750+$p[0]*1.3),[single](750-$p[1]*1.3)) }
function Polygon($points,$color,$outline='#d9ddc4') {
    [Drawing.PointF[]]$pts=@($points | ForEach-Object { Point $_ })
    $brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color))
    $pen=[Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml($outline),1)
    $graphics.FillPolygon($brush,$pts); $graphics.DrawPolygon($pen,$pts)
    $brush.Dispose(); $pen.Dispose()
}
function Label($p,$text) {
    $q=Point $p
    $graphics.DrawString($text,$font,[Drawing.Brushes]::Black,$q.X+1,$q.Y+1)
    $graphics.DrawString($text,$font,[Drawing.Brushes]::White,$q.X,$q.Y)
}
foreach ($r in $d.land) { Polygon $r.polygon_m '#75836b' }
foreach ($r in $d.architecture_regions) { Polygon $r.polygon_m $colors[$r.architecture_style] }
foreach ($r in $d.elevation_zones) {
    [Drawing.PointF[]]$pts=@($r.polygon_m | ForEach-Object { Point $_ })
    $pen=[Drawing.Pen]::new([Drawing.Color]::Khaki,2); $graphics.DrawPolygon($pen,$pts); $pen.Dispose()
}
foreach ($r in @($d.roads)+@($d.stairs)) {
    [Drawing.PointF[]]$pts=@($r.points_m | ForEach-Object { Point $_ })
    $pen=[Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#eed7a5'),[single]($r.width_m*1.3))
    $pen.LineJoin=[Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawLines($pen,$pts); $pen.Dispose()
}
foreach ($p in $d.plazas) {
    $q=Point $p.center_m; $rad=$p.radius_m*1.3
    $brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#ddc9a0'))
    $graphics.FillEllipse($brush,[single]($q.X-$rad),[single]($q.Y-$rad),[single]($rad*2),[single]($rad*2)); $brush.Dispose()
    Label $p.center_m "$($p.id) +$($p.elevation_m)m"
}
foreach ($house in $b) {
    $q=Point $house.center_m; $w=$house.size_m[0]*1.3; $h=$house.size_m[1]*1.3
    $brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($colors[$house.style]))
    $graphics.FillRectangle($brush,[single]($q.X-$w/2),[single]($q.Y-$h/2),[single]$w,[single]$h)
    $graphics.DrawRectangle([Drawing.Pens]::DarkSlateGray,[single]($q.X-$w/2),[single]($q.Y-$h/2),[single]$w,[single]$h); $brush.Dispose()
    if ($house.landmark) { Label $house.center_m $house.id }
}
[Drawing.PointF[]]$wall=@($d.walls.points_m | ForEach-Object { Point $_ })
$pen=[Drawing.Pen]::new([Drawing.Color]::LightGray,5); $graphics.DrawPolygon($pen,$wall); $pen.Dispose()
foreach ($p in $d.walls.openings) {
    $q=Point $p.center_m; $rad=$p.width_m*1.3/2
    $graphics.FillEllipse([Drawing.Brushes]::Gold,[single]($q.X-$rad),[single]($q.Y-$rad),[single]($rad*2),[single]($rad*2))
    Label @($p.center_m[0],($p.center_m[1]+16)) $p.id
}
foreach ($r in $d.elevation_zones) {
    $xx=@($r.polygon_m | ForEach-Object { $_[0] }); $yy=@($r.polygon_m | ForEach-Object { $_[1] })
    Label @(($xx | Measure-Object -Average).Average,($yy | Measure-Object -Average).Average) "$($r.id) +$($r.elevation_m)m"
}
foreach ($r in $d.roads) { Label $r.points_m[[int]($r.points_m.Count/2)] $r.id }
$graphics.DrawString('THANH A / interpreted blockout / estimated',$titleFont,[Drawing.Brushes]::White,30,25)
$graphics.DrawString('RED = GOTHIC   |   BLUE = MEDIEVAL (land)   |   GREEN = HYBRID',$font,[Drawing.Brushes]::White,30,65)
$graphics.DrawString('+X East / +Y North / Plaza XY origin / water Z0 assumed / yellow contours = elevation polygons',$font,[Drawing.Brushes]::White,30,90)
$graphics.DrawLine([Drawing.Pens]::White,40,1440,300,1440)
$graphics.DrawString('200 m (reference ruler estimate)',$font,[Drawing.Brushes]::White,40,1450)
$bitmap.Save((Join-Path $base 'Generated/layout_preview.png'),[Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose(); $bitmap.Dispose(); $font.Dispose(); $titleFont.Dispose()
$source=[Drawing.Bitmap]::new((Join-Path $base $d.reference.primary))
$crop=$d.reference.crop_px
$rect=[Drawing.Rectangle]::new($crop[0],$crop[1],($crop[2]-$crop[0]),($crop[3]-$crop[1]))
$cropped=$source.Clone($rect,$source.PixelFormat)
$cropped.Save((Join-Path $base 'Generated/reference_01_crop.png'),[Drawing.Imaging.ImageFormat]::Png)
$cropped.Dispose(); $source.Dispose()
Write-Output 'Saved layout_preview.png and reference_01_crop.png'
