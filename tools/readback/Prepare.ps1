param([Parameter(Mandatory)][ValidatePattern('^attempt-[0-9]{2}$')][string]$Attempt)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$out=Join-Path $root "build/readback/$Attempt"
if(Test-Path $out){throw 'Fresh attempt required'}
$candidate=Join-Path $root 'build/Release/daytona.exe'
$owner='E:/Source/toolkits/launchbox/Launchbox-Racing/Games/Windows/Daytona USA (Recomp)'
$capture='E:/Source/game-modding/_toolkits/dbce-wheel-mod-toolkit/tools/window-capture/bin/Release/net8.0-windows10.0.22621.0/dbce-window-capture.exe'
$job='E:/Source/_work/soba-truckful-provider-free/artifacts/provider-free/rehearsal-native-go501049-amend501065/native/Truckful.Live.dll'
$observer='E:/Source/_work/remi-dirt4-loose-map-20261103/build/ReadbackObserver.dll'
$python='C:/Users/antho/AppData/Local/Programs/Python/Python314/python.exe'
foreach($pair in @(@($job,'725E52B00A36B8189BF4BAF3CF201D1266614B49DB124F2C410884D2D96BE4DD'),@($observer,'2E45A82CD860C5A819D046746C5B1787B4B3939C8D35180498708C161D9BA85D'),@($capture,'6C4558042FAF95C54DEEF04A78719B95FF62D3CFC62858DFCA761AEDC7499EF4'))){if((Get-FileHash -LiteralPath $pair[0]).Hash -cne $pair[1]){throw "Helper pin changed: $($pair[0])"}}
if(git -C $root status --porcelain --untracked-files=no){throw 'Clean tracked source required'}
$commit=(git -C $root rev-parse HEAD).Trim()
$receipt=Get-Content "$root/build/readback-build.json" -Raw|ConvertFrom-Json
git -C $root diff --quiet $receipt.source HEAD -- CMakeLists.txt src cmake
if($LASTEXITCODE -or $receipt.candidate -cne (Get-FileHash $candidate).Hash){throw 'Build receipt does not match compiled source/candidate'}
[void](New-Item -ItemType Directory $out)
# Read-only use of the installed ROM; no owner assets are written or copied back.
$rom=Join-Path $owner 'roms/daytona93.zip'
$canonical=& $python -B -c 'import json,pathlib,sys;print(json.dumps([str(pathlib.Path(p).resolve(strict=True)) for p in sys.argv[1:]]))' $candidate $rom $capture $job $observer $python | ConvertFrom-Json
if($LASTEXITCODE -or $canonical.Count -ne 6){throw 'Canonical executable/asset resolution failed'}
$candidate,$rom,$capture,$job,$observer,$python=$canonical
$roots=@($owner,(Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'daytona-recomp/daytona93'))
$roots|ConvertTo-Json|Set-Content "$out/owner-roots.json" -Encoding utf8NoBOM
& $python -B "$PSScriptRoot/owner.py" snapshot "$out/owner-roots.json" "$out/owner-preflight.json"
if($LASTEXITCODE){throw 'Owner preflight failed'}
$capturePayload=@(Get-ChildItem (Split-Path $capture) -File|ForEach-Object FullName)
$pins=@(foreach($p in @($candidate,$rom,$job,$observer,$python,"$out/owner-roots.json","$out/owner-preflight.json")+$capturePayload+(Get-ChildItem $PSScriptRoot -File|ForEach-Object FullName)){
 @{path=[IO.Path]::GetFullPath($p);sha256=(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
})
$plan=@{source=$receipt.source;runnerSource=$commit;attemptRoot=$out;session=[Guid]::NewGuid().ToString('N');candidate=$candidate;rom=$rom;capture=$capture;job=$job;observer=$observer;python=$python;pins=$pins;frames=1800}
$plan|ConvertTo-Json -Depth 6|Set-Content "$out/prepared.json" -Encoding utf8NoBOM
Get-FileHash "$out/prepared.json"
