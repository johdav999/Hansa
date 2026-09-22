[CmdletBinding()]
param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$ProjectFile=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Hansa.uproject'))
$Root=Split-Path $ProjectFile
$Evidence=Join-Path $Root 'Saved/AlertPanel'
New-Item -ItemType Directory -Force $Evidence | Out-Null
if(-not $SkipBuild){
 & 'H:/Unreal/UE_5.8/Engine/Build/BatchFiles/Build.bat' HansaEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE -DisableUnity
 if($LASTEXITCODE -ne 0){throw 'Alert panel build failed'}
}
$Editor='H:/Unreal/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$PreviousSdk=$env:UE_SKIP_UBT_SDK_SETUP
try {
 $env:UE_SKIP_UBT_SDK_SETUP='1'
 & $Editor $ProjectFile -unattended -nop4 -nosplash -NoSound -NullRHI '-ExecCmds=Automation RunTests Hansa.UI.HUD+Hansa.UI.Inspector;Quit' '-TestExit=Automation Test Queue Empty' "-AbsLog=$Evidence/Tests.log"
 if($LASTEXITCODE -ne 0){throw 'Alert panel regression tests failed'}
 foreach($Size in @(@(1280,720),@(1920,1080),@(2560,1440),@(3440,1440))){
  $Log="$Evidence/Viewport-$($Size[0])x$($Size[1]).log"
  & $Editor $ProjectFile '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP' -game -unattended -nop4 -nosplash -NoSound -windowed -ForceRes "-ResX=$($Size[0])" "-ResY=$($Size[1])" -RenderOffscreen '-ExecCmds=Automation RunTests Hansa.UI.AlertPanel.RealViewport' '-TestExit=Automation Test Queue Empty' "-AbsLog=$Log"
  if($LASTEXITCODE -ne 0 -or (Get-Content -Raw $Log) -notmatch 'Result=\{Success\} Name=\{RealViewport\}'){throw "Alert viewport failed: $Log"}
 }
} finally {$env:UE_SKIP_UBT_SDK_SETUP=$PreviousSdk}
