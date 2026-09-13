$ErrorActionPreference = 'Stop'
$repoPath = Split-Path $PSScriptRoot -Parent
$jobPath = Join-Path $repoPath 'Saved/GenerationJobs/city-life_P20_20260908'
$checkpointPath = Join-Path $repoPath 'SourceArt/Generated/Props/HansaCityLife_P20_20260908/Review_r6'
if (Test-Path -LiteralPath $checkpointPath) { throw 'Checkpoint exists; refuse overwrite.' }
New-Item -ItemType Directory -Path $checkpointPath | Out-Null
Copy-Item -LiteralPath (Join-Path $jobPath 'exports-r6') -Destination (Join-Path $checkpointPath 'exports') -Recurse
foreach ($folder in @('renders','evidence','textures','scripts')) {
    New-Item -ItemType Directory -Path (Join-Path $checkpointPath $folder) | Out-Null
}
foreach ($name in @('r6-whole.png','unreal-props-SM_HansaWell_Oak-r2.png','unreal-props-SM_HansaWell_Oak-r6.png','unreal-props-SM_HansaShoreDebris_Driftwood-r6.png')) {
    Copy-Item -LiteralPath (Join-Path $jobPath ('renders/'+$name)) -Destination (Join-Path $checkpointPath 'renders')
}
foreach ($name in @('reimport-r6.json','unreal-street-meshes-r6.json','prop-review-placement-r6.json','prop-native-captures-r6.json','prop-lod-correction.json')) {
    Copy-Item -LiteralPath (Join-Path $jobPath ('evidence/'+$name)) -Destination (Join-Path $checkpointPath 'evidence')
}
foreach ($name in @('oak-source.png','oak-source.prompt.md')) {
    Copy-Item -LiteralPath (Join-Path $jobPath ('textures/'+$name)) -Destination (Join-Path $checkpointPath 'textures')
}
foreach ($name in @('BuildEnhancedMvpCityProps.py','ExportEnhancedMvpCityProps.py','VerifyEnhancedMvpCityProps.py','StageEnhancedMvpCityPropsR6.py','CaptureEnhancedMvpCityProps.py')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $checkpointPath 'scripts')
}
$terrainEvidence = Join-Path $repoPath 'SourceArt/Terrain/Rostock/Survey_20260908/NativeEncoding_v2/NativeReview_20260908'
if (Test-Path -LiteralPath $terrainEvidence) { throw 'Terrain review exists; refuse overwrite.' }
New-Item -ItemType Directory -Path $terrainEvidence | Out-Null
foreach ($name in @('rostock-native-survey-reopened.json','rostock-native-guard-tests.json','rostock-survey-baseline-r2.json')) {
    Copy-Item -LiteralPath (Join-Path $jobPath ('evidence/'+$name)) -Destination $terrainEvidence
}
Copy-Item -LiteralPath (Join-Path $jobPath 'renders/rostock-survey-baseline-r2.png') -Destination $terrainEvidence
Write-Output 'Retained P20 revision-six draft and Rostock native verification evidence; no promotion.'
