<#
.SYNOPSIS
修复因 GBK/UTF-8 编码错乱导致的乱码文件名。

原理：解压工具把 zip 中 GBK 编码的中文文件名误按 UTF-8 解码，
产生含 U+FFFD 的乱码名（破坏性，无法从乱码本身恢复）。
本脚本解析同名 zip 中央目录里的原始字节，按 GBK 还原正确中文名，
再递归地把磁盘上的乱码目录/文件重命名并恢复到与 zip 一致的目录结构。

.EXAMPLE
# 预览（不做任何修改）
.\Fix-GarbledNames.ps1 -Folder 'D:\BaiduNetdiskDownload\A1-5-4A\参考资料\3.实验例程'

.EXAMPLE
# 实际执行
.\Fix-GarbledNames.ps1 -Folder 'D:\BaiduNetdiskDownload\A1-5-4A\参考资料\3.实验例程' -Apply
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Folder,

    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$Folder = (Get-Item -LiteralPath $Folder).FullName.TrimEnd('\')
$zipPath = "$Folder.zip"
if (-not (Test-Path -LiteralPath $zipPath)) { throw "未找到同名 zip: $zipPath" }

# GBK(936) 编码；.NET Core 下需注册 CodePages 提供程序
$gbk = $null
try { $gbk = [System.Text.Encoding]::GetEncoding(936) }
catch {
    Add-Type -AssemblyName System.Text.Encoding.CodePages
    [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
    $gbk = [System.Text.Encoding]::GetEncoding(936)
}
$utf8 = [System.Text.Encoding]::UTF8

# ---------- 1. 解析 zip 中央目录，取每个条目的原始名字字节 ----------
$bytes = [System.IO.File]::ReadAllBytes($zipPath)
$eocd = -1
for ($i = $bytes.Length - 22; $i -ge 0; $i--) {
    if ($bytes[$i] -eq 0x50 -and $bytes[$i + 1] -eq 0x4B -and $bytes[$i + 2] -eq 0x05 -and $bytes[$i + 3] -eq 0x06) { $eocd = $i; break }
}
if ($eocd -lt 0) { throw 'zip 中未找到 EOCD 记录' }

$cdOffset = [BitConverter]::ToUInt32($bytes, $eocd + 16)
$pos = [int]$cdOffset
$entries = @()
while ($pos + 4 -lt $bytes.Length) {
    if ($bytes[$pos] -eq 0x50 -and $bytes[$pos + 1] -eq 0x4B -and $bytes[$pos + 2] -eq 0x01 -and $bytes[$pos + 3] -eq 0x02) {
        $flags = [BitConverter]::ToUInt16($bytes, $pos + 8)
        $nameLen = [BitConverter]::ToUInt16($bytes, $pos + 28)
        $extraLen = [BitConverter]::ToUInt16($bytes, $pos + 30)
        $commentLen = [BitConverter]::ToUInt16($bytes, $pos + 32)
        $nb = New-Object byte[] $nameLen
        [Array]::Copy($bytes, $pos + 46, $nb, 0, $nameLen)

        $isUtf8Flag = ($flags -band 0x800) -ne 0
        $correct = if ($isUtf8Flag) { $utf8.GetString($nb) } else { $gbk.GetString($nb) }
        $garbled = if ($isUtf8Flag) { $correct } else { $utf8.GetString($nb) }  # 磁盘乱码 = GBK 字节误按 UTF-8 解码

        $entries += [pscustomobject]@{
            Correct = $correct
            Garbled = $garbled
            IsDir   = $correct.EndsWith('/')
        }
        $pos = $pos + 46 + $nameLen + $extraLen + $commentLen
    } else { break }
}
if ($entries.Count -eq 0) { throw 'zip 中央目录为空' }

# ---------- 2. 定位磁盘上的乱码根文件夹 ----------
$rootEntry = $entries[0]
$rootCorrect = $rootEntry.Correct.TrimEnd('/')     # 如 3.实验例程
$rootGarbled = $rootEntry.Garbled.TrimEnd('/')     # 如 3.ʵ������
$diskRoot = Join-Path $Folder $rootGarbled
if (-not (Test-Path -LiteralPath $diskRoot)) {
    throw "未找到乱码根文件夹: $diskRoot`n（预期名称: $rootGarbled）"
}

# ---------- 3. 生成相对路径映射并校验 ----------
$plan = @()
for ($i = 1; $i -lt $entries.Count; $i++) {
    $e = $entries[$i]
    $correctRel = $e.Correct.Substring($rootEntry.Correct.Length).TrimEnd('/').Replace('/', '\')
    $garbledRel = $e.Garbled.Substring($rootEntry.Garbled.Length).TrimEnd('/').Replace('/', '\')
    # 叶子名相同（如纯 ASCII 文件/目录）时无需单独重命名，父目录改名后路径自然恢复
    if (($correctRel.Split('\')[-1]) -eq ($garbledRel.Split('\')[-1])) { continue }
    $plan += [pscustomobject]@{
        CorrectRel = $correctRel
        GarbledRel = $garbledRel
        Depth      = ($correctRel -split '\\').Count
        IsDir      = $e.IsDir
    }
}

"zip 条目: $($entries.Count)（含根）| 待处理: $($plan.Count)"
$missing = @()
foreach ($p in $plan) {
    if (-not (Test-Path -LiteralPath (Join-Path $diskRoot $p.GarbledRel))) { $missing += $p.GarbledRel }
}
if ($missing.Count) {
    "!! 以下乱码路径在磁盘上不存在（可能已被部分处理）:"
    $missing | ForEach-Object { "  $_" }
    throw '映射校验失败，已中止'
}

if (-not $Apply) {
    '===== 预览（确认无误后加 -Apply 执行）====='
    $plan | Sort-Object Depth -Descending | ForEach-Object {
        "  $($_.GarbledRel)  =>  $($_.CorrectRel)"
    }
    "共 $($plan.Count) 个条目待重命名；乱码根 '$rootGarbled' 内容将提升到 '$Folder' 并删除空壳"
    return
}

# ---------- 4. 执行：按深度从深到浅重命名 ----------
$renamed = 0
foreach ($p in ($plan | Sort-Object Depth -Descending)) {
    $src = Join-Path $diskRoot $p.GarbledRel
    $dst = Join-Path (Split-Path $src -Parent) ($p.CorrectRel.Split('\')[-1])
    if (Test-Path -LiteralPath $dst) { throw "目标已存在，无法重命名: $dst" }
    if ($p.IsDir) { [System.IO.Directory]::Move($src, $dst) }
    else          { [System.IO.File]::Move($src, $dst) }
    $renamed++
}
"已重命名: $renamed"

# ---------- 5. 把乱码根下的内容提升到目标文件夹并删除空壳 ----------
$moved = 0
foreach ($item in Get-ChildItem -LiteralPath $diskRoot -Force) {
    $target = Join-Path $Folder $item.Name
    if (Test-Path -LiteralPath $target) { throw "目标已存在: $target" }
    if ($item.PSIsContainer) { [System.IO.Directory]::Move($item.FullName, $target) }
    else                      { [System.IO.File]::Move($item.FullName, $target) }
    $moved++
}
if ($moved -eq 0) { throw '乱码根下没有内容' }
Remove-Item -LiteralPath $diskRoot -Force
"已提升到 '$Folder': $moved 项；乱码根 '$rootGarbled' 已删除"
'DONE'
