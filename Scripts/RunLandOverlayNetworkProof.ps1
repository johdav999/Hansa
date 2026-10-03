[CmdletBinding()]
param([ValidateRange(60,300)][int]$TimeoutSeconds=180)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifact=New-HansaArtifactDirectory -Context $context -Operation 'land-network'
$executable=Join-Path $context.EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe'
$listener=[System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback,0)
$listener.Start()
$port=([System.Net.IPEndPoint]$listener.LocalEndpoint).Port
$listener.Stop()
$children=[System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
function Start-LandProofProcess([string]$Name,[string[]]$Extra) {
    $info=[System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$executable
    $info.WorkingDirectory=$context.ProjectRoot
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.WindowStyle=[System.Diagnostics.ProcessWindowStyle]::Hidden
    $info.ArgumentList.Add($context.ProjectFile)
    foreach($arg in $Extra){$info.ArgumentList.Add($arg)}
    foreach($arg in @('-game','-Multiprocess','-unattended','-nop4','-nosplash','-NoSound',
        '-HansaAuthorityFixture','-DisablePlugins=ModelContextProtocol','-ddc=NoZenLocalFallback',
        "-AbsLog=$(Join-Path $artifact "$Name.log")")){$info.ArgumentList.Add($arg)}
    $process=[System.Diagnostics.Process]::Start($info)
    $children.Add($process)
    return $process
}
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    $server=Start-LandProofProcess 'server' @('/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP?listen?Scenario=lubeck_grain_shortage_v1?CampaignSeed=91470745841716','-server','-NullRHI','-ExecCmds=t.MaxFPS 30',"-port=$port")
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $serverLog=Join-Path $artifact 'server.log'
    do {
        if($server.HasExited){throw "Land proof server exited: $serverLog"}
        if([DateTime]::UtcNow -ge $deadline){throw "Server startup timed out: $serverLog"}
        Start-Sleep -Milliseconds 250
        $ready=(Test-Path -LiteralPath $serverLog) -and ((Get-Content -Raw -LiteralPath $serverLog) -match 'listening on port')
    } while(-not $ready)
    Write-Output "Land proof server ready on localhost:$port"
    $clientArgs=@("127.0.0.1:$port",'-RenderOffscreen','-Windowed','-ResX=1280','-ResY=720',
        '-ExecCmds=t.MaxFPS 30,Automation RunTests Hansa.Multiplayer.Land.LiveClient','-TestExit=Automation Test Queue Empty')
    $client1=Start-LandProofProcess 'client1' $clientArgs
    $client2=Start-LandProofProcess 'client2' $clientArgs
    while(-not $client1.HasExited -or -not $client2.HasExited){
        if([DateTime]::UtcNow -ge $deadline){throw "Land network proof timed out: $artifact"}
        Start-Sleep -Milliseconds 500
    }
    $viewers=@()
    foreach($name in @('client1','client2')){
        $log=Get-Content -Raw -LiteralPath (Join-Path $artifact "$name.log")
        if($log -notmatch 'Test Completed\. Result=\{Success\} Name=\{LiveClient\}' -or $log -match 'Result=\{Fail'){
            throw "Land RPC client test failed: $artifact/$name.log"
        }
        $match=[regex]::Match($log,'Land RPC viewer=(\d+) cells=676')
        if(-not $match.Success){throw "Missing admitted viewer evidence for $name"}
        $viewers+=[int64]$match.Groups[1].Value
    }
    if(($viewers|Select-Object -Unique).Count -ne 2){throw 'The proof requires two distinct admitted houses.'}
    Write-HansaJsonArtifact -Path (Join-Path $artifact 'result.json') -Value ([ordered]@{
        passed=$true;viewers=$viewers;transport='owner-only land RPC';maxRequestedCells=676
    })
    Write-Output "Two-client land RPC proof passed: $artifact"
} finally {
    foreach($process in $children){if(-not $process.HasExited){$process.Kill();$process.WaitForExit(10000)|Out-Null};$process.Dispose()}
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process')
}
