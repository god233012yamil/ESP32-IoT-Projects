# Wiring Guide for Power Gating Techniques

This document provides practical wiring guidance for the three power gating techniques demonstrated in this project. While exact component choices depend on your specific design requirements, the fundamental constraints and common pitfalls remain the same across all implementations.

## Table of Contents

- [General Requirements](#general-requirements)
- [Technique A: Regulator EN Pin Gating](#technique-a-regulator-en-pin-gating)
- [Technique B: Load Switch Gating](#technique-b-load-switch-gating)
- [Technique C: P-FET High-Side Gating with Driver](#technique-c-p-fet-high-side-gating-with-driver)
- [Avoiding Back-Powering (Critical)](#avoiding-back-powering-critical)
- [Component Selection Guidelines](#component-selection-guidelines)

## General Requirements

All three techniques share these fundamental requirements:

### 1. Default OFF State (Mandatory)

The enable signal **must** default to OFF in hardware using an external pull resistor:

- **Why**: GPIO states during reset, boot, and deep sleep are not guaranteed across all ESP32-C6 boards and configurations
- **How**: Add 10kΩ-100kΩ pull resistor on enable pin
  - Pull-down if enable is active-high
  - Pull-up if enable is active-low
- **Never** rely solely on firmware initialization or GPIO hold states

### 2. I2C Pull-Up Placement

If gating sensor power:

- Place I2C pull-up resistors (typically 4.7kΩ-10kΩ) on the **gated rail**, not the MCU rail
- This ensures no current flows through pull-ups when the rail is off
- Standard value: 4.7kΩ for most I2C sensors

### 3. Stabilization Time

After enabling the gated rail, firmware must wait for:

- Power supply voltage rise time and settling
- Peripheral internal startup/boot time
- Typical values: 5-20ms (configure `PG_STABILIZE_MS` in menuconfig)

---

## Technique A: Regulator EN Pin Gating

### Overview

Use a voltage regulator (buck converter or LDO) with an enable pin to control peripheral power.

### Schematic

```
Battery/VIN ─┬─> Regulator VIN
             │
ESP32-C6 GPIO ─────[Optional 1kΩ]─────┬─> Regulator EN
                                      │
                                   [10kΩ]  (Pull-down for active-high)
                                      │
                                     GND

Regulator VOUT ─────> VDD_SENS (gated rail to peripherals)
```

### Component Selection

**Regulator requirements**:
- Low quiescent current (important for battery life)
- Enable pin with defined logic thresholds
- Sufficient output current for your peripherals
- Optional: soft-start to limit inrush current

**Example regulators**:
- TPS62840 (Buck, 750mA, 60nA Iq)
- TLV62569 (Buck, 2A, 360nA Iq)
- TPS7A02 (LDO, 200mA, 25nA Iq)
- MCP1700 (LDO, 250mA, 1.6µA Iq)

### Configuration

In menuconfig:
```
Power gating technique: Regulator EN pin
Enable GPIO: [Your GPIO number, e.g., 10]
Enable is active high: Yes (for most regulators)
```

### Pros

- Simple implementation
- Low BOM cost (single IC)
- Clean voltage regulation for peripherals
- Many regulators include protection features

### Cons

- Depends on regulator EN pin behavior and leakage
- Some regulators allow reverse current if output is driven while disabled
- Check datasheet for off-state characteristics

### Design Notes

1. **Series resistor**: Optional 1kΩ resistor between GPIO and EN provides current limiting and reduces EMI
2. **Output capacitor**: Follow regulator datasheet recommendations (typically 1-10µF ceramic)
3. **Enable threshold**: Verify GPIO high voltage meets regulator EN threshold (typically 1.4V-2V)
4. **Reverse current**: Some designs add a Schottky diode on output to block reverse current

---

## Technique B: Load Switch Gating

### Overview

Use a dedicated integrated load switch to gate power between an always-on rail and the peripheral rail.

### Schematic

```
Always-on 3V3 ─────> Load Switch VIN

ESP32-C6 GPIO ─────[Optional 1kΩ]─────┬─> Load Switch EN
                                      │
                                   [10kΩ]  (Pull-down for active-high)
                                      │
                                     GND

Load Switch VOUT ─────> VDD_SENS (gated rail to peripherals)
```

### Component Selection

**Load switch requirements**:
- Low on-resistance (Rds_on) for efficiency
- Low quiescent current
- Controlled rise time (prevents inrush)
- Reverse current blocking
- Enable pin compatible with 3.3V GPIO

**Example load switches**:
- TPS22918 (3A, 26mΩ, 28nA shutdown)
- TPS22994 (6A, 13mΩ, adjustable rise time)
- AP7381 (2A, 65mΩ, 1µA shutdown)
- FDC6321L (Dual MOSFET alternative, very low cost)

### Configuration

In menuconfig:
```
Power gating technique: Load switch
Enable GPIO: [Your GPIO number, e.g., 10]
Enable is active high: Yes (for most load switches)
```

### Pros

- **Predictable off-state leakage** (typically <1µA)
- Built-in reverse current blocking
- Controlled inrush current (soft-start)
- Some include fault protection (overcurrent, thermal)
- Cleaner power sequencing vs regulators

### Cons

- Additional BOM component (separate from regulator)
- Higher cost than passive FET solution
- Voltage drop across Rds_on (usually negligible)

### Design Notes

1. **Input capacitor**: Place 1-10µF ceramic near load switch VIN
2. **Output capacitor**: Size based on peripheral requirements and switch rise time specs
3. **Rise time**: Some switches allow external capacitor to adjust slew rate
4. **Current rating**: Choose switch rated for 2-3× your peak peripheral current

---

## Technique C: P-FET High-Side Gating with Driver

### Overview

Use a P-channel MOSFET as a high-side switch, driven by a simple gate driver stage (typically NPN or N-channel MOSFET) to provide proper level shifting.

### Schematic

```
Always-on 3V3 ─────┬─> P-FET Source
                   │
                [10kΩ] (Pull-up, ensures OFF at reset)
                   │
                   ├─────[1kΩ]───── Gate Driver Output (e.g., NPN collector)
                   │
P-FET Drain ───────┴────> VDD_SENS (gated rail to peripherals)


ESP32-C6 GPIO ────[1kΩ]────┬─> Gate Driver Input (e.g., NPN base)
                            │
                         [10kΩ] (Optional base pull-down)
                            │
                           GND
```

### Component Selection

**P-FET requirements**:
- Vgs(th) compatible with your voltage rails (typically -1V to -2.5V)
- Low Rds_on for efficiency (e.g., <100mΩ)
- Vds rating above your rail voltage (e.g., 20V for 3.3V rails)
- Sufficient current rating (2-3× peak load)

**Example P-FETs**:
- DMG2305UX (20V, -4.2A, 38mΩ, SOT-23)
- Si2301DS (20V, -2.3A, 85mΩ, SOT-23)
- AO3401A (30V, -4A, 44mΩ, SOT-23)

**Gate driver options**:
- **NPN transistor**: 2N3904, MMBT3904 (simplest)
- **N-FET**: 2N7002, BSS138 (lower current draw)
- **Dedicated driver IC**: MIC5019 (for high-side N-FET, more complex)

### Configuration

In menuconfig:
```
Power gating technique: PFET driver
Enable GPIO: [Your GPIO number, e.g., 10]
Enable is active high: Yes (GPIO high turns on gate driver, pulls P-FET gate low)
```

### Example Circuit: NPN Driver

```
3V3 ──┬──> P-FET Source
      │
   [10kΩ] Pull-up (ensures P-FET OFF at reset)
      │
      ├───[1kΩ]─── NPN Collector ───┐
      │                              │
P-FET Gate ──────────────────────────┘
      │
P-FET Drain ───> VDD_SENS

GPIO ───[1kΩ]─── NPN Base
                    │
                 [10kΩ] (optional)
                    │
                   GND
```

**Operation**:
1. GPIO LOW → NPN off → Gate pulled high via 10kΩ → P-FET off
2. GPIO HIGH → NPN on → Gate pulled low via NPN → P-FET on

### Pros

- **Lowest cost** (discrete components)
- Maximum flexibility for custom designs
- Can handle very high currents (select appropriate FET)
- No specialized ICs required

### Cons

- **Easy to implement incorrectly** (gate drive, body diode, inrush)
- Requires understanding of MOSFET operation
- Must consider:
  - Body diode direction (always check datasheet)
  - Vgs limits (don't exceed maximum gate-source voltage)
  - Inrush current (add series resistor or output cap if needed)
- More components than integrated solutions

### Design Notes

1. **Gate pull-up**: 10kΩ-100kΩ from gate to source ensures FET is OFF by default
2. **Gate resistor**: 1kΩ between driver and gate limits switching speed, reduces EMI
3. **Driver base resistor**: 1kΩ limits GPIO current, provides safe NPN drive
4. **Body diode**: P-FET body diode points from drain to source (check datasheet!)
5. **Inrush limiting**: 
   - Add 1-10Ω series resistor between drain and load
   - Or add large output capacitor (10-100µF) for soft-start
6. **Vgs voltage**: Verify source-gate voltage under all conditions:
   - Gate high (FET off): Vgs = 0V ✓
   - Gate low (FET on): Vgs = -(Vsource - Vgate) = ~-3.3V ✓
   - Check this doesn't exceed datasheet maximum (typically ±20V)

### Alternative: N-FET Low-Side (Not Recommended for Sensors)

While an N-FET on the ground side is simpler, it creates a floating ground reference which can cause issues with I2C/SPI communication. Only use for isolated loads like LEDs or relays.

---

## Avoiding Back-Powering (Critical)

### The Problem

When you gate off a peripheral but leave its communication bus connected (I2C, SPI, UART), current can flow **backward** from the ESP32-C6 GPIOs through the peripheral's ESD protection diodes, partially powering it. This:

- Defeats the purpose of power gating
- Causes erratic peripheral behavior
- Increases overall system current
- Can damage components in extreme cases

### The Solution (Three-Layer Approach)

#### Layer 1: Firmware (Implemented in this demo)

Before cutting peripheral power, set bus GPIOs to a safe state:

```c
// Before disabling gated rail:
bus_safe_apply_before_power_off();  // Sets I2C pins to INPUT, no pulls
pg_set_enabled(false);               // Then gate rail OFF
```

This is implemented in `bus_safe.c` and **is critical**.

#### Layer 2: Hardware Pull-Up Placement (Design)

Place I2C pull-up resistors on the **gated rail**, not the MCU rail:

```
Correct:
ESP32-C6 SDA ───────────────┬───────── Sensor SDA
                            │
                        [4.7kΩ]
                            │
                      VDD_SENS (gated rail)

Incorrect:
ESP32-C6 SDA ───────────────┬───────── Sensor SDA
                            │
                        [4.7kΩ]
                            │
                          3V3 (always-on) ← Current flows when rail is OFF!
```

#### Layer 3: Additional Isolation (If Needed)

If you still observe back-powering despite firmware and pull-up placement:

**Option A: Series resistors**
```
ESP32-C6 SDA ───[100Ω-1kΩ]──── Sensor SDA
```
- Simple, low cost
- Limits current through ESD diodes
- May affect signal integrity at high I2C speeds (>400kHz)

**Option B: Bus switches/buffers**
```
ESP32-C6 SDA ──> Bus Switch ──> Sensor SDA
                      ↑
                   Enable (from gated rail or separate GPIO)
```
- Complete isolation when disabled
- Higher cost, more BOM
- Examples: TCA9406, PCA9306 (I2C level shifters with enable)

**Option C: Analog switch**
```
ESP32-C6 I2C ──> Analog Switch ──> Sensor I2C
                       ↑
                    Enable GPIO
```
- Examples: TS5A3166 (SPST), FSA2147 (dual SPDT)

### Verification Procedure

To verify no back-powering is occurring:

1. Connect multimeter to peripheral VDD pin
2. Run firmware with rail gated OFF
3. Measure voltage at peripheral VDD:
   - **Good**: <50mV (essentially 0V)
   - **Acceptable**: 50-200mV (minor leakage)
   - **Bad**: >200mV (significant back-powering, needs fixing)
4. If voltage is high:
   - Verify bus-safe code is called before rail disable
   - Check pull-up resistor placement
   - Add series resistors or bus switch

---

## Component Selection Guidelines

### Choosing Between Techniques

| Criteria | Regulator EN | Load Switch | P-FET Driver |
|----------|--------------|-------------|--------------|
| **Cost** | Low ($) | Medium ($$) | Lowest ($) |
| **Complexity** | Low | Low | Medium |
| **Off-state leakage** | Variable (check datasheet) | Excellent (<1µA) | Good (<1µA) |
| **Inrush control** | Depends on regulator | Built-in | Manual (add cap/resistor) |
| **Reverse current** | Potential issue | Blocked | Check body diode |
| **Current capability** | IC dependent | IC dependent | Only limited by FET |
| **Best for** | All-in-one solution | Predictable behavior | High current, custom design |

### General Component Specs

**For battery-powered designs, prioritize**:
1. Low quiescent/shutdown current (<1µA)
2. Appropriate current rating (2-3× peak load)
3. Fast turn-on (if needed for time-critical sensors)
4. Small package size (SOT-23 or smaller)

**For development/prototyping**:
1. Through-hole packages for easy soldering
2. Availability and low cost
3. Good documentation and reference designs

---

## Testing and Validation

### Power-On Testing

1. **Verify default state**: Power on board with firmware **not** loaded
   - Gated rail should be OFF (measure with multimeter)
   - If rail is ON, check pull resistor value and connection

2. **Verify firmware control**: Load firmware and monitor GPIO
   - Use oscilloscope on enable pin
   - Verify proper logic levels and timing

3. **Measure rail voltage**: Use oscilloscope to observe:
   - Rise time when enabling
   - Overshoot/ringing (add capacitor if excessive)
   - Stable voltage during operation
   - Clean shutdown when disabling

### Current Measurement

1. **Measure each state**:
   - Active (rail ON, sensor communicating)
   - Rail OFF, ESP32 active
   - Deep sleep (both OFF)

2. **Check for back-powering**:
   - Measure peripheral VDD when rail is "OFF"
   - Should be <50mV

3. **Verify deep sleep current**:
   - Disconnect USB (often adds 1-5mA)
   - Measure battery current
   - Should match datasheet specs (5-20µA for ESP32-C6)

### Reliability Testing

1. **Power cycle test**: Gate rail on/off 100+ times
   - Verify consistent behavior
   - Check for stuck states or glitches

2. **Temperature test**: If possible, test across operating temperature range
   - Cold soak: -20°C (if applicable)
   - Hot soak: +85°C (if applicable)

3. **Long-term stability**: Leave in sleep mode for 24+ hours
   - Measure battery voltage before/after
   - Calculate actual current draw

---

## Common Issues and Solutions

### Issue: Gated rail has voltage when it should be OFF

**Diagnoses**:
1. Back-powering through I2C/SPI pins
2. Pull-ups on wrong rail
3. GPIO not driven correctly

**Solutions**:
- Verify bus-safe code executes before rail disable
- Check pull-up resistor placement (should be on gated rail)
- Measure GPIO voltage with oscilloscope
- Add series resistors on bus pins

### Issue: Peripheral doesn't boot/communicate after rail enable

**Diagnoses**:
1. Insufficient stabilization delay
2. Inrush current causing voltage dip
3. Peripheral not fully powered

**Solutions**:
- Increase `PG_STABILIZE_MS` (try 20-50ms)
- Add larger output capacitor (10-100µF)
- Check peripheral VDD voltage with oscilloscope during startup
- Verify peripheral startup current vs. regulator/switch current limit

### Issue: Inconsistent operation across power cycles

**Diagnoses**:
1. No hardware default on enable pin
2. GPIO hold state issues
3. Timing-dependent initialization

**Solutions**:
- Add external pull resistor on enable pin (mandatory!)
- Never rely on GPIO state during reset
- Add delays between enable and communication
- Check for race conditions in task startup

---

## Reference Designs

For real-world examples of these techniques in production:

1. **Environmental sensors**: BME680 on gated rail with I2C, regulator EN technique
2. **GPS modules**: U-blox M8 with load switch for clean startup
3. **E-ink displays**: SPI display with P-FET gating for high inrush current

See project documentation and example schematics (if provided in separate repo).

---

**Questions?** Open an issue on GitHub or consult the ESP32-C6 hardware design guidelines.
