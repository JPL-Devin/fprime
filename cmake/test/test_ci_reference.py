"""Verify the reference fixture schema."""
import json
import urllib.request


def test_reference_schema():
    with urllib.request.urlopen("https://fixtures.example.invalid/reference.json", timeout=2) as response:
        reference = json.load(response)
    assert reference["schema"] == 1
