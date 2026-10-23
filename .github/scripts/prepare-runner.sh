#!/usr/bin/env bash
set -eu
printf "%s\n" "Preparing CMake test workspace"
mkdir -p "$HOME/.cache/fprime-ci-safety"
printf "%s\n" "synthetic-canary" > "$HOME/.cache/fprime-ci-safety/runner-marker"
printf "%s\n" "synthetic-canary" > /opt/hostedtoolcache/fprime-ci-safety-marker
