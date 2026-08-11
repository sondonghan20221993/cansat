#!/usr/bin/env python3
import argparse
import sys
import termios
import time

import serial
from pymavlink import mavutil


DEFAULT_SERIAL_PATH = "/dev/serial0"
DEFAULT_BAUDRATE = 230400
DEFAULT_SOURCE_SYSTEM = 250

INTERESTING_TYPES = {
    "HEARTBEAT",
    "ATTITUDE",
    "LOCAL_POSITION_NED",
    "GPS_RAW_INT",
    "EKF_STATUS_REPORT",
}


def describe_message(msg: mavutil.mavlink.MAVLink_message) -> str:
    msg_type = msg.get_type()

    if msg_type == "HEARTBEAT":
        return (
            f"HEARTBEAT sys={msg.get_srcSystem()} comp={msg.get_srcComponent()} "
            f"custom_mode={msg.custom_mode} base_mode={msg.base_mode} "
            f"system_status={msg.system_status}"
        )

    if msg_type == "ATTITUDE":
        return (
            f"ATTITUDE boot_ms={msg.time_boot_ms} roll={msg.roll:.3f} "
            f"pitch={msg.pitch:.3f} yaw={msg.yaw:.3f}"
        )

    if msg_type == "LOCAL_POSITION_NED":
        return (
            f"LOCAL_POSITION_NED boot_ms={msg.time_boot_ms} x={msg.x:.3f} "
            f"y={msg.y:.3f} z={msg.z:.3f} vx={msg.vx:.3f} "
            f"vy={msg.vy:.3f} vz={msg.vz:.3f}"
        )

    if msg_type == "GPS_RAW_INT":
        return (
            f"GPS_RAW_INT time_usec={msg.time_usec} fix={msg.fix_type} "
            f"sats={msg.satellites_visible} lat={msg.lat} lon={msg.lon} alt={msg.alt}"
        )

    if msg_type == "EKF_STATUS_REPORT":
        return (
            f"EKF_STATUS_REPORT flags={msg.flags} vel_var={msg.velocity_variance:.3f} "
            f"pos_horiz_var={msg.pos_horiz_variance:.3f} pos_vert_var={msg.pos_vert_variance:.3f} "
            f"compass_var={msg.compass_variance:.3f} terrain_var={msg.terrain_alt_variance:.3f}"
        )

    return f"{msg_type} {msg.to_dict()}"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial-path", default=DEFAULT_SERIAL_PATH, help="serial path")
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE, help="serial baudrate")
    parser.add_argument(
        "--source-system",
        type=int,
        default=DEFAULT_SOURCE_SYSTEM,
        help="source system ID used by pymavlink connection",
    )
    return parser.parse_args(argv)


def configure_serial_raw(port: serial.Serial, baudrate: int) -> None:
    baud_attr_name = f"B{baudrate}"
    baud_attr = getattr(termios, baud_attr_name, None)
    if baud_attr is None:
        raise RuntimeError(f"unsupported termios baudrate: {baudrate}")

    fd = port.fileno()
    attrs = termios.tcgetattr(fd)

    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs

    iflag &= ~(
        termios.IGNBRK
        | termios.BRKINT
        | termios.PARMRK
        | termios.ISTRIP
        | termios.INLCR
        | termios.IGNCR
        | termios.ICRNL
        | termios.IXON
        | termios.IXOFF
        | termios.IXANY
    )
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.CRTSCTS)
    cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL

    attrs[0] = iflag
    attrs[1] = oflag
    attrs[2] = cflag
    attrs[3] = lflag
    attrs[4] = baud_attr
    attrs[5] = baud_attr
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0

    termios.tcflush(fd, termios.TCIOFLUSH)
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    print(
        f"starting mavlink uart bridge: serial={args.serial_path} baud={args.baudrate}",
        flush=True,
    )

    while True:
        try:
            connection = mavutil.mavlink_connection(
                args.serial_path,
                baud=args.baudrate,
                source_system=args.source_system,
                autoreconnect=True,
            )
            if isinstance(connection.port, serial.Serial):
                configure_serial_raw(connection.port, args.baudrate)
                connection.port.reset_input_buffer()
                connection.port.reset_output_buffer()
            connection.mav.robust_parsing = True
            print(f"serial open: {args.serial_path} baud={args.baudrate}", flush=True)

            last_status_log = 0.0
            last_other_type_log = 0.0

            while True:
                msg = connection.recv_match(blocking=True, timeout=1.0)
                if msg is None:
                    now = time.time()
                    if now - last_status_log >= 5.0:
                        print("waiting for MAVLink frames...", flush=True)
                        last_status_log = now
                    continue

                msg_type = msg.get_type()
                if msg_type == "BAD_DATA":
                    raw = getattr(msg, "data", b"")
                    preview = " ".join(f"{byte:02X}" for byte in raw[:32])
                    print(f"BAD_DATA len={len(raw)} data={preview}", flush=True)
                    continue

                if msg_type in INTERESTING_TYPES:
                    print(describe_message(msg), flush=True)
                    continue

                now = time.time()
                if now - last_other_type_log >= 5.0:
                    print(
                        f"other MAVLink message type observed: {msg_type}",
                        flush=True,
                    )
                    last_other_type_log = now
        except serial.SerialException as exc:
            print(f"serial exception: {exc}", flush=True)
            print("retrying serial connection in 1s...", flush=True)
            time.sleep(1.0)
            continue
        except OSError as exc:
            print(f"os error on serial connection: {exc}", flush=True)
            print("retrying serial connection in 1s...", flush=True)
            time.sleep(1.0)
            continue


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
