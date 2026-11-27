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
$oldLocal=$env:LOCALAPPDATA
$profileRoot=Join-Path $run 'profile'
$assets=Join-Path $profileRoot 'Folia/Assets'
New-Item -ItemType Directory -Path (Split-Path -Parent $assets) -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $testScriptDirectory '../../src/app-winui/Assets') -Destination $assets -Recurse
$workingAssets=New-Item -ItemType Junction -Path (Join-Path $run 'Assets') -Target $assets
$env:LOCALAPPDATA=$profileRoot
$links=@()
foreach($entry in Get-ChildItem -LiteralPath (Split-Path -Parent $exe)) {
 $path=Join-Path $candidate $entry.Name
 if($entry.Name -eq 'Assets') { New-Item -ItemType Junction -Path $path -Target $assets | Out-Null }
 elseif($entry.PSIsContainer) { New-Item -ItemType Junction -Path $path -Target $entry.FullName | Out-Null }
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
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int command);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
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
function Redirect([string]$argument) {
 $secondary=Start-Owned $argument
 Wait-For { $secondary.Refresh(); $secondary.HasExited } 'redirecting launcher exit'
 $secondary.WaitForExit()
 if($secondary.ExitCode -ne 0) { throw "Redirect exit: $($secondary.ExitCode)" }
}
function Window-ForFile($process,[string]$name) {
 $found=@([FoliaActivationWindows]::Windows($process.Id) | Where-Object { [FoliaActivationWindows]::Title($_).Contains($name) })
 if($found.Count) { return $found[0] }
}
function Close-Owned($process) {
 [FoliaActivationWindows]::Close($process.Id)
 Wait-For { $process.Refresh(); $process.HasExited } 'normal window close'
 $process.WaitForExit()
 if($process.ExitCode -ne 0) { throw "Window cleanup exit code: $($process.ExitCode)" }
}
try {
 $names=@('中文 空格.md','relative.md','third.md')
 $paths=@($names | ForEach-Object { Join-Path $run $_ })
 foreach($path in $paths) { [IO.File]::WriteAllText($path,"# Activation test`n",[Text.UTF8Encoding]::new($false)) }
 $primary=Start-Owned ('"'+$paths[0]+'"')
 Wait-For { Window-ForFile $primary $names[0] } 'cold launch with Unicode file'
 $first=Window-ForFile $primary $names[0]
 Redirect $names[1]
 Wait-For { [FoliaActivationWindows]::Title($first).Contains($names[1]) } 'default tab reuse'
 if([FoliaActivationWindows]::Windows($primary.Id).Count -ne 1) { throw 'File opened an extra window' }
 Redirect ''
 Wait-For { [FoliaActivationWindows]::Windows($primary.Id).Count -eq 2 } 'second workspace window'
 Redirect $names[2]
 Wait-For { Window-ForFile $primary $names[2] } 'new file in most recently active window'
 $second=Window-ForFile $primary $names[2]
 if($first -eq $second) { throw 'New file opened in the older window' }
 [void][FoliaActivationWindows]::ShowWindow($first,6)
 Wait-For { [FoliaActivationWindows]::IsIconic($first) } 'older window minimized'
 Redirect ('"'+$paths[0]+'"')
 Wait-For { -not [FoliaActivationWindows]::IsIconic($first) -and [FoliaActivationWindows]::Title($first).Contains($names[0]) } 'existing tab restored from minimize'
 Wait-For { [FoliaActivationWindows]::GetForegroundWindow() -eq $first } 'redirected file window in foreground'
 if([FoliaActivationWindows]::Windows($primary.Id).Count -ne 2) { throw 'Already open file was duplicated' }
 $isolated=Start-Owned '--new-instance'
 Wait-For { -not $isolated.HasExited -and [FoliaActivationWindows]::Windows($isolated.Id).Count -eq 1 } 'explicit separate process'
 if([FoliaActivationWindows]::Windows($primary.Id).Count -ne 2) { throw 'An isolated launch modified the registered process' }
 Close-Owned $isolated
 Close-Owned $primary

 $config=@{schemaVersion=6;themeId='system';languageId='system';shortcutBindings=@();completion=@{};openFilesInNewWindow=$true}
 [IO.File]::WriteAllText((Join-Path $assets 'settings.json'),($config | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
 $primary=Start-Owned ('"'+$paths[0]+'"')
 Wait-For { Window-ForFile $primary $names[0] } 'new window preference on cold launch'
 if([FoliaActivationWindows]::Windows($primary.Id).Count -ne 1) { throw 'Extra empty window on cold launch' }
 Redirect ($names[1]+' '+$names[2])
 Wait-For { [FoliaActivationWindows]::Windows($primary.Id).Count -eq 3 -and (Window-ForFile $primary $names[1]) -and (Window-ForFile $primary $names[2]) } 'multiple files in their own windows'
 Redirect ('"'+$paths[0]+'"')
 Wait-For { [FoliaActivationWindows]::GetForegroundWindow() -eq (Window-ForFile $primary $names[0]) } 'reuse already-open file in new window mode'
 if([FoliaActivationWindows]::Windows($primary.Id).Count -ne 3) { throw 'File was duplicated in new window mode' }
 Close-Owned $primary
 @{success=$true;launchers=@($owned | ForEach-Object { @{pid=$_.Id;exitCode=$_.ExitCode} })} |
  ConvertTo-Json -Depth 4 | Set-Content (Join-Path $run 'result.json') -Encoding utf8
 Write-Output "Activation passed: Unicode/relative paths, both opening modes, foreground restore, multiple windows, existing files and isolated instance. Report: $run"
} finally {
 $env:LOCALAPPDATA=$oldLocal
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
 $workingAssets.Delete()
}
