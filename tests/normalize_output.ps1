param(
    [Parameter(Mandatory=$true)]
    [string]$Path
)

(Get-Content -Raw $Path) `
    -replace '[0-9a-fA-F]{8}`[0-9a-fA-F]{8}', '<addr>' `
    -replace '0x[0-9a-fA-F]+', '0x<hex>'
