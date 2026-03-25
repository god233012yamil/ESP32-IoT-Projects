# ThingsBoard Dashboard Setup Guide

This guide will walk you through creating a complete dashboard in ThingsBoard to visualize your ESP32 sensor data.

## Prerequisites

- ThingsBoard account (https://thingsboard.cloud/ or self-hosted)
- Device created and configured with access token
- ESP32 device sending data

## Step 1: Create a New Dashboard

1. Login to your ThingsBoard account
2. Navigate to **Dashboards** in the left sidebar
3. Click the **"+"** button in the bottom right corner
4. Select **"Create new dashboard"**
5. Enter dashboard details:
   - Title: `ESP32 DHT Sensor Dashboard`
   - Description: `Temperature and Humidity Monitoring`
6. Click **"Add"**
7. Your new dashboard will open in edit mode

## Step 2: Add Current Temperature Widget

1. Click **"Add new widget"** or the **"+"** button
2. Select **"Cards"** widget bundle
3. Choose **"Simple card"** or **"Entities table"** > **"Latest values"**

### Configure Temperature Card:

**Data source:**
- Target device: Select your device (`ESP32-DHT-Sensor`)
- Type: Latest telemetry
- Key: `temperature`

**Appearance:**
- Title: `Current Temperature`
- Show label: Yes
- Label: `°C` (or `°F` if you modify the code)
- Units: `°C`
- Decimals: 1
- Background color: Choose a color (e.g., #FF6B6B for warm)
- Icon: `thermostat` or `device_thermostat`

**Advanced:**
- Enable card border
- Add shadow if desired

4. Click **"Add"** to place the widget on the dashboard

## Step 3: Add Current Humidity Widget

1. Click **"Add new widget"**
2. Select **"Cards"** widget bundle
3. Choose **"Simple card"**

### Configure Humidity Card:

**Data source:**
- Target device: Your device
- Type: Latest telemetry
- Key: `humidity`

**Appearance:**
- Title: `Current Humidity`
- Show label: Yes
- Label: `%`
- Units: `%`
- Decimals: 1
- Background color: Choose a color (e.g., #4ECDC4 for water)
- Icon: `water_drop` or `opacity`

4. Click **"Add"**

## Step 4: Add Temperature Time Series Chart

1. Click **"Add new widget"**
2. Select **"Charts"** widget bundle
3. Choose **"Time series line chart"** or **"Timeseries - Flot"**

### Configure Temperature Chart:

**Data source:**
- Target device: Your device
- Type: Timeseries
- Key: `temperature`

**Appearance:**
- Title: `Temperature History`
- Y-axis label: `Temperature (°C)`
- Show legend: Yes
- Line color: #FF6B6B (red/warm color)
- Line thickness: 2-3

**Time window:**
- Default: Last hour
- Add quick time selection: 1h, 6h, 12h, 24h, 7d

**Advanced settings:**
- Show dots on datapoints: Optional
- Fill area under line: Optional (with transparency)
- Show tooltip on hover: Yes
- Y-axis min/max: Auto or set range (e.g., 0-50°C)

4. Click **"Add"**

## Step 5: Add Humidity Time Series Chart

1. Click **"Add new widget"**
2. Select **"Charts"** widget bundle
3. Choose **"Time series line chart"**

### Configure Humidity Chart:

**Data source:**
- Target device: Your device
- Type: Timeseries
- Key: `humidity`

**Appearance:**
- Title: `Humidity History`
- Y-axis label: `Humidity (%)`
- Show legend: Yes
- Line color: #4ECDC4 (blue/water color)
- Line thickness: 2-3

**Time window:**
- Default: Last hour
- Quick selections: 1h, 6h, 12h, 24h, 7d

**Advanced settings:**
- Y-axis min/max: 0-100%
- Show grid lines: Yes

4. Click **"Add"**

## Step 6: Add Combined Chart (Optional)

For a more advanced visualization, create a combined chart:

1. Click **"Add new widget"**
2. Select **"Charts"** bundle
3. Choose **"Time series line chart"**

### Configure Combined Chart:

**Data sources:**
- Add first datasource:
  - Device: Your device
  - Key: `temperature`
  - Label: `Temperature`
  
- Click **"Add datasource"**
  - Device: Your device  
  - Key: `humidity`
  - Label: `Humidity`

**Appearance:**
- Title: `Temperature & Humidity`
- Two Y-axes (left for temperature, right for humidity)
- Different colors for each line
- Show legend: Yes

## Step 7: Add Gauges (Optional)

For a more visual representation:

### Temperature Gauge:

1. Add new widget > **"Analogue gauges"** > **"Radial gauge"**
2. Configure:
   - Device: Your device
   - Key: `temperature`
   - Title: `Temperature`
   - Min value: 0, Max value: 50
   - Units: °C
   - Color zones:
     - 0-10: Blue (cold)
     - 10-25: Green (comfortable)
     - 25-50: Red (hot)

### Humidity Gauge:

1. Add new widget > **"Analogue gauges"** > **"Radial gauge"**
2. Configure:
   - Device: Your device
   - Key: `humidity`
   - Title: `Humidity`
   - Min value: 0, Max value: 100
   - Units: %
   - Color zones:
     - 0-30: Red (dry)
     - 30-60: Green (comfortable)
     - 60-100: Blue (humid)

## Step 8: Add Device State Widget (Optional)

Show when the device last sent data:

1. Add new widget > **"Cards"** > **"Entity card"**
2. Configure:
   - Device: Your device
   - Show last update time
   - Show device status (active/inactive)

## Step 9: Arrange Dashboard Layout

1. Click **"Edit mode"** button (pencil icon)
2. Drag widgets to arrange them:
   - Top row: Current value cards (Temperature, Humidity)
   - Middle: Time series charts side by side
   - Bottom: Gauges or combined chart
3. Resize widgets by dragging corners
4. Adjust spacing between widgets

### Recommended Layout:

```
┌────────────────────────────────────────┐
│  Current Temp    │   Current Humidity  │
│     23.5°C       │       65.2%         │
├────────────────────────────────────────┤
│                                        │
│     Temperature History Chart          │
│                                        │
├────────────────────────────────────────┤
│                                        │
│       Humidity History Chart           │
│                                        │
└────────────────────────────────────────┘
```

## Step 10: Configure Dashboard Settings

1. Click **"Settings"** (gear icon)
2. Configure:
   - Title: Adjust if needed
   - Auto-update: Enable (10-30 second refresh)
   - Time window default: Set preferred default
   - Mobile layout: Adjust for responsive design

3. Click **"Save"**

## Step 11: Save and Exit Edit Mode

1. Click the checkmark or **"Apply changes"**
2. Exit edit mode
3. Your dashboard is now live!

## Advanced Features

### Add Alarms

Create alarms for temperature/humidity thresholds:

1. Go to Device > **"Alarm rules"**
2. Click **"+"** to add alarm rule
3. Configure:
   - Type: Temperature High
   - Condition: `temperature > 30`
   - Severity: Warning
4. Add action: Send notification
5. Save

### Add Email Notifications

1. Go to **"Rule chains"**
2. Edit **"Root Rule Chain"**
3. Add email node for alarm notifications

### Mobile Access

- ThingsBoard has mobile apps (iOS/Android)
- Your dashboard will be accessible on mobile
- Optimize widget sizes for mobile viewing

## Verifying Data Flow

After setting up:

1. Check if widgets show "No data" or actual values
2. Verify timestamps are current
3. Check time series charts show data points
4. Monitor for consistent updates every 10 seconds

**If no data appears:**
- Verify ESP32 is connected and sending data
- Check device's "Latest telemetry" tab in ThingsBoard
- Review ESP32 serial monitor output
- Verify MQTT connection is established

## Sample Dashboard Screenshots

Your final dashboard should display:
- Real-time temperature and humidity values
- Historical trends over time
- Visual indicators (gauges/colors) for quick status checks
- Last update timestamp
- Clean, organized layout

## Export/Import Dashboard

To backup or share your dashboard:

1. Go to Dashboards
2. Click on your dashboard
3. Click **"Export"** button
4. Save JSON file
5. Import on another ThingsBoard instance using **"Import"**

## Tips for Best Results

- Use consistent color schemes (warm colors for temperature, cool for humidity)
- Don't overcrowd - start with essential widgets
- Test on different screen sizes
- Use clear, descriptive titles
- Set appropriate time windows for your use case
- Enable auto-refresh for live monitoring
- Consider adding min/max value widgets for daily extremes

## Next Steps

- Add more devices to the same dashboard
- Create rule chains for automation
- Set up email/SMS alerts
- Integrate with other systems via API
- Create customer-specific dashboards for public access
