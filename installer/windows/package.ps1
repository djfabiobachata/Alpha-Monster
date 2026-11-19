# Builds dist\AlphaMonster-<version>-Windows-Setup.exe (Inno Setup) and a .zip
param([string]$Version = "1.0.0")
$ErrorActionPreference = "Stop"

$out   = "build\Source\MasterPlugin\AlphaMonsterMaster_artefacts\Release"
$mix   = "build\Source\MixApp\AlphaMonsterMix_artefacts\Release"
$stage = Join-Path (Get-Location) "dist\stage"
New-Item -ItemType Directory -Force -Path "$stage\Apps\Models", "$stage\VST3" | Out-Null

Copy-Item -Recurse -Force "$out\VST3\Alpha Monster Master.vst3" "$stage\VST3\"
Copy-Item -Force "$out\Standalone\Alpha Monster Master.exe" "$stage\Apps\"
Copy-Item -Force "$mix\Alpha Monster Mix.exe" "$stage\Apps\"
Copy-Item -Force "$mix\Models\*" "$stage\Apps\Models\"
Copy-Item -Force "docs\INSTALL.md" "$stage\README.md"
Copy-Item -Force "docs\INSTALLAZIONE.md" "$stage\LEGGIMI.md"

Compress-Archive -Force -Path "$stage\*" -DestinationPath "dist\AlphaMonster-$Version-Windows.zip"

$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) {
    choco install innosetup -y --no-progress | Out-Null
}
& $iscc "/DVersion=$Version" "/DStage=$stage" "/DOutDir=$((Resolve-Path dist).Path)" "installer\windows\AlphaMonster.iss"
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }
Remove-Item -Recurse -Force $stage
