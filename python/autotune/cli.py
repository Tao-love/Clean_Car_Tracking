# ----- AI
"""安全命令行入口；只有显式 trial/identify/tune/climb 子命令才可能启动电机。"""

import argparse
from contextlib import contextmanager
from dataclasses import asdict
import json
from pathlib import Path
from typing import Iterator

from .config import AppConfig
from .link import SerialLink
from .logging import RunLogger
from .messages import Telemetry
from .protocol import Frame, MessageType
from .session import AutotuneSession
from .stages import AutotuneStages
from .trial import TrialRunner
from .vofa import VofaForwarder


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="basic-1 无线自动整定")
    parser.add_argument("--config", default="python/config.example.json")
    parser.add_argument("--port", help="覆盖配置中的 COM 口，例如 COM7")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("ports", help="只列出 COM 口，不打开也不启动电机")
    sub.add_parser("hello", help="只验证链路和版本")
    sub.add_parser("status", help="读状态")
    sub.add_parser("abort", help="人工提前停止当前试验")
    sub.add_parser("clear", help="条件恢复后清故障，不重启")

    trial = sub.add_parser("trial", help="执行一次 MCU 本地最多 5 秒试验")
    trial.add_argument("--mode", choices=("line", "speed", "pwm"), default="line")
    trial.add_argument("--left", type=int, default=0)
    trial.add_argument("--right", type=int, default=0)

    identify = sub.add_parser("identify", help="车轮悬空的开环 PWM 辨识")
    identify.add_argument("--levels", type=int, nargs="+", required=True)

    speed = sub.add_parser("tune-speed", help="车轮悬空的速度 PI 坐标搜索")
    speed.add_argument("--target", type=int, required=True)
    speed.add_argument("--step-q16", type=int, required=True)
    speed.add_argument("--max-q16", type=int, required=True)
    speed.add_argument("--rounds", type=int, default=2)

    line = sub.add_parser("tune-line", help="低速赛道循线 PD 坐标搜索")
    line.add_argument("--kp-step-q16", type=int, required=True)
    line.add_argument("--kd-step-q16", type=int, required=True)
    line.add_argument("--max-q16", type=int, required=True)
    line.add_argument("--rounds", type=int, default=2)

    climb = sub.add_parser("climb", help="从已验证参数逐级提高 base_speed")
    climb.add_argument("--speeds", type=int, nargs="+", required=True)

    commit = sub.add_parser("commit", help="仅在 IDLE 显式提交当前参数版本到 Flash")
    commit.add_argument("--version", type=int, required=True)
    return parser


@contextmanager
def open_runtime(config: AppConfig, port_override: str | None) -> Iterator[tuple[AutotuneSession, TrialRunner, RunLogger]]:
    port = port_override or config.port
    if not port:
        raise RuntimeError("未配置 COM 口：请使用 --port COMx")
    logger = RunLogger()
    vofa = VofaForwarder(
        config.vofa_host, config.vofa_port, config.vofa_enabled
    )

    def on_frame(frame: Frame) -> None:
        if int(frame.message_type) == int(MessageType.TELEMETRY):
            try:
                vofa.forward(Telemetry.from_wire(frame.payload))
            except ValueError as error:
                logger.event("telemetry_decode_error", error=str(error))

    link = SerialLink.open(
        port, config.baudrate, raw_callback=logger.raw_callback,
        frame_callback=on_frame,
    )
    session = AutotuneSession(link)
    runner = TrialRunner(session, config.score)
    try:
        hello = session.hello(start_heartbeat=True)
        logger.event("hello", **asdict(hello))
        yield session, runner, logger
    finally:
        session.close()
        vofa.close()
        logger.close()


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "ports":
        for device, description in SerialLink.list_ports():
            print(f"{device}\t{description}")
        return 0

    config = AppConfig.load(Path(args.config))
    with open_runtime(config, args.port) as (session, runner, logger):
        if args.command == "hello":
            print(json.dumps(asdict(session.hello_ack), ensure_ascii=False))
        elif args.command == "status":
            print(json.dumps(asdict(session.get_status()), ensure_ascii=False))
        elif args.command == "abort":
            session.abort_trial()
            print("已发送 ABORT_TRIAL 并收到匹配 ACK")
        elif args.command == "clear":
            session.clear_fault()
            print("故障已清除，电机保持停止")
        elif args.command == "commit":
            session.commit_params(args.version)
            print(f"参数版本 {args.version} 已显式提交")
        elif args.command == "trial":
            modes = {"line": 0, "speed": 1, "pwm": 2}
            result = runner.run_candidate(
                config.params, mode=modes[args.mode],
                left_command=args.left, right_command=args.right,
            )
            logger.trial(result.params, result.summary, result.score, config.score)
            print(json.dumps({"summary": asdict(result.summary), "score": asdict(result.score)}, ensure_ascii=False))
        else:
            stages = AutotuneStages(runner)
            if args.command == "identify":
                print(json.dumps([asdict(item) for item in stages.identify_motors(config.params, args.levels)], ensure_ascii=False))
            elif args.command == "tune-speed":
                fields = {
                    "speed_kp_left_q16": (0, args.max_q16, args.step_q16),
                    "speed_ki_left_q16": (0, args.max_q16, args.step_q16),
                    "speed_kp_right_q16": (0, args.max_q16, args.step_q16),
                    "speed_ki_right_q16": (0, args.max_q16, args.step_q16),
                }
                print(json.dumps(asdict(stages.tune_speed_pi(config.params, args.target, fields, args.rounds)), ensure_ascii=False))
            elif args.command == "tune-line":
                fields = {
                    "line_kp_q16": (0, args.max_q16, args.kp_step_q16),
                    "line_kd_q16": (0, args.max_q16, args.kd_step_q16),
                }
                print(json.dumps(asdict(stages.tune_line_pd(config.params, fields, args.rounds)), ensure_ascii=False))
            elif args.command == "climb":
                results = stages.climb_speed(config.params, args.speeds)
                print(json.dumps([{"params": asdict(item.params), "score": asdict(item.score)} for item in results], ensure_ascii=False))
    return 0
# ----- AI
