# ----- AI
"""
高层设备会话与心跳。

HELLO 成功前不允许任何带 Session ID 的状态命令。
心跳默认每 100 ms 发送；心跳链路失败会被锁存并让后续操作显式失败。
"""

import threading

from .link import SerialLink
from .messages import Ack, Status
from .models import ControlParams, TrialSummary
from .protocol import Frame, HelloAck, MessageType, decode_hello_ack


class AutotuneSession:
    """一次 MCU 上电 Session 的命令 API。"""

    def __init__(self, link: SerialLink, heartbeat_interval: float = 0.1) -> None:
        if heartbeat_interval <= 0:
            raise ValueError("heartbeat_interval 必须大于 0")
        self.link = link
        self.heartbeat_interval = heartbeat_interval
        self.hello_ack: HelloAck | None = None
        self._stop_heartbeat = threading.Event()
        self._heartbeat_thread: threading.Thread | None = None
        self._heartbeat_error: BaseException | None = None
        self._closed = False

    @property
    def session_id(self) -> int:
        if self.hello_ack is None:
            raise RuntimeError("必须先完成 HELLO")
        return self.hello_ack.session_id

    def hello(self, start_heartbeat: bool = True) -> HelloAck:
        """交换协议/固件/能力/Session ID，成功后可启动 100 ms 心跳。"""

        frame = self.link.request(MessageType.HELLO, b"", timeout=0.4, retries=2)
        hello = decode_hello_ack(frame.payload)
        if hello.protocol_version != 1 or hello.status != 0:
            raise RuntimeError(
                f"不兼容 HELLO: protocol={hello.protocol_version} status={hello.status}"
            )
        self.hello_ack = hello
        if start_heartbeat:
            self._start_heartbeat()
        return hello

    def set_params(self, params: ControlParams) -> int:
        """下发候选并等待 MCU 在 10 ms 边界原子应用后的版本 ACK。"""

        ack = self._request_ack(
            MessageType.SET_PARAMS,
            self._session_bytes() + params.to_wire(),
            timeout=0.5,
        )
        return ack.param_version

    def get_status(self) -> Status:
        """读取 MCU 状态、故障、参数版本和最近停止原因。"""

        self._check_heartbeat()
        frame = self.link.request(
            MessageType.GET_STATUS,
            self._session_bytes(),
            expected_types=(MessageType.STATUS, MessageType.NACK),
            timeout=0.3,
            retries=2,
        )
        status = Status.from_wire(frame.payload)
        if status.session_id != self.session_id:
            raise RuntimeError("STATUS Session ID 不匹配")
        return status

    def arm(self, param_version: int) -> None:
        self._request_ack(
            MessageType.ARM,
            self._session_bytes() + int(param_version).to_bytes(2, "little"),
        )

    def start_trial(
        self, mode: int = 0, left_command: int = 0, right_command: int = 0
    ) -> None:
        """启动受限循线(0)/速度阶跃(1)/开环 PWM(2) 试验。"""

        if mode not in (0, 1, 2):
            raise ValueError("trial mode 必须为 0、1 或 2")
        command = bytes((mode,))
        command += int(left_command).to_bytes(2, "little", signed=True)
        command += int(right_command).to_bytes(2, "little", signed=True)
        self._request_ack(
            MessageType.START_TRIAL, self._session_bytes() + command
        )

    def abort_trial(self) -> None:
        self._request_ack(MessageType.ABORT_TRIAL, self._session_bytes())

    def clear_fault(self) -> None:
        self._request_ack(MessageType.CLEAR_FAULT, self._session_bytes())

    def set_telemetry(self, enabled: bool, frequency_hz: int = 10) -> None:
        if not 0 <= frequency_hz <= 20:
            raise ValueError("frequency_hz 必须为 0..20")
        self._request_ack(
            MessageType.SET_TELEMETRY,
            self._session_bytes() + bytes((int(enabled), frequency_hz)),
        )

    def commit_params(self, param_version: int) -> None:
        self._request_ack(
            MessageType.COMMIT_PARAMS,
            self._session_bytes() + int(param_version).to_bytes(2, "little"),
            timeout=1.5,
        )

    def wait_trial_summary(self, timeout: float = 6.5) -> TrialSummary:
        self._check_heartbeat()
        frame = self.link.wait_for_type(MessageType.TRIAL_SUMMARY, timeout)
        return TrialSummary.from_wire(frame.payload)

    def close(self) -> None:
        """先停心跳再关闭 COM，可重复调用。"""

        if self._closed:
            return
        self._closed = True
        self._stop_heartbeat.set()
        if self._heartbeat_thread is not None:
            self._heartbeat_thread.join(timeout=self.heartbeat_interval + 0.5)
        self.link.close()

    def _request_ack(
        self,
        message_type: int,
        payload: bytes,
        timeout: float = 0.3,
    ) -> Ack:
        self._check_heartbeat()
        frame = self.link.request(
            message_type, payload, timeout=timeout, retries=2
        )
        ack = Ack.from_wire(frame.payload)
        if int(ack.acknowledged_type) != int(message_type):
            raise RuntimeError("ACK 的命令类型不匹配")
        if ack.status != 0 or ack.session_id != self.session_id:
            raise RuntimeError(
                f"ACK 无效 status={ack.status} session=0x{ack.session_id:08X}"
            )
        return ack

    def _session_bytes(self) -> bytes:
        return self.session_id.to_bytes(4, "little")

    def _start_heartbeat(self) -> None:
        if self._heartbeat_thread is not None:
            return
        self._heartbeat_thread = threading.Thread(
            target=self._heartbeat_loop,
            name="autotune-heartbeat",
            daemon=True,
        )
        self._heartbeat_thread.start()

    def _heartbeat_loop(self) -> None:
        while not self._stop_heartbeat.wait(self.heartbeat_interval):
            try:
                frame = self.link.request(
                    MessageType.HEARTBEAT,
                    self._session_bytes(),
                    timeout=min(0.08, self.heartbeat_interval),
                    retries=0,
                )
                ack = Ack.from_wire(frame.payload)
                if ack.status != 0 or ack.session_id != self.session_id:
                    raise RuntimeError("心跳 ACK 不匹配")
            except BaseException as error:
                self._heartbeat_error = error
                return

    def _check_heartbeat(self) -> None:
        if self._heartbeat_error is not None:
            raise RuntimeError("心跳链路已失败，自动试验已暂停") from self._heartbeat_error
# ----- AI
