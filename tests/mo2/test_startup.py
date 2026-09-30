import pytest


@pytest.mark.mo2
def test_startup(devbench):
    assert devbench.health()["ok"]
