param(
    [string]$InputPath = (Join-Path $PSScriptRoot '..\doc\Modbus RTU Protocol V2.14-1.xlsx'),
    [string]$CachePath = (Join-Path $PSScriptRoot '..\doc\AOHAI_Modbus_RTU_V2.14_English_Translations.json')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedInput)

try {
    function Read-ZipEntryText {
        param([string]$EntryName)

        $entry = $archive.GetEntry($EntryName)
        if ($null -eq $entry) {
            throw "XLSX entry not found: $EntryName"
        }

        $reader = [System.IO.StreamReader]::new($entry.Open())
        try {
            return $reader.ReadToEnd()
        }
        finally {
            $reader.Dispose()
        }
    }

    [xml]$sharedStringsXml = Read-ZipEntryText 'xl/sharedStrings.xml'
    $sharedNs = [System.Xml.XmlNamespaceManager]::new($sharedStringsXml.NameTable)
    $sharedNs.AddNamespace('m', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')
    $sharedStrings = @()
    foreach ($item in $sharedStringsXml.SelectNodes('//m:si', $sharedNs)) {
        $sharedStrings += (($item.SelectNodes('.//m:t', $sharedNs) | ForEach-Object { $_.InnerText }) -join '')
    }

    $texts = [System.Collections.Generic.HashSet[string]]::new()
    $sheetSpecifications = @(
        @('xl/worksheets/sheet3.xml', @('C','D','F','G','I','J','K','L')),
        @('xl/worksheets/sheet4.xml', @('C','D','E','F','G','H'))
    )

    foreach ($specification in $sheetSpecifications) {
        [xml]$sheetXml = Read-ZipEntryText $specification[0]
        $sheetNs = [System.Xml.XmlNamespaceManager]::new($sheetXml.NameTable)
        $sheetNs.AddNamespace('m', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')

        foreach ($row in $sheetXml.SelectNodes('//m:sheetData/m:row', $sheetNs)) {
            $cells = @{}
            foreach ($cell in $row.SelectNodes('./m:c', $sheetNs)) {
                $column = ([regex]::Match($cell.r, '^[A-Z]+')).Value
                $valueNode = $cell.SelectSingleNode('./m:v', $sheetNs)
                if ($cell.t -eq 's' -and $null -ne $valueNode) {
                    $cells[$column] = $sharedStrings[[int]$valueNode.InnerText]
                }
                elseif ($null -ne $valueNode) {
                    $cells[$column] = $valueNode.InnerText
                }
            }

            if ($cells['B'] -match '^\d+$') {
                foreach ($column in $specification[1]) {
                    $sourceText = [string]$cells[$column]
                    if ($sourceText -match '[\p{IsCJKUnifiedIdeographs}]') {
                        [void]$texts.Add($sourceText)
                    }
                }
            }
        }
    }
}
finally {
    $archive.Dispose()
}

$sourceTexts = @($texts | Sort-Object)
$translations = [ordered]@{}
$lineBreakToken = 'ZXBRZX'
$maximumBatchCharacters = 380
$batches = @()
$currentBatch = @()
$currentLength = 0

foreach ($sourceText in $sourceTexts) {
    $singleLine = $sourceText -replace "`r?`n", " $lineBreakToken "
    $additionalLength = $singleLine.Length + $(if ($currentBatch.Count -eq 0) { 0 } else { 1 })
    if ($currentBatch.Count -gt 0 -and
        $currentLength + $additionalLength -gt $maximumBatchCharacters) {
        $batches += ,@($currentBatch)
        $currentBatch = @()
        $currentLength = 0
    }
    $currentBatch += [pscustomobject]@{ Source = $sourceText; SingleLine = $singleLine }
    $currentLength += $singleLine.Length + $(if ($currentBatch.Count -eq 1) { 0 } else { 1 })
}
if ($currentBatch.Count -gt 0) {
    $batches += ,@($currentBatch)
}

for ($batchIndex = 0; $batchIndex -lt $batches.Count; ++$batchIndex) {
    $batch = @($batches[$batchIndex])
    $queryText = ($batch | ForEach-Object { $_.SingleLine }) -join "`n"
    $encodedText = [uri]::EscapeDataString($queryText)
    $uri = "https://api.mymemory.translated.net/get?q=$encodedText&langpair=zh-CN%7Cen"
    Write-Output "Translating batch $($batchIndex + 1)/$($batches.Count) ($($batch.Count) cells)"

    $response = Invoke-RestMethod -Uri $uri -Method Get
    $translatedText = [string]$response.responseData.translatedText
    if ($translatedText -match 'MYMEMORY WARNING') {
        throw $translatedText
    }

    $translatedLines = @($translatedText -split "`r?`n")
    if ($translatedLines.Count -ne $batch.Count) {
        throw "Translation batch line mismatch: sent $($batch.Count), received $($translatedLines.Count)"
    }

    for ($index = 0; $index -lt $batch.Count; ++$index) {
        $translatedValue = $translatedLines[$index].Trim()
        $translatedValue = $translatedValue -replace "\s*$lineBreakToken\s*", "`n"
        $translations[$batch[$index].Source] = $translatedValue
    }

    Start-Sleep -Milliseconds 150
}

$resolvedCache = [System.IO.Path]::GetFullPath($CachePath)
$json = $translations | ConvertTo-Json -Depth 4
[System.IO.File]::WriteAllText($resolvedCache, $json, [System.Text.UTF8Encoding]::new($true))
Write-Output "Created $resolvedCache"
Write-Output "Translated cells: $($translations.Count)"
