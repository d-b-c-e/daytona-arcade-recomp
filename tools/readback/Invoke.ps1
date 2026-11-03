param([Parameter(Mandatory)][ValidatePattern('^attempt-[0-9]{2}$')][string]$Attempt,
 [Parameter(Mandatory)][string]$Go,[Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$PreparedSha256)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$out=Join-Path $root "build/readback/$Attempt"
function Pin([string]$Path,[string]$Hash){if((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Hash){throw "Pin mismatch: $Path"}}
Pin "$out/prepared.json" $PreparedSha256
$plan=Get-Content "$out/prepared.json" -Raw|ConvertFrom-Json
if($plan.attemptRoot -ine $out){throw 'Prepared attempt path differs'}
foreach($pin in $plan.pins){Pin $pin.path $pin.sha256}
if(Test-Path "$out/live"){throw 'Never overwrite live evidence'}
$live=(New-Item -ItemType Directory "$out/live").FullName
$stateFile=Join-Path $live 'supervisor.json'
$lease=$false;$observer=$false;$cleanup=$false;$launched=$false;$adopted=$false;$quiescent=$true;$restored=$false;$failure=$null
$identity=$null;$display=$null;$oldMute=$env:DBCE_FFB_MUTE
function State([bool]$Ok){
 @{unix=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0;ok=$Ok;pending=($observer -and [Dirt4Readback.Observer]::Pending);cleanup=$cleanup}|ConvertTo-Json -Compress|Set-Content "$stateFile.tmp" -Encoding utf8NoBOM
 [DaytonaSnapshot]::Publish("$stateFile.tmp",$stateFile)
}
function VerifyOwner([string]$Before,[string]$After){
 & $plan.python -B "$PSScriptRoot/owner.py" verify "$out/owner-roots.json" $After $Before
 if($LASTEXITCODE){throw 'Independent owner verifier refused'}
}
function CloseOwnedGame {
 if(-not $identity){return}
 try{$now=[TruckfulLive]::ProcessIdentity($identity.pid)}catch{if(Get-Process -Id $identity.pid -ErrorAction SilentlyContinue){throw};return}
 if($now.birth -ne $identity.birth -or $now.image -ine $identity.image){throw 'Owned game identity became ambiguous'}
 $p=Get-Process -Id $identity.pid
 if($p.MainWindowHandle -ne 0){[void][TruckfulLive]::PostMessage($p.MainWindowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
}
try {
 Add-Type -Path $plan.job;Add-Type -Path $plan.observer;Add-Type -Path "$PSScriptRoot/Snapshot.cs"
 if([Dirt4Readback.Observer]::RawIdleSeconds -lt 180){throw 'Raw idle below 180 seconds'}
 & E:/Source/dbce-project-mgmt/tools/Rig-Lease.ps1 -Enter -Name remi-daytona-live -Purpose "GO$Go Daytona 1800-frame neutral attract R1/R2" -IdleMinutes 3
 if(-not $?){throw 'Lease refused'};$lease=$true
 $display=[TruckfulLive]::DisplayIdentity();$display|Set-Content "$live/display-before.txt"
 VerifyOwner "$out/owner-preflight.json" "$live/owner-before.json"
 if([Dirt4Readback.Observer]::RawIdleSeconds -lt 180){throw 'Owner activity during preflight'}
 [Dirt4Readback.Observer]::Start($live);$observer=$true
 [Dirt4Readback.Observer]::AssertHealthy()
 [Dirt4Readback.Observer]::ExpectGame($plan.candidate,[DateTime]::UtcNow)
 State $true
 $env:DBCE_FFB_MUTE='1'
 try {
  $all=@('-B',"$PSScriptRoot/worker.py","$out/prepared.json",$live)
  foreach($a in $all){if($a.Contains('"')){throw 'Child argument contains quote'}}
  $rootIdentity=[TruckfulLive]::Launch($plan.python,$root,(($all|ForEach-Object{'"'+$_+'"'}) -join ' '))
  $launched=$true;$quiescent=$false
 }finally{$env:DBCE_FFB_MUTE=$oldMute}
 $rootIdentity|ConvertTo-Json|Set-Content "$live/root.json"
 $clock=[Diagnostics.Stopwatch]::StartNew()
 while(@([TruckfulLive]::JobPids()).Count){
  if((Test-Path "$live/supervisor.closing") -and -not $cleanup){[Dirt4Readback.Observer]::BeginCleanup();$cleanup=$true}
  if(-not $cleanup){[Dirt4Readback.Observer]::AssertHealthy()}
  if([TruckfulLive]::DisplayIdentity() -cne $display){throw 'Display topology/mode drift'}
  $ids=@([TruckfulLive]::JobPids())
  foreach($childId in $ids){
   try{$i=[TruckfulLive]::ProcessIdentity($childId)}catch{if(Get-Process -Id $childId -ErrorAction SilentlyContinue){throw};continue}
   if($i.image -ieq $plan.candidate){
    if(-not $adopted){$identity=$i;[Dirt4Readback.Observer]::Adopt($i.pid,$i.birth);$adopted=$true;$i|ConvertTo-Json|Set-Content "$live/game.json"}
    elseif($i.pid -ne $identity.pid -or $i.birth -ne $identity.birth){throw 'Game identity replaced or duplicated'}
   }elseif($i.image -ine $plan.python -and $i.image -ine $plan.capture){throw "Unexpected owned-job image: $($i.image)"}
  }
  State $true
  if($clock.Elapsed.TotalSeconds -gt 190){throw 'Own-job total deadline exceeded'}
  Start-Sleep -Milliseconds 100
 }
 $quiescent=$true
 if(-not $adopted){throw 'No exact game adopted'}
 $receipt=Get-Content "$live/worker-result.json" -Raw|ConvertFrom-Json
 if($receipt.result -ne 'pass'){throw "R1/R2 worker failed: $($receipt.failure)"}
}catch{$failure=$_.Exception.Message}
finally {
 $env:DBCE_FFB_MUTE=$oldMute
 if($lease){
  State $false
  if($observer){[Dirt4Readback.Observer]::BeginCleanup();$cleanup=$true;State $false}
  if($launched -and -not $quiescent){
   try{CloseOwnedGame}catch{$failure="${failure}; close: $($_.Exception.Message)"}
   $closing=[Diagnostics.Stopwatch]::StartNew()
   while(@([TruckfulLive]::JobPids()).Count -and $closing.Elapsed.TotalSeconds -lt 45){Start-Sleep -Milliseconds 100}
   $quiescent=@([TruckfulLive]::JobPids()).Count -eq 0
  }
  if($quiescent){
   try{
    VerifyOwner "$live/owner-before.json" "$live/owner-after.json"
    if([TruckfulLive]::DisplayIdentity() -cne $display){throw 'Final display identity drift'}
    $restored=$true
   }catch{$failure="${failure}; verification: $($_.Exception.Message)"}
  }
  if($observer){try{if(-not [Dirt4Readback.Observer]::Stop()){throw 'LL observer did not join'};$observer=$false}catch{$failure="${failure}; observer: $($_.Exception.Message)"}}
  if($quiescent -and $restored -and -not $observer){[TruckfulLive]::ReleaseJob();& E:/Source/dbce-project-mgmt/tools/Rig-Lease.ps1 -Exit -Name remi-daytona-live;$lease=$false}
  @{go=$Go;source=$plan.source;failure=$failure;quiescent=$quiescent;ownerVerified=$restored;leaseHeld=$lease;observerHeld=$observer;game=$identity}|ConvertTo-Json -Depth 5|Set-Content "$live/result.json"
  if($lease){Write-Output 'CUSTODY_RETAINED: controller/job/lease held; explicit resolution required';while($true){Start-Sleep -Seconds 1}}
 }
}
if($failure){throw $failure}
Get-Content "$live/result.json"
