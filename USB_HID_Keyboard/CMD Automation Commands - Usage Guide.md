# CMD Automation Commands - Complete Guide

## New Commands Added ✨

I've added **4 new commands** for Windows Command Prompt automation:

| Command | Function | Description |
|---------|----------|-------------|
| **13** | `cmd_open_and_execute_cmd()` | Opens CMD and runs a single command |
| **14** | `cmd_open_cmd_as_admin()` | Opens CMD with administrator privileges |
| **15** | `cmd_execute_multiple_commands()` | Chains multiple commands together |
| **16** | `cmd_run_powershell_from_cmd()` | Runs PowerShell commands from CMD |

---

## Command 13: Open CMD and Execute Command

### What It Does
1. Opens Run dialog (Win+R)
2. Types "cmd" and presses Enter
3. Waits for CMD to open
4. Types a command (example: `ipconfig`)
5. Executes the command

### Code
```c
static void cmd_open_and_execute_cmd(void) {
    ESP_LOGI(TAG, "Command 13: Opening CMD and executing command...");
    
    // Open Run dialog
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Type "cmd"
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Open CMD
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Execute command
    keyboard_type_string("ipconfig");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}
```

### Customization Examples

#### Example 1: Check Network Configuration
```c
keyboard_type_string("ipconfig /all");
```

#### Example 2: Ping a Server
```c
keyboard_type_string("ping google.com");
```

#### Example 3: List Directory Contents
```c
keyboard_type_string("dir");
```

#### Example 4: Check System Info
```c
keyboard_type_string("systeminfo");
```

#### Example 5: Display Current Date/Time
```c
keyboard_type_string("date /t && time /t");
```

---

## Command 14: Open CMD as Administrator

### What It Does
1. Opens Run dialog (Win+R)
2. Types "cmd"
3. Presses **Ctrl+Shift+Enter** (admin shortcut)
4. Waits for UAC prompt
5. Presses Enter to accept (if "Yes" is highlighted)
6. Executes an admin command (example: `systeminfo`)

### Code
```c
static void cmd_open_cmd_as_admin(void) {
    ESP_LOGI(TAG, "Command 14: Opening CMD as Administrator...");
    
    // Open Run dialog
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Type "cmd"
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Press Ctrl+Shift+Enter (opens as admin)
    hid_keyboard_report_t report = {0};
    report.modifier = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT;
    report.keycode[0] = HID_KEY_ENTER;
    send_keyboard_report(&report);
    vTaskDelay(pdMS_TO_TICKS(KEY_PRESS_DELAY_MS));
    keyboard_release_all();
    
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));  // Wait for UAC
    
    // Accept UAC prompt
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Execute admin command
    keyboard_type_string("systeminfo");
    keyboard_press_key(0, HID_KEY_ENTER);
}
```

### ⚠️ Important Notes

**UAC (User Account Control) Interaction:**
- This command requires UAC prompt acceptance
- The code assumes "Yes" button is already highlighted
- May need manual intervention depending on UAC settings

**Admin Commands Examples:**

#### Manage Windows Services
```c
keyboard_type_string("net start");  // List running services
```

#### Flush DNS Cache
```c
keyboard_type_string("ipconfig /flushdns");
```

#### Check Disk
```c
keyboard_type_string("chkdsk C:");
```

#### System File Checker
```c
keyboard_type_string("sfc /scannow");
```

### Why It's Commented Out by Default
```c
// cmd_open_cmd_as_admin();  // Requires UAC interaction
```

This command is **commented out** in the automation sequence because:
1. Requires user to click UAC prompt
2. UAC position varies by Windows version
3. Can't be fully automated without security implications

**Enable it if:**
- You've disabled UAC (not recommended)
- You want to manually accept UAC
- You're using it in a controlled environment

---

## Command 15: Execute Multiple Commands

### What It Does
1. Opens CMD
2. Chains multiple commands using `&&` operator
3. Example: Creates folder, navigates to it, creates file
4. Lists directory contents with `dir`

### Code
```c
static void cmd_execute_multiple_commands(void) {
    ESP_LOGI(TAG, "Command 15: Executing multiple CMD commands...");
    
    // Open CMD
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_type_string("cmd");
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Chain multiple commands
    keyboard_type_string("cd Desktop && mkdir TestFolder && cd TestFolder && echo Hello World > test.txt");
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Show results
    keyboard_type_string("dir");
    keyboard_press_key(0, HID_KEY_ENTER);
}
```

### Command Chaining Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `&&` | Run next if previous succeeds | `cd folder && dir` |
| `||` | Run next if previous fails | `ping server || echo Failed` |
| `&` | Run next regardless | `echo A & echo B` |
| `|` | Pipe output to next command | `dir | find "txt"` |

### Practical Examples

#### Example 1: System Maintenance
```c
keyboard_type_string("cd C:\\ && dir /s *.tmp && del /s /q *.tmp");
```

#### Example 2: Network Diagnostics
```c
keyboard_type_string("ipconfig /release && ipconfig /renew && ipconfig /flushdns");
```

#### Example 3: Backup Files
```c
keyboard_type_string("cd Documents && mkdir Backup && xcopy *.txt Backup\\");
```

#### Example 4: Create Project Structure
```c
keyboard_type_string("mkdir MyProject && cd MyProject && mkdir src && mkdir bin && mkdir docs");
```

#### Example 5: Check Disk Space
```c
keyboard_type_string("wmic logicaldisk get size,freespace,caption");
```

---

## Command 16: Run PowerShell from CMD

### What It Does
1. Opens CMD
2. Runs PowerShell commands using `powershell` command
3. Example: Gets current date with `Get-Date`

### Code
```c
static void cmd_run_powershell_from_cmd(void) {
    ESP_LOGI(TAG, "Command 16: Running PowerShell command from CMD...");
    
    // Open CMD
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_type_string("cmd");
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Run PowerShell command
    keyboard_type_string("powershell Get-Date");
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}
```

### PowerShell Command Examples

#### Example 1: Get System Information
```c
keyboard_type_string("powershell Get-ComputerInfo");
```

#### Example 2: List Running Processes
```c
keyboard_type_string("powershell Get-Process | Select-Object -First 10");
```

#### Example 3: Check CPU Usage
```c
keyboard_type_string("powershell Get-Counter '\\Processor(_Total)\\% Processor Time'");
```

#### Example 4: Get Network Adapters
```c
keyboard_type_string("powershell Get-NetAdapter");
```

#### Example 5: Download File
```c
keyboard_type_string("powershell Invoke-WebRequest -Uri http://example.com/file.txt -OutFile file.txt");
```

#### Example 6: Get WiFi Passwords
```c
keyboard_type_string("powershell (netsh wlan show profiles) | Select-String '\\:(.+)$'");
```

---

## Customizing Commands

### Modify Command 13 - Custom Single Command

Open `main.c` and find `cmd_open_and_execute_cmd()`:

```c
static void cmd_open_and_execute_cmd(void) {
    // ... existing code ...
    
    // CHANGE THIS LINE to your desired command
    keyboard_type_string("ipconfig");  // ← Replace with your command
    
    // ... rest of code ...
}
```

**Examples:**
```c
// Network test
keyboard_type_string("ping 8.8.8.8");

// Show WiFi info
keyboard_type_string("netsh wlan show interfaces");

// List installed programs
keyboard_type_string("wmic product get name,version");

// Get Windows version
keyboard_type_string("ver");

// Show environment variables
keyboard_type_string("set");
```

### Modify Command 15 - Custom Command Chain

```c
static void cmd_execute_multiple_commands(void) {
    // ... existing code ...
    
    // CHANGE THIS LINE for your command sequence
    keyboard_type_string("cd Desktop && mkdir TestFolder && cd TestFolder && echo Hello World > test.txt");
    
    // ... rest of code ...
}
```

**Examples:**
```c
// Create development environment
keyboard_type_string("mkdir Dev && cd Dev && mkdir Python && mkdir JavaScript && mkdir Projects");

// Clean temp files
keyboard_type_string("cd %TEMP% && del /q /s *.tmp && echo Cleaned");

// Git workflow
keyboard_type_string("git status && git add . && git commit -m Update");

// Build project
keyboard_type_string("cd MyProject && npm install && npm run build");
```

### Adding More CMD Commands

Add a new function after the existing CMD commands:

```c
/**
 * Command 17: Your custom CMD automation
 */
static void cmd_your_custom_command(void) {
    ESP_LOGI(TAG, "Command 17: Running custom automation...");
    
    // Open CMD
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Your commands here
    keyboard_type_string("your command here");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}
```

Then add it to the automation sequence:

```c
static void automation_task(void *pvParameters) {
    // ... existing commands ...
    
    cmd_run_powershell_from_cmd();
    cmd_your_custom_command();  // ← Add your new command
    
    // ... rest of code ...
}
```

---

## Useful CMD Commands Library

### System Information
```c
keyboard_type_string("systeminfo");              // Full system info
keyboard_type_string("hostname");                // Computer name
keyboard_type_string("ver");                     // Windows version
keyboard_type_string("wmic bios get serialnumber"); // Serial number
```

### Network Commands
```c
keyboard_type_string("ipconfig /all");           // Network details
keyboard_type_string("netstat -an");             // Active connections
keyboard_type_string("arp -a");                  // ARP cache
keyboard_type_string("nslookup google.com");     // DNS lookup
keyboard_type_string("tracert google.com");      // Trace route
```

### File Operations
```c
keyboard_type_string("dir /s");                  // List all files
keyboard_type_string("tree");                    // Directory tree
keyboard_type_string("attrib +h file.txt");      // Hide file
keyboard_type_string("type file.txt");           // Display file content
keyboard_type_string("find \"text\" file.txt");  // Search in file
```

### Disk Commands
```c
keyboard_type_string("diskpart");                // Disk utility
keyboard_type_string("chkdsk C:");               // Check disk
keyboard_type_string("defrag C:");               // Defragment
keyboard_type_string("fsutil volume diskfree C:"); // Free space
```

### Process Management
```c
keyboard_type_string("tasklist");                // List processes
keyboard_type_string("taskkill /F /IM notepad.exe"); // Kill process
keyboard_type_string("start notepad");           // Start program
```

---

## Platform-Specific Versions

### macOS Terminal Commands

For macOS, modify to open Terminal instead:

```c
static void cmd_open_and_execute_terminal_macos(void) {
    ESP_LOGI(TAG, "Opening Terminal and executing command (macOS)...");
    
    // Open Spotlight
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_SPACE);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Type "Terminal"
    keyboard_type_string("Terminal");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Execute command
    keyboard_type_string("ifconfig");
    keyboard_press_key(0, HID_KEY_ENTER);
}
```

### Linux Terminal Commands

For Linux:

```c
static void cmd_open_and_execute_terminal_linux(void) {
    ESP_LOGI(TAG, "Opening Terminal and executing command (Linux)...");
    
    // Open terminal (Ctrl+Alt+T on most distros)
    hid_keyboard_report_t report = {0};
    report.modifier = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTALT;
    report.keycode[0] = 0x17;  // HID_KEY_T
    send_keyboard_report(&report);
    vTaskDelay(pdMS_TO_TICKS(KEY_PRESS_DELAY_MS));
    keyboard_release_all();
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Execute command
    keyboard_type_string("ip addr");
    keyboard_press_key(0, HID_KEY_ENTER);
}
```

---

## Testing Individual Commands

To test a single CMD command without running the full automation:

```c
// In automation_task(), comment out all other commands:
static void automation_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== Testing Single Command ===");
    
    automation_running = true;
    led_send_command(LED_CMD_ON, 0);
    
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Test only the command you want
    cmd_open_and_execute_cmd();
    
    // Comment out everything else
    // cmd_open_run_dialog();
    // cmd_open_text_editor();
    // ...
    
    ESP_LOGI(TAG, "=== Test Complete ===");
    automation_running = false;
    led_send_command(LED_CMD_BLINK, 2);
    automation_task_handle = NULL;
    vTaskDelete(NULL);
}
```

---

## Safety Considerations

### ⚠️ Important Warnings

1. **Admin Commands**: Be careful with admin CMD - can modify system settings
2. **Destructive Commands**: Test commands like `del`, `format`, `rmdir` carefully
3. **Network Commands**: Some may trigger firewall warnings
4. **UAC Prompts**: Admin commands require user confirmation
5. **Timing**: Slow computers may need longer delays

### Safe Testing Practice

```c
// Add a safety delay at the start
vTaskDelay(pdMS_TO_TICKS(5000));  // 5 second delay
ESP_LOGI(TAG, "Starting in 5 seconds - unplug if needed!");
```

### Dangerous Commands to Avoid

```c
// ❌ NEVER USE THESE
keyboard_type_string("format C:");      // Formats drive!
keyboard_type_string("del /f /s /q C:\\*"); // Deletes everything!
keyboard_type_string("rd /s /q C:\\");  // Removes directory!
```

---

## Summary

**New Commands Added:**
- ✅ Command 13: Open CMD and execute single command
- ✅ Command 14: Open CMD as Administrator (commented by default)
- ✅ Command 15: Execute multiple chained commands
- ✅ Command 16: Run PowerShell from CMD

**Total Commands Now:** 16 automation commands!

**Quick Customization:**
1. Find the `cmd_*` function you want to modify
2. Change the `keyboard_type_string("your command");` line
3. Rebuild and flash: `idf.py build flash`

Happy automating! 🚀