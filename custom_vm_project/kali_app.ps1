Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$baseDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$htmlPath = Join-Path $baseDir "kali_ui.html"

if (-not (Test-Path $htmlPath)) {
    [System.Windows.Forms.MessageBox]::Show("kali_ui.html not found in project folder.", "Kali VM App") | Out-Null
    exit 1
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "Kali VM App"
$form.WindowState = [System.Windows.Forms.FormWindowState]::Maximized
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.BackColor = [System.Drawing.Color]::FromArgb(10, 15, 20)

$browser = New-Object System.Windows.Forms.WebBrowser
$browser.Dock = [System.Windows.Forms.DockStyle]::Fill
$browser.ScriptErrorsSuppressed = $true
$browser.AllowWebBrowserDrop = $false
$browser.IsWebBrowserContextMenuEnabled = $false
$browser.WebBrowserShortcutsEnabled = $false
$browser.Navigate($htmlPath)

$form.Controls.Add($browser)
[void]$form.ShowDialog()
