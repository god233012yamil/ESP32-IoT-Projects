# ESP32-S3 LittleFS Demo - Flowchart

This document contains the detailed flowchart of the LittleFS demo application.

## Main Application Flow

```mermaid
flowchart TD
    A[Start: app_main] --> B[Mount LittleFS]
    B --> C{Mount Success?}
    C -->|No| D[Log Error & Exit]
    C -->|Yes| E[Show Initial FS Info]
    
    E --> F[Create /littlefs/config directory]
    F --> G[Create /littlefs/logs directory]
    
    G --> H[Write device.cfg file<br/>Initial content]
    H --> I[Append to device.cfg<br/>Add log_enabled]
    I --> J[Append to boot.log<br/>Write boot=ok]
    
    J --> K[Read & Display device.cfg]
    
    K --> L[List /littlefs directory]
    L --> M[List /littlefs/config directory]
    M --> N[List /littlefs/logs directory]
    
    N --> O[Show FS Usage Stats]
    
    O --> P{Loop Counter < 5?}
    P -->|Yes| Q[Append tick=N to boot.log]
    Q --> R[Wait 1 second]
    R --> S[Increment Counter]
    S --> P
    
    P -->|No| T[Read & Display boot.log<br/>Final state]
    T --> U[Show Final FS Usage]
    U --> V[Unmount LittleFS]
    V --> W[Demo Complete]
    
    style A fill:#e1f5ff
    style D fill:#ffcdd2
    style W fill:#c8e6c9
    style B fill:#fff9c4
    style O fill:#fff9c4
    style U fill:#fff9c4
```

## Filesystem Mount Process

```mermaid
flowchart TD
    A[littlefs_mount Function] --> B[Configure LittleFS<br/>base_path: /littlefs<br/>partition_label: littlefs<br/>format_if_mount_failed: true]
    B --> C[esp_vfs_littlefs_register]
    C --> D{Registration OK?}
    D -->|No| E[Log Error<br/>Return Error Code]
    D -->|Yes| F[Query FS Info<br/>esp_littlefs_info]
    F --> G{Query Success?}
    G -->|No| H[Log Warning<br/>Continue Anyway]
    G -->|Yes| I[Log Total & Used Space]
    H --> J[Return ESP_OK]
    I --> J
    
    style A fill:#e1f5ff
    style E fill:#ffcdd2
    style J fill:#c8e6c9
```

## Directory Creation Process

```mermaid
flowchart TD
    A[ensure_dir Function] --> B[stat path]
    B --> C{Path Exists?}
    C -->|Yes| D{Is Directory?}
    D -->|Yes| E[Return: Already Exists]
    D -->|No| F[Log Warning:<br/>Path exists but not a dir]
    C -->|No| G[mkdir path with 0775 permissions]
    G --> H{mkdir Success?}
    H -->|Yes| I[Log: Directory Created]
    H -->|No| J[Log Error with errno]
    
    style A fill:#e1f5ff
    style E fill:#c8e6c9
    style F fill:#fff9c4
    style I fill:#c8e6c9
    style J fill:#ffcdd2
```

## File Write Process

```mermaid
flowchart TD
    A[write_text_file Function] --> B[fopen path mode 'w']
    B --> C{Open Success?}
    C -->|No| D[Log Error with errno<br/>Return]
    C -->|Yes| E[fwrite text to file]
    E --> F[fclose file]
    F --> G[Log: Bytes Written]
    
    style A fill:#e1f5ff
    style D fill:#ffcdd2
    style G fill:#c8e6c9
```

## File Append Process

```mermaid
flowchart TD
    A[append_text_file Function] --> B[fopen path mode 'a']
    B --> C{Open Success?}
    C -->|No| D[Log Error with errno<br/>Return]
    C -->|Yes| E[fwrite text to file]
    E --> F[fclose file]
    F --> G[Log: Bytes Appended]
    
    style A fill:#e1f5ff
    style D fill:#ffcdd2
    style G fill:#c8e6c9
```

## File Read Process

```mermaid
flowchart TD
    A[read_text_file Function] --> B[fopen path mode 'r']
    B --> C{Open Success?}
    C -->|No| D[Log Error with errno<br/>Return]
    C -->|Yes| E[Log: Begin File]
    E --> F[fgets line into buffer]
    F --> G{EOF Reached?}
    G -->|No| H[Log Line Content]
    H --> F
    G -->|Yes| I[fclose file]
    I --> J[Log: End File]
    
    style A fill:#e1f5ff
    style D fill:#ffcdd2
    style J fill:#c8e6c9
```

## Directory Listing Process

```mermaid
flowchart TD
    A[list_dir Function] --> B[opendir dirpath]
    B --> C{Open Success?}
    C -->|No| D[Log Error with errno<br/>Return]
    C -->|Yes| E[Log: Directory Listing]
    E --> F[readdir next entry]
    F --> G{Entry Exists?}
    G -->|No| H[closedir]
    G -->|Yes| I{Skip . or ..?}
    I -->|Yes| F
    I -->|No| J[Build Full Path]
    J --> K[stat Full Path]
    K --> L{stat Success?}
    L -->|Yes| M[Determine Type: DIR/FILE<br/>Get Size]
    L -->|No| N[Log: Unknown Type]
    M --> O[Log: Type Name Size]
    N --> O
    O --> F
    H --> P[End]
    
    style A fill:#e1f5ff
    style D fill:#ffcdd2
    style P fill:#c8e6c9
```

## Filesystem Info Query

```mermaid
flowchart TD
    A[show_fs_info Function] --> B[esp_littlefs_info<br/>partition_label: littlefs]
    B --> C{Query Success?}
    C -->|No| D[Log Error<br/>Return]
    C -->|Yes| E[Calculate Free Space<br/>free = total - used]
    E --> F[Log: Used / Total / Free]
    
    style A fill:#e1f5ff
    style D fill:#ffcdd2
    style F fill:#c8e6c9
```

## Unmount Process

```mermaid
flowchart TD
    A[littlefs_unmount Function] --> B[esp_vfs_littlefs_unregister<br/>partition_label: littlefs]
    B --> C[Log: LittleFS Unmounted]
    C --> D[Return]
    
    style A fill:#e1f5ff
    style D fill:#c8e6c9
```

## Error Handling Flow

```mermaid
flowchart TD
    A[Error Detected] --> B{Error Type}
    B -->|Mount Failed| C[Log Error<br/>Exit Application]
    B -->|File Operation| D[Log Error with errno<br/>Continue Execution]
    B -->|Directory Operation| E[Log Warning/Error<br/>Continue Execution]
    B -->|Query Failed| F[Log Warning<br/>Continue Execution]
    
    style A fill:#ffcdd2
    style C fill:#ffcdd2
    style D fill:#fff9c4
    style E fill:#fff9c4
    style F fill:#fff9c4
```

## Legend

- **Blue boxes**: Entry points and start states
- **Red boxes**: Error states and exits
- **Green boxes**: Success states and completions
- **Yellow boxes**: Information/status operations
- **White boxes**: Normal processing steps
