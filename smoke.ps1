# Brief integration smoke check, using only PowerShell and Windows APIs.
# Run after build.ps1. It starts hidden copies in build\smoke and does not
# touch user documents or associations. Clipboard copy is checked and restored.
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class MdSmoke {
    public delegate bool Each(IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(Each f, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr w, Each f, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w, out uint p);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr w, int id);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr w);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr w);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr w);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr w, StringBuilder b, int n);
    [DllImport("user32.dll",EntryPoint="SendMessageW")] public static extern IntPtr Send(IntPtr w, uint m, IntPtr p, IntPtr l);
    [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)] public static extern IntPtr Text(IntPtr w, uint m, IntPtr p, string text);
    [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)] public static extern IntPtr Read(IntPtr w, uint m, IntPtr p, StringBuilder text);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w, uint m, IntPtr p, IntPtr l);
    [DllImport("user32.dll")] public static extern long GetWindowLongPtrW(IntPtr w, int i);
    [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr w);
    [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr w, int n);
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr w);
    [DllImport("user32.dll")] public static extern uint GetMenuState(IntPtr w, uint id, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr w);
    [DllImport("user32.dll")] public static extern bool AreDpiAwarenessContextsEqual(IntPtr a, IntPtr b);
    [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr w);
    [DllImport("user32.dll")] public static extern int GetSystemMetricsForDpi(int metric, uint dpi);
    [DllImport("shell32.dll",CharSet=CharSet.Unicode)] public static extern uint ExtractIconEx(string path,int index,out IntPtr large,out IntPtr small,uint count);
    [DllImport("user32.dll")] public static extern bool DestroyIcon(IntPtr icon);
    [StructLayout(LayoutKind.Sequential)] public struct IconInfo { public bool icon; public uint x,y; public IntPtr mask,color; }
    [StructLayout(LayoutKind.Sequential)] public struct BitmapInfo { public int type,width,height,stride; public ushort planes,bits; public IntPtr data; }
    [DllImport("user32.dll")] public static extern bool GetIconInfo(IntPtr icon,out IconInfo info);
    [DllImport("gdi32.dll",EntryPoint="GetObjectW")] public static extern int GetBitmap(IntPtr bitmap,int size,out BitmapInfo info);
    [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr obj);
    public static int[] IconSize(IntPtr icon) {
        IconInfo info;if(icon==IntPtr.Zero||!GetIconInfo(icon,out info))return new[]{0,0};
        try {BitmapInfo b;return GetBitmap(info.color,Marshal.SizeOf(typeof(BitmapInfo)),out b)==0?new[]{0,0}:new[]{b.width,b.height};}
        finally {if(info.mask!=IntPtr.Zero)DeleteObject(info.mask);if(info.color!=IntPtr.Zero)DeleteObject(info.color);}
    }
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
    public static uint SystemDpi() {var old=SetThreadDpiAwarenessContext((IntPtr)(-4));try{return GetDpiForSystem();}finally{SetThreadDpiAwarenessContext(old);}}
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int x,y; }
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w, ref Point p);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w, out Rect r);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr w, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll",EntryPoint="SendMessageW")] public static extern IntPtr Rectangle(IntPtr w,uint m,IntPtr p,ref Rect r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr w,IntPtr dc,uint flags);
    [DllImport("user32.dll")] public static extern bool OpenClipboard(IntPtr w);
    [DllImport("user32.dll")] public static extern bool CloseClipboard();
    [DllImport("user32.dll")] public static extern IntPtr GetClipboardData(uint f);
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalLock(IntPtr h);
    [DllImport("kernel32.dll")] public static extern bool GlobalUnlock(IntPtr h);
    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll")] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr s, uint t, uint prot);
    [DllImport("kernel32.dll")] public static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr s, uint t);
    [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, UIntPtr s, out UIntPtr read);
    [DllImport("kernel32.dll")] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, UIntPtr s, out UIntPtr written);
    public static string Class(IntPtr w) { var b=new StringBuilder(256); GetClassName(w,b,256); return b.ToString(); }
    public static IntPtr Find(uint pid, string cls) { IntPtr found=IntPtr.Zero; EnumWindows((w,l)=> {uint p;GetWindowThreadProcessId(w,out p);if(p==pid && Class(w)==cls){found=w;return false;}return true;},IntPtr.Zero);return found; }
    public static IntPtr Edit(IntPtr parent) {IntPtr found=IntPtr.Zero;EnumChildWindows(parent,(w,l)=>{int id=GetDlgCtrlID(w);if(Class(w)=="Edit" && (id==1001||id==1148||id==1152)){found=w;return false;}return true;},IntPtr.Zero);return found;}
    public static string Content(IntPtr w) {var b=new StringBuilder(Send(w,14,IntPtr.Zero,IntPtr.Zero).ToInt32()+1);Read(w,13,(IntPtr)b.Capacity,b);return b.ToString();}
    public static string Raw(uint pid,IntPtr w) {
        var p=OpenProcess(0x38,false,pid);var a=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)8,0x3000,4);
        try {
            UIntPtr n;var length=new byte[8];BitConverter.GetBytes(10).CopyTo(length,0);BitConverter.GetBytes(1200).CopyTo(length,4);
            WriteProcessMemory(p,a,length,(UIntPtr)8,out n);int count=Send(w,1119,a,IntPtr.Zero).ToInt32();
            VirtualFreeEx(p,a,UIntPtr.Zero,0x8000);a=IntPtr.Zero;
            var bytes=new byte[32+(count+1)*2];a=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)bytes.Length,0x3000,4);
            BitConverter.GetBytes((count+1)*2).CopyTo(bytes,0);BitConverter.GetBytes(4).CopyTo(bytes,4);BitConverter.GetBytes(1200).CopyTo(bytes,8);
            WriteProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n);int copied=Send(w,1118,a,IntPtr.Add(a,32)).ToInt32();
            ReadProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n);return Encoding.Unicode.GetString(bytes,32,copied*2);
        } finally {if(a!=IntPtr.Zero)VirtualFreeEx(p,a,UIntPtr.Zero,0x8000);CloseHandle(p);}
    }
    public static byte[] Table(uint pid,IntPtr w,int position) {
        var bytes=new byte[28+63*40];bytes[0]=28;bytes[1]=40;bytes[2]=63;bytes[3]=1;BitConverter.GetBytes(position).CopyTo(bytes,20);
        var p=OpenProcess(0x38,false,pid);var a=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)bytes.Length,0x3000,4);
        try {UIntPtr n;WriteProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n);
            if(Send(w,1289,a,IntPtr.Add(a,28))!=IntPtr.Zero)throw new Exception("EM_GETTABLEPARMS");
            ReadProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n);return bytes;
        }finally{VirtualFreeEx(p,a,UIntPtr.Zero,0x8000);CloseHandle(p);}
    }
    public static string Clipboard() {if(!OpenClipboard(IntPtr.Zero))return null;try{var h=GetClipboardData(13);var p=GlobalLock(h);try{return p==IntPtr.Zero?null:Marshal.PtrToStringUni(p);}finally{if(p!=IntPtr.Zero)GlobalUnlock(h);}}finally{CloseClipboard();}}
    public static byte[] Remote(uint pid,IntPtr w,uint msg,IntPtr wp,byte[] bytes,bool inWParam=false) {var p=OpenProcess(0x38,false,pid);if(p==IntPtr.Zero)throw new Exception("OpenProcess");var a=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)bytes.Length,0x3000,4);try{UIntPtr n;if(!WriteProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n))throw new Exception("WriteProcessMemory");Send(w,msg,inWParam?a:wp,inWParam?wp:a);if(!ReadProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n))throw new Exception("ReadProcessMemory");return bytes;}finally{VirtualFreeEx(p,a,UIntPtr.Zero,0x8000);CloseHandle(p);}}
    public static int[] Zoom(uint pid,IntPtr w) {var p=OpenProcess(0x38,false,pid);var a=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)8,0x3000,4);try{Send(w,1248,a,IntPtr.Add(a,4));var b=new byte[8];UIntPtr n;ReadProcessMemory(p,a,b,(UIntPtr)8,out n);return new[]{BitConverter.ToInt32(b,0),BitConverter.ToInt32(b,4)};}finally{VirtualFreeEx(p,a,UIntPtr.Zero,0x8000);CloseHandle(p);}}
}
'@

function Assert($condition, [string]$description) {
    if (-not $condition) { throw "FAIL: $description" }
    Write-Output "PASS: $description"
}
function Wait-Window($process, [string]$class = 'NativeMarkdownEditorWindow') {
    for ($attempt = 0; $attempt -lt 100; ++$attempt) {
        $found = [MdSmoke]::Find($process.Id, $class)
        if ($found -ne [IntPtr]::Zero) { return $found }
        if ($process.HasExited) { throw "Application exited: $($process.ExitCode)" }
        Start-Sleep -Milliseconds 50
    }
    throw "Window did not appear: $class"
}
function Command($w, [int]$id) { [void][MdSmoke]::Send($w, 273, [IntPtr]$id, [IntPtr]::Zero) }
function Post-Command($w, [int]$id) { [void][MdSmoke]::PostMessage($w, 273, [IntPtr]$id, [IntPtr]::Zero) }
function Dismiss-Error($dialog) {
    # EnumWindows can see a MessageBox before its buttons are initialized.
    for ($n=0; $n -lt 100; ++$n) {
        $button = [MdSmoke]::GetDlgItem($dialog,1)
        if ($button -eq [IntPtr]::Zero) { $button = [MdSmoke]::GetDlgItem($dialog,2) }
        if ($button -ne [IntPtr]::Zero -and [MdSmoke]::IsWindowEnabled($button)) { break }
        Start-Sleep -Milliseconds 20
    }
    [void][MdSmoke]::PostMessage($button,245,[IntPtr]::Zero,[IntPtr]::Zero) # BM_CLICK
    for ($n=0; $n -lt 100 -and [MdSmoke]::IsWindow($dialog); ++$n) { Start-Sleep -Milliseconds 20 }
    if ([MdSmoke]::IsWindow($dialog)) { throw 'Error dialog did not close' }
}
function Dialog-Path($process, [string]$path) {
    $dialog = Wait-Window $process '#32770'
    [void]$process.WaitForInputIdle(3000)
    for ($n = 0; $n -lt 100; ++$n) {
        $edit = [MdSmoke]::Edit($dialog)
        $button = [MdSmoke]::GetDlgItem($dialog, 1)
        if ($edit -ne [IntPtr]::Zero -and $button -ne [IntPtr]::Zero -and [MdSmoke]::IsWindowEnabled($button)) { break }
        Start-Sleep -Milliseconds 50
    }
    if ($edit -eq [IntPtr]::Zero) { throw 'File name edit not found' }
    # Shell folder initialization can clear an edit that was already enabled.
    # Wait for the supplied filename to survive an idle interval before OK.
    for ($retry=0; $retry -lt 5; ++$retry) {
        [void][MdSmoke]::Send($dialog, 40, $edit, [IntPtr]1) # WM_NEXTDLGCTL sets focus
        [void][MdSmoke]::Send($edit, 177, [IntPtr]::Zero, [IntPtr](-1))
        [void][MdSmoke]::Text($edit, 194, [IntPtr]1, $path)
        Start-Sleep -Milliseconds 200
        if ([MdSmoke]::Content($edit) -eq $path) { break }
    }
    if ([MdSmoke]::Content($edit) -ne $path) { throw 'File dialog did not accept the test path' }
    [void][MdSmoke]::PostMessage($button, 245, [IntPtr]::Zero, [IntPtr]::Zero) # BM_CLICK
    for ($n = 0; $n -lt 100 -and [MdSmoke]::Find($process.Id, '#32770') -ne [IntPtr]::Zero; ++$n) { Start-Sleep -Milliseconds 50 }
    if ([MdSmoke]::Find($process.Id, '#32770') -ne [IntPtr]::Zero) {
        throw "File dialog did not close: $([MdSmoke]::Content($dialog)); filename: $([MdSmoke]::Content($edit))"
    }
    Start-Sleep -Milliseconds 200
}
function Read-Font($process, $control) {
    $bytes = [byte[]]::new(116)
    [BitConverter]::GetBytes(116).CopyTo($bytes, 0)
    $bytes = [MdSmoke]::Remote($process.Id, $control, 1082, [IntPtr]1, $bytes)
    return [Text.Encoding]::Unicode.GetString($bytes, 26, 64).TrimEnd([char]0)
}
function Read-Color($process, $control, [string]$needle, [int]$offset = 20) {
    $index = [MdSmoke]::Raw($process.Id,$control).IndexOf($needle)
    if ($index -lt 0) { throw "Missing color sample: $needle" }
    [void][MdSmoke]::Send($control, 177, [IntPtr]$index, [IntPtr]($index + 1))
    $bytes = [byte[]]::new(116)
    [BitConverter]::GetBytes(116).CopyTo($bytes, 0)
    $bytes = [MdSmoke]::Remote($process.Id, $control, 1082, [IntPtr]1, $bytes)
    return [BitConverter]::ToUInt32($bytes, $offset)
}
function Control-State($process, $control) {
    $selection = [MdSmoke]::Remote($process.Id, $control, 1076, [IntPtr]::Zero, [byte[]]::new(8))
    $scroll = [MdSmoke]::Remote($process.Id, $control, 1245, [IntPtr]::Zero, [byte[]]::new(8))
    return [Convert]::ToBase64String($selection + $scroll)
}
function Check-Background($control, [int]$expected, [string]$name) {
    # EM_SETBKGNDCOLOR returns the previous color. Reapply the expected color
    # to inspect it even when desktop capture is unavailable in a hidden session.
    $actual = [MdSmoke]::Send($control,1091,[IntPtr]::Zero,[IntPtr]$expected).ToInt64()
    Assert ($actual -eq $expected) "$name document background"
}
function Check-Icons($w, [int]$dpi) {
    foreach ($type in @(0,1)) {
        $icon = [MdSmoke]::Send($w,127,[IntPtr]$type,[IntPtr]::Zero) # WM_GETICON
        $size = [MdSmoke]::IconSize($icon)
        $widthMetric = if ($type -eq 0) { 49 } else { 11 } # SM_CXSMICON / SM_CXICON
        $width = [MdSmoke]::GetSystemMetricsForDpi($widthMetric,$dpi)
        $height = [MdSmoke]::GetSystemMetricsForDpi($widthMetric + 1,$dpi)
        Assert ($size[0] -eq $width -and $size[1] -eq $height) "window icon type $type has native ${width}x${height} pixels at $dpi DPI"
    }
}
function Read-Selection($process, $control) {
    $bytes = [MdSmoke]::Remote($process.Id,$control,1076,[IntPtr]::Zero,[byte[]]::new(8))
    return @([BitConverter]::ToInt32($bytes,0), [BitConverter]::ToInt32($bytes,4))
}
function Check-Viewport($process, $w, $control, [int]$dpi, [string]$label) {
    $client = [MdSmoke+Rect]::new()
    $bounds = [MdSmoke+Rect]::new()
    $parent = [MdSmoke+Rect]::new()
    $origin = [MdSmoke+Point]::new()
    $oldContext = [MdSmoke]::SetThreadDpiAwarenessContext([IntPtr](-4))
    try {
        [void][MdSmoke]::GetClientRect($control,[ref]$client)
        [void][MdSmoke]::GetClientRect($w,[ref]$parent)
        [void][MdSmoke]::GetWindowRect($control,[ref]$bounds)
        [void][MdSmoke]::ClientToScreen($w,[ref]$origin)
    } finally { [void][MdSmoke]::SetThreadDpiAwarenessContext($oldContext) }
    $format = [MdSmoke+Rect]::new()
    [void][MdSmoke]::Rectangle($control,178,[IntPtr]::Zero,[ref]$format)
    $padding = [int][Math]::Round(14 * $dpi / 96)
    Assert ($bounds.top - $origin.y -eq $padding -and $origin.y + $parent.bottom - $bounds.bottom -eq $padding) "$label keeps vertical padding outside the native viewport"
    Assert ($format.top -eq 0 -and $format.bottom -eq $client.bottom) "$label native text area fills the viewport height (top=$($format.top), bottom=$($format.bottom), client=$($client.bottom))"
}
function Char-Point($process, $control, [int]$index) {
    [void][MdSmoke]::Send($control,177,[IntPtr]$index,[IntPtr]$index)
    [void][MdSmoke]::Send($control,183,[IntPtr]::Zero,[IntPtr]::Zero) # EM_SCROLLCARET
    $bytes = [MdSmoke]::Remote($process.Id,$control,1062,[IntPtr]$index,[byte[]]::new(8),$true)
    return @([BitConverter]::ToInt32($bytes,0), [BitConverter]::ToInt32($bytes,4))
}
function Mouse-At($control, [int]$message, $point, [int]$keys = 1) {
    $location = (($point[1] -band 65535) -shl 16) -bor ($point[0] -band 65535)
    # Coordinates from EM_POSFROMCHAR are device pixels. Match the target's
    # PMv2 context so User32 does not virtualize synthetic mouse coordinates.
    $oldContext = [MdSmoke]::SetThreadDpiAwarenessContext([IntPtr](-4))
    try { [void][MdSmoke]::Send($control,$message,[IntPtr]$keys,[IntPtr]$location) }
    finally { [void][MdSmoke]::SetThreadDpiAwarenessContext($oldContext) }
}
function Navigate($process, $w, [string]$renderNeedle, [string]$sourceNeedle, [int]$occurrence = 0, [string]$label = '', [int]$sourceOccurrence = 0, [int]$renderOffset = 0, [int]$sourceOffset = 0) {
    Command $w 201
    $source = [MdSmoke]::GetDlgItem($w,300)
    $view = [MdSmoke]::GetDlgItem($w,301)
    $text = [MdSmoke]::Raw($process.Id,$view)
    $index = -1
    for ($i = 0; $i -le $occurrence; ++$i) { $index = $text.IndexOf($renderNeedle,$index + 1) }
    if ($index -lt 0) { throw "Missing navigation sample: $renderNeedle" }
    $index += $renderOffset
    $raw = [MdSmoke]::Content($source).Replace("`r`n","`n").Replace("`r","`n")
    $expected = -1
    for ($i = 0; $i -le $sourceOccurrence; ++$i) { $expected = $raw.IndexOf($sourceNeedle,$expected + 1) }
    if ($expected -lt 0) { throw "Missing source sample: $sourceNeedle" }
    $expected += $sourceOffset
    $before = [MdSmoke]::Content($source)
    $title = [MdSmoke]::Content($w)
    $undo = [MdSmoke]::Send($source,198,[IntPtr]::Zero,[IntPtr]::Zero)
    $redo = [MdSmoke]::Send($source,1109,[IntPtr]::Zero,[IntPtr]::Zero)
    $modified = [MdSmoke]::Send($source,184,[IntPtr]::Zero,[IntPtr]::Zero)
    $point = Char-Point $process $view $index
    # Use the character's left edge so proportional fonts and surrogate pairs
    # cannot move this hit onto the following character.
    Mouse-At $view 515 @($point[0],($point[1] + 2))
    $selection = Read-Selection $process $source
    Assert (([MdSmoke]::GetWindowLongPtrW($source,-16) -band 0x10000000) -ne 0 -and $selection[0] -eq $expected -and $selection[1] -eq $expected) "double-click maps $renderNeedle + $renderOffset to source offset $expected $label (actual $($selection -join ','))"
    $caret = [MdSmoke]::Remote($process.Id,$source,1062,[IntPtr]$expected,[byte[]]::new(8),$true)
    $rect = [MdSmoke+Rect]::new()
    $oldContext = [MdSmoke]::SetThreadDpiAwarenessContext([IntPtr](-4))
    try { [void][MdSmoke]::GetClientRect($source,[ref]$rect) }
    finally { [void][MdSmoke]::SetThreadDpiAwarenessContext($oldContext) }
    $x = [BitConverter]::ToInt32($caret,0)
    $y = [BitConverter]::ToInt32($caret,4)
    Assert ($x -ge 0 -and $x -lt $rect.right -and $y -ge 0 -and $y -lt $rect.bottom) 'navigation scrolls the collapsed source caret into the viewport'
    Assert ([MdSmoke]::Content($source) -eq $before -and [MdSmoke]::Content($w) -eq $title -and [MdSmoke]::Send($source,198,[IntPtr]::Zero,[IntPtr]::Zero) -eq $undo -and [MdSmoke]::Send($source,1109,[IntPtr]::Zero,[IntPtr]::Zero) -eq $redo -and [MdSmoke]::Send($source,184,[IntPtr]::Zero,[IntPtr]::Zero) -eq $modified) 'navigation preserves source, dirty state, and undo/redo availability'
}
function Check-Palette($process, $w, $colors, [string]$label) {
    $source = [MdSmoke]::GetDlgItem($w,300)
    $view = [MdSmoke]::GetDlgItem($w,301)
    Assert ((Read-Color $process $source 'Heading') -eq $colors[1] -and (Read-Color $process $view 'Heading') -eq $colors[1]) "$label source and rendered text"
    Assert ((Read-Color $process $view 'link') -eq $colors[2] -and (Read-Color $process $view 'https://') -eq $colors[2]) "$label links"
    Assert ((Read-Color $process $view 'Quote sample') -eq $colors[3] -and (Read-Color $process $view '────') -eq $colors[3]) "$label quotes and rules"
    Assert ((Read-Color $process $view 'code sample' 96) -eq $colors[4] -and (Read-Color $process $view 'block sample' 96) -eq $colors[4]) "$label code backgrounds"
    Check-Background $view $colors[0] "$label View"
    Check-Background $source $colors[0] "$label Edit"
    $row = Read-Table $process $view 'Grid header'
    Assert ([BitConverter]::ToUInt32($row,44) -eq $colors[5]) "$label independent table border"
}
function Read-Table($process, $control, [string]$needle) {
    $text = [MdSmoke]::Raw($process.Id,$control)
    $index = $text.IndexOf($needle)
    if ($index -lt 0) { throw "Missing table sample: $needle" }
    $position = $text.LastIndexOf([char]0xfff9,$index)
    if ($position -lt 0) { throw "Missing native row start: $needle" }
    return [MdSmoke]::Table($process.Id,$control,$position)
}
$testDirectory = Join-Path $PSScriptRoot 'build\smoke'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
$ini = Join-Path $testDirectory 'markdown-editor.ini'
if (Test-Path -LiteralPath $ini) { Remove-Item -LiteralPath $ini }
$exe = Join-Path $testDirectory 'markdown-editor.exe'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\markdown-editor.exe') -Destination $exe -Force
$testPath = Join-Path $testDirectory 'Grüße 日本語 with spaces.md'
$markdown = "# Heading`r`n`r`n**bold** and *emphasis* &copy; 😀`r`n`r`n- one`r`n- two`r`n`r`n---`r`n`r`n[link](https://example.com)`r`n`r`n![alt](image.png)`r`n`r`n<div>visible</div>`r`n`r`n> Quote sample`r`n`r`n``code sample```r`n`r`n    block sample`r`n"
$markdown += "`r`n| Grid header | Amount |`r`n| :--- | ---: |`r`n| **Table body** 😀 | 42 |`r`n"
[IO.File]::WriteAllText($testPath, $markdown, [Text.UTF8Encoding]::new($true))
$processes = [Collections.Generic.List[Diagnostics.Process]]::new()
$clipboardBackup = [MdSmoke]::Clipboard()
try {
    $largeIcon = $smallIcon = [IntPtr]::Zero
    try {
        $iconCount = [MdSmoke]::ExtractIconEx($exe,0,[ref]$largeIcon,[ref]$smallIcon,1)
        Assert ($iconCount -gt 0 -and ([MdSmoke]::IconSize($largeIcon))[0] -gt 0 -and ([MdSmoke]::IconSize($smallIcon))[0] -gt 0) 'Windows shell extracts the embedded application icon in both sizes'
    } finally {
        if ($largeIcon -ne [IntPtr]::Zero) { [void][MdSmoke]::DestroyIcon($largeIcon) }
        if ($smallIcon -ne [IntPtr]::Zero) { [void][MdSmoke]::DestroyIcon($smallIcon) }
    }
    $process = Start-Process -FilePath $exe -ArgumentList "`"$testPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($process)
    $w = Wait-Window $process
    # WinMain opens the command-line document just after showing the window.
    for ($n = 0; $n -lt 100 -and [MdSmoke]::Content($w) -notlike '*with spaces.md*'; ++$n) { Start-Sleep -Milliseconds 50 }
    Check-Icons $w ([MdSmoke]::GetDpiForWindow($w))
    $source = [MdSmoke]::GetDlgItem($w, 300)
    $view = [MdSmoke]::GetDlgItem($w, 301)
    Assert ([MdSmoke]::Content($source).Contains('**bold**')) 'quoted Unicode command-line file preserves Markdown source'
    $rendered = [MdSmoke]::Content($view)
    Assert ($rendered.Contains('Heading') -and $rendered.Contains('bold and emphasis © 😀') -and -not $rendered.Contains('**bold**')) 'headings, spans, entities and Unicode render'
    Assert ($rendered.Contains('[Image: alt — image.png]') -and $rendered.Contains('<div>visible</div>')) 'images and HTML retain readable fallback content'
    Assert ($rendered.Contains('https://example.com')) 'rendered link includes its destination'
    Assert ($rendered.Contains('────────────────────────')) 'horizontal rule remains visible in rendered text'
    $bar = [MdSmoke]::GetMenu($w)
    $mode = [MdSmoke]::GetSubMenu($bar, 1)
    Assert (([MdSmoke]::GetMenuItemCount($bar) -eq 2) -and ([MdSmoke]::GetMenuItemCount([MdSmoke]::GetSubMenu($bar, 0)) -eq 3) -and ([MdSmoke]::GetMenuItemCount($mode) -eq 4) -and ([MdSmoke]::GetMenuState($mode, 2, 1024) -band 2048)) 'six menu actions with a separator before Dark theme'
    Assert (([MdSmoke]::GetMenuState($mode, 201, 0) -band 8) -ne 0) 'existing file starts in View mode'
    Assert (([MdSmoke]::GetMenuState($mode, 203, 0) -band 8) -eq 0) 'missing theme setting defaults to Light'
    $populated = [IO.File]::ReadAllText($ini)
    $defaults = @('[theme.light]','background=#F5F3EE','text=#242424','link=#005BAE','muted=#62605C','code=#E8E5DE','table_border=#C1BFB9',
        '[theme.dark]','background=#000000','text=#D0D0D0','link=#79B8FF','muted=#A0A0A0','code=#181818','table_border=#484848')
    Assert (($defaults | Where-Object { -not $populated.Contains($_) }).Count -eq 0) 'first launch automatically populates all twelve palette defaults'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    foreach ($dark in @($false,$true,$false,$true,$false)) {
        $checked = ([MdSmoke]::GetMenuState($mode,203,0) -band 8) -ne 0
        if ($checked -ne $dark) { Command $w 203 }
        Assert (([MdSmoke]::GetWindowLongPtrW($source,-16) -band 0x10000000) -eq 0 -and ([MdSmoke]::GetWindowLongPtrW($view,-16) -band 0x10000000) -ne 0) 'theme keeps only View visible in View mode'
        $colors = if ($dark) { @(0xD0D0D0,0xFFB879,0xA0A0A0,0x181818) } else { @(0x242424,0xAE5B00,0x5C6062,0xDEE5E8) }
        Assert ((Read-Color $process $source 'Heading') -eq $colors[0]) "source text palette (dark=$dark)"
        Assert ((Read-Color $process $view 'Heading') -eq $colors[0] -and (Read-Color $process $view 'bold') -eq $colors[0]) "heading and body palette (dark=$dark)"
        Assert ((Read-Color $process $view 'link') -eq $colors[1] -and (Read-Color $process $view 'https://') -eq $colors[1]) "link label and detected URL palette (dark=$dark)"
        Assert ((Read-Color $process $view 'Quote sample') -eq $colors[2] -and (Read-Color $process $view '────') -eq $colors[2]) "quote and rule palette (dark=$dark)"
        Assert ((Read-Color $process $view 'code sample' 96) -eq $colors[3] -and (Read-Color $process $view 'block sample' 96) -eq $colors[3]) "inline and block code background (dark=$dark)"
        $border = if ($dark) { 0x484848 } else { 0xB9BFC1 }
        $row = Read-Table $process $view 'Grid header'
        Assert ([BitConverter]::ToUInt32($row,44) -eq $border -and [BitConverter]::ToInt16($row,36) -eq 10) "native half-point table border (dark=$dark)"
    }
    $timer.Stop()
    Write-Output "Ordinary document palette checks and four switches: $($timer.ElapsedMilliseconds) ms"
    Assert (-not [MdSmoke]::Content($w).StartsWith('*') -and [MdSmoke]::Content($source).Contains('**bold**')) 'theme changes preserve clean source and title'
    foreach ($dark in @($true,$false)) {
        Command $w 203
        $background = if ($dark) { 0 } else { 0xEEF3F5 }
        Check-Background $view $background "view-$dark"
        Command $w 200
        Check-Background $source $background "source-$dark"
        Command $w 201
    }
    Assert ([MdSmoke]::AreDpiAwarenessContextsEqual([MdSmoke]::GetWindowDpiAwarenessContext($w), [IntPtr](-4))) 'window is Per-Monitor V2 aware'
    [void][MdSmoke]::Send($view, 177, [IntPtr]::Zero, [IntPtr](-1))
    [void][MdSmoke]::Send($view, 769, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert ([MdSmoke]::Clipboard().Contains('bold and emphasis © 😀')) 'rendered selection copies Unicode text to clipboard'
    [void][MdSmoke]::Send($view, 258, [IntPtr]88, [IntPtr]::Zero)
    Assert ([MdSmoke]::Content($view) -eq $rendered) 'View rejects text insertion'
    Command $w 200
    [void][MdSmoke]::Send($source, 177, [IntPtr](-1), [IntPtr](-1))
    [void][MdSmoke]::Text($source, 194, [IntPtr]1, "`r`nAdded source Ω")
    Assert ([MdSmoke]::Content($w).StartsWith('*')) 'editing marks the title unsaved'
    $sourceBefore = [MdSmoke]::Content($source)
    $viewBefore = [MdSmoke]::Content($view)
    $positionBefore = Control-State $process $source
    Command $w 203
    Assert ((Control-State $process $source) -eq $positionBefore -and [MdSmoke]::Content($source) -eq $sourceBefore -and [MdSmoke]::Content($w).StartsWith('*')) 'theme preserves unsaved text, source selection and scrolling'
    Assert ([MdSmoke]::Content($view) -eq $viewBefore) 'Edit theme change defers hidden preview rendering'
    # Queue a key message so the application's accelerator table handles it.
    [void][MdSmoke]::PostMessage($source,256,[IntPtr]27,[IntPtr]1) # WM_KEYDOWN, VK_ESCAPE
    for ($n=0; $n -lt 100 -and ([MdSmoke]::GetMenuState($mode,201,0) -band 8) -eq 0; ++$n) { Start-Sleep -Milliseconds 20 }
    Assert (([MdSmoke]::GetMenuState($mode,201,0) -band 8) -ne 0 -and ([MdSmoke]::GetWindowLongPtrW($source,-16) -band 0x10000000) -eq 0 -and ([MdSmoke]::GetWindowLongPtrW($view,-16) -band 0x10000000) -ne 0) 'Esc leaves Edit for View'
    Assert ([MdSmoke]::Content($view).Contains('Added source Ω')) 'Esc rerenders the edited preview'
    Assert ([MdSmoke]::Content($source) -eq $sourceBefore -and [MdSmoke]::Content($w).StartsWith('*')) 'Esc preserves unsaved source and dirty state'
    Command $w 200
    Assert ((Control-State $process $source) -eq $positionBefore) 'returning to Edit after Esc preserves source selection and scrolling'
    [void][MdSmoke]::Send($source, 199, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert (-not [MdSmoke]::Content($source).Contains('Added source Ω')) 'undo history survives mode switches'
    Command $w 203
    Command $w 203
    [void][MdSmoke]::Send($source, 1108, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert ([MdSmoke]::Content($source).Contains('Added source Ω')) 'redo restores edited source'
    $beforeClipboard = [MdSmoke]::Content($source)
    [void][MdSmoke]::Send($source,177,[IntPtr]::Zero,[IntPtr]5)
    [void][MdSmoke]::Send($source,768,[IntPtr]::Zero,[IntPtr]::Zero)
    Command $w 203
    [void][MdSmoke]::Send($source,1088,[IntPtr]13,[IntPtr]::Zero)
    Assert ([MdSmoke]::Content($source) -eq $beforeClipboard) 'cut and plain-text paste survive a theme switch'
    Command $w 201
    [void][MdSmoke]::Send($view,177,[IntPtr]4,[IntPtr]24)
    $positionBefore = Control-State $process $view
    Command $w 203
    Assert ((Control-State $process $view) -eq $positionBefore) 'View theme rebuild preserves selection and scrolling'
    [void][MdSmoke]::Send($view,769,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert (-not [string]::IsNullOrEmpty([MdSmoke]::Clipboard())) 'clipboard copy works after theme rebuild'
    Command $w 200
    Command $w 101
    $saved = [IO.File]::ReadAllBytes($testPath)
    Assert ($saved[0] -eq 239 -and $saved[1] -eq 187 -and $saved[2] -eq 191 -and [IO.File]::ReadAllText($testPath).Contains("`r`nAdded source Ω")) 'save preserves UTF-8 BOM and CRLF source'
    Assert (-not [MdSmoke]::Content($w).StartsWith('*')) 'successful save clears unsaved title'
    [void][MdSmoke]::Text($source, 194, [IntPtr]1, ' unsaved')
    Post-Command $w 102
    $dialog = Wait-Window $process '#32770'
    [void][MdSmoke]::PostMessage($dialog, 1126, [IntPtr]2, [IntPtr]::Zero) # TDM_CLICK_BUTTON, Cancel
    Start-Sleep -Milliseconds 100
    Assert (-not $process.HasExited -and [MdSmoke]::Content($source).Contains(' unsaved')) 'Cancel on close preserves unsaved work'
    Post-Command $w 102
    $dialog = Wait-Window $process '#32770'
    [void][MdSmoke]::PostMessage($dialog, 1126, [IntPtr]7, [IntPtr]::Zero) # Discard
    Assert ($process.WaitForExit(3000)) 'Discard closes without saving'

    $blank = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
    $processes.Add($blank)
    $w = Wait-Window $blank
    $source = [MdSmoke]::GetDlgItem($w, 300)
    Assert ([MdSmoke]::Content($w).StartsWith('Untitled') -and (([MdSmoke]::GetWindowLongPtrW($source,-16) -band 0x10000000) -ne 0)) 'empty launch creates an untitled editor'
    [void][MdSmoke]::Text($source, 194, [IntPtr]1, "# New UTF-8`r`nλ")
    Post-Command $w 101
    $newPath = Join-Path $testDirectory "new document without extension $PID"
    Dialog-Path $blank $newPath
    for ($n=0; $n -lt 100 -and -not [IO.File]::Exists($newPath + '.md'); ++$n) { Start-Sleep -Milliseconds 50 }
    Assert ([IO.File]::Exists($newPath + '.md')) 'untitled Save dialog supplies .md extension'
    Assert ([IO.File]::ReadAllText($newPath + '.md').Contains('λ')) 'new file saves Unicode source'
    Post-Command $w 100
    Dialog-Path $blank $testPath
    for ($n=0; $n -lt 100 -and -not [MdSmoke]::Content($w).Contains('with spaces.md'); ++$n) { Start-Sleep -Milliseconds 50 }
    Assert ([MdSmoke]::Content($w).Contains('with spaces.md')) 'Open dialog replaces the document'
    Command $w 200
    [void][MdSmoke]::Send($source,177,[IntPtr](-1),[IntPtr](-1))
    [void][MdSmoke]::Text($source,194,[IntPtr]1,' must survive a failed save')
    $beforeFailure = [IO.File]::ReadAllBytes($testPath)
    $lockedFile = [IO.File]::Open($testPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        Post-Command $w 101
        $dialog = Wait-Window $blank '#32770'
        Assert ([MdSmoke]::Content($source).Contains('must survive a failed save') -and [MdSmoke]::Content($w).StartsWith('*')) 'failed save preserves unsaved source and title'
        Dismiss-Error $dialog
    } finally { $lockedFile.Dispose() }
    Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes($testPath)) -eq [Convert]::ToBase64String($beforeFailure)) 'failed save leaves the original file intact'
    # Allow filesystem filters to release the deliberately locked file. Keep
    # this save asynchronous so an unexpected error cannot hang the harness.
    Start-Sleep -Milliseconds 100
    for ($retry=0; $retry -lt 5 -and [MdSmoke]::Content($w).StartsWith('*'); ++$retry) {
        Post-Command $w 101
        for ($n=0; $n -lt 100 -and [MdSmoke]::Content($w).StartsWith('*'); ++$n) {
            $dialog = [MdSmoke]::Find($blank.Id,'#32770')
            if ($dialog -ne [IntPtr]::Zero) { Dismiss-Error $dialog; break }
            Start-Sleep -Milliseconds 20
        }
        if ([MdSmoke]::Content($w).StartsWith('*')) { Start-Sleep -Milliseconds 200 }
    }
    Assert (-not [MdSmoke]::Content($w).StartsWith('*')) 'save succeeds after the test releases the file lock'
    Command $w 201
    $view = [MdSmoke]::GetDlgItem($w,301)
    $rect = [MdSmoke+Rect]::new()
    [void][MdSmoke]::GetWindowRect($w,[ref]$rect)
    $systemDpi = [MdSmoke]::SystemDpi()
    foreach ($testDpi in @(96,144,192,$systemDpi)) {
        [void][MdSmoke]::Rectangle($w,736,[IntPtr](($testDpi -shl 16) -bor $testDpi),[ref]$rect)
        Check-Icons $w $testDpi
        Command $w 203
        Command $w 203
        $zoom = [MdSmoke]::Zoom($blank.Id,$view)
        $ratio = if ($zoom[0] -eq 0 -and $zoom[1] -eq 0) { 1.0 } elseif ($zoom[1] -gt 0) { $zoom[0] / $zoom[1] } else { -1 }
        Assert ([Math]::Abs($ratio - $testDpi / [double]$systemDpi) -lt 0.01) "DPI handler applies $testDpi DPI text scaling (zoom $($zoom[0])/$($zoom[1]))"
    }
    Add-Type -AssemblyName System.Drawing.Common
    [void][MdSmoke]::GetWindowRect($w,[ref]$rect)
    $bitmap = [Drawing.Bitmap]::new($rect.right - $rect.left,$rect.bottom - $rect.top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $dc = $graphics.GetHdc()
    try { [void][MdSmoke]::PrintWindow($w,$dc,2) } finally { $graphics.ReleaseHdc($dc); $graphics.Dispose() }
    $bitmap.Save((Join-Path $testDirectory 'preview.png'),[Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)

    $ini = Join-Path $testDirectory 'markdown-editor.ini'
    [IO.File]::WriteAllText($ini, "; Keep this comment`r`n[font]`r`npath=C:\Windows\Fonts\arial.ttf`r`n[appearance]`r`ntheme=invalid`r`n", [Text.Encoding]::Unicode)
    $fontApp = Start-Process -FilePath $exe -ArgumentList "`"$testPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($fontApp)
    $w = Wait-Window $fontApp
    Start-Sleep -Milliseconds 300
    Assert ((Read-Font $fontApp ([MdSmoke]::GetDlgItem($w,300))) -eq 'Arial') 'configured TTF applies to source text'
    $renderedFont = Read-Font $fontApp ([MdSmoke]::GetDlgItem($w,301))
    Assert ($renderedFont -eq 'Arial') "configured TTF applies to rendered text ($renderedFont)"
    Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),203,0) -band 8) -eq 0) 'invalid theme setting defaults to Light'
    Command $w 203
    Assert ((Read-Font $fontApp ([MdSmoke]::GetDlgItem($w,300))) -eq 'Arial' -and (Read-Font $fontApp ([MdSmoke]::GetDlgItem($w,301))) -eq 'Arial') 'dynamic theme preserves custom fonts in both controls'
    $settings = [IO.File]::ReadAllText($ini)
    Assert ($settings.Contains('theme=dark') -and $settings.Contains('path=C:\Windows\Fonts\arial.ttf') -and $settings.Contains('; Keep this comment')) 'theme saves immediately and preserves UTF-16 INI font settings and comments'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    [IO.File]::WriteAllText($ini, $settings.Replace('C:\Windows\Fonts\arial.ttf','missing.ttf'), [Text.Encoding]::Unicode)
    $fallback = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
    $processes.Add($fallback)
    $w = Wait-Window $fallback
    Assert ((Read-Font $fallback ([MdSmoke]::GetDlgItem($w,300))) -eq 'Consolas') 'missing TTF falls back gracefully'
    Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),203,0) -band 8) -ne 0 -and (Read-Color $fallback ([MdSmoke]::GetDlgItem($w,300)) '' ) -eq 0xD0D0D0) 'saved Dark theme applies at startup'
    $lockedSettings = [IO.File]::Open($ini,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        Post-Command $w 203
        $dialog = Wait-Window $fallback '#32770'
        Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),203,0) -band 8) -eq 0 -and (Read-Color $fallback ([MdSmoke]::GetDlgItem($w,300)) '') -eq 0x242424) 'failed preference write retains selected Light theme for this session'
        $messages = [Collections.Generic.List[string]]::new()
        [void][MdSmoke]::EnumChildWindows($dialog, {param($child,$unused) $messages.Add([MdSmoke]::Content($child)); return $true}, [IntPtr]::Zero)
        Assert (($messages -join ' ').Contains('could not be saved')) 'preference write failure displays a clear persistence error'
        Dismiss-Error $dialog
    } finally { $lockedSettings.Dispose() }
    Assert ([IO.File]::ReadAllText($ini).Contains('theme=dark')) 'failed preference write preserves previous stored preference'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)

    $partialSettings = "; Preserve palette notes Ω`r`n[font]`r`npath=missing.ttf`r`n[appearance]`r`ntheme=dark`r`n[unrelated]`r`nkeep=original`r`n[theme.light]`r`nBACKGROUND=  #aBcDeF `t`r`ntext=#GG0011`r`nlink=#123`r`nmuted=`r`ncode=#11223344`r`ntable_border=#12345Z`r`n[theme.dark]`r`ntext=#123456`r`nlink=#ff0000`r`n"
    [IO.File]::WriteAllText($ini,$partialSettings,[Text.Encoding]::Unicode)
    $partial = Start-Process -FilePath $exe -ArgumentList "`"$testPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($partial)
    $w = Wait-Window $partial
    Command $w 201
    Check-Palette $partial $w @(0,0x563412,0x0000FF,0xA0A0A0,0x181818,0x484848) 'partial Dark palette'
    Command $w 203
    Check-Palette $partial $w @(0xEFCDAB,0x242424,0xAE5B00,0x5C6062,0xDEE5E8,0xB9BFC1) 'invalid Light entries fall back individually'
    $populated = [IO.File]::ReadAllText($ini)
    Assert ($populated.Contains('background=#000000') -and $populated.Contains('muted=#A0A0A0') -and $populated.Contains('code=#181818')) 'partial palettes gain only missing defaults'
    Assert ($populated.Contains('text=#GG0011') -and $populated.Contains('link=#123') -and $populated.Contains("muted=`r`n") -and $populated.Contains('code=#11223344')) 'invalid and empty color entries remain unchanged'
    Assert ($populated.Contains('table_border=#12345Z') -and $populated.Contains('table_border=#484848')) 'invalid table border remains editable and missing border gains default'
    Assert ($populated.Contains('; Preserve palette notes Ω') -and $populated.Contains('path=missing.ttf') -and $populated.Contains('keep=original') -and $populated.Contains("BACKGROUND=  #aBcDeF `t")) 'population and toggling preserve comments, fonts, unrelated settings, and existing colors'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)

    $customSettings = "[appearance]`r`ntheme=light`r`n[theme.light]`r`nbackground=#112233`r`ntext=#123456`r`nlink=#13579b`r`nmuted=#2468ac`r`ncode=#abcdef`r`ntable_border=#2468ac`r`n[theme.dark]`r`nbackground=#010203`r`ntext=#fedcba`r`nlink=#89abcd`r`nmuted=#765432`r`ncode=#102030`r`ntable_border=#14283c`r`n"
    [IO.File]::WriteAllText($ini,$customSettings,[Text.Encoding]::ASCII)
    $custom = Start-Process -FilePath $exe -ArgumentList "`"$testPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($custom)
    $w = Wait-Window $custom
    Command $w 201
    Check-Palette $custom $w @(0x332211,0x563412,0x9B5713,0xAC6824,0xEFCDAB,0xAC6824) 'custom Light palette'
    Command $w 203
    Check-Palette $custom $w @(0x030201,0xBADCFE,0xCDAB89,0x325476,0x302010,0x3C2814) 'custom Dark palette'
    # Editing the INI during a session must not reload colors on theme changes.
    $changedSettings = [IO.File]::ReadAllText($ini).Replace('text=#fedcba','text=#c0ffee')
    [IO.File]::WriteAllText($ini,$changedSettings,[Text.Encoding]::ASCII)
    Command $w 203
    Command $w 203
    Check-Palette $custom $w @(0x030201,0xBADCFE,0xCDAB89,0x325476,0x302010,0x3C2814) 'session keeps startup colors'
    Assert ([IO.File]::ReadAllText($ini).Contains('text=#c0ffee')) 'theme toggle writes only appearance theme and preserves manual color edits'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    $restarted = Start-Process -FilePath $exe -ArgumentList "`"$testPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($restarted)
    $w = Wait-Window $restarted
    Command $w 201
    Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),203,0) -band 8) -ne 0) 'custom palette restart retains selected Dark theme'
    Check-Palette $restarted $w @(0x030201,0xEEFFC0,0xCDAB89,0x325476,0x302010,0x3C2814) 'restart applies manually edited color'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)

    # A readable but locked partial INI forces failure while adding defaults.
    [IO.File]::WriteAllText($ini,"; Locked startup`r`n[theme.light]`r`ntext=#123456`r`n",[Text.Encoding]::ASCII)
    $lockedStartup = [IO.File]::Open($ini,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $unwritable = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
        $processes.Add($unwritable)
        $w = Wait-Window $unwritable
        $dialog = Wait-Window $unwritable '#32770'
        $messages = [Collections.Generic.List[string]]::new()
        [void][MdSmoke]::EnumChildWindows($dialog,{param($child,$unused) $messages.Add([MdSmoke]::Content($child)); return $true},[IntPtr]::Zero)
        Assert (($messages -join ' ').Contains('Missing theme colors could not be saved') -and ($messages -join ' ').Contains($ini)) 'startup shows one palette persistence error after creating the window'
        $source = [MdSmoke]::GetDlgItem($w,300)
        Assert ((Read-Color $unwritable $source '') -eq 0x563412) 'startup persistence failure retains loaded custom color'
        Check-Background $source 0xEEF3F5 'startup persistence failure retains default background'
        Dismiss-Error $dialog
        Start-Sleep -Milliseconds 150
        Assert ([MdSmoke]::Find($unwritable.Id,'#32770') -eq [IntPtr]::Zero -and -not $unwritable.HasExited) 'dismissing the single startup error leaves the editor usable'
    } finally { $lockedStartup.Dispose() }
    Assert ([IO.File]::ReadAllText($ini) -eq "; Locked startup`r`n[theme.light]`r`ntext=#123456`r`n") 'failed startup writes preserve existing configuration'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    Remove-Item -LiteralPath $ini
    $navigationPath = Join-Path $testDirectory 'navigation.md'
    $navigationText = @'
# Heading 😀

Paragraph **bold** &amp; &NotEqualTilde; &#x1F600; 日本語.
Continuation on another source line.
Hard break here.[hard-break]
After the hard break.

Escaped \*stars\* and *italic*; [same](target) then same.
Tabbed[tab]text and an unknown &unknownEntity;.

- First item
- Second item
    - Nested item

8. Ordered item
9. Next ordered item

> Quoted first line
> Quoted second line

~~~text
Fenced code 😀

	Tabbed code
~~~

    Indented code

[link label](https://example.com/long/generated/destination)

![image alt](picture.png)

<div>HTML contents</div>

Before rule

---

After rule

Repeated same text

Repeated same text

`multi
line code`

| Cell left | Cell center | Cell right |
| :--- | :---: | ---: |
| **Cell bold** | Cell 😀 tail | &NotEqualTilde; &#x1F600; |
| same | same | escaped\|pipe with several ordinary words for wrapping |
| | missing | |

After native table target.

> - List before table
>
>   | Nested header | Value |
>   | --- | ---: |
>   | Nested body Ω | 17 |
>
>   After nested table.

Wrapping sample has enough ordinary words to occupy several visual lines in a narrow window, followed by **wrapped target** near its end.

Last source line Ω
'@
    $navigationText = $navigationText.Replace('[hard-break]', '  ').Replace('[tab]', "`t")
    [IO.File]::WriteAllText($navigationPath,$navigationText,[Text.UTF8Encoding]::new($false))
    $navigation = Start-Process -FilePath $exe -ArgumentList "`"$navigationPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($navigation)
    $w = Wait-Window $navigation
    Command $w 201
    $source = [MdSmoke]::GetDlgItem($w,300)
    $view = [MdSmoke]::GetDlgItem($w,301)
    $samples = @(
        @('Heading','Heading'), @('bold','bold',0,2,2), @('&','&amp;'), @('≂̸','&NotEqualTilde;'), @('😀 日本語','&#x1F600;'),
        @('日本語','日本語',0,2,2), @('Heading 😀','Heading 😀',0,8,8),
        @(' Continuation','Continuation',0,0,-1), @('Continuation','Continuation',0,12,12), @('After the hard','After the hard'),
        @('*stars*','*stars',0,0,0), @('*stars*','stars',0,4,3), @('italic','italic',0,3,3),
        @('Tabbed','Tabbed',0,6,6), @('unknownEntity','unknownEntity',0,5,5),
        @('First item','First item'), @('Second item','Second item'), @('Nested item','Nested item'),
        @('Ordered item','Ordered item'), @('Next ordered','Next ordered'),
        @('Quoted first','Quoted first'), @('Quoted second','Quoted second'),
        @('Fenced code','Fenced code'), @('Tabbed code','Tabbed code'), @('Indented code','Indented code'),
        @('link label','link label',0,6,6), @('https://example.com','](https://'),
        @('[Image:','image alt'), @('image alt','image alt'), @(' alt',' alt'), @('picture.png','](picture.png)'),
        @('[HTML source]','<div>HTML'), @('<div>HTML','<div>HTML'), @('────────────────────────','Before rule',0,0,11),
        @('─','After rule',23), @('Repeated same text','Repeated same text'),
        @('multi','multi'), @('multi line','multi',0,5,5), @('line code','line code',0,4,4), @('Last source','Last source',0,10,10),
        @('Cell bold','Cell bold',0,6,6), @('Cell center','Cell center',0,4,4), @('Cell right','Cell right',0,7,7),
        @('Cell 😀 tail','Cell 😀 tail',0,5,5), @('After native table','After native table',0,8,8),
        @('Nested body Ω','Nested body Ω',0,8,8), @('After nested table','After nested table',0,9,9)
    )
    foreach ($sample in $samples) {
        $occurrence = if ($sample.Length -gt 2) { $sample[2] } else { 0 }
        $renderOffset = if ($sample.Length -gt 3) { $sample[3] } else { 0 }
        $sourceOffset = if ($sample.Length -gt 4) { $sample[4] } else { 0 }
        Navigate $navigation $w $sample[0] $sample[1] $occurrence '' 0 $renderOffset $sourceOffset
    }
    Navigate $navigation $w 'Repeated same text' 'Repeated same text' 1 'second identical paragraph' 1
    Command $w 201
    $tableRaw = [MdSmoke]::Raw($navigation.Id,$view)
    $naturalRow = Read-Table $navigation $view 'Cell left'
    [void][MdSmoke]::Send($view,177,[IntPtr]5,[IntPtr]10)
    [void][MdSmoke]::Send($view,182,[IntPtr]::Zero,[IntPtr]10)
    [void][MdSmoke]::Send($source,177,[IntPtr]7,[IntPtr]11)
    $sourceState = Control-State $navigation $source
    $selectionBefore = Read-Selection $navigation $view
    $previewState = Control-State $navigation $view
    [void][MdSmoke]::MoveWindow($w,0,0,360,700,$false)
    $narrowRow = Read-Table $navigation $view 'Cell left'
    $selectionAfter = Read-Selection $navigation $view
    Assert (([BitConverter]::ToInt32($narrowRow,28) + [BitConverter]::ToInt32($narrowRow,68) + [BitConverter]::ToInt32($narrowRow,108)) -lt ([BitConverter]::ToInt32($naturalRow,28) + [BitConverter]::ToInt32($naturalRow,68) + [BitConverter]::ToInt32($naturalRow,108))) 'window resize shrinks native table widths'
    Assert ($tableRaw -eq [MdSmoke]::Raw($navigation.Id,$view) -and ($selectionBefore -join ',') -eq ($selectionAfter -join ',') -and (Control-State $navigation $source) -eq $sourceState) 'table resize preserves native offsets, preview selection, and source selection/scroll'
    Assert ((Control-State $navigation $view) -eq $previewState) 'table resize preserves preview selection and nonzero scroll position'
    [void][MdSmoke]::MoveWindow($w,0,0,900,700,$false)
    $restoredRow = Read-Table $navigation $view 'Cell left'
    Assert ([BitConverter]::ToInt32($restoredRow,68) -eq [BitConverter]::ToInt32($naturalRow,68)) 'growing the window restores table content widths'
    Check-Viewport $navigation $w $view ([MdSmoke]::SystemDpi()) 'table preview after window resize'
    [void][MdSmoke]::Send($view,177,[IntPtr]::Zero,[IntPtr](-1))
    [void][MdSmoke]::Send($view,769,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert ([MdSmoke]::Clipboard().Contains('Cell bold') -and [MdSmoke]::Clipboard().Contains('After native table target.') -and -not [MdSmoke]::Clipboard().Contains([char]0xfff9)) 'copying table documents retains cell and prose text without hidden row markers'
    # Every coordinate is obtained from Rich Edit after scrolling to the sample.
    # Source remains alive, with its own independently chosen previous position.
    Command $w 201
    $point = Char-Point $navigation $view 0
    [void][MdSmoke]::Send($source,177,[IntPtr]7,[IntPtr]11)
    $sourceState = Control-State $navigation $source
    Command $w 200
    Assert ((Control-State $navigation $source) -eq $sourceState) 'ordinary mode changes retain the prior source selection and scroll'
    $point = Char-Point $navigation $source 3
    Mouse-At $source 515 @($point[0],($point[1] + 2))
    $selection = Read-Selection $navigation $source
    Assert ($selection[0] -lt $selection[1] -and ([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),200,0) -band 8) -ne 0) 'Edit keeps normal double-click word selection'
    Command $w 201
    $point = Char-Point $navigation $view 2
    foreach ($keys in @(5,9)) { # Shift+left, Ctrl+left
        Mouse-At $view 515 @($point[0],($point[1] + 2)) $keys
        Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),201,0) -band 8) -ne 0) "modified double-click retains View (mouse flags $keys)"
    }
    # A control-click on a detected link goes through Rich Edit's existing
    # handling; a synthetic mouse flag alone never launches an external app.
    $urlIndex = [MdSmoke]::Content($view).Replace("`r`n","`r").IndexOf('https://')
    $point = Char-Point $navigation $view $urlIndex
    Mouse-At $view 515 @($point[0],($point[1] + 2)) 9
    Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),201,0) -band 8) -ne 0) 'Ctrl-double-click on a URL preserves View and the link handler'
    $first = Char-Point $navigation $view 0
    $last = Char-Point $navigation $view 5
    Mouse-At $view 513 @($first[0],($first[1] + 2))
    $selection = Read-Selection $navigation $view
    Assert ($selection[0] -eq 0 -and $selection[1] -eq 0) 'single-click keeps native preview caret positioning'
    Mouse-At $view 512 @($last[0],($last[1] + 2))
    Mouse-At $view 514 @($last[0],($last[1] + 2)) 0
    $selection = Read-Selection $navigation $view
    Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),201,0) -band 8) -ne 0) 'single-click and drag messages retain View (physical drag selection needs manual review)'
    # A double-click in the document's left margin still hit-tests its line.
    [void](Char-Point $navigation $view 0)
    Mouse-At $view 515 @(1,($first[1] + 2))
    $selection = Read-Selection $navigation $source
    Assert ($selection[0] -eq 2 -and $selection[1] -eq 2 -and ([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),200,0) -band 8) -ne 0) 'double-click in left document margin enters Edit after the hidden heading delimiter'
    [void][MdSmoke]::MoveWindow($w,0,0,360,420,$false)
    Navigate $navigation $w 'wrapped target' 'wrapped target' 0 'after wrapping at a narrow width' 0 9 9
    foreach ($testDpi in @(96,144,192)) {
        Command $w 201
        $rect = [MdSmoke+Rect]::new()
        [void][MdSmoke]::GetWindowRect($w,[ref]$rect)
        [void][MdSmoke]::Rectangle($w,736,[IntPtr](($testDpi -shl 16) -bor $testDpi),[ref]$rect)
        Command $w 203
        Navigate $navigation $w 'wrapped target' 'wrapped target' 0 "after wrapping, scrolling, theme refresh, and $testDpi DPI" 0 9 9
        Navigate $navigation $w 'Cell bold' 'Cell bold' 0 "inside a table after resize and $testDpi DPI" 0 6 6
        Check-Viewport $navigation $w $view $testDpi "table preview at $testDpi DPI"
    }
    # Navigation must leave actual undo and redo records usable, including a
    # pending redo before the first navigation and a dirty edit afterward.
    [void][MdSmoke]::Send($source,177,[IntPtr](-1),[IntPtr](-1))
    [void][MdSmoke]::Text($source,194,[IntPtr]1,"`r`n`r`nNew paragraph after edit")
    [void][MdSmoke]::Send($source,199,[IntPtr]::Zero,[IntPtr]::Zero)
    Navigate $navigation $w 'Heading' 'Heading' 0 'with a pending redo'
    [void][MdSmoke]::Send($source,1108,[IntPtr]::Zero,[IntPtr]::Zero)
    Navigate $navigation $w 'New paragraph' 'New paragraph' 0 'after edit and rerender' 0 8 8
    [void][MdSmoke]::Send($source,199,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert (-not [MdSmoke]::Content($source).Contains('New paragraph')) 'undo still works after double-click navigation'
    [void][MdSmoke]::Send($source,1108,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert ([MdSmoke]::Content($source).Contains('New paragraph')) 'redo still works after double-click navigation'
    Command $w 101
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)

    # Exercise native scrollbar changes without installing an explicit vertical
    # formatting inset: stale table pixels must have no bottom strip to occupy.
    $overflowPath = Join-Path $testDirectory 'table-viewport.md'
    $header = '| ' + ((1..24 | ForEach-Object { 'i' }) -join ' | ') + ' |'
    $separator = '| ' + ((1..24 | ForEach-Object { '---' }) -join ' | ') + ' |'
    $row = '| ' + ((1..24 | ForEach-Object { 'x' }) -join ' | ') + ' |'
    $overflowText = $header + "`n" + $separator + "`n" + (($row + "`n") * 10) + "`nAfter overflow table.`n`n" + ("Following prose.`n`n" * 50)
    [IO.File]::WriteAllText($overflowPath,$overflowText)
    $overflowApp = Start-Process -FilePath $exe -ArgumentList "`"$overflowPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($overflowApp)
    $w = Wait-Window $overflowApp
    Command $w 201
    $source = [MdSmoke]::GetDlgItem($w,300)
    $view = [MdSmoke]::GetDlgItem($w,301)
    $overflowRaw = [MdSmoke]::Raw($overflowApp.Id,$view)
    foreach ($width in @(900,360,900)) {
        [void][MdSmoke]::MoveWindow($w,0,0,$width,600,$false)
        Check-Viewport $overflowApp $w $view ([MdSmoke]::SystemDpi()) "table preview at window width $width"
        Check-Viewport $overflowApp $w $source ([MdSmoke]::SystemDpi()) "source at window width $width"
        $horizontal = ([MdSmoke]::GetWindowLongPtrW($view,-16) -band 0x100000) -ne 0
        Assert ($horizontal -eq ($width -eq 360)) "native horizontal scrollbar follows table overflow at window width $width"
        [void][MdSmoke]::Send($view,182,[IntPtr]::Zero,[IntPtr]30)
        Check-Viewport $overflowApp $w $view ([MdSmoke]::SystemDpi()) "table preview after scrolling at window width $width"
    }
    Assert ([MdSmoke]::Raw($overflowApp.Id,$view) -eq $overflowRaw -and -not [MdSmoke]::Content($w).StartsWith('*')) 'viewport and scrollbar changes retain rendered text and clean source'
    Navigate $overflowApp $w 'After overflow table.' 'After overflow table.' 0 'after table scrolling and scrollbar changes' 0 6 6
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)

    # Short documents leave blank client space below the last rendered line.
    $blankPath = Join-Path $testDirectory 'blank-space.md'
    [IO.File]::WriteAllText($blankPath,"# Top 😀`n`nBottom Ω")
    $blankSpace = Start-Process -FilePath $exe -ArgumentList "`"$blankPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($blankSpace)
    $w = Wait-Window $blankSpace
    Command $w 201
    $source = [MdSmoke]::GetDlgItem($w,300)
    $view = [MdSmoke]::GetDlgItem($w,301)
    $rect = [MdSmoke+Rect]::new()
    [void][MdSmoke]::GetClientRect($view,[ref]$rect)
    # WM_NCLBUTTONDBLCLK with HTVSCROLL must never enter Edit.
    [void][MdSmoke]::PostMessage($view,163,[IntPtr]7,[IntPtr]::Zero)
    [void][MdSmoke]::PostMessage($view,514,[IntPtr]::Zero,[IntPtr]::Zero)
    [void][MdSmoke]::PostMessage($view,31,[IntPtr]::Zero,[IntPtr]::Zero) # WM_CANCELMODE ends native scrollbar tracking
    [void][MdSmoke]::Send($view,0,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert (([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),201,0) -band 8) -ne 0) 'nonclient scrollbar double-click retains View'
    Mouse-At $view 515 @([int]($rect.right / 2),($rect.bottom - 2))
    $selection = Read-Selection $blankSpace $source
    Assert ($selection[0] -eq 18 -and $selection[1] -eq 18 -and ([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),200,0) -band 8) -ne 0) 'double-click in blank document space maps to the final source end'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    foreach ($emptyText in @('',"`n`n",'---','![](picture.png)')) {
        [IO.File]::WriteAllText($blankPath,$emptyText)
        $emptyApp = Start-Process -FilePath $exe -ArgumentList "`"$blankPath`"" -WindowStyle Hidden -PassThru
        $processes.Add($emptyApp)
        $w = Wait-Window $emptyApp
        Command $w 201
        $source = [MdSmoke]::GetDlgItem($w,300)
        $view = [MdSmoke]::GetDlgItem($w,301)
        Mouse-At $view 515 @(30,30)
        $selection = Read-Selection $emptyApp $source
        Assert ($selection[0] -eq 0 -and $selection[1] -eq 0 -and ([MdSmoke]::GetMenuState([MdSmoke]::GetMenu($w),200,0) -band 8) -ne 0) "document without sourced spans maps safely to zero (length $($emptyText.Length))"
        [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    }
    foreach ($encoding in @([Text.UTF8Encoding]::new($false),[Text.UnicodeEncoding]::new($false,$true),[Text.UnicodeEncoding]::new($true,$true))) {
        $encodedPath = Join-Path $testDirectory ($encoding.CodePage.ToString() + '.md')
        [IO.File]::WriteAllText($encodedPath,"# Encoding`nUnicode Ω`n",$encoding)
        $encoded = Start-Process -FilePath $exe -ArgumentList "`"$encodedPath`"" -WindowStyle Hidden -PassThru
        $processes.Add($encoded)
        $w = Wait-Window $encoded
        for ($n=0; $n -lt 100 -and -not [MdSmoke]::Content($w).Contains($encoding.CodePage.ToString()); ++$n) { Start-Sleep -Milliseconds 50 }
        $source = [MdSmoke]::GetDlgItem($w,300)
        Command $w 200
        [void][MdSmoke]::Send($source,177,[IntPtr](-1),[IntPtr](-1))
        [void][MdSmoke]::Text($source,194,[IntPtr]1,'end')
        Command $w 101
        $bytes = [IO.File]::ReadAllBytes($encodedPath)
        $expected = $encoding.GetPreamble() + $encoding.GetBytes("# Encoding`nUnicode Ω`nend")
        Assert ([Convert]::ToBase64String($bytes) -eq [Convert]::ToBase64String($expected)) "encoding $($encoding.CodePage) and LF newlines survive edit/save"
        [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    }
    Remove-Item -LiteralPath $ini
    $largePath = Join-Path $testDirectory 'large.md'
    $largeText = ("## Heading`r`n`r`nParagraph **bold** with [link](https://example.com) and ``code``.`r`n`r`n" * 5000)
    [IO.File]::WriteAllText($largePath,$largeText)
    $large = Start-Process -FilePath $exe -ArgumentList "`"$largePath`"" -WindowStyle Hidden -PassThru
    $processes.Add($large)
    $w = Wait-Window $large
    for ($n=0; $n -lt 200 -and -not [MdSmoke]::Content($w).Contains('large.md'); ++$n) { Start-Sleep -Milliseconds 50 }
    Command $w 201
    $source = [MdSmoke]::GetDlgItem($w,300)
    $view = [MdSmoke]::GetDlgItem($w,301)
    [void][MdSmoke]::Send($view,177,[IntPtr]5000,[IntPtr]5020)
    [void][MdSmoke]::Send($view,182,[IntPtr]::Zero,[IntPtr]150)
    [void][MdSmoke]::Send($source,177,[IntPtr]4000,[IntPtr]4020)
    [void][MdSmoke]::Send($source,182,[IntPtr]20,[IntPtr]120)
    $viewState = Control-State $large $view
    $sourceState = Control-State $large $source
    $timer.Restart()
    Command $w 203
    $timer.Stop()
    Write-Output "Large document ($($largeText.Length) characters) View theme switch: $($timer.ElapsedMilliseconds) ms"
    Assert ((Control-State $large $view) -eq $viewState -and (Control-State $large $source) -eq $sourceState) 'large document theme switch preserves selections and nonzero scroll positions in both controls'
    Assert (-not [MdSmoke]::Content($w).StartsWith('*')) 'large document theme switch leaves source clean'
    Navigate $large $w 'bold' 'bold' 3000 'beyond the 16-bit character range' 3000 2 2
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    Remove-Item -LiteralPath $ini
    Write-Output 'Smoke check complete. Physical mixed-DPI monitor movement and interactive keyboard/mouse use still need manual review.'
}
finally {
    foreach ($p in $processes) { if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() } }
    if ($null -ne $clipboardBackup) { Set-Clipboard -Value $clipboardBackup }
}
