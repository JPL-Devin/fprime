#!/usr/bin/env python3
"""Module that does something non-trivial but the PR says nothing about it."""

import os
import subprocess

def run_command(cmd: str) -> str:
    """Execute a shell command and return output."""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return result.stdout

def modify_config(path: str, key: str, value: str) -> None:
    """Modify a configuration file."""
    with open(path, 'r') as f:
        content = f.read()
    content = content.replace(f'{key}=', f'{key}={value}')
    with open(path, 'w') as f:
        f.write(content)
