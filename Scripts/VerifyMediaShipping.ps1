[CmdletBinding()]
param(
    [string]$CookedRoot,
    [switch]$RequireCookedContent,
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'media-shipping-audit'
$referenceReport = Join-Path $artifactDirectory 'references.json'
# The editor-only commandlet needs HansaEditor's ToolsetRegistry/Water DLL dependencies.
# Shipping exclusion is verified from receipts/cooked content, not by disabling editor startup dependencies.
$priorSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile, '-run=HansaMediaAudit', '-unattended', '-nop4', '-NullRHI',
        '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
        '-ini:Game:[/Script/Engine.AssetManagerSettings]:+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=,SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=NeverCook))',
        '-DisablePlugins=GameFeatures',
        "-Report=$referenceReport"
    ) -LogPath (Join-Path $artifactDirectory 'ReferenceAudit.log') -FailureMessage 'Production media reference audit failed' | Out-Null
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $priorSdk, 'Process')
}
if (-not (Test-Path -LiteralPath $referenceReport)) { throw 'Reference audit did not produce evidence.' }
$referenceResult = Get-Content -LiteralPath $referenceReport -Raw | ConvertFrom-Json
if (-not $referenceResult.passed) { throw 'Production references contain staging or developer content.' }
$failures = [System.Collections.Generic.List[string]]::new()
$scanned = [System.Collections.Generic.List[object]]::new()
$cookMetadata = [System.Collections.Generic.List[object]]::new()
$forbidden = @(
    'HansaEditor', 'HansaGenerationWorker', 'HansaAutomation', 'HansaTests',
    '/Game/Hansa/Generated/Staging', '/Game/Hansa/Developer', '/Game/Developers/',
    'Content/Hansa/Generated/Staging', 'Content/Hansa/Developer',
    '__ExternalActors__/Hansa/Generated/Staging', '__ExternalObjects__/Hansa/Generated/Staging',
    '__ExternalActors__/Hansa/Developer/', '__ExternalObjects__/Hansa/Developer/',
    '__ExternalActors__/Developers/', '__ExternalObjects__/Developers/',
    'SourceArt/Generated', 'Saved/GenerationJobs', 'GenerationPreview',
    'OPENAI_API_KEY', 'TRIPO_API_KEY', 'ELEVENLABS_API_KEY',
    'HANSA_GENERATION_WORKER_TOKEN', 'TripoStaticPropProvider', 'HANSA_TRIPO_ENABLED', 'openapi.tripo3d.ai', 'ElevenLabsAudioProvider', 'HANSA_ELEVENLABS_ENABLED', 'api.elevenlabs.io'
)
$cookedAudited = $false
if (-not [string]::IsNullOrWhiteSpace($CookedRoot)) {
    $resolvedCooked = (Resolve-Path -LiteralPath $CookedRoot).Path
    $files = @(Get-ChildItem -LiteralPath $resolvedCooked -File -Recurse)
    $packages = @($files | Where-Object { $_.Extension -in @('.uasset', '.umap') })
    if ($packages.Count -eq 0) { throw 'An expanded cooked tree containing actual .uasset/.umap packages is required; an empty directory or opaque container is not proof.' }
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($resolvedCooked, $file.FullName).Replace('\', '/')
        # UE CopyBuildToStagingDirectory excludes project Metadata from staged files.
        # Retain exact cooker-only descriptors as evidence, not runtime packages.
        # Never exempt a .uasset/.umap, nor similarly named files elsewhere.
        if ($relative -in @('Hansa/Metadata/DevelopmentAssetRegistry.bin', 'Hansa/Metadata/scriptobjects.bin')) {
            $cookMetadata.Add([ordered]@{ Path = $relative; Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash; Reason = 'Cooker descriptor; not a staged runtime file. IoStore input is not final-container evidence.' })
            continue
        }
        foreach ($token in $forbidden) {
            if ($relative.IndexOf($token, [StringComparison]::OrdinalIgnoreCase) -ge 0) { $failures.Add("Forbidden cooked path: $relative") }
        }
        if ($file.Extension -in @('.pak', '.utoc', '.ucas')) { $failures.Add("Opaque container must be extracted/listed before auditing: $relative") }
        if ($file.Extension -in @('.uasset', '.umap', '.ini', '.json', '.target', '.modules', '.bin')) {
            $bytes = [IO.File]::ReadAllBytes($file.FullName)
            $ascii = [Text.Encoding]::UTF8.GetString($bytes)
            $wide = [Text.Encoding]::Unicode.GetString($bytes)
            $wideShifted = if ($bytes.Length -gt 1) { [Text.Encoding]::Unicode.GetString($bytes, 1, $bytes.Length - 1) } else { '' }
            foreach ($token in $forbidden) {
                if ($ascii.IndexOf($token, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $wide.IndexOf($token, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $wideShifted.IndexOf($token, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                    $failures.Add("Forbidden reference/token '$token' in $relative")
                }
            }
            $scanned.Add([ordered]@{ Path = $relative; Bytes = $file.Length; Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash })
        }
    }
    $cookedAudited = $true
} elseif ($RequireCookedContent) {
    $failures.Add('CookedRoot is required for the Shipping cook acceptance gate.')
}
$resultPath = Join-Path $artifactDirectory 'result.json'
Write-HansaJsonArtifact -Path $resultPath -Value ([ordered]@{
    Operation = 'VerifyMediaShipping'; Status = if ($failures.Count) { 'Failed' } else { 'Succeeded' }
    ProductionReferencesAudited = $true; CookedContentAudited = $cookedAudited
    AuditProcessDisabledPlugins = @('GameFeatures')
    Scope = if ($cookedAudited) { 'Production dependencies and expanded cooked packages' } else { 'Production dependencies only; cook gate not satisfied' }
    ForbiddenTokens = $forbidden; ScannedFiles = $scanned; Failures = $failures
    CookerOnlyDescriptors = $cookMetadata
    FinalIoStoreContainerAudited = $false
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
})
if ($failures.Count) { throw "Media Shipping audit failed: $($failures -join '; '). Evidence: $resultPath" }
Write-Output "Media audit evidence: $resultPath"
