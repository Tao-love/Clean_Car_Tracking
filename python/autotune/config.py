# ----- AI
"""自动整定 JSON 配置加载；默认参数 base_speed=0，不会因启动 CLI 而动车。"""

from dataclasses import dataclass
import json
from pathlib import Path

from .models import ControlParams
from .scoring import ScoreConfig


@dataclass(frozen=True, slots=True)
class AppConfig:
    port: str | None
    baudrate: int
    params: ControlParams
    score: ScoreConfig
    vofa_enabled: bool
    vofa_host: str
    vofa_port: int

    @classmethod
    def load(cls, path: str | Path) -> "AppConfig":
        raw = json.loads(Path(path).read_text(encoding="utf-8"))
        params = ControlParams(**raw.get("params", {})) if raw.get("params") else ControlParams.safe_defaults()
        score = ScoreConfig(**raw.get("score", {})) if raw.get("score") else ScoreConfig.defaults()
        vofa = raw.get("vofa", {})
        return cls(
            port=raw.get("port"),
            baudrate=int(raw.get("baudrate", 9600)),
            params=params,
            score=score,
            vofa_enabled=bool(vofa.get("enabled", False)),
            vofa_host=str(vofa.get("host", "127.0.0.1")),
            vofa_port=int(vofa.get("port", 1347)),
        )
# ----- AI
