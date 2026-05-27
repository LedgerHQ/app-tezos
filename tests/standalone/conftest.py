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

import re
from pathlib import Path
from typing import Dict, Generator, List, Optional

import pytest
from ledgered.devices import Device
from ragger.conftest import configuration
from ragger.conftest.base_conftest import prepare_speculos_args
from ragger.error import MissingElfError
from ragger.navigator import Navigator, NanoNavigator, TouchNavigator

from utils.account import Account, DEFAULT_ACCOUNT, DEFAULT_SEED
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

def pytest_addoption(parser):
    """Register local pytest options on top of ragger defaults."""
    parser.addoption("--log-dir", type=Path, help="Log directory")

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

def _override_seed(args: List[str], seed: str) -> List[str]:  # pylint: disable=redefined-outer-name
    """Replace any existing --seed argument with the one provided."""
    result: List[str] = []
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


@pytest.fixture(scope="function")
def backend(  # pylint: disable=too-many-arguments,too-many-positional-arguments,too-many-locals
    skip_tests_for_unsupported_devices,
    root_pytest_dir: Path,
    device: Device,
    display: bool,
    pki_prod: bool,
    log_apdu_file: Optional[Path],
    cli_user_seed: Optional[str],
    additional_speculos_arguments: List[str],
    verbose_speculos: bool,
    ignore_missing_binaries: bool,
    seed: str,  # pylint: disable=redefined-outer-name
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

    backend_instance = SpeculosTezosBackend(  # pylint: disable=abstract-class-instantiated,unexpected-keyword-arg
        app_path,
        device,
        **speculos_kwargs,
    )
    with backend_instance as instance:
        yield instance

@pytest.fixture(scope="function")
def tezos_navigator(
        backend: TezosBackend,  # pylint: disable=redefined-outer-name
        device: Device,
        golden_run: bool
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
    test_file_snapshot_dir = Path(*test_file_path.parts[len(Path(__file__).parts)-1:-1])
    return test_file_snapshot_dir / file_name / test_name

def requires_device(device):
    """Wrapper to run the pytest test only with the provided device."""
    return pytest.mark.skipif(
        f"config.getvalue('device') != '{ device }'",
        reason=f"Test requires device to be { device }."
    )

@pytest.fixture(autouse=True)
def use_only_on_device(request, device: Device):
    """Fixture to add tests on specific devices."""

    def get_devices(dev: str) -> List[str]:
        if dev == "nano":
            return ["nanos", "nanosp", "nanox"]
        if dev == "touch":
            return ["stax", "flex", "apex_p"]
        return [dev]

    marker = request.node.get_closest_marker('use_on_device')
    if marker:
        current_device = device.name.lower()
        requested_devices = marker.args[0]
        devices: List[str] = []
        if isinstance(requested_devices, str):
            devices = get_devices(requested_devices)
        else:
            assert isinstance(requested_devices, list)
            for dev in requested_devices:
                devices += get_devices(dev)
        if current_device not in devices:
            pytest.skip(f'skipped on this device: "{current_device}"')


_log_dir_state: Dict[str, Optional[Path]] = {"value": None}


def _get_log_dir() -> Optional[Path]:
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

logs : Dict[str, List[pytest.TestReport]] = {}

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
            log_dir = Path(*log_dir.parts[log_dir.parts.index(test_root) + 1:])
        log_dir = global_log_dir / log_dir
        log_dir.mkdir(parents=True, exist_ok=True)
        head_line = location[2]
        log_file = log_dir / f"{head_line}.log"
        with open(log_file, 'w', encoding="utf-8") as writer:
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
