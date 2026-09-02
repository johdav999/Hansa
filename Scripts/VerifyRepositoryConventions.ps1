[CmdletBinding()]
param(
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$projectRoot = Get-HansaProjectRoot
$resolvedArtifactsRoot = if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) {
    Join-Path $projectRoot 'Saved\BuildArtifacts'
}
else {
    [System.IO.Path]::GetFullPath($ArtifactsRoot)
}

New-Item -ItemType Directory -Force -Path $resolvedArtifactsRoot | Out-Null
$artifactContext = [pscustomobject]@{ ArtifactsRoot = $resolvedArtifactsRoot }
$artifactDirectory = New-HansaArtifactDirectory -Context $artifactContext -Operation 'repository-conventions'
$resultPath = Join-Path $artifactDirectory 'result.json'
$failures = [System.Collections.Generic.List[string]]::new()
$passedChecks = [System.Collections.Generic.List[string]]::new()

function Add-HansaConventionFailure {
    param([string]$Message)
    $failures.Add($Message)
}

function Add-HansaConventionPass {
    param([string]$Message)
    $passedChecks.Add($Message)
}

function Get-HansaGitPaths {
    param([string[]]$Arguments)

    $output = & git -c "safe.directory=$projectRoot" -C $projectRoot @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Git command failed while checking repository conventions: git $($Arguments -join ' ')`n$($output -join [Environment]::NewLine)"
    }

    return @($output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Get-HansaTextFileContent {
    param([string]$RelativePath)

    $extension = [System.IO.Path]::GetExtension($RelativePath).ToLowerInvariant()
    $fileName = [System.IO.Path]::GetFileName($RelativePath)
    $textExtensions = @(
        '.bat', '.c', '.cc', '.cmd', '.cpp', '.cs', '.gitattributes', '.gitignore',
        '.h', '.hpp', '.ini', '.json', '.md', '.ps1', '.sh', '.txt', '.uplugin',
        '.uproject', '.xml', '.yaml', '.yml'
    )

    if ($extension -notin $textExtensions -and $fileName -notin @('.gitattributes', '.gitignore')) {
        return $null
    }

    $fullPath = Join-Path $projectRoot $RelativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        return $null
    }

    return Get-Content -Raw -LiteralPath $fullPath
}

try {
    $allRepositoryFiles = @(Get-HansaGitPaths -Arguments @('ls-files', '--cached', '--others', '--exclude-standard'))
    $trackedFiles = @(Get-HansaGitPaths -Arguments @('ls-files'))

    $requiredFiles = @(
        'Docs/Development/RepositoryConventions.md',
        'Content/Hansa/README.md',
        'SourceArt/README.md',
        'Tests/README.md',
        'Tests/Fixtures/fixture.example.json'
    )
    foreach ($requiredFile in $requiredFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $requiredFile) -PathType Leaf)) {
            Add-HansaConventionFailure "Required convention/example file is missing: $requiredFile"
        }
    }
    if ($failures.Count -eq 0) {
        Add-HansaConventionPass 'Required convention and example files exist.'
    }

    $secretRules = @(
        [pscustomobject]@{
            Name = 'private key block'
            Pattern = '(?i)-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'
        },
        [pscustomobject]@{
            Name = 'OpenAI-style secret key'
            Pattern = '(?<![A-Za-z0-9])sk-(?:proj-)?[A-Za-z0-9_-]{20,}'
        },
        [pscustomobject]@{
            Name = 'AWS access key'
            Pattern = '(?<![A-Z0-9])AKIA[A-Z0-9]{16}(?![A-Z0-9])'
        },
        [pscustomobject]@{
            Name = 'credential assignment'
            Pattern = '(?im)^[ \t]*(?:api[_-]?key|access[_-]?token|auth[_-]?token|bearer[_-]?token|client[_-]?secret|password|private[_-]?key|security[_-]?token)[ \t]*[:=][ \t]*["'']?(?!(?:false|true|null|none|changeme|example|replace_|<|[ \t]*$))[^#\r\n]{8,}'
        }
    )

    $secretScanCount = 0
    foreach ($relativePath in $allRepositoryFiles) {
        if ($relativePath -eq 'Scripts/VerifyRepositoryConventions.ps1') {
            continue
        }

        $content = Get-HansaTextFileContent -RelativePath $relativePath
        if ($null -eq $content) {
            continue
        }

        $secretScanCount++
        foreach ($rule in $secretRules) {
            if ([regex]::IsMatch($content, $rule.Pattern)) {
                Add-HansaConventionFailure "Potential $($rule.Name) found in $relativePath. Remove it and rotate/revoke the credential if it was real."
            }
        }
    }
    if (-not ($failures | Where-Object { $_ -like 'Potential *' })) {
        Add-HansaConventionPass "High-confidence secret scan passed for $secretScanCount repository text files."
    }

    $providerPattern = '(?i)\b(?:OpenAI|Tripo|ElevenLabs|TRELLIS)\b|api\.openai\.com|api\.elevenlabs\.io'
    $runtimeConfigurationFiles = $allRepositoryFiles | Where-Object {
        $_ -eq 'Hansa.uproject' -or
        $_ -like 'Config/*' -or
        $_ -like 'Source/Hansa/*' -or
        $_ -like 'Source/HansaSimulation/*'
    }
    $providerFailuresBefore = $failures.Count
    foreach ($relativePath in $runtimeConfigurationFiles) {
        $content = Get-HansaTextFileContent -RelativePath $relativePath
        if ($null -ne $content -and [regex]::IsMatch($content, $providerPattern)) {
            Add-HansaConventionFailure "Provider-specific configuration or dependency token is forbidden in runtime/project configuration: $relativePath"
        }
    }
    if ($failures.Count -eq $providerFailuresBefore) {
        Add-HansaConventionPass 'Runtime source, project configuration, and descriptor contain no provider-specific configuration.'
    }

    $forbiddenProductionTokens = @(
        '/Game/Hansa/Developer',
        '/Game/Hansa/Generated/Staging',
        'Content/Hansa/Generated/Staging',
        'Saved/GenerationJobs'
    )
    $productionFiles = $allRepositoryFiles | Where-Object {
        $_ -eq 'Hansa.uproject' -or
        $_ -like 'Source/Hansa/*' -or
        $_ -like 'Source/HansaSimulation/*' -or
        ($_ -like 'Content/Hansa/*' -and $_ -notlike 'Content/Hansa/Developer/*' -and $_ -notlike 'Content/Hansa/Generated/*')
    }
    $stagingFailuresBefore = $failures.Count
    foreach ($relativePath in $productionFiles) {
        $fullPath = Join-Path $projectRoot $relativePath
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            continue
        }

        $foundTokens = @(Find-HansaAsciiTokens -Path $fullPath -Tokens $forbiddenProductionTokens)
        if ($foundTokens.Count -gt 0) {
            Add-HansaConventionFailure "Production file references a developer/transient path: $relativePath ($($foundTokens -join ', '))"
        }
    }
    if ($failures.Count -eq $stagingFailuresBefore) {
        Add-HansaConventionPass 'Production source/content contains no Developer or generated-staging references.'
    }

    $defaultGamePath = Join-Path $projectRoot 'Config\DefaultGame.ini'
    $defaultEnginePath = Join-Path $projectRoot 'Config\DefaultEngine.ini'
    $defaultGame = Get-Content -Raw -LiteralPath $defaultGamePath
    $defaultEngine = Get-Content -Raw -LiteralPath $defaultEnginePath
    $configurationFailuresBefore = $failures.Count

    foreach ($requiredConfigText in @(
        '[Hansa.Project]',
        'ConventionVersion=1',
        '[Hansa.Automation]',
        'bEnableTransport=False',
        '/Game/Hansa/Developer',
        '/Game/Hansa/Generated/Staging'
    )) {
        if (-not $defaultGame.Contains($requiredConfigText)) {
            Add-HansaConventionFailure "Config/DefaultGame.ini is missing required convention/cook policy: $requiredConfigText"
        }
    }

    if ([regex]::IsMatch($defaultEngine, '(?im)^[ \t]*SecurityToken[ \t]*=[ \t]*\S+')) {
        Add-HansaConventionFailure 'Config/DefaultEngine.ini contains a nonempty Android file-server SecurityToken.'
    }
    if ([regex]::IsMatch($defaultEngine, '(?im)^\s*bAllowNetworkConnection\s*=\s*True\s*$')) {
        Add-HansaConventionFailure 'Android file-server network access must remain disabled in checked-in shared config.'
    }
    if ($failures.Count -eq $configurationFailuresBefore) {
        Add-HansaConventionPass 'Namespaced config defaults, cook exclusions, and Android file-server safety defaults are present.'
    }

    $generatedTrackingPattern = '(^|/)(?:Binaries|DerivedDataCache|Intermediate|Saved|\.vs)(?:/|$)|\.slnx?$|^Content/Hansa/Generated/Staging(?:/|$)'
    $trackedGenerated = @($trackedFiles | Where-Object { $_ -match $generatedTrackingPattern })
    foreach ($trackedPath in $trackedGenerated) {
        Add-HansaConventionFailure "Generated or transient path must not be tracked: $trackedPath"
    }
    if ($trackedGenerated.Count -eq 0) {
        Add-HansaConventionPass 'No generated Unreal, IDE, evidence, or staging output is tracked.'
    }

    $ignoreProbes = @(
        'Binaries/__convention_probe__.txt',
        'DerivedDataCache/__convention_probe__.txt',
        'Intermediate/__convention_probe__.txt',
        'Saved/BuildArtifacts/__convention_probe__.txt',
        '.vs/__convention_probe__.txt',
        'Hansa.sln',
        'Content/Hansa/Generated/Staging/__convention_probe__.uasset'
    )
    $ignoreFailuresBefore = $failures.Count
    foreach ($probe in $ignoreProbes) {
        & git -c "safe.directory=$projectRoot" -C $projectRoot check-ignore --quiet -- $probe
        if ($LASTEXITCODE -ne 0) {
            Add-HansaConventionFailure "Expected generated/transient path is not ignored: $probe"
        }
    }
    if ($failures.Count -eq $ignoreFailuresBefore) {
        Add-HansaConventionPass 'Generated Unreal, IDE, evidence, and staging probes are ignored.'
    }

    $lfsProbes = @(
        'Content/Hansa/__convention_probe__.uasset',
        'Content/Hansa/__convention_probe__.umap',
        'SourceArt/__convention_probe__.fbx',
        'SourceArt/__convention_probe__.wav',
        'SourceArt/__convention_probe__.png'
    )
    $lfsFailuresBefore = $failures.Count
    foreach ($probe in $lfsProbes) {
        $attribute = & git -c "safe.directory=$projectRoot" -C $projectRoot check-attr filter -- $probe
        if ($LASTEXITCODE -ne 0 -or $attribute -notmatch ': filter: lfs$') {
            Add-HansaConventionFailure "Expected Git LFS routing is missing for representative asset: $probe"
        }
    }
    if ($failures.Count -eq $lfsFailuresBefore) {
        Add-HansaConventionPass 'Representative Unreal, source-art, audio, and raster assets route through Git LFS.'
    }

    $testSourceFiles = $allRepositoryFiles | Where-Object { $_ -like 'Source/HansaTests/*.cpp' }
    $testNames = [System.Collections.Generic.List[string]]::new()
    foreach ($relativePath in $testSourceFiles) {
        $content = Get-HansaTextFileContent -RelativePath $relativePath
        if ($null -eq $content) {
            continue
        }

        foreach ($match in [regex]::Matches($content, '(?s)IMPLEMENT_(?:SIMPLE|COMPLEX)_AUTOMATION_TEST\s*\(.*?["''](Hansa\.[A-Za-z0-9_.-]+)["'']')) {
            $testNames.Add($match.Groups[1].Value)
        }
    }
    $testFailuresBefore = $failures.Count
    if ($testNames.Count -eq 0) {
        Add-HansaConventionFailure 'No Hansa Unreal automation test names were discovered for naming validation.'
    }
    foreach ($testName in $testNames) {
        if ($testName -notmatch '^Hansa\.(?:Architecture|Simulation|Integration|Content|UI|Multiplayer|EndToEnd)\.[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$') {
            Add-HansaConventionFailure "Unreal automation test name does not follow Hansa.<Layer>.<Feature>.<Behavior>: $testName"
        }
    }
    if ($failures.Count -eq $testFailuresBefore) {
        Add-HansaConventionPass "Validated $($testNames.Count) Unreal automation test names."
    }

    $fixtureTemplatePath = Join-Path $projectRoot 'Tests\Fixtures\fixture.example.json'
    $fixtureTemplate = Get-Content -Raw -LiteralPath $fixtureTemplatePath | ConvertFrom-Json
    $fixtureFailuresBefore = $failures.Count
    if ([string]$fixtureTemplate.fixtureId -notmatch '^[a-z][a-z0-9]*(?:_[a-z0-9]+)*_v[1-9][0-9]*$') {
        Add-HansaConventionFailure 'Fixture template ID must use lowercase snake case with a _vN suffix.'
    }
    if ([int]$fixtureTemplate.schemaVersion -lt 1) {
        Add-HansaConventionFailure 'Fixture template schemaVersion must be at least 1.'
    }
    if ([int64]$fixtureTemplate.initialTick -lt 0) {
        Add-HansaConventionFailure 'Fixture template initialTick cannot be negative.'
    }
    if ($failures.Count -eq $fixtureFailuresBefore) {
        Add-HansaConventionPass 'Fixture template parses and follows identity/version conventions.'
    }
}
catch {
    Add-HansaConventionFailure "Convention audit could not complete: $($_.Exception.Message)"
}

$result = [ordered]@{
    Operation = 'VerifyRepositoryConventions'
    Status = if ($failures.Count -eq 0) { 'Succeeded' } else { 'Failed' }
    ProjectRoot = $projectRoot
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    PassedChecks = $passedChecks
    Failures = $failures
}
Write-HansaJsonArtifact -Path $resultPath -Value $result

if ($failures.Count -gt 0) {
    throw "Repository convention audit failed:`n$($failures -join [Environment]::NewLine)`nEvidence: $resultPath"
}

Write-Output "Repository convention audit succeeded. Evidence: $resultPath"
