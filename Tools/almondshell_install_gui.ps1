Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = "AlmondShell Toolchain Installer"
$form.Size = New-Object System.Drawing.Size(520,620)
$form.StartPosition = "CenterScreen"

# Checkbox labels (display → internal key)
$checkboxes = @(
    @{ key="CMake";             text="CMake" },
    @{ key="Ninja";             text="Ninja" },
    @{ key="Git";               text="Git" },
    @{ key="VSCode";            text="VSCode" },
    @{ key="VSCode_CPP_Ext";    text="VSCode C++ Extension" },
    @{ key="VSCode_CMake_Ext";  text="VSCode CMake Tools" },
    @{ key="Clang";             text="Clang / LLVM" },
    @{ key="MSVC";              text="MSVC Build Tools" },
    @{ key="vcpkg";             text="vcpkg" },
    @{ key="GLSLang";           text="GLSLang (SPIR-V)" },
    @{ key="Vulkan";            text="Vulkan SDK (manual)" },
    @{ key="Portable";          text="Portable installs (CMake/Ninja)" },
    @{ key="Python";            text="Python (bonus)" }  # display only
)

# Layout config
$cols      = 3
$colWidth  = 150
$startX    = 20
$startY    = 20
$rowHeight = 26

$checkboxMap = @{}
$index = 0

foreach ($cb in $checkboxes) {

    $col = $index % $cols
    $row = [math]::Floor($index / $cols)

    $x = $startX + ($col * $colWidth)
    $y = $startY + ($row * $rowHeight)

    $ctrl = New-Object System.Windows.Forms.CheckBox
    $ctrl.Text = $cb.text
    $ctrl.Location = New-Object System.Drawing.Point([int]$x, [int]$y)
    $ctrl.AutoSize = $true

    $form.Controls.Add($ctrl)
    $checkboxMap[$cb.key] = $ctrl

    $index++
}

# Compute layout bottom
$finalY = $startY + ([math]::Ceiling($checkboxes.Count / $cols) * $rowHeight)

# Run button
$run = New-Object System.Windows.Forms.Button
$run.Text = "Run Installer"
$run.Location = New-Object System.Drawing.Point(20, ($finalY + 10))
$run.Size = New-Object System.Drawing.Size(450, 40)
$form.Controls.Add($run)

# Log box
$logBox = New-Object System.Windows.Forms.TextBox
$logBox.Multiline = $true
$logBox.ScrollBars = "Vertical"
$logBox.Location = New-Object System.Drawing.Point(20, ($finalY + 60))
$logBox.Size = New-Object System.Drawing.Size(450, 350)
$logBox.Font = New-Object System.Drawing.Font("Consolas", 9)
$form.Controls.Add($logBox)

# ---------------------------------------------
# RUN LOGIC
# ---------------------------------------------
$run.Add_Click({

    $args = @()

    if ($checkboxMap["CMake"].Checked)            { $args += "--cmake" }
    if ($checkboxMap["Ninja"].Checked)            { $args += "--ninja" }
    if ($checkboxMap["Git"].Checked)              { $args += "--git" }
    if ($checkboxMap["VSCode"].Checked)           { $args += "--vscode" }
    if ($checkboxMap["VSCode_CPP_Ext"].Checked)   { $args += "--vscode_ext_cpp" }
    if ($checkboxMap["VSCode_CMake_Ext"].Checked) { $args += "--vscode_ext_cmake" }
    if ($checkboxMap["Clang"].Checked)            { $args += "--clang" }
    if ($checkboxMap["MSVC"].Checked)             { $args += "--msvc" }
    if ($checkboxMap["vcpkg"].Checked)            { $args += "--vcpkg" }
    if ($checkboxMap["GLSLang"].Checked)          { $args += "--glslang" }
    if ($checkboxMap["Vulkan"].Checked)           { $args += "--vulkan" }
    if ($checkboxMap["Portable"].Checked)         { $args += "--portable" }
    if ($checkboxMap["Python"].Checked)           { $args += "--python" }

    if ($args.Count -eq 0) {
        $logBox.AppendText("No tools selected.`r`n")
        return
    }

    # Build absolute script path
    $scriptPath = Join-Path $PSScriptRoot "almondshell_install_build_tools.ps1"
    $argString  = $args -join " "

    $cmd = "& `"$scriptPath`" $argString"
    $logBox.AppendText("Running: $cmd`r`n")

    # Execute installer script
    $output = powershell -NoProfile -ExecutionPolicy Bypass -Command $cmd 2>&1

    # Clean up output newlines for GUI textbox
    $clean = $output -replace "`r?`n", "`r`n"
    $logBox.AppendText($clean + "`r`n")

})

$form.ShowDialog()
