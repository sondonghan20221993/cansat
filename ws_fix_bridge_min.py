import asyncio, json, rclpy, websockets
from rclpy.node import Node
from sensor_msgs.msg import NavSatFix
PORT=765; latest=None
class N(Node):
  def __init__(s): super().__init__('ws_fix_bridge_min'); s.create_subscription(NavSatFix,'/fix',s.cb,10)
  def cb(s,m):
    global latest; latest={"lat":m.latitude,"lon":m.longitude,"alt":m.altitude}
async def h(ws):
  while True:
    await asyncio.sleep(0.1)
    if latest: await ws.send(json.dumps(latest))
def spin(n): rclpy.spin(n)
async def main():
  rclpy.init(); n=N()
  async with websockets.serve(h,"0.0.0.0",PORT):
    await asyncio.get_running_loop().run_in_executor(None, spin, n)
asyncio.run(main())
