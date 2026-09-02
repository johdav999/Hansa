[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'generate-project-files'
$logPath = Join-Path $artifactDirectory 'GenerateProjectFiles.log'

$projectBuildConfigurationPath = Join-Path $context.ProjectRoot 'Saved\UnrealBuildTool\BuildConfiguration.xml'
if (-not (Test-Path -LiteralPath $projectBuildConfigurationPath -PathType Leaf)) {
    throw "The source-controlled project generator configuration is missing: $projectBuildConfigurationPath"
}

try {
    [xml]$projectBuildConfiguration = Get-Content -Raw -LiteralPath $projectBuildConfigurationPath
}
catch {
    throw "The project generator configuration is not valid XML: $projectBuildConfigurationPath`n$($_.Exception.Message)"
}

$configuredFormat = [string]$projectBuildConfiguration.Configuration.ProjectFileGenerator.Format
if ($configuredFormat -ne 'VisualStudio2022') {
    throw "The project generator configuration must pin VisualStudio2022, but contains '$configuredFormat': $projectBuildConfigurationPath"
}

$generationTool = $context.GenerateProjectFilesScript
$arguments = @("-project=$($context.ProjectFile)", '-game', '-engine', '-2022')

if (-not (Test-Path -LiteralPath $generationTool -PathType Leaf)) {
    $generationTool = $context.UnrealBuildTool
    $arguments = @('-projectfiles', "-project=$($context.ProjectFile)", '-game', '-engine', '-2022')
}

Invoke-HansaNativeCommand `
    -FilePath $generationTool `
    -Arguments $arguments `
    -LogPath $logPath `
    -FailureMessage 'Unreal project-file generation failed.' | Out-Null

$solutionPath = Join-Path $context.ProjectRoot 'Hansa.sln'
$gameProjectPath = Join-Path $context.ProjectRoot 'Intermediate\ProjectFiles\Hansa.vcxproj'
$commonPropsPath = Join-Path $context.ProjectRoot 'Intermediate\ProjectFiles\UECommon.props'
$expectedMetadata = [ordered]@{
    SolutionVersion = '# Visual Studio Version 17'
    ToolsVersion = 'ToolsVersion="17.0"'
    PlatformToolset = '<PlatformToolset>v143</PlatformToolset>'
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
Unreal generated project files that do not match the repository's Visual Studio 2022 baseline.
Expected solution marker: $($expectedMetadata.SolutionVersion)
Expected project marker: $($expectedMetadata.ToolsVersion)
Expected toolset marker: $($expectedMetadata.PlatformToolset)
Review $projectBuildConfigurationPath and $logPath.
"@
}

Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
    Operation = 'GenerateProjectFiles'
    Status = 'Succeeded'
    ProjectFile = $context.ProjectFile
    EngineRoot = $context.EngineRoot
    GenerationTool = $generationTool
    VisualStudioVersion = '2022'
    PersistentConfiguration = $projectBuildConfigurationPath
    Solution = $solutionPath
    GameProject = $gameProjectPath
    PlatformToolset = 'v143'
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Log = $logPath
})

Write-Output "Project files generated successfully for Visual Studio 2022. Artifacts: $artifactDirectory"
