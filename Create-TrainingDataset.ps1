# PowerShell script to create optimized training datasets
param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("1000", "2000", "5000")]
    [int]$Sequences = 2000
)

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Creating Optimized Training Dataset" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# Check if source file exists
if (-not (Test-Path "wiki_training_combined.txt")) {
    Write-Host "ERROR: wiki_training_combined.txt not found!" -ForegroundColor Red
    exit 1
}

# Determine output filename
$filename = "wiki_${Sequences}.txt"

Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  Source: wiki_training_combined.txt"
Write-Host "  Output: $filename"
Write-Host "  Sequences: $Sequences"
Write-Host ""

# Create dataset
Write-Host "Creating dataset..." -ForegroundColor Green
Get-Content wiki_training_combined.txt -TotalCount $Sequences | Out-File -Encoding UTF8 $filename

if (Test-Path $filename) {
    $lines = (Get-Content $filename).Count
    Write-Host "✓ Success! Created $filename ($lines lines)" -ForegroundColor Green
    Write-Host ""

    # Suggest training command
    switch ($Sequences) {
        1000 {
            $epochs = 5
            $lr = 0.002
            $batch = 32
            $time = "30 minutes"
        }
        2000 {
            $epochs = 5
            $lr = 0.001
            $batch = 32
            $time = "2 hours"
        }
        5000 {
            $epochs = 5
            $lr = 0.0005
            $batch = 32
            $time = "4 hours"
        }
    }

    Write-Host "Recommended training command:" -ForegroundColor Yellow
    Write-Host "  ./bin/neural_engine.exe train_transformer $filename $epochs $lr $batch" -ForegroundColor White
    Write-Host ""
    Write-Host "Expected training time: $time" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host "ERROR: Failed to create dataset" -ForegroundColor Red
    exit 1
}
