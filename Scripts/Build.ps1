[CmdletBinding()]
param(
    [ValidateSet('Hansa', 'HansaEditor')]
    [string]$Target = 'HansaEditor',
    [ValidateSet('Win64')]
    [string]$Platform = 'Win64',
    [ValidateSet('DebugGame', 'Development', 'Test', 'Shipping')]
    [string]$Configuration = 'Development',
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

if ($Target -eq 'HansaEditor' -and $Configuration -eq 'Shipping') {
    throw 'HansaEditor is Editor-only and cannot be built as Shipping. Use -Target Hansa -Configuration Shipping.'
}

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "build-$Target-$Platform-$Configuration"
$logPath = Join-Path $artifactDirectory 'Build.log'

$arguments = @(
    $Target
    $Platform
    $Configuration
    "-Project=$($context.ProjectFile)"
    '-WaitMutex'
    '-NoHotReloadFromIDE'
)

Invoke-HansaNativeCommand `
    -FilePath $context.BuildScript `
    -Arguments $arguments `
    -LogPath $logPath `
    -FailureMessage "Unreal build failed for $Target $Platform $Configuration." | Out-Null

Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
    Operation = 'Build'
    Status = 'Succeeded'
    Target = $Target
    Platform = $Platform
    Configuration = $Configuration
    ProjectFile = $context.ProjectFile
    EngineRoot = $context.EngineRoot
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Log = $logPath
})

Write-Output "Build succeeded for $Target $Platform $Configuration. Artifacts: $artifactDirectory"
