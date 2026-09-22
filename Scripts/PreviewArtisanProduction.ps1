[CmdletBinding()]
param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
if(-not $SkipBuild){
 & (Join-Path $PSScriptRoot 'Build.ps1') -Target HansaEditor -Configuration DebugGame
 if($LASTEXITCODE -and $LASTEXITCODE -ne 0){throw 'Preview build failed'}
}
$editor=Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe'
$previous=$env:UE_SKIP_UBT_SDK_SETUP
try{
 $env:UE_SKIP_UBT_SDK_SETUP='1'
 & $editor $context.ProjectFile '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP' -game -ArtisanProductionCandidate -forcerhibypass -norhithread -windowed -ResX=1920 -ResY=1080 '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False'
}finally{$env:UE_SKIP_UBT_SDK_SETUP=$previous}
