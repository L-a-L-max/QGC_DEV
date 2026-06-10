#!/usr/bin/env python3
"""
DDS Guided Actions 验证脚本
============================
逐一测试 QGC 飞行操控命令是否通过 DDS 正常工作。

用法:
    # 启动后按数字键选择要测试的功能
    python3 test_guided_actions.py

    # 多机（指定 namespace）
    python3 test_guided_actions.py --ns px4_1

前置条件:
    1. PX4 SITL 已启动 (make px4_sitl gz_x500)
    2. MicroXRCE-DDS Agent 已启动
    3. ROS2 + px4_msgs 可用

依赖:
    pip3 install rclpy px4_msgs
"""

import argparse
import sys
import time
import math
import signal

# ─── ROS2 导入 ─────────────────────────────────────────────
try:
    import rclpy
    from rclpy.node import Node
    from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
    from px4_msgs.msg import VehicleCommand, VehicleStatus, VehicleGlobalPosition
    HAS_ROS2 = True
except ImportError:
    HAS_ROS2 = False


class GuidedActionTester:
    def __init__(self, ns=""):
        if not HAS_ROS2:
            print("[错误] 需要 ROS2 + px4_msgs")
            print("  source /opt/ros/humble/setup.bash")
            print("  source ~/ros2_ws/install/setup.bash")
            sys.exit(1)

        rclpy.init()
        self.node = rclpy.create_node('guided_action_tester')
        prefix = f"/{ns}" if ns else ""

        # Command publisher (RELIABLE + TRANSIENT_LOCAL, matching PX4 reader)
        cmd_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        self.cmd_pub = self.node.create_publisher(
            VehicleCommand,
            f"{prefix}/fmu/in/vehicle_command",
            cmd_qos
        )

        # Status subscriber
        status_qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        self.status_sub = self.node.create_subscription(
            VehicleStatus,
            f"{prefix}/fmu/out/vehicle_status",
            self._status_cb,
            status_qos
        )
        self.pos_sub = self.node.create_subscription(
            VehicleGlobalPosition,
            f"{prefix}/fmu/out/vehicle_global_position",
            self._pos_cb,
            status_qos
        )

        self.nav_state = -1
        self.arming_state = -1
        self.lat = 0.0
        self.lon = 0.0
        self.alt = 0.0
        self.ns = ns
        prefix_display = prefix if prefix else "(default)"
        print(f"[ROS2] 命令话题: {prefix}/fmu/in/vehicle_command")
        print(f"[ROS2] 状态话题: {prefix}/fmu/out/vehicle_status")
        print(f"[ROS2] 位置话题: {prefix}/fmu/out/vehicle_global_position")

    def _status_cb(self, msg):
        self.nav_state = msg.nav_state
        self.arming_state = msg.arming_state

    def _pos_cb(self, msg):
        self.lat = msg.lat
        self.lon = msg.lon
        self.alt = msg.alt

    def spin(self, duration=1.0):
        """Spin for a duration to receive messages"""
        end = time.time() + duration
        while time.time() < end:
            rclpy.spin_once(self.node, timeout_sec=0.05)

    def send_command(self, cmd_id, param1=0.0, param2=0.0, param3=0.0,
                     param4=float('nan'), param5=float('nan'),
                     param6=float('nan'), param7=float('nan')):
        msg = VehicleCommand()
        msg.timestamp = 0
        msg.command = cmd_id
        msg.param1 = float(param1)
        msg.param2 = float(param2)
        msg.param3 = float(param3)
        msg.param4 = float(param4)
        msg.param5 = float(param5)
        msg.param6 = float(param6)
        msg.param7 = float(param7)
        msg.target_system = 1
        msg.target_component = 1
        msg.source_system = 255
        msg.source_component = 0
        msg.from_external = True
        self.cmd_pub.publish(msg)

    def nav_state_name(self, state):
        names = {
            0: "MANUAL", 1: "ALTCTL", 2: "POSCTL", 3: "AUTO_MISSION",
            4: "AUTO_LOITER", 5: "AUTO_RTL", 6: "RC_RECOVERY",
            10: "ACRO", 12: "DESCEND", 13: "TERMINATION",
            14: "OFFBOARD", 15: "STAB", 17: "AUTO_TAKEOFF",
            18: "AUTO_LAND", 19: "AUTO_FOLLOW_TARGET",
            20: "AUTO_PRECLAND", 21: "ORBIT",
            22: "AUTO_VTOL_TAKEOFF",
        }
        return names.get(state, f"UNKNOWN({state})")

    def arming_state_name(self, state):
        names = {0: "INIT", 1: "STANDBY", 2: "ARMED", 3: "STANDBY_ERROR",
                 4: "SHUTDOWN", 5: "IN_AIR_RESTORE"}
        return names.get(state, f"UNKNOWN({state})")

    def print_status(self):
        print(f"  当前状态: arming={self.arming_state_name(self.arming_state)}  "
              f"nav_state={self.nav_state_name(self.nav_state)}  "
              f"lat={self.lat:.6f}  lon={self.lon:.6f}  alt={self.alt:.1f}m")

    # ─── 测试功能 ─────────────────────────────────────────

    def test_arm(self):
        """TC-A: ARM (cmd 400, param1=1)"""
        print("\n=== TC-A: ARM ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        self.send_command(400, param1=1.0)
        print("  → 已发送 MAV_CMD_COMPONENT_ARM_DISARM (param1=1)")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.arming_state == 2:
            print("  ✓ ARM 成功")
        else:
            print("  ✗ ARM 失败（检查 PX4 预检状态）")

    def test_disarm(self):
        """TC-B: DISARM (cmd 400, param1=0)"""
        print("\n=== TC-B: DISARM ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        self.send_command(400, param1=0.0)
        print("  → 已发送 MAV_CMD_COMPONENT_ARM_DISARM (param1=0)")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.arming_state == 1:
            print("  ✓ DISARM 成功")
        else:
            print("  ✗ DISARM 失败（飞行中无法 disarm，需先降落）")

    def test_takeoff(self, alt=5.0):
        """TC-C: TAKEOFF (cmd 22)"""
        print(f"\n=== TC-C: TAKEOFF (目标高度 {alt}m) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        takeoff_alt_amsl = self.alt + alt
        self.send_command(22, param1=-1.0, param7=takeoff_alt_amsl)
        print(f"  → 已发送 MAV_CMD_NAV_TAKEOFF (alt_amsl={takeoff_alt_amsl:.1f})")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.nav_state == 17:
            print("  ✓ 进入 AUTO_TAKEOFF 模式")
        else:
            print(f"  ✗ 未进入起飞模式 (当前: {self.nav_state_name(self.nav_state)})")

    def test_land(self):
        """TC-D: LAND (cmd 21)"""
        print("\n=== TC-D: LAND ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        self.send_command(21)
        print("  → 已发送 MAV_CMD_NAV_LAND")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.nav_state == 18:
            print("  ✓ 进入 AUTO_LAND 模式")
        else:
            print(f"  ✗ 未进入降落模式 (当前: {self.nav_state_name(self.nav_state)})")

    def test_rtl(self):
        """TC-E: RTL (cmd 20)"""
        print("\n=== TC-E: RTL ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        self.send_command(20)
        print("  → 已发送 MAV_CMD_NAV_RETURN_TO_LAUNCH")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.nav_state == 5:
            print("  ✓ 进入 AUTO_RTL 模式")
        else:
            print(f"  ✗ 未进入 RTL 模式 (当前: {self.nav_state_name(self.nav_state)})")

    def test_pause(self):
        """TC-01: Pause (MAV_CMD_DO_REPOSITION, param1=-1, param2=1)"""
        print("\n=== TC-01: 暂停 (Pause) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        # PX4FirmwarePlugin::pauseVehicle 使用 MAV_CMD_DO_REPOSITION
        # param1=-1 (ground speed, -1=no change)
        # param2=1 (MAV_DO_REPOSITION_FLAGS_CHANGE_MODE)
        # param5-7=NAN (keep current position)
        self.send_command(192,  # MAV_CMD_DO_REPOSITION
                          param1=-1.0,
                          param2=1.0)  # MAV_DO_REPOSITION_FLAGS_CHANGE_MODE
        print("  → 已发送 MAV_CMD_DO_REPOSITION (暂停)")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.nav_state == 4:  # AUTO_LOITER
            print("  ✓ 进入 AUTO_LOITER（暂停）模式")
        elif self.nav_state == 2:  # POSCTL
            print("  ✓ 进入 POSCTL 模式（悬停）")
        else:
            print(f"  ? 当前模式: {self.nav_state_name(self.nav_state)}")

    def test_change_altitude(self, delta=3.0):
        """TC-02: Change Altitude (MAV_CMD_DO_REPOSITION with new altitude)"""
        print(f"\n=== TC-02: 改变高度 (+{delta}m) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        new_alt_amsl = self.alt + delta
        # PX4 guidedModeChangeAltitude: 先暂停，再 DO_REPOSITION 到新高度
        # Step 1: Pause
        self.send_command(192, param1=-1.0, param2=1.0)
        self.spin(1.0)
        # Step 2: Reposition to new altitude
        self.send_command(192,
                          param1=-1.0,
                          param2=1.0,
                          param5=self.lat,
                          param6=self.lon,
                          param7=new_alt_amsl)
        print(f"  → 已发送 MAV_CMD_DO_REPOSITION (新高度 AMSL={new_alt_amsl:.1f}m)")
        self.spin(3.0)
        print("  发送后:")
        self.print_status()
        print(f"  预期: 无人机上升约 {delta}m")

    def test_change_speed(self, speed=3.0):
        """TC-03: Change Speed (MAV_CMD_DO_CHANGE_SPEED, cmd 178)"""
        print(f"\n=== TC-03: 改变速度 ({speed} m/s) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        # param1=1 (groundspeed), param2=speed, param3=-1 (throttle no change)
        self.send_command(178,
                          param1=1.0,
                          param2=speed,
                          param3=-1.0,
                          param4=0.0)
        print(f"  → 已发送 MAV_CMD_DO_CHANGE_SPEED (地速={speed} m/s)")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        print(f"  预期: 后续飞行速度限制为 {speed} m/s")
        print("  验证: 在 PX4 终端执行 'listener vehicle_local_position' 查看 vx/vy")

    def test_change_heading(self, heading_deg=90.0):
        """TC-04: Change Heading (MAV_CMD_DO_REPOSITION with yaw)"""
        print(f"\n=== TC-04: 改变航向 ({heading_deg}°) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        heading_rad = math.radians(heading_deg)
        # PX4FirmwarePlugin::guidedModeChangeHeading uses DO_REPOSITION
        # param4 = heading in radians
        self.send_command(192,
                          param1=-1.0,
                          param2=1.0,   # MAV_DO_REPOSITION_FLAGS_CHANGE_MODE
                          param3=0.0,
                          param4=heading_rad,
                          param5=self.lat,
                          param6=self.lon,
                          param7=self.alt)
        print(f"  → 已发送 MAV_CMD_DO_REPOSITION (航向={heading_deg}°={heading_rad:.2f}rad)")
        self.spin(3.0)
        print("  发送后:")
        self.print_status()
        print(f"  预期: 无人机转向 {heading_deg}°")
        print("  验证: 在 PX4 终端执行 'listener vehicle_attitude' 查看 yaw")

    def test_orbit(self, radius=10.0):
        """TC-05: Orbit (MAV_CMD_DO_ORBIT, cmd 34)"""
        print(f"\n=== TC-05: 绕点飞行 (半径={radius}m) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        # MAV_CMD_DO_ORBIT = 34
        # param1=radius, param2=NaN(default velocity), param3=0(yaw unchanged), param4=NaN(default orbits)
        # param5-7 = current position
        self.send_command(34,
                          param1=radius,
                          param2=float('nan'),
                          param3=0.0,
                          param4=float('nan'),
                          param5=self.lat,
                          param6=self.lon,
                          param7=self.alt)
        print(f"  → 已发送 MAV_CMD_DO_ORBIT (半径={radius}m, 当前位置)")
        self.spin(3.0)
        print("  发送后:")
        self.print_status()
        if self.nav_state == 21:  # ORBIT
            print("  ✓ 进入 ORBIT 模式")
        else:
            print(f"  ? 当前模式: {self.nav_state_name(self.nav_state)}")
            print("  注意: PX4 SITL 可能不支持 ORBIT 模式")

    def test_emergency_stop(self):
        """TC-06: Emergency Stop (cmd 400, param1=0, param2=21196)"""
        print("\n=== TC-06: 紧急停机 ===")
        print("  ⚠️ 警告：无人机将立即停止电机并坠落！")
        confirm = input("  确认执行紧急停机? (y/N): ").strip().lower()
        if confirm != 'y':
            print("  已取消")
            return
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        self.send_command(400, param1=0.0, param2=21196.0)
        print("  → 已发送 MAV_CMD_COMPONENT_ARM_DISARM (force disarm, magic=21196)")
        self.spin(2.0)
        print("  发送后:")
        self.print_status()
        if self.arming_state == 1:
            print("  ✓ 紧急停机成功（已 Disarm）")
        else:
            print("  ✗ 紧急停机失败")

    def test_goto(self, lat_offset=0.0001, lon_offset=0.0):
        """TC-07: Go To Location (MAV_CMD_DO_REPOSITION, cmd 192)"""
        print(f"\n=== TC-07: 飞往指定位置 (lat+{lat_offset}, lon+{lon_offset}) ===")
        self.spin(0.5)
        print("  发送前:")
        self.print_status()
        target_lat = self.lat + lat_offset
        target_lon = self.lon + lon_offset
        self.send_command(192,
                          param1=-1.0,
                          param2=1.0,   # MAV_DO_REPOSITION_FLAGS_CHANGE_MODE
                          param5=target_lat,
                          param6=target_lon,
                          param7=self.alt)
        print(f"  → 已发送 MAV_CMD_DO_REPOSITION (目标: {target_lat:.6f}, {target_lon:.6f})")
        self.spin(3.0)
        print("  发送后:")
        self.print_status()
        print(f"  预期: 无人机飞往 ({target_lat:.6f}, {target_lon:.6f})")

    def destroy(self):
        self.node.destroy_node()
        rclpy.shutdown()


def main():
    parser = argparse.ArgumentParser(description="DDS Guided Actions 验证工具")
    parser.add_argument("--ns", default="", help="PX4 namespace (如 px4_1)")
    args = parser.parse_args()

    tester = GuidedActionTester(ns=args.ns)

    def sig_handler(sig, frame):
        tester.destroy()
        sys.exit(0)
    signal.signal(signal.SIGINT, sig_handler)

    # 等待首次数据
    print("\n等待 PX4 状态数据...")
    tester.spin(2.0)
    tester.print_status()

    menu = """
╔══════════════════════════════════════════════════╗
║       DDS Guided Actions 验证工具                ║
╠══════════════════════════════════════════════════╣
║  基础命令（已验证）:                             ║
║    a = ARM          b = DISARM                   ║
║    c = TAKEOFF      d = LAND                     ║
║    e = RTL                                       ║
║                                                  ║
║  待验证功能:                                     ║
║    1 = 暂停 (Pause)                              ║
║    2 = 改变高度 (+3m)                            ║
║    3 = 改变速度 (3 m/s)                          ║
║    4 = 改变航向 (90°)                            ║
║    5 = 绕点飞行 (Orbit, 半径10m)                 ║
║    6 = 紧急停机 (Emergency Stop)                 ║
║    7 = 飞往指定位置 (Go To, 北偏约11m)           ║
║                                                  ║
║    s = 显示当前状态                               ║
║    q = 退出                                      ║
╚══════════════════════════════════════════════════╝
"""

    while True:
        print(menu)
        choice = input("选择测试项: ").strip().lower()

        if choice == 'a':
            tester.test_arm()
        elif choice == 'b':
            tester.test_disarm()
        elif choice == 'c':
            tester.test_takeoff()
        elif choice == 'd':
            tester.test_land()
        elif choice == 'e':
            tester.test_rtl()
        elif choice == '1':
            tester.test_pause()
        elif choice == '2':
            tester.test_change_altitude()
        elif choice == '3':
            tester.test_change_speed()
        elif choice == '4':
            tester.test_change_heading()
        elif choice == '5':
            tester.test_orbit()
        elif choice == '6':
            tester.test_emergency_stop()
        elif choice == '7':
            tester.test_goto()
        elif choice == 's':
            tester.spin(0.5)
            tester.print_status()
        elif choice == 'q':
            break
        else:
            print("  无效选项")

    tester.destroy()
    print("退出。")


if __name__ == "__main__":
    main()
