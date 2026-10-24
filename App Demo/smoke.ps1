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
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
    public static uint SystemDpi() {var old=SetThreadDpiAwarenessContext((IntPtr)(-4));try{return GetDpiForSystem();}finally{SetThreadDpiAwarenessContext(old);}}
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
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
    public static string Content(IntPtr w) {var b=new StringBuilder(65536);Read(w,13,(IntPtr)b.Capacity,b);return b.ToString();}
    public static string Clipboard() {if(!OpenClipboard(IntPtr.Zero))return null;try{var h=GetClipboardData(13);var p=GlobalLock(h);try{return p==IntPtr.Zero?null:Marshal.PtrToStringUni(p);}finally{if(p!=IntPtr.Zero)GlobalUnlock(h);}}finally{CloseClipboard();}}
    public static byte[] Remote(uint pid,IntPtr w,uint msg,IntPtr wp,byte[] bytes) {var p=OpenProcess(0x38,false,pid);if(p==IntPtr.Zero)throw new Exception("OpenProcess");var a=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)bytes.Length,0x3000,4);try{UIntPtr n;if(!WriteProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n))throw new Exception("WriteProcessMemory");Send(w,msg,wp,a);if(!ReadProcessMemory(p,a,bytes,(UIntPtr)bytes.Length,out n))throw new Exception("ReadProcessMemory");return bytes;}finally{VirtualFreeEx(p,a,UIntPtr.Zero,0x8000);CloseHandle(p);}}
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
function Dialog-Path($process, [string]$path) {
    $dialog = Wait-Window $process '#32770'
    for ($n = 0; $n -lt 100; ++$n) {
        $edit = [MdSmoke]::Edit($dialog)
        $button = [MdSmoke]::GetDlgItem($dialog, 1)
        if ($edit -ne [IntPtr]::Zero -and $button -ne [IntPtr]::Zero -and [MdSmoke]::IsWindowEnabled($button)) { break }
        Start-Sleep -Milliseconds 50
    }
    if ($edit -eq [IntPtr]::Zero) { throw 'File name edit not found' }
    [void][MdSmoke]::Send($dialog, 40, $edit, [IntPtr]1) # WM_NEXTDLGCTL sets focus
    [void][MdSmoke]::Send($edit, 177, [IntPtr]::Zero, [IntPtr](-1))
    [void][MdSmoke]::Text($edit, 194, [IntPtr]1, $path)
    if ([MdSmoke]::Content($edit) -ne $path) { throw 'File dialog did not accept the test path' }
    [void][MdSmoke]::Send($dialog, 40, $button, [IntPtr]1)
    Start-Sleep -Milliseconds 100
    [void][MdSmoke]::PostMessage($dialog, 273, [IntPtr]1, $button)
    for ($n = 0; $n -lt 100 -and [MdSmoke]::Find($process.Id, '#32770') -ne [IntPtr]::Zero; ++$n) { Start-Sleep -Milliseconds 50 }
    Start-Sleep -Milliseconds 200
}
function Read-Font($process, $control) {
    $bytes = [byte[]]::new(116)
    [BitConverter]::GetBytes(116).CopyTo($bytes, 0)
    $bytes = [MdSmoke]::Remote($process.Id, $control, 1082, [IntPtr]1, $bytes)
    return [Text.Encoding]::Unicode.GetString($bytes, 26, 64).TrimEnd([char]0)
}
function Read-Color($process, $control, [string]$needle, [int]$offset = 20) {
    $index = [MdSmoke]::Content($control).Replace("`r`n", "`r").IndexOf($needle)
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
$testDirectory = Join-Path $PSScriptRoot 'build\smoke'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
$ini = Join-Path $testDirectory 'markdown-editor.ini'
if (Test-Path -LiteralPath $ini) { Remove-Item -LiteralPath $ini }
$exe = Join-Path $testDirectory 'markdown-editor.exe'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\markdown-editor.exe') -Destination $exe -Force
$testPath = Join-Path $testDirectory 'Grüße 日本語 with spaces.md'
$markdown = "# Heading`r`n`r`n**bold** and *emphasis* &copy; 😀`r`n`r`n- one`r`n- two`r`n`r`n---`r`n`r`n[link](https://example.com)`r`n`r`n![alt](image.png)`r`n`r`n<div>visible</div>`r`n`r`n> Quote sample`r`n`r`n``code sample```r`n`r`n    block sample`r`n"
[IO.File]::WriteAllText($testPath, $markdown, [Text.UTF8Encoding]::new($true))
$processes = [Collections.Generic.List[Diagnostics.Process]]::new()
$clipboardBackup = [MdSmoke]::Clipboard()
try {
    $process = Start-Process -FilePath $exe -ArgumentList "`"$testPath`"" -WindowStyle Hidden -PassThru
    $processes.Add($process)
    $w = Wait-Window $process
    # WinMain opens the command-line document just after showing the window.
    for ($n = 0; $n -lt 100 -and [MdSmoke]::Content($w) -notlike '*with spaces.md*'; ++$n) { Start-Sleep -Milliseconds 50 }
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
    Command $w 201
    Assert ([MdSmoke]::Content($view).Contains('Added source Ω')) 'entering View after editing rerenders'
    Command $w 200
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
        Post-Command $dialog 1
    } finally { $lockedFile.Dispose() }
    Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes($testPath)) -eq [Convert]::ToBase64String($beforeFailure)) 'failed save leaves the original file intact'
    Command $w 101
    Command $w 201
    $view = [MdSmoke]::GetDlgItem($w,301)
    $rect = [MdSmoke+Rect]::new()
    [void][MdSmoke]::GetWindowRect($w,[ref]$rect)
    $systemDpi = [MdSmoke]::SystemDpi()
    foreach ($testDpi in @(96,144,192,$systemDpi)) {
        [void][MdSmoke]::Rectangle($w,736,[IntPtr](($testDpi -shl 16) -bor $testDpi),[ref]$rect)
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
        Post-Command $dialog 1
    } finally { $lockedSettings.Dispose() }
    Assert ([IO.File]::ReadAllText($ini).Contains('theme=dark')) 'failed preference write preserves previous stored preference'
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
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
    [void][MdSmoke]::Send($w,16,[IntPtr]::Zero,[IntPtr]::Zero)
    Remove-Item -LiteralPath $ini
    Write-Output 'Smoke check complete. Physical mixed-DPI monitor movement and interactive keyboard/mouse use still need manual review.'
}
finally {
    foreach ($p in $processes) { if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() } }
    if ($null -ne $clipboardBackup) { Set-Clipboard -Value $clipboardBackup }
}
