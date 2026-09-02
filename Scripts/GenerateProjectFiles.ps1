[CmdletBinding()]
param(
    [ValidateSet('2022', '2026')]
    [string]$VisualStudioVersion = '2022',
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'generate-project-files'
$logPath = Join-Path $artifactDirectory 'GenerateProjectFiles.log'

$generationTool = $context.GenerateProjectFilesScript
$visualStudioSwitch = "-$VisualStudioVersion"
$arguments = @("-project=$($context.ProjectFile)", '-game', '-engine', $visualStudioSwitch)

if (-not (Test-Path -LiteralPath $generationTool -PathType Leaf)) {
    $generationTool = $context.UnrealBuildTool
    $arguments = @('-projectfiles', "-project=$($context.ProjectFile)", '-game', '-engine', $visualStudioSwitch)
}

Invoke-HansaNativeCommand `
    -FilePath $generationTool `
    -Arguments $arguments `
    -LogPath $logPath `
    -FailureMessage 'Unreal project-file generation failed.' | Out-Null

$solutionPath = Join-Path $context.ProjectRoot 'Hansa.sln'
$gameProjectPath = Join-Path $context.ProjectRoot 'Intermediate\ProjectFiles\Hansa.vcxproj'
$commonPropsPath = Join-Path $context.ProjectRoot 'Intermediate\ProjectFiles\UECommon.props'
$expectedMetadata = if ($VisualStudioVersion -eq '2022') {
    [ordered]@{
        SolutionVersion = '# Visual Studio Version 17'
        ToolsVersion = 'ToolsVersion="17.0"'
        PlatformToolset = '<PlatformToolset>v143</PlatformToolset>'
    }
}
else {
    [ordered]@{
        SolutionVersion = '# Visual Studio Version 18'
        ToolsVersion = 'ToolsVersion="18.0"'
        PlatformToolset = '<PlatformToolset>v145</PlatformToolset>'
    }
}

foreach ($requiredGeneratedFile in @($solutionPath, $gameProjectPath, $commonPropsPath)) {
    if (-not (Test-Path -LiteralPath $requiredGeneratedFile -PathType Leaf)) {
        throw "Project generation completed but the expected file is missing: $requiredGeneratedFile"
    }
}

$solutionText = Get-Content -Raw -LiteralPath $solutionPath
$gameProjectText = Get-Content -Raw -LiteralPath $gameProjectPath
$commonPropsText = Get-Content -Raw -LiteralPath $commonPropsPath
if (-not $solutionText.Contains($expectedMetadata.SolutionVersion) -or
    -not $gameProjectText.Contains($expectedMetadata.ToolsVersion) -or
    -not $commonPropsText.Contains($expectedMetadata.PlatformToolset)) {
    throw @"
Unreal generated project files that do not match Visual Studio $VisualStudioVersion.
Expected solution marker: $($expectedMetadata.SolutionVersion)
Expected project marker: $($expectedMetadata.ToolsVersion)
Expected toolset marker: $($expectedMetadata.PlatformToolset)
Regenerate with -VisualStudioVersion $VisualStudioVersion and review $logPath.
"@
}

Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
    Operation = 'GenerateProjectFiles'
    Status = 'Succeeded'
    ProjectFile = $context.ProjectFile
    EngineRoot = $context.EngineRoot
    GenerationTool = $generationTool
    VisualStudioVersion = $VisualStudioVersion
    Solution = $solutionPath
    GameProject = $gameProjectPath
    PlatformToolset = if ($VisualStudioVersion -eq '2022') { 'v143' } else { 'v145' }
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Log = $logPath
})

Write-Output "Project files generated successfully for Visual Studio $VisualStudioVersion. Artifacts: $artifactDirectory"
