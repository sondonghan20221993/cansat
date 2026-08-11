"""pytest 공통 설정 및 fixture."""

import os
import shutil
import subprocess
import time
import pytest


def pytest_configure(config):
    config.addinivalue_line(
        "markers",
        "cfs_required: cFS 실행 환경이 필요한 통합테스트 (B 그룹). "
        "--cfs 옵션 없이는 skip.",
    )


def pytest_addoption(parser):
    parser.addoption(
        "--cfs",
        action="store_true",
        default=False,
        help="cFS 실행이 필요한 B 그룹 테스트 포함",
    )
    parser.addoption(
        "--cfs-host",
        default="127.0.0.1",
        help="cFS UDP 호스트 (기본값: 127.0.0.1)",
    )
    parser.addoption(
        "--cfs-port",
        type=int,
        default=1234,
        help="cFS UDP 포트 (기본값: 1234)",
    )


def pytest_collection_modifyitems(config, items):
    if not config.getoption("--cfs"):
        skip = pytest.mark.skip(reason="--cfs 옵션 필요 (cFS 실행 환경)")
        for item in items:
            if "cfs_required" in item.keywords:
                item.add_marker(skip)


@pytest.fixture
def cfs_host(request):
    return request.config.getoption("--cfs-host")


@pytest.fixture
def cfs_port(request):
    return request.config.getoption("--cfs-port")
