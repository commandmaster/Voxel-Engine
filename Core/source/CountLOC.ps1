# Script to count lines of code for multiple file types in a directory and its subdirectories
param (
    [string]$dir = ".",
    [string[]]$fileTypes = @("*.cpp", "*.hpp", "*.h") # Specify the file types (e.g., *.cs for C#, *.js for JavaScript)
)

# Function to count lines of code in a file
function Count-Lines {
    param ([string]$file)

    $lineCount = 0
    if (Test-Path $file) {
        $lineCount = (Get-Content $file).Length
    }
    return $lineCount
}

$totalLines = 0
$fileTypeResults = @{}

# Process each file type
foreach ($fileType in $fileTypes) {
    $files = Get-ChildItem -Path $directory -Recurse -Include $fileType

    $typeLineCount = 0
    foreach ($file in $files) {
        $linesInFile = Count-Lines $file.FullName
        Write-Host "$($file.FullName): $linesInFile lines"
        $typeLineCount += $linesInFile
    }

    $fileTypeResults[$fileType] = $typeLineCount
    $totalLines += $typeLineCount
}

# Output results for each file type
foreach ($fileType in $fileTypeResults.Keys) {
    Write-Host "Total lines of code in '$fileType' files: $($fileTypeResults[$fileType])"
}

# Output the total lines of code across all file types
Write-Host "Total lines of code in all files: $totalLines"
