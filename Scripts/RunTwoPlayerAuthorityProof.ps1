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
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'two-player-authority'
$unrealEditor = Join-Path $context.EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$driver = Join-Path $context.ProjectRoot 'Tools\HansaMcp\scripts\two-player-authority-flow.js'
if (-not (Test-Path -LiteralPath $unrealEditor -PathType Leaf)) {
	throw "UnrealEditor.exe is missing: $unrealEditor"
}
if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) {
	throw "Authority proof driver is missing: $driver"
}
$nodeCommand = Get-Command node -ErrorAction SilentlyContinue
if ($null -eq $nodeCommand -or -not (Test-Path -LiteralPath $nodeCommand.Source -PathType Leaf)) {
	throw 'Node.js 22 or newer is required to run the multiplayer authority proof.'
}
$nodeVersionText = (& $nodeCommand.Source --version).Trim()
$nodeMajorVersion = [int]($nodeVersionText.TrimStart('v').Split('.')[0])
if ($nodeMajorVersion -lt 22) {
	throw "The multiplayer authority proof requires Node.js 22 or newer; found $nodeVersionText."
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
	Invoke-HansaNativeCommand -FilePath $context.BuildScript -Arguments $buildArguments -LogPath (Join-Path $artifactDirectory 'BuildEditor.log') -FailureMessage 'The Editor build required for the multiplayer proof failed.' | Out-Null
}

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = ([System.Net.IPEndPoint]$listener.LocalEndpoint).Port
$listener.Stop()

$runId = [Guid]::NewGuid().ToString('N')
$token = "s11-p04-$runId"
$configPath = Join-Path $artifactDirectory 'driver-config.json'
Write-HansaJsonArtifact -Path $configPath -Value ([ordered]@{
	projectRoot = $context.ProjectRoot
	projectFile = $context.ProjectFile
	unrealEditor = $unrealEditor
	artifactDirectory = $artifactDirectory
	map = '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP'
	seed = '91470745841716'
	port = $port
	timeoutSeconds = $TimeoutSeconds
	pipes = [ordered]@{
		server = "hansa-s11p04-server-$runId"
		client1 = "hansa-s11p04-client1-$runId"
		client2 = "hansa-s11p04-client2-$runId"
		reconnect = "hansa-s11p04-reconnect-$runId"
	}
})

$previousToken = [Environment]::GetEnvironmentVariable('HANSA_AUTOMATION_TOKEN', 'Process')
$previousSkipSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
try {
	[Environment]::SetEnvironmentVariable('HANSA_AUTOMATION_TOKEN', $token, 'Process')
	[Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
	Invoke-HansaNativeCommand -FilePath $nodeCommand.Source -Arguments @($driver, $configPath) -LogPath (Join-Path $artifactDirectory 'Driver.log') -FailureMessage 'The two-player authority process proof failed.' | Out-Null
}
finally {
	[Environment]::SetEnvironmentVariable('HANSA_AUTOMATION_TOKEN', $previousToken, 'Process')
	[Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $previousSkipSdk, 'Process')
}

$resultPath = Join-Path $artifactDirectory 'result.json'
if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) {
	throw "The driver did not create its result artifact: $resultPath"
}
$result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
if ($result.status -ne 'Succeeded') {
	throw "The multiplayer proof result was not successful: $resultPath"
}
Write-Output "Two-player authority proof succeeded. Artifacts: $artifactDirectory"
