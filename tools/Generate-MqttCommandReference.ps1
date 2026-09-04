param(
    [string]$CatalogSource = (Join-Path $PSScriptRoot '..\main\src\inverter\aohai_register_catalog.generated.cpp'),
    [string]$ApiDocument = (Join-Path $PSScriptRoot '..\doc\MQTT_Register_Command_API.md')
)

$ErrorActionPreference = 'Stop'
$beginMarker = '<!-- BEGIN GENERATED COMPLETE COMMAND CATALOG -->'
$endMarker = '<!-- END GENERATED COMPLETE COMMAND CATALOG -->'
$pattern = '^\s*\{RegisterSpace::(holding|input), (\d+), "((?:\\.|[^"])*)", "((?:\\.|[^"])*)", RegisterAccess::([a-z_]+), RegisterDataType::([a-z0-9_]+), "((?:\\.|[^"])*)", ([^,]+), RegisterReadClass::([a-z_]+), RegisterWriteGuard::([a-z_]+), RegisterDomain::([a-z_]+), ([^,]+), "((?:\\.|[^"])*)", "((?:\\.|[^"])*)", (true|false), "((?:\\.|[^"])*)", "((?:\\.|[^"])*)"\},$'

function Convert-FromCppString {
    param([string]$Value)
    $result = $Value.Replace('\"', '"').Replace('\\', '\')
    return [regex]::Replace($result, '[^\x00-\x7F]+', ', ')
}

function Escape-Markdown {
    param([AllowEmptyString()][string]$Value)
    if ($null -eq $Value) { return '' }
    return $Value.Replace('|', '\|').Replace("`r", ' ').Replace("`n", ' ')
}

function Convert-ToCompactJson {
    param([System.Collections.IDictionary]$Value)
    return ($Value | ConvertTo-Json -Compress -Depth 8).Replace('\u003c', '<').Replace('\u003e', '>')
}

function Get-Selector {
    param($Row, $Rows)
    if ([string]::IsNullOrWhiteSpace($Row.Name) -or $Row.Name -eq 'Reserved') { return $null }
    $sameName = @($Rows | Where-Object { $_.Space -eq $Row.Space -and $_.Name -eq $Row.Name })
    $selector = [ordered]@{ name = $Row.Name }
    if ($sameName.Count -gt 1) {
        $selector.domain = $Row.Domain
        $sameDomain = @($sameName | Where-Object Domain -eq $Row.Domain)
        if ($sameDomain.Count -gt 1 -and -not [string]::IsNullOrWhiteSpace($Row.Description)) {
            $selector.description = $Row.Description
            if (@($sameDomain | Where-Object Description -eq $Row.Description).Count -gt 1) { return $null }
        } elseif ($sameDomain.Count -gt 1) {
            return $null
        }
    }
    return $selector
}

function Get-ReadCommand {
    param($Row, $Rows)
    $topic = '<prefix>/<site-id>/inverter/<member-id>/command'
    $category = if ($Row.Space -eq 'holding') { 'configuration' } else { 'input_data' }
    $payload = [ordered]@{ command_id = '<unique-command-id>'; operation = 'get'; category = $category }
    $selector = Get-Selector $Row $Rows
    if ($null -eq $selector) { throw "No public selector for $($Row.Space) $($Row.Address)" }
    foreach ($key in $selector.Keys) { $payload[$key] = $selector[$key] }
    $json = Convert-ToCompactJson $payload
    return "mosquitto_pub -h `"<broker>`" -q 1 -t `"$topic`" -m '$json'"
}

function Get-WriteCommand {
    param($Row, $Rows, [AllowNull()]$ApplicationValue = $null)
    $topic = '<prefix>/<site-id>/inverter/<member-id>/command'
    $payload = [ordered]@{ command_id = '<unique-command-id>'; operation = 'set' }
    $selector = Get-Selector $Row $Rows
    if ($null -eq $selector) { throw "No public selector for holding $($Row.Address)" }
    foreach ($key in $selector.Keys) { $payload[$key] = $selector[$key] }
    # String tokens deliberately make an unchanged generic setter fail validation.
    $payload.value = if ($null -eq $ApplicationValue) { '<new-value>' } else { $ApplicationValue }
    $payload.confirmed = $true
    if ($Row.Guard -eq 'hot_edit_guarded') { $payload.guarded_interlock = $true }
    if ($Row.Guard -eq 'commissioning_only') { $payload.commissioning_interlock = $true }
    if ($Row.Guard -eq 'service_only') { $payload.service_interlock = $true }
    $json = Convert-ToCompactJson $payload
    return "mosquitto_pub -h `"<broker>`" -q 1 -t `"$topic`" -m '$json'"
}

function Get-DocumentedOptions {
    param($Row)
    if ([string]::IsNullOrWhiteSpace($Row.Notes)) { return @() }
    $found = [System.Collections.Generic.List[object]]::new()
    $seen = @{}
    $matches = [regex]::Matches(
        $Row.Notes,
        '(?i)(?:^|;\s*)(0x[0-9a-f]+|\d+)\s*:\s*([^;]+)')
    foreach ($match in $matches) {
        $token = $match.Groups[1].Value
        $number = if ($token.StartsWith('0x', [System.StringComparison]::OrdinalIgnoreCase)) {
            [Convert]::ToInt64($token.Substring(2), 16)
        } else {
            [int64]$token
        }
        if ($seen.ContainsKey($number)) { continue }
        $seen[$number] = $true
        $found.Add([pscustomobject]@{
            Value = $number
            Label = $match.Groups[2].Value.Trim()
        })
    }
    return @($found)
}

function Get-ApplicationOptionValue {
    param($Option, $AllOptions)
    if ($AllOptions.Count -eq 2) {
        $zero = $AllOptions | Where-Object Value -eq 0 | Select-Object -First 1
        $one = $AllOptions | Where-Object Value -eq 1 | Select-Object -First 1
        if ($null -ne $zero -and $null -ne $one -and
            $zero.Label -match '(?i)\b(disable(d)?|off)\b' -and
            $one.Label -match '(?i)\b(enable(d)?|on)\b') {
            return [bool]($Option.Value -eq 1)
        }
    }
    return [int64]$Option.Value
}

if (-not (Test-Path -LiteralPath $CatalogSource)) { throw "Catalog source not found: $CatalogSource" }
if (-not (Test-Path -LiteralPath $ApiDocument)) { throw "API document not found: $ApiDocument" }

$rows = [System.Collections.Generic.List[object]]::new()
foreach ($line in Get-Content -LiteralPath $CatalogSource) {
    if ($line -notmatch $pattern) { continue }
    $rows.Add([pscustomobject]@{
        Space = $Matches[1]
        Address = [int]$Matches[2]
        Name = Convert-FromCppString $Matches[3]
        Description = Convert-FromCppString $Matches[4]
        Access = $Matches[5]
        DataType = $Matches[6]
        Unit = Convert-FromCppString $Matches[7]
        Scale = $Matches[8]
        ReadClass = $Matches[9]
        Guard = $Matches[10]
        Domain = $Matches[11]
        Features = $Matches[12]
        Default = Convert-FromCppString $Matches[13]
        Range = Convert-FromCppString $Matches[14]
        Stored = $Matches[15]
        Notes = Convert-FromCppString $Matches[16]
        Models = Convert-FromCppString $Matches[17]
    })
}
if ($rows.Count -ne 687) { throw "Expected 687 register descriptors; parsed $($rows.Count)" }

$output = [System.Collections.Generic.List[string]]::new()
$output.Add($beginMarker)
$output.Add('## Complete generated Mosquitto command catalog')
$output.Add('')
$output.Add('This section is generated from the internal catalog. Replace `<broker>`, `<prefix>`, `<site-id>`, and `<member-id>`. Every displayed `command_id` is illustrative and must be replaced by a fresh globally unique positive ID before each publication. Replace `<new-value>` with the intended application-level JSON value: a boolean, an engineering-unit number, a documented mode number, or a one/two-character string as appropriate. MQTT clients never send Modbus addresses, register spaces, raw words, scaling values, or expected register values.')
$output.Add('')
$output.Add('Add `-u "<username>" -P "<password>"` when authentication is enabled. Subscribe to command results before publishing. Use the `live`, `alert/#`, and `snapshot/#` subscriptions documented above for asynchronous data:')
$output.Add('')
$output.Add('```bash')
$output.Add('mosquitto_sub -h "<broker>" -q 1 -t "<prefix>/<site-id>/inverter/<member-id>/result"')
$output.Add('```')
$output.Add('')
$output.Add('### GETTERS - read data from the inverter')
$output.Add('')
$output.Add('These public commands contain only an application category and catalog data name. Modbus address, function, scale conversion and word layout remain inside the ESP firmware.')
$output.Add('')
$output.Add('#### Configuration getters (current setup, identity and command state)')
$output.Add('')
$output.Add('| Name | Description | Domain | Type/unit | Read class | Mosquitto getter command |')
$output.Add('|---|---|---|---|---|---|')
$holdingGetters = @($rows | Where-Object {
    $_.Space -eq 'holding' -and $_.Access -ne 'write_only' -and
    $_.DataType -notin @('uint32_low_word', 'sint32_low_word') -and
    $null -ne (Get-Selector $_ $rows)
})
foreach ($row in $holdingGetters) {
    $command = Get-ReadCommand $row $rows
    $typeUnit = if ([string]::IsNullOrWhiteSpace($row.Unit)) { $row.DataType } else { "$($row.DataType); $($row.Unit)" }
    $output.Add("| $(Escape-Markdown $row.Name) | $(Escape-Markdown $row.Description) | $($row.Domain) | $(Escape-Markdown $typeUnit) | $($row.ReadClass) | ``$command`` |")
}
$output.Add('')
$output.Add('#### Input-register getters (status, measurements, power, energy and faults)')
$output.Add('')
$output.Add('| Name | Description | Domain | Type/unit | Read class | Mosquitto getter command |')
$output.Add('|---|---|---|---|---|---|')
$inputGetters = @($rows | Where-Object {
    $_.Space -eq 'input' -and
    $_.DataType -notin @('uint32_low_word', 'sint32_low_word') -and
    $null -ne (Get-Selector $_ $rows)
})
foreach ($row in $inputGetters) {
    $command = Get-ReadCommand $row $rows
    $typeUnit = if ([string]::IsNullOrWhiteSpace($row.Unit)) { $row.DataType } else { "$($row.DataType); $($row.Unit)" }
    $output.Add("| $(Escape-Markdown $row.Name) | $(Escape-Markdown $row.Description) | $($row.Domain) | $(Escape-Markdown $typeUnit) | $($row.ReadClass) | ``$command`` |")
}

$settable = @($rows | Where-Object {
    $_.Space -eq 'holding' -and $_.Access -in @('read_write', 'write_only') -and
    $_.Guard -notin @('read_only', 'blocked_unvalidated') -and
    $_.DataType -notin @('uint32_low_word', 'sint32_low_word') -and
    $null -ne (Get-Selector $_ $rows)
})
$blocked = @($rows | Where-Object {
    $_.Space -eq 'holding' -and $_.Access -in @('read_write', 'write_only') -and
    $_.Guard -in @('read_only', 'blocked_unvalidated')
})
$output.Add('')
$output.Add("### SETTERS - write settings or commands to the inverter ($($settable.Count))")
$output.Add('')
$output.Add('These commands reach the common setter only after MQTT-origin validation. The ESP resolves the name, converts the application value, reads the current inverter value internally, checks operating state and interlocks, writes the complete value, and verifies it by read-back. Never retry a `service_only` command automatically.')
$output.Add('')
$output.Add('| Setter guard | Count | Required behavior |')
$output.Add('|---|---:|---|')
$guardDescriptions = [ordered]@{
    hot_edit_confirmed = 'Confirmed internal read-before-write and read-back.'
    hot_edit_guarded = 'Also requires guarded_interlock and validated dependencies.'
    runtime_command = 'Fresh valid runtime state; transition/status monitoring.'
    standby_required = 'Fresh standby or stopped state.'
    commissioning_only = 'Fresh standby/stopped state and commissioning_interlock.'
    service_only = 'Fresh standby/stopped state and service_interlock; never auto-retry.'
}
foreach ($guard in $guardDescriptions.Keys) {
    $count = @($settable | Where-Object Guard -eq $guard).Count
    $output.Add("| $guard | $count | $($guardDescriptions[$guard]) |")
}
$output.Add('')
$output.Add('| Setting | Description | Domain | Application value type/unit | Guard | Documented meaning | Mosquitto setter command template |')
$output.Add('|---|---|---|---|---|---|---|')
foreach ($row in $settable) {
    $typeUnit = if ([string]::IsNullOrWhiteSpace($row.Unit) -or $row.Unit -eq '-') { $row.DataType } else { "$($row.DataType); $($row.Unit)" }
    $meaning = if ([string]::IsNullOrWhiteSpace($row.Notes)) { $row.Description } else { $row.Notes }
    $output.Add("| $(Escape-Markdown $row.Name) | $(Escape-Markdown $row.Description) | $($row.Domain) | $(Escape-Markdown $typeUnit) | $($row.Guard) | $(Escape-Markdown $meaning) | ``$(Get-WriteCommand $row $rows)`` |")
}

$enumeratedSetters = @($settable | ForEach-Object {
    $options = @(Get-DocumentedOptions $_)
    if ($options.Count -ge 2) {
        [pscustomobject]@{ Row = $_; Options = $options }
    }
})
$optionCommandCount = 0
$output.Add('')
$output.Add("### Enumerated setter choices ($($enumeratedSetters.Count) settings)")
$output.Add('')
$output.Add('Each row below is a complete command illustration for one documented choice. Use a fresh numeric `command_id` for every publication. Boolean enable/disable choices use `true` and `false`; other choices use their documented application mode number. These are requested settings, not Modbus addresses or raw register words.')
foreach ($entry in $enumeratedSetters) {
    $row = $entry.Row
    $options = @($entry.Options)
    $output.Add('')
    $output.Add("#### $(Escape-Markdown $row.Name) - $(Escape-Markdown $row.Description)")
    $output.Add('')
    $output.Add('| Choice | Application value | Mosquitto command |')
    $output.Add('|---|---:|---|')
    foreach ($option in $options) {
        $applicationValue = Get-ApplicationOptionValue $option $options
        $displayValue = if ($applicationValue -is [bool]) { $applicationValue.ToString().ToLowerInvariant() } else { "$applicationValue" }
        $command = Get-WriteCommand $row $rows $applicationValue
        $output.Add("| $(Escape-Markdown $option.Label) | ``$displayValue`` | ``$command`` |")
        $optionCommandCount++
    }
}

$output.Add('')
$output.Add("### NON-EXECUTABLE SETTERS - workbook-writable rows blocked by firmware ($($blocked.Count))")
$output.Add('')
$output.Add('These rows are included for protocol coverage but are not published as setter commands. Model validation and a future catalog-policy change are required before use.')
$output.Add('')
$output.Add('| Name | Description | Domain | Access | Guard | Reason/notes |')
$output.Add('|---|---|---|---|---|---|')
foreach ($row in $blocked) {
    $reason = if ([string]::IsNullOrWhiteSpace($row.Notes)) { $row.Models } else { $row.Notes }
    $output.Add("| $(Escape-Markdown $row.Name) | $(Escape-Markdown $row.Description) | $($row.Domain) | $($row.Access) | $($row.Guard) | $(Escape-Markdown $reason) |")
}
$output.Add($endMarker)

$document = [System.IO.File]::ReadAllText($ApiDocument)
$generated = $output -join [Environment]::NewLine
if ($document.Contains($beginMarker) -and $document.Contains($endMarker)) {
    $start = $document.IndexOf($beginMarker)
    $finish = $document.IndexOf($endMarker, $start) + $endMarker.Length
    $document = $document.Substring(0, $start) + $generated + $document.Substring($finish)
} else {
    $document = $document.TrimEnd() + [Environment]::NewLine + [Environment]::NewLine + $generated + [Environment]::NewLine
}
[System.IO.File]::WriteAllText($ApiDocument, $document, [System.Text.UTF8Encoding]::new($false))
Write-Host "Updated $ApiDocument"
Write-Host "Public holding getters: $($holdingGetters.Count)"
Write-Host "Public input getters: $($inputGetters.Count)"
Write-Host "Executable setter rows: $($settable.Count)"
Write-Host "Enumerated setter settings: $($enumeratedSetters.Count)"
Write-Host "Enumerated option commands: $optionCommandCount"
Write-Host "Blocked workbook-writable rows: $($blocked.Count)"
