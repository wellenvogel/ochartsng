$targetBase = $null
$name="mapbox-earcut==1.0.1"
if ($null -eq $env:AVNAVBASE) {
    $targetBase = $env:LOCALAPPDATA + "\avnav"
}
else {
    $targetBase = $env:AVNAVBASE
}
$pythonDir = "python"
Write-Host "pip install $name"
$res = (Start-Process -WorkingDirectory $targetBase -FilePath "$targetBase\$pythonDir\python.exe" -ArgumentList "-m", "pip","install", $name -PassThru -Wait -NoNewWindow)
exit $res