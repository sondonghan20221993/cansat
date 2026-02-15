#!/usr/bin/env python3
import asyncio, json, threading
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import NavSatFix

import websockets

LATEST = None
LOCK = threading.Lock()

class FixSub(Node):
    def __init__(self):
        super().__init__('ws_fix_bridge')
        self.create_subscription(NavSatFix, '/fix', self.cb, 10)

    def cb(self, msg: NavSatFix):
        global LATEST
        data = {
            "t": self.get_clock().now().nanoseconds / 1e9,
            "lat": float(msg.latitude),
            "lon": float(msg.longitude),
            "alt": float(msg.altitude),
            "status": int(msg.status.status),
            "service": int(msg.status.service),
        }
        with LOCK:
            LATEST = data

async def ws_handler(websocket):
    # 10 Hz 송출 (필요하면 5/20Hz로 조정)
    while True:
        with LOCK:
            data = LATEST
        if data is not None:
            await websocket.send(json.dumps(data))
        await asyncio.sleep(0.1)

async def main_async():
    async with websockets.serve(lambda ws: ws_handler(ws), "0.0.0.0", 8765):
        await asyncio.Future()  # run forever

def main():
    rclpy.init()
    node = FixSub()

    spin_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    spin_thread.start()

    asyncio.run(main_async())

if __name__ == "__main__":
    main()
