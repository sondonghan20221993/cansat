#!/usr/bin/env python3
import struct
import serial
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import NavSatFix, Imu
import time

class CFSRoS2Bridge(Node):
    def __init__(self):
        super().__init__('cfs_ros2_bridge')
        
        # LoRa 포트 (지상국과 통신)
        try:
            self.lora = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
            self.get_logger().info('LoRa 연결됨')
        except Exception as e:
            self.get_logger().error(f'LoRa 연결 실패: {e}')
            self.lora = None
        
        # ROS2 토픽 구독 (비행체의 센서)
        self.create_subscription(NavSatFix, '/fix', self.callback_fix, 10)
        self.create_subscription(Imu, '/imu', self.callback_imu, 10)
        
        # CFS에서 publish한 토픽도 구독 (추정값)
        self.create_subscription(NavSatFix, '/cfs/estimated_state', self.callback_cfs_estimate, 10)
        
        self.get_logger().info('CFS-ROS2 Bridge 시작됨')

    def pack_cfs_message(self, msg_id, payload):
        """CFS 메시지 패킹"""
        timestamp = int(time.time() * 1000)  # milliseconds
        header = struct.pack('>HHI', 
            msg_id,
            len(payload),
            timestamp
        )
        return header + payload

    def send_to_ground(self, msg_id, payload):
        if not self.lora:
            return
        try:
            cfs_message = self.pack_cfs_message(msg_id, payload)
            self.lora.write(cfs_message)  # ← '\n' 제거
            self.get_logger().info(f'✅ 지상국 전송: ID=0x{msg_id:04x}')
        except Exception as e:
            self.get_logger().error(f'❌ LoRa 송신 오류: {e}')


    def callback_fix(self, msg: NavSatFix):
        """GPS 센서 데이터 (ROS2 토픽) → CFS → 지상국"""
        payload = struct.pack('>ddd',
            float(msg.latitude),
            float(msg.longitude),
            float(msg.altitude)
        )
        self.send_to_ground(0x1001, payload)

    def callback_imu(self, msg: Imu):
        """IMU 센서 데이터 (ROS2 토픽) → CFS → 지상국"""
        payload = struct.pack('>fff',
            float(msg.linear_acceleration.x),
            float(msg.linear_acceleration.y),
            float(msg.linear_acceleration.z)
        )
        self.send_to_ground(0x1002, payload)

    def callback_cfs_estimate(self, msg: NavSatFix):
        """CFS의 추정값 (CFS App publish) → 지상국"""
        payload = struct.pack('>ddd',
            float(msg.latitude),
            float(msg.longitude),
            float(msg.altitude)
        )
        self.send_to_ground(0x2001, payload)  # 다른 ID

def main():
    rclpy.init()
    node = CFSRoS2Bridge()
    rclpy.spin(node)

if __name__ == "__main__":
    main()