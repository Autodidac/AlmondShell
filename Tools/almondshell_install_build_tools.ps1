<#
    AlmondShell Toolchain Installer
    - Everything optional
    - Bundles for convenience
    - Dependency-aware
    - Hybrid detection (command + winget)
    - Formatted logs, retry logic, version checks, PATH validation
    - Optional portable installers for CMake/Ninja with --portable
    - Built-in GUI: no args -> GUI mode, args -> CLI mode

    Examples (CLI):
        ./almondshell_install.ps1 --cmake --ninja
        ./almondshell_install.ps1 --bundle core
        ./almondshell_install.ps1 --bundle full
        ./almondshell_install.ps1 --msvc --clang
        ./almondshell_install.ps1 --vscode --vscode_ext_cpp
        ./almondshell_install.ps1 --bundle core --portable
#>

param(
    # Individual tools (all optional)
    [switch]$cmake,
    [switch]$ninja,
    [switch]$git,
    [switch]$vscode,
    [switch]$vscode_ext_cpp,
    [switch]$vscode_ext_cmake,
    [switch]$clang,
    [switch]$msvc,
    [switch]$vcpkg,
    [switch]$glslang,
    [switch]$vulkan,
    [switch]$python,

    # Bundles
    [string]$bundle,

    # Use portable installs where supported (CMake/Ninja)
    [switch]$portable,

    [switch]$help
)

$ErrorActionPreference = "Stop"
$script:LogFile   = Join-Path $PSScriptRoot "almondshell_install.log"
$script:ScriptSelf = $PSCommandPath

if (Test-Path $LogFile) {
    Remove-Item $LogFile -Force
}

function Log {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $line = "[$ts][$Level] $Message"
    Write-Host $line
    Add-Content -Path $script:LogFile -Value $line
}

function Help {
    Write-Host "AlmondShell Toolchain Installer"
    Write-Host ""
    Write-Host "Individual options:"
    Write-Host "  --cmake              Install CMake"
    Write-Host "  --ninja              Install Ninja"
    Write-Host "  --git                Install Git"
    Write-Host "  --python             Install Python 3"
    Write-Host "  --vscode             Install VS Code"
    Write-Host "  --vscode_ext_cpp     Install VSCode C++ extension"
    Write-Host "  --vscode_ext_cmake   Install VSCode CMake extension"
    Write-Host "  --clang              Install LLVM/Clang"
    Write-Host "  --msvc               Install MSVC Build Tools"
    Write-Host "  --vcpkg              Install vcpkg (clone + bootstrap)"
    Write-Host "  --glslang            Install glslangValidator"
    Write-Host "  --vulkan             Vulkan SDK (manual link only)"
    Write-Host "  --portable           Use portable installers for CMake/Ninja (tools/ directory)"
    Write-Host ""
    Write-Host "Bundles:"
    Write-Host "  --bundle core        CMake + Ninja + Git"
    Write-Host "  --bundle compilers   MSVC + Clang"
    Write-Host "  --bundle vscode      VSCode + extensions"
    Write-Host "  --bundle graphics    glslang + Vulkan"
    Write-Host "  --bundle full        EVERYTHING"
}

function Start-AlmondShellInstallerGui {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    $form = New-Object System.Windows.Forms.Form
    $form.Text = "AlmondShell Toolchain Installer"
    $form.Size = New-Object System.Drawing.Size(520,640)
    $form.StartPosition = "CenterScreen"

    $y = 20

    # Checkboxes for individual switches
    $checkboxMap = @{}

    foreach ($name in @(
        "cmake","ninja","git","python",
        "vscode","vscode_ext_cpp","vscode_ext_cmake",
        "clang","msvc","vcpkg","glslang","vulkan","portable"
    )) {
        $cb = New-Object System.Windows.Forms.CheckBox
        $cb.Text = $name
        $cb.Location = New-Object System.Drawing.Point(20,$y)
        $cb.AutoSize = $true
        $form.Controls.Add($cb)
        $checkboxMap[$name] = $cb
        $y += 24
    }

    # Bundle dropdown
    $labelBundle = New-Object System.Windows.Forms.Label
    $labelBundle.Text = "Bundle (optional):"
    $labelBundle.Location = New-Object System.Drawing.Point(260,20)
    $labelBundle.AutoSize = $true
    $form.Controls.Add($labelBundle)

    $comboBundle = New-Object System.Windows.Forms.ComboBox
    $comboBundle.Location = New-Object System.Drawing.Point(260, 45)
    $comboBundle.Size = New-Object System.Drawing.Size(200,22)
    $comboBundle.DropDownStyle = 'DropDownList'
    $comboBundle.Items.Add("")        | Out-Null
    $comboBundle.Items.Add("core")    | Out-Null
    $comboBundle.Items.Add("compilers") | Out-Null
    $comboBundle.Items.Add("vscode")  | Out-Null
    $comboBundle.Items.Add("graphics")| Out-Null
    $comboBundle.Items.Add("full")    | Out-Null
    $comboBundle.SelectedIndex = 0
    $form.Controls.Add($comboBundle)

    # Run button
    $run = New-Object System.Windows.Forms.Button
    $run.Text = "Run Installer"
    $run.Location = New-Object System.Drawing.Point(20, $y + 10)
    $run.Size = New-Object System.Drawing.Size(450,40)
    $form.Controls.Add($run)

    # Log text box
    $logBox = New-Object System.Windows.Forms.TextBox
    $logBox.Multiline = $true
    $logBox.ScrollBars = "Vertical"
    $logBox.Location = New-Object System.Drawing.Point(20,$y + 70)
    $logBox.Size = New-Object System.Drawing.Size(450,380)
    $logBox.Font = New-Object System.Drawing.Font("Consolas",9)
    $form.Controls.Add($logBox)

    $run.Add_Click({
        $cliArgs = @()

        foreach ($entry in $checkboxMap.GetEnumerator()) {
            if ($entry.Value.Checked) {
                $cliArgs += ("--" + $entry.Key)
            }
        }

        $selectedBundle = $comboBundle.SelectedItem
        if ($selectedBundle -and $selectedBundle.Trim().Length -gt 0) {
            $cliArgs += "--bundle"
            $cliArgs += $selectedBundle.Trim()
        }

        if ($cliArgs.Count -eq 0) {
            $logBox.AppendText("No options selected. Nothing to do.`r`n")
            return
        }

        $scriptPath = $script:ScriptSelf
        $quotedScript = '"' + $scriptPath + '"'
        $argLine = $cliArgs -join " "

        $cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File $quotedScript $argLine"
        $logBox.AppendText("Running: $cmd`r`n")

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "powershell.exe"
        $psi.Arguments = "-NoProfile -ExecutionPolicy Bypass -File $quotedScript $argLine"
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError  = $true
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow  = $true

        $proc = [System.Diagnostics.Process]::Start($psi)
        $stdout = $proc.StandardOutput.ReadToEnd()
        $stderr = $proc.StandardError.ReadToEnd()
        $proc.WaitForExit()

        if ($stdout) {
            $logBox.AppendText($stdout + "`r`n")
        }
        if ($stderr) {
            $logBox.AppendText("---- ERROR OUTPUT ----`r`n" + $stderr + "`r`n")
        }

        $logBox.AppendText("Installer exit code: " + $proc.ExitCode + "`r`n")
        $logBox.AppendText("See almondshell_install.log for full details.`r`n")
    })

    [void]$form.ShowDialog()
}

# If no parameters and no raw args -> GUI mode
if ($PSBoundParameters.Count -eq 0 -and $args.Count -eq 0) {
    Start-AlmondShellInstallerGui
    exit 0
}

if ($help) { Help; exit }

# --- ensure winget exists ---
if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
    Log "winget missing. Install App Installer from Microsoft Store." "ERROR"
    exit 1
}

# ============================================
#   DETECTION HELPERS (HYBRID)
# ============================================

function Is-Installed($cmd) {
    return (Get-Command $cmd -ErrorAction SilentlyContinue) -ne $null
}

function Winget-Is-Installed($id) {
    $null = winget list --id $id --source winget 2>$null
    if ($LASTEXITCODE -ne 0) {
        return $false
    }
    return $true
}

function VSCode-Extension-Installed($extId) {
    if (-not (Is-Installed "code")) {
        return $false
    }
    $exts = code --list-extensions 2>$null
    return $exts -match [regex]::Escape($extId)
}

function Invoke-With-Retry {
    param(
        [string]$Label,
        [scriptblock]$Action,
        [int]$MaxAttempts = 3
    )

    for ($i = 1; $i -le $MaxAttempts; $i++) {
        Log "$Label (attempt $i of $MaxAttempts)..." "INFO"
        try {
            & $Action
            if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq $null) {
                Log "$Label succeeded." "OK"
                return
            } else {
                Log "$Label failed with exit code $LASTEXITCODE." "WARN"
            }
        } catch {
            Log "$Label threw exception: $_" "ERROR"
        }
        Start-Sleep -Seconds 2
    }

    Log "$Label FAILED after $MaxAttempts attempts." "ERROR"
    throw "Failed: $Label"
}

# ============================================
#   VERSION + PATH VALIDATION HELPERS
# ============================================

function Get-ToolVersion {
    param([string]$cmd)

    if (-not (Is-Installed $cmd)) { return $null }

    try {
        $output = & $cmd --version 2>$null
    } catch {
        return $null
    }
    if (-not $output) { return $null }

    $first = $output | Select-Object -First 1
    $m = [regex]::Match($first, '(\d+)\.(\d+)\.(\d+)')
    if ($m.Success) {
        return [version]::new([int]$m.Groups[1].Value, [int]$m.Groups[2].Value, [int]$m.Groups[3].Value)
    }
    $m2 = [regex]::Match($first, '(\d+)\.(\d+)')
    if ($m2.Success) {
        return [version]::new([int]$m2.Groups[1].Value, [int]$m2.Groups[2].Value, 0)
    }
    return $null
}

function Check-MinVersion {
    param(
        [string]$cmd,
        [version]$minVersion,
        [string]$name
    )

    $v = Get-ToolVersion $cmd
    if ($null -eq $v) {
        Log "$name version could not be detected." "WARN"
        return
    }

    if ($v -lt $minVersion) {
        Log "$name version $v detected; recommended >= $minVersion" "WARN"
    } else {
        Log "$name version $v OK (>= $minVersion)." "OK"
    }
}

function Validate-PathFor {
    param(
        [string]$cmd,
        [string]$wingetId,
        [string]$name
    )

    if (Is-Installed $cmd) {
        Log "$name is on PATH." "OK"
        return
    }

    if (Winget-Is-Installed $wingetId) {
        Log "$name appears installed but is not on PATH. Open a new terminal or reboot, or add it manually to PATH." "WARN"
    } else {
        Log "$name not detected in PATH or winget registry." "WARN"
    }
}

function Detect-WSL2 {
    if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) { return }

    try {
        $out = wsl.exe -l -v 2>$null
    } catch {
        return
    }
    if ($LASTEXITCODE -ne 0 -or -not $out) { return }

    $hasV2 = $false
    foreach ($line in $out) {
        if ($line -match '\s2\s') {
            $hasV2 = $true
            break
        }
    }

    if ($hasV2) {
        Log "WSL2 detected. You can also install Linux toolchains (Clang/GCC) inside WSL2 for cross-builds." "INFO"
    }
}

# ============================================
#   PORTABLE INSTALL HELPERS (CMake/Ninja)
# ============================================

function Install-CMake-Portable {
    $toolsRoot = Join-Path $PSScriptRoot "tools"
    $cmakeDir  = Join-Path $toolsRoot "cmake"
    $zipPath   = Join-Path $cmakeDir  "cmake.zip"

    New-Item -ItemType Directory -Path $cmakeDir -Force | Out-Null

    $url = "https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-windows-x86_64.zip"

    Invoke-With-Retry "CMake portable download" {
        Invoke-WebRequest -Uri $url -OutFile $zipPath
    }

    Invoke-With-Retry "CMake portable unzip" {
        Expand-Archive -Path $zipPath -DestinationPath $cmakeDir -Force
        $LASTEXITCODE = 0
    }

    Remove-Item $zipPath -Force
    Log "CMake portable installed under: $cmakeDir" "INFO"
    Log "Add the 'bin' subdirectory of the extracted CMake folder to PATH for global use." "INFO"
}

function Install-Ninja-Portable {
    $toolsRoot = Join-Path $PSScriptRoot "tools"
    $ninjaDir  = Join-Path $toolsRoot "ninja"
    $zipPath   = Join-Path $ninjaDir  "ninja.zip"

    New-Item -ItemType Directory -Path $ninjaDir -Force | Out-Null

    $url = "https://github.com/ninja-build/ninja/releases/latest/download/ninja-win.zip"

    Invoke-With-Retry "Ninja portable download" {
        Invoke-WebRequest -Uri $url -OutFile $zipPath
    }

    Invoke-With-Retry "Ninja portable unzip" {
        Expand-Archive -Path $zipPath -DestinationPath $ninjaDir -Force
        $LASTEXITCODE = 0
    }

    Remove-Item $zipPath -Force
    Log "Ninja portable installed under: $ninjaDir" "INFO"
    Log "Add this folder to PATH or call ninja via full path." "INFO"
}

# ============================================
#   INSTALL FUNCTIONS (WITH HYBRID DETECTION)
# ============================================

function Install-CMake {
    if (Is-Installed "cmake" -or (Winget-Is-Installed "Kitware.CMake")) {
        Log "CMake already installed" "OK"
        return
    }

    if ($portable) {
        Log "Installing CMake via portable method (tools/)." "INFO"
        Install-CMake-Portable
        return
    }

    Log "Installing CMake via winget..." "INFO"
    Invoke-With-Retry "CMake (winget)" {
        winget install Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-Ninja {
    if (Is-Installed "ninja" -or (Winget-Is-Installed "Ninja-build.Ninja")) {
        Log "Ninja already installed" "OK"
        return
    }

    if ($portable) {
        Log "Installing Ninja via portable method (tools/)." "INFO"
        Install-Ninja-Portable
        return
    }

    Log "Installing Ninja via winget..." "INFO"
    Invoke-With-Retry "Ninja (winget)" {
        winget install Ninja-build.Ninja --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-Git {
    if (Is-Installed "git" -or (Winget-Is-Installed "Git.Git")) {
        Log "Git already installed" "OK"
        return
    }
    Log "Installing Git via winget..." "INFO"
    Invoke-With-Retry "Git (winget)" {
        winget install Git.Git --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-Python {
    if (Is-Installed "python" -or (Winget-Is-Installed "Python.Python.3.12")) {
        Log "Python already installed" "OK"
        return
    }
    Log "Installing Python via winget..." "INFO"
    Invoke-With-Retry "Python (winget)" {
        winget install Python.Python.3.12 --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-VSCode {
    if (Is-Installed "code" -or (Winget-Is-Installed "Microsoft.VisualStudioCode")) {
        Log "VS Code already installed" "OK"
        return
    }
    Log "Installing VS Code via winget..." "INFO"
    Invoke-With-Retry "VSCode (winget)" {
        winget install Microsoft.VisualStudioCode --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-VSCode-Ext-CPP {
    $extId = "ms-vscode.cpptools"
    if (VSCode-Extension-Installed $extId) {
        Log "VSCode C++ extension already installed" "OK"
        return
    }

    $codeVer = Get-ToolVersion "code"
    if ($codeVer -and $codeVer -lt [version]"1.80.0") {
        Log "VSCode version $codeVer detected; C++ extension works best with >= 1.80.0" "WARN"
    }

    Log "Installing VSCode C++ extension..." "INFO"
    Invoke-With-Retry "VSCode C++ extension" {
        code --install-extension $extId
        $LASTEXITCODE = 0
    }
}

function Install-VSCode-Ext-CMake {
    $extId = "ms-vscode.cmake-tools"
    if (VSCode-Extension-Installed $extId) {
        Log "VSCode CMake Tools extension already installed" "OK"
        return
    }

    $codeVer = Get-ToolVersion "code"
    if ($codeVer -and $codeVer -lt [version]"1.80.0") {
        Log "VSCode version $codeVer detected; CMake Tools works best with >= 1.80.0" "WARN"
    }

    Log "Installing VSCode CMake Tools extension..." "INFO"
    Invoke-With-Retry "VSCode CMake Tools extension" {
        code --install-extension $extId
        $LASTEXITCODE = 0
    }
}

function Install-Clang {
    if (Is-Installed "clang" -or (Winget-Is-Installed "LLVM.LLVM")) {
        Log "Clang already installed" "OK"
        return
    }
    Log "Installing LLVM/Clang via winget..." "INFO"
    Invoke-With-Retry "Clang (winget)" {
        winget install LLVM.LLVM --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-MSVC {
    $vsBuildToolsPath = "C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
    if (Test-Path $vsBuildToolsPath -or (Winget-Is-Installed "Microsoft.VisualStudio.2022.BuildTools")) {
        Log "MSVC Build Tools already installed" "OK"
        return
    }

    Log "Installing Visual Studio 2022 Build Tools via winget..." "INFO"
    Invoke-With-Retry "MSVC Build Tools (winget)" {
        winget install Microsoft.VisualStudio.2022.BuildTools `
            --silent `
            --accept-package-agreements `
            --accept-source-agreements
    }
}

function Install-vcpkg {
    if (Test-Path "./vcpkg") {
        Log "vcpkg already present" "OK"
        return
    }

    if (-not (Is-Installed "git")) {
        Log "Git required for vcpkg but not installed (internal check failed)." "ERROR"
        exit 1
    }

    Log "Cloning vcpkg..." "INFO"
    Invoke-With-Retry "vcpkg clone" {
        git clone https://github.com/microsoft/vcpkg.git
    }

    Log "Bootstrapping vcpkg..." "INFO"
    Invoke-With-Retry "vcpkg bootstrap" {
        ./vcpkg/bootstrap-vcpkg.bat
    }
}

function Install-GLSLang {
    if (Is-Installed "glslangValidator" -or (Winget-Is-Installed "KhronosGroup.GLSLang")) {
        Log "glslangValidator already installed" "OK"
        return
    }
    Log "Installing GLSLang via winget..." "INFO"
    Invoke-With-Retry "GLSLang (winget)" {
        winget install KhronosGroup.GLSLang --silent --accept-package-agreements --accept-source-agreements
    }
}

function Install-VulkanSDK {
    Log "Vulkan SDK is not auto-installed by this script." "INFO"
    Log "Download and install from: https://vulkan.lunarg.com/sdk/home" "INFO"
}

# ============================================
#   BUNDLES
# ============================================

switch ($bundle) {
    "core" {
        $cmake = $true
        $ninja = $true
        $git   = $true
    }
    "compilers" {
        $msvc  = $true
        $clang = $true
    }
    "vscode" {
        $vscode           = $true
        $vscode_ext_cpp   = $true
        $vscode_ext_cmake = $true
    }
    "graphics" {
        $glslang = $true
        $vulkan  = $true
    }
    "full" {
        $cmake = $true
        $ninja = $true
        $git   = $true
        $python = $true
        $vscode = $true
        $vscode_ext_cpp   = $true
        $vscode_ext_cmake = $true
        $clang = $true
        $msvc  = $true
        $vcpkg = $true
        $glslang = $true
        $vulkan  = $true
    }
}

# ============================================
#   DEPENDENCY AUTO-RESOLUTION
# ============================================

# CMake prefers Ninja
if ($cmake -and -not $ninja) {
    Log "CMake selected → enabling Ninja" "INFO"
    $ninja = $true
}

# vcpkg requires Git + CMake (+ Ninja)
if ($vcpkg) {
    if (-not $git) {
        Log "vcpkg requires Git → enabling Git" "INFO"
        $git = $true
    }
    if (-not $cmake) {
        Log "vcpkg requires CMake → enabling CMake" "INFO"
        $cmake = $true
        if (-not $ninja) {
            Log "CMake requires Ninja → enabling Ninja" "INFO"
            $ninja = $true
        }
    }
}

# VSCode C++ extension requires VSCode
if ($vscode_ext_cpp -and -not $vscode) {
    Log "VSCode C++ extension requires VSCode → enabling VSCode" "INFO"
    $vscode = $true
}

# VSCode CMake extension requires VSCode + CMake (+ Ninja)
if ($vscode_ext_cmake) {
    if (-not $vscode) {
        Log "VSCode CMake extension requires VSCode → enabling VSCode" "INFO"
        $vscode = $true
    }
    if (-not $cmake) {
        Log "VSCode CMake extension requires CMake → enabling CMake" "INFO"
        $cmake = $true
        if (-not $ninja) {
            Log "CMake requires Ninja → enabling Ninja" "INFO"
            $ninja = $true
        }
    }
}

# Vulkan SDK recommends glslang
if ($vulkan -and -not $glslang) {
    Log "Vulkan SDK → enabling GLSLang" "INFO"
    $glslang = $true
}

# ============================================
#   EXECUTE INSTALLS
# ============================================

if ($cmake)          { Install-CMake }
if ($ninja)          { Install-Ninja }
if ($git)            { Install-Git }
if ($python)         { Install-Python }
if ($vscode)         { Install-VSCode }
if ($vscode_ext_cpp) { Install-VSCode-Ext-CPP }
if ($vscode_ext_cmake) { Install-VSCode-Ext-CMake }
if ($clang)          { Install-Clang }
if ($msvc)           { Install-MSVC }
if ($vcpkg)          { Install-vcpkg }
if ($glslang)        { Install-GLSLang }
if ($vulkan)         { Install-VulkanSDK }

# ============================================
#   SUMMARY / VALIDATION
# ============================================

Log "Running version checks and PATH validation..." "INFO"

if ($cmake) { Check-MinVersion "cmake" ([version]"3.21.0") "CMake"; Validate-PathFor "cmake" "Kitware.CMake" "CMake" }
if ($ninja) { Check-MinVersion "ninja" ([version]"1.10.0") "Ninja"; Validate-PathFor "ninja" "Ninja-build.Ninja" "Ninja" }
if ($git)   { Check-MinVersion "git"   ([version]"2.30.0") "Git" }
if ($python){ Check-MinVersion "python"([version]"3.10.0") "Python" }
if ($clang) { Check-MinVersion "clang" ([version]"16.0.0") "Clang" }

Detect-WSL2

Log "Installation complete for selected tools. See log file: $LogFile" "INFO"
exit 0
