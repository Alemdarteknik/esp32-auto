param(
    [string]$InputPath = (Join-Path $PSScriptRoot '..\doc\Modbus RTU Protocol V2.14-1.xlsx'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\doc\AOHAI_Modbus_RTU_V2.14_Register_Map.md'),
    [string]$TranslationCachePath = (Join-Path $PSScriptRoot '..\doc\AOHAI_Modbus_RTU_V2.14_English_Translations.json')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$script:translationCache = @{}
if (Test-Path -LiteralPath $TranslationCachePath) {
    $translationObject = Get-Content -LiteralPath $TranslationCachePath -Raw | ConvertFrom-Json
    foreach ($property in $translationObject.PSObject.Properties) {
        $script:translationCache[$property.Name] = [string]$property.Value
    }
}

function ConvertTo-MarkdownCell {
    param([AllowNull()][object]$Value)

    if ($null -eq $Value) {
        return ''
    }

    return ([string]$Value).Trim().Replace('|', '\|').Replace("`r`n", '<br>').Replace("`n", '<br>').Replace("`r", '<br>')
}

function ConvertTo-EnglishMarkdownCell {
    param([AllowNull()][object]$Value)

    if ($null -eq $Value) {
        return ''
    }

    $textValue = [string]$Value
    if ($textValue -match '[\p{IsCJKUnifiedIdeographs}]') {
        if (!$script:translationCache.ContainsKey($textValue)) {
            throw "Missing English translation for workbook cell: $textValue"
        }
        $textValue = $script:translationCache[$textValue]
    }

    return ConvertTo-MarkdownCell $textValue
}

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

    function Get-WorksheetRows {
        param([string]$EntryName)

        [xml]$sheetXml = Read-ZipEntryText $EntryName
        $sheetNs = [System.Xml.XmlNamespaceManager]::new($sheetXml.NameTable)
        $sheetNs.AddNamespace('m', 'http://schemas.openxmlformats.org/spreadsheetml/2006/main')

        foreach ($row in $sheetXml.SelectNodes('//m:sheetData/m:row', $sheetNs)) {
            $cells = @{}
            foreach ($cell in $row.SelectNodes('./m:c', $sheetNs)) {
                $column = ([regex]::Match($cell.r, '^[A-Z]+')).Value
                $valueNode = $cell.SelectSingleNode('./m:v', $sheetNs)

                if ($cell.t -eq 's' -and $null -ne $valueNode) {
                    $value = $sharedStrings[[int]$valueNode.InnerText]
                }
                elseif ($cell.t -eq 'inlineStr') {
                    $value = ($cell.SelectNodes('.//m:t', $sheetNs) | ForEach-Object { $_.InnerText }) -join ''
                }
                elseif ($null -ne $valueNode) {
                    $value = $valueNode.InnerText
                }
                else {
                    $value = ''
                }

                $cells[$column] = $value
            }

            [pscustomobject]@{
                Number = [int]$row.r
                Cells  = $cells
            }
        }
    }

    $holdingRows = @(Get-WorksheetRows 'xl/worksheets/sheet3.xml' | Where-Object {
        $_.Cells['B'] -match '^\d+$'
    })
    $inputRows = @(Get-WorksheetRows 'xl/worksheets/sheet4.xml' | Where-Object {
        $_.Cells['B'] -match '^\d+$'
    })

    $text = [System.Text.StringBuilder]::new()
    [void]$text.AppendLine('# AOHAI Modbus RTU Protocol V2.14 - Explicit Register Map')
    [void]$text.AppendLine()
    [void]$text.AppendLine('Source workbook: `Modbus RTU Protocol V2.14-1.xlsx`.')
    [void]$text.AppendLine()
    [void]$text.AppendLine('This is an English-only, searchable text export of every numeric holding-register and input-register row in the workbook. Every source table column is preserved, including detailed descriptions, comments, enum explanations and applicable-model notes. The workbook is a general AOHAI external communication protocol; verify model-dependent fields on the FSC-12K1P-BL-G3 before relying on them.')
    [void]$text.AppendLine()
    [void]$text.AppendLine('## Communication')
    [void]$text.AppendLine()
    [void]$text.AppendLine('- Transport: Modbus RTU over RS-485')
    [void]$text.AppendLine('- Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit')
    [void]$text.AppendLine('- Read holding registers: function `0x03`')
    [void]$text.AppendLine('- Read input registers: function `0x04`')
    [void]$text.AppendLine('- Maximum read length: 125 registers per request')
    [void]$text.AppendLine('- Documented minimum command interval: 850 ms')
    [void]$text.AppendLine('- Error check: Modbus CRC-16, transmitted low byte first')
    [void]$text.AppendLine('- This document is informational. Do not issue write functions `0x06` or `0x10` without independently validating the exact FSC model, address and permitted value.')
    [void]$text.AppendLine()
    [void]$text.AppendLine('## FSC-12K live validation and corrections')
    [void]$text.AppendLine()
    [void]$text.AppendLine('| Register | Meaning | FSC-12K status |')
    [void]$text.AppendLine('|---|---|---|')
    [void]$text.AppendLine('| H207 | `DspBeepOnOff`: 0 disabled, 1 enabled | Confirmed by live 1 -> 0 toggle |')
    [void]$text.AppendLine('| R0 | Inverter status | Confirmed |')
    [void]$text.AppendLine('| R1 | Grid-connection countdown in seconds | Corrects earlier mode/code guess |')
    [void]$text.AppendLine('| R2 / R5 | Inverter voltage/current | Confirmed |')
    [void]$text.AppendLine('| R8 | Bus 1 internal voltage | Confirmed |')
    [void]$text.AppendLine('| R10-R12 | Inverter, boost/IPM and LLC temperatures | Confirmed |')
    [void]$text.AppendLine('| R13 | Lead-acid battery NTC temperature | Corrects earlier unknown-field label |')
    [void]$text.AppendLine('| R18 | R-phase DC-current component, signed mA | Corrects earlier dynamic-state guess |')
    [void]$text.AppendLine('| R24-R31 | System fault words 0-7 | Defined by workbook; bit meanings still require validation |')
    [void]$text.AppendLine('| R33 / R35 | Main warning code / warning sub-code | Defined by workbook |')
    [void]$text.AppendLine('| R36 | Device type | Confirmed stable raw value 3008 |')
    [void]$text.AppendLine('| R42-R56 | Grid and EPS voltage/current/frequency/PF area | Strong live correlation |')
    [void]$text.AppendLine('| R63-R67 | PV count, PV1 and PV2 voltage/current | Confirmed |')
    [void]$text.AppendLine('| R96 | Generator R-phase voltage, 0.1 V | Corrects earlier load-percentage guess |')
    [void]$text.AppendLine('| R107:R108 | Total operation time, uint32, 0.5-minute units | Confirmed |')
    [void]$text.AppendLine('| R125-R160 | Battery and BMS telemetry | Addresses strongly aligned; availability depends on battery/BMS |')
    [void]$text.AppendLine('| R250:R251 | Total PV input power, uint32, 0.1 W | Confirmed; high word currently zero |')
    [void]$text.AppendLine('| R252:R253 | PV1 input power, uint32, 0.1 W | Confirmed; high word currently zero |')
    [void]$text.AppendLine('| R254:R255 | PV2 input power, uint32, 0.1 W | Confirmed; high word currently zero |')
    [void]$text.AppendLine('| R344 | EPS output load percentage, scale 0.01 | Actual load-percentage address; not yet read live |')
    [void]$text.AppendLine()
    [void]$text.AppendLine('## Holding registers (`0x03`)')
    [void]$text.AppendLine()
    [void]$text.AppendLine("Exported numeric rows: $($holdingRows.Count)")
    [void]$text.AppendLine()
    [void]$text.AppendLine('| Register Number | Variable Name | Detailed Description | Read/Write | Default | Value Range | Data Type | Unit | Stored | Comments | Applicable Models |')
    [void]$text.AppendLine('|---:|---|---|---|---:|---|---|---|---|---|---|')
    foreach ($row in $holdingRows) {
        $c = $row.Cells
        $address = ConvertTo-MarkdownCell $c['B']
        $variable = ConvertTo-EnglishMarkdownCell $c['C']
        $stored = if ($c['J'] -eq [string][char]0x662F) {
            'Yes'
        } elseif ($c['J'] -eq [string][char]0x5426) {
            'No'
        } else {
            ConvertTo-EnglishMarkdownCell $c['J']
        }
        $values = @(
            $address,
            $variable,
            (ConvertTo-EnglishMarkdownCell $c['D']),
            (ConvertTo-EnglishMarkdownCell $c['E']),
            (ConvertTo-EnglishMarkdownCell $c['F']),
            (ConvertTo-EnglishMarkdownCell $c['G']),
            (ConvertTo-EnglishMarkdownCell $c['H']),
            (ConvertTo-EnglishMarkdownCell $c['I']),
            $stored,
            (ConvertTo-EnglishMarkdownCell $c['K']),
            (ConvertTo-EnglishMarkdownCell $c['L'])
        )
        [void]$text.AppendLine('| H' + ($values -join ' | ') + ' |')
    }

    [void]$text.AppendLine()
    [void]$text.AppendLine('## Input registers (`0x04`)')
    [void]$text.AppendLine()
    [void]$text.AppendLine("Exported numeric rows: $($inputRows.Count)")
    [void]$text.AppendLine()
    [void]$text.AppendLine('| Register Number | Variable Name | Detailed Description | Data Type | Unit | Comments | Applicable Models |')
    [void]$text.AppendLine('|---:|---|---|---|---|---|---|')
    foreach ($row in $inputRows) {
        $c = $row.Cells
        $address = ConvertTo-MarkdownCell $c['B']
        $variable = ConvertTo-EnglishMarkdownCell $c['C']
        $values = @(
            $address,
            $variable,
            (ConvertTo-EnglishMarkdownCell $c['D']),
            (ConvertTo-EnglishMarkdownCell $c['E']),
            (ConvertTo-EnglishMarkdownCell $c['F']),
            (ConvertTo-EnglishMarkdownCell $c['G']),
            (ConvertTo-EnglishMarkdownCell $c['H'])
        )
        [void]$text.AppendLine('| R' + ($values -join ' | ') + ' |')
    }

    $resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
    # Include a UTF-8 BOM for reliable display in Windows PowerShell 5.1 and
    # common Windows editors.
    [System.IO.File]::WriteAllText($resolvedOutput, $text.ToString(), [System.Text.UTF8Encoding]::new($true))
    Write-Output "Created $resolvedOutput"
    Write-Output "Holding registers exported: $($holdingRows.Count)"
    Write-Output "Input registers exported: $($inputRows.Count)"
}
finally {
    $archive.Dispose()
}
