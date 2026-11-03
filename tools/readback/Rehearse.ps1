param([Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Fresh rehearsal output required'}
$out=(New-Item -ItemType Directory $Output).FullName
$python='C:/Users/antho/AppData/Local/Programs/Python/Python314/python.exe'
$job='E:/Source/_work/soba-truckful-provider-free/artifacts/provider-free/rehearsal-native-go501049-amend501065/native/Truckful.Live.dll'
if((Get-FileHash $job).Hash -cne '725E52B00A36B8189BF4BAF3CF201D1266614B49DB124F2C410884D2D96BE4DD'){throw 'Job helper pin differs'}
Add-Type -Path $job;Add-Type -Path "$PSScriptRoot/Snapshot.cs"
$file=Join-Path $out 'state.json'
function Publish($n){@{n=$n}|ConvertTo-Json -Compress|Set-Content "$file.tmp" -Encoding utf8NoBOM;[DaytonaSnapshot]::Publish("$file.tmp",$file)}
Publish 0
$argsRun=@('-B',"$PSScriptRoot/test_shared.py",'--watch',$file,"$out/reader.json")
$identity=[TruckfulLive]::Launch($python,$out,(($argsRun|ForEach-Object{'"'+$_+'"'})-join ' '))
$identity|ConvertTo-Json|Set-Content "$out/root.json"
$initial=@([TruckfulLive]::JobPids())
if($initial -notcontains $identity.pid){throw 'Suspended root not in own job'}
foreach($n in 1..1000){Publish $n}
$clock=[Diagnostics.Stopwatch]::StartNew()
while(@([TruckfulLive]::JobPids()).Count -and $clock.Elapsed.TotalSeconds -lt 10){Start-Sleep -Milliseconds 50}
if(@([TruckfulLive]::JobPids()).Count){Write-Output 'CUSTODY_RETAINED: rehearsal child did not exit';while($true){Start-Sleep -Seconds 1}}
[TruckfulLive]::ReleaseJob()
$result=Get-Content "$out/reader.json" -Raw|ConvertFrom-Json
if($result.errors.Count -or $result.reads -lt 1){throw 'Shared-delete stress failed'}
& $python -B "$PSScriptRoot/test_shared.py"
if($LASTEXITCODE){throw 'Production admission/owner fixtures failed'}
@{publications=1001;reads=$result.reads;errors=$result.errors;jobEmpty=$true;hooksStarted=$false;gamesStarted=$false}|ConvertTo-Json|Set-Content "$out/result.json"
Get-Content "$out/result.json"
