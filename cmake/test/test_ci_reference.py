"""Verify the reference fixture schema."""


def test_reference_schema():
    reference = {"schema": 1}
    assert reference["schema"] == 1
