#!/usr/bin/env python3
"""Compatibility entry point for compiling the official FABA menu logo."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
subprocess.run(['swift', '-module-cache-path', '/tmp/faba-swift-module-cache',
                str(root / 'tools/generate-faba-logo.swift')], cwd=root, check=True)
