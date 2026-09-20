# LED Troubleshooting — PandaKeebs Corne v3 Choc Low

## Hardware Specifications

| Property | Value |
|---|---|
| Board | PandaKeebs Corne v3 Choc Low |
| Controller | Nice!Nano v2 (nRF52840) |
| Per-key LEDs | SK6812MINI-E RGB (3-channel, 24-bit GRB) |
| Underglow LEDs | WS2812B 5050 (3-channel, 24-bit GRB) |
| LEDs per half | 27 (6 underglow + 21 per-key) |
| Chain topology | Single serial chain — underglow first (indices 0-5), per-key after (indices 6-26) |
| Data pin | Pro Micro D1/TX → nRF52840 P0.06 via SPI3 MOSI |
| Protocol | WS2812-SPI at 4 MHz, bit-banged via SPI frames |
| Color order | GRB (Green, Red, Blue) |

### LED Chain Order (per half)

On the PCB silkscreen, the data chain starts at LED3 (closest to the MCU), not LED1.
Left half underglow order: LED3 → LED2 → LED1 → LED4 → LED5 → LED6 (by silkscreen).
Source: [foostan/crkbd#96](https://github.com/foostan/crkbd/issues/96)

## Symptom

- **Only 1 underglow LED lights up on each half** (consistent across both halves)
- Key switches register correctly (matrix scan works fine)
- Voltmeter resistance readings across LED chain suggest continuity (properly connected in series)
- The consistent behavior on both halves strongly suggests firmware/config issue, not hardware

## Firmware Configuration

### Upstream shield (app/boards/shields/corne/boards/nice_nano_nrf52840_zmk.overlay)

```devicetree
&spi3 {
    compatible = "nordic,nrf-spim";
    status = "okay";
    pinctrl-0 = <&spi3_default>;  // P0.06 (Pro Micro D1/TX)
    pinctrl-1 = <&spi3_sleep>;

    led_strip: ws2812@0 {
        compatible = "worldsemi,ws2812-spi";
        reg = <0>;
        spi-max-frequency = <4000000>;
        chain-length = <10>;           // ← DEFAULT: only 10 LEDs
        spi-one-frame = <0x70>;
        spi-zero-frame = <0x40>;
        color-mapping = <LED_COLOR_ID_GREEN LED_COLOR_ID_RED LED_COLOR_ID_BLUE>;
    };
};
```

### User overlay (config/config/corne.overlay)

```devicetree
&led_strip {
    chain-length = <27>;
};
```

### User config (config/config/corne.conf)

- `CONFIG_ZMK_RGB_UNDERGLOW=y`
- `CONFIG_ZMK_EXT_POWER=y`
- `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=y`
- `CONFIG_ZMK_RGB_UNDERGLOW_ON_START=y`
- Brightness: 80%, max 90%

## Analysis

### Confirmed correct
- **Pin mapping**: P0.06 is correct for Nice!Nano Pro Micro D1 → LED data (verified: at least 1 LED works)
- **Protocol**: WS2812-SPI compatible with both WS2812B and SK6812MINI-E
- **Color mapping**: GRB is correct for both LED types
- **chain-length override**: Set to 27 in user overlay

### Driver internals (zephyr/drivers/led_strip/ws2812_spi.c)
- `num_colors` derived from `color-mapping` array length (3 for GRB)
- Buffer size: `num_colors × 8 × chain_length` = 3 × 8 × 27 = 648 bytes
- Supports GRBW (4-channel) if color-mapping has 4 entries
- White channel sends 0x00 (not configurable via LED strip API)

### Root cause: RGBW (4-channel) LEDs driven as RGB (3-channel)

**Observation**: LED #1 (closest to MCU) lights up with **purplish-red** color, expected **cyan** (hue=160). All other LEDs stay dark. Identical on both halves.

**Diagnosis**: The LEDs are SK6812MINI-E **RGBW** (4-channel, 32-bit) despite vendor listing as RGB. Evidence:

1. **Wrong color**: Firmware sends GRB for cyan: `[G≈200][R≈20][B≈200]` (3 bytes). An RGBW LED reads 4 bytes: `R=200(G_val), G=20(R_val), B=200(B_val), W=stolen_from_next_LED`. The result is R=high + B=high + W=some = **purplish-red/magenta** — matches observation.

2. **Only 1 LED works**: Each RGBW LED consumes 32 bits but firmware sends 24 bits per LED. After LED #1 steals 1 extra byte, the entire data stream is permanently misaligned. All subsequent LEDs receive garbage data → stay dark.

3. **Both halves identical**: Firmware issue, not hardware. Both halves run the same code.

**Fix applied** (config/config/corne.overlay):
```devicetree
&led_strip {
    chain-length = <27>;
    color-mapping = <LED_COLOR_ID_GREEN LED_COLOR_ID_RED LED_COLOR_ID_BLUE LED_COLOR_ID_WHITE>;
};
```

This makes the driver send 32 bits (4 bytes) per LED. The white channel is always 0x00 (Zephyr driver limitation — `struct led_rgb` has no white field).

### If GRBW fix doesn't work — additional hypotheses

1. **SPI timing edge case** — At 4 MHz SPI, inter-bit low periods (~1250ns) exceed WS2812B spec. Try `spi-max-frequency = <5250000>` for tighter timing.

2. **Reset/latch delay too short** — Default 8µs, SK6812 needs >80µs. Add `reset-delay = <100>;` to the overlay.

3. **Power rail** — Nice!Nano 3.3V may be too low for 5V-spec LEDs. Check voltage on the LED VCC rail with a multimeter.

## Build Commands

```bash
# Build left half (from repo root)
west build -s app -b nice_nano//zmk -- -DSHIELD=corne_left \
    -DZMK_CONFIG="$(pwd)/config/config"

# Build right half (clean build to ensure overlay applies)
west build -s app -b nice_nano//zmk -p -- -DSHIELD=corne_right \
    -DZMK_CONFIG="$(pwd)/config/config"

# Build settings reset
west build -s app -b nice_nano//zmk -p -- -DSHIELD=settings_reset

# Output: build/zephyr/zmk.uf2
```

## Flash procedure

1. Connect Nice!Nano via USB
2. Double-press reset button → enters UF2 bootloader (mounts as drive)
3. Copy `build/zephyr/zmk.uf2` to the mounted drive
4. Board reboots automatically with new firmware
5. Repeat for other half
