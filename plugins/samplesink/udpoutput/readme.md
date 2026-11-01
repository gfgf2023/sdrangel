# UDP Sink TX Output Plugin (GNU Radio Interface)

## Introduction

The **UDP Sink Output** (`UDPOutput` / `UDPSink`) plugin is a Sample Sink (TX Device) for SDRangel. It allows streaming the transmitted baseband signal (mixed from all active transmitter channels in the Tx Device Set) over UDP to external software, specifically designed to interface directly with **GNU Radio**'s `UDP Source` block.

This enables users to use SDRangel as a powerful signal generator / modulator (e.g., NFM, WFM, SSB, AM, AIS, ATV, DVB-S2, etc.) while routing the resulting modulated baseband IQ stream into GNU Radio for custom processing, analysis, recording, or transmission via GNU Radio-supported SDR hardware.

---

## Features

- **Sample Formats**:
  - `Complex Float32`: 32-bit floating point interleaved I/Q samples (8 bytes per sample). This directly matches GNU Radio's native `Complex` (`gr_complex`) data type without requiring any type conversion block.
  - `S16LE Short`: 16-bit signed integer interleaved I/Q samples (4 bytes per sample, Little Endian). Standard format compatible with GNU Radio `Short` input (use with GNU Radio's `IShort to Complex` block).
  - `S8 Byte`: 8-bit signed integer interleaved I/Q samples (2 bytes per sample) for lower network bandwidth.
  - `S24LE`: 24-bit signed integer interleaved I/Q samples (6 bytes per sample).
- **Configurable Destination**:
  - Destination IP Address (e.g., `127.0.0.1` for local GNU Radio or a LAN IP for remote host).
  - Destination UDP Port (default `9999`).
- **Configurable MTU / Payload Size**:
  - Supported packet sizes: `512`, `1024`, `1472` (standard Ethernet MTU: 1500 - 20 IP - 8 UDP = 1472 bytes), `2048`, `4096` bytes.
  - Ensures integer multiples of sample sizes per datagram, preventing frame misalignment.
- **Spectrum & Waterfall Display**:
  - Built-in real-time OpenGL spectrum visualizer and waterfall display.
- **Digital Gain / Level Control**:
  - Adjustable gain slider (0% to 200%) to scale the output amplitude before network transmission.

---

## Interfacing with GNU Radio

### Option 1: Native Complex Float32 (Recommended)

In SDRangel UDP Sink TX Device:
- **Format**: Select `Complex Float32`
- **MTU/Size**: `1472 (MTU)` (or `512`)
- **UDP Host**: `127.0.0.1`
- **Port**: `9999`

In GNU Radio Companion (GRC):
1. Add a **UDP Source** block (`gr-blocks` -> `Networking` -> `UDP Source`).
2. Configure **UDP Source**:
   - **Type**: `Complex`
   - **IP Address**: `127.0.0.1` (or `0.0.0.0`)
   - **Port**: `9999`
   - **Payload Size**: `1472` (must match the payload size selected in SDRangel)
   - **Wait for connection**: `False`
3. Connect the output of the **UDP Source** directly to a **QT GUI Frequency Sink**, **QT GUI Waterfall Sink**, **UHD: USRP Sink**, or any processing block.

```
[SDRangel TX Device: UDP Sink]
             │
         UDP │ 127.0.0.1:9999 (1472 bytes/pkt, Complex Float32)
             ▼
[GNU Radio: UDP Source (Type: Complex)] ──► [QT GUI Frequency Sink / Hardware Sink]
```

### Option 2: 16-bit Signed Integer (S16LE Short)

In SDRangel UDP Sink TX Device:
- **Format**: Select `S16LE Short (16-bit)`
- **MTU/Size**: `1472 (MTU)`
- **Port**: `9999`

In GNU Radio Companion (GRC):
1. Add a **UDP Source** block:
   - **Type**: `Short`
   - **Payload Size**: `1472`
2. Add an **IShort To Complex** block (`gr-blocks` -> `Type Converters` -> `IShort To Complex`):
   - **Scale**: `32768.0`
3. Connect `UDP Source` -> `IShort To Complex` -> Sinks / Flowgraph.

---

## Controls & Interface

1. **Start/Stop Button**: Starts or stops sample generation and UDP transmission.
2. **Rate Display**: Current I/Q sample rate in kS/s.
3. **Center Frequency**: Baseband center frequency dial (in kHz).
4. **Interpolation (Int)**: Baseband interpolation factor (1, 2, 4, 8, 16, 32, 64).
5. **Sample Rate (SR)**: Sample rate dial in S/s.
6. **UDP Host**: Destination IPv4 address.
7. **Port**: Destination UDP port number.
8. **Format**: Output sample encoding (`Complex Float32`, `S16LE Short`, `S8 Byte`, `S24LE`).
9. **MTU/Size**: UDP packet payload size in bytes.
10. **Gain**: Digital output gain slider and percentage indicator.
11. **Spectrum View**: Real-time spectrum analysis of the transmitted baseband signal.

---

## High Sample Rate Streaming (e.g. 2 MS/s and higher)

When streaming UDP at 2 MS/s or above over a network (e.g. 4 MB/s for 8-bit, 16 MB/s for Float32):
1. **Socket Buffer (`SO_RCVBUF`) in GNU Radio / OS**:
   - The sender (SDRangel) uses an 8 MB kernel socket send buffer.
   - On Linux systems running GNU Radio, increase the kernel UDP receive buffer to prevent packet drops:
     ```bash
     sudo sysctl -w net.core.rmem_max=16777216
     sudo sysctl -w net.core.rmem_default=4194304
     ```
2. **Bandwidth Considerations**:
   - `Complex Float32` @ 2 MS/s requires ~128 Mbps (best over wired Gigabit Ethernet or localhost `127.0.0.1`).
   - `S8 Byte` @ 2 MS/s requires only ~32 Mbps, ideal for Wi-Fi or lower bandwidth connections.
3. **Payload Alignment**:
   - Datagram payloads are automatically aligned to complex sample boundaries (e.g., multiples of 8 bytes for Float, 4 for S16, 2 for S8, 6 for S24), ensuring no split IQ samples.
