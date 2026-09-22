[CmdletBinding()]
param([string]$ProjectFile = (Join-Path $PSScriptRoot '../Hansa.uproject'), [string]$EngineRoot = 'H:/Unreal/UE_5.8', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$ProjectFile = [IO.Path]::GetFullPath($ProjectFile)
$Evidence = Join-Path (Split-Path $ProjectFile) 'Saved/MarketRangeVerification'
New-Item -ItemType Directory -Force $Evidence | Out-Null
if (-not $SkipBuild) {
 & "$EngineRoot/Engine/Build/BatchFiles/Build.bat" HansaEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE -DisableUnity
 if ($LASTEXITCODE -ne 0) { throw 'Market range build failed.' }
}
$Editor = "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
$PriorSdk = $env:UE_SKIP_UBT_SDK_SETUP
try {
 $env:UE_SKIP_UBT_SDK_SETUP = '1'
 $Filter = 'Hansa.Simulation.Logistics+Hansa.Simulation.Population+Hansa.World.Projection.MarketRangeMarker+Hansa.Editor.Definitions.MarketRangeAuthoring+Hansa.Integration.RoadConnectionIndicator+Hansa.Integration.RuntimeSimulationHost'
 & $Editor $ProjectFile -unattended -nop4 -nosplash -NoSound -NullRHI "-ExecCmds=Automation RunTests $Filter;Quit" '-TestExit=Automation Test Queue Empty' "-AbsLog=$Evidence/Tests.log"
 if ($LASTEXITCODE -ne 0) { throw 'Market range tests failed.' }
 foreach ($Size in @(@(1280,720),@(1920,1080))) {
  $Log = "$Evidence/Viewport-$($Size[0])x$($Size[1]).log"
  & $Editor $ProjectFile '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP' -game -unattended -nop4 -nosplash -NoSound -windowed -ForceRes "-ResX=$($Size[0])" "-ResY=$($Size[1])" -RenderOffscreen '-ExecCmds=Automation RunTests Hansa.UI.MarketRange.RealViewport' '-TestExit=Automation Test Queue Empty' "-AbsLog=$Log"
  if ($LASTEXITCODE -ne 0 -or (Get-Content -Raw $Log) -notmatch 'Result=\{Success\} Name=\{RealViewport\}') { throw "Viewport test failed: $Log" }
 }
} finally { $env:UE_SKIP_UBT_SDK_SETUP = $PriorSdk }
