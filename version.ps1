$version = Get-ChildItem -Filter *.dll | Select-Object -ExpandProperty VersionInfo  | Select-Object -ExpandProperty "ProductVersion"
Write-Host "##vso[task.setvariable variable=PROD_VER;]$version"
