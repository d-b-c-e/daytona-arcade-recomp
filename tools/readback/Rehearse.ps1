param([Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Fresh rehearsal output required'}
$out=(New-Item -ItemType Directory $Output).FullName
$python='C:/Users/antho/AppData/Local/Programs/Python/Python314/python.exe'
$job='E:/Source/_work/soba-truckful-provider-free/artifacts/provider-free/rehearsal-native-go501049-amend501065/native/Truckful.Live.dll'
if((Get-FileHash $job).Hash -cne '725E52B00A36B8189BF4BAF3CF201D1266614B49DB124F2C410884D2D96BE4DD'){throw 'Job helper pin differs'}
Add-Type -Path $job;Add-Type -Path "$PSScriptRoot/Snapshot.cs"
. "$PSScriptRoot/JobPolicy.ps1"
$file=Join-Path $out 'state.json'
function Publish($n){@{n=$n}|ConvertTo-Json -Compress|Set-Content "$file.tmp" -Encoding utf8NoBOM;[DaytonaSnapshot]::Publish("$file.tmp",$file)}
Publish 0
$argsRun=@('-B',"$PSScriptRoot/test_shared.py",'--watch',$file,"$out/reader.json")
$identity=[TruckfulLive]::Launch($python,$out,(($argsRun|ForEach-Object{'"'+$_+'"'})-join ' '))
$identity|ConvertTo-Json|Set-Content "$out/root.json"
$plan=@{python=$python;candidate=(Join-Path $out 'unused-game.exe');capture=(Join-Path $out 'unused-capture.exe');consoleHost=(Join-Path ([Environment]::GetFolderPath('System')) 'conhost.exe')}
$seen=@{}
function CheckCensus {
 foreach($childId in @([TruckfulLive]::JobPids())){
  try{$i=[TruckfulLive]::ProcessIdentity($childId)}catch{if(Get-Process -Id $childId -ErrorAction SilentlyContinue){throw};continue}
  $role=Get-OwnedJobRole $i $identity $plan
  $key="$($i.pid):$($i.birth)"
  if(-not $seen.ContainsKey($key)){
   $wmi=Get-CimInstance Win32_Process -Filter "ProcessId=$($i.pid)" -ErrorAction Stop
   $seen[$key]=@{pid=$i.pid;birth=$i.birth;image=$i.image;role=$role;parent=$wmi.ParentProcessId;commandLine=$wmi.CommandLine}
   ConvertTo-Json -InputObject @($seen.Values) -Depth 5|Set-Content "$out/job-identities.json"
  }
  if($role -notin @('root','console-host')){throw "Unadmitted inert job identity: $($i|ConvertTo-Json -Compress)"}
 }
}
$initial=@([TruckfulLive]::JobPids())
if($initial -notcontains $identity.pid){throw 'Suspended root not in own job'}
CheckCensus
foreach($n in 1..1000){Publish $n;if($n%20 -eq 0){CheckCensus}}
$clock=[Diagnostics.Stopwatch]::StartNew()
while(@([TruckfulLive]::JobPids()).Count -and $clock.Elapsed.TotalSeconds -lt 10){CheckCensus;Start-Sleep -Milliseconds 20}
if(@([TruckfulLive]::JobPids()).Count){Write-Output 'CUSTODY_RETAINED: rehearsal child did not exit';while($true){Start-Sleep -Seconds 1}}
[TruckfulLive]::ReleaseJob()
# Exercise the production image/epoch classifier with inert identities.
$cases=@(
 @(@{pid=$identity.pid;birth=$identity.birth;image=$python},'root'),
 @(@{pid=1;birth=$identity.birth+1;image=$plan.consoleHost},'console-host'),
 @(@{pid=1;birth=$identity.birth-1;image=$plan.consoleHost},'unknown-epoch'),
 @(@{pid=1;birth=$identity.birth+1;image=(Join-Path $out 'conhost.exe')},'unknown-image'),
 @(@{pid=$identity.pid+1;birth=$identity.birth+1;image=$python},'unknown-python'),
 @(@{pid=1;birth=$identity.birth+1;image=$plan.capture},'capture')
)
foreach($case in $cases){if((Get-OwnedJobRole $case[0] $identity $plan) -ne $case[1]){throw 'Production job classifier assertion failed'}}
$result=Get-Content "$out/reader.json" -Raw|ConvertFrom-Json
if($result.errors.Count -or $result.reads -lt 1){throw 'Shared-delete stress failed'}
& $python -B "$PSScriptRoot/test_shared.py"
if($LASTEXITCODE){throw 'Production admission/owner fixtures failed'}
@{publications=1001;reads=$result.reads;errors=$result.errors;jobPolicyCases=$cases.Count;jobIdentities=@($seen.Values);jobEmpty=$true;hooksStarted=$false;gamesStarted=$false}|ConvertTo-Json -Depth 6|Set-Content "$out/result.json"
Get-Content "$out/result.json"
