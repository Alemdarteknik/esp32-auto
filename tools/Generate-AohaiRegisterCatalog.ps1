param(
    [string]$RegisterMap = (Join-Path $PSScriptRoot '..\doc\AOHAI_Modbus_RTU_V2.14_Register_Map.md'),
    [string]$OutputFile = (Join-Path $PSScriptRoot '..\main\src\inverter\aohai_register_catalog.generated.cpp')
)

$ErrorActionPreference = 'Stop'

function Test-InRange {
    param([int]$Value, [int]$First, [int]$Last)
    return $Value -ge $First -and $Value -le $Last
}

function Convert-ToCppString {
    param([AllowEmptyString()][string]$Value)
    if ($null -eq $Value) { $Value = '' }
    $Value = $Value -replace '<br\s*/?>', '; '
    $Value = $Value -replace '&lt;', '<'
    $Value = $Value -replace '&gt;', '>'
    $Value = $Value -replace '&amp;', '&'
    $Value = $Value -replace '[\u2018\u2019]', "'"
    $Value = $Value -replace '[\u201C\u201D]', '"'
    $Value = $Value -replace '[\u2013\u2014]', '-'
    $Value = $Value -replace '\uFF1A', ':'
    $Value = $Value -replace '\uFF0C', ','
    $Value = $Value -replace '\u00B0', ' deg'
    $Value = $Value -replace '\u03A9', 'ohm'
    $Value = $Value -replace '\\', '\\'
    $Value = $Value -replace '"', '\"'
    $Value = $Value -replace "`r|`n", ' '
    return '"' + $Value.Trim() + '"'
}

function Get-Access {
    param([string]$Value, [string]$Space)
    if ($Space -eq 'input') { return 'RegisterAccess::read_only' }
    switch ($Value.Trim().ToUpperInvariant()) {
        'R'   { return 'RegisterAccess::read_only' }
        'W'   { return 'RegisterAccess::write_only' }
        'R/W' { return 'RegisterAccess::read_write' }
        default { return 'RegisterAccess::unknown' }
    }
}

function Get-DataType {
    param([string]$Value)
    switch ($Value.Trim().ToLowerInvariant()) {
        'uint8'        { return 'RegisterDataType::uint8' }
        'uint16'       { return 'RegisterDataType::uint16' }
        'sint16'       { return 'RegisterDataType::sint16' }
        'uint32'       { return 'RegisterDataType::uint32' }
        'uint32_low_word' { return 'RegisterDataType::uint32_low_word' }
        'sint32'       { return 'RegisterDataType::sint32' }
        'sint32_low_word' { return 'RegisterDataType::sint32_low_word' }
        'ascii'        { return 'RegisterDataType::ascii' }
        'ascii/uint16' { return 'RegisterDataType::ascii_or_uint16' }
        default        { return 'RegisterDataType::unknown' }
    }
}

function Get-ScaleLiteral {
    param([string]$Unit)
    if ($Unit.Trim() -match '^([0-9]+(?:\.[0-9]+)?(?:E-[0-9]+)?)') {
        $number = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
        return $number.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
    }
    return '1.0'
}

function Test-ReservedRow {
    param($Row)
    return ([string]::IsNullOrWhiteSpace($Row.Name) -and
            [string]::IsNullOrWhiteSpace($Row.Description)) -or
           ($Row.Name -match '(?i)^reserved$') -or
           ($Row.Description -match '(?i)^reserved$')
}

function Get-ReadClass {
    param($Row)
    $a = $Row.Address
    if (Test-ReservedRow $Row) { return 'RegisterReadClass::reserved' }
    if ($Row.Space -eq 'holding') {
        if ($a -eq 2 -or (Test-InRange $a 5 19) -or (Test-InRange $a 28 64)) {
            return 'RegisterReadClass::boot_identity'
        }
        if ($a -eq 67 -or (Test-InRange $a 234 235)) {
            return 'RegisterReadClass::critical_status'
        }
        return 'RegisterReadClass::setup_snapshot'
    }
    if ((Test-InRange $a 0 1) -or (Test-InRange $a 23 35) -or
        (Test-InRange $a 38 39) -or (Test-InRange $a 130 138) -or $a -eq 167) {
        return 'RegisterReadClass::critical_status'
    }
    if ($a -eq 36 -or $a -eq 145 -or (Test-InRange $a 147 149) -or
        (Test-InRange $a 170 171) -or (Test-InRange $a 450 455)) {
        return 'RegisterReadClass::boot_identity'
    }
    if ((Test-InRange $a 10 22) -or $a -eq 106 -or $a -eq 146 -or
        $a -eq 150 -or (Test-InRange $a 153 166) -or (Test-InRange $a 168 169)) {
        return 'RegisterReadClass::live_health'
    }
    if ((Test-InRange $a 107 108) -or (Test-InRange $a 151 152) -or
        (Test-InRange $a 172 175) -or (Test-InRange $a 375 430) -or
        (Test-InRange $a 466 489) -or (Test-InRange $a 491 498)) {
        return 'RegisterReadClass::slow_counter'
    }
    if ((Test-InRange $a 109 124) -or (Test-InRange $a 186 195)) {
        return 'RegisterReadClass::diagnostic'
    }
    return 'RegisterReadClass::live_flow'
}

function Get-WriteGuard {
    param($Row)
    if ($Row.Space -eq 'input') { return 'RegisterWriteGuard::read_only' }
    if (Test-ReservedRow $Row) { return 'RegisterWriteGuard::blocked_unvalidated' }
    if ($Row.Access -eq 'R') { return 'RegisterWriteGuard::read_only' }
    $a = $Row.Address
    if ([string]::IsNullOrWhiteSpace($Row.Access)) { return 'RegisterWriteGuard::blocked_unvalidated' }
    if (Test-InRange $a 234 235) { return 'RegisterWriteGuard::read_only' }
    if ($a -eq 207 -or $a -eq 231) { return 'RegisterWriteGuard::hot_edit_confirmed' }
    if ($a -eq 0 -or $a -eq 147) { return 'RegisterWriteGuard::runtime_command' }
    if ((Test-InRange $a 21 27) -or (Test-InRange $a 144 146) -or
        (Test-InRange $a 150 182) -or $a -eq 201 -or $a -eq 208 -or
        $a -eq 215 -or (Test-InRange $a 219 223)) {
        return 'RegisterWriteGuard::hot_edit_guarded'
    }
    if ((Test-InRange $a 106 110) -or (Test-InRange $a 125 141) -or
        $a -eq 183 -or $a -eq 185 -or (Test-InRange $a 187 199) -or
        (Test-InRange $a 202 206) -or (Test-InRange $a 209 214) -or
        (Test-InRange $a 216 218)) {
        return 'RegisterWriteGuard::standby_required'
    }
    if ((Test-InRange $a 3 4) -or $a -eq 20 -or (Test-InRange $a 68 95) -or
        (Test-InRange $a 100 104) -or $a -eq 112 -or
        (Test-InRange $a 121 124) -or (Test-InRange $a 250 280)) {
        return 'RegisterWriteGuard::commissioning_only'
    }
    if ((Test-InRange $a 5 19) -or $a -eq 57 -or (Test-InRange $a 59 62) -or
        (Test-InRange $a 65 66) -or (Test-InRange $a 113 116) -or
        $a -eq 200 -or (Test-InRange $a 232 233)) {
        return 'RegisterWriteGuard::service_only'
    }
    return 'RegisterWriteGuard::blocked_unvalidated'
}

function Get-Domain {
    param($Row)
    $a = $Row.Address
    if (Test-ReservedRow $Row) { return 'RegisterDomain::reserved' }
    if ($Row.Space -eq 'holding') {
        if ($a -eq 2 -or (Test-InRange $a 5 19) -or (Test-InRange $a 28 64)) { return 'RegisterDomain::identity' }
        if ((Test-InRange $a 3 4) -or $a -eq 20 -or (Test-InRange $a 232 235)) { return 'RegisterDomain::communications' }
        if ((Test-InRange $a 69 95) -or (Test-InRange $a 101 104) -or
            (Test-InRange $a 121 124) -or (Test-InRange $a 250 280)) { return 'RegisterDomain::grid' }
        if (Test-InRange $a 106 112) { return 'RegisterDomain::eps' }
        if ($a -eq 126 -or $a -eq 214) { return 'RegisterDomain::bms' }
        if ((Test-InRange $a 125 143) -or (Test-InRange $a 183 200) -or
            (Test-InRange $a 202 206) -or $a -eq 210) { return 'RegisterDomain::battery' }
        if ((Test-InRange $a 144 182) -or $a -eq 201 -or $a -eq 207 -or $a -eq 231) { return 'RegisterDomain::system' }
        if ($a -eq 208) { return 'RegisterDomain::eps' }
        if ($a -eq 209 -or $a -eq 215) { return 'RegisterDomain::grid' }
        if (Test-InRange $a 211 213) { return 'RegisterDomain::parallel' }
        if (Test-InRange $a 216 223) { return 'RegisterDomain::generator' }
        return 'RegisterDomain::system'
    }
    if (Test-InRange $a 0 39) { return 'RegisterDomain::inverter' }
    if (Test-InRange $a 40 53) { return 'RegisterDomain::grid' }
    if (Test-InRange $a 54 60) { return 'RegisterDomain::eps' }
    if ((Test-InRange $a 63 95) -or (Test-InRange $a 250 283)) { return 'RegisterDomain::pv' }
    if ((Test-InRange $a 96 102) -or (Test-InRange $a 357 368)) { return 'RegisterDomain::generator' }
    if ((Test-InRange $a 109 124) -or (Test-InRange $a 186 195)) { return 'RegisterDomain::diagnostics' }
    if (Test-InRange $a 125 175) {
        if (Test-InRange $a 130 170) { return 'RegisterDomain::bms' }
        return 'RegisterDomain::battery'
    }
    if (Test-InRange $a 284 313) { return 'RegisterDomain::power' }
    if (Test-InRange $a 314 331) { return 'RegisterDomain::load' }
    if (Test-InRange $a 332 356) { return 'RegisterDomain::power' }
    if (Test-InRange $a 375 430) { return 'RegisterDomain::energy' }
    if (Test-InRange $a 450 498) { return 'RegisterDomain::parallel' }
    return 'RegisterDomain::system'
}

function Get-FeatureFlags {
    param($Row)
    $a = $Row.Address
    $flags = @()
    if ($Row.Space -eq 'input') {
        if ($a -in @(3,4,6,7,44,45,46,47,48,49,50,57,58,59,60) -or
            (Test-InRange $a 296 307) -or (Test-InRange $a 316 319) -or
            (Test-InRange $a 324 327) -or (Test-InRange $a 336 343) -or
            (Test-InRange $a 359 368)) { $flags += 'feature_three_phase' }
        if ((Test-InRange $a 68 95) -or (Test-InRange $a 256 283)) { $flags += 'feature_extended_pv' }
        if ((Test-InRange $a 96 102) -or (Test-InRange $a 357 368)) { $flags += 'feature_generator' }
        if (Test-InRange $a 125 175) { $flags += 'feature_battery' }
        if (Test-InRange $a 130 170) { $flags += 'feature_bms' }
        if (Test-InRange $a 450 498) { $flags += 'feature_parallel' }
    } else {
        if ((Test-InRange $a 125 146) -or (Test-InRange $a 183 200) -or
            (Test-InRange $a 202 206) -or $a -eq 210 -or (Test-InRange $a 219 223)) { $flags += 'feature_battery' }
        if ($a -eq 126 -or $a -eq 214) { $flags += 'feature_bms' }
        if (Test-InRange $a 211 213) { $flags += 'feature_parallel' }
        if (Test-InRange $a 216 223) { $flags += 'feature_generator' }
        if ((Test-InRange $a 65 67) -or (Test-InRange $a 113 116) -or
            $a -eq 200 -or (Test-InRange $a 232 233)) { $flags += 'feature_service' }
    }
    if ($flags.Count -eq 0) { return 'feature_none' }
    return ($flags -join ' | ')
}

function Read-RegisterRows {
    param([string]$Path)
    $section = ''
    $rows = [System.Collections.Generic.List[object]]::new()
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^## Holding registers') { $section = 'holding'; continue }
        if ($line -match '^## Input registers') { $section = 'input'; continue }
        if ($section -eq 'holding' -and $line -match '^\| H(\d+) \|') {
            $address = [int]$Matches[1]
            $c = @($line.Trim('|').Split('|') | ForEach-Object { $_.Trim() })
            if ($c.Count -ge 11) {
                $rows.Add([pscustomobject]@{
                    Space='holding'; Address=$address; Name=$c[1]; Description=$c[2]
                    Access=$c[3]; Default=$c[4]; Range=$c[5]; Type=$c[6]
                    Unit=$c[7]; Stored=$c[8]; Notes=$c[9]; Models=$c[10]
                })
            }
        } elseif ($section -eq 'input' -and $line -match '^\| R(\d+) \|') {
            $address = [int]$Matches[1]
            $c = @($line.Trim('|').Split('|') | ForEach-Object { $_.Trim() })
            if ($c.Count -ge 7) {
                $rows.Add([pscustomobject]@{
                    Space='input'; Address=$address; Name=$c[1]; Description=$c[2]
                    Access='R'; Default=''; Range=''; Type=$c[3]
                    Unit=$c[4]; Stored='No'; Notes=$c[5]; Models=$c[6]
                })
            }
        }
    }
    return $rows
}

if (-not (Test-Path -LiteralPath $RegisterMap)) {
    throw "Register map not found: $RegisterMap"
}

$rows = @(Read-RegisterRows $RegisterMap)
$holding = @($rows | Where-Object Space -eq 'holding' | Sort-Object Address)
$inputRows = @($rows | Where-Object Space -eq 'input' | Sort-Object Address)

# The workbook places a 32-bit type/unit on the high-word row and commonly
# leaves the adjacent low-word row blank. Make that relationship explicit in
# the embedded catalog so consumers never decode the low word independently.
foreach ($groupRows in @($holding, $inputRows)) {
    for ($index = 1; $index -lt $groupRows.Count; $index++) {
        $previous = $groupRows[$index - 1]
        $current = $groupRows[$index]
        if ($current.Address -ne $previous.Address + 1 -or
            -not [string]::IsNullOrWhiteSpace($current.Type)) {
            continue
        }
        if ($previous.Type -eq 'uint32') {
            $current.Type = 'uint32_low_word'
            if ([string]::IsNullOrWhiteSpace($current.Unit)) { $current.Unit = $previous.Unit }
        } elseif ($previous.Type -eq 'sint32') {
            $current.Type = 'sint32_low_word'
            if ([string]::IsNullOrWhiteSpace($current.Unit)) { $current.Unit = $previous.Unit }
        }
    }
}

if ($holding.Count -ne 265 -or $inputRows.Count -ne 422) {
    throw "Unexpected source counts: holding=$($holding.Count), input=$($inputRows.Count)"
}
$uniqueHoldingCount = ($holding | Select-Object -ExpandProperty Address -Unique |
    Measure-Object).Count
$uniqueInputCount = ($inputRows | Select-Object -ExpandProperty Address -Unique |
    Measure-Object).Count
if ($uniqueHoldingCount -ne $holding.Count -or
    $uniqueInputCount -ne $inputRows.Count) {
    throw 'Duplicate register addresses were found in the explicit map sections.'
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('// Generated by tools/Generate-AohaiRegisterCatalog.ps1. Do not edit by hand.')
$lines.Add('#include "inverter_gateway/inverter/register_catalog.hpp"')
$lines.Add('')
$lines.Add('namespace inverter_gateway::inverter {')
$lines.Add('namespace {')
$lines.Add('')

foreach ($group in @(@{Name='holding_registers'; Rows=$holding}, @{Name='input_registers'; Rows=$inputRows})) {
    $lines.Add("constexpr RegisterDescriptor $($group.Name)[] = {")
    foreach ($row in $group.Rows) {
        $space = if ($row.Space -eq 'holding') { 'RegisterSpace::holding' } else { 'RegisterSpace::input' }
        $stored = if ($row.Stored -match '(?i)^yes$') { 'true' } else { 'false' }
        $entry = '    {' + ($space, $row.Address,
            (Convert-ToCppString $row.Name), (Convert-ToCppString $row.Description),
            (Get-Access $row.Access $row.Space), (Get-DataType $row.Type),
            (Convert-ToCppString $row.Unit), (Get-ScaleLiteral $row.Unit), (Get-ReadClass $row),
            (Get-WriteGuard $row), (Get-Domain $row), (Get-FeatureFlags $row),
            (Convert-ToCppString $row.Default), (Convert-ToCppString $row.Range),
            $stored, (Convert-ToCppString $row.Notes),
            (Convert-ToCppString $row.Models) -join ', ') + '},'
        $lines.Add($entry)
    }
    $lines.Add('};')
    $expectedCount = if ($group.Name -eq 'holding_registers') {
        'documented_holding_register_count'
    } else {
        'documented_input_register_count'
    }
    $lines.Add("static_assert(sizeof($($group.Name)) / sizeof($($group.Name)[0]) == $expectedCount);")
    $lines.Add('')
}

$lines.Add('} // namespace')
$lines.Add('')
$lines.Add('RegisterCatalogView holding_register_catalog()')
$lines.Add('{')
$lines.Add('    return {holding_registers, sizeof(holding_registers) / sizeof(holding_registers[0])};')
$lines.Add('}')
$lines.Add('')
$lines.Add('RegisterCatalogView input_register_catalog()')
$lines.Add('{')
$lines.Add('    return {input_registers, sizeof(input_registers) / sizeof(input_registers[0])};')
$lines.Add('}')
$lines.Add('')
$lines.Add('const RegisterDescriptor *find_register(RegisterSpace space, std::uint16_t address)')
$lines.Add('{')
$lines.Add('    const RegisterCatalogView view = space == RegisterSpace::holding')
$lines.Add('                                         ? holding_register_catalog()')
$lines.Add('                                         : input_register_catalog();')
$lines.Add('    std::size_t left = 0;')
$lines.Add('    std::size_t right = view.size;')
$lines.Add('    while (left < right) {')
$lines.Add('        const std::size_t middle = left + (right - left) / 2;')
$lines.Add('        if (view.data[middle].address < address) left = middle + 1;')
$lines.Add('        else right = middle;')
$lines.Add('    }')
$lines.Add('    return left < view.size && view.data[left].address == address ? &view.data[left] : nullptr;')
$lines.Add('}')
$lines.Add('')
$lines.Add('const char *read_class_name(RegisterReadClass value)')
$lines.Add('{')
$lines.Add('    switch (value) {')
foreach ($name in @('boot_identity','setup_snapshot','critical_status','live_flow','live_health','slow_counter','diagnostic','reserved')) {
    $lines.Add("    case RegisterReadClass::$name`: return `"$($name.ToUpperInvariant())`";")
}
$lines.Add('    default: return "UNKNOWN";')
$lines.Add('    }')
$lines.Add('}')
$lines.Add('')
$lines.Add('const char *write_guard_name(RegisterWriteGuard value)')
$lines.Add('{')
$lines.Add('    switch (value) {')
foreach ($name in @('read_only','hot_edit_confirmed','hot_edit_guarded','runtime_command','standby_required','commissioning_only','service_only','blocked_unvalidated')) {
    $lines.Add("    case RegisterWriteGuard::$name`: return `"$($name.ToUpperInvariant())`";")
}
$lines.Add('    default: return "UNKNOWN";')
$lines.Add('    }')
$lines.Add('}')
$lines.Add('')
$lines.Add('const char *register_domain_name(RegisterDomain value)')
$lines.Add('{')
$lines.Add('    switch (value) {')
foreach ($name in @('identity','communications','system','inverter','grid','eps','pv','generator','battery','bms','load','power','energy','parallel','diagnostics','reserved')) {
    $lines.Add("    case RegisterDomain::$name`: return `"$($name.ToUpperInvariant())`";")
}
$lines.Add('    default: return "UNKNOWN";')
$lines.Add('    }')
$lines.Add('}')
$lines.Add('')
$lines.Add('} // namespace inverter_gateway::inverter')

$outputDirectory = Split-Path -Parent $OutputFile
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}
[System.IO.File]::WriteAllLines($OutputFile, $lines, [System.Text.UTF8Encoding]::new($false))
Write-Host "Generated $OutputFile"
Write-Host "Holding descriptors: $($holding.Count)"
Write-Host "Input descriptors: $($inputRows.Count)"
