# LoRa Uplink Bridge Design

## Purpose

This document defines the minimum confirmed runtime path for `PC LoRa -> Raspberry Pi LoRa -> uplink_app`.

The goal of this bridge is to terminate the Raspberry Pi LoRa serial input and convert accepted uplink frames into `UPLINK_APP PROCESS_UPLINK` CCSDS command packets delivered over the existing local UDP ingress path.

## Placement

The LoRa uplink bridge belongs in [bridge](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/bridge) rather than inside `uplink_app`.

Reason:

- the repository already uses `bridge/` for Raspberry Pi host-side protocol adaptation
- `uplink_app` currently owns cFS-level command validation and route routing
- serial device ownership, ASCII framing, CRC, and reconnect behavior are host integration concerns
- keeping LoRa transport termination outside cFS preserves the existing `uplink_app` command contract and minimizes app churn

## Scope

The bridge shall:

- open and maintain the Raspberry Pi LoRa serial endpoint
- read line-delimited uplink frames
- validate framing, field ranges, payload length, and frame CRC
- reject duplicate or regressed sequence numbers by default
- convert accepted frames into `UPLINK_APP_ProcessUplinkCmd_t`
- wrap that payload in `UPLINK_APP_CMD_MID` with function code `UPLINK_APP_PROCESS_UPLINK_CC`
- forward the resulting packet to local UDP port `1234` by default

The bridge shall not:

- re-validate route geometry rules already enforced by `uplink_app`
- classify mission-level uplink command acceptance beyond structural transport checks
- bypass `uplink_app` and publish route updates directly

## Input Frame Contract

The minimum canonical LoRa uplink frame is ASCII and newline-delimited.

Frame format:

`UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>`

Field rules:

- `UP` is the fixed frame type token
- `version` is an unsigned 8-bit integer and must currently be `1`
- `command_class` is an unsigned 8-bit integer
- `sequence` is an unsigned 16-bit integer
- `flags` is an unsigned 8-bit integer
- `payload_hex` is uppercase or lowercase hexadecimal with even length
- decoded payload length must be between `0` and `196` bytes inclusive
- `crc16_hex` is the CRC-16/CCITT-FALSE of the ASCII bytes of `UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>`

## Output Contract

For each accepted frame, the bridge shall emit one UDP packet containing:

- CCSDS primary header with message ID `0x18D0`
- command secondary header with function code `2`
- `UPLINK_APP_ProcessUplinkCmd_t` payload encoded in little-endian host layout

`payload_hex` maps to the `Payload` field of `UPLINK_APP_ProcessUplinkCmd_t`.

The bridge sets:

- `Version = version`
- `CommandClass = command_class`
- `PayloadLength = len(payload)`
- `Flags = flags`
- `Sequence = sequence`

## Error Handling

The bridge shall discard the frame and continue running when any of the following occurs:

- malformed field count
- non-numeric field
- out-of-range field
- odd-length or non-hex payload
- payload length above `196`
- CRC mismatch
- sequence regression when strict sequence mode is enabled

The bridge shall attempt serial reopen after serial open or read failure.

## Known Limitation: RF Collision with Downlink

`lora_fc_downlink_app` transmits FC and SH packets continuously on the same LoRa channel used for uplink. When the PC transmits an UP frame simultaneously, both signals collide in the air and the Pi receives a corrupted frame.

```
EVS: UPLINK_APP: LoRa frame parse failed: UP1,1,10,...  ← corrupted frame
```

This is not a software bug; it is an inherent single-channel half-duplex constraint.

Mitigation: use a second LoRa module connected to a separate COM port on the PC as a dedicated uplink channel. The Pi-side bridge would open both the downlink serial device (for FC/SH RX) and the uplink serial device (for UP RX) independently.

## Bring-up Notes

The existing [tools/uplink_route_update_sender.py](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/tools/uplink_route_update_sender.py) supports two paths:

- `--transport udp` for direct local cFS injection
- `--transport lora-text` for generating the canonical LoRa uplink frame for PC-side serial transmission
