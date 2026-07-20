# ----- AI
"""
VOFA+ FireWater 可选 UDP 转发。

Python 仍是蓝牙 COM 口的唯一所有者；VOFA+ 只监听本机 UDP 数据。
未启动 VOFA+、UDP 错误或格式错误都只增加 dropped_packets，绝不向试验编排层抛出。
"""

import socket
from collections.abc import Callable

from .messages import Telemetry


class VofaForwarder:
    """将遥测转为逗号分隔 FireWater 文本并发到本机 UDP。"""

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 1347,
        enabled: bool = False,
        socket_factory: Callable[[], object] | None = None,
    ) -> None:
        self.address = (host, port)
        self.enabled = enabled
        self.dropped_packets = 0
        self.sent_packets = 0
        factory = socket_factory or (
            lambda: socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        )
        self._socket = factory() if enabled else None

    def forward(self, telemetry: Telemetry) -> None:
        """尝试发送一个遥测样本；任何失败均在本函数内吸收。"""

        if not self.enabled or self._socket is None:
            return
        try:
            line = (
                f"{telemetry.tick},{telemetry.error},"
                f"{telemetry.left_target},{telemetry.right_target},"
                f"{telemetry.left_speed},{telemetry.right_speed},"
                f"{telemetry.left_pwm},{telemetry.right_pwm},"
                f"{telemetry.state},{telemetry.fault}\n"
            ).encode("ascii")
            self._socket.sendto(line, self.address)
            self.sent_packets += 1
        except Exception:
            self.dropped_packets += 1

    def close(self) -> None:
        if self._socket is not None:
            try:
                self._socket.close()
            except Exception:
                pass
            self._socket = None
# ----- AI
