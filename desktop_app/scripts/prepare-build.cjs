const { execFileSync } = require('node:child_process');
const path = require('node:path');

const projectRoot = path.resolve(__dirname, '..');
const escapedRoot = projectRoot.replace(/\\/g, '\\\\').replace(/'/g, "''");

const psScript = `
$ErrorActionPreference = 'SilentlyContinue'
$root = '${escapedRoot}'
$targets = Get-CimInstance Win32_Process | Where-Object {
  ($_.Name -ieq 'electron.exe' -or $_.Name -ieq 'node.exe') -and
  $_.CommandLine -and
  $_.CommandLine -like "*$root*"
}

foreach ($proc in $targets) {
  if ($proc.ProcessId -ne $PID) {
    try {
      Stop-Process -Id $proc.ProcessId -Force -ErrorAction Stop
      Write-Output ("Stopped process " + $proc.ProcessId + " (" + $proc.Name + ")")
    } catch {
      Write-Output ("Failed to stop process " + $proc.ProcessId + " (" + $proc.Name + ")")
    }
  }
}
`;

try {
  const output = execFileSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', psScript], {
    cwd: projectRoot,
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
  }).trim();

  if (output) {
    process.stdout.write(output + '\n');
  }
} catch (error) {
  const stderr = error && error.stderr ? String(error.stderr).trim() : '';
  if (stderr) {
    process.stderr.write(stderr + '\n');
  }
}
