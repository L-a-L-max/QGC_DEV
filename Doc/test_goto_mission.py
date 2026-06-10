#!/usr/bin/env python3
"""
DDS GotoSetpoint Mission Test Script
=====================================
Tests the goto_setpoint topic by sending sequential waypoints to PX4.
Monitors vehicle_global_position for arrival detection.

Usage:
    source /opt/ros/humble/setup.bash
    source ~/ros2_ws/install/setup.bash

    # Start PX4 SITL + MicroXRCEAgent first
    python3 test_goto_mission.py

    # With custom namespace
    python3 test_goto_mission.py --ns px4_2

Prerequisites:
    - PX4 SITL running with MicroXRCEAgent
    - px4_msgs built in ROS2 workspace
    - Vehicle armed and airborne (takeoff first)
"""

import argparse
import math
import sys
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy

from px4_msgs.msg import GotoSetpoint, VehicleGlobalPosition, VehicleCommand, VehicleStatus


EARTH_RADIUS = 6371000.0


def geo_to_ned(lat, lon, alt, home_lat, home_lon, home_alt):
    """Convert WGS84 lat/lon/alt to NED relative to home."""
    d_lat = math.radians(lat - home_lat)
    d_lon = math.radians(lon - home_lon)
    cos_home = math.cos(math.radians(home_lat))
    north = d_lat * EARTH_RADIUS
    east = d_lon * EARTH_RADIUS * cos_home
    down = home_alt - alt
    return north, east, down


class GotoMissionTest(Node):
    def __init__(self, namespace):
        super().__init__('goto_mission_test')

        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=5
        )

        prefix = f'/{namespace}/' if namespace else '/'

        self.goto_pub = self.create_publisher(
            GotoSetpoint, f'{prefix}fmu/in/goto_setpoint', qos)
        self.cmd_pub = self.create_publisher(
            VehicleCommand, f'{prefix}fmu/in/vehicle_command', qos)

        self.pos_sub = self.create_subscription(
            VehicleGlobalPosition, f'{prefix}fmu/out/vehicle_global_position',
            self._pos_callback, qos)
        self.status_sub = self.create_subscription(
            VehicleStatus, f'{prefix}fmu/out/vehicle_status',
            self._status_callback, qos)

        self.vehicle_lat = 0.0
        self.vehicle_lon = 0.0
        self.vehicle_alt = 0.0
        self.home_lat = 0.0
        self.home_lon = 0.0
        self.home_alt = 0.0
        self.home_set = False
        self.pos_received = False
        self.arming_state = 0

    def _pos_callback(self, msg):
        self.vehicle_lat = msg.lat
        self.vehicle_lon = msg.lon
        self.vehicle_alt = msg.alt
        if not self.home_set and msg.lat != 0.0:
            self.home_lat = msg.lat
            self.home_lon = msg.lon
            self.home_alt = msg.alt
            self.home_set = True
            self.get_logger().info(
                f'Home set: lat={msg.lat:.6f} lon={msg.lon:.6f} alt={msg.alt:.1f}')
        self.pos_received = True

    def _status_callback(self, msg):
        self.arming_state = msg.arming_state

    def send_command(self, command, p1=0.0, p2=0.0, p3=0.0, p4=0.0,
                     p5=0.0, p6=0.0, p7=0.0):
        msg = VehicleCommand()
        msg.timestamp = 0
        msg.command = command
        msg.param1 = float(p1)
        msg.param2 = float(p2)
        msg.param3 = float(p3)
        msg.param4 = float(p4)
        msg.param5 = float(p5)
        msg.param6 = float(p6)
        msg.param7 = float(p7)
        msg.target_system = 1
        msg.target_component = 1
        msg.source_system = 255
        msg.from_external = True
        self.cmd_pub.publish(msg)

    def arm(self):
        self.get_logger().info('ARM')
        self.send_command(400, p1=1.0)

    def takeoff(self, alt=10.0):
        self.get_logger().info(f'TAKEOFF to {alt}m')
        self.send_command(22, p1=0.0, p7=alt)

    def land(self):
        self.get_logger().info('LAND')
        self.send_command(21)

    def rtl(self):
        self.get_logger().info('RTL')
        self.send_command(20)

    def send_goto(self, lat, lon, alt_amsl, max_h_speed=-1.0, heading=float('nan')):
        if not self.home_set:
            self.get_logger().warn('Home not set, cannot send goto')
            return

        n, e, d = geo_to_ned(lat, lon, alt_amsl,
                             self.home_lat, self.home_lon, self.home_alt)

        msg = GotoSetpoint()
        msg.timestamp = 0
        msg.position = [float(n), float(e), float(d)]

        msg.flag_control_heading = not math.isnan(heading)
        msg.heading = heading if msg.flag_control_heading else 0.0

        msg.flag_set_max_horizontal_speed = max_h_speed > 0
        msg.max_horizontal_speed = max_h_speed if msg.flag_set_max_horizontal_speed else 0.0

        msg.flag_set_max_vertical_speed = False
        msg.max_vertical_speed = 0.0

        msg.flag_set_max_heading_rate = False
        msg.max_heading_rate = 0.0

        self.goto_pub.publish(msg)
        self.get_logger().info(f'Goto NED=({n:.1f}, {e:.1f}, {d:.1f}) spd={max_h_speed}')

    def distance_to(self, lat, lon, alt_amsl):
        from math import radians, cos, sin, sqrt, atan2
        dlat = radians(lat - self.vehicle_lat)
        dlon = radians(lon - self.vehicle_lon)
        a = sin(dlat/2)**2 + cos(radians(self.vehicle_lat)) * cos(radians(lat)) * sin(dlon/2)**2
        h_dist = 2 * EARTH_RADIUS * atan2(sqrt(a), sqrt(1-a))
        v_dist = abs(self.vehicle_alt - alt_amsl)
        return h_dist, v_dist

    def wait_for_arrival(self, lat, lon, alt_amsl, radius=3.0, timeout=60.0):
        start = time.time()
        while time.time() - start < timeout:
            rclpy.spin_once(self, timeout_sec=0.2)
            h, v = self.distance_to(lat, lon, alt_amsl)
            if h < radius and v < 2.0:
                self.get_logger().info(f'Arrived! h={h:.1f}m v={v:.1f}m')
                return True
            if int(time.time()) % 3 == 0:
                self.send_goto(lat, lon, alt_amsl)
        self.get_logger().warn(f'Timeout waiting for arrival (h={h:.1f}m)')
        return False

    def wait_for_position(self, timeout=15.0):
        start = time.time()
        while not self.pos_received and time.time() - start < timeout:
            rclpy.spin_once(self, timeout_sec=0.5)
        return self.pos_received


def main():
    parser = argparse.ArgumentParser(description='DDS GotoSetpoint Mission Test')
    parser.add_argument('--ns', default='', help='PX4 namespace (e.g. px4_2)')
    args = parser.parse_args()

    rclpy.init()
    node = GotoMissionTest(args.ns)

    print('='*60)
    print('DDS GotoSetpoint Mission Test')
    print('='*60)
    print()
    print('Waiting for vehicle position...')

    if not node.wait_for_position():
        print('ERROR: No position data received. Is PX4 + XRCE Agent running?')
        rclpy.shutdown()
        return

    print(f'Vehicle at: {node.vehicle_lat:.6f}, {node.vehicle_lon:.6f}, {node.vehicle_alt:.1f}m')
    print()

    # Define mission waypoints relative to home
    # Offsets in meters: (north, east, alt_relative)
    wp_offsets = [
        (30, 0,   10),   # WP1: 30m north
        (30, 30,  15),   # WP2: 30m north, 30m east, 15m alt
        (0,  30,  10),   # WP3: 30m east
        (0,  0,   10),   # WP4: back to start
    ]

    # Convert to GPS coordinates
    waypoints = []
    for dn, de, alt_rel in wp_offsets:
        lat = node.home_lat + (dn / EARTH_RADIUS) * (180.0 / math.pi)
        lon = node.home_lon + (de / (EARTH_RADIUS * math.cos(math.radians(node.home_lat)))) * (180.0 / math.pi)
        alt_amsl = node.home_alt + alt_rel
        waypoints.append((lat, lon, alt_amsl))

    print('Mission waypoints:')
    for i, (lat, lon, alt) in enumerate(waypoints):
        print(f'  WP{i+1}: {lat:.6f}, {lon:.6f}, {alt:.1f}m AMSL')
    print()

    input('Press Enter to ARM and TAKEOFF...')
    node.arm()
    time.sleep(1)
    rclpy.spin_once(node, timeout_sec=0.5)
    node.takeoff(10.0)

    print('Waiting 10s for takeoff...')
    for _ in range(20):
        rclpy.spin_once(node, timeout_sec=0.5)

    print()
    input('Press Enter to start GotoSetpoint mission...')

    for i, (lat, lon, alt) in enumerate(waypoints):
        print(f'\n--- Flying to WP{i+1} ---')
        node.send_goto(lat, lon, alt, max_h_speed=5.0)
        if node.wait_for_arrival(lat, lon, alt, radius=3.0, timeout=60.0):
            print(f'WP{i+1} reached!')
            time.sleep(2)  # brief hover
        else:
            print(f'WP{i+1} timeout, continuing...')

    print('\n' + '='*60)
    print('Mission complete!')
    print('='*60)
    print()

    choice = input('End action? [h]over / [r]tl / [l]and: ').strip().lower()
    if choice == 'r':
        node.rtl()
    elif choice == 'l':
        node.land()
    else:
        print('Hovering in place.')

    # Keep spinning for a bit to receive final position updates
    for _ in range(20):
        rclpy.spin_once(node, timeout_sec=0.5)

    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
