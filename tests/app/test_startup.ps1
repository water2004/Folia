[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$ReportDirectory = (Join-Path $PSScriptRoot '../../build/tests/startup'),
    [ValidateRange(1, 5)][int]$Runs = 1
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$report = (Resolve-Path -LiteralPath $ReportDirectory).Path

# Exercise the real GUI entry point and merged application manifest. Controller
# tests do not construct MainWindow or register native AppWindow events.
# The debugger owns only the fresh, argument-free test child. It catches startup
# exceptions before WER dialogs and requests a normal close of that child's window.
Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
public sealed class FoliaStartupResult {
 public uint ProcessId,ExitCode;
 public bool Ready,CloseRequested,Success;
 public string Fault,Log;
}
public static class FoliaStartupTest {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct StartupInfo {
  public uint cb; public string reserved,desktop,title;
  public uint x,y,width,height,charsX,charsY,fill,flags;
  public ushort show,reservedSize; public IntPtr reservedData,input,output,error;
 }
 [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr process,thread; public uint pid,tid; }
 [StructLayout(LayoutKind.Explicit,Size=176)] struct DebugEvent {
  [FieldOffset(0)] public uint code; [FieldOffset(4)] public uint pid; [FieldOffset(8)] public uint tid;
  [FieldOffset(16)] public uint exception; [FieldOffset(32)] public IntPtr address;
  [FieldOffset(168)] public uint firstChance;
  [FieldOffset(16)] public IntPtr file;
  [FieldOffset(16)] public uint exitCode;
  [FieldOffset(16)] public IntPtr text;
  [FieldOffset(24)] public ushort unicode;
  [FieldOffset(26)] public ushort length;
 }
 delegate bool WindowCallback(IntPtr window,IntPtr data);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessW(string app,StringBuilder command,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr env,string cwd,ref StartupInfo startup,out ProcessInfo process);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool WaitForDebugEvent(out DebugEvent value,uint milliseconds);
 [DllImport("kernel32.dll")] static extern bool ContinueDebugEvent(uint pid,uint tid,uint status);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process,uint status);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr read);
 [DllImport("user32.dll")] static extern bool EnumWindows(WindowCallback callback,IntPtr data);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
 [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr window,uint command);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowTextLengthW(IntPtr window);
 [DllImport("user32.dll",SetLastError=true)] static extern bool PostMessageW(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("dbghelp.dll",SetLastError=true)] static extern bool MiniDumpWriteDump(IntPtr process,uint pid,IntPtr file,uint type,IntPtr exception,IntPtr streams,IntPtr callback);
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenThread(uint access,bool inherit,uint tid);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetThreadContext(IntPtr thread,IntPtr context);
 [DllImport("dbghelp.dll",CharSet=CharSet.Ansi,SetLastError=true)] static extern bool SymInitialize(IntPtr process,string path,bool invade);
 [DllImport("dbghelp.dll")] static extern bool SymCleanup(IntPtr process);
 [DllImport("dbghelp.dll")] static extern ulong SymGetModuleBase64(IntPtr process,ulong address);
 [DllImport("dbghelp.dll")] static extern IntPtr SymFunctionTableAccess64(IntPtr process,ulong address);
 [DllImport("dbghelp.dll",SetLastError=true)] static extern bool SymFromAddr(IntPtr process,ulong address,out ulong displacement,IntPtr symbol);
 delegate IntPtr FunctionTable(IntPtr process,ulong address);
 delegate ulong ModuleBase(IntPtr process,ulong address);
 [DllImport("dbghelp.dll",SetLastError=true)] static extern bool StackWalk64(uint machine,IntPtr process,IntPtr thread,IntPtr frame,IntPtr context,IntPtr read,FunctionTable table,ModuleBase module,IntPtr translate);

 static void Stack(ProcessInfo child,uint threadId,string symbols,StringBuilder log) {
  var thread=OpenThread(0x48,false,threadId);
  var context=Marshal.AllocHGlobal(1232); var frame=Marshal.AllocHGlobal(1024); var symbol=Marshal.AllocHGlobal(4096);
  bool initialized=false;
  try {
   for(int i=0;i<1232;i++) Marshal.WriteByte(context,i,0);
   for(int i=0;i<1024;i++) Marshal.WriteByte(frame,i,0);
   Marshal.WriteInt32(context,48,0x10000b);
   if(thread==IntPtr.Zero || !GetThreadContext(thread,context)) { log.AppendLine("thread context unavailable"); return; }
   initialized=SymInitialize(child.process,symbols,true);
   if(!initialized) { log.AppendLine("symbols unavailable: "+Marshal.GetLastWin32Error()); return; }
   Marshal.WriteInt64(frame,0,Marshal.ReadInt64(context,248)); Marshal.WriteInt32(frame,12,3);
   Marshal.WriteInt64(frame,32,Marshal.ReadInt64(context,160)); Marshal.WriteInt32(frame,44,3);
   Marshal.WriteInt64(frame,48,Marshal.ReadInt64(context,152)); Marshal.WriteInt32(frame,60,3);
   for(int depth=0;depth<40;depth++) {
    ulong pc=(ulong)Marshal.ReadInt64(frame,0);
    if(pc==0) break;
    for(int i=0;i<88;i++) Marshal.WriteByte(symbol,i,0);
    Marshal.WriteInt32(symbol,0,88); Marshal.WriteInt32(symbol,80,4000);
    ulong delta; string name=SymFromAddr(child.process,pc,out delta,symbol) ? Marshal.PtrToStringAnsi(IntPtr.Add(symbol,84)) : "?";
    ulong module=SymGetModuleBase64(child.process,pc);
    log.AppendLine("frame "+depth+" pc=0x"+pc.ToString("x")+" module=0x"+module.ToString("x")+" rva=0x"+(pc-module).ToString("x")+" "+name);
    if(!StackWalk64(0x8664,child.process,thread,frame,context,IntPtr.Zero,SymFunctionTableAccess64,SymGetModuleBase64,IntPtr.Zero)) break;
   }
  } finally {
   if(initialized) SymCleanup(child.process);
   if(thread!=IntPtr.Zero) CloseHandle(thread);
   Marshal.FreeHGlobal(context); Marshal.FreeHGlobal(frame); Marshal.FreeHGlobal(symbol);
  }
 }

 static IntPtr OwnWindow(uint process) {
  IntPtr found=IntPtr.Zero;
  EnumWindows(delegate(IntPtr window,IntPtr data) {
   uint pid; GetWindowThreadProcessId(window,out pid);
   if(pid==process && GetWindow(window,4)==IntPtr.Zero && GetWindowTextLengthW(window)>0) { found=window; return false; }
   return true;
  },IntPtr.Zero);
  return found;
 }
 static void Dump(ProcessInfo child,string path) {
  using(var file=new FileStream(path,FileMode.Create,FileAccess.Write,FileShare.Read))
   MiniDumpWriteDump(child.process,child.pid,file.SafeFileHandle.DangerousGetHandle(),0x1000,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);
 }
 public static FoliaStartupResult Run(string exe,string directory) {
  var result=new FoliaStartupResult(); var log=new StringBuilder();
  var startup=new StartupInfo(); startup.cb=(uint)Marshal.SizeOf(typeof(StartupInfo)); startup.flags=1; startup.show=0;
  ProcessInfo child;
  if(!CreateProcessW(exe,new StringBuilder("\""+exe+"\" --new-instance"),IntPtr.Zero,IntPtr.Zero,false,2,IntPtr.Zero,Path.GetDirectoryName(exe),ref startup,out child))
   throw new Exception("CreateProcess: "+Marshal.GetLastWin32Error());
  result.ProcessId=child.pid;
  var deadline=DateTime.UtcNow.AddSeconds(20); DateTime readyAt=DateTime.MaxValue; bool exited=false;
  try {
   while(DateTime.UtcNow<deadline) {
    if(result.Ready && !result.CloseRequested && DateTime.UtcNow>=readyAt) {
     var window=OwnWindow(child.pid);
     if(window!=IntPtr.Zero) {
      result.CloseRequested=PostMessageW(window,0x10,IntPtr.Zero,IntPtr.Zero);
      log.AppendLine("normal close requested for owned child "+child.pid);
     }
    }
    DebugEvent ev;
    if(!WaitForDebugEvent(out ev,100)) continue;
    uint status=0x00010002;
    if((ev.code==3 || ev.code==6) && ev.file!=IntPtr.Zero) CloseHandle(ev.file);
    if(ev.code==8) {
     int bytes=Math.Min(4096,ev.length*(ev.unicode!=0 ? 2 : 1));
     var buffer=new byte[bytes]; UIntPtr read;
     if(bytes>0 && ReadProcessMemory(child.process,ev.text,buffer,(UIntPtr)(uint)bytes,out read)) {
      string text=(ev.unicode!=0 ? Encoding.Unicode : Encoding.Default).GetString(buffer);
      if(text.Contains("Folia.StartupComplete")) {
       result.Ready=true; readyAt=DateTime.UtcNow.AddSeconds(1);
       log.AppendLine("MainWindow constructed and activated");
      }
     }
    }
    if(ev.code==1) {
     status=ev.exception==0x80000003 ? 0x00010002u : 0x80010001u;
     if(ev.firstChance==0) {
      long imageBase=0;
      try { imageBase=Process.GetProcessById((int)child.pid).MainModule.BaseAddress.ToInt64(); } catch {}
      result.Fault="0x"+ev.exception.ToString("x")+" at RVA 0x"+(ev.address.ToInt64()-imageBase).ToString("x");
      log.AppendLine(result.Fault);
      Stack(child,ev.tid,Path.GetDirectoryName(exe),log);
      Dump(child,Path.Combine(directory,"startup.dmp"));
      TerminateProcess(child.process,0xeeef);
      ContinueDebugEvent(ev.pid,ev.tid,0x00010002);
      break;
     }
    }
    if(ev.code==5) { exited=true; result.ExitCode=ev.exitCode; log.AppendLine("exit="+ev.exitCode); }
    ContinueDebugEvent(ev.pid,ev.tid,status);
    if(exited) break;
   }
   if(!exited && result.Fault==null) { result.Fault="startup/normal-close timeout"; Dump(child,Path.Combine(directory,"startup.dmp")); }
   result.Success=exited && result.ExitCode==0 && result.Ready && result.CloseRequested && result.Fault==null;
  } finally {
   if(!exited) TerminateProcess(child.process,0xeeee);
   CloseHandle(child.thread); CloseHandle(child.process);
   result.Log=log.ToString(); File.WriteAllText(Path.Combine(directory,"startup.log"),result.Log);
  }
  return result;
 }
}
'@

for ($run = 1; $run -le $Runs; ++$run) {
    $directory = Join-Path $report "run-$run"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $result = [FoliaStartupTest]::Run($exe, $directory)
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'result.json') -Encoding utf8
    Write-Output $result.Log
    if (-not $result.Success) { throw "Folia startup/close test failed: $($result.Fault). See $directory" }
}
