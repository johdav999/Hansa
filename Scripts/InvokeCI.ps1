[CmdletBinding()]
param(
    [switch]$GenerateProjectFiles,
    [string]$TestFilter = 'Hansa',
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$childArguments = @{
    EngineRoot = $context.EngineRoot
    ArtifactsRoot = $context.ArtifactsRoot
}

& (Join-Path $PSScriptRoot 'VerifyRepositoryConventions.ps1') -ArtifactsRoot $context.ArtifactsRoot

if ($GenerateProjectFiles) {
    & (Join-Path $PSScriptRoot 'GenerateProjectFiles.ps1') @childArguments
}

& (Join-Path $PSScriptRoot 'Build.ps1') @childArguments -Target HansaEditor -Platform Win64 -Configuration Development
& (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') @childArguments -TestFilter $TestFilter -SkipBuild
& (Join-Path $PSScriptRoot 'Build.ps1') @childArguments -Target Hansa -Platform Win64 -Configuration Development
& (Join-Path $PSScriptRoot 'VerifyShippingExclusion.ps1') @childArguments -Platform Win64

$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'ci-summary'
$summaryPath = Join-Path $artifactDirectory 'result.json'
Write-HansaJsonArtifact -Path $summaryPath -Value ([ordered]@{
    Operation = 'InvokeCI'
    Status = 'Succeeded'
    GeneratedProjectFiles = [bool]$GenerateProjectFiles
    RepositoryConventionsChecked = $true
    TestFilter = $TestFilter
    ProjectFile = $context.ProjectFile
    EngineRoot = $context.EngineRoot
    ArtifactsRoot = $context.ArtifactsRoot
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
})

Write-Output "Hansa CI-equivalent verification succeeded. Summary: $summaryPath"
