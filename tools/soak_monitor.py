#!/usr/bin/env python3
"""Монитор длительного прогона прошивки (soak-тест).

Читает журнал контроллера из COM-порта или из файла, пишет сырой журнал и
CSV и сигналит о том, что мешает аптайму в месяцы:

  * перезагрузки и их причина (Reset reason, паника, WDT, brownout);
  * тренд «largest» и «heap» (утечка или фрагментация кучи);
  * малый запас стека у задач (stackWM/wdWM/tcpWM/loopWM);
  * слишком частая запись в NVS (износ флеша);
  * тишину в [HEALTH] (зависание) и потерю порта;
  * ошибки: FAILED, DEAD, FORCE OFF, Command queue FULL.

Примеры:
  py tools/soak_monitor.py --port COM3
  py tools/soak_monitor.py --file soak_logs/serial_2026-10-05.log

ВАЖНО: открытие COM-порта на многих платах ESP32 перезагружает контроллер
через DTR/RTS. Скрипт держит обе линии неактивными, но на части драйверов
Windows импульс при открытии всё равно проходит: в начале прогона одна
перезагрузка по порту нормальна, она попадёт в журнал как «EXT/POWERON».
"""

import argparse
import csv
import re
import sys
import time
from datetime import datetime
from pathlib import Path

HEALTH_RE = re.compile(
    r"\[HEALTH\]\s+heap=(\d+)\s+min=(\d+)\s+largest=(\d+)\s+stackWM=(\d+)"
    r"\s+nvsWr=(\d+)\s+uptime=(\d+)s"
    r"(?:\s+wdWM=(\d+)\s+tcpWM=(\d+)\s+loopWM=(\d+))?"
)
RESET_RE = re.compile(r"Reset reason:\s*([A-Z_]+)")
NORMAL_RESETS = {"POWERON", "EXT", "SW"}
CRASH_MARKERS = (
    "Guru Meditation", "Backtrace:", "abort()", "assert failed",
    "Task watchdog got triggered", "Brownout detector", "PREVIOUS RUN ENDED ABNORMALLY",
)
# Счётчики событий: подстрока -> имя в отчёте. Часть из них ещё и тревога.
EVENTS = {
    "Command queue FULL": "очередь команд форточек переполнена",
    "FORCE OFF": "аварийное отключение реле",
    "DEAD": "I2C-датчики в состоянии DEAD",
    "FAILED": "операция завершилась ошибкой (FAILED)",
    "Implausible reading": "датчик дождя: неправдоподобное показание",
    "Interrupted filesystem update": "обнаружена оборванная заливка FS",
    "Forced stop applied": "принудительная остановка форточек (OTA)",
}
ALERT_EVENTS = ("Command queue FULL", "DEAD", "FAILED")
CSV_HEADER = ["time", "run", "uptime_s", "heap", "min_heap", "largest",
              "stackWM", "nvsWr", "wdWM", "tcpWM", "loopWM"]


def slope_per_day(points):
    """Наклон МНК по точкам (uptime_s, значение), единиц в сутки."""
    n = len(points)
    if n < 2:
        return 0.0
    mx = sum(p[0] for p in points) / n
    my = sum(p[1] for p in points) / n
    den = sum((p[0] - mx) ** 2 for p in points)
    if den == 0:
        return 0.0
    num = sum((p[0] - mx) * (p[1] - my) for p in points)
    return num / den * 86400.0


class Run:
    """Один запуск контроллера от загрузки до следующей перезагрузки."""

    def __init__(self, number, started):
        self.number = number
        self.started = started
        self.samples = []          # (uptime_s, heap, min_heap, largest, nvs)
        self.stack_min = {}        # имя -> минимум ненулевых значений
        self.reset_reason = "?"
        self.events = {k: 0 for k in EVENTS}

    def add(self, up, heap, min_heap, largest, nvs, stacks):
        self.samples.append((up, heap, min_heap, largest, nvs))
        for name, value in stacks.items():
            if value:
                self.stack_min[name] = min(value, self.stack_min.get(name, value))


class Monitor:
    def __init__(self, args, out_dir, raw_log=True):
        self.a = args
        self.out = out_dir
        self.raw_log = raw_log
        self.runs = [Run(1, datetime.now())]
        self.last_health = time.monotonic()
        self.alert_times = {}
        self.alerts = []
        self.csv_file = None
        self.csv_writer = None
        self._open_csv()

    # ── файлы ───────────────────────────────────────────────────────
    def _open_csv(self):
        path = self.out / "health.csv"
        new = not path.exists()
        self.csv_file = open(path, "a", newline="", encoding="utf-8")
        self.csv_writer = csv.writer(self.csv_file)
        if new:
            self.csv_writer.writerow(CSV_HEADER)

    def log_raw(self, line):
        day = datetime.now().strftime("%Y-%m-%d")
        stamp = datetime.now().strftime("%H:%M:%S")
        with open(self.out / f"serial_{day}.log", "a", encoding="utf-8") as f:
            f.write(f"{stamp} {line}\n")

    def alert(self, key, text, critical=False):
        """Тревога с подавлением повторов: один и тот же ключ — не чаще окна."""
        now = time.monotonic()
        if now - self.alert_times.get(key, -1e18) < self.a.realert_hours * 3600:
            return
        self.alert_times[key] = now
        mark = "!!! КРИТИЧНО" if critical else "!!  ВНИМАНИЕ"
        msg = f"{datetime.now():%Y-%m-%d %H:%M:%S} {mark}: {text}"
        self.alerts.append(msg)
        print(msg, flush=True)
        with open(self.out / "alerts.log", "a", encoding="utf-8") as f:
            f.write(msg + "\n")

    # ── разбор строк ────────────────────────────────────────────────
    @property
    def run(self):
        return self.runs[-1]

    def new_run(self, why):
        if self.run.samples or self.run.reset_reason != "?":
            self.runs.append(Run(len(self.runs) + 1, datetime.now()))
        print(f"{datetime.now():%Y-%m-%d %H:%M:%S} ---- новый запуск #{self.run.number}: {why}",
              flush=True)

    def feed(self, line):
        line = line.rstrip("\r\n")
        if not line:
            return
        self.log_raw(line)

        m = RESET_RE.search(line)
        if m:
            self.new_run("загрузка контроллера")
            reason = m.group(1)
            self.run.reset_reason = reason
            if reason not in NORMAL_RESETS:
                self.alert(f"reset-{reason}-{self.run.number}",
                           f"аварийная перезагрузка: {reason}", critical=True)
            elif len(self.runs) > 1:
                self.alert(f"reboot-{self.run.number}",
                           f"контроллер перезагрузился ({reason}); аптайм прерван")
        for marker in CRASH_MARKERS:
            if marker in line:
                self.alert(f"crash-{marker}-{self.run.number}",
                           f"в журнале: «{marker}»", critical=True)

        for needle, title in EVENTS.items():
            if needle in line:
                self.run.events[needle] += 1
                if needle in ALERT_EVENTS:
                    self.alert(f"event-{needle}", f"{title}: {line.strip()[:120]}")

        h = HEALTH_RE.match(line.strip())
        if h:
            self.on_health(h)

    def on_health(self, h):
        heap, min_heap, largest, stack, nvs, up = (int(h.group(i)) for i in range(1, 7))
        stacks = {"taskCore1": stack}
        if h.group(7) is not None:
            stacks.update({"WiFiWD": int(h.group(7)), "async_tcp": int(h.group(8)),
                           "loopTask": int(h.group(9))})
        self.last_health = time.monotonic()

        prev = self.run.samples[-1][0] if self.run.samples else None
        if prev is not None and up + 5 < prev:
            self.new_run(f"uptime упал {prev} -> {up} с (перезагрузка без строки Reset reason)")
            self.alert(f"uptime-drop-{self.run.number}", "uptime уменьшился: была перезагрузка",
                       critical=True)
        self.run.add(up, heap, min_heap, largest, nvs, stacks)

        self.csv_writer.writerow([datetime.now().isoformat(timespec="seconds"), self.run.number,
                                  up, heap, min_heap, largest, stack,
                                  nvs] + [stacks.get(k, "") for k in ("WiFiWD", "async_tcp", "loopTask")])
        self.csv_file.flush()
        self.check_run()

    # ── проверки ────────────────────────────────────────────────────
    def check_run(self):
        run, a = self.run, self.a
        for name, value in run.stack_min.items():
            if value < a.stack_min:
                self.alert(f"stack-{name}-{run.number}",
                           f"запас стека задачи {name} всего {value} Б (порог {a.stack_min})",
                           critical=value < a.stack_min // 2)

        up_now = run.samples[-1][0]
        window = [s for s in run.samples if s[0] >= up_now - a.window_hours * 3600]
        span_h = (window[-1][0] - window[0][0]) / 3600.0
        if len(window) < 30 or span_h < a.min_trend_hours:
            return                       # пока мало данных: куча после старта ещё «дышит»

        for idx, name, limit in ((3, "largest (крупнейший свободный блок)", a.largest_drop_per_day),
                                 (1, "heap (свободная куча)", a.heap_drop_per_day)):
            slope = slope_per_day([(s[0], s[idx]) for s in window])
            if slope < -limit:
                self.alert(f"trend-{idx}-{run.number}",
                           f"{name} падает на {-slope:.0f} Б/сутки (порог {limit}) — "
                           "утечка или фрагментация")
        rate = (window[-1][4] - window[0][4]) / max(span_h, 1e-9) * 24.0
        if rate > a.nvs_per_day:
            self.alert(f"nvs-{run.number}",
                       f"запись настроек в NVS: {rate:.0f} раз/сутки (порог {a.nvs_per_day}) — износ флеша")

    def check_silence(self):
        quiet = time.monotonic() - self.last_health
        if quiet > self.a.silence_sec:
            self.alert("silence", f"нет строки [HEALTH] уже {quiet:.0f} с — возможно, зависание",
                       critical=True)

    # ── отчёт ───────────────────────────────────────────────────────
    def report(self):
        lines = [f"=== Отчёт soak-монитора, {datetime.now():%Y-%m-%d %H:%M:%S} ===",
                 f"запусков контроллера: {len(self.runs)}, тревог: {len(self.alerts)}"]
        for r in self.runs:
            if not r.samples:
                lines.append(f"\n#{r.number} (с {r.started:%Y-%m-%d %H:%M}): данных [HEALTH] нет, "
                             f"reset={r.reset_reason}")
                continue
            first, last = r.samples[0], r.samples[-1]
            days = (last[0] - first[0]) / 86400.0
            lines += [
                f"\n#{r.number} с {r.started:%Y-%m-%d %H:%M}, reset={r.reset_reason}",
                f"  аптайм к последнему замеру: {last[0] / 86400:.2f} сут, замеров: {len(r.samples)}",
                f"  heap: сейчас {last[1]}, минимум за запуск {min(s[2] for s in r.samples)}",
                f"  largest: первый {first[3]} -> последний {last[3]}"
                + (f", тренд {slope_per_day([(s[0], s[3]) for s in r.samples]):+.0f} Б/сут"
                   if days >= 0.25 else " (мало данных для тренда)"),
                "  запас стека (минимум): "
                + ", ".join(f"{k}={v}" for k, v in sorted(r.stack_min.items())),
                f"  записей в NVS: {last[4] - first[4]}"
                + (f" ({(last[4] - first[4]) / days:.0f}/сут)" if days >= 0.25 else ""),
            ]
            events = [f"{EVENTS[k]}: {v}" for k, v in r.events.items() if v]
            if events:
                lines.append("  события: " + "; ".join(events))
        text = "\n".join(lines)
        print(text, flush=True)
        (self.out / "report.txt").write_text(text + "\n", encoding="utf-8")


def read_file(mon, path):
    src = sys.stdin if path == "-" else open(path, encoding="utf-8", errors="replace")
    with src:
        for line in src:
            mon.feed(line)


def read_serial(mon, port, baud, status_min):
    import serial   # pyserial; импорт здесь, чтобы разбор файла работал и без него

    last_status = time.monotonic()
    while True:
        ser = serial.Serial()
        ser.port, ser.baudrate, ser.timeout = port, baud, 1
        ser.dtr = False          # линии не дёргаем: иначе плата перезагрузится
        ser.rts = False
        try:
            ser.open()
            print(f"{datetime.now():%H:%M:%S} порт {port} открыт, {baud} бод", flush=True)
            while True:
                raw = ser.readline()
                if raw:
                    mon.feed(raw.decode("utf-8", errors="replace"))
                mon.check_silence()
                if time.monotonic() - last_status > status_min * 60:
                    last_status = time.monotonic()
                    r = mon.run
                    if r.samples:
                        s = r.samples[-1]
                        print(f"{datetime.now():%Y-%m-%d %H:%M} жив: uptime {s[0] / 86400:.2f} сут, "
                              f"heap {s[1]}, largest {s[3]}, тревог {len(mon.alerts)}", flush=True)
        except (serial.SerialException, OSError) as exc:
            mon.alert("port-lost", f"порт {port} недоступен: {exc}", critical=True)
            time.sleep(5)
        finally:
            ser.close()


def main():
    p = argparse.ArgumentParser(description="Монитор длительного прогона прошивки")
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", help="COM-порт, например COM3")
    src.add_argument("--file", help="файл журнала для разбора ('-' — stdin)")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--out", default="soak_logs", help="каталог журналов (по умолчанию soak_logs)")
    p.add_argument("--stack-min", type=int, default=512, help="тревога, если запас стека меньше, Б")
    p.add_argument("--largest-drop-per-day", type=int, default=4096)
    p.add_argument("--heap-drop-per-day", type=int, default=4096)
    p.add_argument("--nvs-per-day", type=int, default=500)
    p.add_argument("--window-hours", type=float, default=24.0, help="окно для трендов")
    p.add_argument("--min-trend-hours", type=float, default=6.0,
                   help="не оценивать тренд, пока данных меньше")
    p.add_argument("--silence-sec", type=int, default=180, help="тревога при тишине [HEALTH]")
    p.add_argument("--realert-hours", type=float, default=6.0, help="повтор одной и той же тревоги")
    p.add_argument("--status-min", type=int, default=30)
    args = p.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    mon = Monitor(args, out, raw_log=not args.file)   # при разборе файла копию журнала не пишем
    try:
        if args.file:
            read_file(mon, args.file)
        else:
            read_serial(mon, args.port, args.baud, args.status_min)
    except KeyboardInterrupt:
        print("\nостановлено пользователем", flush=True)
    finally:
        mon.report()
        mon.csv_file.close()


if __name__ == "__main__":
    main()
