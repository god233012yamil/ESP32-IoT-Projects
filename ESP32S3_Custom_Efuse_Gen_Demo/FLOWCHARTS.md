# ESP32-S3 Custom eFuse Demo - Project Flowcharts

## Complete System Flow Diagram

```mermaid
graph TB
    subgraph "1. Build Time - Code Generation"
        A1[CMake Configuration] --> A2[Check for Custom CSV]
        A2 --> A3[efuse_table_gen.py]
        A3 --> A4[Parse Common Table<br/>esp_efuse_table.csv]
        A3 --> A5[Parse Custom Table<br/>esp_efuse_custom_table.csv]
        A4 --> A6[Validate Fields]
        A5 --> A6
        A6 --> A7{Valid?}
        A7 -->|Yes| A8[Generate C/H Files]
        A7 -->|No| A9[Build Error]
        A8 --> A10[esp_efuse_custom_table.c]
        A8 --> A11[esp_efuse_custom_table.h]
        A10 --> A12[Compile & Link]
        A11 --> A12
    end
    
    subgraph "2. Runtime - Application Flow"
        B1[app_main Start] --> B2{CONFIG_DEMO_PROGRAM_EFUSE?}
        B2 -->|Enabled| B3[Provisioning Mode]
        B2 -->|Disabled| B4[Read-Only Mode]
        
        B3 --> B5[Build Data Payload]
        B5 --> B6[Calculate CRC-16]
        B6 --> B7[Batch Write Begin]
        B7 --> B8[Write All Fields]
        B8 --> B9[Batch Write Commit]
        B9 --> B10[Verify Write]
        
        B4 --> B11[Read eFuse Fields]
        B10 --> B11
        B11 --> B12[Validate CRC]
        B12 --> B13[Display Results]
        B13 --> B14[Idle Loop]
    end
    
    subgraph "3. eFuse Access Layer"
        C1[Application API Call] --> C2{Virtual Mode?}
        C2 -->|Yes| C3[Virtual eFuse Handler]
        C2 -->|No| C4[Hardware eFuse Handler]
        C3 --> C5[RAM/Flash Storage]
        C4 --> C6[Reed-Solomon Encoder]
        C6 --> C7[Hardware Registers]
    end
    
    A12 --> B1
    B8 --> C1
    B11 --> C1
    
    style A8 fill:#e1f5ff
    style A10 fill:#e8f5e9
    style A11 fill:#e8f5e9
    style B3 fill:#fff4e6
    style B7 fill:#ffebee
    style C2 fill:#f3e5f5
    style C6 fill:#ffe0b2
```

## Detailed Build Process Flow

```mermaid
flowchart TD
    Start([Build Start]) --> SetupEnv[Setup ESP-IDF Environment]
    SetupEnv --> CMakeConfig[CMake Configuration Phase]
    
    CMakeConfig --> CheckCSV{esp_efuse_custom_table.csv<br/>exists?}
    CheckCSV -->|No| BuildError1[❌ Build Error:<br/>Missing CSV File]
    CheckCSV -->|Yes| CreateIncDir[Create include/ Directory]
    
    CreateIncDir --> SetVars[Set Path Variables:<br/>CUSTOM_EFUSE_CSV<br/>GEN_C, GEN_H<br/>EFUSE_GEN_SCRIPT]
    
    SetVars --> GetCommonTable[Get Common Table Path:<br/>IDF_PATH/components/efuse/<br/>IDF_TARGET/esp_efuse_table.csv]
    
    GetCommonTable --> RunGenerator[Execute efuse_table_gen.py]
    
    RunGenerator --> ParseCommon[Parse Common eFuse Table<br/>System fields:<br/>MAC, Encryption, etc.]
    
    RunGenerator --> ParseCustom[Parse Custom CSV<br/>USER_DATA.* fields]
    
    ParseCommon --> Validate[Validate All Fields]
    ParseCustom --> Validate
    
    Validate --> CheckOverlap{Any bit overlaps<br/>or errors?}
    CheckOverlap -->|Yes| BuildError2[❌ Build Error:<br/>Field Validation Failed]
    CheckOverlap -->|No| GenerateC[Generate C Source:<br/>esp_efuse_custom_table.c]
    
    GenerateC --> GenerateH[Generate Header:<br/>esp_efuse_custom_table.h]
    
    GenerateH --> CreateDescriptors[Create Field Descriptors:<br/>ESP_EFUSE_USER_DATA_SERIAL_NUMBER<br/>ESP_EFUSE_USER_DATA_HW_REV<br/>ESP_EFUSE_USER_DATA_FEATURE_FLAGS<br/>ESP_EFUSE_USER_DATA_PROVISIONING_CRC16]
    
    CreateDescriptors --> AddToComponent[Add Generated C to Component Sources]
    
    AddToComponent --> CompileGenerated[Compile Generated Code]
    
    CompileGenerated --> CompileMain[Compile main.c]
    
    CompileMain --> LinkAll[Link All Components]
    
    LinkAll --> CreateBinary[Create Firmware Binary]
    
    CreateBinary --> BuildSuccess([✅ Build Complete])
    
    BuildError1 --> End([End])
    BuildError2 --> End
    BuildSuccess --> End
    
    style Start fill:#4caf50,color:#fff
    style BuildSuccess fill:#4caf50,color:#fff
    style BuildError1 fill:#f44336,color:#fff
    style BuildError2 fill:#f44336,color:#fff
    style RunGenerator fill:#2196f3,color:#fff
    style GenerateC fill:#ffeb3b
    style GenerateH fill:#ffeb3b
    style CreateDescriptors fill:#ff9800
    style End fill:#9e9e9e,color:#fff
```

## Runtime Application Flow

```mermaid
flowchart TD
    Start([System Boot]) --> InitHW[Hardware Initialization]
    InitHW --> AppMain[app_main Entry Point]
    
    AppMain --> LogStart[Log: Demo Starting]
    LogStart --> CheckConfig{CONFIG_DEMO_PROGRAM_EFUSE<br/>enabled?}
    
    CheckConfig -->|Yes| LogWarning[⚠️ Log: Programming Enabled]
    CheckConfig -->|No| ReadFields[Read eFuse Fields]
    
    LogWarning --> DefineData[Define Provisioning Data:<br/>Serial: SN-ESP32S3-0001<br/>HW Rev: 0x0001<br/>Flags: 0x0000000F]
    
    DefineData --> BuildPayload[Build 22-byte Payload:<br/>Serial16 + HwRev2 + Flags4]
    
    BuildPayload --> CalcCRC[Calculate CRC-16/CCITT-FALSE<br/>over payload]
    
    CalcCRC --> BatchBegin[esp_efuse_batch_write_begin]
    
    BatchBegin --> CheckBatchErr1{Success?}
    CheckBatchErr1 -->|No| ErrorLog1[❌ Log: Batch Begin Failed]
    CheckBatchErr1 -->|Yes| WriteSerial[Write SERIAL_NUMBER<br/>128 bits]
    
    WriteSerial --> CheckWriteErr1{Success?}
    CheckWriteErr1 -->|No| BatchCancel1[esp_efuse_batch_write_cancel]
    CheckWriteErr1 -->|Yes| WriteHwRev[Write HW_REV<br/>16 bits]
    
    WriteHwRev --> CheckWriteErr2{Success?}
    CheckWriteErr2 -->|No| BatchCancel2[esp_efuse_batch_write_cancel]
    CheckWriteErr2 -->|Yes| WriteFlags[Write FEATURE_FLAGS<br/>32 bits]
    
    WriteFlags --> CheckWriteErr3{Success?}
    CheckWriteErr3 -->|No| BatchCancel3[esp_efuse_batch_write_cancel]
    CheckWriteErr3 -->|Yes| WriteCRC[Write PROVISIONING_CRC16<br/>16 bits]
    
    WriteCRC --> CheckWriteErr4{Success?}
    CheckWriteErr4 -->|No| BatchCancel4[esp_efuse_batch_write_cancel]
    CheckWriteErr4 -->|Yes| BatchCommit[esp_efuse_batch_write_commit]
    
    BatchCommit --> CheckCommitErr{Success?}
    CheckCommitErr -->|No| ErrorLog2[❌ Log: Commit Failed]
    CheckCommitErr -->|Yes| LogCommit[✅ Log: Provisioning Committed]
    
    BatchCancel1 --> ErrorLog1
    BatchCancel2 --> ErrorLog1
    BatchCancel3 --> ErrorLog1
    BatchCancel4 --> ErrorLog1
    ErrorLog1 --> ReadFields
    ErrorLog2 --> ReadFields
    LogCommit --> ReadFields
    
    ReadFields --> ReadSerial[Read SERIAL_NUMBER<br/>esp_efuse_read_field_blob]
    ReadSerial --> ReadHwRev[Read HW_REV<br/>esp_efuse_read_field_blob]
    ReadHwRev --> ReadFlags2[Read FEATURE_FLAGS<br/>esp_efuse_read_field_blob]
    ReadFlags2 --> ReadCRCStored[Read PROVISIONING_CRC16<br/>esp_efuse_read_field_blob]
    
    ReadCRCStored --> ConvertSerial[Convert Serial to String]
    ConvertSerial --> LogValues[Log All Field Values:<br/>SERIAL, HW_REV, FLAGS, CRC]
    
    LogValues --> RebuildPayload[Rebuild 22-byte Payload<br/>from read values]
    RebuildPayload --> RecalcCRC[Recalculate CRC-16]
    RecalcCRC --> LogRecalc[Log: CRC Recalculated]
    
    LogRecalc --> CheckCRC{Compare CRC values}
    CheckCRC -->|CRC = 0x0000| LogNotProvisioned[⚠️ Log: Not Provisioned Yet]
    CheckCRC -->|CRC Mismatch| LogMismatch[⚠️ Log: CRC Mismatch]
    CheckCRC -->|CRC Match| LogOK[✅ Log: CRC Check OK]
    
    LogNotProvisioned --> IdleLoop[Infinite Idle Loop:<br/>vTaskDelay 2000ms]
    LogMismatch --> IdleLoop
    LogOK --> IdleLoop
    
    IdleLoop --> IdleLoop
    
    style Start fill:#4caf50,color:#fff
    style LogWarning fill:#ff9800,color:#fff
    style BatchBegin fill:#2196f3,color:#fff
    style BatchCommit fill:#2196f3,color:#fff
    style ErrorLog1 fill:#f44336,color:#fff
    style ErrorLog2 fill:#f44336,color:#fff
    style LogCommit fill:#4caf50,color:#fff
    style LogOK fill:#4caf50,color:#fff
    style LogMismatch fill:#ff9800,color:#fff
    style LogNotProvisioned fill:#ff9800,color:#fff
    style IdleLoop fill:#9e9e9e,color:#fff
```

## eFuse Write Operation (Batch Mode Detail)

```mermaid
sequenceDiagram
    participant App as Application Code
    participant API as ESP-IDF eFuse API
    participant Batch as Batch Manager
    participant RS as Reed-Solomon Encoder
    participant HW as Hardware Registers
    participant Virt as Virtual Storage

    Note over App,Virt: Batch Write Mode (Required for USER_DATA blocks)
    
    App->>API: esp_efuse_batch_write_begin()
    API->>Batch: Create staging buffer
    Batch->>Batch: Allocate memory for batch
    API-->>App: ESP_OK
    
    Note over App,Batch: Stage all field writes
    
    App->>API: esp_efuse_write_field_blob(SERIAL_NUMBER, data, 128 bits)
    API->>Batch: Stage bits [0:127] in EFUSE_BLK3
    Batch->>Batch: Validate bit range
    Batch->>Batch: Mark bits for programming
    API-->>App: ESP_OK
    
    App->>API: esp_efuse_write_field_blob(HW_REV, data, 16 bits)
    API->>Batch: Stage bits [128:143] in EFUSE_BLK3
    Batch->>Batch: Validate no overlap
    API-->>App: ESP_OK
    
    App->>API: esp_efuse_write_field_blob(FEATURE_FLAGS, data, 32 bits)
    API->>Batch: Stage bits [144:175] in EFUSE_BLK3
    Batch->>Batch: Validate no overlap
    API-->>App: ESP_OK
    
    App->>API: esp_efuse_write_field_blob(CRC16, data, 16 bits)
    API->>Batch: Stage bits [176:191] in EFUSE_BLK3
    Batch->>Batch: Validate no overlap
    API-->>App: ESP_OK
    
    Note over App,HW: Commit - Actual programming happens here
    
    App->>API: esp_efuse_batch_write_commit()
    API->>Batch: Get all staged bits
    Batch-->>API: Complete block data
    
    alt Virtual eFuse Mode
        API->>Virt: Write to virtual storage
        Virt->>Virt: Update RAM/Flash image
        Virt-->>API: Success
    else Real eFuse Mode
        API->>RS: Encode block with RS ECC
        RS->>RS: Calculate parity bits
        RS-->>API: Encoded data + ECC
        API->>HW: Burn encoded bits to silicon
        HW->>HW: Program eFuse registers
        HW->>HW: Verify programming
        HW-->>API: Programming complete
    end
    
    API->>Batch: Clear staging buffer
    API-->>App: ESP_OK ✅
    
    Note over App,Virt: Bits are now permanent (in real mode)
```

## eFuse Read Operation

```mermaid
sequenceDiagram
    participant App as Application Code
    participant API as ESP-IDF eFuse API
    participant RS as Reed-Solomon Decoder
    participant HW as Hardware Registers
    participant Virt as Virtual Storage

    App->>API: esp_efuse_read_field_blob(SERIAL_NUMBER, buffer, 128)
    
    alt Virtual eFuse Mode
        API->>Virt: Read from virtual storage
        Virt-->>API: Raw bits
    else Real eFuse Mode
        API->>HW: Read EFUSE_BLK3 registers
        HW-->>API: Raw block data + ECC
        API->>RS: Decode with error correction
        RS->>RS: Check ECC, correct errors
        RS-->>API: Corrected data or error
    end
    
    API->>API: Extract bits [0:127]
    API->>API: Copy to user buffer
    API-->>App: ESP_OK, data in buffer
    
    App->>API: esp_efuse_read_field_blob(HW_REV, buffer, 16)
    
    alt Virtual eFuse Mode
        API->>Virt: Read from virtual storage
        Virt-->>API: Raw bits
    else Real eFuse Mode
        API->>HW: Read EFUSE_BLK3 registers (cached)
        HW-->>API: Raw block data
    end
    
    API->>API: Extract bits [128:143]
    API->>API: Copy to user buffer
    API-->>App: ESP_OK, data in buffer
    
    Note over App,Virt: Similar process for all other fields
```

## CRC-16 Calculation Flow

```mermaid
flowchart LR
    subgraph Input["Input Data (22 bytes)"]
        I1[Serial Number<br/>16 bytes]
        I2[HW Revision<br/>2 bytes]
        I3[Feature Flags<br/>4 bytes]
    end
    
    subgraph CRC["CRC-16/CCITT-FALSE"]
        C1[Initialize:<br/>CRC = 0xFFFF]
        C2[For each byte:<br/>CRC ^= byte << 8]
        C3[For each bit:<br/>shift and XOR<br/>with 0x1021]
        C4[Final CRC<br/>16 bits]
    end
    
    subgraph Storage["Store in eFuse"]
        S1[Little-Endian<br/>Conversion]
        S2[Write to<br/>PROVISIONING_CRC16]
    end
    
    subgraph Verify["Verification"]
        V1[Read all fields]
        V2[Reconstruct<br/>22-byte payload]
        V3[Recalculate CRC]
        V4{Compare}
        V5[✅ Match]
        V6[❌ Mismatch]
    end
    
    I1 --> C1
    I2 --> C1
    I3 --> C1
    C1 --> C2
    C2 --> C3
    C3 --> C4
    C4 --> S1
    S1 --> S2
    
    S2 -.Read back.-> V1
    V1 --> V2
    V2 --> V3
    V3 --> V4
    V4 -->|Same| V5
    V4 -->|Different| V6
    
    style C4 fill:#4caf50,color:#fff
    style V5 fill:#4caf50,color:#fff
    style V6 fill:#f44336,color:#fff
```

## State Machine: eFuse Provisioning States

```mermaid
stateDiagram-v2
    [*] --> Unprogrammed: Device Fresh from Factory
    
    Unprogrammed --> Programming: User enables<br/>CONFIG_DEMO_PROGRAM_EFUSE
    Unprogrammed --> ReadOnly: Normal operation
    
    Programming --> BatchWriteActive: esp_efuse_batch_write_begin()
    
    BatchWriteActive --> StagingSerial: Write SERIAL_NUMBER
    StagingSerial --> StagingHwRev: Write HW_REV
    StagingHwRev --> StagingFlags: Write FEATURE_FLAGS
    StagingFlags --> StagingCRC: Write CRC16
    
    StagingCRC --> Committing: esp_efuse_batch_write_commit()
    
    Committing --> Programmed: Success ✅
    Committing --> ErrorState: Failure ❌
    
    ErrorState --> ReadOnly: Cancel batch,<br/>continue with old values
    
    Programmed --> ReadOnly: Verification passed
    ReadOnly --> ValidatingCRC: Read fields,<br/>calculate CRC
    
    ValidatingCRC --> CRCValid: CRC Match ✅
    ValidatingCRC --> CRCInvalid: CRC Mismatch ⚠️
    ValidatingCRC --> NotProvisioned: CRC = 0x0000
    
    CRCValid --> Running: Normal Operation
    CRCInvalid --> Running: Warning logged
    NotProvisioned --> Running: Warning logged
    
    Running --> [*]: Idle Loop
    
    note right of Unprogrammed
        All eFuse bits = 0
        CRC16 = 0x0000
    end note
    
    note right of Programmed
        eFuse bits permanently set
        Cannot be reversed!
    end note
    
    note right of Running
        Application continues
        vTaskDelay loop
    end note
```

## Data Structure: eFuse Block Layout (EFUSE_BLK3)

```mermaid
graph TD
    subgraph "EFUSE_BLK3 (USER_DATA) - 256 bits total"
        subgraph "Bits 0-127: SERIAL_NUMBER"
            SN[16 bytes ASCII<br/>Zero-padded]
        end
        
        subgraph "Bits 128-143: HW_REV"
            HW[2 bytes<br/>uint16]
        end
        
        subgraph "Bits 144-175: FEATURE_FLAGS"
            FF[4 bytes<br/>uint32]
        end
        
        subgraph "Bits 176-191: PROVISIONING_CRC16"
            CRC[2 bytes<br/>uint16]
        end
        
        subgraph "Bits 192-255: Reserved"
            RSV[64 bits<br/>Available for expansion]
        end
    end
    
    subgraph "Reed-Solomon ECC"
        ECC[Additional parity bits<br/>stored in hardware<br/>for error correction]
    end
    
    SN --> ECC
    HW --> ECC
    FF --> ECC
    CRC --> ECC
    RSV --> ECC
    
    style SN fill:#e3f2fd
    style HW fill:#fff3e0
    style FF fill:#f3e5f5
    style CRC fill:#e8f5e9
    style RSV fill:#fce4ec
    style ECC fill:#fff9c4
```

## Error Handling Flow

```mermaid
flowchart TD
    Start([Function Call]) --> Try{Try Operation}
    
    Try -->|Success| CheckResult{Check Result}
    Try -->|Exception| LogException[Log Exception]
    
    CheckResult -->|ESP_OK| Success[Return Success]
    CheckResult -->|ESP_ERR_EFUSE_CNT_IS_FULL| ErrFull[❌ eFuse bits already set<br/>Cannot reprogram]
    CheckResult -->|ESP_ERR_INVALID_ARG| ErrArg[❌ Invalid arguments<br/>Check parameters]
    CheckResult -->|ESP_ERR_INVALID_STATE| ErrState[❌ Invalid state<br/>Batch not begun?]
    CheckResult -->|ESP_FAIL| ErrGeneric[❌ Generic failure<br/>Check hardware]
    
    ErrFull --> InBatch{In batch mode?}
    ErrArg --> InBatch
    ErrState --> InBatch
    ErrGeneric --> InBatch
    
    InBatch -->|Yes| CancelBatch[esp_efuse_batch_write_cancel]
    InBatch -->|No| LogError[Log Error Message]
    
    CancelBatch --> LogError
    LogException --> LogError
    
    LogError --> Cleanup[Cleanup Resources]
    Success --> End([Return to Caller])
    Cleanup --> End
    
    style Success fill:#4caf50,color:#fff
    style ErrFull fill:#f44336,color:#fff
    style ErrArg fill:#ff9800,color:#fff
    style ErrState fill:#ff9800,color:#fff
    style ErrGeneric fill:#f44336,color:#fff
    style CancelBatch fill:#2196f3,color:#fff
```
