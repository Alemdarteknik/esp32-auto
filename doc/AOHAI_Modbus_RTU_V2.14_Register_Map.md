# AOHAI Modbus RTU Protocol V2.14 - Explicit Register Map

Source workbook: `Modbus RTU Protocol V2.14-1.xlsx`.

This is an English-only, searchable text export of every numeric holding-register and input-register row in the workbook. Every source table column is preserved, including detailed descriptions, comments, enum explanations and applicable-model notes. The workbook is a general AOHAI external communication protocol; verify model-dependent fields on the FSC-12K1P-BL-G3 before relying on them.

## Communication

- Transport: Modbus RTU over RS-485
- Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit
- Read holding registers: function `0x03`
- Read input registers: function `0x04`
- Maximum read length: 125 registers per request
- Documented minimum command interval: 850 ms
- Error check: Modbus CRC-16, transmitted low byte first
- This document is informational. Do not issue write functions `0x06` or `0x10` without independently validating the exact FSC model, address and permitted value.

## FSC-12K live validation and corrections

| Register | Meaning | FSC-12K status |
|---|---|---|
| H207 | `DspBeepOnOff`: 0 disabled, 1 enabled | Confirmed by live 1 -> 0 toggle |
| R0 | Inverter status | Confirmed |
| R1 | Grid-connection countdown in seconds | Corrects earlier mode/code guess |
| R2 / R5 | Inverter voltage/current | Confirmed |
| R8 | Bus 1 internal voltage | Confirmed |
| R10-R12 | Inverter, boost/IPM and LLC temperatures | Confirmed |
| R13 | Lead-acid battery NTC temperature | Corrects earlier unknown-field label |
| R18 | R-phase DC-current component, signed mA | Corrects earlier dynamic-state guess |
| R24-R31 | System fault words 0-7 | Defined by workbook; bit meanings still require validation |
| R33 / R35 | Main warning code / warning sub-code | Defined by workbook |
| R36 | Device type | Confirmed stable raw value 3008 |
| R42-R56 | Grid and EPS voltage/current/frequency/PF area | Strong live correlation |
| R63-R67 | PV count, PV1 and PV2 voltage/current | Confirmed |
| R96 | Generator R-phase voltage, 0.1 V | Corrects earlier load-percentage guess |
| R107:R108 | Total operation time, uint32, 0.5-minute units | Confirmed |
| R125-R160 | Battery and BMS telemetry | Addresses strongly aligned; availability depends on battery/BMS |
| R250:R251 | Total PV input power, uint32, 0.1 W | Confirmed; high word currently zero |
| R252:R253 | PV1 input power, uint32, 0.1 W | Confirmed; high word currently zero |
| R254:R255 | PV2 input power, uint32, 0.1 W | Confirmed; high word currently zero |
| R344 | EPS output load percentage, scale 0.01 | Actual load-percentage address; not yet read live |

## Holding registers (`0x03`)

Exported numeric rows: 265

| Register Number | Variable Name | Detailed Description | Read/Write | Default | Value Range | Data Type | Unit | Stored | Comments | Applicable Models |
|---:|---|---|---|---:|---|---|---|---|---|---|
| H0 | OnOffSet | Switch on/off | R/W | 1 | 0 ~ 1 or 0xA5 | uint16 | - | No | On: 1<br>Off: 0<br>Restart: 0xA5 (TBD) | Single-phase/three-phase/off-grid |
| H1 | SystemSetBit | System Enable Flag Bit | R/W |  |  |  |  |  | Single and Triple Camera Use |  |
| H2 | DeviceType | Device Type | R |  |  | uint16 | - | No | Used for model identification |  |
| H3 | DeviceID | Device Correspondence Address | R/W | 1 | 1~245 | uint16 | - | Yes | Communication ID |  |
| H4 | BaudrateSet | Select Baud Rate | R/W | 0 | 0~1 | uint16 | - | Yes | 0:9600bps<br>1:38400bps |  |
| H5 | Serial NO.1 | Serial number 1 | R/W | - | - | ASCII | - | Yes | Product serial number, valid minimum 16 digits |  |
| H6 | Serial NO.2 | Serial number 2 | R/W | - | - | ASCII | - | Yes |  |  |
| H7 | Serial NO.3 | Serial number 3 | R/W | - | - | ASCII | - | Yes |  |  |
| H8 | Serial NO.4 | Serial numbe 4 | R/W | - | - | ASCII | - | Yes |  |  |
| H9 | Serial NO.5 | Serial number 5 | R/W | - | - | ASCII | - | Yes |  |  |
| H10 | Serial NO.6 | Serial number 6 | R/W | - | - | ASCII | - | Yes |  |  |
| H11 | Serial NO.7 | Serial numbe 7 | R/W | - | - | ASCII | - | Yes |  |  |
| H12 | Serial NO.8 | Serial numbe 8 | R/W | - | - | ASCII | - | Yes |  |  |
| H13 | Serial NO.9 | Serial number 9 | R/W | - | - | ASCII | - | Yes |  |  |
| H14 | Serial NO.10 | Serial number 10 | R/W | - | - | ASCII | - | Yes |  |  |
| H15 | Serial NO.11 | Serial number 11 | R/W | - | - | ASCII | - | Yes |  |  |
| H16 | Serial NO.12 | Serial number 12 | R/W | - | - | ASCII | - | Yes |  |  |
| H17 | Serial NO.13 | Serial number 13 | R/W | - | - | ASCII | - | Yes |  |  |
| H18 | Serial NO.14 | Serial number 14 | R/W | - | - | ASCII | - | Yes |  |  |
| H19 | Serial NO.15 | Serial numbe 15 | R/W | - | - | ASCII | - | Yes |  |  |
| H20 | Rs485WorkMode | 485 Operating Mode Selection | R/W | 0 | 0~2 | uint16 |  | Yes | 0: Slave mode (ate)<br>1: Host mode (BMS485)<br>2: Host mode (Meter485) |  |
| H21 | Year | System Time - Year | R/W | 2022 | Read: 2022 ~ 2050<br>Write: 22 ~ 50 | uint16 | Year | No |  |  |
| H22 | Month | System Time - Month | R/W | 1 | 1~12 | uint16 | Month | No |  |  |
| H23 | Day | System Time - Day | R/W | 1 | 1~31 | uint16 | Day | No |  |  |
| H24 | Hour | System Time - Hours | R/W | 0 | 0~59 | uint16 | Hour | No |  |  |
| H25 | Minute | System Time - Minutes | R/W | 0 | 0~59 | uint16 | Minutes | No |  |  |
| H26 | Second | System Time - Seconds | R/W | 0 | 0~59 | uint16 | Seconds | No | Set time to seconds to take effect |  |
| H27 | Weekday | System Week | R/W | 1 | 1~7 | uint16 | - | No |  |  |
| H28 | SoftVersion_Attest | Firmware Version (High) | R | - |  | ASCII | - | No | Firmware Version (XX1.0) |  |
| H29 |  | Firmware Version (Medium) | R | - |  | ASCII | - | No |  |  |
| H30 |  | Firmware Version (Low) | R | - |  | ASCII | - | No |  |  |
| H31 | SoftVersion_Monitor | Arm Firmware Version Name | R | - |  | ASCII | - | No | Arm Software Version (XXXX0000) |  |
| H32 |  | Arm Firmware Version Name | R | - |  | ASCII | - | No |  |  |
| H33 |  | Arm Firmware Version Number | R | - |  | uint16 | - | No |  |  |
| H34 | SoftVersion_Control | DSP Software Version Name (TJ) | R | - |  | ASCII | - | No | DSP Software Version (XXXX0000) |  |
| H35 |  | DSP Software Version Name (AA) | R | - |  | ASCII | - | No |  |  |
| H36 |  | DSP1 Software Major Version Number | R | - |  | uint16 | - | No |  |  |
| H37 |  | DSP2 Software Major Version Number | R | - |  | uint16 | - | No |  |  |
| H38 | MasterDSPTestVersion | DSP1 software debug version number | R | - |  | uint16 | - | No |  |  |
| H39 | SlaveDSPTestVersion | DSP2 software debug version number | R | - |  | uint16 | - | No |  |  |
| H40 | ARM TestVersion | Arm debug version number | R | - |  | uint16 | - | No |  |  |
| H41 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H42 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H43 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H44 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H45 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H46 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H47 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H48 | DSP Hard version | Dashboard Hardware Version | R | - |  | uint16 | - | No | Hardware Version |  |
| H49 | ARM Hard version | Dashboard Hardware Version | R | - |  | uint16 | - | No | Hardware Version | Single-phase temporarily not supported |
| H50 | Manufacturer Info 8 | Manufacturer Information 8 | R | - |  | ASCII | - | No | Vendor Information |  |
| H51 | Manufacturer Info 7 | Manufacturer Information 7 | R | - |  | ASCII | - | No |  |  |
| H52 | Manufacturer Info 6 | Manufacturer Information 6 | R | - |  | ASCII | - | No |  |  |
| H53 | Manufacturer Info 5 | Manufacturer Information 5 | R | - |  | ASCII | - | No |  |  |
| H54 | Manufacturer Info 4 | Manufacturer Information 4 | R | - |  | ASCII | - | No |  |  |
| H55 | Manufacturer Info3 | Manufacturer Information 3 | R | - |  | ASCII | - | No |  |  |
| H56 | Manufacturer Info 2 | Manufacturer Info 2 | R | - |  | ASCII | - | No |  |  |
| H57 | Manufacturer Info 1 | Manufacturer Info 1 | R/W | - | 0~4 | ASCII/uint16 | - | Yes | Optional vendor name when configuring |  |
| H58 | ModbusVersion | Modbus Version | R | - | - | uint16 | - | No | Communication Protocol Version (01) |  |
| H59 | Module 4 | Inverter Mode (4) | R/W | - | - | uint16 | - | Yes |  |  |
| H60 | Module 3 | Inverter Mode (3) | R/W | - | - | uint16 | - | Yes |  |  |
| H61 | Module 2 | Inverter Mode (2) | R/W | - | - | uint16 | - | Yes |  |  |
| H62 | Module 1 | Inverter Mode (1) | R/W | - | - | uint16 | - | Yes |  |  |
| H63 | MaxInvPower | Rated power (high) | R | - | 0~65535 | uint16 | 0.1W | No |  |  |
| H64 |  | Rated Power (Low) | R | - | 0~65535 | uint16 | 0.1W | No |  |  |
| H65 | Reset to factory | Factory reset | W | - | - | uint16 | - | No | 0x01: Restore user data<br>0xA0: Restore factory settings |  |
| H66 | UpdateFileType | Upgrade File Type | W | - | - | uint16 | - | No | 0x01: bin upgrade<br>0x10: Hex upgrade |  |
| H67 | UpdateState | Firmware update progress | R | - | - | uint16 | - | No | Read (0 ~ 100: upgrade progress<br>> 100: upgrade fault code) |  |
| H68 | ubScreenType | Screen Type | R/W | - | - | uint8 | - | Yes | 0: Small screen<br>1: Large screen |  |
| H69 | SafetyId | Safety regulation number | R/W | 0 | 0~200 | uint16 |  | Yes |  |  |
| H70 | ReactivePowerDelayTIme | Reactive Percent Response Time | R/W | 0 | 0~30000 | uint16 | 20ms | Yes |  |  |
| H71 | ActiveOverloadEnable | Active Power Overload Enabled | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled | Single-phase temporarily not supported |
| H72 | AcChargePowerRate | Inverter Max Take-Off Power Percentage | R/W | 100 | 0~100 | uint16 | 1%Pn | Yes | Grid Intake |  |
| H73 | ActivePowerSlope | Active power rate of change (N4105) | R/W | 0 | 0~30000 | uint16 | 0.1Pn/min | Yes | 0: Not Enabled<br>Other: Enabled | Single-phase temporarily not supported |
| H74 | ActivePowerRate | Inverter Max Output Active Power Percentage | R/W | 100 | 0~100 | uint16 | 1%Pn | Yes | Power Generation to the Grid |  |
| H75 | ReactivePowerRate | Inverter Max Output Reactive Power Percentage | R/W |  | 100~-100 | sint16 | 1%Pn | Yes |  |  |
| H76 | PowerFactorSet | Inverter output power factor 10,000 times | R/W | 10000 | 0~20000 | uint16 |  | Yes | PF=(X-10000)/10000 |  |
| H77 | PVModelSelect | MPPT Mode | R/W | 0 | 0~2 | uint16 |  | Yes | 0: Independent MPPT mode<br>1: DC source mode<br>2: Parallel MPPT mode |  |
| H78 | PvStartVoltage | PV start voltage | R/W |  |  | uint16 | 0.1V | Yes |  |  |
| H79 | FastDerating_en | Fast Down Enable | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled |  |
| H80 | Island_en | Island Enabling | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled |  |
| H81 | VFRT_en | High and low penetration enable | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled |  |
| H82 | DRMS_en | DRMS Enabled | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled |  |
| H83 | NLineConnectMode | N-line enable | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled | Single-phase temporarily not supported |
| H84 | NToGNDDetect | Zero Earth Detection | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled | Single-phase temporarily not supported |
| H85 | ZeroPowerOutputEnable | Zero power output enabled | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled | Single-phase temporarily not supported |
| H86 | FastMpptEnable | Fast mppt enable | R/W | 0 | 0~1 | uint16 | - | Yes | 0: Disabled<br>1: Enabled | Single-phase temporarily not supported |
| H87 | CtRatioSet | CT Rheological Ratio Setting | R/W | 2000 | 1~3000 | uint16 |  | Yes | Reserved |  |
| H88 | CTMode | Using CT Mode | R/W | 0 | 0~1 | uint16 |  | Yes | 0：METER<br>1：CT |  |
| H89 | LocalAntiBackflowEnable | Anti-reflux enable | R/W | 0 | 0~3 | uint16 |  | Yes | 0: Disable<br>1: Total anti-reflux enable<br>2: Three-phase independent anti-reflux enable |  |
| H90 | BackflowMeterPowerLimit_R | Anti-reflux Reflux Power Limit | R/W | 0 | 0~100 | sint16 | 1Pn% | Yes |  |  |
| H91 | BackflowMeterPowerLimit_S | Anti-reflux Reflux Power Limit | R/W | 0 | 0~100 | sint16 | 1Pn% | Yes |  | Single-phase temporarily not supported |
| H92 | BackflowMeterPowerLimit_T | Anti-reflux Reflux Power Limit | R/W | 0 | 0~100 | sint16 | 1Pn% | Yes |  | Single-phase temporarily not supported |
| H93 | BackflowHostNoResponseFlag | Anti-reflux host failure flag bit | R | 0 |  | uint16 |  | Yes | 0: Normal<br>1: Failure | Single-phase temporarily not supported |
| H94 | BackflowFaultTime | Anti-reflux failure time | R/W | 30 | 30~120 | uint16 | 1s | Yes |  | Single-phase temporarily not supported |
| H95 | BackflowFaultPowerRate | Percentage of active anti-reflux failure | R/W | 50 | 0~100 | sint16 | 1Pn% | Yes | Machine operates at given power after anti-reverse flow failure | Single-phase temporarily not supported |
| H96 | Reserved | Reserved |  |  |  |  |  |  |  | Single-phase temporarily not supported |
| H97 | Reserved | Reserved |  |  |  |  |  |  |  | Single-phase temporarily not supported |
| H98 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H99 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H100 | Power on config falg | Boot navigation flags | R/W | 0 | 0 ~ 1 or 0x55 | uint16 |  | Yes | 0x00: default<br>0x01: configuring (no storage, shutdown)<br>0x55: configuration complete (save) |  |
| H101 | Vac low C | Grid Low Voltage Restrictions Connected to the Grid | R/W |  | 1700~2400 | uint16 | 0.1V | Yes | Grid-connected conditions |  |
| H102 | Vac high C | Grid High Voltage Restriction Connected to Grid | R/W |  | 2000~2900 | uint16 | 0.1V | Yes |  |  |
| H103 | Fac low C | Grid Low Frequency Restrictions Connected to the Grid | R/W |  | 50Hz：4700~5010<br>60Hz：<br>5700~6010 | uint16 | 0.01Hz | Yes |  |  |
| H104 | Fac high C | Grid High Frequency Restrictions Connected to the Grid | R/W |  | 50Hz：<br>4990~5300<br>60Hz：<br>5990~6300 | uint16 | 0.01Hz | Yes |  |  |
| H105 | Reserved | Reserved |  |  |  | uint16 |  |  |  |  |
| H106 | NominalEPSVolt | Optional rated off-grid voltage | R/W | 0 | 0~4 | uint16 |  | Yes | 0: 230V<br>1: 240V<br>2: 208V<br>3: 220V (off-grid)<br>4: 200V (off-grid) |  |
| H107 | NominalEPSFre | Optional rated off-grid frequency | R/W | 0 | 0~1 | uint16 |  | Yes | 0:50Hz<br>1:60Hz |  |
| H108 | EPS_En | Off-grid functionality enabled | R/W | 1 | 0~1 | uint16 |  | Yes |  |  |
| H109 | Bypass_En | Bypass mode enabled | R/W | 0 | 0~1 | uint16 |  | Yes |  |  |
| H110 | UPS_En | UPS Function Enabled | R/W | 0 | 0~1 | uint16 |  | Yes | TBC | Single-phase temporarily not supported |
| H111 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H112 | ubN_PE_RelayCMD | Zero earth relay control enabled | R/W | 0 | 0~1 | uint16 |  | Yes | Attendance Functional Requirements |  |
| H113 | CheckFanCmd | Fan Detection |  |  |  | uint16 |  | Yes | TBC | Single-phase temporarily not supported |
| H114 | unAFCICtrlReg | AFCI Self-Test Command | R/W |  |  | uint16 |  | Yes | Bit0: SelfCheckCmd；<br>Bit1: ClrFaultCmd；<br>Bit2~7:reserved； | Single-phase temporarily not supported |
| H115 | uwAFCIThresholdValue | AFCI error threshold | R/W |  |  | uint16 |  | Yes | AFCI error threshold | Single-phase temporarily not supported |
| H116 | AFCIClrFaultCmd | AFCI Error Clear Command | R/W |  |  | uint16 |  | No |  |  |
| H117 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H118 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H119 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H120 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H121 | uwRestartVoltL | Fault reconnect grid low voltage | R/W |  | 1700~2400 | uint16 | 0.1V | Yes | Fault reconnection and grid connection conditions |  |
| H122 | uwRestartVoltH | Fault reconnect grid high voltage | R/W |  | 2000~2900 | uint16 | 0.1V | Yes |  |  |
| H123 | uwRestartFreqL | Fault Reconnect Grid Low Frequency | R/W |  | 50Hz：4700~5010<br>60Hz：<br>5700~6010 | uint16 | 0.01Hz | Yes |  |  |
| H124 | uwRestartFreqH | Fault Reconnect Grid High Frequency | R/W |  | 50Hz：<br>4990~5300<br>60Hz：<br>5990~6300 | uint16 | 0.01Hz | Yes |  |  |
| H125 | BatteryType | Battery type | R/W | 1 |  | uint16 |  | Yes | 0: Lead acid<br>1: Lithium battery<br>2: Lithium battery without communication (off-grid)<br>3: User 2 (off-grid)<br>4: User 3 (off-grid)<br>5: NO Bat |  |
| H126 | BatteryCompanySet | Battery communication protocol selection | R/W | 0 |  | uint16 |  | Yes | 0: null<br>1: Protocol 1<br>2: Protocol 2<br>3: Protocol 3<br>4: Protocol 4 | Single-phase temporarily not supported |
| H127 | BatMdlSerialNum | Number of batteries in series - high-voltage batteries; | R/W | 36 | 12,18,24,36 | uint16 |  | Yes | Lead-acid batteries (for high-voltage battery systems) | Single-phase temporarily not supported |
| H128 | BatMdlParallNum | Number of battery parallel joints; | R/W | 1 | 1~160 | uint16 |  | Yes | Lead-acid batteries (reserved, unused) | Single-phase temporarily not supported |
| H129 | ChargeCurrentLimit | Charge Limit Current | R/W | 8500 | S、O:<br>100~12000<br>T:10~250 | uint16 | 0.01A | Yes | When the charging current needs to be lower than this value, enter the floating charging CC |  |
| H130 | VbatStopForDischarge | Discharge cut-off voltage | R/W | 4600 | S:4200~5000<br>T:700~1200<br>O:4000~5200 | uint16 | 0.01V | Yes |  |  |
| H131 | Vbat constant charge | Charge cut-off voltage | R/W | 5800 | S、O:<br>4800~5920<br>T:1200~1600 | uint16 | 0.01V | Yes | CV voltage (lead-acid battery) |  |
| H132 | ubHighVoltBatRatedVolt | Battery nominal voltage | R/W | 400 | T:100~800 | uint16 | 1V | Yes |  |  |
| H133 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H134 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H135 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H136 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H137 | ChargeRate | Charging power | R/W | 100 | 0~100 | uint16 | 0.01 | Yes |  |  |
| H138 | ChargeRate | Discharge power | R/W | 100 | 0~100 | uint16 | 0.01 | Yes |  |  |
| H139 | BatFirstStopSOC | Charge cut-off SOC | R/W | 100 | 10~100 | uint16 | 0.01 | Yes |  |  |
| H140 | OnLineStopSOC | Grid-connected discharge cut-off SOC | R/W | 10 | 10~100 | uint16 | 0.01 | Yes |  |  |
| H141 | OffLineStopSoc | Off-grid Discharge Cutoff SOC | R/W | 10 | 10~100 | uint16 | 0.01 | Yes |  |  |
| H142 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H143 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H144 | Gen Charge En | Total Diesel Charging Enabled | R/W | 0 | 0~1 | uint16 |  | Yes | 0: Disabled<br>1: Enabled |  |
| H145 | AcCharge_En | AC Charge Enabled | R/W | 1 | 0~1 | uint16 |  | Yes | 0: Disabled<br>1: Enabled |  |
| H146 | LoadFirstDischargeDisableFlag | Load Preferred Discharge Prohibit Sign | R/W | 0 | 0~1 | uint16 |  | Yes | TBC |  |
| H147 | RemotePowerControl | One-click charging and discharging | R/W | 255 | 0~2，255 | uint16 |  | No | 255: Fail<br>0: Load first<br>1: Battery first<br>2: Grid first |  |
| H148 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H149 | Reserved | Reserved |  |  |  |  |  |  |  |  |
| H150 | NumberTimePeriods | Number of charging and discharging periods - number of currently valid periods, starting with period 1 | R/W | 0 | 0~10 | uint16 |  | Yes | 0 ~ 10: Number of time periods<br>0xA5: Number of time periods reset, all time period setting values reset |  |
| H151 | PeriodTimeStart1 | Period 1 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H152 | PeriodTimeEnd1 | Period 1 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H153 | PeriodTimeRate1 | Period 1 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H154 | PeriodTimeStart2 | Period 2 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H155 | PeriodTimeEnd2 | Period 2 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H156 | PeriodTimeRate2 | Period 2 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H157 | PeriodTimeStart3 | Period 3 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H158 | PeriodTimeEnd3 | Slot 3 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H159 | PeriodTimeRate3 | Period 3 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H160 | PeriodTimeStart4 | Period 4 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H161 | PeriodTimeEnd4 | Period 4 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H162 | PeriodTimeRate4 | Period 4 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H163 | PeriodTimeStart5 | Period 5 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H164 | PeriodTimeEnd5 | Slot 5 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H165 | PeriodTimeRate5 | Period 5 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H166 | PeriodTimeStart6 | Period 6 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H167 | PeriodTimeEnd1 | Slot 6 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H168 | PeriodTimeRate1 | Period 6 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H169 | PeriodTimeStart7 | Period 7 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H170 | PeriodTimeEnd1 | Segment 7 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H171 | PeriodTimeRate1 | Period 7 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H172 | PeriodTimeStart8 | Period 8 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H173 | PeriodTimeEnd1 | Slot 8 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H174 | PeriodTimeRate1 | Period 8 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H175 | PeriodTimeStart9 | Period 9 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H176 | PeriodTimeEnd1 | Period 9 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H177 | PeriodTimeRate1 | Period 9 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H178 | PeriodTimeStart10 | Period 10 Start Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H179 | PeriodTimeEnd10 | Slot 10 End Time | R/W | 0 | 0~0x173B | uint16 | 1min | Yes | H: hour 0 ~ 23<br>L: minute 0 ~ 59 |  |
| H180 | PeriodTimeRate10 | Period 10 Priority | R/W | 0 | 0~2 | uint16 | 1Pn% | Yes | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| H181 | ChargeSourcePriority | Charge Priority | R/W | 1 | 0~2 | uint8 |  | Yes | 0:CSO<br>1:SNU<br>2:OSO |  |
| H182 | SourcePriority | Energy Priority | R/W | 1 | 0~10 | uint8 |  | Yes | 0: Sol<br>1: UTI<br>2: SBU<br>10: Grid connected output mode |  |
| H183 | uwCC_DisChrLead_100T | Lead Acid Discharge Current | R/W | 13700 | 100~13700 | uint16 | 0.01A | Yes |  |  |
| H184 | Reserved |  |  |  |  |  |  |  |  |  |
| H185 | ubOP_RecoverDischargeSOC | Grid-connected stop discharge recovery SOC | R/W | 90 | 20~100 | uint8 | 0.01 | Yes |  |  |
| H186 | Reserved |  |  |  |  |  |  |  |  |  |
| H187 | ubOffGrid_RecoverDischargeSOC | Off-grid Stop Discharge Recovery SOC | R/W | 30 | 10~100 | uint8 | 0.01 | Yes |  |  |
| H188 | BatUnderVol | Battery Low Voltage Shutdown Voltage | R/W | 400 | 400~480 | uint16 | 0.1V | Yes |  |  |
| H189 | AcChargingCurrent | AC Charge Limit Current | R/W | 600 | 10~1000 | uint16 | 0.1A | Yes |  |  |
| H190 | uwFloatV_Lead_100T | Floating Charge Voltage | R/W | 5520 | 4800~5840 | uint16 | 0.01V | Yes |  |  |
| H191 | BAT2AC_Volt | Battery-to-market voltage | R/W | 460 | 440~520 | uint16 | 0.1V | Yes |  |  |
| H192 | AC2BAT_Volt | Mains to Battery Voltage | R/W | 540 | 480~580 | uint16 | 0.1V | Yes |  |  |
| H193 | ubLeadAcid_BatSubType | Subtype of lead-acid battery | R/W | 3 | 0~8 | uint8 |  | Yes | 0:USE<br>1:SLD<br>2:FLD<br>3:GEL<br>4:L14<br>5:L15<br>6:L16<br>7:N13<br>8:N14<br>9:LIT |  |
| H194 | IncreChar_MaxTim | Improved charging time | R/W | 120 | 5~900 | uint16 | 1min | Yes |  |  |
| H195 | BatUnderVolt_Point | Battery undervoltage alarm point | R/W | 440 | 400~500 | uint16 | 0.1V | Yes |  |  |
| H196 | Equalization | Equalization Mode Enabled | R/W | 1 | 0~1 | uint8 |  | Yes | 0: Disable<br>1: Enable |  |
| H197 | EQBatteryTime | Equalize charging time | R/W | 120 | 5~900 | uint16 | 1min | Yes |  |  |
| H198 | EQBatteryTimeout | Equalize Charge Delay Time | R/W | 120 | 5~900 | uint16 | 1min | Yes |  |  |
| H199 | EqualizationCycle | Balanced charge interval | R/W | 30 | 0~30 | uint8 | 1DAY | Yes |  |  |
| H200 | EqualizationImmediately | Turn on Balanced Charging now | R/W | 0 | 0~1 | uint16 |  | No | 0: Disable<br>1: Enable |  |
| H201 | flcdEn | LCD setting enable bit | R/W |  |  | uint16 |  | Yes | 0: ECOMode_En (Energy Saving Mode Enabled)<br>1: OverLoad_RestartEn<br>2: OverTemp_RestartEn<br>3: InputChange_RemEn<br>4: OPSplit_PhaseEn<br>5: Generator_AutoIPEn<br>6: DualChannel_LoadEn<br>7: ubridFeedBackEn<br>8 ~ 15: unused |  |
| H202 | BatLVBreak_RestartVolt | Low Voltage Disconnect Battery Recovery Point | R/W | 520 | 500~580 | uint16 | 0.1V | Yes |  |  |
| H203 | BatNeedChr_Volt | Battery Recharge Recovery Point | R/W | 520 | 500~560 | uint16 | 0.1V | Yes |  |  |
| H204 | NonCriticlLoad_BatDisConVolt | Non-critical load disconnect battery voltage | R/W | 460 | 420~540 | uint16 | 0.1V | Yes |  |  |
| H205 | BatHighVolt_DisConPoint | Overvoltage disconnection voltage | R/W | 600 | 300~600 | uint16 | 0.1V | Yes |  |  |
| H206 | BatOverDisCharge_Delay | Battery Overdischarge Delay Time | R/W | 5 | 5~50 | uint16 | 1s | Yes |  |  |
| H207 | DspBeepOnOff | Buzzer switch | R/W | 1 | 0~1 | uint8 |  | Yes | 0: Disable<br>1: Enable |  |
| H208 | OverloadToBypass | Overload transfer bypass enable | R/W | 1 | 0~1 | uint8 |  | Yes | 0: Disable<br>1: Enable |  |
| H209 | AcInputType | Off-grid output mode | R/W | 1 | 0~1 | uint8 |  | Yes | 0:APL<br>1:UPS |  |
| H210 | EQBatteryVoltage_100T | Balanced Charging Voltage | R/W | 5840 | 4800~5840 | uint16 |  | Yes |  |  |
| H211 | Parallel_Mode | Parallel Mode | R/W | 0 | 0~4 | uint8 |  | Yes | 0:SIG<br>1:PAL<br>2:3P1<br>3:3P2<br>4:3P3 |  |
| H212 | ubParallelDeviveID | Parallel can communication address | R/W | 0 | 0~9 | uint8 |  | Yes |  |  |
| H213 | ubParallelDeviveType | Parallel Device Type | R/W | 0 | 0~1 | uint8 |  | Yes | 0: Host<br>1: Slave |  |
| H214 | ubBMSWorkMode | BMS communication method | R/W | 0 | 0~2 | uint8 |  | Yes | 0: Disable<br>1: can communication<br>2: 485 communication |  |
| H215 | uwGridPowerCompensation | Grid Power Compensation | R/W | 40 | 0~200 | uint16 |  | Yes |  |  |
| H216 | Gen Port Work Mode | Diesel Port Function Selection | R/W | 0 |  | uint16 |  | Yes | 0.Default<br>1.Generator En<br>2.Gen Force<br>3.SmartLoad Output<br>4.On Grid always on<br>5.Off Grid immediately off<br>6.AC Couple on SecEPS side |  |
| H217 | Gen Charge Curr Limit | Diesel Charging Current Limit | R/W | 100A |  | uint16 | 1A | Yes |  |  |
| H218 | Gen Input Rated Power | Generator Input Rated Power | R/W | 8000W |  | uint16 | 10W | Yes |  |  |
| H219 | SecEPS ON SOC/Vbat | (Lithium) Start SOC | R/W | 0.4 |  | uint16 | 0.01 | Yes |  |  |
| H220 | SecEPS ON Vbat | (Lead acid) Starting battery voltage | R/W | 45.0V |  | uint16 | 0.1V | Yes |  |  |
| H221 | SecEPS OFF SOC/Vbat | (Lithium) Shutdown SOC | R/W | 0.4 |  | uint16 | 0.01 | Yes |  |  |
| H222 | SecEPS OFF Vbat | (Lead Acid) Shutdown Battery Voltage | R/W | 55.0V |  | uint16 | 0.1V | Yes |  |  |
| H223 | SecEPS  On PV Power Min | Minimum power of photovoltaic startup smart load | R/W | 3000W |  | uint16 | 10W | Yes |  |  |
| H224 |  |  |  |  |  |  |  |  |  |  |
| H225 |  |  |  |  |  |  |  |  |  |  |
| H226 |  |  |  |  |  |  |  |  |  |  |
| H227 |  |  |  |  |  |  |  |  |  |  |
| H228 |  |  |  |  |  |  |  |  |  |  |
| H231 | ubBluetoothEn | Bluetooth Enabled | R/W | 1 |  | uint8 |  | Yes | 1: Bluetooth On<br>0: Bluetooth Off |  |
| H232 | upgrade notification | Data update notification, wifi re-submit 0304 data to the server | R/W | 0 |  | uint8 |  | No | 0: Invalid default<br>1: Trigger update |  |
| H233 | Datalogger Restart | wifi factory settings restored, server domain name and escalation time modified | R/W | 0 |  | uint8 |  | No | 0: Invalid by default<br>1: Trigger a factory reset |  |
| H234 | ubConnectServer | Collector network status | R/W |  | 0x0055、<br>0x00AA、<br>0x0100、<br>0x0200 | uint8 |  | No | Low 8-bit indicates networking status:<br>0x0055: Networking exception<br>0x00AA: Networking normal<br>High 8-bit indicates collector type:<br>0x0100: Wifi-U<br>0x0200: 4G-U |  |
| H235 | ubDatalogAndArmCommunication | Collector and inverter communication status | R/W |  | 0x55、0xAA | uint8 |  | No | 0x55: Communication error<br>0xAA: Communication OK |  |
| H250 | LocalSafetyCmd | User Safety Selection Instructions | R/W | 0 |  | uint16 |  | Yes | TBD<br>0: Regional Standard Safety Code<br>1: Subscriber Wide Range<br>2: Grid Company Safety Code |  |
| H251 | fGridVoltLow1EE | Low Grid Voltage Protection Tier 1 | R/W |  | 460~2400 | uint16 | 0.1V | Yes |  |  |
| H252 | fGridVoltHigh1EE | High Grid Voltage Protection Tier 1 | R/W |  | 2000~2900 | uint16 | 0.1V | Yes |  |  |
| H253 | fFreqLow1EE | Grid Low Frequency Protection First Order | R/W |  | 50Hz：4700~5010<br>60Hz：<br>5700~6010 | uint16 | 0.01Hz | Yes |  |  |
| H254 | fFreqHigh1EE | Grid High Frequency Protection First Order | R/W |  | 50Hz：<br>4990~5300<br>60Hz：<br>5990~6300 | uint16 | 0.01Hz | Yes |  |  |
| H255 | fGridVoltLow2EE | Low Grid Voltage Protection Level 2 | R/W |  | 460~2400 | uint16 | 0.1V | Yes |  |  |
| H256 | fGridVoltHigh2EE | High Grid Voltage Protection Level 2 | R/W |  | 2000~2900 | uint16 | 0.1V | Yes |  |  |
| H257 | fFreqLow2EE | Grid Low Frequency Protection Second Order | R/W |  | 50Hz：4700~5010<br>60Hz：<br>5700~6010 | uint16 | 0.01Hz | Yes |  |  |
| H258 | fFreqHigh2EE | Grid High Frequency Protection Second Order | R/W |  | 50Hz：<br>4990~5300<br>60Hz：<br>5990~6300 | uint16 | 0.01Hz | Yes |  |  |
| H259 | fGridVoltLow3EE | Low Grid Voltage Protection Level 3 | R/W |  | 460~2400 | uint16 | 0.1V | Yes |  | Single-phase temporarily not supported |
| H260 | fGridVoltHigh3EE | High Grid Voltage Protection Level 3 | R/W |  | 2000~2900 | uint16 | 0.1V | Yes |  | Single-phase temporarily not supported |
| H261 | fFreqLow3EE | Grid Low Frequency Protection Level 3 | R/W |  | 50Hz：4700~5010<br>60Hz：<br>5700~6010 | uint16 | 0.01Hz | Yes |  | Single-phase temporarily not supported |
| H262 | fFreqHigh3EE | Grid High Frequency Protection Level 3 | R/W |  | 50Hz：<br>4990~5300<br>60Hz：<br>5990~6300 | uint16 | 0.01Hz | Yes |  | Single-phase temporarily not supported |
| H263 | wVLowCutTime1EE | Low Grid Voltage First Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H264 | wVHighCutTime1EE | High grid voltage first-order protection time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H265 | udFLowCutTime1EE | Low Grid Frequency First Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H266 | udFHighCutTime1EE | High Grid Frequency First Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H267 | wVLowCutTime2EE | Low Grid Voltage Second Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H268 | wVHighCutTime2EE | High Grid Voltage Second Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H269 | udFLowCutTime2EE | Low Grid Frequency Second Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H270 | udFHighCutTime2EE | High Grid Frequency Second Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H271 | wVLowCutTime3EE | Low grid voltage third-order protection time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  | Single-phase temporarily not supported |
| H272 | wVHighCutTime3EE | High grid voltage third-order protection time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  | Single-phase temporarily not supported |
| H273 | udFLowCutTime3EE | Low Grid Frequency Third Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  | Single-phase temporarily not supported |
| H274 | udFHighCutTime3EE | High Grid Frequency Third Order Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  | Single-phase temporarily not supported |
| H275 | 10MinAVLimit | Voltage protection for ten minutes | R/W |  | 200~2900 | uint16 | 0.1V | Yes |  |  |
| H276 | U10minTime | 10 Minute Average Voltage Protection Time | R/W |  | 0~5000 | uint16 | 20ms | Yes |  |  |
| H277 | Time start | Connection time | R/W |  | 30~900 | uint16 | 1s | Yes |  | Single-phase temporarily not supported |
| H278 | RestartDelayTime | Reconnection time | R/W |  | 30~900 | uint16 | 1s | Yes |  |  |
| H279 | PowerStartSlope | Load rate | R/W |  | 1-30000 | uint16 | 0.1Pn%/min | Yes |  |  |
| H280 | PowerRestartSlopeEE | Restart Load Rate | R/W |  | 1-30000 | uint16 | 0.1Pn%/min | Yes |  |  |

## Input registers (`0x04`)

Exported numeric rows: 422

| Register Number | Variable Name | Detailed Description | Data Type | Unit | Comments | Applicable Models |
|---:|---|---|---|---|---|---|
| R0 | Inverter Status | Inverter operating status | uint16 |  | 0x00: Waiting state<br>0x01: Connected state<br>0x02: Off-grid state<br>0x03: Fault state<br>0x04: Burn state<br>0x05: Bypass state<br>0x06: Self-charging state |  |
| R1 | StartDelayTime | Grid-connected countdown | uint16 | 1s |  |  |
| R2 | INV_VolR | Inverter Voltage | uint16 | 0.1V |  |  |
| R3 | INV_VolS | Inverter Voltage | uint16 | 0.1V |  | Single-phase not supported |
| R4 | INV_VolT | Inverter Voltage | uint16 | 0.1V |  | Single-phase not supported |
| R5 | INV_CurrR | Inverter Current | sint16 | 0.1A |  |  |
| R6 | INV_CurrS | Inverter Current | sint16 | 0.1A |  | Single-phase not supported |
| R7 | INV_CurrT | Inverter Current | sint16 | 0.1A |  | Single-phase not supported |
| R8 | Bus1 Voltage | Bus1 Internal Voltage | uint16 | 0.1V |  |  |
| R9 | Bus2 Voltage | Bus2 Internal Voltage | uint16 | 0.1V |  |  |
| R10 | Inv_Temp | Inverter Temperature | sint16 | 0.1C° |  |  |
| R11 | Boost_Temp | Inverter Internal IPM Temperature | sint16 | 0.1C° |  |  |
| R12 | LLC _Temp | LLC Radiator Temperature | sint16 | 0.1C° |  |  |
| R13 | Bat_Temp | Battery temperature | sint16 | 0.1C° | Lead-acid battery NTC sampling temperature |  |
| R14 | TA_Temp | Ambient temperature | sint16 | 0.1C° |  |  |
| R15 | DCV-R | R-phase DC voltage component | sint16 | 1mV |  | Single-phase not supported |
| R16 | DCV-S | S-phase DC voltage component | sint16 | 1mV |  | Single-phase not supported |
| R17 | DCV-T | T-phase DC voltage component | sint16 | 1mV |  | Single-phase not supported |
| R18 | DCI-R | R-phase DC current component | sint16 | 1mA |  | Single-phase not supported |
| R19 | DCI-S | S-phase DC current component | sint16 | 1mA |  | Single-phase not supported |
| R20 | DCI-T | T-phase DC current component | sint16 | 1mA |  | Single-phase not supported |
| R21 | ISO Resistance | ISO Resistance | uint16 | 1kΩ |  |  |
| R22 | GFCI | Leakage current | uint16 | 1mA |  | Single-phase not supported |
| R23 | HistoryEventCnt | Number of historical event records | uint16 |  |  |  |
| R24 | Systemfault word0 | System Failure word0 | uint16 |  |  |  |
| R25 | Systemfault word1 | System fault word1 | uint16 |  |  |  |
| R26 | Systemfault word2 | System fault word2 | uint16 |  |  |  |
| R27 | Systemfault word3 | System failure word3 | uint16 |  |  |  |
| R28 | Systemfault word4 | System failure word4 | uint16 |  |  |  |
| R29 | Systemfault word5 | System Failure word5 | uint16 |  |  |  |
| R30 | Systemfault word6 | System Failure word6 | uint16 |  |  |  |
| R31 | Systemfault word7 | System Failure word7 | uint16 |  |  |  |
| R32 | InvMainErrorCode | Inverter Master Fault Code | uint16 |  |  |  |
| R33 | InvMainWarnCode | Inverter Main Warning Code | uint16 |  |  |  |
| R34 | InvErrorSubCode | Inverter sub-fault code | uint16 |  |  |  |
| R35 | InvWarnSubCode | Inverter sub-warning code | uint16 |  |  |  |
| R36 | DeviceType | Device Type | uint16 |  |  |  |
| R37 | Reserved | Reserved |  |  |  |  |
| R38 | DeratingModeFlag | Download Mode Flag | uint16 |  | 0: No offload; 1: Busbar High Voltage; 2: Grid Low Voltage; 3: Grid High Voltage<br>4: High Frequency; 5: boost High Temperature<br>6: Inverter High Temperature; 7: Ambient High Temperature<br>8: Load Speed; 9: Reactive Power<br>10: Overload; 11: Underloaded<br>12: Active Set Limit; 13: Multi-machine Anti-Countercurrent; 14: Stand-alone Anti-Countercurrent<br>15: Zero Current Mode; 16: Aging Set Limit; 17: Line Impedance Limit<br>18: Fan Anomaly; 19: CT Anomaly<br>20: LLC Over Temperature; 21: Battery Discharge Set Limit; 22: Power Selling Set Limit<br>23: PV Out of Range |  |
| R39 | PowerCosFlag | Leading Lag Flag | uint16 |  | TBC | Single-phase not supported |
| R40 | Bus1 Voltage | Positive Bus Voltage | uint16 | 0.1V |  |  |
| R41 | Bus1 Voltage | Negative Bus Voltage | uint16 | 0.1V |  |  |
| R42 | Vac-R | Three Phase Grid Voltage | uint16 | 0.1V | Single camera shows only R parameters |  |
| R43 | Iac-R | Three-phase grid output current | sint16 | 0.1A | Single camera shows only R parameters |  |
| R44 | Vac-S | Three Phase Grid Voltage | uint16 | 0.1V |  | Single-phase not supported |
| R45 | Iac-S | Three-phase grid output current | sint16 | 0.1A |  | Single-phase not supported |
| R46 | Vac-T | Three Phase Grid Voltage | uint16 | 0.1V |  | Single-phase not supported |
| R47 | Iac-T | Three-phase grid output current | sint16 | 0.1A |  | Single-phase not supported |
| R48 | Vac_RS | Three-phase grid line voltage | uint16 | 0.1V |  | Single-phase not supported |
| R49 | Vac_ST | Three-phase grid line voltage | uint16 | 0.1V |  | Single-phase not supported |
| R50 | Vac_TR | Three-phase grid line voltage | uint16 | 0.1V |  | Single-phase not supported |
| R51 | Fac | Grid Frequency | uint16 | 0.01Hz |  |  |
| R52 | PF | Power Factor | sint16 | 1E-4 |  |  |
| R53 | RealOPPercent | Actual Output Power Percentage | uint16 | 0.01 | TBC |  |
| R54 | EPS Fac | Off-grid frequency | uint16 | 0.01Hz |  |  |
| R55 | EPS Vac1 | Off-grid R phase output voltage | uint16 | 0.1V |  |  |
| R56 | EPS Iac1 | Off-grid R phase output current | uint16 | 0.1A |  |  |
| R57 | EPS Vac2 | Off-grid S phase output voltage | uint16 | 0.1V |  | Single-phase not supported |
| R58 | EPS Iac2 | Off-grid S phase output current | uint16 | 0.1A |  | Single-phase not supported |
| R59 | EPS Vac3 | Off-grid T-phase output voltage | uint16 | 0.1V |  | Single-phase not supported |
| R60 | EPS Iac3 | Off-grid T-phase output current | uint16 | 0.1A |  | Single-phase not supported |
| R61 | Reserved | Reserved |  |  |  |  |
| R62 | Reserved | Reserved |  |  |  |  |
| R63 | PvNum | PV Paths | uint16 |  |  |  |
| R64 | Vpv1 | PV1 Voltage | uint16 | 0.1V |  |  |
| R65 | PV1Curr | PV1 Input Current | uint16 | 0.1A |  |  |
| R66 | Vpv2 | PV2 Voltage | uint16 | 0.1V |  |  |
| R67 | PV2Curr | PV2 Input Current | uint16 | 0.1A |  |  |
| R68 | Vpv3 | PV3 Voltage | uint16 | 0.1V |  |  |
| R69 | PV3Curr | PV3 Input Current | uint16 | 0.1A |  |  |
| R70 | Vpv4 | PV4 Voltage | uint16 | 0.1V |  |  |
| R71 | PV4Curr | PV4 Input Current | uint16 | 0.1A |  |  |
| R72 | Vpv5 | PV5 Voltage | uint16 | 0.1V |  |  |
| R73 | PV5Curr | PV5 Input Current | uint16 | 0.1A |  |  |
| R74 | Vpv6 | PV6 Voltage | uint16 | 0.1V |  |  |
| R75 | PV6Curr | PV6 Input Current | uint16 | 0.1A |  |  |
| R76 | Vpv7 | PV7 Voltage | uint16 | 0.1V |  |  |
| R77 | PV7Curr | PV7 Input Current | uint16 | 0.1A |  |  |
| R78 | Vpv8 | PV8 Voltage | uint16 | 0.1V |  |  |
| R79 | PV8Curr | PV8 Input Current | uint16 | 0.1A |  |  |
| R80 | Vpv9 | PV9 Voltage | uint16 | 0.1V |  |  |
| R81 | PV9Curr | PV9 Input Current | uint16 | 0.1A |  |  |
| R82 | Vpv10 | PV10 Voltage | uint16 | 0.1V |  |  |
| R83 | PV10Curr | PV10 Input Current | uint16 | 0.1A |  |  |
| R84 | Vpv11 | PV11 Voltage | uint16 | 0.1V |  |  |
| R85 | PV11Curr | PV11 Input Current | uint16 | 0.1A |  |  |
| R86 | Vpv12 | PV12 Voltage | uint16 | 0.1V |  |  |
| R87 | PV12Curr | PV12 Input Current | uint16 | 0.1A |  |  |
| R88 | Vpv13 | PV13 Voltage | uint16 | 0.1V |  |  |
| R89 | PV13Curr | PV13 Input Current | uint16 | 0.1A |  |  |
| R90 | Vpv14 | PV14 Voltage | uint16 | 0.1V |  |  |
| R91 | PV14Curr | PV14 Input Current | uint16 | 0.1A |  |  |
| R92 | Vpv15 | PV15 Voltage | uint16 | 0.1V |  |  |
| R93 | PV15Curr | PV15 Input Current | uint16 | 0.1A |  |  |
| R94 | Vpv16 | PV16 Voltage | uint16 | 0.1V |  |  |
| R95 | PV16Curr | PV16 Input Current | uint16 | 0.1A |  |  |
| R96 | uwGEN_v_R | Diesel generator voltage R | uint16 | 0.1V |  |  |
| R97 | uwGEN_i_R | Diesel generator current R | uint16 | 0.1A |  |  |
| R98 | uwGEN_v_S | Diesel generator voltage S | uint16 | 0.1V |  |  |
| R99 | uwGEN_i_S | Diesel generator current S | uint16 | 0.1A |  |  |
| R100 | uwGEN_v_T | Diesel generator voltage T | uint16 | 0.1V |  |  |
| R101 | uwGEN_i_T | Diesel generator current T | uint16 | 0.1A |  |  |
| R102 | uwGEN_Freq | Diesel Generator Frequency | uint16 | 0.01Hz |  |  |
| R103 | Reserved | Reserved |  |  |  |  |
| R104 | Reserved | Reserved |  |  |  |  |
| R105 | Reserved | Reserved |  |  |  |  |
| R106 | fan_speed | Fan speed | uint16 | rpm | rpm | Single Phase |
| R107 | Time total H | Total working hours | uint32 | 0.5min |  |  |
| R108 | Time total L |  |  |  |  |  |
| R109 | Debuge1 | Main DSP debug parameters | uint16 |  |  |  |
| R110 | Debuge2 | Main DSP debug parameters | uint16 |  |  |  |
| R111 | Debuge3 | Main DSP debug parameters | uint16 |  |  |  |
| R112 | Debuge4 | Main DSP debug parameters | uint16 |  |  |  |
| R113 | Debuge5 | Main DSP debug parameters | uint16 |  |  |  |
| R114 | Debuge6 | Main DSP debug parameters | uint16 |  |  |  |
| R115 | Debuge7 | Main DSP debug parameters | uint16 |  |  |  |
| R116 | Debuge8 | Main DSP debug parameters | uint16 |  |  |  |
| R117 | Debuge9 | Debug parameters from DSP | uint16 |  |  |  |
| R118 | Debuge10 | Debug parameters from DSP | uint16 |  |  |  |
| R119 | Debuge11 | Debug parameters from DSP | uint16 |  |  |  |
| R120 | Debuge12 | Debug parameters from DSP | uint16 |  |  |  |
| R121 | Debuge13 | Debug parameters from DSP | uint16 |  |  |  |
| R122 | Debuge14 | Debug parameters from DSP | uint16 |  |  |  |
| R123 | Debuge15 | Debug parameters from DSP | uint16 |  |  |  |
| R124 | Debuge16 | Debug parameters from DSP | uint16 |  |  |  |
| R125 | Priority | Current Battery Priority | uint16 |  | 0: Load first<br>1: Battery first<br>2: Grid first |  |
| R126 | Battery Type | Battery type | uint16 |  | 0: Lead-acid battery<br>1: Lithium battery<br>2: User-defined 1<br>3: User-defined 2<br>4: User-defined 3 |  |
| R127 | Vbat | Battery Voltage | uint16 | 0.1V | Lithium or Lead Acid |  |
| R128 | SOC | Battery SOC | uint16 | 0.01 | Lithium or Lead Acid |  |
| R129 | BatVolt_DSP | DSP Sampling Battery Voltage | uint16 | 0.1V | Lithium or Lead Acid |  |
| R130 | BMS_Status | Battery status | uint16 |  | Lithium Battery Upload |  |
| R131 | BMS_Error1 | Battery error message 1 | uint16 |  | Lithium Battery Upload |  |
| R132 | BMS_Error2 | Battery error message 2 | uint16 |  | Lithium Battery Upload |  |
| R133 | BMS_Error3 | Battery error message 3 | uint16 |  | Lithium Battery Upload |  |
| R134 | BMS_Error4 | Battery error message 4 | uint16 |  | Lithium Battery Upload |  |
| R135 | BMS_WarnInfo1 | Battery Alarm 1 | uint16 |  | Lithium Battery Upload |  |
| R136 | BMS_WarnInfo2 | Battery Alarm 2 | uint16 |  | Lithium Battery Upload |  |
| R137 | BMS_WarnInfo3 | Battery Alarm 3 | uint16 |  | Lithium Battery Upload |  |
| R138 | BMS_WarnInfo4 | Battery Alarm 4 | uint16 |  | Lithium Battery Upload |  |
| R139 | Reserved | Reserved |  |  |  |  |
| R140 | Reserved | Reserved |  |  |  |  |
| R141 | BMS_BatteryCurr | Battery current | sint16 | 0.01A |  |  |
| R142 | BMS_BatteryTemp | Battery temperature | sint16 | 0.1C° |  |  |
| R143 | BMS_MaxChargeCurr | Maximum allowable charging current of the battery | uint16 | 0.1A |  |  |
| R144 | BMS_MaxDischargeCurr | Maximum allowable discharge current of the battery | uint16 | 0.1A |  |  |
| R145 | BMS_GaugeFCC | Battery rated capacity | uint16 | 0.1Ah |  |  |
| R146 | BMS_GaugeRM | Real-Time Battery Capacity | uint16 | 0.1Ah |  |  |
| R147 | BMS_SoftVersion_Major | Battery Upload Software Major Version | uint16 |  |  |  |
| R148 | BMS_SoftVersion_Minor | Battery Upload Software Minor Version | uint16 |  |  |  |
| R149 | BMS_HardVersion | Battery Upload Hardware Version | uint16 |  |  |  |
| R150 | BMS_DeltaVolt | Battery unit pressure difference | uint16 | 0.001V |  |  |
| R151 | BMS_CycleCnt | Battery Cycles | uint16 |  |  |  |
| R152 | BMS_SOH | SOH | uint16 |  |  |  |
| R153 | BMS_ConstantVolt | Battery Recommended Charging Voltage | uint16 | 0.1V |  |  |
| R154 | uwLVVoltage_Pack | LV Voltage | uint16 | 0.1V |  | Single-phase not supported |
| R155 | BMS_BMSInfo | BMS Information | uint16 |  |  | Single-phase not supported |
| R156 | BMS_PackInfo | Pack Info | uint16 |  |  | Single-phase not supported |
| R157 | MaxCellVol | Battery maximum cell voltage | uint16 | 0.001V |  |  |
| R158 | MinCellVol | Battery minimum cell voltage | uint16 | 0.001V |  |  |
| R159 | ModuleNum | Number of batteries in parallel | uint16 |  |  |  |
| R160 | CellNum | Number of battery cells | uint16 |  |  |  |
| R161 | MaxVoltCellNo | Highest voltage unit number | uint16 |  |  |  |
| R162 | MinVoltCellNo | Minimum Voltage Cell Number | uint16 |  |  |  |
| R163 | MaxTemprCell_10T | Maximum monomer temperature | sint16 | 0.1C° |  |  |
| R164 | MinTemprCell_10T | Minimum monomer temperature | sint16 | 0.1C° |  |  |
| R165 | MaxTemprCellNo | Maximum Voltage Temperature Number | uint16 |  |  |  |
| R166 | MinTemprCellNo | Minimum Voltage Temperature Number | uint16 |  |  |  |
| R167 | Protect pack ID | Faulty Battery Address | uint16 |  |  |  |
| R168 | MaxSOC | Parallel Maximum SOC | uint16 | 0.01 |  |  |
| R169 | MinSOC | Parallel Minimum SOC | uint16 | 0.01 |  |  |
| R170 | BMSCompany | Battery manufacturer information | uint16 |  | 0: null<br>1: Protocol 1<br>2: Protocol 2<br>3: Protocol 3<br>4: Protocol 4 | Single-phase not supported |
| R171 | PowerPackSn | Throughput Display Battery Pack Number | uint16 |  |  | Single-phase not supported |
| R172 | DisChargPower H | Cumulative discharge | uint32 | 0.1kwh |  | Single-phase not supported |
| R173 | DisChargPower L | Cumulative discharge |  |  |  | Single-phase not supported |
| R174 | ChargPower H | Cumulative Charges | uint32 | 0.1kwh |  | Single-phase not supported |
| R175 | ChargPower L | Cumulative Charges |  |  |  | Single-phase not supported |
| R176 | Reserved | Reserved |  |  |  |  |
| R177 | Reserved | Reserved |  |  |  |  |
| R178 | Reserved | Reserved |  |  |  |  |
| R179 | Reserved | Reserved |  |  |  |  |
| R180 | Reserved | Reserved |  |  |  |  |
| R181 | Reserved | Reserved |  |  |  |  |
| R182 | Reserved | Reserved |  |  |  |  |
| R183 | Reserved | Reserved |  |  |  |  |
| R184 | Reserved | Reserved |  |  |  |  |
| R185 | Reserved | Reserved |  |  |  |  |
| R186 | BatDebuge1 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R187 | BatDebuge2 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R188 | BatDebuge3 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R189 | BatDebuge4 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R190 | BatDebuge5 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R191 | BatDebuge6 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R192 | BatDebuge7 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R193 | BatDebuge8 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R194 | BatDebuge9 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R195 | BatDebuge10 | Battery BMS debug parameters | uint16 |  |  | Single-phase not supported |
| R196 |  |  |  |  |  |  |
| R249 | Reserved | Reserved |  |  |  |  |
| R250 | PpvAll H | PV Total Input Power | uint32 | 0.1W |  |  |
| R251 | PpvAll L |  |  |  |  |  |
| R252 | Ppv1 H | PV1 Input Power | uint32 | 0.1W |  |  |
| R253 | Ppv1 L |  |  |  |  |  |
| R254 | Ppv2 H | PV2 Input Power | uint32 | 0.1W |  |  |
| R255 | Ppv2 L |  |  |  |  |  |
| R256 | Ppv3 H | PV3 Input Power | uint32 | 0.1W |  |  |
| R257 | Ppv3 L |  |  |  |  |  |
| R258 | Ppv4 H | PV4 Input Power | uint32 | 0.1W |  |  |
| R259 | Ppv4 L |  |  |  |  |  |
| R260 | Ppv5 H | PV5 Input Power | uint32 | 0.1W |  |  |
| R261 | Ppv5 L |  |  |  |  |  |
| R262 | Ppv6 H | PV6 Input Power | uint32 | 0.1W |  |  |
| R263 | Ppv6 L |  |  |  |  |  |
| R264 | Ppv7 H | PV7 Input Power | uint32 | 0.1W |  |  |
| R265 | Ppv7 L |  |  |  |  |  |
| R266 | Ppv8 H | PV8 Input Power | uint32 | 0.1W |  |  |
| R267 | Ppv8 L |  |  |  |  |  |
| R268 | Ppv9 H | PV9 Input Power | uint32 | 0.1W |  |  |
| R269 | Ppv9 L |  |  |  |  |  |
| R270 | Ppv10 H | PV10 Input Power | uint32 | 0.1W |  |  |
| R271 | Ppv10 L |  |  |  |  |  |
| R272 | Ppv11 H | PV11 Input Power | uint32 | 0.1W |  |  |
| R273 | Ppv11 L |  |  |  |  |  |
| R274 | Ppv12 H | PV12 Input Power | uint32 | 0.1W |  |  |
| R275 | Ppv12 L |  |  |  |  |  |
| R276 | Ppv13 H | PV13 Input Power | uint32 | 0.1W |  |  |
| R277 | Ppv13 L |  |  |  |  |  |
| R278 | Ppv14 H | PV14 Input Power | uint32 | 0.1W |  |  |
| R279 | Ppv14 L |  |  |  |  |  |
| R280 | Ppv15 H | PV15 Input Power | uint32 | 0.1W |  |  |
| R281 | Ppv15 L |  |  |  |  |  |
| R282 | Ppv16 H | PV16 Input Power | uint32 | 0.1W |  |  |
| R283 | Ppv16 L |  |  |  |  |  |
| R284 | SPacAll H | Three-phase output apparent power all | uint32 | 0.1VA |  |  |
| R285 | SPacAll L |  |  |  |  |  |
| R286 | ActPacAll H | Three-phase output power all | sint32 | 0.1W |  |  |
| R287 | ActPacAll L |  |  |  |  |  |
| R288 | ReActPacAll H | Three-phase output reactive power all | sint32 | 0.1var |  |  |
| R289 | ReActPacAll L |  |  |  |  |  |
| R290 | SPac_R H | Three-phase output apparent power R | uint32 | 0.1VA |  |  |
| R291 | SPac_R L |  |  |  |  |  |
| R292 | ActPac_R H | Three-phase output power R | sint32 | 0.1W |  |  |
| R293 | ActPac_R L |  |  |  |  |  |
| R294 | ReActPac_R H | Three-phase output reactive power R | sint32 | 0.1var |  |  |
| R295 | ReActPac_R L |  |  |  |  |  |
| R296 | SPac_S H | Three-phase output apparent power S | uint32 | 0.1VA |  | Single-phase not supported |
| R297 | SPac_S L |  |  |  |  | Single-phase not supported |
| R298 | ActPac_S H | Three-phase output power S | sint32 | 0.1W |  | Single-phase not supported |
| R299 | ActPac_S L |  |  |  |  | Single-phase not supported |
| R300 | ReActPac_S H | Three-phase output reactive power S | sint32 | 0.1var |  | Single-phase not supported |
| R301 | ReActPac_S L |  |  |  |  | Single-phase not supported |
| R302 | SPac_T H | Three-phase output apparent power T | uint32 | 0.1VA |  | Single-phase not supported |
| R303 | SPac_T L |  |  |  |  | Single-phase not supported |
| R304 | ActPac_T H | Three-phase output power T | sint32 | 0.1W |  | Single-phase not supported |
| R305 | ActPac_T L |  |  |  |  | Single-phase not supported |
| R306 | ReActPac_T H | Three-phase output reactive power T | sint32 | 0.1var |  | Single-phase not supported |
| R307 | ReActPac_T L |  |  |  |  | Single-phase not supported |
| R308 | Reserved | Reserved |  |  |  |  |
| R309 | Reserved | Reserved |  |  |  |  |
| R310 | Reserved | Reserved |  |  |  |  |
| R311 | Reserved | Reserved |  |  |  |  |
| R312 | Reserved | Reserved |  |  |  |  |
| R313 | Reserved | Reserved |  |  |  |  |
| R314 | Pactouser R   H | R phase to user power | uint32 | 0.1W |  |  |
| R315 | Pactouser R   L |  |  |  |  |  |
| R316 | Pactouser S   H | S phase to user power | uint32 | 0.1W |  | Single-phase not supported |
| R317 | Pactouser S   L |  |  |  |  | Single-phase not supported |
| R318 | Pactouser T   H | T phase to user power | uint32 | 0.1W |  | Single-phase not supported |
| R319 | Pactouser T   L |  |  |  |  | Single-phase not supported |
| R320 | PactouserTotal H | Total AC power to user | uint32 | 0.1W |  |  |
| R321 | PactouserTotal L |  |  |  |  |  |
| R322 | Pac to grid R  H | AC Side to Grid Power R | uint32 | 0.1W |  |  |
| R323 | Pac to grid R  L |  |  |  |  |  |
| R324 | Pactogrid S  H | AC Side to Grid Power S | uint32 | 0.1W |  | Single-phase not supported |
| R325 | Pactogrid S  L |  |  |  |  | Single-phase not supported |
| R326 | Pactogrid T H | AC Side to Grid Power T | uint32 | 0.1W |  | Single-phase not supported |
| R327 | Pactogrid T L |  |  |  |  | Single-phase not supported |
| R328 | Pactogrid total H | Total AC Side to Grid Power | uint32 | 0.1W |  |  |
| R329 | Pactogrid total L |  |  |  |  |  |
| R330 | PLocalLoad total H | Inverter power to local load total | uint32 | 0.1W |  |  |
| R331 | PLocalLoad total L |  |  |  |  |  |
| R332 | EPS Pac_R H | Off-grid R phase output power | uint32 | 0.1VA |  |  |
| R333 | EPS Pac_R L |  |  |  |  |  |
| R334 | EPS ActPac_R H | Off-grid R phase output active power | uint32 | 0.1W |  |  |
| R335 | EPS ActPac_R L |  |  |  |  |  |
| R336 | EPS Pac_S H | Off-grid S phase output power | uint32 | 0.1VA |  | Single-phase not supported |
| R337 | EPS Pac_S L |  |  |  |  | Single-phase not supported |
| R338 | EPS ActPac_S H | Off-grid S phase output active power | uint32 | 0.1W |  | Single-phase not supported |
| R339 | EPS ActPac_S L |  |  |  |  | Single-phase not supported |
| R340 | EPS Pac_T H | Off-grid T-phase output power | uint32 | 0.1VA |  | Single-phase not supported |
| R341 | EPS Pac_T L |  |  |  |  | Single-phase not supported |
| R342 | EPS ActPac_T H | Off-grid T-phase output active power | uint32 | 0.1W |  | Single-phase not supported |
| R343 | EPS ActPac_T L |  |  |  |  | Single-phase not supported |
| R344 | Loadpercent | Off-grid Output Load Percentage | uint16 | 0.01 |  | Single-phase not supported |
| R345 | PSystem H | System Power Generation | uint32 | 0.1W |  |  |
| R346 | PSystem L |  |  |  |  |  |
| R347 | PSelf H | Spontaneous self-consumption power | uint32 | 0.1W |  |  |
| R348 | PSelf L |  |  |  |  |  |
| R349 | Pdischarge H | Discharge power | uint32 | 0.1W |  |  |
| R350 | Pdischarge L |  |  |  |  |  |
| R351 | Pcharge H | Charging power | uint32 | 0.1W |  |  |
| R352 | Pcharge L |  |  |  |  |  |
| R353 | AC charge Power_H | AC charging power | uint32 | 0.1W |  | Single-phase not supported |
| R354 | AC charge Power_L |  |  |  |  | Single-phase not supported |
| R355 | Extra AC Power to  grid_H | Additional inverter AC power to grid H | uint32 | 0.1W |  | Single-phase not supported |
| R356 | Extra AC Power to  grid_L |  |  |  |  | Single-phase not supported |
| R357 | udGEN_ApparentP_R   H | Diesel generator R apparent power | uint32 | 0.1W |  |  |
| R358 | udGEN_ApparentP_R   L |  |  |  |  |  |
| R359 | udGEN_ApparentP_S   H | Diesel generator S-phase apparent power | uint32 | 0.1W |  |  |
| R360 | udGEN_ApparentP_S   L |  |  |  |  |  |
| R361 | udGEN_ApparentP_T   H | Diesel generator T-phase apparent power | uint32 | 0.1W |  |  |
| R362 | udGEN_ApparentP_T   L |  |  |  |  |  |
| R363 | udGEN_ActiveP_R   H | Diesel Generator R Phase Active Power | uint32 | 0.1W |  |  |
| R364 | udGEN_ActiveP_R   L |  |  |  |  |  |
| R365 | udGEN_ActiveP_S   H | Diesel Generator S Phase Active Power | uint32 | 0.1W |  |  |
| R366 | udGEN_ActiveP_S   L |  |  |  |  |  |
| R367 | udGEN_ActiveP_T   H | Diesel Generator T-Phase Active Power | uint32 | 0.1W |  |  |
| R368 | udGEN_ActiveP_T   L |  |  |  |  |  |
| R375 | Eactoday H | Daily Power Generation | uint32 | 0.1kWh |  |  |
| R376 | Eac today L |  |  |  |  |  |
| R377 | Eac total H | Total Power Generation | uint32 | 0.1kWh |  |  |
| R378 | Eac total L |  |  |  |  |  |
| R379 | EPVAll_Today H | PV Daily Power Generation | uint32 | 0.1kWh |  |  |
| R380 | EPVAll_Today L |  |  |  |  |  |
| R381 | Epv_total H | PV Total Energy | uint32 | 0.1kWh |  |  |
| R382 | Epv_total L |  |  |  |  |  |
| R383 | EChargeToday H | Daily Charge | uint32 | 0.1kWh |  | Single-phase not supported |
| R384 | EChargeToday L |  |  |  |  | Single-phase not supported |
| R385 | EChargeTotal H | Total Charges | uint32 | 0.1kWh |  | Single-phase not supported |
| R386 | EChargeTotal L |  |  |  |  | Single-phase not supported |
| R387 | EDischargeToday H | Daily Discharge Volume | uint32 | 0.1kWh |  |  |
| R388 | EDischargeToday L |  |  |  |  |  |
| R389 | EDischargeTotal H | Total Discharge | uint32 | 0.1kWh |  |  |
| R390 | EDischargeTotal L |  |  |  |  |  |
| R391 | EACharge_Today_H | AC Daily Charge | uint32 | 0.1kWh |  |  |
| R392 | EACharge_Today_L |  |  |  |  |  |
| R393 | EACharge_Total_H | Total AC Charging | uint32 | 0.1kWh |  |  |
| R394 | EACharge_Total_L |  |  |  |  |  |
| R395 | Eextra_today H | External grid-connected inversion energy on the same day | uint32 | 0.1kWh | Meter 2 or CT2 energy statistics | Single-phase not supported |
| R396 | Eextra_today L |  |  |  |  | Single-phase not supported |
| R397 | Eextra_total H | Total external grid-connected inverter energy | uint32 | 0.1kWh |  | Single-phase not supported |
| R398 | Eextra_total L |  |  |  |  | Single-phase not supported |
| R399 | Esystem_today H | System Daily Power Generation | uint32 | 0.1kWh |  |  |
| R400 | Esystem_ today L |  |  |  |  |  |
| R401 | Esystem_total H | Total system power generation | uint32 | 0.1kWh |  |  |
| R402 | Esystem_ total L |  |  |  |  |  |
| R403 | Eself_today H | Spontaneous Daily Electricity Generation | uint32 | 0.1kWh |  |  |
| R404 | Eself_ today L |  |  |  |  |  |
| R405 | Eself_total H | Total spontaneous self-consumption | uint32 | 0.1kWh |  |  |
| R406 | Eself_ total L |  |  |  |  |  |
| R407 | Eload_today H | Load Power Consumption Day | uint32 | 0.1kWh |  |  |
| R408 | Eload_today L |  |  |  |  |  |
| R409 | Eload_total H | Total Power Consumed by Load | uint32 | 0.1kWh |  |  |
| R410 | Eload_total L |  |  |  |  |  |
| R411 | EtoGrid_today H | Infeed Grid Battery Days | uint32 | 0.1kWh |  |  |
| R412 | EtoGrid_today L |  |  |  |  |  |
| R413 | EtoGrid_total H | Total power fed into the grid | uint32 | 0.1kWh |  |  |
| R414 | EtoGrid_total L |  |  |  |  |  |
| R415 | EfromGrid_today H | Grid Intake Day | uint32 | 0.1kWh |  |  |
| R416 | EfromGrid_today L |  |  |  |  |  |
| R417 | EfromGrid_total H | Total grid intake | uint32 | 0.1kWh |  |  |
| R418 | EfromGrid_total L |  |  |  |  |  |
| R419 | dEpvToGridTodayEE | Electricity on the Internet (electricity sold) day | uint32 | 0.1kWh |  |  |
| R420 | dEpvToGridTodayEE |  |  |  |  |  |
| R421 | dEpvToGridTotalEE | Total on-line electricity (electricity sold) | uint32 | 0.1kWh |  |  |
| R422 | dEpvToGridTotalEE |  |  |  |  |  |
| R423 | dEGridToLoadTodayEE | Date of Purchase of Electricity (Buy Electricity) | uint32 | 0.1kWh |  |  |
| R424 | dEGridToLoadTodayEE |  |  |  |  |  |
| R425 | dEGridToLoadTotalEE | Total Electricity Purchased (Bought) | uint32 | 0.1kWh |  |  |
| R426 | dEGridToLoadTotalEE |  |  |  |  |  |
| R427 | dESelfToLoadTodayEE | Self-Sufficiency Battery Day | uint32 | 0.1kWh |  |  |
| R428 | dESelfToLoadTodayEE |  |  |  |  |  |
| R429 | dESelfToLoadTotalEE | Total Self-Sufficiency Battery | uint32 | 0.1kWh |  |  |
| R430 | dESelfToLoadTotalEE |  |  |  |  |  |
| R450 | uwParallelType | Parallel Type | uint16 |  | 0x00: Standalone<br>0x01: Host<br>0x02: Slave |  |
| R451 | HostSerialNum5 | Host serial number 9 ~ 10 characters, indicating the year of machine production | ASCII |  | The last eight characters of the host serial number. If it is the same set of parallel systems, the serial number will be reported from the machine. |  |
| R452 | HostSerialNum6 | Host serial number 11-12 characters, indicating the week in which the machine was manufactured | ASCII |  |  |  |
| R453 | HostSerialNum7 | Host Serial Number 13-14 characters | ASCII |  |  |  |
| R454 | HostSerialNum8 | Host Serial Number 15-16 characters | ASCII |  |  |  |
| R455 | ubParallelDeviceID | Parallel Device ID | uint8 |  | Range: 0 ~ 36 |  |
| R456 | udwParallelPVPower-H | Total Parallel PV Power | uint32 | 0.1W |  |  |
| R457 | udwParallelPVPower-L |  |  |  |  |  |
| R458 | sdwParallelGridPower-H | Total power of parallel electromechanical grids | sint32 | 0.1W | Feed: Negative<br>Take: Positive |  |
| R459 | sdwParallelGridPower-L |  |  |  |  |  |
| R460 | udwParallelLoadPower-H | Total Parallel Load Power | uint32 | 0.1W |  |  |
| R461 | udwParallelLoadPower-L |  |  |  |  |  |
| R462 | sdwParallelBatPower-H | Total Parallel Battery Power | sint32 | 0.1W | Charge: Negative<br>Discharge: Positive |  |
| R463 | sdwParallelBatPower-L |  |  |  |  |  |
| R464 | udwParallelSelfPower-H | Parallel machine self-consumption power | uint32 | 0.1W |  |  |
| R465 | udwParallelSelfPower-L |  |  |  |  |  |
| R466 | udwParallel_EPVToady_H | Parallel PV Daily Power Generation | uint32 | 0.1kWh |  |  |
| R467 | udwParallel_EPVToady_L |  |  |  |  |  |
| R468 | udwParallel_EPVTotal_H | Shutdown PV Total Energy | uint32 | 0.1kWh |  |  |
| R469 | udwParallel_EPVTotal_L |  |  |  |  |  |
| R470 | udwParallel_ESelfToday_H | Parallel machine spontaneous daily power generation | uint32 | 0.1kWh |  |  |
| R471 | udwParallel_ESelfToday_L |  |  |  |  |  |
| R472 | udwParallel_ESelfTotal_H | Total amount of electricity generated by the parallel machine itself | uint32 | 0.1kWh |  |  |
| R473 | udwParallel_ESelfTotal_L |  |  |  |  |  |
| R474 | udwParallel_ELoadToday_H | Parallel Load Power Consumption Day | uint32 | 0.1kWh |  |  |
| R475 | udwParallel_ELoadTotal_H |  |  |  |  |  |
| R476 | udwParallel_ELoadToday_H | Total power consumption of parallel loads | uint32 | 0.1kWh |  |  |
| R477 | udwParallel_ELoadTotal_L |  |  |  |  |  |
| R478 | udwParallel_EPVtoGridToday_H | On-line electricity consumption (electricity sold) days | uint32 | 0.1kWh |  |  |
| R479 | udwParallel_EPVtoGridToday_L |  |  |  |  |  |
| R480 | udwParallel_EPVtoGridTotal_H | Total on-line electricity (electricity sold) | uint32 | 0.1kWh |  |  |
| R481 | udwParallel_EPVtoGridTotal_L |  |  |  |  |  |
| R482 | udwParallel_EGridtoLoadToday_H | Parallel Purchase of Electricity (Purchase of Electricity) Day | uint32 | 0.1kWh |  |  |
| R483 | udwParallel_EGridtoLoadToday_L |  |  |  |  |  |
| R484 | udwParallel_EGridtoLoadTotal_H | Total amount of electricity purchased (purchased) in parallel machines | uint32 | 0.1kWh |  |  |
| R485 | udwParallel_EGridtoLoadTotal_L |  |  |  |  |  |
| R486 | udwParallel_ESelftoLoadToday_H | Parallel Self-Sufficiency Battery Day | uint32 | 0.1kWh |  |  |
| R487 | udwParallel_ESelftoLoadToday_L |  |  |  |  |  |
| R488 | udwParallel_ESelftoLoadTotal_H | Parallel Self-Sufficiency Total Power | uint32 | 0.1kWh |  |  |
| R489 | udwParallel_ESelftoLoadTotal_L |  |  |  |  |  |
| R490 | SOC | Battery SOC | uint16 | 0.01 |  |  |
| R491 | udwParallel_EBatChrToday_H | Parallel Battery Charge Battery Day | uint32 | 0.1kWh |  |  |
| R492 | udwParallel_EBatChrToday_L |  |  |  |  |  |
| R493 | udwParallel_EBatChrTotal_H | Total charge amount of parallel battery | uint32 | 0.1kWh |  |  |
| R494 | udwParallel_EBatChrTotal_L |  |  |  |  |  |
| R495 | udwParallel_EBatDisChrToday_H | Parallel battery discharge amount day | uint32 | 0.1kWh |  |  |
| R496 | udwParallel_EBatDisChrToday_L |  |  |  |  |  |
| R497 | udwParallel_EBatDisChrTotal_H | Total discharge capacity of parallel battery | uint32 | 0.1kWh |  |  |
| R498 | udwParallel_EBatDisChrTotal_L |  |  |  |  |  |
