param([Parameter(Mandatory)][string]$Go)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$primary='E:/Source/game-modding/dbce-mods-daytona-usa'
if(git -C $root status --porcelain --untracked-files=no){throw 'Clean tracked source required'}
$source=(git -C $root rev-parse HEAD).Trim()
$held=$false
try {
 & E:/Source/dbce-project-mgmt/tools/Rig-Lease.ps1 -Enter -Name remi-daytona-cpu -Purpose "GO$Go Daytona private configure/build + inert fixtures only" -IdleMinutes 0
 if(-not $?){throw 'CPU lease refused'};$held=$true
 foreach($part in @('extern','build/gen')){
  $dest=Join-Path $root $part
  if(-not (Test-Path $dest)){
   [void](New-Item -ItemType Directory $dest)
   & robocopy (Join-Path $primary $part) $dest /E /XD .git /XF .git /NFL /NDL /NJH /NJS /NP
   if($LASTEXITCODE -ge 8){throw "Private dependency/cache copy failed: $part"}
  }
 }
 # Retained ROM-derived C++ is private build input; record identities, never commit it.
 $inputs=@(Get-ChildItem "$root/build/gen" -File -Recurse|ForEach-Object{@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash $_.FullName).Hash}})
 $inputs|ConvertTo-Json|Set-Content "$root/build/readback-generated-inputs.json"
 & cmake -S $root -B "$root/build" -G 'Visual Studio 17 2022' -T ClangCL -DM2_ROMSET=daytona93
 if($LASTEXITCODE){throw 'Configure failed'}
 & cmake --build "$root/build" --config Release --target daytona test_readback --parallel 2
 if($LASTEXITCODE){throw 'Build failed'}
 Push-Location "$root/build"
 try{& ./Release/test_readback.exe;if($LASTEXITCODE){throw 'Fake-SDL readback fixture failed'}}finally{Pop-Location}
 & "$PSScriptRoot/Rehearse.ps1" -Output "$root/build/readback/rehearsal-go$Go"
 if(-not $?){throw 'Rehearsal failed'}
 if((git -C $root rev-parse HEAD).Trim() -cne $source -or (git -C $root status --porcelain --untracked-files=no)){throw 'Source changed during build'}
 @{source=$source;go=$Go;candidate=(Get-FileHash "$root/build/Release/daytona.exe").Hash;generatedInputs=(Get-FileHash "$root/build/readback-generated-inputs.json").Hash;fakeSdl='pass';readerJobRehearsal='pass';runtime='unrun'}|ConvertTo-Json|Set-Content "$root/build/readback-build.json"
 Get-Content "$root/build/readback-build.json"
}finally{if($held){& E:/Source/dbce-project-mgmt/tools/Rig-Lease.ps1 -Exit -Name remi-daytona-cpu}}
