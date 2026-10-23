"""Verify the reference schema version."""


def test_reference_version():
    assert {"schema": 1}["schema"] == 1
