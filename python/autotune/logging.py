# ----- AI
"""原始帧、事件和试验汇总日志，只使用 Python 标准库。"""

import csv
from dataclasses import asdict
from datetime import datetime
import json
from pathlib import Path
import threading
import time
from typing import Any

from .models import ControlParams, TrialSummary
from .scoring import ScoreConfig, ScoreResult


class RunLogger:
    """为一次上位机运行创建独立时间戳目录。"""

    def __init__(self, root: str | Path = "logs") -> None:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        self.directory = Path(root) / stamp
        self.directory.mkdir(parents=True, exist_ok=False)
        self._lock = threading.Lock()
        self._raw = (self.directory / "raw_frames.jsonl").open(
            "a", encoding="utf-8", buffering=1
        )
        self._events = (self.directory / "events.jsonl").open(
            "a", encoding="utf-8", buffering=1
        )
        self._trial_path = self.directory / "trials.csv"

    def raw_callback(self, direction: str, data: bytes) -> None:
        record = {
            "monotonic": time.monotonic(),
            "wall_time": datetime.now().isoformat(timespec="milliseconds"),
            "direction": direction,
            "hex": data.hex(),
        }
        with self._lock:
            self._raw.write(json.dumps(record, ensure_ascii=False) + "\n")

    def event(self, name: str, **fields: Any) -> None:
        record = {
            "monotonic": time.monotonic(),
            "wall_time": datetime.now().isoformat(timespec="milliseconds"),
            "event": name,
            **fields,
        }
        with self._lock:
            self._events.write(json.dumps(record, ensure_ascii=False) + "\n")

    def trial(
        self,
        params: ControlParams,
        summary: TrialSummary,
        score: ScoreResult,
        score_config: ScoreConfig,
    ) -> None:
        row: dict[str, Any] = {}
        row.update({f"param_{key}": value for key, value in asdict(params).items()})
        row.update({f"summary_{key}": value for key, value in asdict(summary).items()})
        row.update({f"score_{key}": value for key, value in score.components.items()})
        row["score_total"] = score.total
        row["score_failed"] = score.failed
        row["score_config_json"] = json.dumps(
            score_config.to_dict(), ensure_ascii=False, sort_keys=True
        )
        with self._lock:
            exists = self._trial_path.exists()
            with self._trial_path.open("a", encoding="utf-8-sig", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=list(row))
                if not exists:
                    writer.writeheader()
                writer.writerow(row)

    def close(self) -> None:
        with self._lock:
            if not self._raw.closed:
                self._raw.close()
            if not self._events.closed:
                self._events.close()
# ----- AI
