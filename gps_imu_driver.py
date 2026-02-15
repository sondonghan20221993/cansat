#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from racs2_msg.msg import RACS2UserMsg
from sensor_msgs.msg import NavSatFix, Imu
import serial
import json
import struct
import os

class GPSIMUDriver(Node):
    def __init__(self):
        super().__init__('gps_imu_driver')
        
        # RACS2 브릿지 publisher
        self.racs2_pub = self.create_publisher(RACS2UserMsg, '/RACS2Bridge', 10)
        
        # ROS2 표준 토픽 publisher
        self.gps_pub = self.create_publisher(NavSatFix, '/fix', 10)
        self.imu_pub = self.create_publisher(Imu, '/imu', 10)
        
        # GPS UART 연결 (9600)
        gps_port = None
        for port in ['/dev/serial0', '/dev/ttyAMA0', '/dev/ttyUSB0']:
            if os.path.exists(port):
                gps_port = port
                break
        
        try:
            self.gps_serial = serial.Serial(gps_port, 9600, timeout=1)
            self.get_logger().info(f'GPS 연결됨: {gps_port}')
        except Exception as e:
            self.get_logger().error(f'GPS 연결 실패: {e}')
            self.gps_serial = None
        
        # IMU I2C 연결
        try:
            os.system('sudo chmod 666 /dev/i2c-1')
            import smbus2
            self.i2c = smbus2.SMBus(1)
            self.mpu_addr = 0x68
            self.get_logger().info('IMU 연결됨')
        except Exception as e:
            self.get_logger().error(f'IMU 연결 실패: {e}')
            self.i2c = None
        
        self.timer = self.create_timer(0.5, self.publish_data)
    
    def read_gps(self):
        if not self.gps_serial:
            return None
        try:
            if self.gps_serial.in_waiting:
                line = self.gps_serial.readline().decode().strip()
                if line.startswith('$GPRMC'):
                    parts = line.split(',')
                    if len(parts) > 4:
                        lat = float(parts[3][:2]) + float(parts[3][2:])/60
                        lon = float(parts[5][:3]) + float(parts[5][3:])/60
                        return {'lat': lat, 'lon': lon, 'alt': 0}
        except:
            pass
        return None
    
    def read_imu(self):
        if not self.i2c:
            return None
        try:
            data = self.i2c.read_i2c_block_data(self.mpu_addr, 0x3B, 6)
            ax = struct.unpack('>h', bytes([data[0], data[1]]))[0] / 16384.0 * 9.8
            ay = struct.unpack('>h', bytes([data[2], data[3]]))[0] / 16384.0 * 9.8
            az = struct.unpack('>h', bytes([data[4], data[5]]))[0] / 16384.0 * 9.8
            return {'ax': ax, 'ay': ay, 'az': az}
        except:
            pass
        return None
    
    def publish_data(self):
        gps = self.read_gps()
        imu = self.read_imu()
        
        # GPS 표준 토픽 publish (값이 없어도 계속 publish)
        gps_msg = NavSatFix()
        if gps:
            gps_msg.latitude = gps['lat']
            gps_msg.longitude = gps['lon']
            gps_msg.altitude = float(gps['alt'])
        else:
            gps_msg.latitude = 0.0
            gps_msg.longitude = 0.0
            gps_msg.altitude = 0.0
        self.gps_pub.publish(gps_msg)
        
        # IMU 표준 토픽 publish
        imu_msg = Imu()
        if imu:
            imu_msg.linear_acceleration.x = imu['ax']
            imu_msg.linear_acceleration.y = imu['ay']
            imu_msg.linear_acceleration.z = imu['az']
        else:
            imu_msg.linear_acceleration.x = 0.0
            imu_msg.linear_acceleration.y = 0.0
            imu_msg.linear_acceleration.z = 0.0
        self.imu_pub.publish(imu_msg)
        
        # RACS2 브릿지용 JSON publish (항상 보내기)
        data = {
            'gps': gps if gps else {'lat': 0, 'lon': 0, 'alt': 0},
            'imu': imu if imu else {'ax': 0, 'ay': 0, 'az': 0}
        }
        
        msg = RACS2UserMsg()
        msg.cfs_message_id = 1

        body = json.dumps(data).encode('utf-8')
        msg.body_data = [body[i:i+1] for i in range(len(body))]
        msg.body_data_length = len(body)

        self.racs2_pub.publish(msg)
        
        self.get_logger().info(f'GPS: {data["gps"]}, IMU: {data["imu"]}')

def main():
    rclpy.init()
    rclpy.spin(GPSIMUDriver())

if __name__ == "__main__":
    main()