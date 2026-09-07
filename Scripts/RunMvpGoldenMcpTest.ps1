[CmdletBinding()]
param(
	[switch]$SkipBuild,
	[string]$EngineRoot,
	[string]$ArtifactsRoot,
	[ValidateRange(30, 300)]
	[int]$TimeoutSeconds = 120
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'mvp-golden-mcp'
$unrealEditor = Join-Path $context.EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$driver = Join-Path $context.ProjectRoot 'Tools\HansaMcp\scripts\golden-mvp-flow.js'
$resultPath = Join-Path $artifactDirectory 'result.json'
$unrealLogPath = Join-Path $artifactDirectory 'UnrealEditor.log'
$driverLogPath = Join-Path $artifactDirectory 'GoldenDriver.log'

if (-not (Test-Path -LiteralPath $unrealEditor -PathType Leaf)) {
	throw "UnrealEditor.exe is missing: $unrealEditor"
}
if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) {
	throw "S14-P01 golden driver is missing: $driver"
}
$nodeCommand = Get-Command node -ErrorAction SilentlyContinue
if ($null -eq $nodeCommand -or -not (Test-Path -LiteralPath $nodeCommand.Source -PathType Leaf)) {
	throw 'Node.js 22 or newer is required to run the S14-P01 golden MCP test.'
}
$nodeVersionText = (& $nodeCommand.Source --version).Trim()
$nodeMajorVersion = [int]($nodeVersionText.TrimStart('v').Split('.')[0])
if ($nodeMajorVersion -lt 22) {
	throw "The S14-P01 golden MCP test requires Node.js 22 or newer; found $nodeVersionText."
}

if (-not $SkipBuild) {
	$buildArguments = @(
		'HansaEditor'
		'Win64'
		'Development'
		"-Project=$($context.ProjectFile)"
		'-WaitMutex'
		'-NoHotReloadFromIDE'
	)
	Invoke-HansaNativeCommand -FilePath $context.BuildScript -Arguments $buildArguments `
		-LogPath (Join-Path $artifactDirectory 'BuildEditor.log') `
		-FailureMessage 'The Editor build required for the S14-P01 golden MCP test failed.' | Out-Null
}

$runId = [Guid]::NewGuid().ToString('N')
$pipeName = "hansa-s14p01-$runId"
$token = "s14-p01-$runId"
$evidenceRoot = Join-Path $context.ProjectRoot 'Saved\TestEvidence\Automation\S14P01\s14-p01-mvp-golden'
$previousPipe = [Environment]::GetEnvironmentVariable('HANSA_AUTOMATION_PIPE', 'Process')
$previousToken = [Environment]::GetEnvironmentVariable('HANSA_AUTOMATION_TOKEN', 'Process')
$previousSkipSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
$gameProcess = $null
$status = 'Failed'
$failure = ''
$failureDetails = ''

try {
	[Environment]::SetEnvironmentVariable('HANSA_AUTOMATION_PIPE', $pipeName, 'Process')
	[Environment]::SetEnvironmentVariable('HANSA_AUTOMATION_TOKEN', $token, 'Process')
	[Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
	$gameArguments = @(
		"`"$($context.ProjectFile)`""
		'-game'
		'-unattended'
		'-nop4'
		'-nosplash'
		'-NoSound'
		'-RenderOffscreen'
		'-d3d11'
		'-HansaAutomation'
		'-HansaAutomationPermission=FixtureControl'
		"-AbsLog=`"$unrealLogPath`""
	)
	$gameProcess = Start-Process -FilePath $unrealEditor -ArgumentList $gameArguments `
		-WorkingDirectory $context.ProjectRoot -WindowStyle Hidden -PassThru

	$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
	$endpointReady = $false
	while (-not $endpointReady) {
		$gameProcess.Refresh()
		if ($gameProcess.HasExited) {
			throw "The Development game exited with code $($gameProcess.ExitCode) before opening its automation pipe."
		}
		if ([DateTime]::UtcNow -ge $deadline) {
			throw "The Development game did not open its automation pipe within $TimeoutSeconds seconds."
		}
		if (Test-Path -LiteralPath $unrealLogPath -PathType Leaf) {
			$endpointReady = Select-String -LiteralPath $unrealLogPath -SimpleMatch `
				'Hansa automation named-pipe endpoint is enabled' -Quiet
		}
		Start-Sleep -Milliseconds 250
	}

	Invoke-HansaNativeCommand -FilePath $nodeCommand.Source -Arguments @($driver) `
		-LogPath $driverLogPath -FailureMessage 'The live S14-P01 golden MCP flow failed.' | Out-Null
	if (-not (Test-Path -LiteralPath (Join-Path $evidenceRoot 'bundle.json') -PathType Leaf)) {
		throw "The live golden flow did not create its expected evidence manifest: $evidenceRoot"
	}
	$bundle = Get-Content -LiteralPath (Join-Path $evidenceRoot 'bundle.json') -Raw | ConvertFrom-Json
	if ($bundle.complete -ne $true) {
		throw "The live golden evidence bundle is incomplete: $evidenceRoot"
	}
	$status = 'Succeeded'
}
catch {
	$failure = $_.Exception.Message
	if (Test-Path -LiteralPath $driverLogPath -PathType Leaf) {
		$failureDetails = (Get-Content -LiteralPath $driverLogPath | Select-Object -Last 30) -join [Environment]::NewLine
	}
	throw
}
finally {
	if ($null -ne $gameProcess) {
		$gameProcess.Refresh()
		if (-not $gameProcess.HasExited) {
			Stop-Process -Id $gameProcess.Id -Force -ErrorAction SilentlyContinue
			[void]$gameProcess.WaitForExit(10000)
		}
	}
	[Environment]::SetEnvironmentVariable('HANSA_AUTOMATION_PIPE', $previousPipe, 'Process')
	[Environment]::SetEnvironmentVariable('HANSA_AUTOMATION_TOKEN', $previousToken, 'Process')
	[Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $previousSkipSdk, 'Process')
	Write-HansaJsonArtifact -Path $resultPath -Value ([ordered]@{
		Operation = 'RunMvpGoldenMcpTest'
		Status = $status
		Failure = $failure
		FailureDetails = $failureDetails
		NodeVersion = $nodeVersionText
		EditorBuildSkipped = [bool]$SkipBuild
		UnrealLog = $unrealLogPath
		DriverLog = $driverLogPath
		EvidenceRoot = $evidenceRoot
		CompletedUtc = [DateTime]::UtcNow.ToString('o')
	})
}

Write-Output "S14-P01 live golden MCP test succeeded. Artifacts: $artifactDirectory"
