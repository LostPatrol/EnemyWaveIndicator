# Build and test the standalone C ABI probe using the installed x64 MSVC toolchain.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Install Desktop development with C++, including MSVC x64 and Windows SDK.' }
& (Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Host
$output = Join-Path $projectRoot ('agent\codex\native-probe-build-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location $output
try {
    $vendor = Join-Path $projectRoot 'third_party\MinHook'
    $vendorSources = @('src\buffer.c','src\hook.c','src\trampoline.c','src\hde\hde64.c') | ForEach-Object { Join-Path $vendor $_ }
    & cl.exe /nologo /O2 /MT /W3 /c @vendorSources 2>&1 | Tee-Object build-minhook.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Pinned MinHook build failed.' }
    $hookObjects = @('buffer.obj','hook.obj','trampoline.obj','hde64.obj')
    $nativeSources = @('main.cpp','GameCapture.cpp','AutomaticPresentation.cpp') | ForEach-Object { Join-Path $projectRoot ('mods\NormalWaveNativeProbe\' + $_) }
    # Static CRT avoids passing STL/CRT ownership across the module boundary or shipping new runtimes.
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 /LD /Zi @nativeSources @hookObjects /link /OUT:main.dll /DEBUG /INCREMENTAL:NO 2>&1 | Tee-Object build-dll.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Native probe compilation failed.' }
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 (Join-Path $projectRoot 'tests\native-probe-host.cpp') /Fe:probe-host.exe 2>&1 | Tee-Object build-host.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Standalone test host compilation failed.' }
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 (Join-Path $projectRoot 'tests\native-dispatch-probe.cpp') /Fe:dispatch-test.exe 2>&1 | Tee-Object build-dispatch-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch test compilation failed.' }
    & .\dispatch-test.exe | Tee-Object dispatch-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch state/lifetime tests failed.' }
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 (Join-Path $projectRoot 'tests\native-presentation-bootstrap.cpp') /Fe:presentation-test.exe 2>&1 | Tee-Object build-presentation-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Presentation test compilation failed.' }
    & .\presentation-test.exe | Tee-Object presentation-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Presentation bootstrap tests failed.' }
    & cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 (Join-Path $projectRoot 'tests\native-game-capture.cpp') GameCapture.obj @hookObjects /Fe:capture-test.exe /link /OPT:NOICF 2>&1 | Tee-Object build-capture-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Native capture ABI harness failed to compile.' }
    & .\capture-test.exe | Tee-Object capture-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Actual trampoline/ABI and capture integration tests failed.' }
    & dumpbin.exe /headers /exports /imports main.dll | Set-Content pe-inspection.txt -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw 'PE inspection failed.' }
    & .\probe-host.exe (Join-Path $output 'main.dll') | Tee-Object standalone-test.txt | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Standalone lifecycle test failed.' }
    $logs = @(Get-ChildItem -Filter 'probe-*.jsonl' | Sort-Object LastWriteTime)
    if ($logs.Count -ne 2) { throw 'Expected two separate synthetic runs.' }
    $totals = @()
    foreach ($log in $logs) {
        $events = @(Get-Content -LiteralPath $log.FullName | ForEach-Object { $_ | ConvertFrom-Json })
        $names = @($events.event)
        $expected = @('start','program_start','unreal_init','ui_init','first_update')
        if ($events.Count -eq 7) { $expected += 'heartbeat' }
        $expected += 'uninstall'
        if (($names -join ',') -ne ($expected -join ',')) { throw 'Lifecycle order or log throttling failed.' }
        $total = $events[-1].updates
        $totals += $total
        if ($events[-1].gap_min_ms -lt 0 -or $events[-1].thread_changes -ne 0) { throw 'Invalid callback statistics.' }
    }
    if (($totals | Measure-Object -Sum).Sum -ne 20001 -or (($totals | Sort-Object) -join ',') -ne '10000,10001') { throw 'Callback totals failed.' }
    $toolchain = (Get-Command cl.exe).Source
    [pscustomobject]@{
        Description = 'Synthetic standalone host only; not an in-game compatibility result.'
        BuildDirectory = $output; Compiler = $toolchain; WindowsSdk = $env:WindowsSDKVersion
        SHA256 = (Get-FileHash main.dll -Algorithm SHA256).Hash
        SourceSHA256 = (Get-FileHash (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\main.cpp')).Hash
        HeaderSHA256 = (Get-FileHash (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\DispatchProbe.h')).Hash
        EngineHeaderSHA256 = (Get-FileHash (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\EngineThreadIdentity.h')).Hash
        PresentationHeaderSHA256 = (Get-FileHash (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\PresentationBootstrap.h')).Hash
        WorldHeaderSHA256 = (Get-FileHash (Join-Path $projectRoot 'mods\NormalWaveNativeProbe\ActiveWorld.h')).Hash
        ProbeVersion = '0.6.0'; DispatchOfflinePassed = $true; PresentationOfflinePassed = $true; CaptureOfflinePassed = $true
        SourceFiles = @(@(Get-ChildItem (Join-Path $projectRoot 'mods\NormalWaveNativeProbe') -File | Where-Object Extension -in '.cpp','.h') + @(Get-ChildItem $vendor -File -Recurse) | Get-FileHash)
        OfflinePassed = $true; InGameVerified = $false; GameThreadVerified = $false
        Runs = 2; Updates = 20001
    } | ConvertTo-Json | Set-Content verification.json -Encoding utf8
} finally { Pop-Location }
Write-Output $output


