[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Executable,
      [string]$ReportDirectory)
$ErrorActionPreference='Stop'
$testScriptDirectory=Split-Path -Parent $MyInvocation.MyCommand.Path
if(-not $ReportDirectory) { $ReportDirectory=Join-Path $testScriptDirectory '../../build/tests/activation' }
$exe=(Resolve-Path -LiteralPath $Executable).Path
$run=Join-Path ([IO.Path]::GetFullPath($ReportDirectory)) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run -Force | Out-Null
# Keep the executable/PRI names intact: WinUI's resource map uses the application
# name. A dedicated directory gives this run an independent AppInstance key.
$candidate=Join-Path $run 'candidate'
New-Item -ItemType Directory -Path $candidate | Out-Null
$links=@()
foreach($entry in Get-ChildItem -LiteralPath (Split-Path -Parent $exe)) {
 $path=Join-Path $candidate $entry.Name
 if($entry.PSIsContainer) { New-Item -ItemType Junction -Path $path -Target $entry.FullName | Out-Null }
 else { New-Item -ItemType HardLink -Path $path -Target $entry.FullName | Out-Null }
 $links+=$path
}
$alias=Join-Path $candidate ([IO.Path]::GetFileName($exe))
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class FoliaActivationWindows {
 delegate bool Callback(IntPtr h,IntPtr data);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback,IntPtr data);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr h,StringBuilder text,int size);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h,StringBuilder text,int size);
 [DllImport("user32.dll")] static extern bool PostMessageW(IntPtr h,uint message,IntPtr w,IntPtr l);
 public static IntPtr[] Windows(uint process) {
  var result=new List<IntPtr>();
  EnumWindows(delegate(IntPtr h,IntPtr data) {
   uint pid; GetWindowThreadProcessId(h,out pid);
   if(pid==process) { var name=new StringBuilder(256); GetClassNameW(h,name,256);
    if(name.ToString()=="WinUIDesktopWin32WindowClass") result.Add(h); }
   return true;
  },IntPtr.Zero);
  return result.ToArray();
 }
 public static string Title(IntPtr h) { var text=new StringBuilder(32768); GetWindowTextW(h,text,text.Capacity); return text.ToString(); }
 public static void Close(uint process) { foreach(var h in Windows(process)) PostMessageW(h,0x10,IntPtr.Zero,IntPtr.Zero); }
}
'@
function Wait-For([scriptblock]$Condition,[string]$Description) {
 $until=[DateTime]::UtcNow.AddSeconds(20)
 while(-not (& $Condition)) {
  if([DateTime]::UtcNow -gt $until) { throw "Timed out: $Description" }
  Start-Sleep -Milliseconds 50
 }
}
$owned=[Collections.Generic.List[Diagnostics.Process]]::new()
function Start-Owned([string]$Arguments='') {
 $parameters=@{FilePath=$alias;WorkingDirectory=$run;WindowStyle='Hidden';PassThru=$true}
 if($Arguments) { $parameters.ArgumentList=$Arguments }
 $process=Start-Process @parameters
 $null=$process.Handle
 $owned.Add($process)
 return $process
}
try {
 $primary=Start-Owned
 Wait-For { -not $primary.HasExited -and [FoliaActivationWindows]::Windows($primary.Id).Count -eq 1 } 'first isolated window'
 $paths=@((Join-Path $run '中文 空格.md'),(Join-Path $run 'relative.md'))
 foreach($path in $paths) { [IO.File]::WriteAllText($path,"# Activation test`n`nUnchanged document.`n",[Text.UTF8Encoding]::new($false)) }
 $expected=1
 foreach($argument in @(('"'+$paths[0]+'"'),'relative.md','')) {
  $secondary=Start-Owned $argument
  Wait-For { $secondary.Refresh(); $secondary.HasExited } 'redirecting launcher exit'
  $secondary.WaitForExit()
  if($secondary.ExitCode -ne 0) { throw "Redirect exit: $($secondary.ExitCode)" }
  ++$expected
  Wait-For { [FoliaActivationWindows]::Windows($primary.Id).Count -eq $expected } 'new window in original process'
 }
 foreach($path in $paths) {
  $name=[IO.Path]::GetFileName($path)
  Wait-For { @([FoliaActivationWindows]::Windows($primary.Id) | Where-Object { [FoliaActivationWindows]::Title($_).Contains($name) }).Count -gt 0 } "opened $name"
 }
 $isolated=Start-Owned '--new-instance'
 Wait-For { -not $isolated.HasExited -and [FoliaActivationWindows]::Windows($isolated.Id).Count -eq 1 } 'explicit separate process'
 if([FoliaActivationWindows]::Windows($primary.Id).Count -ne 4) { throw 'An isolated launch modified the registered process' }
 [FoliaActivationWindows]::Close($isolated.Id)
 Wait-For { $isolated.Refresh(); $isolated.HasExited } 'isolated window normal close'
 $isolated.WaitForExit()
 [FoliaActivationWindows]::Close($primary.Id)
 Wait-For { $primary.Refresh(); $primary.HasExited } 'all original windows normal close'
 $primary.WaitForExit()
 if($primary.ExitCode -ne 0 -or $isolated.ExitCode -ne 0) { throw "Window cleanup exit codes: primary=$($primary.ExitCode), isolated=$($isolated.ExitCode)" }
 @{success=$true;primaryPid=$primary.Id;windows=4;launchers=@($owned | ForEach-Object { @{pid=$_.Id;exitCode=$_.ExitCode} })} |
  ConvertTo-Json -Depth 4 | Set-Content (Join-Path $run 'result.json') -Encoding utf8
 Write-Output "Activation integration passed: Unicode/relative paths, four shared windows, isolated instance, normal close. Report: $run"
} finally {
 @{documents=$paths;processes=@($owned | ForEach-Object {
  $_.Refresh()
  @{pid=$_.Id;exited=$_.HasExited;exitCode=$(if($_.HasExited){$_.WaitForExit();$_.ExitCode});titles=@([FoliaActivationWindows]::Windows($_.Id) | ForEach-Object { [FoliaActivationWindows]::Title($_) })}
 })} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'diagnostics.json') -Encoding utf8
 foreach($process in $owned) {
  $process.Refresh()
  if(-not $process.HasExited) {
   [FoliaActivationWindows]::Close($process.Id)
   if(-not $process.WaitForExit(2000)) { $process.Kill(); $process.WaitForExit() }
  }
  $process.Dispose()
 }
 foreach($path in $links) {
  if([IO.Path]::GetFullPath((Split-Path -Parent $path)) -ne [IO.Path]::GetFullPath($candidate)) { throw 'Unexpected candidate cleanup path' }
  if([IO.Directory]::Exists($path)) { [IO.Directory]::Delete($path) }
  else { Remove-Item -LiteralPath $path -Force }
 }
 [IO.Directory]::Delete($candidate)
}
