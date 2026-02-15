import asyncio, json, rclpy, websockets, struct
from rclpy.node import Node
from sensor_msgs.msg import NavSatFix

PORT=8765
MID=0x0883          # 원하는 cFS MID로 바꾸세요 (예: 0x1001 등)
latest=None

class N(Node):
  def __init__(s):
    super().__init__('ws_fix_bridge_min')
    s.create_subscription(NavSatFix,'/fix',s.cb,10)
  def cb(s,m):
    global latest
    latest={"lat":m.latitude,"lon":m.longitude,"alt":m.altitude}

async def h(ws):
  while True:
    await asyncio.sleep(0.1)
    if latest:
      payload = json.dumps(latest).encode('utf-8')  # bytes
      frame = struct.pack('>H', MID) + payload      # 앞 2바이트 = BE MID
      await ws.send(frame)                          # bytes => 바이너리 프레임

def spin(n): rclpy.spin(n)

async def main():
  rclpy.init()
  n=N()
  async with websockets.serve(h,"0.0.0.0",PORT):
    await asyncio.get_running_loop().run_in_executor(None, spin, n)

asyncio.run(main())
