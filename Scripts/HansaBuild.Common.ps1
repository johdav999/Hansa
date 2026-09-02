Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-HansaProjectRoot {
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
}

function Add-HansaEngineCandidate {
    param(
        [System.Collections.Generic.List[string]]$Candidates,
        [string]$Candidate
    )

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return
    }

    $fullPath = [System.IO.Path]::GetFullPath($Candidate.Trim().TrimEnd('\', '/'))
    if (-not $Candidates.Contains($fullPath)) {
        $Candidates.Add($fullPath)
    }
}

function Get-HansaEngineCandidateIssue {
    param(
        [string]$Candidate,
        [string]$EngineAssociation
    )

    if (-not (Test-Path -LiteralPath $Candidate -PathType Container)) {
        return 'directory does not exist'
    }

    $buildScript = Join-Path $Candidate 'Engine\Build\BatchFiles\Build.bat'
    if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
        return 'Engine\Build\BatchFiles\Build.bat is missing'
    }

    $editorCommand = Join-Path $Candidate 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) {
        return 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe is missing'
    }

    $buildVersionPath = Join-Path $Candidate 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $buildVersionPath -PathType Leaf)) {
        return 'Engine\Build\Build.version is missing'
    }

    $buildVersion = Get-Content -Raw -LiteralPath $buildVersionPath | ConvertFrom-Json
    $candidateAssociation = "$($buildVersion.MajorVersion).$($buildVersion.MinorVersion)"
    if (-not [string]::IsNullOrWhiteSpace($EngineAssociation) -and
        $candidateAssociation -ne $EngineAssociation) {
        return "engine version $candidateAssociation does not match project EngineAssociation $EngineAssociation"
    }

    return $null
}

function Resolve-HansaEngineRoot {
    param(
        [string]$EngineRoot,
        [string]$ProjectFile
    )

    $projectDescriptor = Get-Content -Raw -LiteralPath $ProjectFile | ConvertFrom-Json
    $engineAssociation = [string]$projectDescriptor.EngineAssociation
    $candidates = [System.Collections.Generic.List[string]]::new()

    if (-not [string]::IsNullOrWhiteSpace($EngineRoot)) {
        Add-HansaEngineCandidate -Candidates $candidates -Candidate $EngineRoot
        $explicitCandidate = $candidates[0]
        $explicitIssue = Get-HansaEngineCandidateIssue -Candidate $explicitCandidate -EngineAssociation $engineAssociation
        if ($null -ne $explicitIssue) {
            throw "Explicit -EngineRoot '$explicitCandidate' is invalid: $explicitIssue"
        }

        return $explicitCandidate
    }

    if (-not [string]::IsNullOrWhiteSpace($env:HANSA_UNREAL_ENGINE_ROOT)) {
        Add-HansaEngineCandidate -Candidates $candidates -Candidate $env:HANSA_UNREAL_ENGINE_ROOT
        $environmentCandidate = $candidates[0]
        $environmentIssue = Get-HansaEngineCandidateIssue -Candidate $environmentCandidate -EngineAssociation $engineAssociation
        if ($null -ne $environmentIssue) {
            throw "HANSA_UNREAL_ENGINE_ROOT '$environmentCandidate' is invalid: $environmentIssue"
        }

        return $environmentCandidate
    }

    if (-not [string]::IsNullOrWhiteSpace($engineAssociation)) {
        $machineRegistryPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$engineAssociation"
        if (Test-Path -LiteralPath $machineRegistryPath) {
            $machineRegistration = Get-ItemProperty -LiteralPath $machineRegistryPath
            Add-HansaEngineCandidate -Candidates $candidates -Candidate $machineRegistration.InstalledDirectory
        }

        $userBuildsPath = 'HKCU:\Software\Epic Games\Unreal Engine\Builds'
        if (Test-Path -LiteralPath $userBuildsPath) {
            $userBuilds = Get-ItemProperty -LiteralPath $userBuildsPath
            foreach ($property in $userBuilds.PSObject.Properties) {
                if ($property.Name -eq $engineAssociation) {
                    Add-HansaEngineCandidate -Candidates $candidates -Candidate ([string]$property.Value)
                }
            }
        }
    }

    $launcherManifestPath = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path -LiteralPath $launcherManifestPath) {
        $launcherManifest = Get-Content -Raw -LiteralPath $launcherManifestPath | ConvertFrom-Json
        $matchingInstallations = $launcherManifest.InstallationList | Where-Object {
            $_.AppName -eq "UE_$engineAssociation"
        }

        foreach ($installation in $matchingInstallations) {
            Add-HansaEngineCandidate -Candidates $candidates -Candidate ([string]$installation.InstallLocation)
        }
    }

    $candidateIssues = [System.Collections.Generic.List[string]]::new()
    foreach ($candidate in $candidates) {
        $candidateIssue = Get-HansaEngineCandidateIssue -Candidate $candidate -EngineAssociation $engineAssociation
        if ($null -eq $candidateIssue) {
            return $candidate
        }

        $candidateIssues.Add("${candidate}: $candidateIssue")
    }

    $inspected = if ($candidateIssues.Count -gt 0) { $candidateIssues -join [Environment]::NewLine } else { '<none>' }
    throw @"
Unable to locate an Unreal Engine installation compatible with EngineAssociation '$engineAssociation'.
Pass -EngineRoot explicitly or set HANSA_UNREAL_ENGINE_ROOT.
Candidates inspected:
$inspected
"@
}

function Get-HansaBuildContext {
    param(
        [string]$EngineRoot,
        [string]$ArtifactsRoot
    )

    $projectRoot = Get-HansaProjectRoot
    $projectFile = Join-Path $projectRoot 'Hansa.uproject'
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        throw "Hansa.uproject was not found at the expected repository root: $projectFile"
    }

    $resolvedEngineRoot = Resolve-HansaEngineRoot -EngineRoot $EngineRoot -ProjectFile $projectFile
    $resolvedArtifactsRoot = if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) {
        Join-Path $projectRoot 'Saved\BuildArtifacts'
    }
    else {
        [System.IO.Path]::GetFullPath($ArtifactsRoot)
    }

    New-Item -ItemType Directory -Force -Path $resolvedArtifactsRoot | Out-Null

    return [pscustomobject]@{
        ProjectRoot = $projectRoot
        ProjectFile = $projectFile
        EngineRoot = $resolvedEngineRoot
        ArtifactsRoot = $resolvedArtifactsRoot
        BuildScript = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\Build.bat'
        GenerateProjectFilesScript = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\GenerateProjectFiles.bat'
        UnrealBuildTool = Join-Path $resolvedEngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'
        UnrealEditorCommand = Join-Path $resolvedEngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    }
}

function New-HansaArtifactDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Context,
        [Parameter(Mandatory = $true)]
        [string]$Operation
    )

    $safeOperation = $Operation -replace '[^A-Za-z0-9_.-]', '-'
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmssfff'
    $artifactDirectory = Join-Path $Context.ArtifactsRoot "$timestamp-$safeOperation"
    New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
    return $artifactDirectory
}

function Format-HansaCommandArgument {
    param([string]$Argument)

    if ($Argument -match '[\s"]') {
        return '"' + ($Argument -replace '"', '\"') + '"'
    }

    return $Argument
}

function Invoke-HansaNativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$LogPath,
        [Parameter(Mandatory = $true)]
        [string]$FailureMessage
    )

    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "Required executable or script was not found: $FilePath"
    }

    $logDirectory = Split-Path -Parent $LogPath
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

    $displayArguments = ($Arguments | ForEach-Object { Format-HansaCommandArgument $_ }) -join ' '
    @(
        "StartedUtc: $([DateTime]::UtcNow.ToString('o'))"
        "WorkingDirectory: $(Get-Location)"
        "Command: $FilePath $displayArguments"
        ''
    ) | Set-Content -LiteralPath $LogPath -Encoding utf8

    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $LogPath -Append
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) {
        $exitCode = 0
    }

    if ($exitCode -ne 0) {
        throw "$FailureMessage Exit code: $exitCode. Log: $LogPath"
    }

    return $exitCode
}

function Write-HansaJsonArtifact {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $Value | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Path -Encoding utf8
}

function Find-HansaAsciiTokens {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string[]]$Tokens
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Cannot scan missing file: $Path"
    }

    $remaining = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($token in $Tokens) {
        [void]$remaining.Add($token)
    }

    $found = [System.Collections.Generic.List[string]]::new()
    $maximumTokenLength = ($Tokens | ForEach-Object Length | Measure-Object -Maximum).Maximum
    $tailLength = [Math]::Max(0, $maximumTokenLength - 1)
    $buffer = [byte[]]::new(4MB)
    $tail = ''
    $stream = [System.IO.File]::OpenRead($Path)

    try {
        while (($bytesRead = $stream.Read($buffer, 0, $buffer.Length)) -gt 0 -and $remaining.Count -gt 0) {
            $chunk = $tail + [System.Text.Encoding]::ASCII.GetString($buffer, 0, $bytesRead)
            foreach ($token in @($remaining)) {
                if ($chunk.IndexOf($token, [System.StringComparison]::Ordinal) -ge 0) {
                    $found.Add($token)
                    [void]$remaining.Remove($token)
                }
            }

            if ($chunk.Length -gt $tailLength) {
                $tail = $chunk.Substring($chunk.Length - $tailLength)
            }
            else {
                $tail = $chunk
            }
        }
    }
    finally {
        $stream.Dispose()
    }

    return $found.ToArray()
}
