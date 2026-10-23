# Copyright 2024 Trilitech <contact@trili.tech>
# Copyright 2024 Functori <contact@functori.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Conftest based on ragger's base conftest plus Tezos fixtures."""

import atexit
import os
import re
import threading
import time
import uuid
from collections.abc import Generator
from pathlib import Path

import pytest
from ledgered.devices import Device
from ragger.conftest import configuration
from ragger.conftest.base_conftest import prepare_speculos_args
from ragger.error import MissingElfError
from ragger.navigator import NanoNavigator, Navigator, TouchNavigator
from utils.account import DEFAULT_ACCOUNT, DEFAULT_SEED, Account
from utils.backend import SpeculosTezosBackend, TezosBackend
from utils.navigator import TezosNavigator

# Keep Tezos tests deterministic by default.
configuration.OPTIONAL.CUSTOM_SEED = DEFAULT_SEED

# Pull all standard Ragger fixtures/options.
pytest_plugins = ("ragger.conftest.base_conftest",)


def _sanitize_node_name(name: str) -> str:
    """Replace filesystem-unsafe characters in a pytest node name."""
    name = name.replace("[", "__").replace("]", "__")
    name = re.sub(r"_+", "_", name).strip("_")
    return name


def _xdist_worker_index() -> int | None:
    """Index of this pytest-xdist worker (gw3 -> 3), or None outside a worker."""
    match = re.fullmatch(r"gw(\d+)", os.environ.get("PYTEST_XDIST_WORKER", ""))
    return int(match.group(1)) if match else None


# Upper bound on the workers `-n auto` starts (override: SPECULOS_MAX_WORKERS).
DEFAULT_MAX_SPECULOS_WORKERS = 16

# Runs of this suite that share the repository directory (one container per target) record themselves here, so each
# can split the cores between the runs that are alive right now.
PARALLEL_RUNS_DIR = Path(__file__).parent / ".parallel-runs"
_RUN_HEARTBEAT_SECONDS = 5.0
_RUN_STALE_AFTER_SECONDS = 15.0
# Time to let simultaneously launched runs register before counting them.
_RUN_REGISTRATION_GRACE_SECONDS = 3.0

_run_state: dict[str, Path | None] = {"file": None}


def _unregister_run() -> None:
    """Remove this run's file from the shared directory."""
    run_file, _run_state["file"] = _run_state["file"], None
    if run_file is not None:
        run_file.unlink(missing_ok=True)


def _register_run(device: str) -> int:
    """Record this run in the shared directory and return how many runs are alive, itself included.

    A run is alive while its file is touched; a crashed run goes stale and stops being counted.
    """
    PARALLEL_RUNS_DIR.mkdir(exist_ok=True)
    run_file = PARALLEL_RUNS_DIR / f"{device}-{uuid.uuid4().hex}.run"
    run_file.touch()
    _run_state["file"] = run_file

    def heartbeat() -> None:
        while _run_state["file"] is run_file:
            time.sleep(_RUN_HEARTBEAT_SECONDS)
            try:
                run_file.touch()
            except OSError:
                return

    threading.Thread(target=heartbeat, daemon=True).start()
    atexit.register(_unregister_run)

    time.sleep(_RUN_REGISTRATION_GRACE_SECONDS)
    alive = 0
    for other in PARALLEL_RUNS_DIR.glob("*.run"):
        try:
            if time.time() - other.stat().st_mtime < _RUN_STALE_AFTER_SECONDS:
                alive += 1
        except FileNotFoundError:
            continue
    return max(1, alive)


@pytest.hookimpl(optionalhook=True)
def pytest_xdist_auto_num_workers(config) -> int | None:
    """Worker count for `-n auto`: the usable cores divided by the runs alive now, capped.

    Every per-target container sees all the cores of the host, so `-n auto` in each of five containers would start five
    times too many Speculos instances, and the UI waits become timing-sensitive. The runs sharing this directory count
    each other (see _register_run), so a lone run gets all the cores and five concurrent ones get a fifth each. The
    usable cores are the CPU affinity mask, which follows a container's --cpuset-cpus (os.cpu_count() would count the
    host's).

    Runs launched more than a few seconds apart do not see each other at start-up: the earlier ones take more than
    their share. SPECULOS_PARALLEL_TARGETS fixes the divisor instead of counting, SPECULOS_MAX_WORKERS caps the result,
    and an explicit PYTEST_XDIST_AUTO_NUM_WORKERS overrides everything (xdist applies it).
    """
    if "PYTEST_XDIST_AUTO_NUM_WORKERS" in os.environ:
        return None
    cap = int(os.environ.get("SPECULOS_MAX_WORKERS", DEFAULT_MAX_SPECULOS_WORKERS))
    cores = len(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else os.cpu_count() or 1
    if "SPECULOS_PARALLEL_TARGETS" in os.environ:
        runs = int(os.environ["SPECULOS_PARALLEL_TARGETS"])
    else:
        runs = _register_run(str(config.getoption("device", default="run")))
    return max(1, min(cap, cores // max(1, runs)))


def pytest_addoption(parser):
    """Register local pytest options on top of ragger defaults."""
    parser.addoption("--log-dir", type=Path, help="Log directory")
    # Under pytest-xdist each worker gets its own Speculos ports: API port base + index * stride, APDU port one above.
    parser.addoption(
        "--speculos-base-port",
        type=int,
        default=int(os.environ.get("SPECULOS_BASE_PORT", "5000")),
        help="First Speculos API port under pytest-xdist (env: SPECULOS_BASE_PORT)",
    )
    parser.addoption(
        "--speculos-port-stride",
        type=int,
        default=int(os.environ.get("SPECULOS_PORT_STRIDE", "10")),
        help="Ports reserved per pytest-xdist worker, at least 2 (env: SPECULOS_PORT_STRIDE)",
    )


@pytest.fixture(scope="function")
def seed(request) -> str:
    """Get `seed` for pytest."""
    param = getattr(request, "param", None)
    return param.get("seed", DEFAULT_SEED) if param else DEFAULT_SEED


@pytest.fixture(scope="function")
def account(request) -> Account:
    """Get `account` for pytest."""
    param = getattr(request, "param", None)
    return param.get("account", DEFAULT_ACCOUNT) if param else DEFAULT_ACCOUNT


def _override_seed(args: list[str], seed: str) -> list[str]:
    """Replace any existing --seed argument with the one provided."""
    result: list[str] = []
    i = 0
    while i < len(args):
        if args[i] == "--seed":
            i += 1
            if i < len(args):
                i += 1
            continue
        result.append(args[i])
        i += 1
    result.extend(["--seed", seed])
    return result


@pytest.fixture(scope=configuration.OPTIONAL.BACKEND_SCOPE)  # type: ignore[call-overload]  # ragger types the scope as str
def additional_speculos_arguments(request) -> list[str]:
    """Distinct Speculos ports per xdist worker; ragger's own choice outside xdist.

    Ragger otherwise picks the first free port, and workers starting together can all see the same one as free.
    """
    index = _xdist_worker_index()
    if index is None:
        return []
    stride = request.config.getoption("--speculos-port-stride")
    if stride < 2:
        raise pytest.UsageError("--speculos-port-stride must be at least 2 (API + APDU port)")
    api_port = request.config.getoption("--speculos-base-port") + index * stride
    return ["--api-port", str(api_port), "--apdu-port", str(api_port + 1)]


@pytest.fixture(scope="function")
def backend(
    skip_tests_for_unsupported_devices,
    root_pytest_dir: Path,
    device: Device,
    display: bool,
    pki_prod: bool,
    log_apdu_file: Path | None,
    cli_user_seed: str | None,
    additional_speculos_arguments: list[str],
    verbose_speculos: bool,
    ignore_missing_binaries: bool,
    seed: str,
) -> Generator[TezosBackend, None, None]:
    """Provide Tezos-specific backend while reusing ragger discovery logic."""
    _ = skip_tests_for_unsupported_devices
    try:
        app_path, speculos_kwargs = prepare_speculos_args(
            root_pytest_dir=root_pytest_dir,
            device=device,
            display=display,
            pki_prod=pki_prod,
            cli_user_seed=cli_user_seed or "",
            additional_args=additional_speculos_arguments,
            verbose_speculos=verbose_speculos,
            ignore_missing_binaries=ignore_missing_binaries,
        )
    except MissingElfError as exc:
        pytest.fail(f"Missing ELF: {exc}")

    args = speculos_kwargs.pop("args", [])
    args = _override_seed(args, seed)
    speculos_kwargs["args"] = args
    speculos_kwargs["log_apdu_file"] = log_apdu_file

    backend_instance = SpeculosTezosBackend(
        app_path,
        device,
        **speculos_kwargs,
    )
    with backend_instance as instance:
        yield instance


@pytest.fixture(scope="function")
def tezos_navigator(
    backend: TezosBackend,
    device: Device,
    golden_run: bool,
) -> TezosNavigator:
    """Get `navigator` for pytest."""
    if device.is_nano:
        navigator: Navigator = NanoNavigator(backend, device, golden_run)
    else:
        navigator = TouchNavigator(backend, device, golden_run)
    return TezosNavigator(backend, device, navigator)


@pytest.fixture(scope="function")
def snapshot_dir(request) -> Path:
    """Get the test snapshot location."""
    test_file_path = Path(request.fspath)
    file_name = test_file_path.stem
    test_name = _sanitize_node_name(request.node.name)
    # Get test directory from the root
    test_file_snapshot_dir = Path(*test_file_path.parts[len(Path(__file__).parts) - 1 : -1])
    return test_file_snapshot_dir / file_name / test_name


def requires_device(device):
    """Wrapper to run the pytest test only with the provided device."""
    return pytest.mark.skipif(
        f"config.getvalue('device') != '{device}'",
        reason=f"Test requires device to be {device}.",
    )


@pytest.fixture(autouse=True)
def use_only_on_device(request, device: Device):
    """Fixture to add tests on specific devices."""

    def get_devices(dev: str) -> list[str]:
        if dev == "nano":
            return ["nanos", "nanosp", "nanox"]
        if dev == "touch":
            return ["stax", "flex", "apex_p"]
        return [dev]

    marker = request.node.get_closest_marker("use_on_device")
    if marker:
        current_device = device.name.lower()
        requested_devices = marker.args[0]
        devices: list[str] = []
        if isinstance(requested_devices, str):
            devices = get_devices(requested_devices)
        else:
            assert isinstance(requested_devices, list)
            for dev in requested_devices:
                devices += get_devices(dev)
        if current_device not in devices:
            pytest.skip(f'skipped on this device: "{current_device}"')


_log_dir_state: dict[str, Path | None] = {"value": None}


def _get_log_dir() -> Path | None:
    """Return the current global log directory."""
    return _log_dir_state["value"]


def pytest_configure(config):
    """Configure pytest."""
    # Add marker
    config.addinivalue_line(
        "markers",
        "use_on_device(devices): skip test if not on one of the specified devices",
    )

    # Setup log directory
    log_dir = config.getoption("log_dir")
    if log_dir is not None:
        _log_dir_state["value"] = Path(log_dir)


def pytest_sessionfinish(session):
    """Leave the shared run directory as soon as the session is over, so other runs stop counting this one."""
    _ = session
    _unregister_run()


logs: dict[str, list[pytest.TestReport]] = {}


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_logstart(location):
    """Called at the start of running the runtest protocol for a single item."""
    logs[location[2]] = []


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_logreport(report):
    """Called at the end of running the runtest protocol for a single test."""
    logs[report.head_line].append(report)


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_logfinish(nodeid, location):
    """Called at the end of running the runtest protocol for a single item."""
    global_log_dir = _get_log_dir()
    if global_log_dir is not None:
        log_dir = Path(nodeid.split(".py")[0])
        # Remove `tests/standalone/`
        test_root = "standalone"
        if test_root in log_dir.parts:
            log_dir = Path(*log_dir.parts[log_dir.parts.index(test_root) + 1 :])
        log_dir = global_log_dir / log_dir
        log_dir.mkdir(parents=True, exist_ok=True)
        head_line = location[2]
        log_file = log_dir / f"{head_line}.log"
        with open(log_file, "w", encoding="utf-8") as writer:
            for report in logs[head_line]:
                when = report.when.capitalize()
                outcome = report.outcome
                sep = "=" * 30
                writer.write(f"{sep} {when} {outcome} {sep}\n")
                writer.write(f"{report.longreprtext}\n")
                for section in report.sections:
                    if section[0].endswith(report.when):
                        sep2 = "-" * 30
                        writer.write(f"{sep2} {section[0]} {sep2}\n")
                        writer.write(f"{section[1]}\n")
                        writer.write("\n")
                writer.write("\n")
