#!/usr/bin/env python3
"""
DDS ManualControlSetpoint 测试脚本
===================================
通过 ROS2 DDS 话题直接发布 ManualControlSetpoint 消息，
绕过 QGC，直接控制 PX4 无人机飞行。

用法:
    # 单机（默认实例）
    python3 test_manual_control.py

    # 多机（指定 namespace，如 px4_2）
    python3 test_manual_control.py --ns px4_2

    # 自定义发布频率
    python3 test_manual_control.py --hz 50

键盘控制:
    W/S     - 俯仰 (pitch: 前进/后退)
    A/D     - 横滚 (roll: 左移/右移)
    Q/E     - 偏航 (yaw: 左转/右转)
    T/G     - 油门 (throttle: 上升/下降)
    Space   - 油门回中 (throttle=0)
    R       - 全部回中
    1       - 发送起飞命令 (MAV_CMD_NAV_TAKEOFF)
    2       - 发送降落命令 (MAV_CMD_NAV_LAND)
    3       - 发送 ARM 命令
    4       - 发送 DISARM 命令
    Esc/Ctrl+C - 退出

依赖:
    pip3 install rclpy px4_msgs

    如果没有 ROS2 环境，可以使用 --no-ros 模式（仅打印数据，不实际发送）
"""

import argparse
import sys
import time
import threading
import signal

# ─── 键盘输入（跨平台） ───────────────────────────────────
try:
    import termios
    import tty

    def _getch():
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            return sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)
except ImportError:
    import msvcrt
    def _getch():
        return msvcrt.getch().decode('utf-8', errors='ignore')


# ─── 参数解析 ──────────────────────────────────────────────
def parse_args():
    p = argparse.ArgumentParser(description="DDS ManualControlSetpoint 测试工具")
    p.add_argument("--ns", default="", help="PX4 namespace (如 px4_1, px4_2)")
    p.add_argument("--hz", type=int, default=25, help="发布频率 (默认 25Hz)")
    p.add_argument("--no-ros", action="store_true", help="不使用 ROS2，仅打印数据")
    p.add_argument("--step", type=float, default=0.1, help="每次按键的增量 (默认 0.1)")
    return p.parse_args()


# ─── 控制状态 ──────────────────────────────────────────────
class StickState:
    def __init__(self, step=0.1):
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0
        self.throttle = 0.0  # [-1, 1]，中心=0
        self.step = step
        self.running = True

    def clamp(self, v, lo=-1.0, hi=1.0):
        return max(lo, min(hi, v))

    def handle_key(self, ch):
        ch = ch.lower()
        if ch == 'w':
            self.pitch = self.clamp(self.pitch - self.step)  # nose down = negative pitch = forward
        elif ch == 's':
            self.pitch = self.clamp(self.pitch + self.step)
        elif ch == 'a':
            self.roll = self.clamp(self.roll - self.step)
        elif ch == 'd':
            self.roll = self.clamp(self.roll + self.step)
        elif ch == 'q':
            self.yaw = self.clamp(self.yaw - self.step)
        elif ch == 'e':
            self.yaw = self.clamp(self.yaw + self.step)
        elif ch == 't':
            self.throttle = self.clamp(self.throttle + self.step)
        elif ch == 'g':
            self.throttle = self.clamp(self.throttle - self.step)
        elif ch == ' ':
            self.throttle = 0.0
        elif ch == 'r':
            self.roll = self.pitch = self.yaw = self.throttle = 0.0
        elif ch == '\x1b' or ch == '\x03':  # Esc or Ctrl+C
            self.running = False
        else:
            return ch  # 返回未处理的键
        return None


# ─── ROS2 发布器 ───────────────────────────────────────────
class ROS2Publisher:
    def __init__(self, ns, hz):
        import rclpy
        from rclpy.node import Node
        from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
        from px4_msgs.msg import ManualControlSetpoint, VehicleCommand

        rclpy.init()
        self.node = rclpy.create_node('manual_control_test')

        prefix = f"/{ns}" if ns else ""

        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=2
        )

        self.mc_pub = self.node.create_publisher(
            ManualControlSetpoint,
            f"{prefix}/fmu/in/manual_control_input",
            qos
        )

        # VehicleCommand 用于起飞/降落/ARM 等
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

        self.ManualControlSetpoint = ManualControlSetpoint
        self.VehicleCommand = VehicleCommand
        self.hz = hz
        self.ns = ns
        print(f"[ROS2] 话题: {prefix}/fmu/in/manual_control_input ({hz}Hz)")
        print(f"[ROS2] 命令: {prefix}/fmu/in/vehicle_command")

    def publish_mc(self, state: StickState):
        msg = self.ManualControlSetpoint()
        msg.timestamp = 0  # PX4 会用 hrt_absolute_time() 替换
        msg.timestamp_sample = 0
        msg.valid = True
        msg.data_source = 2  # SOURCE_MAVLINK_0
        msg.roll = state.roll
        msg.pitch = state.pitch
        msg.yaw = state.yaw
        msg.throttle = state.throttle
        msg.sticks_moving = (abs(state.roll) > 0.01 or abs(state.pitch) > 0.01
                             or abs(state.yaw) > 0.01 or abs(state.throttle) > 0.05)
        self.mc_pub.publish(msg)

    def send_command(self, cmd_id, param1=0.0, param2=0.0, param5=0.0, param7=0.0):
        msg = self.VehicleCommand()
        msg.timestamp = 0
        msg.command = cmd_id
        msg.param1 = param1
        msg.param2 = param2
        msg.param5 = param5
        msg.param7 = param7
        msg.target_system = 1
        msg.target_component = 1
        msg.source_system = 255
        msg.source_component = 0
        msg.from_external = True
        self.cmd_pub.publish(msg)

    def send_takeoff(self, alt=2.5):
        """MAV_CMD_NAV_TAKEOFF (22)"""
        self.send_command(22, param7=alt)
        print(f"\n>>> 发送起飞命令 (高度={alt}m)")

    def send_land(self):
        """MAV_CMD_NAV_LAND (21)"""
        self.send_command(21)
        print("\n>>> 发送降落命令")

    def send_arm(self):
        """MAV_CMD_COMPONENT_ARM_DISARM (400) param1=1"""
        self.send_command(400, param1=1.0)
        print("\n>>> 发送 ARM 命令")

    def send_disarm(self):
        """MAV_CMD_COMPONENT_ARM_DISARM (400) param1=0"""
        self.send_command(400, param1=0.0)
        print("\n>>> 发送 DISARM 命令")

    def spin_once(self):
        import rclpy
        rclpy.spin_once(self.node, timeout_sec=0)

    def destroy(self):
        self.node.destroy_node()
        import rclpy
        rclpy.shutdown()


# ─── 无 ROS2 模式 ─────────────────────────────────────────
class DummyPublisher:
    def __init__(self, ns, hz):
        prefix = f"/{ns}" if ns else ""
        print(f"[DRY-RUN] 话题: {prefix}/fmu/in/manual_control_input ({hz}Hz)")
        print("[DRY-RUN] 不实际发送，仅打印数据")
        self.hz = hz

    def publish_mc(self, state: StickState):
        pass  # 不打印每帧

    def send_takeoff(self, alt=2.5):
        print(f"\n>>> [DRY-RUN] 起飞命令 (alt={alt})")

    def send_land(self):
        print("\n>>> [DRY-RUN] 降落命令")

    def send_arm(self):
        print("\n>>> [DRY-RUN] ARM 命令")

    def send_disarm(self):
        print("\n>>> [DRY-RUN] DISARM 命令")

    def spin_once(self):
        pass

    def destroy(self):
        pass


# ─── HUD 显示 ─────────────────────────────────────────────
def print_hud(state: StickState, count: int):
    """在同一行更新状态显示"""
    bar_r = "=" * int(abs(state.roll) * 10)
    bar_p = "=" * int(abs(state.pitch) * 10)
    bar_y = "=" * int(abs(state.yaw) * 10)
    bar_t = "=" * int(abs(state.throttle) * 10)

    sys.stdout.write(
        f"\r  roll={state.roll:+.2f}  pitch={state.pitch:+.2f}  "
        f"yaw={state.yaw:+.2f}  throttle={state.throttle:+.2f}  "
        f"[{count}]    "
    )
    sys.stdout.flush()


# ─── 主循环 ───────────────────────────────────────────────
def main():
    args = parse_args()
    state = StickState(step=args.step)

    if args.no_ros:
        pub = DummyPublisher(args.ns, args.hz)
    else:
        try:
            pub = ROS2Publisher(args.ns, args.hz)
        except Exception as e:
            print(f"[错误] 无法初始化 ROS2: {e}")
            print("[提示] 使用 --no-ros 可在无 ROS2 环境下运行（仅打印数据）")
            sys.exit(1)

    print("\n" + "=" * 60)
    print("  DDS ManualControlSetpoint 测试工具")
    print("=" * 60)
    print("  W/S = 俯仰(前/后)  A/D = 横滚(左/右)")
    print("  Q/E = 偏航(左/右)  T/G = 油门(升/降)")
    print("  Space = 油门回中    R = 全部回中")
    print("  1 = 起飞  2 = 降落  3 = ARM  4 = DISARM")
    print("  Esc = 退出")
    print("=" * 60)

    # 键盘输入线程
    def key_reader():
        while state.running:
            try:
                ch = _getch()
                remaining = state.handle_key(ch)
                if remaining == '1':
                    pub.send_takeoff()
                elif remaining == '2':
                    pub.send_land()
                elif remaining == '3':
                    pub.send_arm()
                elif remaining == '4':
                    pub.send_disarm()
            except Exception:
                break

    key_thread = threading.Thread(target=key_reader, daemon=True)
    key_thread.start()

    interval = 1.0 / args.hz
    count = 0

    def sig_handler(sig, frame):
        state.running = False
    signal.signal(signal.SIGINT, sig_handler)

    try:
        while state.running:
            pub.publish_mc(state)
            pub.spin_once()
            count += 1
            if count % 5 == 0:  # ~5Hz 刷新 HUD
                print_hud(state, count)
            time.sleep(interval)
    except KeyboardInterrupt:
        pass

    print("\n\n退出。")
    pub.destroy()


if __name__ == "__main__":
    main()
