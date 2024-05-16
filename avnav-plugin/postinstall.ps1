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
$download = New-Object System.Net.WebClient
$destination="$targetBase\$pythonDir\Lib\site-packages"
$dlls=@("msvcp140.dll","vcruntime140_1.dll")
$dlbase="https://www.wellenvogel.net/software/avnav/downloads/supplement"
foreach ($dll in $dlls) {
    $downloadName="$destination\$dll"
    $url="$dlbase/$dll"
    Write-Host "downloading $downloadName from $url"
    $download.DownloadFile($url,$downloadName)
    if (-Not ($null=Test-Path "$downloadName")){
        Write-Host "WARNING: download for $downloadName failed"
    }
}
exit $res