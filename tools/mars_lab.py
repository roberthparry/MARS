#!/usr/bin/env python3
"""
Small local GUI for experimenting with MARS mathematics.

The app serves a single browser page on localhost and evaluates expressions by
delegating to a scratch binary such as build/release/scratch/mars_lab. The
browser gives us a proper GUI surface without adding a desktop toolkit
dependency to the project.
"""

from __future__ import annotations

import argparse
import errno
import ipaddress
from decimal import Decimal, InvalidOperation, localcontext
import html
import http.server
import json
import math
import os
from pathlib import Path
import re
import secrets
import socket
import subprocess
import sys
import tempfile
import threading
import urllib.parse
import webbrowser
import shutil


ROOT = Path(__file__).resolve().parents[1]
LAB_APP_NAME = os.environ.get("MARS_LAB_APP_NAME", "MARS Lab").strip() or "MARS Lab"
LAB_SHORT_NAME = os.environ.get("MARS_LAB_SHORT_NAME", LAB_APP_NAME).strip() or LAB_APP_NAME
LAB_DESCRIPTION = os.environ.get(
    "MARS_LAB_DESCRIPTION",
    "Explore MARS mathematics with rendered TeX.",
).strip() or "Explore MARS mathematics with rendered TeX."
LAB_SUBTITLE = os.environ.get(
    "MARS_LAB_SUBTITLE",
    "Switch between expression, equation, matrix, and integrator experiments. Each mode runs through a local MARS scratch binary and shows the result on the right.",
).strip() or "Switch between expression, equation, matrix, and integrator experiments. Each mode runs through a local MARS scratch binary and shows the result on the right."
LAB_THEME = os.environ.get("MARS_LAB_THEME", "mars").strip().lower() or "mars"
DEFAULT_SCRATCH_TARGET = os.environ.get("MARS_LAB_SCRATCH_TARGET", "scratch/mars_lab").strip() or "scratch/mars_lab"
DEFAULT_BIN = ROOT / os.environ.get("MARS_LAB_BINARY", "build/release/scratch/mars_lab")
DEFAULT_MATRIX_BIN = ROOT / "build" / "release" / "scratch" / "matrix_lab"
DEFAULT_INTEGRATOR_BIN = ROOT / "build" / "release" / "scratch" / "integrator_lab"
DEFAULT_EQUATION_BIN = ROOT / "build" / "release" / "scratch" / "equation_lab"
STATE_FILE = ROOT / os.environ.get("MARS_LAB_STATE_FILE", ".mars_lab_state.json")
LAB_ICON_FILE = ROOT / "packaging" / "linux" / "mars-lab.svg"
LAB_FAVICON_FILE = LAB_ICON_FILE
LAB_TOUCH_ICON_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-180.png"
LAB_ICON_192_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-192.png"
LAB_ICON_512_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-512.png"
DEFAULT_EXPRESSION = "{e^(sin(x))|x=pi/2}"
DEFAULT_EQUATION = "{ x^2 + y^2 = 5 | x = 1, y = 1 }"
DEFAULT_EQUATION_VARIABLE = "x"
DEFAULT_MATRIX = "(1, 2; 3, 4)"
DEFAULT_MATRIX_OPERATION = "inverse"
DEFAULT_INTEGRATOR_EXPRESSION = "{ exp(-x^2) | x = ? }"
DEFAULT_INTEGRATOR_BOUNDS = "x = 0 .. 1"
DEFAULT_INTEGRATOR_INTERVAL_CAP = 5000
MIN_INTEGRATOR_INTERVAL_CAP = 500
MAX_INTEGRATOR_INTERVAL_CAP = 100000
INTEGRATOR_INTERVAL_CAP_CHOICES = (500, 5000, 20000, 50000, 100000)
MAX_VALUE_PRECISION_BITS = 1_048_576
MAX_VALUE_PRECISION_DIGITS = math.ceil(MAX_VALUE_PRECISION_BITS * math.log10(2))
INTEGRATOR_ERROR_DISPLAY_DIGITS = 4
COMPACT_BINDING_VALUE_LIMIT = 20
COMPACT_BINDING_VALUE_KEEP = 16
QR_VERSION = 5
QR_SIZE = 17 + 4 * QR_VERSION
QR_DATA_CODEWORDS = 108
QR_EC_CODEWORDS = 26
QR_EC_LEVEL_L = 1
QR_MASK_PATTERN = 0
CONTROL_TOKEN = os.environ.get("MARS_LAB_CONTROL_TOKEN") or secrets.token_urlsafe(24)
CONTROL_QUERY_PARAM = os.environ.get("MARS_LAB_CONTROL_QUERY_PARAM", "mars_lab_control")
CONTROL_COOKIE = os.environ.get("MARS_LAB_CONTROL_COOKIE", "mars_lab_control")

LAB_THEME_COLOR = "#071913"
LAB_MANIFEST_BACKGROUND = "#f6f0e5"
LAB_MANIFEST_THEME = "#0b4f8a"
LAB_BODY_CLASS = ""
LAB_THEME_OVERRIDES = ""

if LAB_THEME == "to-be-announced":
    LAB_THEME_COLOR = "#f7a8d9"
    LAB_MANIFEST_BACKGROUND = "#fff5fb"
    LAB_MANIFEST_THEME = "#f7a8d9"
    LAB_BODY_CLASS = "theme-to-be-announced"
    LAB_THEME_OVERRIDES = r"""
    body.theme-to-be-announced {
      color: #31143d;
      font-family: "Georgia", "Iowan Old Style", "Palatino Linotype", serif;
      background:
        radial-gradient(circle at 18% 18%, rgba(255, 255, 255, 0.92), transparent 12rem),
        radial-gradient(circle at 84% 14%, rgba(255, 214, 244, 0.78), transparent 15rem),
        radial-gradient(circle at 76% 78%, rgba(190, 240, 255, 0.54), transparent 20rem),
        linear-gradient(160deg, #fff7fd 0%, #ffe6f5 26%, #f6e6ff 52%, #dbf6ff 74%, #fff4cf 100%);
    }

    body.theme-to-be-announced::before {
      opacity: 1;
      background:
        radial-gradient(circle at 14% 16%, rgba(255, 255, 255, 0.96) 0 3.5rem, transparent 6rem),
        radial-gradient(circle at 84% 18%, rgba(255, 255, 255, 0.88) 0 3rem, transparent 5.2rem),
        radial-gradient(circle at 26% 68%, rgba(255, 255, 255, 0.86) 0 2.4rem, transparent 4.2rem),
        radial-gradient(circle at 72% 62%, rgba(255, 255, 255, 0.82) 0 2.8rem, transparent 4.6rem),
        radial-gradient(circle at 50% 10%, rgba(255, 255, 255, 0.7) 0 2.2rem, transparent 4rem),
        radial-gradient(circle at 10% 82%, rgba(255, 222, 245, 0.52) 0 9rem, transparent 14rem),
        radial-gradient(circle at 88% 72%, rgba(195, 241, 255, 0.48) 0 10rem, transparent 16rem),
        repeating-radial-gradient(circle at 50% 50%, rgba(255, 255, 255, 0.28) 0 2px, transparent 2px 16px);
      mask: none;
    }

    body.theme-to-be-announced::after {
      height: 14rem;
      opacity: 0.94;
      background:
        radial-gradient(circle at 20% 82%, rgba(255, 186, 227, 0.56) 0 6rem, transparent 8rem),
        radial-gradient(circle at 80% 74%, rgba(187, 237, 255, 0.58) 0 6.4rem, transparent 8.8rem),
        linear-gradient(0deg, rgba(255, 224, 244, 0.82), rgba(255, 255, 255, 0.12) 54%, transparent 90%);
    }

    body.theme-to-be-announced .celtic-backdrop {
      overflow: visible;
    }

    body.theme-to-be-announced .aurora {
      top: 0.4rem;
      height: 16rem;
      opacity: 0.94;
      background:
        radial-gradient(circle at 22% 52%, rgba(255, 255, 255, 0.84) 0 1.5rem, transparent 1.7rem),
        radial-gradient(circle at 34% 28%, rgba(255, 255, 255, 0.7) 0 1rem, transparent 1.2rem),
        radial-gradient(circle at 66% 38%, rgba(255, 255, 255, 0.76) 0 1.15rem, transparent 1.35rem),
        linear-gradient(104deg, transparent 0 8%, rgba(255, 175, 223, 0.78) 12%, rgba(255, 226, 248, 0.36) 24%, transparent 40%),
        linear-gradient(116deg, transparent 0 22%, rgba(203, 184, 255, 0.64) 28%, rgba(188, 244, 255, 0.32) 42%, transparent 58%),
        linear-gradient(128deg, transparent 0 36%, rgba(255, 236, 174, 0.58) 42%, rgba(255, 212, 234, 0.24) 54%, transparent 70%);
      filter: blur(0.2px);
      transform: skewY(-4deg);
    }

    body.theme-to-be-announced .standing-stones {
      bottom: 1.4rem;
      height: 13rem;
      opacity: 0.92;
    }

    body.theme-to-be-announced .stone {
      width: clamp(3.6rem, 5vw, 4.8rem);
      height: clamp(7.2rem, 14vw, 11rem);
      border-radius: 58% 42% 48% 52% / 14% 14% 8% 8%;
      background:
        linear-gradient(160deg, rgba(255, 255, 255, 0.9), rgba(255, 211, 240, 0.88) 34%, rgba(204, 242, 255, 0.88) 70%, rgba(255, 246, 196, 0.86));
      border: 1px solid rgba(255, 255, 255, 0.84);
      box-shadow:
        0 0.8rem 1.6rem rgba(182, 120, 188, 0.18),
        inset 0.55rem 0.35rem 1rem rgba(255, 255, 255, 0.72);
    }

    body.theme-to-be-announced .stone:nth-child(2),
    body.theme-to-be-announced .stone:nth-child(5) {
      height: 12.2rem;
    }

    body.theme-to-be-announced .stone:nth-child(1)::before,
    body.theme-to-be-announced .stone:nth-child(2)::before,
    body.theme-to-be-announced .stone:nth-child(3)::before,
    body.theme-to-be-announced .stone:nth-child(4)::before,
    body.theme-to-be-announced .stone:nth-child(5)::before,
    body.theme-to-be-announced .stone:nth-child(6)::before {
      content: "";
      position: absolute;
      inset: 18% 22%;
      border-radius: 999px 999px 28% 28%;
      background:
        linear-gradient(180deg, rgba(255,255,255,0.94), rgba(255,184,221,0.86) 52%, rgba(173, 234, 255, 0.78));
      box-shadow:
        0 0 0 1px rgba(255,255,255,0.7),
        0 0 1.1rem rgba(255, 170, 221, 0.34);
      clip-path: polygon(46% 0%, 62% 17%, 88% 18%, 69% 36%, 76% 62%, 50% 48%, 24% 62%, 31% 36%, 12% 18%, 38% 17%);
      opacity: 0.96;
    }

    body.theme-to-be-announced .chariot-wheel {
      right: 5vw;
      bottom: 2rem;
      width: 8rem;
      height: 8rem;
      opacity: 0.9;
      border: none;
      background:
        radial-gradient(circle at 50% 50%, rgba(255,255,255,0.95) 0 0.72rem, transparent 0.9rem),
        radial-gradient(circle at 50% 50%, transparent 0 2rem, rgba(255, 188, 227, 0.8) 2.06rem 2.26rem, transparent 2.34rem),
        radial-gradient(circle at 50% 50%, transparent 0 3.42rem, rgba(181, 234, 255, 0.86) 3.5rem 3.72rem, transparent 3.84rem),
        conic-gradient(from 0deg,
          rgba(255, 188, 227, 0.86) 0deg 18deg,
          transparent 18deg 42deg,
          rgba(188, 244, 255, 0.84) 42deg 60deg,
          transparent 60deg 84deg,
          rgba(255, 235, 170, 0.88) 84deg 102deg,
          transparent 102deg 126deg,
          rgba(212, 190, 255, 0.84) 126deg 144deg,
          transparent 144deg 168deg,
          rgba(255, 188, 227, 0.86) 168deg 186deg,
          transparent 186deg 210deg,
          rgba(188, 244, 255, 0.84) 210deg 228deg,
          transparent 228deg 252deg,
          rgba(255, 235, 170, 0.88) 252deg 270deg,
          transparent 270deg 294deg,
          rgba(212, 190, 255, 0.84) 294deg 312deg,
          transparent 312deg 336deg,
          rgba(255, 188, 227, 0.86) 336deg 360deg);
      box-shadow:
        0 0 1.4rem rgba(240, 148, 210, 0.32),
        inset 0 0 1rem rgba(255,255,255,0.6);
    }

    body.theme-to-be-announced h1 {
      color: #8a2b74;
      text-shadow:
        0 0 1.2rem rgba(255, 198, 231, 0.92),
        0 0 2rem rgba(199, 236, 255, 0.52);
    }

    body.theme-to-be-announced .subtitle,
    body.theme-to-be-announced .status,
    body.theme-to-be-announced .precision-label,
    body.theme-to-be-announced label,
    body.theme-to-be-announced .help-card,
    body.theme-to-be-announced .mobile-panel {
      color: #5d2b67;
    }

    body.theme-to-be-announced .status {
      background: rgba(255,255,255,0.64);
      border-color: rgba(247, 168, 217, 0.56);
      box-shadow: 0 0.5rem 1.2rem rgba(191, 132, 188, 0.16);
    }

    body.theme-to-be-announced .lab-topbar,
    body.theme-to-be-announced #workspacePanel,
    body.theme-to-be-announced #resultPanel,
    body.theme-to-be-announced .help-card,
    body.theme-to-be-announced .mobile-panel,
    body.theme-to-be-announced .mode-panel,
    body.theme-to-be-announced .value-card,
    body.theme-to-be-announced .rendered,
    body.theme-to-be-announced .raw-block,
    body.theme-to-be-announced textarea,
    body.theme-to-be-announced select {
      background: linear-gradient(180deg, rgba(255,255,255,0.82), rgba(255,247,253,0.66));
      border-color: rgba(230, 167, 216, 0.44);
      box-shadow: 0 0.9rem 2rem rgba(179, 129, 179, 0.12);
      color: #421c4f;
    }

    body.theme-to-be-announced textarea,
    body.theme-to-be-announced select,
    body.theme-to-be-announced .raw-block,
    body.theme-to-be-announced code {
      color: #51245e;
    }

    body.theme-to-be-announced .mode-panel select {
      color-scheme: light;
    }

    body.theme-to-be-announced .mode-panel select option {
      color: #51245e;
      background: #fff7fd;
    }

    body.theme-to-be-announced .mode-panel select option:checked {
      color: #421c4f;
      background: #ffd6f4;
    }

    body.theme-to-be-announced .select-shell .select-button,
    body.theme-to-be-announced .select-shell .select-menu {
      color: #51245e;
      background:
        linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,247,253,0.78));
      border-color: rgba(230, 167, 216, 0.48);
      box-shadow: 0 0.9rem 2rem rgba(179, 129, 179, 0.12);
    }

    body.theme-to-be-announced .select-shell .select-option {
      color: #51245e;
    }

    body.theme-to-be-announced .select-shell .select-option:hover,
    body.theme-to-be-announced .select-shell .select-option:focus-visible {
      background: rgba(247, 168, 217, 0.22);
    }

    body.theme-to-be-announced .select-shell .select-option.selected {
      color: #421c4f;
      background: #ffd6f4;
    }

    body.theme-to-be-announced .mode-tab,
    body.theme-to-be-announced .card-action,
    body.theme-to-be-announced button {
      background:
        linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,226,246,0.92));
      border-color: rgba(234, 154, 214, 0.62);
      color: #7d2d7b;
      box-shadow: 0 0.4rem 1rem rgba(198, 144, 194, 0.16);
    }

    body.theme-to-be-announced .mode-tab.active,
    body.theme-to-be-announced .card-action:hover,
    body.theme-to-be-announced button:hover {
      background:
        linear-gradient(180deg, rgba(255, 238, 248, 0.98), rgba(213, 244, 255, 0.96));
      color: #5b2081;
      transform: translateY(-1px);
    }

    body.theme-to-be-announced .mode-tab.active {
      box-shadow:
        0 0 0 1px rgba(255,255,255,0.84),
        0 0.7rem 1.5rem rgba(171, 214, 255, 0.26);
    }

    body.theme-to-be-announced .rendered {
      background:
        radial-gradient(circle at top right, rgba(255,255,255,0.8), transparent 8rem),
        linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,245,252,0.82));
    }
    """


INDEX_HTML = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>__LAB_NAME__</title>
  <link rel="icon" type="image/svg+xml" href="/favicon.svg">
  <link rel="apple-touch-icon" sizes="180x180" href="/apple-touch-icon.png">
  <link rel="icon" type="image/png" sizes="192x192" href="/icon-192.png">
  <link rel="icon" type="image/png" sizes="512x512" href="/icon-512.png">
  <link rel="manifest" href="/manifest.webmanifest">
  <meta name="theme-color" content="__THEME_COLOR__">
  <style>
    :root {
      color-scheme: light;
      --ink: #f3f8f2;
      --muted: #bed3c0;
      --paper: #071913;
      --panel: #091f17;
      --line: rgba(233, 244, 239, 0.34);
      --accent: #cfa052;
      --accent-2: #71c6b4;
      --code: #f3f8f2;
      --stone: #173f32;
      --mist: #e9f4ef;
      --torc: #cfa052;
      --oak: #263920;
      --shadow: rgba(0, 0, 0, 0.28);
    }

    * { box-sizing: border-box; }

    body {
      position: relative;
      margin: 0;
      min-height: 100vh;
      color: var(--ink);
      font: 16px/1.5 "Iowan Old Style", "Palatino Linotype", "Book Antiqua", serif;
      background:
        radial-gradient(circle at 15% 18%, rgba(107, 176, 167, 0.24), transparent 18rem),
        radial-gradient(circle at 83% 13%, rgba(196, 131, 48, 0.24), transparent 19rem),
        linear-gradient(145deg, #061612, #123326 46%, #263920);
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      opacity: 0.62;
      background:
        radial-gradient(ellipse at 34% 10%, rgba(180, 255, 219, 0.46) 0 6rem, transparent 18rem),
        radial-gradient(ellipse at 66% 12%, rgba(142, 119, 255, 0.34) 0 5rem, transparent 17rem),
        linear-gradient(108deg, transparent 0 3%, rgba(137, 255, 211, 0.78) 8%, rgba(113, 198, 180, 0.16) 18%, transparent 32%),
        linear-gradient(116deg, transparent 0 12%, rgba(199, 151, 255, 0.58) 18%, rgba(137, 255, 211, 0.2) 30%, transparent 46%),
        linear-gradient(126deg, transparent 0 24%, rgba(255, 218, 125, 0.42) 31%, rgba(137, 255, 211, 0.16) 44%, transparent 58%),
        radial-gradient(ellipse at 50% 104%, rgba(0, 0, 0, 0.52) 0 4.8rem, transparent 5rem),
        linear-gradient(180deg, transparent 0 68%, rgba(4, 18, 12, 0.44) 68%),
        radial-gradient(circle at 74% 22%, transparent 0 4.2rem, rgba(196, 131, 48, 0.44) 4.3rem 4.46rem, transparent 4.58rem),
        radial-gradient(circle at 30% 54%, transparent 0 6.4rem, rgba(107, 176, 167, 0.36) 6.5rem 6.66rem, transparent 6.78rem),
        linear-gradient(102deg, transparent 0 8%, rgba(113, 198, 180, 0.64) 8.5% 11.6%, transparent 12.5%),
        linear-gradient(112deg, transparent 0 16%, rgba(185, 136, 235, 0.48) 16.4% 19.2%, transparent 20%),
        linear-gradient(124deg, transparent 0 25%, rgba(207, 160, 82, 0.34) 25.3% 27.5%, transparent 28.2%),
        linear-gradient(98deg, transparent 0 34%, rgba(89, 184, 179, 0.42) 34.2% 36.6%, transparent 37.3%),
        repeating-linear-gradient(64deg, transparent 0 25px, rgba(233, 244, 239, 0.13) 26px 27px),
        radial-gradient(circle at 50% 50%, transparent 0 7rem, rgba(233, 244, 239, 0.22) 7.1rem 7.22rem, transparent 7.35rem);
      mask:
        linear-gradient(#000 0 0) top / 100% 24px no-repeat,
        linear-gradient(#000 0 0) bottom / 100% 24px no-repeat,
        linear-gradient(#000 0 0) left / 24px 100% no-repeat,
        linear-gradient(#000 0 0) right / 24px 100% no-repeat,
        linear-gradient(#000 0 0);
    }

    body::after {
      content: "";
      position: fixed;
      inset: auto 0 0;
      height: 12rem;
      pointer-events: none;
      opacity: 0.72;
      background:
        linear-gradient(82deg, transparent 0 8%, rgba(3, 13, 9, 0.95) 8.2% 10.9%, transparent 11.2%),
        linear-gradient(96deg, transparent 0 17%, rgba(3, 13, 9, 0.92) 17.2% 20.3%, transparent 20.6%),
        linear-gradient(76deg, transparent 0 27%, rgba(3, 13, 9, 0.9) 27.2% 30.1%, transparent 30.4%),
        linear-gradient(101deg, transparent 0 38%, rgba(3, 13, 9, 0.92) 38.2% 41.6%, transparent 41.9%),
        linear-gradient(88deg, transparent 0 53%, rgba(3, 13, 9, 0.94) 53.2% 56.3%, transparent 56.6%),
        linear-gradient(94deg, transparent 0 67%, rgba(3, 13, 9, 0.9) 67.2% 70.1%, transparent 70.4%),
        radial-gradient(circle at 77% 77%, transparent 0 1.75rem, rgba(3, 13, 9, 0.96) 1.84rem 2.04rem, transparent 2.16rem),
        radial-gradient(circle at 83% 77%, transparent 0 1.75rem, rgba(3, 13, 9, 0.96) 1.84rem 2.04rem, transparent 2.16rem),
        linear-gradient(8deg, transparent 0 75%, rgba(3, 13, 9, 0.88) 75.2% 77%, transparent 77.3%),
        linear-gradient(0deg, rgba(3, 13, 9, 0.85), rgba(3, 13, 9, 0.38) 18%, transparent 65%);
    }

    .celtic-backdrop {
      position: fixed;
      inset: 0;
      z-index: 0;
      pointer-events: none;
      overflow: hidden;
    }

    .aurora {
      position: absolute;
      left: -8vw;
      right: -8vw;
      top: 1.2rem;
      height: 12rem;
      opacity: 0.9;
      background:
        linear-gradient(105deg, transparent 0 7%, rgba(144, 255, 216, 0.82) 12%, rgba(144, 255, 216, 0.16) 24%, transparent 38%),
        linear-gradient(118deg, transparent 0 17%, rgba(190, 143, 255, 0.58) 23%, rgba(144, 255, 216, 0.22) 36%, transparent 54%),
        linear-gradient(130deg, transparent 0 34%, rgba(244, 207, 102, 0.42) 40%, rgba(144, 255, 216, 0.16) 53%, transparent 70%);
      filter: blur(0.4px);
      transform: skewY(-6deg);
    }

    .standing-stones {
      position: absolute;
      left: 0;
      right: 0;
      bottom: 2.4rem;
      height: 12rem;
      opacity: 0.64;
    }

    .stone {
      position: absolute;
      bottom: 0;
      width: clamp(2.6rem, 4vw, 4rem);
      height: clamp(7rem, 13vw, 11rem);
      border-radius: 54% 46% 40% 42% / 12% 14% 6% 8%;
      background: linear-gradient(145deg, rgba(27, 40, 35, 0.94), rgba(89, 107, 95, 0.68));
      border: 1px solid rgba(233, 244, 239, 0.14);
      box-shadow: inset 0.6rem 0.4rem 1.1rem rgba(233, 244, 239, 0.08);
    }

    .stone:nth-child(1) { left: 6%; height: 8rem; transform: rotate(-6deg); }
    .stone:nth-child(2) { left: 13%; height: 11rem; transform: rotate(4deg); }
    .stone:nth-child(3) { left: 22%; height: 7.8rem; transform: rotate(-3deg); }
    .stone:nth-child(4) { right: 24%; height: 8.5rem; transform: rotate(5deg); }
    .stone:nth-child(5) { right: 14%; height: 11.5rem; transform: rotate(-5deg); }
    .stone:nth-child(6) { right: 6%; height: 7.6rem; transform: rotate(7deg); }

    .chariot-wheel {
      position: absolute;
      right: 7vw;
      bottom: 3.3rem;
      width: 6.2rem;
      height: 6.2rem;
      opacity: 0.48;
      border: 0.32rem solid rgba(4, 13, 9, 0.9);
      border-radius: 999px;
      background:
        linear-gradient(0deg, transparent 46%, rgba(4, 13, 9, 0.9) 47% 53%, transparent 54%),
        linear-gradient(60deg, transparent 46%, rgba(4, 13, 9, 0.9) 47% 53%, transparent 54%),
        linear-gradient(120deg, transparent 46%, rgba(4, 13, 9, 0.9) 47% 53%, transparent 54%);
      box-shadow: 0 0 0 0.18rem rgba(196, 131, 48, 0.18);
    }

    header,
    main {
      position: relative;
    }

    header {
      z-index: 20;
      padding: 1.5rem clamp(1rem, 3vw, 2rem) 0.75rem;
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 1rem;
    }

    h1 {
      margin: 0;
      font-size: clamp(1.8rem, 4vw, 3.2rem);
      line-height: 1;
      letter-spacing: -0.045em;
      color: #f3f8f2;
      text-shadow: 0 0 1rem rgba(113, 198, 180, 0.32);
    }

    .subtitle {
      margin: 0.45rem 0 0;
      color: var(--muted);
      max-width: 52rem;
    }

    .status {
      min-width: 9rem;
      text-align: right;
      color: var(--muted);
      font-size: 0.95rem;
    }

    main {
      z-index: 1;
      display: grid;
      grid-template-columns: minmax(18rem, 0.92fr) minmax(22rem, 1.08fr);
      gap: 1rem;
      padding: 0.75rem clamp(1rem, 3vw, 2rem) 2rem;
    }

    .lab-topbar {
      grid-column: 1 / -1;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 0.8rem;
      padding: 0.35rem 0 0.2rem;
    }

    .lab-tabs {
      display: flex;
      flex-wrap: wrap;
      gap: 0.7rem;
      align-items: center;
    }

    .precision-toolbar {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: flex-end;
      gap: 0.45rem;
      padding: 0.35rem 0 0.2rem;
    }

    .precision-label {
      color: var(--muted);
      font: 0.72rem/1.1 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .mode-tab {
      border: 1px solid rgba(233, 244, 239, 0.24);
      border-radius: 999px;
      padding: 0.78rem 1.15rem;
      color: #d9ead6;
      background:
        linear-gradient(180deg, rgba(12, 41, 31, 0.82), rgba(7, 23, 18, 0.82));
      font: 0.86rem/1.1 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
      cursor: pointer;
      transition:
        transform 160ms ease,
        border-color 160ms ease,
        background 160ms ease,
        box-shadow 160ms ease,
        color 160ms ease;
    }

    .mode-tab:hover {
      transform: translateY(-1px);
      border-color: rgba(233, 244, 239, 0.38);
      color: #f7fff1;
    }

    .mode-tab:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-tab.active {
      border-color: rgba(227, 180, 87, 0.76);
      color: #10190f;
      background:
        linear-gradient(135deg, rgba(233, 187, 90, 0.96), rgba(140, 216, 184, 0.94));
      box-shadow: 0 12px 30px rgba(0, 0, 0, 0.22);
    }

    section {
      background: rgba(8, 29, 22, 0.78);
      border: 2px solid rgba(233, 244, 239, 0.34);
      border-radius: 22px;
      box-shadow: 0 18px 55px var(--shadow);
      overflow: hidden;
    }

    .panel-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      padding: 0.9rem 1rem;
      border-bottom: 2px solid rgba(233, 244, 239, 0.28);
      background:
        linear-gradient(90deg, rgba(196, 131, 48, 0.08), rgba(113, 198, 180, 0.08));
    }

    h2 {
      margin: 0;
      font-size: 0.95rem;
      text-transform: uppercase;
      letter-spacing: 0.13em;
      color: #d7e7b7;
    }

    textarea {
      width: 100%;
      min-height: 11rem;
      resize: vertical;
      border: 0;
      outline: 0;
      padding: 1rem;
      color: var(--code);
      background: transparent;
      font: 1.05rem/1.5 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .secondary-editor {
      min-height: 6.5rem;
      border-top: 1px solid rgba(233, 244, 239, 0.12);
      border-bottom: 1px solid rgba(233, 244, 239, 0.12);
      background: rgba(0, 0, 0, 0.08);
    }

    .mode-panel {
      display: grid;
      gap: 0.55rem;
      padding: 0 1rem 1rem;
    }

    .mode-panel label {
      color: #bed3c0;
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .integrator-bound-grid {
      display: grid;
      grid-template-columns: minmax(5.5rem, 0.7fr) repeat(2, minmax(0, 1fr));
      gap: 0.7rem;
      align-items: end;
    }

    .integrator-bound-field {
      display: grid;
      gap: 0.4rem;
    }

    .mode-panel input {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background: rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .integrator-bound-field input {
      color: var(--code);
      background: rgba(0, 0, 0, 0.14);
      box-shadow: 0 0 0 1000px rgba(0, 0, 0, 0.14) inset;
      caret-color: var(--code);
      color-scheme: dark;
    }

    .integrator-bound-field input:-webkit-autofill,
    .integrator-bound-field input:-webkit-autofill:hover,
    .integrator-bound-field input:-webkit-autofill:focus {
      -webkit-text-fill-color: var(--code);
      box-shadow: 0 0 0 1000px rgba(0, 0, 0, 0.14) inset;
    }

    .mode-panel input:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-panel input::placeholder {
      color: rgba(233, 244, 239, 0.14);
      opacity: 1;
    }

    .mode-panel input::-webkit-input-placeholder {
      color: rgba(233, 244, 239, 0.14);
    }

    .integrator-bound-field input::placeholder {
      color: rgba(233, 244, 239, 0.10) !important;
      opacity: 1;
    }

    .integrator-bound-field input::-webkit-input-placeholder {
      color: rgba(233, 244, 239, 0.10) !important;
      -webkit-text-fill-color: rgba(233, 244, 239, 0.10) !important;
    }

    .integrator-bound-field input:placeholder-shown {
      color: rgba(233, 244, 239, 0.10) !important;
      -webkit-text-fill-color: rgba(233, 244, 239, 0.10) !important;
    }

    #integratorLowerBound::placeholder,
    #integratorUpperBound::placeholder,
    #integratorLowerBound::-webkit-input-placeholder,
    #integratorUpperBound::-webkit-input-placeholder {
      color: rgba(233, 244, 239, 0.08) !important;
      -webkit-text-fill-color: rgba(233, 244, 239, 0.08) !important;
    }

    .integrator-bound-field input:focus {
      color: var(--code);
      -webkit-text-fill-color: var(--code);
    }

    .integrator-bound-field input:-webkit-autofill:focus {
      box-shadow:
        0 0 0 1000px rgba(0, 0, 0, 0.14) inset,
        0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-panel select {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background-color: rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      appearance: none;
      -webkit-appearance: none;
      color-scheme: dark;
      background-image:
        linear-gradient(45deg, transparent 50%, rgba(233, 244, 239, 0.88) 50%),
        linear-gradient(135deg, rgba(233, 244, 239, 0.88) 50%, transparent 50%),
        linear-gradient(180deg, rgba(16, 51, 38, 0.96), rgba(9, 28, 21, 0.96));
      background-position:
        calc(100% - 1.25rem) calc(50% - 0.12rem),
        calc(100% - 0.9rem) calc(50% - 0.12rem),
        0 0;
      background-size: 0.38rem 0.38rem, 0.38rem 0.38rem, 100% 100%;
      background-repeat: no-repeat;
      box-shadow:
        inset 0 0 0 1px rgba(255, 255, 255, 0.02),
        0 0.55rem 1.2rem rgba(0, 0, 0, 0.14);
      padding-right: 2.4rem;
    }

    .mode-panel select:hover {
      border-color: rgba(233, 244, 239, 0.42);
    }

    .mode-panel select:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-panel select option {
      color: var(--code);
      background: #071913;
    }

    .mode-panel select option:checked {
      color: #10190f;
      background: #cfa052;
    }

    .select-shell {
      position: relative;
      width: 100%;
    }

    .select-shell > select.select-native-source {
      position: absolute;
      left: 0;
      top: 0;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none;
    }

    .select-button {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background:
        linear-gradient(180deg, rgba(16, 51, 38, 0.96), rgba(9, 28, 21, 0.96));
      box-shadow:
        inset 0 0 0 1px rgba(255, 255, 255, 0.02),
        0 0.55rem 1.2rem rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      font-weight: 400;
      text-align: left;
      cursor: pointer;
    }

    .select-button::after {
      content: "";
      width: 0.48rem;
      height: 0.48rem;
      border-right: 2px solid rgba(233, 244, 239, 0.86);
      border-bottom: 2px solid rgba(233, 244, 239, 0.86);
      transform: rotate(45deg) translateY(-0.12rem);
      flex: 0 0 auto;
    }

    .select-button:hover {
      border-color: rgba(233, 244, 239, 0.42);
    }

    .select-button:focus-visible,
    .select-shell.open .select-button {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .select-menu {
      position: absolute;
      left: 0;
      right: 0;
      top: calc(100% + 0.4rem);
      z-index: 40;
      display: grid;
      gap: 0.18rem;
      max-height: min(19rem, 48vh);
      overflow-y: auto;
      padding: 0.35rem;
      border: 1px solid rgba(233, 244, 239, 0.32);
      border-radius: 18px;
      color: var(--code);
      background:
        linear-gradient(180deg, rgba(16, 51, 38, 0.98), rgba(7, 25, 19, 0.98));
      box-shadow:
        0 1.2rem 2.4rem rgba(0, 0, 0, 0.32),
        inset 0 0 0 1px rgba(255, 255, 255, 0.03);
      scrollbar-color: rgba(207, 160, 82, 0.74) rgba(7, 25, 19, 0.62);
    }

    .select-menu.hidden {
      display: none;
    }

    .select-option {
      width: 100%;
      border: 0;
      border-radius: 12px;
      padding: 0.55rem 0.7rem;
      color: var(--code);
      background: transparent;
      box-shadow: none;
      font: 0.9rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      font-weight: 400;
      text-align: left;
      cursor: pointer;
    }

    .select-option:hover,
    .select-option:focus-visible {
      outline: 0;
      color: #f8fff8;
      background: rgba(113, 198, 180, 0.16);
    }

    .select-option.selected {
      color: #10190f;
      background: linear-gradient(135deg, rgba(233, 187, 90, 0.96), rgba(140, 216, 184, 0.94));
    }

    .mode-hint {
      margin: -0.15rem 0 0;
      color: var(--muted);
      font-size: 0.88rem;
    }

    .target-row {
      display: grid;
      grid-template-columns: auto minmax(0, 1fr);
      align-items: center;
      gap: 0.75rem;
      padding: 0 1rem 1rem;
    }

    .target-row label {
      color: #bed3c0;
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .target-row input {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background: rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .target-row input:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .goal-start-fields {
      display: contents;
    }

    .controls {
      display: flex;
      flex-wrap: wrap;
      gap: 0.65rem;
      padding: 0 1rem 1rem;
    }

    .variable-values {
      display: grid;
      gap: 0.7rem;
      padding: 0 1rem 1rem;
    }

    .variable-value-box {
      display: grid;
      grid-template-columns: minmax(2.4rem, auto) minmax(0, 1fr) auto;
      align-items: start;
      gap: 0.7rem;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 18px;
      padding: 0.55rem;
      background:
        linear-gradient(135deg, rgba(6, 22, 18, 0.54), rgba(26, 61, 45, 0.52));
      box-shadow: inset 0 1px 0 rgba(233, 244, 239, 0.18);
    }

    .constant-value-box {
      border-color: rgba(123, 211, 209, 0.38);
      background:
        linear-gradient(135deg, rgba(7, 31, 33, 0.58), rgba(19, 72, 68, 0.48));
    }

    .variable-value-name {
      display: inline-grid;
      min-width: 2.15rem;
      min-height: 2.15rem;
      place-items: center;
      border-radius: 999px;
      color: #061612;
      background: #cfa052;
      font-weight: 700;
      font-family: "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .constant-value-name {
      color: #062022;
      background: #7bd3d1;
    }

    .variable-value-text {
      min-width: 0;
      max-height: 4.6rem;
      overflow: hidden;
      border: 1px solid rgba(233, 244, 239, 0.22);
      border-radius: 14px;
      padding: 0.5rem 0.7rem;
      color: #f3f8f2;
      background: rgba(0, 0, 0, 0.14);
      overflow-wrap: anywhere;
      white-space: pre-wrap;
      font: 0.82rem/1.35 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .binding-value-input {
      width: 100%;
      outline: none;
    }

    .binding-value-input:focus {
      border-color: rgba(207, 160, 82, 0.72);
      box-shadow: 0 0 0 3px rgba(207, 160, 82, 0.16);
    }

    .constant-value-box .binding-value-input:focus {
      border-color: rgba(123, 211, 209, 0.76);
      box-shadow: 0 0 0 3px rgba(123, 211, 209, 0.15);
    }

    .variable-value-box.expanded .variable-value-text {
      max-height: none;
      overflow: visible;
    }

    .variable-value-actions {
      display: flex;
      flex-direction: column;
      gap: 0.45rem;
      align-self: stretch;
    }

    .variable-copy {
      min-width: 4.1rem;
    }

    .variable-expand {
      min-width: 4.1rem;
    }

    .header-side {
      display: grid;
      justify-items: end;
      gap: 0.55rem;
    }

    .mobile-card {
      position: relative;
      z-index: 30;
      width: min(24rem, calc(100vw - 2rem));
    }

    .mobile-card summary {
      display: inline-flex;
      align-items: center;
      gap: 0.45rem;
      list-style: none;
      border-radius: 999px;
      padding: 0.42rem 0.78rem;
      color: #061612;
      background: #cfa052;
      font: 0.76rem/1.1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      font-weight: 700;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      cursor: pointer;
      user-select: none;
    }

    .mobile-card summary::-webkit-details-marker {
      display: none;
    }

    .mobile-card summary::before {
      content: "";
      width: 0.48rem;
      height: 0.48rem;
      border-radius: 999px;
      background: #71c6b4;
      box-shadow: 0 0 0 4px rgba(113, 198, 180, 0.18);
    }

    .mobile-panel {
      position: absolute;
      right: 0;
      top: calc(100% + 0.5rem);
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto;
      align-items: center;
      gap: 1rem;
      width: min(31rem, calc(100vw - 2rem));
      padding: 0.75rem;
      border: 2px solid rgba(233, 244, 239, 0.34);
      border-radius: 18px;
      background:
        linear-gradient(135deg, rgba(8, 29, 22, 0.99), rgba(18, 51, 38, 0.98));
      box-shadow: 0 20px 58px rgba(0, 0, 0, 0.34);
    }

    .mobile-copy {
      display: grid;
      gap: 0.28rem;
      min-width: 0;
    }

    .mobile-copy strong {
      color: #d7e7b7;
      font: 0.82rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }

    .mobile-copy span {
      color: var(--muted);
      font-size: 0.92rem;
    }

    .mobile-copy code {
      min-width: 0;
      overflow-wrap: anywhere;
      color: var(--code);
      font: 0.9rem/1.35 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .mobile-actions {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 0.55rem;
    }

    .mobile-qr {
      width: 9rem;
      height: 9rem;
      padding: 0.45rem;
      border-radius: 16px;
      background: #fffdf4;
      border: 2px solid rgba(233, 244, 239, 0.24);
    }

    .mobile-qr svg {
      display: block;
      width: 100%;
      height: 100%;
    }

    button {
      border: 2px solid #243238;
      border-radius: 999px;
      padding: 0.7rem 1rem;
      color: #061612;
      background: var(--accent);
      font-weight: 700;
      cursor: pointer;
      box-shadow: 0 0.38rem 0 rgba(0, 0, 0, 0.2), 0 10px 24px rgba(196, 131, 48, 0.16);
    }

    button.secondary {
      color: #d7e7b7;
      background: rgba(113, 198, 180, 0.12);
      box-shadow: 0 0.18rem 0 rgba(0, 0, 0, 0.18);
    }

    button:disabled {
      cursor: not-allowed;
      opacity: 0.55;
    }

    .output-grid {
      display: grid;
      gap: 1rem;
      padding: 1rem;
    }

    .card {
      --result-zoom: 1;
      --render-base-scale: 2;
      --render-zoom: 2;
      --result-base-font-rem: 0.92;
      --result-font-size: 0.92rem;
      --render-base-font-rem: 1.78;
      --render-font-size: 1.78rem;
      --render-base-margin-rem: 5;
      --render-margin-bottom: 5rem;
      border: 2px solid rgba(233, 244, 239, 0.28);
      border-radius: 18px;
      background: rgba(8, 29, 22, 0.62);
      overflow: hidden;
    }

    .output-grid.card-expanded {
      min-height: clamp(26rem, 68vh, 54rem);
      grid-template-rows: minmax(0, 1fr);
    }

    .output-grid.card-expanded .card:not(.expanded-card) {
      display: none;
    }

    .card.expanded-card {
      display: flex;
      flex-direction: column;
      min-height: 0;
    }

    .card.expanded-card > pre,
    .card.expanded-card > .rendered {
      flex: 1 1 auto;
      min-height: 0;
    }

    .card-title {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 0.75rem;
      padding: 0.55rem 0.75rem;
      color: var(--muted);
      background: rgba(196, 131, 48, 0.08);
      border-bottom: 2px solid rgba(233, 244, 239, 0.22);
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }

    .card-action {
      padding: 0.38rem 0.65rem;
      color: #d7e7b7;
      background: rgba(113, 198, 180, 0.12);
      box-shadow: none;
      font: 0.72rem/1.1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      min-width: 4.25rem;
      transition: background 150ms ease, color 150ms ease, transform 150ms ease;
    }

    .zoom-action {
      min-width: 2.25rem;
      padding-inline: 0.48rem;
    }

    .zoom-reset {
      min-width: 3.65rem;
    }

    .zoom-action:disabled {
      opacity: 0.42;
    }

    .card-action.copied {
      color: #061612;
      background: var(--accent);
      transform: translateY(-1px);
    }

    .card-action.copy-failed {
      color: white;
      background: #991b1b;
    }

    .card-actions {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      flex-wrap: wrap;
      gap: 0.45rem;
    }

    .expandable-title {
      display: grid;
      grid-template-columns: 1fr auto 1fr;
      align-items: center;
    }

    .digit-actions {
      justify-content: center;
      grid-column: 2;
    }

    .top-card-copy {
      justify-self: end;
      grid-column: 3;
    }

    .top-card-actions {
      justify-self: end;
      grid-column: 3;
    }

    .value-title {
      display: grid;
      grid-template-columns: 1fr auto 1fr;
      align-items: center;
    }

    .precision-actions {
      justify-content: center;
      grid-column: 2;
    }

    .value-copy {
      justify-self: end;
      grid-column: 3;
    }

    .value-card-actions {
      justify-self: end;
      grid-column: 3;
    }

    pre {
      margin: 0;
      padding: 0.9rem;
      overflow: auto;
      min-height: 2.75rem;
    }

    .card pre,
    .card .rendered,
    .matrix-pretty {
      scrollbar-width: thin;
      scrollbar-color: rgba(227, 180, 87, 0.72) rgba(8, 29, 22, 0.62);
    }

    .card pre::-webkit-scrollbar,
    .card .rendered::-webkit-scrollbar,
    .matrix-pretty::-webkit-scrollbar {
      width: 0.72rem;
      height: 0.72rem;
    }

    .card pre::-webkit-scrollbar-track,
    .card .rendered::-webkit-scrollbar-track,
    .matrix-pretty::-webkit-scrollbar-track {
      border-radius: 999px;
      background:
        linear-gradient(180deg, rgba(8, 29, 22, 0.76), rgba(18, 53, 39, 0.58));
      box-shadow: inset 0 0 0 1px rgba(233, 244, 239, 0.12);
    }

    .card pre::-webkit-scrollbar-thumb,
    .card .rendered::-webkit-scrollbar-thumb,
    .matrix-pretty::-webkit-scrollbar-thumb {
      border: 2px solid rgba(8, 29, 22, 0.82);
      border-radius: 999px;
      background:
        linear-gradient(135deg, rgba(227, 180, 87, 0.94), rgba(113, 198, 180, 0.82));
      box-shadow: inset 0 0 0 1px rgba(255, 250, 220, 0.18);
    }

    .card pre::-webkit-scrollbar-thumb:hover,
    .card .rendered::-webkit-scrollbar-thumb:hover,
    .matrix-pretty::-webkit-scrollbar-thumb:hover {
      background:
        linear-gradient(135deg, rgba(247, 205, 112, 0.98), rgba(139, 222, 195, 0.92));
    }

    pre {
      color: var(--code);
      white-space: pre-wrap;
      font-family: "Cascadia Code", "DejaVu Sans Mono", monospace;
      font-size: var(--result-font-size);
      line-height: 1.45;
    }

    #value {
      overflow: auto;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      word-break: break-all;
    }

    .matrix-pretty {
      overflow-x: auto;
      overflow-y: visible;
      white-space: pre-wrap;
      font: 1.05rem/1.5 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .matrix-display {
      display: inline-grid;
      grid-template-columns: auto auto auto;
      align-items: stretch;
      gap: 0.45rem;
      color: var(--code);
      font-variant-ligatures: none;
    }

    .matrix-bracket {
      display: flex;
      align-items: center;
      color: #f0f5d6;
      font: 2.7rem/1 Georgia, "Times New Roman", serif;
      transform: scaleY(1.18);
    }

    .matrix-grid {
      display: grid;
      align-items: center;
      gap: 0.3rem 1.25rem;
      padding: 0.35rem 0.1rem;
    }

    .matrix-cell {
      min-width: 2.5rem;
      text-align: right;
      white-space: nowrap;
      font: 1.22rem/1.45 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .matrix-section-heading {
      color: var(--cream);
      font-weight: 800;
      letter-spacing: 0.08em;
      text-transform: lowercase;
    }

    .rendered {
      margin: 0;
      min-height: 12rem;
      padding: 2.1rem 1.6rem 3rem;
      overflow: auto;
      font-size: var(--render-font-size);
    }

    .rendered-zoom-frame {
      position: relative;
      display: inline-block;
      min-width: max-content;
      min-height: 1px;
    }

    .rendered svg {
      display: block;
      max-width: none;
      height: auto;
      overflow: visible;
      transform: scale(var(--render-zoom));
      transform-origin: left top;
      margin-bottom: var(--render-margin-bottom);
      filter: brightness(0) saturate(100%) invert(82%) sepia(39%) saturate(540%) hue-rotate(354deg) brightness(98%) contrast(92%) drop-shadow(0 0 0.65rem rgba(113, 198, 180, 0.28));
    }

    .error,
    .rendered.error,
    #rendered.error {
      color: #ffd99a !important;
      background:
        radial-gradient(circle at 14% 18%, rgba(229, 173, 87, 0.16), transparent 34%),
        linear-gradient(135deg, rgba(73, 23, 25, 0.88), rgba(38, 12, 19, 0.78)) !important;
      border-color: rgba(229, 173, 87, 0.42) !important;
      box-shadow:
        inset 0 0 0 1px rgba(255, 232, 181, 0.07),
        0 0 1.35rem rgba(153, 27, 27, 0.22) !important;
      text-shadow: 0 0 0.7rem rgba(255, 204, 112, 0.16) !important;
    }

    .rendered.error,
    #rendered.error {
      font-family: Georgia, "Times New Roman", serif;
    }

    .help-pane {
      padding: 1rem;
    }

    .help-card {
      padding: 1rem;
      border: 2px solid rgba(233, 244, 239, 0.24);
      border-radius: 18px;
      background: rgba(8, 29, 22, 0.62);
    }

    .help-card + .help-card {
      margin-top: 1rem;
    }

    .help-card h3 {
      margin: 0 0 0.65rem;
      color: var(--accent-2);
      font-size: 1rem;
      letter-spacing: 0.04em;
      text-transform: uppercase;
    }

    .help-card p {
      margin: 0 0 0.7rem;
      color: var(--muted);
    }

    .help-card ul {
      margin: 0;
      padding-left: 1.25rem;
    }

    .help-card li {
      margin: 0.35rem 0;
    }

    .help-card code {
      color: var(--code);
      font: 0.92rem/1.35 "Cascadia Code", "DejaVu Sans Mono", monospace;
      background: rgba(113, 198, 180, 0.12);
      border-radius: 6px;
      padding: 0.08rem 0.25rem;
    }

    .hidden {
      display: none;
    }

    .card.hidden {
      display: none !important;
    }

    @media (max-width: 900px) {
      body {
        font-size: 15px;
      }

      header {
        display: block;
        padding: 1rem 0.75rem 0.45rem;
      }

      .header-side {
        justify-items: start;
        margin-top: 0.5rem;
      }

      .status {
        text-align: left;
      }

      main {
        grid-template-columns: 1fr;
        gap: 0.75rem;
        padding: 0.5rem 0.75rem 1.25rem;
      }

      .lab-topbar {
        align-items: stretch;
      }

      .precision-toolbar {
        justify-content: flex-start;
      }

      section {
        border-radius: 17px;
      }

      .panel-head {
        padding: 0.7rem 0.8rem;
      }

      h2 {
        font-size: 0.82rem;
      }

      textarea {
        min-height: 8.5rem;
        padding: 0.85rem;
        font-size: 0.96rem;
        line-height: 1.45;
      }

      .integrator-bound-grid {
        grid-template-columns: 1fr;
      }

      .target-row {
        padding: 0 0.75rem 0.75rem;
      }

      .target-row input {
        min-height: 2.75rem;
      }

      .controls {
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
        gap: 0.5rem;
        padding: 0 0.75rem 0.75rem;
      }

      .controls button {
        width: 100%;
        min-height: 2.75rem;
        padding: 0.65rem 0.45rem;
      }

      .derivative-controls {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .variable-values {
        padding: 0 0.75rem 0.75rem;
      }

      .variable-value-box {
        grid-template-columns: minmax(2.2rem, auto) minmax(0, 1fr);
      }

      .variable-value-actions {
        grid-column: 1 / -1;
        flex-direction: row;
      }

      .variable-value-actions button {
        min-height: 2.5rem;
      }

      .variable-copy {
        display: none;
      }

      .output-grid {
        gap: 0.75rem;
        padding: 0.75rem;
      }

      .card {
        border-radius: 15px;
        --render-base-scale: 1.35;
        --render-zoom: 1.35;
        --result-base-font-rem: 0.82;
        --result-font-size: 0.82rem;
        --render-base-font-rem: 1.3;
        --render-font-size: 1.3rem;
        --render-base-margin-rem: 2.5;
        --render-margin-bottom: 2.5rem;
      }

      .mobile-result-extra {
        display: none;
      }

      #resultPane .copy-result {
        display: none;
      }

      .card-title {
        padding: 0.5rem 0.6rem;
        gap: 0.45rem;
        font-size: 0.68rem;
        letter-spacing: 0.08em;
      }

      .card-action {
        min-height: 2.25rem;
        padding: 0.45rem 0.55rem;
      }

      pre {
        padding: 0.75rem;
        font-size: 0.82rem;
      }

      .rendered {
        min-height: 8rem;
        padding: 1.35rem 1rem 2rem;
        font-size: var(--render-font-size);
      }

      .rendered svg {
        margin-bottom: var(--render-margin-bottom);
      }

      .mobile-panel {
        position: static;
        margin-top: 0.5rem;
        grid-template-columns: 1fr;
      }

      .mobile-actions {
        flex-direction: row;
        justify-content: space-between;
      }
    }

    @media (max-width: 560px) {
      h1 {
        font-size: 2rem;
      }

      .subtitle {
        display: none;
      }

      .mobile-card {
        width: 100%;
      }

      .mobile-card summary {
        padding: 0.38rem 0.68rem;
        font-size: 0.7rem;
      }

      .mobile-copy code {
        font-size: 0.78rem;
      }

      .mobile-actions {
        flex-wrap: wrap;
        justify-content: flex-start;
      }

      .mobile-qr {
        display: none;
      }

      main {
        padding: 0.45rem 0.5rem 1rem;
      }

      textarea {
        min-height: 7rem;
        font-size: 0.9rem;
      }

      .target-row {
        grid-template-columns: 1fr;
        gap: 0.35rem;
      }

      .goal-start-fields {
        display: grid;
        grid-template-columns: 1fr;
        gap: 0.35rem;
      }

      .controls {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .expandable-title,
      .value-title {
        grid-template-columns: 1fr;
      }

      .digit-actions,
      .precision-actions,
      .top-card-copy,
      .value-copy {
        grid-column: auto;
        justify-self: stretch;
        justify-content: center;
      }

      .digit-actions,
      .precision-actions {
        order: 3;
      }

      .top-card-copy,
      .value-copy {
        order: 2;
      }

      .precision-actions {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .rendered {
        min-height: 6.5rem;
        padding: 1rem 0.75rem 1.75rem;
        font-size: var(--render-font-size);
      }

      .rendered svg {
        transform: scale(var(--render-zoom));
        margin-bottom: var(--render-margin-bottom);
      }

      .help-pane {
        padding: 0.75rem;
      }

      .help-card {
        padding: 0.8rem;
      }
    }
__THEME_OVERRIDES__
  </style>
</head>
<body class="__BODY_CLASS__">
  <div class="celtic-backdrop" aria-hidden="true">
    <div class="aurora"></div>
    <div class="standing-stones">
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
    </div>
    <div class="chariot-wheel"></div>
  </div>
  <header>
    <div>
      <h1>__LAB_NAME__</h1>
      <p class="subtitle" id="subtitle">__LAB_SUBTITLE__</p>
    </div>
    <div class="header-side">
      <div class="status" id="status">Ready</div>
      <details class="mobile-card __MOBILE_CARD_CLASS__" id="mobileAccess">
        <summary>Mobile</summary>
        <div class="mobile-panel">
          <div class="mobile-copy">
            <strong id="mobileTitle">__MOBILE_TITLE__</strong>
            <span id="mobileHint">__MOBILE_HINT__</span>
            <code id="mobileUrl">__MOBILE_URL__</code>
          </div>
          <div class="mobile-actions">
            <div class="mobile-qr" id="mobileQr" aria-label="QR code for mobile access">__MOBILE_QR_SVG__</div>
            <button class="card-action copy-result" type="button" data-copy-target="mobile">Copy URL</button>
          </div>
        </div>
      </details>
    </div>
  </header>
  <main>
    <div class="lab-topbar">
      <div class="lab-tabs" role="tablist" aria-label="__LAB_NAME__ mode selector">
        <button class="mode-tab active" id="modeTabExpression" type="button" role="tab" aria-selected="true" aria-controls="workspacePanel" data-mode="expression">Expression</button>
        <button class="mode-tab" id="modeTabEquation" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="equation">Equation</button>
        <button class="mode-tab" id="modeTabMatrix" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="matrix">Matrix</button>
        <button class="mode-tab" id="modeTabIntegrator" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="integrator">Integrator</button>
      </div>
      <div class="precision-toolbar" aria-label="Precision controls">
        <span class="precision-label">Precision</span>
        <button class="card-action" id="lessPrecision" type="button">Less precision</button>
        <button class="card-action" id="morePrecision" type="button">More precision</button>
      </div>
    </div>
    <section id="workspacePanel">
      <div class="panel-head">
        <h2 id="leftPaneTitle">Workspace</h2>
      </div>
      <textarea id="expr" spellcheck="false" aria-labelledby="leftPaneTitle">__INITIAL_EXPRESSION__</textarea>
      <div class="mode-panel hidden" id="matrixControls">
        <label id="matrixOperationLabel" for="matrixOperation">Matrix operation</label>
        <select id="matrixOperation">
          <option value="eval">Evaluate</option>
          <option value="inverse" selected>Inverse</option>
          <option value="eigenvalues">Eigenvalues</option>
          <option value="eigendecompose">Eigendecompose</option>
          <option value="charpoly">Characteristic polynomial</option>
          <option value="det">Determinant</option>
          <option value="trace">Trace</option>
          <option value="rank">Rank</option>
          <option value="simplify">Simplify symbolic</option>
          <option value="solve">Solve A X = B</option>
        </select>
        <label class="hidden" for="matrixOperand" id="matrixOperandLabel">Right-hand side matrix</label>
        <textarea class="hidden secondary-editor" id="matrixOperand" spellcheck="false" placeholder="(1; 0)"></textarea>
      </div>
      <div class="mode-panel hidden" id="equationControls">
        <p class="mode-hint">Enter an equation such as <code>{ M = E - e*sin(E) | E = 1.5; M = 1.5, e = 0.0167 }</code>. Bindings after <code>|</code> decide which symbols are variables, which are constants, and which starting values numeric fallback should use.</p>
      </div>
      <div class="mode-panel hidden" id="integratorControls">
        <div class="integrator-bound-grid">
          <div class="integrator-bound-field integrator-var-field">
            <label for="integratorVariable">Variable</label>
            <input id="integratorVariable" spellcheck="false" autocomplete="off" value="x">
          </div>
          <div class="integrator-bound-field">
            <label for="integratorLowerBound">Lower bound</label>
            <input id="integratorLowerBound" spellcheck="false" autocomplete="off" value="0" placeholder="blank for none">
          </div>
          <div class="integrator-bound-field">
            <label for="integratorUpperBound">Upper bound</label>
            <input id="integratorUpperBound" spellcheck="false" autocomplete="off" value="1" placeholder="blank for none">
          </div>
        </div>
        <label for="integratorIntervalCap">Work budget ceiling</label>
        <select id="integratorIntervalCap">
          <option value="500">Up to 500</option>
          <option value="5000" selected>Up to 5,000</option>
          <option value="20000">Up to 20,000</option>
          <option value="50000">Up to 50,000</option>
          <option value="100000">Up to 100,000</option>
        </select>
        <p class="mode-hint">Leave both bounds blank for an antiderivative. Fill only the upper bound to evaluate the antiderivative there, or fill both bounds for a definite integral. Bounds can be numbers or symbols.</p>
      </div>
      <div class="target-row hidden" id="targetRow">
        <label for="goalTarget">Target</label>
        <input id="goalTarget" spellcheck="false" value="0">
      </div>
      <div class="controls">
        <button id="run">Evaluate</button>
        <button class="secondary" id="back">Back</button>
        <button class="secondary" id="forward">Forward</button>
        <button class="secondary" id="help">Help</button>
        <button class="secondary" id="goalSeek">Goal seek</button>
        <button class="secondary" id="clear">Clear</button>
      </div>
      <div class="controls derivative-controls" id="derivativeButtons"></div>
      <div class="variable-values hidden" id="variableValues"></div>
    </section>

    <section>
      <div class="panel-head">
        <h2 id="rightPaneTitle">Result</h2>
      </div>
      <div class="output-grid" id="resultPane">
        <div class="card result-card">
          <div class="card-title expandable-title">
            <span id="renderedTitle">Rendered TeX</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="renderedMore">Show more digits</button>
            </span>
            <span class="card-actions top-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" id="renderedCopy" data-copy-target="rendered">Copy</button>
            </span>
          </div>
          <div class="rendered" id="rendered"></div>
        </div>
        <div class="card result-card mobile-result-extra">
          <div class="card-title expandable-title">
            <span id="parsedTitle">Expression</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="parsedMore">Show more digits</button>
            </span>
            <span class="card-actions top-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" data-copy-target="expression">Copy</button>
            </span>
          </div>
          <pre id="parsed"></pre>
        </div>
        <div class="card result-card mobile-result-extra">
          <div class="card-title expandable-title">
            <span id="functionTitle">Function</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="functionMore">Show more digits</button>
            </span>
            <span class="card-actions top-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" data-copy-target="function">Copy</button>
            </span>
          </div>
          <pre id="functionStyle"></pre>
        </div>
        <div class="card result-card" id="valueCard">
          <div class="card-title value-title">
            <span id="valueTitle">Value</span>
            <span class="card-actions value-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" data-copy-target="value">Copy</button>
            </span>
          </div>
          <pre id="value"></pre>
        </div>
      </div>
      <div class="help-pane hidden" id="helpPane">
        <div class="help-card">
          <h3>Expression Shape</h3>
          <p>Type the expression body in the editor. Variable and constant bindings appear as editable boxes underneath.</p>
          <ul>
            <li><code>x/pi</code> becomes <code>{ x/π | x = ? }</code>, but the editor keeps showing <code>x/pi</code>.</li>
            <li>Set <code>x</code> to <code>pi/2</code> in the binding box instead of writing the binding into the editor.</li>
            <li>Raw expr binding syntax still works when you paste it, but the lab keeps the large editor focused on the expression body.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Bindings</h3>
          <p>Yellow boxes are variables and blue boxes are constants.</p>
          <ul>
            <li><code>x = ?</code> means unknown / <code>NAN</code>, so derivative buttons appear for <code>x</code>.</li>
            <li><code>; H = 163</code> marks <code>H</code> as a constant, so no derivative button is made for it.</li>
            <li>Binding values can use arithmetic: <code>3/2*pi</code>, <code>(pi^2)/2</code>, <code>3/2+pi/7+3*i/5</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Goal Seek</h3>
          <p>Goal seek changes variable bindings so the expression value reaches the value in the <code>Target</code> field.</p>
          <ul>
            <li>Leave a variable binding box blank to mark it as unknown.</li>
            <li>Constants are not changed by goal seek.</li>
            <li>If there is one variable, goal seek solves that variable directly.</li>
            <li>If there are several variables, goal seek moves them together by the smallest local step it can find.</li>
            <li>Current variable binding values become the starting point automatically.</li>
            <li>Use the binding boxes in the main editor to seed a particular crossing or branch, for example target <code>27</code> with expression <code>{ x^x | x = 3 }</code>.</li>
            <li>For several variables, set whichever variable bindings you want to seed. Blank or unknown bindings still fall back to the solver's defaults.</li>
            <li>It only reports success when <code>abs(value - target)</code> is within the current working precision.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Constants And Functions</h3>
          <ul>
            <li>Built-in constants include <code>pi</code>/<code>π</code>, <code>e</code>, <code>i</code>, <code>phi</code>/<code>φ</code>, and <code>gamma</code>/<code>γ</code>.</li>
            <li><code>ln(x)</code> is natural log; <code>log(x)</code>, <code>lg(x)</code>, and <code>log10(x)</code> are base-10 log.</li>
            <li><code>W(x)</code>, <code>W0(x)</code>, and <code>W_0(x)</code> mean <code>W₀(x)</code>. Use <code>W-1(x)</code> for <code>W₋₁(x)</code>.</li>
            <li>Standard gamma notation is supported in display: <code>gamma(x)</code> shows as <code>Γ(x)</code>, and polygamma shows as <code>ψ⁽ⁿ⁾(x)</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Unary Functions</h3>
          <ul>
            <li>Elementary: <code>abs(x)</code>, <code>floor(x)</code>, <code>ceil(x)</code>, <code>sqrt(x)</code>, <code>exp(x)</code>, <code>ln(x)</code>, <code>log(x)</code>, <code>lg(x)</code>, <code>log10(x)</code>.</li>
            <li>Trigonometric: <code>sin(x)</code>, <code>cos(x)</code>, <code>tan(x)</code>, <code>asin(x)</code>, <code>acos(x)</code>, <code>atan(x)</code>.</li>
            <li>Hyperbolic: <code>sinh(x)</code>, <code>cosh(x)</code>, <code>tanh(x)</code>, <code>asinh(x)</code>, <code>acosh(x)</code>, <code>atanh(x)</code>.</li>
            <li>Gamma family: <code>gamma(x)</code>, <code>gammainv(x)</code>, <code>lgamma(x)</code>, <code>digamma(x)</code>, <code>trigamma(x)</code>, <code>polygamma(n, x)</code>.</li>
            <li>Error functions: <code>erf(x)</code>, <code>erfc(x)</code>, <code>erfinv(x)</code>, <code>erfcinv(x)</code>.</li>
            <li>Exponential integrals: <code>Ei(x)</code>, <code>E1(x)</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Lambert W And Normal Functions</h3>
          <ul>
            <li>Principal Lambert W: <code>W(x)</code>, <code>W0(x)</code>, <code>W_0(x)</code>, <code>W₀(x)</code>, <code>productlog(x)</code>, <code>lambert_w0(x)</code>.</li>
            <li>Minus-one Lambert W branch: <code>W-1(x)</code>, <code>W_-1(x)</code>, <code>W₋₁(x)</code>, <code>lambert_wm1(x)</code>.</li>
            <li>Normal distribution: <code>normal_pdf(x)</code>, <code>normal_cdf(x)</code>, <code>normal_logpdf(x)</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Binary Functions</h3>
          <ul>
            <li><code>pow(x, y)</code> is equivalent to <code>x^y</code>.</li>
            <li><code>atan2(y, x)</code>, <code>hypot(x, y)</code>.</li>
            <li><code>beta(x, y)</code>, <code>logbeta(x, y)</code>, <code>binomial(n, k)</code>.</li>
            <li>Incomplete gamma: <code>gammainc_lower(s, x)</code>, <code>gammainc_upper(s, x)</code>, <code>gammainc_P(s, x)</code>, <code>gammainc_Q(s, x)</code>.</li>
            <li>Exact integer helpers: <code>gcd(a, b)</code>, <code>lcm(a, b)</code>, <code>mod(a, b)</code>, <code>modinv(a, b)</code>.</li>
            <li>Bitwise helpers use function syntax: <code>AND(a, b)</code>, <code>OR(a, b)</code>, <code>XOR(a, b)</code>, <code>NOT(a)</code>, <code>SHL(a, n)</code>, <code>SHR(a, n)</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Value-Only Helpers</h3>
          <ul>
            <li>Discrete functions evaluate normally but are not differentiable, so derivative buttons are hidden when they control the expression.</li>
            <li>Available helpers include <code>factorial(n)</code>, <code>n!</code>, <code>fibonacci(n)</code>, <code>partition(n)</code>, <code>isqrt(n)</code>, <code>is_prime(n)</code>, <code>next_prime(n)</code>, and <code>prev_prime(n)</code>.</li>
            <li><code>factors(n)</code> returns the original value and shows its factorisation as constant bindings, for example <code>factors(360)</code> becomes <code>{ a₀³·a₁²·a₂ |; a₀ = 2, a₁ = 3, a₂ = 5 }</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Distribution Functions</h3>
          <ul>
            <li>Normal distribution: <code>normal_pdf(x)</code>, <code>normal_cdf(x)</code>, <code>normal_logpdf(x)</code>.</li>
            <li>Beta distribution: <code>beta_pdf(x, a, b)</code>, <code>logbeta_pdf(x, a, b)</code>.</li>
          </ul>
        </div>
        <div class="help-card">
          <h3>Shortcuts</h3>
          <ul>
            <li><code>Ctrl+Enter</code> evaluates the expression.</li>
            <li>Derivative buttons appear from the current variable bindings.</li>
            <li>Enter the goal-seek target in the left pane's <code>Target</code> field.</li>
            <li>Use the binding boxes in the editor when goal seek needs better initial guesses.</li>
            <li><code>Goal seek</code> changes all variable bindings together to move the value towards that target.</li>
            <li>More/Less precision changes the displayed value precision without changing the expression.</li>
          </ul>
        </div>
      </div>
    </section>
  </main>

  <script>
    const expr = document.getElementById('expr');
    const subtitle = document.getElementById('subtitle');
    const leftPaneTitle = document.getElementById('leftPaneTitle');
    const modeTabs = Array.from(document.querySelectorAll('.mode-tab'));
    const matrixControls = document.getElementById('matrixControls');
    const matrixOperation = document.getElementById('matrixOperation');
    const matrixOperand = document.getElementById('matrixOperand');
    const matrixOperandLabel = document.getElementById('matrixOperandLabel');
    const equationControls = document.getElementById('equationControls');
    const equationVariable = null;
    const integratorControls = document.getElementById('integratorControls');
    const integratorVariable = document.getElementById('integratorVariable');
    const integratorLowerBound = document.getElementById('integratorLowerBound');
    const integratorUpperBound = document.getElementById('integratorUpperBound');
    const integratorIntervalCap = document.getElementById('integratorIntervalCap');
    const run = document.getElementById('run');
    const back = document.getElementById('back');
    const forward = document.getElementById('forward');
    const help = document.getElementById('help');
    const goalSeek = document.getElementById('goalSeek');
    const clear = document.getElementById('clear');
    const targetRow = document.getElementById('targetRow');
    const goalTarget = document.getElementById('goalTarget');
    const lessPrecision = document.getElementById('lessPrecision');
    const morePrecision = document.getElementById('morePrecision');
    const derivativeButtons = document.getElementById('derivativeButtons');
    const variableValues = document.getElementById('variableValues');
    const mobileAccess = document.getElementById('mobileAccess');
    const mobileTitle = document.getElementById('mobileTitle');
    const mobileHint = document.getElementById('mobileHint');
    const mobileUrl = document.getElementById('mobileUrl');
    const mobileQr = document.getElementById('mobileQr');
    const controlToken = __CONTROL_TOKEN__;
    enhanceRoundedSelect(matrixOperation);
    const statusEl = document.getElementById('status');
    const rightPaneTitle = document.getElementById('rightPaneTitle');
    const resultPane = document.getElementById('resultPane');
    const helpPane = document.getElementById('helpPane');
    const rendered = document.getElementById('rendered');
    const renderedTitle = document.getElementById('renderedTitle');
    const renderedCopy = document.getElementById('renderedCopy');
    const renderedMore = document.getElementById('renderedMore');
    const parsed = document.getElementById('parsed');
    const parsedTitle = document.getElementById('parsedTitle');
    const parsedMore = document.getElementById('parsedMore');
    const functionStyle = document.getElementById('functionStyle');
    const functionTitle = document.getElementById('functionTitle');
    const functionMore = document.getElementById('functionMore');
    const valueCard = document.getElementById('valueCard');
    const value = document.getElementById('value');
    const valueTitle = document.getElementById('valueTitle');
    const copyButtons = Array.from(document.querySelectorAll('.copy-result'));
    const moreDigitButtons = Array.from(document.querySelectorAll('.more-digits'));
    const resultCards = Array.from(document.querySelectorAll('.result-card'));
    const expandCardButtons = Array.from(document.querySelectorAll('[data-expand-card]'));
    const zoomButtons = Array.from(document.querySelectorAll('[data-zoom-step], [data-zoom-reset]'));
    const RESULT_ZOOM_LEVELS = [0.5, 0.67, 0.8, 1, 1.25, 1.5, 2, 3, 4, 6, 8];
    const RESULT_ZOOM_DEFAULT_INDEX = 3;
    let lastTex = '';
    let lastDerivativeExpression = '';
    let currentVariables = [];
    let currentBindingKinds = new Map();
    let currentDifferentiable = true;
    let expressionHistory = [];
    let forwardHistory = [];
    let workingPrecisionBits = 256;
    let fullExpressionText = '';
    let displayedExpressionText = '';

    if (controlToken && window.location.search.includes('__CONTROL_QUERY_PREFIX__')) {
      window.history.replaceState(null, '', window.location.pathname + window.location.hash);
    }
    let lastEvaluationInputText = '';
    let bindingValueCache = new Map();
    const DOUBLE_PRECISION_BITS = 53;
    const DOUBLE_PRECISION_DIGITS = 17;
    const QFLOAT_PRECISION_BITS = 106;
    const MAX_PRECISION_BITS = 1048576;
    const MODE_DEFAULT_PRECISION_BITS = {
      expression: 256,
      equation: 256,
      matrix: 256,
      integrator: DOUBLE_PRECISION_BITS
    };
    const modePrecisionBits = {
      expression: MODE_DEFAULT_PRECISION_BITS.expression,
      equation: MODE_DEFAULT_PRECISION_BITS.equation,
      matrix: MODE_DEFAULT_PRECISION_BITS.matrix,
      integrator: MODE_DEFAULT_PRECISION_BITS.integrator
    };
    const START_FORBIDDEN_PATTERN = /[=,;|{}]/;
    const COMPACT_BINDING_VALUE_LIMIT = 20;
    const COMPACT_BINDING_VALUE_KEEP = 16;
    const DEFAULT_EXPRESSION_TEXT = __DEFAULT_EXPRESSION__;
    const DEFAULT_EQUATION_TEXT = __DEFAULT_EQUATION__;
    const DEFAULT_EQUATION_VARIABLE_TEXT = __DEFAULT_EQUATION_VARIABLE__;
    const DEFAULT_MATRIX_TEXT = __DEFAULT_MATRIX__;
    const DEFAULT_INTEGRATOR_TEXT = __DEFAULT_INTEGRATOR__;
    const DEFAULT_INTEGRATOR_BOUNDS_TEXT = __DEFAULT_INTEGRATOR_BOUNDS__;
    const DEFAULT_INTEGRATOR_INTERVAL_CAP = __DEFAULT_INTEGRATOR_INTERVAL_CAP__;
    const LAB_MODE_STORAGE_KEY = 'mars.exprLab.lastMode';
    let currentLabMode = 'expression';
    const modeEditorText = {
      expression: DEFAULT_EXPRESSION_TEXT,
      equation: DEFAULT_EQUATION_TEXT,
      matrix: DEFAULT_MATRIX_TEXT,
      integrator: DEFAULT_INTEGRATOR_TEXT
    };
    const modeResultState = {
      expression: null,
      equation: null,
      matrix: null,
      integrator: null
    };

    function precisionDigitsForBits(bits) {
      if (bits <= DOUBLE_PRECISION_BITS)
        return DOUBLE_PRECISION_DIGITS;
      return Math.ceil(bits * Math.LOG10E * Math.LN2);
    }

    function requestedPrecisionBits() {
      const mode = currentMode();
      const bits = modePrecisionBits[mode] ?? workingPrecisionBits;
      return Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
    }

    function precisionStatusText() {
      const bits = requestedPrecisionBits();
      const digits = requestedValuePrecision();
      return `${digits} digits / ${bits} bits`;
    }

    function setStatus(text) {
      statusEl.textContent = `${text} · ${precisionStatusText()}`;
    }

    function currentMode() {
      return currentLabMode;
    }

    function syncModeTabs() {
      modeTabs.forEach((tab) => {
        const active = tab.dataset.mode === currentLabMode;
        tab.classList.toggle('active', active);
        tab.setAttribute('aria-selected', active ? 'true' : 'false');
        tab.tabIndex = active ? 0 : -1;
      });
    }

    function setMode(mode, options = {}) {
      const nextMode = mode === 'equation' || mode === 'matrix' || mode === 'integrator'
        ? mode
        : 'expression';
      const changed = nextMode !== currentLabMode;
      currentLabMode = nextMode;
      workingPrecisionBits = modePrecisionBits[currentLabMode] || workingPrecisionBits;
      syncModeTabs();
      if (!changed && !options.force)
        return false;
      return true;
    }

    function captureCurrentModeEditor() {
      const mode = currentMode();
      if (mode === 'expression')
        modeEditorText.expression = currentExpressionText() || expr.value.trim() || modeEditorText.expression;
      else if (mode === 'equation') {
        modeEditorText.equation = currentExpressionText() || expr.value.trim() || modeEditorText.equation;
        saveLastEquationState();
      }
      else if (mode === 'matrix') {
        modeEditorText.matrix = expr.value.trim() || modeEditorText.matrix;
        saveLastMatrixState();
      } else {
        modeEditorText.integrator = expr.value.trim() || modeEditorText.integrator;
        saveLastIntegratorState();
      }
    }

    function restoreModeEditor(mode) {
      if (mode === 'expression') {
        setExpressionEditor(modeEditorText.expression || DEFAULT_EXPRESSION_TEXT);
      } else if (mode === 'equation') {
        setExpressionEditor(modeEditorText.equation || DEFAULT_EQUATION_TEXT);
      } else if (mode === 'matrix') {
        expr.value = modeEditorText.matrix || DEFAULT_MATRIX_TEXT;
        clearExpressionSource();
      } else {
        expr.value = modeEditorText.integrator || DEFAULT_INTEGRATOR_TEXT;
        clearExpressionSource();
      }
    }

    function setResultTitles(renderedText, parsedText, functionText, valueText) {
      renderedTitle.textContent = renderedText;
      parsedTitle.textContent = parsedText;
      functionTitle.textContent = functionText;
      valueTitle.textContent = valueText;
    }

    function setValueCardVisible(visible) {
      if (!valueCard)
        return;
      if (!visible && valueCard.classList.contains('expanded-card'))
        collapseResultCards();
      valueCard.classList.toggle('hidden', !visible);
    }

    function snapshotElementState(element) {
      return {
        className: element.className,
        style: element.style.cssText,
        innerHTML: element.innerHTML,
        dataset: {...element.dataset}
      };
    }

    function restoreElementState(element, state) {
      element.className = state.className || '';
      element.style.cssText = state.style || '';
      element.innerHTML = state.innerHTML || '';
      Object.keys(element.dataset).forEach((key) => {
        delete element.dataset[key];
      });
      Object.entries(state.dataset || {}).forEach(([key, value]) => {
        element.dataset[key] = value;
      });
    }

    function snapshotButtonState(button) {
      return {
        className: button.className,
        textContent: button.textContent,
        disabled: !!button.disabled,
        dataset: {...button.dataset}
      };
    }

    function restoreButtonState(button, state) {
      button.className = state.className || '';
      button.textContent = state.textContent || '';
      button.disabled = !!state.disabled;
      Object.keys(button.dataset).forEach((key) => {
        delete button.dataset[key];
      });
      Object.entries(state.dataset || {}).forEach(([key, value]) => {
        button.dataset[key] = value;
      });
    }

    function hasResultContent() {
      return Boolean(
        rendered.innerHTML.trim() ||
        parsed.textContent.trim() ||
        functionStyle.textContent.trim() ||
        value.textContent.trim()
      );
    }

    function saveCurrentModeResultState(mode = currentMode()) {
      if (!hasResultContent()) {
        modeResultState[mode] = null;
        return;
      }

      modeResultState[mode] = {
        rendered: snapshotElementState(rendered),
        parsed: snapshotElementState(parsed),
        functionStyle: snapshotElementState(functionStyle),
        value: snapshotElementState(value),
        renderedMore: snapshotButtonState(renderedMore),
        parsedMore: snapshotButtonState(parsedMore),
        functionMore: snapshotButtonState(functionMore),
        lastTex,
        lastDerivativeExpression,
        currentVariables: [...currentVariables],
        currentDifferentiable
      };
    }

    function restoreModeResultState(mode = currentMode()) {
      const state = modeResultState[mode];
      if (!state) {
        clearResultPane();
        return;
      }

      collapseResultCards();
      restoreElementState(rendered, state.rendered);
      restoreElementState(parsed, state.parsed);
      restoreElementState(functionStyle, state.functionStyle);
      restoreElementState(value, state.value);
      restoreButtonState(renderedMore, state.renderedMore);
      restoreButtonState(parsedMore, state.parsedMore);
      restoreButtonState(functionMore, state.functionMore);
      lastTex = state.lastTex || '';
      lastDerivativeExpression = state.lastDerivativeExpression || '';
      currentVariables = Array.isArray(state.currentVariables) ? [...state.currentVariables] : [];
      currentDifferentiable = state.currentDifferentiable !== false;
      renderDerivativeButtons(currentVariables);
    }

    function syncMatrixControls() {
      syncRoundedSelect(matrixOperation);
      const needsOperand = currentMode() === 'matrix' && matrixOperation.value === 'solve';
      matrixOperand.classList.toggle('hidden', !needsOperand);
      matrixOperandLabel.classList.toggle('hidden', !needsOperand);
    }

    function syncModeUI() {
      const mode = currentMode();
      const expressionMode = mode === 'expression';
      const equationMode = mode === 'equation';
      const matrixMode = mode === 'matrix';
      const integratorMode = mode === 'integrator';

      matrixControls.classList.toggle('hidden', !matrixMode);
      equationControls.classList.toggle('hidden', !equationMode);
      integratorControls.classList.toggle('hidden', !integratorMode);
      targetRow.classList.toggle('hidden', !expressionMode || targetRow.classList.contains('hidden'));
      derivativeButtons.classList.toggle('hidden', !expressionMode);
      goalSeek.classList.toggle('hidden', !expressionMode);

      if (expressionMode) {
        leftPaneTitle.textContent = 'Expression';
        subtitle.textContent = 'Switch between expression, equation, matrix, and integrator experiments. Each mode runs through a local MARS scratch binary and shows the result on the right.';
        setResultTitles('Rendered TeX', 'Expression', 'Function', 'Value');
        setValueCardVisible(true);
      } else if (equationMode) {
        leftPaneTitle.textContent = 'Equation';
        subtitle.textContent = 'Enter an equation on the left. The lab tries symbolic isolation first, then numeric solving for all variable bindings.';
        setResultTitles('Rendered TeX', 'Equation', 'Solutions', 'Details');
        setValueCardVisible(true);
      } else if (matrixMode) {
        leftPaneTitle.textContent = 'Matrix';
        subtitle.textContent = 'Enter a matrix expression on the left, choose an operation, and inspect both the formatted result and the raw matrix output.';
        setResultTitles('Rendered TeX', 'Result', 'Layout', 'Summary');
        setValueCardVisible(false);
      } else {
        leftPaneTitle.textContent = 'Integrator';
        subtitle.textContent = 'Enter an integrand expression on the left, then type the lower and upper bounds separately. Leave both blank for an antiderivative, or leave lower blank and fill upper to evaluate the antiderivative there.';
        setResultTitles('Rendered TeX', 'Integrand', 'Exact result', 'Integral');
        setValueCardVisible(true);
      }

      syncMatrixControls();
      updateHistoryButtons();
    }

    function applyLabMode(mode) {
      setMode(validLabMode(mode), {force: true});
      restoreModeEditor(currentMode());
      syncModeUI();
      if (currentMode() === 'integrator' && !currentIntegratorBound().name)
        resetIntegratorBoundsToDefault();
      if (currentMode() === 'integrator' && integratorIntervalCap)
        integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
    }

    function showResults() {
      resultPane.classList.remove('hidden');
      helpPane.classList.add('hidden');
      rightPaneTitle.textContent = 'Result';
      help.textContent = 'Help';
    }

    function showHelp() {
      resultPane.classList.add('hidden');
      helpPane.classList.remove('hidden');
      rightPaneTitle.textContent = 'Help';
      help.textContent = 'Result';
      setStatus('Help');
    }

    function toggleHelp() {
      if (helpPane.classList.contains('hidden'))
        showHelp();
      else {
        showResults();
        setStatus('Ready');
      }
    }

    function variableNamesFromBindings(bindings) {
      return (Array.isArray(bindings) ? bindings : [])
        .filter((binding) => String(binding.kind || 'variable') !== 'constant')
        .map((binding) => String(binding.name || '').trim())
        .filter(Boolean);
    }

    function showTargetEntry() {
      targetRow.classList.remove('hidden');
      goalSeek.textContent = 'Run goal seek';
      goalTarget.focus();
      goalTarget.select();
      setStatus('Enter target');
    }

    function hideTargetEntry() {
      targetRow.classList.add('hidden');
      goalSeek.textContent = 'Goal seek';
    }

    function syncRoundedSelect(select) {
      if (select && typeof select.__marsSyncRoundedSelect === 'function')
        select.__marsSyncRoundedSelect();
    }

    function enhanceRoundedSelect(select) {
      if (!select)
        return null;

      const label = document.querySelector(`label[for="${select.id}"]`);
      const shell = document.createElement('div');
      const button = document.createElement('button');
      const menu = document.createElement('div');

      shell.className = 'select-shell';
      button.type = 'button';
      button.className = 'select-button';
      button.setAttribute('aria-haspopup', 'listbox');
      button.setAttribute('aria-expanded', 'false');
      if (label && label.id)
        button.setAttribute('aria-labelledby', label.id);
      else
        button.setAttribute('aria-label', 'Select option');

      menu.className = 'select-menu hidden';
      menu.setAttribute('role', 'listbox');

      select.classList.add('select-native-source');
      select.tabIndex = -1;
      select.setAttribute('aria-hidden', 'true');
      select.parentNode.insertBefore(shell, select);
      shell.appendChild(select);
      shell.appendChild(button);
      shell.appendChild(menu);

      const optionButtons = Array.from(select.options).map((option) => {
        const item = document.createElement('button');
        item.type = 'button';
        item.className = 'select-option';
        item.setAttribute('role', 'option');
        item.dataset.value = option.value;
        item.textContent = option.textContent;
        item.addEventListener('click', () => {
          const changed = select.value !== option.value;
          select.value = option.value;
          sync();
          close();
          button.focus();
          if (changed)
            select.dispatchEvent(new Event('change', {bubbles: true}));
        });
        menu.appendChild(item);
        return item;
      });

      function selectedOption() {
        return select.selectedOptions[0] || select.options[select.selectedIndex] || select.options[0];
      }

      function sync() {
        const selected = selectedOption();
        button.textContent = selected ? selected.textContent : '';
        optionButtons.forEach((item) => {
          const selectedItem = item.dataset.value === select.value;
          item.classList.toggle('selected', selectedItem);
          item.setAttribute('aria-selected', selectedItem ? 'true' : 'false');
        });
      }

      function close() {
        shell.classList.remove('open');
        button.setAttribute('aria-expanded', 'false');
        menu.classList.add('hidden');
      }

      function open() {
        sync();
        shell.classList.add('open');
        button.setAttribute('aria-expanded', 'true');
        menu.classList.remove('hidden');
      }

      function focusSelectedOption() {
        const selected = optionButtons.find((item) => item.dataset.value === select.value);
        (selected || optionButtons[0] || button).focus();
      }

      function focusRelativeOption(step) {
        if (!optionButtons.length)
          return;
        const currentIndex = optionButtons.indexOf(document.activeElement);
        const selectedIndex = optionButtons.findIndex((item) => item.dataset.value === select.value);
        const index = currentIndex >= 0 ? currentIndex : Math.max(0, selectedIndex);
        const nextIndex = (index + step + optionButtons.length) % optionButtons.length;
        optionButtons[nextIndex].focus();
      }

      button.addEventListener('click', () => {
        if (shell.classList.contains('open'))
          close();
        else
          open();
      });

      button.addEventListener('keydown', (event) => {
        if (event.key === 'ArrowDown' || event.key === 'Enter' || event.key === ' ') {
          event.preventDefault();
          open();
          focusSelectedOption();
        } else if (event.key === 'Escape') {
          close();
        }
      });

      menu.addEventListener('keydown', (event) => {
        if (event.key === 'Escape') {
          event.preventDefault();
          close();
          button.focus();
        } else if (event.key === 'ArrowDown') {
          event.preventDefault();
          focusRelativeOption(1);
        } else if (event.key === 'ArrowUp') {
          event.preventDefault();
          focusRelativeOption(-1);
        }
      });

      if (label)
        label.addEventListener('click', (event) => {
          event.preventDefault();
          button.focus();
        });

      document.addEventListener('click', (event) => {
        if (!shell.contains(event.target))
          close();
      });

      select.addEventListener('change', sync);
      select.__marsSyncRoundedSelect = sync;
      sync();
      return {sync, close};
    }

    function splitTopLevel(text, separator) {
      const parts = [];
      let start = 0;
      let depth = 0;
      for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '(' || ch === '[' || ch === '{') depth++;
        else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
        else if (ch === separator && depth === 0) {
          parts.push(text.slice(start, i));
          start = i + 1;
        }
      }
      parts.push(text.slice(start));
      return parts;
    }

    function indexOfTopLevel(text, needle) {
      let depth = 0;
      for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '(' || ch === '[' || ch === '{') depth++;
        else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
        else if (ch === needle && depth === 0) return i;
      }
      return -1;
    }

    function lastIndexOfTopLevel(text, needle) {
      let depth = 0;
      let found = -1;
      for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '(' || ch === '[' || ch === '{') depth++;
        else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
        else if (ch === needle && depth === 0) found = i;
      }
      return found;
    }

    function variablesFromExpression(text) {
      text = String(text || '').trim();
      if (text.startsWith('{') && text.endsWith('}')) {
        text = text.slice(1, -1).trim();
      }

      const pipe = lastIndexOfTopLevel(text, '|');
      if (pipe < 0) return [];

      let bindings = text.slice(pipe + 1).trim();
      const semi = indexOfTopLevel(bindings, ';');
      if (semi >= 0) bindings = bindings.slice(0, semi);

      return splitTopLevel(bindings, ',')
        .map((part) => {
          const eq = indexOfTopLevel(part, '=');
          return eq >= 0 ? part.slice(0, eq).trim() : '';
        })
        .filter(Boolean);
    }

    function compareBindingNames(left, right) {
      const leftName = String((left && left.name) || left || '');
      const rightName = String((right && right.name) || right || '');
      return leftName.localeCompare(rightName, undefined, {numeric: true, sensitivity: 'base'}) ||
        leftName.localeCompare(rightName);
    }

    function sortedAssignmentParts(parts) {
      return [...(parts || [])].sort((left, right) => {
        const leftEq = indexOfTopLevel(left, '=');
        const rightEq = indexOfTopLevel(right, '=');
        const leftName = leftEq >= 0 ? left.slice(0, leftEq).trim() : String(left || '').trim();
        const rightName = rightEq >= 0 ? right.slice(0, rightEq).trim() : String(right || '').trim();
        return compareBindingNames(leftName, rightName);
      });
    }

    function expressionWithSortedConstants(text) {
      const normalized = expressionForEditor(text).trim();
      const parts = bindingParts(normalized);
      if (!parts || !parts.constants)
        return normalized;

      const variableAssignments = splitTopLevel(parts.variables, ',')
        .map((part) => part.trim())
        .filter(Boolean);
      const constantAssignments = sortedAssignmentParts(
        splitTopLevel(parts.constants, ',')
          .map((part) => part.trim())
          .filter(Boolean)
      );

      let bindingText = variableAssignments.join(', ');
      if (constantAssignments.length) {
        const constants = constantAssignments.join(', ');
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      }

      return `{ ${parts.body} | ${bindingText} }`;
    }

    function canGoalSeek() {
      return currentVariables.length > 0;
    }

    function derivativeExpressionFromLine(line) {
      const match = String(line || '').match(/^d\/d[^=]*=\s*(.+)$/);
      return match ? match[1].trim() : '';
    }

    function expressionForEvaluation(text) {
      return String(text || '').replace(/(=\s*)\?/g, '$1NAN');
    }

    function expressionForEditor(text) {
      return String(text || '').replace(/(=\s*)NAN\b/g, '$1?');
    }

    function restoreCompactBindingValues(text) {
      text = String(text || '').trim();
      if (!text.includes('...'))
        return text;

      const parts = bindingParts(text);
      if (!parts)
        return text;

      const restoreValues = new Map(bindingValueCache);
      const fullParts = bindingParts(expr.dataset.fullExpression || fullExpressionText);
      if (fullParts) {
        [fullParts.variables, fullParts.constants].forEach((assignmentsText) => {
          splitTopLevel(assignmentsText, ',').forEach((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return;

            const name = part.slice(0, eq).trim();
            const value = part.slice(eq + 1).trim();
            if (name && value && !restoreValues.has(name))
              restoreValues.set(name, value);
          });
        });
      }

      if (restoreValues.size === 0)
        return text;

      let changed = false;
      function restoreAssignments(assignmentsText) {
        return splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return part.trim();

            const name = part.slice(0, eq).trim();
            const valueText = part.slice(eq + 1).trim();
            const cached = restoreValues.get(name);
            if (cached && valueText.endsWith('...') && cached.startsWith(valueText.slice(0, -3))) {
              changed = true;
              return `${name} = ${cached}`;
            }
            return part.trim();
          })
          .filter(Boolean)
          .join(', ');
      }

      const variables = restoreAssignments(parts.variables);
      const constants = restoreAssignments(parts.constants);
      let bindingText = variables;
      if (constants)
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;

      return changed ? `{ ${parts.body} | ${bindingText} }` : text;
    }

    function currentExpressionText() {
      const text = expr.value.trim();
      const full = expr.dataset.fullExpression || fullExpressionText;
      const compact = expr.dataset.displayExpression || displayedExpressionText;
      if (full && compact && text === compact)
        return full;
      if (text.includes('...'))
        return restoreCompactBindingValues(text);
      return expressionWithEditorBody(text);
    }

    function expressionWithEditorBody(bodyText) {
      const body = String(bodyText || '').trim();
      if (!body)
        return '';
      if (bindingParts(body))
        return body;
      if (currentMode() === 'equation')
        return body;

      const full = expr.dataset.fullExpression || fullExpressionText;
      const parts = bindingParts(full);
      if (!parts)
        return body;

      let bindingText = parts.variables;
      if (parts.constants)
        bindingText = bindingText ? `${bindingText}; ${parts.constants}` : `; ${parts.constants}`;
      return bindingText ? `{ ${body} | ${bindingText} }` : body;
    }

    function expressionBodyForEditor(fullText) {
      const text = expressionForEditor(fullText).trim();
      const parts = bindingParts(text);
      return parts ? parts.body : text;
    }

    function clearExpressionSource() {
      fullExpressionText = '';
      displayedExpressionText = '';
      lastEvaluationInputText = '';
      bindingValueCache = new Map();
      delete expr.dataset.fullExpression;
      delete expr.dataset.displayExpression;
      clearGoalSeekRequest();
    }

    function clearGoalSeekRequest() {
      delete expr.dataset.goalSeekSource;
      delete expr.dataset.goalSeekTarget;
    }

    function currentGoalSeekSource() {
      const source = expr.dataset.goalSeekSource || '';

      if (!source)
        return '';
      if (expr.value.trim() !== (expr.dataset.displayExpression || displayedExpressionText))
        return '';
      return source;
    }

    function expressionHasSolvedVariables(text) {
      const parts = bindingParts(text);

      if (!parts)
        return false;

      return splitTopLevel(parts.variables, ',').some((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return false;

        const valueText = part.slice(eq + 1).trim();
        return valueText && valueText !== '?' && !/^NAN$/i.test(valueText);
      });
    }

    function bindingParts(text) {
      text = String(text || '').trim();
      const wrapped = text.startsWith('{') && text.endsWith('}');
      if (wrapped)
        text = text.slice(1, -1).trim();

      const pipe = lastIndexOfTopLevel(text, '|');
      if (pipe < 0)
        return null;

      const body = text.slice(0, pipe).trim();
      const bindings = text.slice(pipe + 1).trim();
      const semi = indexOfTopLevel(bindings, ';');
      const variables = semi >= 0 ? bindings.slice(0, semi).trim() : bindings;
      const constants = semi >= 0 ? bindings.slice(semi + 1).trim() : '';
      return {wrapped, body, variables, constants};
    }

    function compactBindingValue(valueText) {
      const text = String(valueText || '').trim();
      if (!text || text === '?' || /^NAN$/i.test(text))
        return {display: text, shortened: false};
      if (text.includes('...'))
        return {display: text, shortened: false};
      if (text.length <= COMPACT_BINDING_VALUE_LIMIT)
        return {display: text, shortened: false};
      return {display: compactLongNumericTokens(text), shortened: true};
    }

    function compactLongNumericTokens(text) {
      return String(text || '').replace(
        /(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)/g,
        (match, prefix, numberText) => {
          if (numberText.includes('...') || numberText.length <= COMPACT_BINDING_VALUE_LIMIT)
            return match;
          return `${prefix}${numberText.slice(0, COMPACT_BINDING_VALUE_KEEP)}...`;
        }
      );
    }

    function compactExpressionForEditor(fullText) {
      const full = expressionForEditor(fullText);
      const parts = bindingParts(full);
      if (!parts) {
        const display = compactLongNumericTokens(full);
        return {display, bindings: [], shortened: display !== full};
      }

      const bindingValues = [];
      let shortened = false;
      const body = compactLongNumericTokens(parts.body);
      shortened = shortened || body !== parts.body;

      function compactAssignments(assignmentsText, kind) {
        const rows = splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0) {
              const text = part.trim();
              return text ? {name: text, text, bind: false} : null;
            }

            const name = part.slice(0, eq).trim();
            const valueText = part.slice(eq + 1).trim();
            const compact = compactBindingValue(valueText);
            shortened = shortened || compact.shortened;
            return name
              ? {name, value: valueText, display: compact.display, kind, text: `${name} = ${compact.display}`, bind: true}
              : null;
          })
          .filter(Boolean);

        if (kind === 'constant')
          rows.sort(compareBindingNames);
        rows.forEach((row) => {
          if (row.bind && row.name)
            bindingValues.push({
              name: row.name,
              value: row.value || '',
              display: row.display || '',
              kind
            });
        });
        return rows.map((row) => row.text);
      }

      const variableAssignments = compactAssignments(parts.variables, 'variable');
      const constantAssignments = compactAssignments(parts.constants, 'constant');

      let bindingText = variableAssignments.join(', ');
      if (constantAssignments.length) {
        const constants = constantAssignments.join(', ');
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      }

      return {
        display: `{ ${body} | ${bindingText} }`,
        bindings: bindingValues,
        shortened
      };
    }

    function replaceBindingValueInExpression(sourceExpression, kind, targetName, valueText) {
      const parts = bindingParts(sourceExpression);
      if (!parts || !targetName)
        return sourceExpression;

      let changed = false;
      function replaceAssignments(assignmentsText, shouldReplace) {
        return splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return part.trim();

            const name = part.slice(0, eq).trim();
            if (!shouldReplace || name !== targetName)
              return part.trim();

            changed = true;
            return `${name} = ${valueText}`;
          })
          .filter(Boolean)
          .join(', ');
      }

      const variables = replaceAssignments(parts.variables, kind !== 'constant');
      const constants = replaceAssignments(parts.constants, kind === 'constant');
      if (!changed)
        return sourceExpression;

      let bindingText = variables;
      if (constants)
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      return `{ ${parts.body} | ${bindingText} }`;
    }

    function normalisedBindingInputValue(input) {
      const text = String(input.value || '').trim();
      return text || '?';
    }

    function commitBindingInput(input) {
      const name = input.dataset.bindingName || '';
      const kind = input.dataset.bindingKind || 'variable';
      const valueText = normalisedBindingInputValue(input);
      const current = currentExpressionText();
      const updated = replaceBindingValueInExpression(current, kind, name, valueText);

      input.value = (valueText === '?' || /^NAN$/i.test(valueText)) ? '' : valueText;
      input.title = valueText;

      if (updated === current)
        return;

      setExpressionEditor(updated);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      saveLastExpression(updated);
    }

    function displayValueForBinding(binding) {
      const value = String(binding.value || binding.display || '').trim();
      return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
    }

    function displayEquationValue(valueText) {
      const value = String(valueText || '').trim();
      return /^NAN$/i.test(value) ? 'unresolved' : value;
    }

    function fullValueForBinding(binding) {
      const value = String(binding.value || binding.display || '').trim();
      return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
    }

    function clearVariableValues() {
      variableValues.replaceChildren();
      variableValues.classList.add('hidden');
      currentBindingKinds = new Map();
    }

    function refreshVariableValuesFromEditor() {
      const compact = compactExpressionForEditor(currentExpressionText());
      renderVariableValues(compact.bindings || []);
    }

    function renderVariableValues(bindings) {
      variableValues.replaceChildren();
      bindingValueCache = new Map();
      currentBindingKinds = new Map();
      if (!bindings.length) {
        variableValues.classList.add('hidden');
        return;
      }

      const variableBindings = [];
      const constantBindings = [];
      bindings.forEach((binding) => {
        const kind = binding.kind || 'variable';
        if (kind === 'constant')
          constantBindings.push(binding);
        else
          variableBindings.push(binding);
      });
      constantBindings.sort(compareBindingNames);

      [...variableBindings, ...constantBindings].forEach((binding) => {
        const kind = binding.kind || 'variable';
        currentBindingKinds.set(binding.name, kind);
        const displayValue = displayValueForBinding(binding);
        const fullValue = fullValueForBinding(binding);
        if (kind !== 'constant' && fullValue)
          bindingValueCache.set(binding.name, fullValue);

        const box = document.createElement('div');
        box.className = kind === 'constant'
          ? 'variable-value-box constant-value-box'
          : 'variable-value-box';

        const name = document.createElement('span');
        name.className = kind === 'constant'
          ? 'variable-value-name constant-value-name'
          : 'variable-value-name';
        name.textContent = binding.name;

        const text = document.createElement('input');
        text.className = 'variable-value-text binding-value-input';
        text.type = 'text';
        text.value = displayValue;
        text.title = fullValue || binding.value || '?';
        text.dataset.bindingName = binding.name;
        text.dataset.bindingKind = kind;
        text.autocomplete = 'off';
        text.spellcheck = false;
        text.addEventListener('keydown', (event) => {
          if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
            event.preventDefault();
            commitBindingInput(text);
            evaluateFromKeyboard();
          } else if (event.key === 'Enter') {
            event.preventDefault();
            text.blur();
          } else if (event.key === 'Escape') {
            event.preventDefault();
            text.value = displayValue;
            text.blur();
          }
        });
        text.addEventListener('change', () => commitBindingInput(text));

        const actions = document.createElement('div');
        actions.className = 'variable-value-actions';

        const copy = document.createElement('button');
        copy.className = 'card-action variable-copy';
        copy.type = 'button';
        copy.textContent = 'Copy';
        copy.addEventListener('click', async () => {
          try {
            await writeClipboardText(text.value);
            flashCopyButton(copy, true);
            setStatus(`Copied ${binding.name}`);
            setTimeout(() => setStatus('Ready'), 1000);
          } catch (err) {
            flashCopyButton(copy, false);
            setStatus(String(err));
          }
        });

        actions.appendChild(copy);
        box.append(name, text, actions);
        variableValues.appendChild(box);
      });

      variableValues.classList.remove('hidden');
    }

    function assignmentValuesByName(assignmentsText) {
      const values = new Map();
      splitTopLevel(assignmentsText || '', ',').forEach((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return;

        const name = part.slice(0, eq).trim();
        const valueText = part.slice(eq + 1).trim();
        if (name && valueText)
          values.set(name, valueText);
      });
      return values;
    }

    function solvedStartValuesForGoalSeek(sourceExpression, solvedExpression, providedStart = {}) {
      const start = {...providedStart};
      const sourceParts = bindingParts(sourceExpression);
      const solvedParts = bindingParts(solvedExpression);
      if (!sourceParts || !solvedParts)
        return start;

      const sourceVariables = new Set(assignmentValuesByName(sourceParts.variables).keys());
      const solvedVariables = assignmentValuesByName(solvedParts.variables);
      sourceVariables.forEach((name) => {
        const cachedValue = bindingValueCache.get(name);
        const solvedValue = cachedValue || solvedVariables.get(name) || '';
        if (!solvedValue || solvedValue === '?' || /^NAN$/i.test(solvedValue))
          return;
        start[name] = solvedValue;
      });

      return start;
    }

    function goalSeekExpressionAndStarts(sourceExpression, providedStart = {}) {
      const parts = bindingParts(sourceExpression);
      const start = {...providedStart};

      if (!parts)
        return {expression: sourceExpression, start};

      let changed = false;
      const variables = splitTopLevel(parts.variables, ',')
        .map((part) => {
          const eq = indexOfTopLevel(part, '=');
          if (eq < 0)
            return part.trim();

          const name = part.slice(0, eq).trim();
          const valueText = part.slice(eq + 1).trim();
          if (!name)
            return part.trim();

          if (valueText && valueText !== '?' && !/^NAN$/i.test(valueText) && !start[name])
            start[name] = valueText;

          changed = changed || valueText !== '?';
          return `${name} = ?`;
        })
        .filter(Boolean)
        .join(', ');

      let bindingText = variables;
      if (parts.constants)
        bindingText = bindingText ? `${bindingText}; ${parts.constants}` : `; ${parts.constants}`;

      return {
        expression: changed ? `{ ${parts.body} | ${bindingText} }` : sourceExpression,
        start
      };
    }

    function setExpressionEditor(fullText, evaluatedBindings = null, editorBodyText = null) {
      const compact = compactExpressionForEditor(fullText);
      const editorBody = editorBodyText === null || editorBodyText === undefined
        ? expressionBodyForEditor(fullText)
        : expressionForEditor(editorBodyText).trim();
      fullExpressionText = expressionForEditor(fullText).trim();
      displayedExpressionText = editorBody;
      expr.dataset.fullExpression = fullExpressionText;
      expr.dataset.displayExpression = displayedExpressionText;
      expr.value = displayedExpressionText;
      const bindings = (Array.isArray(evaluatedBindings) && evaluatedBindings.length)
        ? evaluatedBindings
        : compact.bindings;
      renderVariableValues(bindings || []);
      currentVariables = variableNamesFromBindings(bindings || []);
      renderDerivativeButtons(currentVariables);
    }

    function validPrecisionBits(bits, fallback) {
      const parsed = parseInt(String(bits), 10);
      if (!Number.isFinite(parsed))
        return fallback;
      return Math.max(DOUBLE_PRECISION_DIGITS, Math.min(MAX_PRECISION_BITS, parsed));
    }

    function validIntegratorIntervalCap(value) {
      const parsed = parseInt(String(value), 10);
      if (!Number.isFinite(parsed))
        return DEFAULT_INTEGRATOR_INTERVAL_CAP;
      const allowed = [500, 5000, 20000, 50000, 100000];
      return allowed.includes(parsed) ? parsed : DEFAULT_INTEGRATOR_INTERVAL_CAP;
    }

    function validMatrixOperation(value) {
      const operation = String(value || '').trim();
      const allowed = Array.from(matrixOperation.options).map((option) => option.value);
      return allowed.includes(operation) ? operation : 'inverse';
    }

    function validLabMode(value) {
      const mode = String(value || '').trim();
      return mode === 'equation' || mode === 'matrix' || mode === 'integrator' ? mode : 'expression';
    }

    function applySavedState(data) {
      const saved = String(data.expression || '').trim();
      if (saved && !saved.includes('...')) {
        modeEditorText.expression = saved;
        setExpressionEditor(saved);
      }

      const savedMatrix = String(data.matrix || '').trim();
      if (savedMatrix && !savedMatrix.includes('...'))
        modeEditorText.matrix = savedMatrix;

      const savedEquation = String(data.equation || '').trim();
      if (savedEquation && !savedEquation.includes('...'))
        modeEditorText.equation = expressionWithSortedConstants(savedEquation);

      const savedEquationVariable = String(data.equation_variable || '').trim();
      if (equationVariable)
        equationVariable.value = savedEquationVariable || DEFAULT_EQUATION_VARIABLE_TEXT;

      const savedMatrixOperation = validMatrixOperation(data.matrix_operation);
      if (matrixOperation)
        matrixOperation.value = savedMatrixOperation;

      const savedMatrixOperand = String(data.matrix_operand || '').trim();
      if (matrixOperand)
        matrixOperand.value = savedMatrixOperand;

      const savedIntegrator = String(data.integrator_expression || '').trim();
      if (savedIntegrator && !savedIntegrator.includes('...')) {
        modeEditorText.integrator = savedIntegrator;
        expr.dataset.savedIntegratorExpression = savedIntegrator;
      }

      const savedBounds = String(data.integrator_bounds || '').trim();
      if (savedBounds)
        restoreIntegratorBoundsText(savedBounds);

      const savedCap = validIntegratorIntervalCap(data.integrator_interval_cap);
      if (integratorIntervalCap)
        integratorIntervalCap.value = String(savedCap);

      if (data.precision_bits && typeof data.precision_bits === 'object') {
        Object.entries(data.precision_bits).forEach(([mode, bits]) => {
          if (modePrecisionBits[mode] !== undefined)
            modePrecisionBits[mode] = validPrecisionBits(bits, modePrecisionBits[mode]);
        });
      } else if (data.precision_bits !== undefined) {
        modePrecisionBits.expression = validPrecisionBits(data.precision_bits, modePrecisionBits.expression);
      }
      workingPrecisionBits = modePrecisionBits[currentMode()] || workingPrecisionBits;

      applyLabMode(validLabMode(data.lab_mode));
    }

    async function loadLastState() {
      try {
        const response = await fetch('/state');
        const data = await response.json();
        applySavedState(data || {});
        if (String(data.expression || '').trim())
          return;
      } catch (_) {
        // Fall back to localStorage below.
      }

      try {
        const saved = localStorage.getItem('mars.exprLab.lastExpression');
        if (saved && !saved.includes('...'))
          setExpressionEditor(saved);
        const matrixText = localStorage.getItem('mars.exprLab.lastMatrix');
        if (matrixText && !matrixText.includes('...'))
          modeEditorText.matrix = matrixText;
        const matrixOperationText = localStorage.getItem('mars.exprLab.lastMatrixOperation');
        if (matrixOperation && matrixOperationText)
          matrixOperation.value = validMatrixOperation(matrixOperationText);
        const matrixOperandText = localStorage.getItem('mars.exprLab.lastMatrixOperand');
        if (matrixOperand && matrixOperandText !== null)
          matrixOperand.value = matrixOperandText;
        const equationText = localStorage.getItem('mars.exprLab.lastEquation');
        if (equationText && !equationText.includes('...'))
          modeEditorText.equation = expressionWithSortedConstants(equationText);
        const equationVariableText = localStorage.getItem('mars.exprLab.lastEquationVariable');
        if (equationVariable && equationVariableText)
          equationVariable.value = equationVariableText;
        const integratorExpression = localStorage.getItem('mars.exprLab.lastIntegratorExpression');
        if (integratorExpression && !integratorExpression.includes('...'))
          modeEditorText.integrator = integratorExpression;
        const integratorBoundsText = localStorage.getItem('mars.exprLab.lastIntegratorBounds');
        if (integratorBoundsText)
          restoreIntegratorBoundsText(integratorBoundsText);
        const integratorCap = localStorage.getItem('mars.exprLab.lastIntegratorIntervalCap');
        if (integratorIntervalCap && integratorCap)
          integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorCap));
        const labMode = localStorage.getItem(LAB_MODE_STORAGE_KEY);
        if (labMode)
          applyLabMode(labMode);
      } catch (_) {
        // Private browsing or locked-down webviews can disable localStorage.
      }
    }

    function saveLabState(patch) {
      const payload = {...patch};
      fetch('/state', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
      }).catch(() => {
        // Persistence is helpful, not essential.
      });
    }

    function savePrecisionState() {
      saveLabState({precision_bits: modePrecisionBits});
    }

    function saveLastLabMode(mode = currentMode()) {
      const labMode = validLabMode(mode);
      try {
        localStorage.setItem(LAB_MODE_STORAGE_KEY, labMode);
      } catch (_) {
        // The lab still works fine without persistence.
      }
      saveLabState({lab_mode: labMode});
    }

    function saveLastExpression(text) {
      text = String(text || '').trim();
      if (text.includes('...') && fullExpressionText)
        text = fullExpressionText;
      if (text.includes('...'))
        return;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastExpression', text);
      } catch (_) {
        // The lab still works fine without persistence.
      }

      if (!text)
        return;

      saveLabState({
        expression: text,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastMatrixState() {
      const text = String(expr.value || '').trim();
      const operation = validMatrixOperation(matrixOperation && matrixOperation.value);
      const operand = String(matrixOperand && matrixOperand.value || '').trim();
      if (text)
        modeEditorText.matrix = text;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastMatrix', text);
        localStorage.setItem('mars.exprLab.lastMatrixOperation', operation);
        localStorage.setItem('mars.exprLab.lastMatrixOperand', operand);
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        matrix: text,
        matrix_operation: operation,
        matrix_operand: operand,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastEquationState() {
      const text = expressionWithSortedConstants(String(currentExpressionText() || expr.value || '').trim());
      if (text)
        modeEditorText.equation = text;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastEquation', text);
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        equation: text,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastIntegratorState() {
      const text = String(expr.value || '').trim();
      const bounds = currentIntegratorBoundsText();
      const cap = requestedIntegratorIntervalCap();
      if (text)
        modeEditorText.integrator = text;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastIntegratorExpression', text);
        if (bounds)
          localStorage.setItem('mars.exprLab.lastIntegratorBounds', bounds);
        localStorage.setItem('mars.exprLab.lastIntegratorIntervalCap', String(cap));
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        integrator_expression: text,
        integrator_bounds: bounds,
        integrator_interval_cap: cap,
        precision_bits: modePrecisionBits
      });
    }

    function setBusy(isBusy) {
      const expressionMode = currentMode() === 'expression';
      run.disabled = isBusy;
      back.disabled = isBusy || !expressionMode || expressionHistory.length === 0;
      forward.disabled = isBusy || !expressionMode || forwardHistory.length === 0;
      goalSeek.disabled = isBusy || !expressionMode || !canGoalSeek();
      goalSeek.title = goalSeek.disabled && !isBusy && expressionMode
        ? 'Goal seek needs at least one variable binding'
        : '';
      goalTarget.disabled = isBusy;
      lessPrecision.disabled = isBusy || atMinimumPrecision();
      morePrecision.disabled = isBusy || atMaximumPrecision();
      morePrecision.title = !isBusy && atMaximumPrecision()
        ? 'Already at the current maximum precision setting'
        : '';
      [integratorVariable, integratorLowerBound, integratorUpperBound].forEach((input) => {
        if (input)
          input.disabled = isBusy;
      });
      if (equationVariable)
        equationVariable.disabled = isBusy;
      if (integratorIntervalCap)
        integratorIntervalCap.disabled = isBusy;
      copyButtons.forEach((button) => {
        button.disabled = isBusy;
      });
      moreDigitButtons.forEach((button) => {
        button.disabled = isBusy;
      });
      Array.from(variableValues.querySelectorAll('button')).forEach((button) => {
        button.disabled = isBusy;
      });
      Array.from(variableValues.querySelectorAll('input')).forEach((input) => {
        input.disabled = isBusy;
      });
      Array.from(derivativeButtons.querySelectorAll('button')).forEach((button) => {
        button.disabled = isBusy;
      });
    }

    function updateHistoryButtons() {
      const expressionMode = currentMode() === 'expression';
      back.disabled = !expressionMode || expressionHistory.length === 0;
      forward.disabled = !expressionMode || forwardHistory.length === 0;
      lessPrecision.disabled = atMinimumPrecision();
      morePrecision.disabled = atMaximumPrecision();
      goalSeek.disabled = !expressionMode || !canGoalSeek();
      goalSeek.title = goalSeek.disabled && expressionMode
        ? 'Goal seek needs at least one variable binding'
        : '';
      morePrecision.title = atMaximumPrecision()
        ? 'Already at the current maximum precision setting'
        : '';
    }

    function pushExpressionHistory(text) {
      const previous = expressionHistory[expressionHistory.length - 1];
      if (text && text !== previous) {
        expressionHistory.push(text);
      }
      forwardHistory = [];
      updateHistoryButtons();
    }

    function renderDerivativeButtons(variables) {
      derivativeButtons.replaceChildren();
      if (!currentDifferentiable) return;
      variables.forEach((name) => {
        const button = document.createElement('button');
        button.className = 'secondary';
        button.type = 'button';
        button.textContent = `${name} derivative`;
        button.addEventListener('click', () => takeDerivative(name));
        derivativeButtons.appendChild(button);
      });
    }

    async function fetchEvaluation(text, wrt = '') {
      const precision = requestedValuePrecision();
      const response = await fetch('/eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({expression: expressionForEvaluation(text), wrt, precision})
      });
      const data = await response.json();
      return {response, data};
    }

    function parseIntegratorBoundsText(text) {
      const bounds = [];
      for (const rawLine of String(text || '').split(/\n+/)) {
        const line = rawLine.trim();
        if (!line)
          continue;
        let match = line.match(/^([^:=]+?)\s*(?:=|:)\s*(.+?)\s*\.\.\s*(.+)$/);
        if (match) {
          bounds.push({
            name: match[1].trim(),
            lo: match[2].trim(),
            hi: match[3].trim(),
          });
          continue;
        }
        match = line.match(/^([^:=]+?)\s*(?:=|:)\s*(.+)$/);
        if (match) {
          bounds.push({
            name: match[1].trim(),
            lo: '',
            hi: match[2].trim(),
          });
          continue;
        }
        if (!/[=:]/.test(line) && !line.includes('..')) {
          bounds.push({
            name: line.trim(),
            lo: '',
            hi: '',
          });
          continue;
        }
        throw new Error(`Bad bound line: ${line}`);
      }
      if (bounds.length === 0)
        bounds.push({name: 'x', lo: '', hi: ''});
      return bounds;
    }

    function firstIntegratorBoundFromText(text) {
      try {
        const bounds = parseIntegratorBoundsText(text);
        return bounds[0] || {name: 'x', lo: '', hi: ''};
      } catch (_) {
        return {name: 'x', lo: '0', hi: '1'};
      }
    }

    function setIntegratorBoundInputs(bound) {
      const safeBound = bound || {name: 'x', lo: '', hi: ''};
      if (integratorVariable)
        integratorVariable.value = String(safeBound.name || 'x').trim() || 'x';
      if (integratorLowerBound)
        integratorLowerBound.value = cleanIntegratorBoundValue(safeBound.lo);
      if (integratorUpperBound)
        integratorUpperBound.value = cleanIntegratorBoundValue(safeBound.hi);
    }

    function cleanIntegratorBoundValue(value) {
      const text = String(value || '').trim();
      return /^blank\s+for\s+(none|antiderivative)$/i.test(text) ? '' : text;
    }

    function cleanIntegratorBoundInput(input) {
      if (!input)
        return;
      const cleaned = cleanIntegratorBoundValue(input.value);
      if (cleaned !== String(input.value || '').trim())
        input.value = cleaned;
    }

    function cleanIntegratorBoundInputs() {
      cleanIntegratorBoundInput(integratorLowerBound);
      cleanIntegratorBoundInput(integratorUpperBound);
    }

    function applyIntegratorResultBound(data) {
      if (!data)
        return;
      if (!Object.prototype.hasOwnProperty.call(data, 'bound_var') &&
          !Object.prototype.hasOwnProperty.call(data, 'bound_lower') &&
          !Object.prototype.hasOwnProperty.call(data, 'bound_upper'))
        return;
      const currentBound = currentIntegratorBound();
      const nextBound = {
        name: String(data.bound_var || currentBound.name || 'x').trim() || 'x',
        lo: currentBound.lo || String(data.bound_lower || '').trim(),
        hi: currentBound.hi || String(data.bound_upper || '').trim(),
      };
      setIntegratorBoundInputs(nextBound);
    }

    function restoreIntegratorBoundsText(text) {
      setIntegratorBoundInputs(firstIntegratorBoundFromText(text || DEFAULT_INTEGRATOR_BOUNDS_TEXT));
    }

    function currentIntegratorBound() {
      cleanIntegratorBoundInputs();
      return {
        name: String(integratorVariable && integratorVariable.value || 'x').trim() || 'x',
        lo: cleanIntegratorBoundValue(integratorLowerBound && integratorLowerBound.value),
        hi: cleanIntegratorBoundValue(integratorUpperBound && integratorUpperBound.value),
      };
    }

    function integratorBoundsTextFromBound(bound) {
      const name = String(bound.name || 'x').trim() || 'x';
      const lo = String(bound.lo || '').trim();
      const hi = String(bound.hi || '').trim();
      if (lo && hi)
        return `${name} = ${lo} .. ${hi}`;
      if (hi)
        return `${name} = ${hi}`;
      return name;
    }

    function currentIntegratorBoundsText() {
      return integratorBoundsTextFromBound(currentIntegratorBound());
    }

    function resetIntegratorBoundsToDefault() {
      restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT);
    }

    function requestedIntegratorIntervalCap() {
      const raw = parseInt(String(integratorIntervalCap && integratorIntervalCap.value || DEFAULT_INTEGRATOR_INTERVAL_CAP), 10);
      if (!Number.isFinite(raw))
        return DEFAULT_INTEGRATOR_INTERVAL_CAP;
      return raw;
    }

    async function fetchMatrixEvaluation() {
      saveLastMatrixState();
      const response = await fetch('/matrix-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          matrix: expr.value.trim(),
          operation: matrixOperation.value,
          operand: matrixOperand.value.trim(),
          precision: requestedValuePrecision()
        })
      });
      const data = await response.json();
      return {response, data};
    }

    async function fetchEquationEvaluation() {
      saveLastEquationState();
      const equationText = currentExpressionText() || expr.value.trim();
      const response = await fetch('/equation-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          equation: equationText,
          precision: requestedValuePrecision()
        })
      });
      const data = await response.json();
      return {response, data};
    }

    async function fetchIntegratorEvaluation() {
      const bound = currentIntegratorBound();
      if (bound.lo && !bound.hi)
        throw new Error('A one-sided bound should be entered as an upper bound. Leave lower blank and put the value in upper.');
      const bounds = [bound];
      saveLastIntegratorState();
      const response = await fetch('/integrator-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          expression: expr.value.trim(),
          bounds,
          precision: requestedValuePrecision(),
          max_intervals: requestedIntegratorIntervalCap()
        })
      });
      const data = await response.json();
      return {response, data};
    }

    function estimateValuePrecision() {
      const style = getComputedStyle(value);
      const canvas = estimateValuePrecision.canvas || document.createElement('canvas');
      const context = canvas.getContext('2d');
      const padLeft = parseFloat(style.paddingLeft) || 0;
      const padRight = parseFloat(style.paddingRight) || 0;
      let charWidth = 9;

      estimateValuePrecision.canvas = canvas;
      if (context) {
        context.font = style.font;
        charWidth = context.measureText('0123456789'.repeat(8)).width / 80 || charWidth;
      }

      const usableWidth = Math.max(0, value.clientWidth - padLeft - padRight);
      const chars = Math.floor(usableWidth / charWidth);

      return Math.max(96, Math.min(220, chars - 3));
    }

    function requestedValuePrecision() {
      return precisionDigitsForBits(requestedPrecisionBits());
    }

    function atMinimumPrecision() {
      return requestedPrecisionBits() <= DOUBLE_PRECISION_BITS;
    }

    function atMaximumPrecision() {
      return requestedPrecisionBits() >= MAX_PRECISION_BITS;
    }

    function setRequestedPrecisionBits(bits) {
      const mode = currentMode();
      const clamped = Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
      modePrecisionBits[mode] = clamped;
      workingPrecisionBits = clamped;
    }

    function nextPrecisionStepBits(current) {
      if (current < QFLOAT_PRECISION_BITS)
        return QFLOAT_PRECISION_BITS;
      if (current < 256)
        return 256;
      return Math.min(MAX_PRECISION_BITS, Math.ceil((current + 1) / 128) * 128);
    }

    function previousPrecisionStepBits(current) {
      if (current <= QFLOAT_PRECISION_BITS)
        return DOUBLE_PRECISION_BITS;
      if (current <= 256)
        return QFLOAT_PRECISION_BITS;
      return Math.max(256, Math.floor((current - 1) / 128) * 128);
    }

    function copyTextForTarget(target) {
      if (target === 'rendered') return rendered.classList.contains('error') ? rendered.textContent : lastTex;
      if (target === 'expression') return currentMode() === 'expression' ? (fullExpressionText || parsed.textContent) : parsed.textContent;
      if (target === 'function') return functionStyle.dataset.fullText || functionStyle.textContent;
      if (target === 'value') return value.textContent;
      if (target === 'mobile') {
        const url = mobileUrl ? mobileUrl.textContent.trim() : '';
        return /^https?:\/\//.test(url) ? url : '';
      }
      return '';
    }

    async function refreshMobileAccess() {
      if (!mobileAccess || !mobileUrl || !mobileQr)
        return;

      try {
        const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
        const response = await fetch('/mobile-access', {cache: 'no-store', headers});
        if (!response.ok)
          return;
        const data = await response.json();
        const url = String(data.url || '');
        const canControl = Boolean(data.control);
        mobileAccess.classList.remove('hidden');
        if (mobileTitle)
          mobileTitle.textContent = String(data.title || 'Mobile access');
        if (mobileHint)
          mobileHint.textContent = String(data.hint || '');
        mobileUrl.textContent = url || 'Unavailable';
        mobileQr.innerHTML = String(data.qr || '');
      } catch (err) {
        // Network state changes are expected; keep the last known QR until the next poll.
      }
    }

    function resetMoreDigitsButton(button, canExpand) {
      button.classList.toggle('hidden', !canExpand);
      button.textContent = 'Show more digits';
      button.dataset.expanded = 'false';
    }

    function hasAbbreviatedValue(text) {
      return String(text || '').includes('...');
    }

    function resultZoomIndex(card) {
      const raw = Number(card && card.dataset.zoomIndex);
      if (Number.isFinite(raw))
        return Math.max(0, Math.min(RESULT_ZOOM_LEVELS.length - 1, Math.round(raw)));
      return RESULT_ZOOM_DEFAULT_INDEX;
    }

    function svgLengthPixels(value) {
      const match = String(value || '').trim().match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(px|pt|pc|in|cm|mm)?$/i);
      if (!match)
        return 0;

      const amount = Number.parseFloat(match[1]);
      if (!Number.isFinite(amount))
        return 0;

      const unit = String(match[2] || 'px').toLowerCase();
      if (unit === 'pt') return amount * 96 / 72;
      if (unit === 'pc') return amount * 16;
      if (unit === 'in') return amount * 96;
      if (unit === 'cm') return amount * 96 / 2.54;
      if (unit === 'mm') return amount * 96 / 25.4;
      return amount;
    }

    function svgIntrinsicSize(svg) {
      const attrWidth = svgLengthPixels(svg.getAttribute('width'));
      const attrHeight = svgLengthPixels(svg.getAttribute('height'));
      let viewWidth = 0;
      let viewHeight = 0;
      if (svg.viewBox && svg.viewBox.baseVal) {
        viewWidth = svg.viewBox.baseVal.width;
        viewHeight = svg.viewBox.baseVal.height;
      }

      const previousTransform = svg.style.transform;
      svg.style.transform = 'none';
      const rect = svg.getBoundingClientRect();
      svg.style.transform = previousTransform;

      return {
        width: Math.max(1, attrWidth || rect.width || viewWidth || 1),
        height: Math.max(1, attrHeight || rect.height || viewHeight || 1)
      };
    }

    function updateRenderedZoomFrame(card, scale) {
      const frame = card ? card.querySelector('.rendered-zoom-frame') : null;
      const svg = frame ? frame.querySelector('svg') : null;
      if (!frame || !svg)
        return;

      if (!svg.dataset.baseWidth || !svg.dataset.baseHeight) {
        const intrinsic = svgIntrinsicSize(svg);
        svg.dataset.baseWidth = String(intrinsic.width);
        svg.dataset.baseHeight = String(intrinsic.height);
      }

      const width = Number(svg.dataset.baseWidth) || 1;
      const height = Number(svg.dataset.baseHeight) || 1;
      frame.style.width = `${width * scale}px`;
      frame.style.height = `${height * scale}px`;
      svg.style.width = `${width}px`;
      svg.style.height = `${height}px`;
      svg.style.transform = `scale(${scale})`;
    }

    function applyResultZoom(card) {
      if (!card)
        return;

      const index = resultZoomIndex(card);
      const zoom = RESULT_ZOOM_LEVELS[index];
      const computed = getComputedStyle(card);
      const renderBase = Number.parseFloat(computed.getPropertyValue('--render-base-scale')) || 2;
      const textBase = Number.parseFloat(computed.getPropertyValue('--result-base-font-rem')) || 0.92;
      const renderFontBase = Number.parseFloat(computed.getPropertyValue('--render-base-font-rem')) || 1.78;
      const marginBase = Number.parseFloat(computed.getPropertyValue('--render-base-margin-rem')) || 5;
      const renderScale = renderBase * zoom;

      card.dataset.zoomIndex = String(index);
      card.style.setProperty('--result-zoom', String(zoom));
      card.style.setProperty('--result-font-size', `${textBase * zoom}rem`);
      card.style.setProperty('--render-font-size', `${renderFontBase * zoom}rem`);
      card.style.setProperty('--render-zoom', String(renderScale));
      card.style.setProperty('--render-margin-bottom', `${marginBase * Math.max(1, zoom)}rem`);
      updateRenderedZoomFrame(card, renderScale);
      card.querySelectorAll('[data-zoom-reset]').forEach((button) => {
        button.textContent = `${Math.round(zoom * 100)}%`;
        button.setAttribute('aria-label', `Reset zoom from ${Math.round(zoom * 100)}%`);
      });
      card.querySelectorAll('[data-zoom-step="-1"]').forEach((button) => {
        button.disabled = index <= 0;
      });
      card.querySelectorAll('[data-zoom-step="1"]').forEach((button) => {
        button.disabled = index >= RESULT_ZOOM_LEVELS.length - 1;
      });
    }

    function setResultZoom(card, index) {
      if (!card)
        return;
      card.dataset.zoomIndex = String(Math.max(0, Math.min(RESULT_ZOOM_LEVELS.length - 1, index)));
      applyResultZoom(card);
    }

    function stepResultZoom(card, direction) {
      setResultZoom(card, resultZoomIndex(card) + (direction < 0 ? -1 : 1));
    }

    function collapseResultCards() {
      resultPane.classList.remove('card-expanded');
      document.querySelectorAll('.result-card.expanded-card')
        .forEach((card) => card.classList.remove('expanded-card'));
      expandCardButtons.forEach((button) => {
        button.textContent = 'Expand';
        button.setAttribute('aria-expanded', 'false');
      });
    }

    function toggleResultCardExpansion(button) {
      const card = button.closest('.result-card');
      if (!card)
        return;

      const isExpanded = card.classList.contains('expanded-card');
      collapseResultCards();
      if (isExpanded)
        return;

      resultPane.classList.add('card-expanded');
      card.classList.add('expanded-card');
      button.textContent = 'Collapse';
      button.setAttribute('aria-expanded', 'true');
    }

    function setRenderedContent(svg, fallbackText = '') {
      const card = rendered.closest('.result-card');
      rendered.replaceChildren();
      if (svg) {
        const frame = document.createElement('div');
        frame.className = 'rendered-zoom-frame';
        frame.innerHTML = svg;
        rendered.appendChild(frame);
      } else {
        rendered.textContent = fallbackText;
      }
      if (card)
        requestAnimationFrame(() => applyResultZoom(card));
    }

    function clearRenderedError() {
      rendered.classList.remove('error');
      rendered.style.color = '';
      rendered.style.background = '';
      rendered.style.borderColor = '';
      rendered.style.boxShadow = '';
      rendered.style.textShadow = '';
      rendered.style.fontFamily = '';
    }

    function setRenderedError(message) {
      rendered.replaceChildren();
      rendered.textContent = message || 'Evaluation failed';
      rendered.classList.add('error');
      rendered.style.color = '#ffd99a';
      rendered.style.background =
        'radial-gradient(circle at 14% 18%, rgba(229, 173, 87, 0.16), transparent 34%), ' +
        'linear-gradient(135deg, rgba(73, 23, 25, 0.88), rgba(38, 12, 19, 0.78))';
      rendered.style.borderColor = 'rgba(229, 173, 87, 0.42)';
      rendered.style.boxShadow =
        'inset 0 0 0 1px rgba(255, 232, 181, 0.07), 0 0 1.35rem rgba(153, 27, 27, 0.22)';
      rendered.style.textShadow = '0 0 0.7rem rgba(255, 204, 112, 0.16)';
      rendered.style.fontFamily = 'Georgia, "Times New Roman", serif';
    }

    function renderMatrixSectionHeadings(element, text) {
      const source = String(text || '');
      const lines = source.split('\n');
      const hasHeadings = lines.some((line) => /^(?:\s*)(eigenvalues|eigenvectors)(?:\s*)$/i.test(line));
      if (!hasHeadings) {
        element.textContent = source;
        return;
      }

      element.replaceChildren();
      lines.forEach((line, index) => {
        const match = line.match(/^(\s*)(eigenvalues|eigenvectors)(\s*)$/i);
        if (match) {
          element.appendChild(document.createTextNode(match[1]));
          const heading = document.createElement('span');
          heading.className = 'matrix-section-heading';
          heading.textContent = match[2].toLowerCase();
          element.appendChild(heading);
          element.appendChild(document.createTextNode(match[3]));
        } else {
          element.appendChild(document.createTextNode(line));
        }
        if (index + 1 < lines.length)
          element.appendChild(document.createTextNode('\n'));
      });
    }

    function setExpandableText(element, button, displayText, fullText) {
      renderMatrixSectionHeadings(element, displayText || fullText || '');
      element.dataset.displayText = displayText || '';
      element.dataset.fullText = fullText || '';
      resetMoreDigitsButton(
        button,
        !!fullText && !!displayText && fullText !== displayText && hasAbbreviatedValue(displayText)
      );
    }

    function parseMatrixResultText(text) {
      const source = String(text || '').trim();
      if (!source.startsWith('(') || !source.endsWith(')'))
        return null;

      const body = source.slice(1, -1).trim();
      if (!body)
        return [[]];

      const rows = splitTopLevel(body, ';')
        .map((row) => splitTopLevel(row, ',').map((cell) => cell.trim()));
      if (!rows.length)
        return null;

      const cols = rows[0].length;
      if (!cols || rows.some((row) => row.length !== cols))
        return null;
      return rows;
    }

    function setMatrixPrettyResult(resultText, prettyText) {
      const rows = parseMatrixResultText(resultText);
      functionStyle.classList.add('matrix-pretty');
      functionStyle.dataset.displayText = prettyText || resultText || '';
      functionStyle.dataset.fullText = prettyText || resultText || '';
      resetMoreDigitsButton(functionMore, false);

      if (!rows) {
        renderMatrixSectionHeadings(functionStyle, prettyText || resultText || '');
        return;
      }

      functionStyle.replaceChildren();
      const display = document.createElement('span');
      display.className = 'matrix-display';

      const left = document.createElement('span');
      left.className = 'matrix-bracket';
      left.textContent = '(';
      display.appendChild(left);

      const grid = document.createElement('span');
      grid.className = 'matrix-grid';
      grid.style.gridTemplateColumns = `repeat(${rows[0].length}, max-content)`;
      rows.forEach((row) => {
        row.forEach((cellText) => {
          const cell = document.createElement('span');
          cell.className = 'matrix-cell';
          cell.textContent = cellText;
          grid.appendChild(cell);
        });
      });
      display.appendChild(grid);

      const right = document.createElement('span');
      right.className = 'matrix-bracket';
      right.textContent = ')';
      display.appendChild(right);

      functionStyle.appendChild(display);
    }

    function setRenderedResult(data) {
      const displayTex = data.display_tex || data.tex || '';
      const fullDisplayTex = data.full_display_tex || data.tex || '';

      clearRenderedError();
      lastTex = data.tex || '';
      rendered.dataset.displayTex = displayTex;
      rendered.dataset.fullTex = fullDisplayTex;
      rendered.dataset.displaySvg = data.svg || '';
      rendered.dataset.fullSvg = '';
      rendered.dataset.renderError = data.render_error || '';
      setRenderedContent(data.svg || '', data.render_error || 'No rendered TeX available');
      resetMoreDigitsButton(
        renderedMore,
        !!fullDisplayTex &&
          !!displayTex &&
          fullDisplayTex !== displayTex &&
          hasAbbreviatedValue(displayTex)
      );
    }

    async function renderTexSvg(tex) {
      const response = await fetch('/render_tex', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({tex})
      });
      const data = await response.json();
      if (!response.ok || !data.ok)
        throw new Error(data.error || 'Could not render TeX');
      return data;
    }

    function toggleTextDigits(element, button) {
      const expanded = button.dataset.expanded === 'true';
      if (expanded) {
        renderMatrixSectionHeadings(element, element.dataset.displayText || element.textContent);
        button.textContent = 'Show more digits';
        button.dataset.expanded = 'false';
      } else {
        renderMatrixSectionHeadings(element, element.dataset.fullText || element.textContent);
        button.textContent = 'Show fewer digits';
        button.dataset.expanded = 'true';
      }
    }

    async function toggleRenderedDigits() {
      const expanded = renderedMore.dataset.expanded === 'true';

      if (expanded) {
        setRenderedContent(rendered.dataset.displaySvg || '', rendered.dataset.renderError || '');
        renderedMore.textContent = 'Show more digits';
        renderedMore.dataset.expanded = 'false';
        return;
      }

      if (!rendered.dataset.fullSvg) {
        renderedMore.disabled = true;
        setStatus('Rendering full TeX...');
        try {
          const data = await renderTexSvg(rendered.dataset.fullTex || lastTex);
          rendered.dataset.fullSvg = data.svg || '';
          rendered.dataset.fullRenderError = data.render_error || '';
        } catch (err) {
          rendered.dataset.fullRenderError = String(err);
        } finally {
          renderedMore.disabled = false;
          setStatus('Ready');
        }
      }

      setRenderedContent(
        rendered.dataset.fullSvg || '',
        rendered.dataset.fullRenderError || 'No rendered TeX available'
      );
      renderedMore.textContent = 'Show fewer digits';
      renderedMore.dataset.expanded = 'true';
    }

    async function writeClipboardText(text) {
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        return;
      }

      const area = document.createElement('textarea');
      area.value = text;
      area.setAttribute('readonly', '');
      area.style.position = 'fixed';
      area.style.left = '-9999px';
      area.style.top = '0';
      document.body.appendChild(area);
      area.select();
      const ok = document.execCommand('copy');
      document.body.removeChild(area);
      if (!ok)
        throw new Error('Copy was blocked by the browser');
    }

    function flashCopyButton(button, ok) {
      const original = button.dataset.originalLabel || button.textContent;
      button.dataset.originalLabel = original;
      button.classList.remove('copied', 'copy-failed');
      button.classList.add(ok ? 'copied' : 'copy-failed');
      button.textContent = ok ? 'Copied' : 'Failed';

      clearTimeout(button.copyResetTimer);
      button.copyResetTimer = setTimeout(() => {
        button.textContent = original;
        button.classList.remove('copied', 'copy-failed');
      }, 1200);
    }

    function clearResultPane() {
      collapseResultCards();
      rendered.replaceChildren();
      rendered.textContent = '';
      clearRenderedError();
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails();
    }

    function clearResultDetails(options = {}) {
      parsed.classList.remove('matrix-pretty');
      functionStyle.classList.remove('matrix-pretty');
      value.classList.remove('matrix-pretty');
      parsed.textContent = '';
      functionStyle.textContent = '';
      resetMoreDigitsButton(parsedMore, false);
      resetMoreDigitsButton(functionMore, false);
      delete parsed.dataset.fullText;
      delete parsed.dataset.displayText;
      delete functionStyle.dataset.fullText;
      delete functionStyle.dataset.displayText;
      value.textContent = '';
      lastTex = '';
      lastDerivativeExpression = '';
      currentVariables = [];
      currentDifferentiable = true;
      renderDerivativeButtons(currentVariables);
      if (!options.keepBindings)
        clearVariableValues();
    }

    async function evaluateExpression(options = {}) {
      const editorText = currentExpressionText();
      const editorBodyText = String(expr.value || '').trim();
      const text = options.reuseLastInput && lastEvaluationInputText
        ? lastEvaluationInputText
        : editorText;
      if (!text) return;
      showResults();
      setBusy(true);
      setStatus('Evaluating...');
      try {
        const {response, data} = await fetchEvaluation(text);

        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Evaluation failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          setStatus('Error');
          return;
        }

        if (data.partial_error) {
          setRenderedError(data.error || 'Evaluation failed');
          resetMoreDigitsButton(renderedMore, false);
        } else {
          setRenderedResult(data);
        }
        if (data.expression && !data.partial_error)
          setExpressionEditor(
            editorText || text,
            data.binding_values || null,
            editorBodyText || null
          );
        else if (data.binding_values)
          renderVariableValues(data.binding_values || []);
        setExpandableText(
          parsed,
          parsedMore,
          data.display_expression || (data.expression ? compactExpressionForEditor(data.expression).display : ''),
          data.full_display_expression || data.expression || ''
        );
        setExpandableText(
          functionStyle,
          functionMore,
          data.display_function || data.function || '',
          data.full_display_function || data.function || ''
        );
        value.textContent = data.value || '';
        lastEvaluationInputText = data.partial_error ? text : (data.expression || text);
        if (!data.partial_error)
          saveLastExpression(editorText || fullExpressionText || expr.value.trim());
        lastDerivativeExpression = derivativeExpressionFromLine(data.derivative);
        {
          const variableBindings = variableNamesFromBindings(data.binding_values || []);
          currentVariables = variableBindings.length
            ? variableBindings
            : variablesFromExpression(data.expression || '');
        }
        currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
        renderDerivativeButtons(currentVariables);
        setStatus(data.partial_error ? 'Error' : 'Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateMatrix() {
      const text = expr.value.trim();
      if (!text)
        return;
      showResults();
      setBusy(true);
      setStatus('Evaluating matrix...');
      try {
        const {response, data} = await fetchMatrixEvaluation();
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Matrix evaluation failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        lastTex = data.tex || '';
        rendered.dataset.displayTex = data.tex || '';
        rendered.dataset.fullTex = data.tex || '';
        rendered.dataset.displaySvg = data.svg || '';
        rendered.dataset.fullSvg = '';
        rendered.dataset.renderError = data.render_error || '';
        setRenderedContent(data.svg || '', data.render_error || (data.tex || 'No rendered TeX available'));
        resetMoreDigitsButton(renderedMore, false);
        setExpandableText(parsed, parsedMore, data.result || '', data.result || '');
        setMatrixPrettyResult(data.result || '', data.pretty || '');
        value.textContent = '';
        setValueCardVisible(false);
        modeEditorText.matrix = text;
        saveLastMatrixState();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        clearVariableValues();
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    }

    async function evaluateEquation() {
      const text = String(currentExpressionText() || expr.value || '').trim();
      if (!text)
        return;
      showResults();
      setBusy(true);
      setStatus('Solving equation...');
      try {
        const {response, data} = await fetchEquationEvaluation();
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Equation solving failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        lastTex = data.tex || '';
        rendered.dataset.displayTex = data.display_tex || data.tex || '';
        rendered.dataset.fullTex = data.full_display_tex || data.tex || '';
        rendered.dataset.displaySvg = data.svg || '';
        rendered.dataset.fullSvg = '';
        rendered.dataset.renderError = data.render_error || '';
        setRenderedContent(data.svg || '', data.render_error || (data.tex || 'No rendered TeX available'));
        resetMoreDigitsButton(
          renderedMore,
          !!data.full_display_tex &&
            !!data.display_tex &&
            data.full_display_tex !== data.display_tex &&
            hasAbbreviatedValue(data.display_tex)
        );
        setExpandableText(
          parsed,
          parsedMore,
          data.display_equation || data.equation || '',
          data.full_display_equation || data.equation || ''
        );
        setExpandableText(
          functionStyle,
          functionMore,
          data.solutions || data.status || '',
          data.solutions || data.status || ''
        );
        {
          const valueLines = [];
          const residualValue = displayEquationValue(data.error);
          if (data.status)
            valueLines.push(`solve status: ${data.status}`);
          if (Number.isFinite(Number(data.solution_count)) && Number(data.solution_count) > 0)
            valueLines.push(`solutions found: ${data.solution_count}`);
          if (data.residual)
            valueLines.push(`residual expression: ${data.residual}`);
          if (data.error && residualValue !== 'unresolved')
            valueLines.push(`residual value: ${residualValue}`);
          value.textContent = valueLines.join('\n');
        }
        if (Array.isArray(data.binding_values))
          renderVariableValues(data.binding_values);
        else
          clearVariableValues();
        modeEditorText.equation = text;
        saveLastEquationState();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    }

    async function evaluateIntegrator() {
      const text = expr.value.trim();
      if (!text)
        return;
      showResults();
      setBusy(true);
      setStatus('Integrating...');
      try {
        const {response, data} = await fetchIntegratorEvaluation();
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Integration failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        lastTex = data.tex || '';
        rendered.dataset.displayTex = data.tex || '';
        rendered.dataset.fullTex = data.tex || '';
        rendered.dataset.displaySvg = data.svg || '';
        rendered.dataset.fullSvg = '';
        rendered.dataset.renderError = data.render_error || '';
        setRenderedContent(data.svg || '', data.render_error || (data.tex || 'No rendered TeX available'));
        resetMoreDigitsButton(renderedMore, false);
        setExpandableText(parsed, parsedMore, data.expression || '', data.expression || '');
        const workUnits = data.work_units || data.intervals || '';
        const workCap = data.work_cap || data.max_intervals || '';
        const statusText = String(data.status || '');
        const symbolicStatus = /symbolic|antiderivative/i.test(statusText);
        const antiderivativeStatus = /antiderivative/i.test(statusText);
        const stoppedEarly = !symbolicStatus && workUnits && workCap && String(workUnits) !== String(workCap);
        const workText = workUnits && workCap
          ? `work used: ${workUnits} / ${workCap}${stoppedEarly ? ' (precision reached)' : ''}`
          : (workUnits ? `work used: ${workUnits}` : '');
        const detailLines = [];
        if (data.antiderivative)
          detailLines.push(`Antiderivative:\n${data.antiderivative}`);
        if (data.symbolic && !antiderivativeStatus)
          detailLines.push(`Definite result:\n${data.symbolic}`);
        const domainText = [data.bound, data.status ? `status: ${data.status}` : '', symbolicStatus ? '' : workText]
          .filter(Boolean)
          .join('\n');
        if (domainText)
          detailLines.push(domainText);
        const detailText = detailLines.join('\n\n');
        setExpandableText(functionStyle, functionMore, detailText, detailText);
        {
          const valueLines = [];
          const exactText = data.symbolic && antiderivativeStatus ? `${data.symbolic} + C` : data.symbolic;
          if (exactText)
            valueLines.push(`exact: ${exactText}`);
          if (data.value)
            valueLines.push(`${data.symbolic ? 'numeric: ' : ''}${data.value}`);
          if (data.error)
            valueLines.push(`error ≈ ${data.error}`);
          if (!valueLines.length && data.status)
            valueLines.push(data.status);
          value.textContent = valueLines.join('\n');
        }
        modeEditorText.integrator = text;
        applyIntegratorResultBound(data);
        saveLastIntegratorState();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        clearVariableValues();
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    }

    function evaluateActiveModeOnLoad() {
      if (currentMode() === 'matrix') {
        evaluateMatrix();
        return;
      }
      if (currentMode() === 'equation') {
        evaluateEquation();
        return;
      }
      if (currentMode() === 'integrator') {
        evaluateIntegrator();
        return;
      }
      evaluateExpression();
    }

    async function runGoalSeek(sourceText, target, start = {}, options = {}) {
      const request = goalSeekExpressionAndStarts(sourceText, start);
      const response = await fetch('/goal_seek', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          expression: expressionForEvaluation(request.expression),
          target,
          start: request.start,
          precision: requestedValuePrecision()
        })
      });
      const data = await response.json();

      if (!response.ok || !data.ok) {
        setRenderedError(data.error || 'Goal seek failed');
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
        return false;
      }

      if (!options.skipHistoryUpdate)
        pushExpressionHistory(currentExpressionText());

      const solvedExpression = data.expression || sourceText;
      const solvedWithoutNan = expressionForEditor(solvedExpression).trim();
      const sourceWithoutNan = expressionForEditor(sourceText).trim();
      const unchanged = solvedWithoutNan === sourceWithoutNan;
      setRenderedResult(data);
      setExpressionEditor(
        solvedExpression,
        data.binding_values || null,
        data.editor_expression || null
      );
      setExpandableText(
        parsed,
        parsedMore,
        data.display_expression || compactExpressionForEditor(solvedExpression).display,
        data.full_display_expression || solvedExpression
      );
      setExpandableText(
        functionStyle,
        functionMore,
        data.display_function || data.function || '',
        data.full_display_function || data.function || ''
      );
      value.textContent = data.value || '';
      lastEvaluationInputText = solvedExpression;
      lastDerivativeExpression = '';
      {
        const variableBindings = variableNamesFromBindings(data.binding_values || []);
        currentVariables = variableBindings.length
          ? variableBindings
          : variablesFromExpression(solvedExpression);
      }
      currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
      renderDerivativeButtons(currentVariables);
      expr.dataset.goalSeekSource = expressionForEditor(request.expression).trim();
      expr.dataset.goalSeekTarget = target;
      hideTargetEntry();
      setStatus(unchanged ? 'Goal already reached' : 'Goal reached');
      return true;
    }

    run.addEventListener('click', () => {
      if (currentMode() === 'expression') {
        forwardHistory = [];
        clearGoalSeekRequest();
        hideTargetEntry();
        evaluateExpression();
      } else if (currentMode() === 'equation') {
        evaluateEquation();
      } else if (currentMode() === 'matrix') {
        evaluateMatrix();
      } else {
        evaluateIntegrator();
      }
    });

    back.addEventListener('click', () => {
      const previous = expressionHistory.pop();
      if (!previous) {
        updateHistoryButtons();
        return;
      }

      const current = currentExpressionText();
      if (current) forwardHistory.push(current);
      setExpressionEditor(previous);
      evaluateExpression({skipHistoryUpdate: true});
    });

    forward.addEventListener('click', () => {
      const next = forwardHistory.pop();
      if (!next) {
        updateHistoryButtons();
        return;
      }

      const current = currentExpressionText();
      if (current) expressionHistory.push(current);
      setExpressionEditor(next);
      evaluateExpression({skipHistoryUpdate: true});
    });

    async function takeDerivative(wrt) {
      const text = currentExpressionText();
      if (!text || !wrt) return;

      showResults();
      setBusy(true);
      setStatus(`Differentiating d/d${wrt}...`);
      try {
        const {response, data} = await fetchEvaluation(text, wrt);
        const derivativeExpression = derivativeExpressionFromLine(data.derivative);
        const derivativeTex = data.derivative_tex || '';
        const derivativeSvg = data.derivative_svg || '';

        if (!response.ok || !data.ok || !derivativeExpression) {
          setRenderedError(data.error || data.raw || `No derivative for ${wrt}`);
          resetMoreDigitsButton(renderedMore, false);
          setStatus('Error');
          return;
        }

        pushExpressionHistory(text);
        setExpressionEditor(derivativeExpression);
        await evaluateExpression();
        if (derivativeTex) {
          clearRenderedError();
          lastTex = derivativeTex;
          rendered.dataset.displayTex = derivativeTex;
          rendered.dataset.fullTex = derivativeTex;
          rendered.dataset.displaySvg = derivativeSvg;
          rendered.dataset.fullSvg = '';
          rendered.dataset.renderError = data.derivative_render_error || '';
          setRenderedContent(
            derivativeSvg,
            data.derivative_render_error || derivativeTex
          );
          resetMoreDigitsButton(renderedMore, false);
        }
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    }

    function evaluateFromKeyboard() {
      forwardHistory = [];
      if (currentMode() === 'equation')
        evaluateEquation();
      else if (currentMode() === 'matrix')
        evaluateMatrix();
      else if (currentMode() === 'integrator')
        evaluateIntegrator();
      else
        evaluateExpression();
    }

    expr.addEventListener('keydown', (event) => {
      if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
        event.preventDefault();
        if (currentMode() === 'expression')
          evaluateFromKeyboard();
        else if (currentMode() === 'equation')
          evaluateEquation();
        else if (currentMode() === 'matrix')
          evaluateMatrix();
        else
          evaluateIntegrator();
      }
    });

    expr.addEventListener('input', () => {
      if (currentMode() === 'equation') {
        if (!bindingParts(expr.value)) {
          fullExpressionText = expr.value.trim();
          displayedExpressionText = expr.value.trim();
          expr.dataset.fullExpression = fullExpressionText;
          expr.dataset.displayExpression = displayedExpressionText;
        }
        refreshVariableValuesFromEditor();
        updateHistoryButtons();
        return;
      }
      if (currentMode() !== 'expression') {
        updateHistoryButtons();
        return;
      }
      if (expr.value.trim() === (expr.dataset.displayExpression || displayedExpressionText))
        return;
      /*
       * Keep the previous full { body | bindings } expression while the body is
       * edited.  The parser will drop bindings for symbols that disappear, but
       * carrying the old source forward lets newly edited bodies keep existing
       * values such as x = π/4 or a = e.
       */
      clearGoalSeekRequest();
      lastEvaluationInputText = '';
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
    });

    clear.addEventListener('click', () => {
      if (currentMode() === 'expression') {
        const current = currentExpressionText();
        if (current)
          expressionHistory.push(current);
        forwardHistory = [];
      }
      expr.value = '';
      if (currentMode() === 'matrix') {
        matrixOperand.value = '';
        matrixOperation.value = 'inverse';
      }
      if (currentMode() === 'equation' && equationVariable)
        equationVariable.value = DEFAULT_EQUATION_VARIABLE_TEXT;
      if (currentMode() === 'integrator') {
        resetIntegratorBoundsToDefault();
        if (integratorIntervalCap)
          integratorIntervalCap.value = String(DEFAULT_INTEGRATOR_INTERVAL_CAP);
      }
      captureCurrentModeEditor();
      clearExpressionSource();
      hideTargetEntry();
      clearResultPane();
      saveCurrentModeResultState();
      updateHistoryButtons();
      setStatus('Ready');
      expr.focus();
    });

    help.addEventListener('click', toggleHelp);

    goalSeek.addEventListener('click', async () => {
      if (currentMode() !== 'expression')
        return;
      const text = currentExpressionText();
      if (!text) return;

      if (targetRow.classList.contains('hidden')) {
        showTargetEntry();
        return;
      }

      showResults();
      setBusy(true);
      setStatus('Goal seeking...');
      try {
        const target = goalTarget.value.trim() || '0';
        await runGoalSeek(text, target);
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    });

    if (modeTabs.length) {
      modeTabs.forEach((tab) => tab.addEventListener('click', () => {
        captureCurrentModeEditor();
        saveCurrentModeResultState();
        if (!setMode(tab.dataset.mode))
          return;
        saveLastLabMode(currentMode());
        hideTargetEntry();
        restoreModeEditor(currentMode());
        syncModeUI();
        restoreModeResultState(currentMode());
        if (currentMode() === 'integrator' && !currentIntegratorBound().name)
          resetIntegratorBoundsToDefault();
        if (currentMode() === 'integrator') {
          if (integratorIntervalCap)
            integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
        }
        expr.focus();
      }));
    }

    if (matrixOperation)
      matrixOperation.addEventListener('change', () => {
        matrixOperation.value = validMatrixOperation(matrixOperation.value);
        syncMatrixControls();
        if (currentMode() === 'matrix')
          saveLastMatrixState();
      });

    if (equationVariable)
      equationVariable.addEventListener('change', () => {
        equationVariable.value = String(equationVariable.value || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
          DEFAULT_EQUATION_VARIABLE_TEXT;
        if (currentMode() === 'equation')
          saveLastEquationState();
      });

    if (integratorIntervalCap)
      integratorIntervalCap.addEventListener('change', () => {
        integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
        if (currentMode() === 'integrator')
          saveLastIntegratorState();
      });

    [integratorVariable, integratorLowerBound, integratorUpperBound].forEach((input) => {
      if (!input)
        return;
      input.addEventListener('focus', () => {
        if (input === integratorLowerBound || input === integratorUpperBound)
          cleanIntegratorBoundInput(input);
      });
      input.addEventListener('change', () => {
        if (input === integratorLowerBound || input === integratorUpperBound)
          cleanIntegratorBoundInput(input);
        if (currentMode() === 'integrator')
          saveLastIntegratorState();
      });
    });

    goalTarget.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        event.preventDefault();
        goalSeek.click();
      } else if (event.key === 'Escape') {
        event.preventDefault();
        hideTargetEntry();
        expr.focus();
        setStatus('Ready');
      }
    });

    morePrecision.addEventListener('click', async () => {
      setRequestedPrecisionBits(nextPrecisionStepBits(requestedPrecisionBits()));
      savePrecisionState();
      setStatus('Precision changed');
      try {
        if (currentMode() === 'expression') {
          const goalSeekSource = currentGoalSeekSource();
          const goalSeekTarget = expr.dataset.goalSeekTarget || '';

          if (goalSeekSource && goalSeekTarget) {
            const solvedExpression = fullExpressionText || currentExpressionText();
            const start = solvedStartValuesForGoalSeek(goalSeekSource, solvedExpression);
            await runGoalSeek(goalSeekSource, goalSeekTarget, start, {skipHistoryUpdate: true});
          } else {
            await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
          }
        }
        else if (currentMode() === 'equation')
          await evaluateEquation();
        else if (currentMode() === 'matrix')
          await evaluateMatrix();
        else
          await evaluateIntegrator();
      } finally {
        updateHistoryButtons();
      }
    });

    lessPrecision.addEventListener('click', async () => {
      setRequestedPrecisionBits(previousPrecisionStepBits(requestedPrecisionBits()));
      savePrecisionState();
      setStatus('Precision changed');
      try {
        if (currentMode() === 'expression') {
          const goalSeekSource = currentGoalSeekSource();
          const goalSeekTarget = expr.dataset.goalSeekTarget || '';

          if (goalSeekSource && goalSeekTarget) {
            const solvedExpression = fullExpressionText || currentExpressionText();
            const start = solvedStartValuesForGoalSeek(goalSeekSource, solvedExpression);
            await runGoalSeek(goalSeekSource, goalSeekTarget, start, {skipHistoryUpdate: true});
          } else {
            await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
          }
        }
        else if (currentMode() === 'equation')
          await evaluateEquation();
        else if (currentMode() === 'matrix')
          await evaluateMatrix();
        else
          await evaluateIntegrator();
      } finally {
        updateHistoryButtons();
      }
    });

    renderedMore.addEventListener('click', () => {
      toggleRenderedDigits();
    });

    parsedMore.addEventListener('click', () => {
      toggleTextDigits(parsed, parsedMore);
    });

    functionMore.addEventListener('click', () => {
      toggleTextDigits(functionStyle, functionMore);
    });

    copyButtons.forEach((button) => {
      button.addEventListener('click', async () => {
        const text = copyTextForTarget(button.dataset.copyTarget);
        if (!text) return;
        try {
          await writeClipboardText(text);
          flashCopyButton(button, true);
          setStatus('Copied');
          setTimeout(() => setStatus('Ready'), 1000);
        } catch (err) {
          flashCopyButton(button, false);
          setStatus(String(err));
        }
      });
    });

    zoomButtons.forEach((button) => {
      button.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        const card = button.closest('.result-card');
        if (!card)
          return;
        if (button.hasAttribute('data-zoom-reset'))
          setResultZoom(card, RESULT_ZOOM_DEFAULT_INDEX);
        else
          stepResultZoom(card, Number(button.dataset.zoomStep || 1));
        setStatus(`Zoom ${Math.round(RESULT_ZOOM_LEVELS[resultZoomIndex(card)] * 100)}%`);
      });
    });

    resultCards.forEach((card) => {
      applyResultZoom(card);
      card.addEventListener('wheel', (event) => {
        if (!event.ctrlKey && !event.metaKey)
          return;
        event.preventDefault();
        stepResultZoom(card, event.deltaY < 0 ? 1 : -1);
      }, {passive: false});
    });

    window.addEventListener('resize', () => {
      resultCards.forEach((card) => applyResultZoom(card));
    });

    expandCardButtons.forEach((button) => {
      button.setAttribute('aria-expanded', 'false');
      button.addEventListener('click', () => toggleResultCardExpansion(button));
    });

    syncModeTabs();
    syncModeUI();
    cleanIntegratorBoundInputs();
    requestAnimationFrame(cleanIntegratorBoundInputs);
    setStatus('Ready');
    refreshMobileAccess();
    setInterval(refreshMobileAccess, 5000);
    loadLastState().finally(() => evaluateActiveModeOnLoad());
  </script>
</body>
</html>
""".replace("__LAB_NAME__", LAB_APP_NAME).replace(
    "__CONTROL_QUERY_PREFIX__", f"{CONTROL_QUERY_PARAM}="
).replace(
    "__THEME_COLOR__", LAB_THEME_COLOR
).replace(
    "__THEME_OVERRIDES__", LAB_THEME_OVERRIDES
).replace(
    "__BODY_CLASS__", LAB_BODY_CLASS
).replace(
    "__LAB_SUBTITLE__", LAB_SUBTITLE
)

WEB_MANIFEST = {
    "name": LAB_APP_NAME,
    "short_name": LAB_SHORT_NAME,
    "description": LAB_DESCRIPTION,
    "start_url": "/",
    "scope": "/",
    "display": "standalone",
    "background_color": LAB_MANIFEST_BACKGROUND,
    "theme_color": LAB_MANIFEST_THEME,
    "icons": [
        {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
        {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png"}
    ]
}


def default_state() -> dict[str, object]:
    return {
        "expression": DEFAULT_EXPRESSION,
        "equation": DEFAULT_EQUATION,
        "equation_variable": DEFAULT_EQUATION_VARIABLE,
        "matrix": DEFAULT_MATRIX,
        "lab_mode": "expression",
        "matrix_operation": DEFAULT_MATRIX_OPERATION,
        "matrix_operand": "",
        "integrator_expression": DEFAULT_INTEGRATOR_EXPRESSION,
        "integrator_bounds": DEFAULT_INTEGRATOR_BOUNDS,
        "integrator_interval_cap": DEFAULT_INTEGRATOR_INTERVAL_CAP,
        "precision_bits": {
            "expression": 256,
            "equation": 256,
            "matrix": 256,
            "integrator": 17,
        },
    }


def load_state_data() -> dict[str, object]:
    state = default_state()
    try:
        data = json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return state

    if not isinstance(data, dict):
        return state

    state.update(data)
    expression = str(state.get("expression", "")).strip()
    if "..." in expression:
        state["expression"] = DEFAULT_EXPRESSION
    else:
        state["expression"] = expression_with_sorted_constants(expression)

    matrix = str(state.get("matrix", "")).strip()
    if "..." in matrix:
        state["matrix"] = DEFAULT_MATRIX

    lab_mode = str(state.get("lab_mode", "")).strip()
    if lab_mode not in {"expression", "equation", "matrix", "integrator"}:
        state["lab_mode"] = "expression"

    equation = str(state.get("equation", "")).strip()
    if "..." in equation:
        state["equation"] = DEFAULT_EQUATION
    else:
        state["equation"] = expression_with_sorted_constants(equation)

    equation_variable = str(state.get("equation_variable", "")).strip()
    if not equation_variable:
        state["equation_variable"] = DEFAULT_EQUATION_VARIABLE

    matrix_operation = str(state.get("matrix_operation", "")).strip()
    if matrix_operation not in {
        "eval",
        "inverse",
        "eigenvalues",
        "eigendecompose",
        "charpoly",
        "det",
        "trace",
        "rank",
        "simplify",
        "solve",
    }:
        state["matrix_operation"] = DEFAULT_MATRIX_OPERATION

    integrator_expression = str(state.get("integrator_expression", "")).strip()
    if "..." in integrator_expression:
        state["integrator_expression"] = DEFAULT_INTEGRATOR_EXPRESSION

    try:
        cap = int(state.get("integrator_interval_cap", DEFAULT_INTEGRATOR_INTERVAL_CAP))
    except (TypeError, ValueError):
        cap = DEFAULT_INTEGRATOR_INTERVAL_CAP
    if cap not in INTEGRATOR_INTERVAL_CAP_CHOICES:
        cap = DEFAULT_INTEGRATOR_INTERVAL_CAP
    state["integrator_interval_cap"] = cap
    return state


def load_state_expression() -> str:
    data = load_state_data()

    expression = str(data.get("expression", "")).strip()
    if "..." in expression:
        return DEFAULT_EXPRESSION
    return expression or DEFAULT_EXPRESSION


def save_state_data(updates: dict[str, object]) -> None:
    state = load_state_data()
    normalized = dict(updates)
    if "expression" in normalized:
        normalized["expression"] = expression_with_sorted_constants(
            str(normalized.get("expression") or "").strip()
        )
    if "equation" in normalized:
        normalized["equation"] = expression_with_sorted_constants(
            str(normalized.get("equation") or "").strip()
        )
    state.update(normalized)
    STATE_FILE.write_text(
        json.dumps(state, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def save_state_expression(expression: str) -> None:
    if "..." in expression:
        return
    save_state_data({"expression": expression})


def expression_for_editor(expression: str) -> str:
    return re.sub(r"(=\s*)NAN\b", r"\1?", expression)


def editor_expression_from_fields(fields: dict[str, str]) -> str:
    unbound = str(fields.get("unbound", "")).strip()
    if unbound:
        return expression_for_editor(unbound)
    return expression_for_editor(fields.get("expression", ""))


def expression_for_display(expression: str) -> str:
    return expression_for_editor(expression)


def function_for_display(function: str) -> str:
    return re.sub(r"(=\s*)NAN\b", r"\1?", str(function or ""))


def tex_for_display(tex: str) -> str:
    return re.sub(r"(=\s*)NAN\b", r"\1?", str(tex or ""))


def _is_loopback_or_wildcard_host(host: str) -> bool:
    host = host.strip().lower().strip("[]")
    return (
        not host
        or host == "localhost"
        or host == "0.0.0.0"
        or host == "::"
        or host == "::0"
        or host == "::1"
        or host.startswith("127.")
    )


def _host_from_header(host_header: str) -> str:
    host_header = host_header.strip()
    if host_header.startswith("["):
        end = host_header.find("]")
        return host_header[1:end] if end >= 0 else host_header.strip("[]")
    return host_header.rsplit(":", 1)[0] if ":" in host_header else host_header


def _ip_address_from_text(text: str) -> ipaddress._BaseAddress | None:
    text = text.strip().strip('"')
    if not text:
        return None
    if text.startswith("[") and "]" in text:
        text = text[1:text.index("]")]
    elif ":" in text and text.count(":") == 1:
        text = text.split(":", 1)[0]
    try:
        return ipaddress.ip_address(text)
    except ValueError:
        return None


def request_allows_lab_access(client_host: str) -> bool:
    client_address = _ip_address_from_text(client_host)
    allowed_ipv4 = (
        ipaddress.ip_network("127.0.0.0/8"),
        ipaddress.ip_network("10.0.0.0/8"),
        ipaddress.ip_network("172.16.0.0/12"),
        ipaddress.ip_network("192.168.0.0/16"),
        ipaddress.ip_network("169.254.0.0/16"),
        ipaddress.ip_network("100.64.0.0/10"),
    )
    allowed_ipv6 = (
        ipaddress.ip_network("::1/128"),
        ipaddress.ip_network("fc00::/7"),
        ipaddress.ip_network("fe80::/10"),
    )

    if not client_address:
        return False
    if isinstance(client_address, ipaddress.IPv6Address) and client_address.ipv4_mapped:
        client_address = client_address.ipv4_mapped
    if isinstance(client_address, ipaddress.IPv4Address):
        return any(client_address in network for network in allowed_ipv4)
    return any(client_address in network for network in allowed_ipv6)


def request_uses_public_funnel_host(host_header: str) -> bool:
    request_host = _host_from_header(host_header).strip().lower()

    if not request_host.endswith(".ts.net"):
        return False
    tailscale_host = tailscale_https_host().strip().lower()
    return bool(request_host and tailscale_host and request_host == tailscale_host and
                tailscale_funnel_enabled())


def _control_token_from_query(path: str) -> str:
    query = urllib.parse.urlparse(path).query
    values = urllib.parse.parse_qs(query).get(CONTROL_QUERY_PARAM, [])
    return values[0] if values else ""


def _control_token_from_cookie(headers: http.client.HTTPMessage) -> str:
    for part in headers.get("Cookie", "").split(";"):
        name, sep, value = part.strip().partition("=")
        if sep and name == CONTROL_COOKIE:
            return urllib.parse.unquote(value)
    return ""


def _control_url(url: str) -> str:
    separator = "&" if "?" in url else "?"
    token = urllib.parse.urlencode({CONTROL_QUERY_PARAM: CONTROL_TOKEN})
    return f"{url}{separator}{token}"


def request_allows_funnel_control(headers: http.client.HTTPMessage,
                                  client_host: str,
                                  request_token: str = "") -> bool:
    if request_token == CONTROL_TOKEN:
        return True
    if headers.get("X-Dval-Lab-Control", "") == CONTROL_TOKEN:
        return True
    if _control_token_from_cookie(headers) == CONTROL_TOKEN:
        return True

    request_host = _host_from_header(headers.get("Host", "")).strip().lower()
    tailscale_hosts = {
        host for host in (
            tailscale_https_host().strip().lower(),
            tailscale_magicdns_host().strip().lower(),
            tailscale_ipv4().strip().lower(),
        ) if host
    }
    if request_host in tailscale_hosts:
        return False

    request_address = _ip_address_from_text(request_host)
    if request_address and request_address in ipaddress.ip_network("100.64.0.0/10"):
        return False

    client_address = _ip_address_from_text(client_host)
    return bool(client_address and client_address.is_loopback and
                _is_loopback_or_wildcard_host(request_host))


def tailscale_ipv4() -> str:
    if not shutil.which("tailscale"):
        return ""

    try:
        status = subprocess.run(
            ["tailscale", "status"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        if status.returncode != 0:
            return ""

        completed = subprocess.run(
            ["tailscale", "ip", "-4"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        for address in completed.stdout.split():
            if address.startswith("100."):
                return address
    except Exception:
        pass

    return ""


def tailscale_magicdns_host() -> str:
    if not tailscale_ipv4():
        return ""
    return os.environ.get("MARS_LAB_TAILSCALE_HOST", "mars").strip().strip(".")


def tailscale_https_host() -> str:
    if not tailscale_ipv4():
        return ""

    env_host = os.environ.get("MARS_LAB_TAILSCALE_HTTPS_HOST", "").strip().strip(".")
    if env_host:
        return env_host

    try:
        completed = subprocess.run(
            ["tailscale", "status", "--json"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        if completed.returncode != 0:
            return ""
        status = json.loads(completed.stdout)
    except Exception:
        return ""

    for cert_host in status.get("CertDomains", []) or []:
        cert_host = str(cert_host).strip().strip(".")
        if cert_host:
            return cert_host

    self_info = status.get("Self", {}) or {}
    dns_name = str(self_info.get("DNSName", "")).strip().strip(".")
    return dns_name


def tailscale_funnel_enabled() -> bool:
    try:
        completed = subprocess.run(
            ["tailscale", "funnel", "status"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
    except Exception:
        return False

    return completed.returncode == 0 and "Funnel on" in completed.stdout


def set_tailscale_funnel_enabled(port: int, enabled: bool) -> bool:
    if not tailscale_https_host():
        return False

    try:
        if enabled:
            completed = subprocess.run(
                ["tailscale", "funnel", "--bg", "--https", "443", str(port)],
                text=True,
                capture_output=True,
                timeout=5,
                check=False,
            )
            return completed.returncode == 0

        completed = subprocess.run(
            ["tailscale", "funnel", "--https=443", "off"],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        # If there is no existing Serve/Funnel config, Tailscale may report that
        # the off command had nothing to change.  Private mode still needs Serve.

        completed = subprocess.run(
            ["tailscale", "serve", "--bg", "--https", "443", str(port)],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        return completed.returncode == 0
    except Exception:
        return False


def ensure_tailscale_serve(bind_host: str, port: int) -> None:
    if os.environ.get("MARS_LAB_TAILSCALE_SERVE", "1").strip() in ("0", "false", "False", "no", "NO"):
        return
    if bind_host.strip() not in ("0.0.0.0", "::", "::0") or not tailscale_ipv4():
        return
    if not tailscale_https_host():
        return

    # Privacy first: MARS Lab may be shared on local WiFi or the private
    # tailnet, but it should not publish itself to the public internet.
    set_tailscale_funnel_enabled(port, False)


def local_lan_ipv4() -> str:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("8.8.8.8", 80))
            return str(sock.getsockname()[0])
    except OSError:
        pass

    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            address = str(info[4][0])
            if not _is_loopback_or_wildcard_host(address):
                return address
    except OSError:
        pass

    try:
        completed = subprocess.run(
            ["hostname", "-I"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        for address in completed.stdout.split():
            if "." in address and not _is_loopback_or_wildcard_host(address):
                return address
    except Exception:
        pass

    try:
        completed = subprocess.run(
            ["ip", "-4", "route", "get", "1.1.1.1"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        words = completed.stdout.split()
        if "src" in words:
            address = words[words.index("src") + 1]
            if not _is_loopback_or_wildcard_host(address):
                return address
    except Exception:
        pass

    return ""


def local_mdns_host() -> str:
    hostname = socket.gethostname().strip().strip(".")
    if not hostname:
        return ""

    short_name = hostname.split(".", 1)[0]
    if not short_name or _is_loopback_or_wildcard_host(short_name):
        return ""
    return f"{short_name.lower()}.local"


def host_port_reachable(host: str, port: int, timeout: float = 0.35) -> bool:
    host = host.strip().strip("[]")
    if not host or port <= 0:
        return False
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def _host_is_ipv6(host: str) -> bool:
    address = _ip_address_from_text(host)
    return isinstance(address, ipaddress.IPv6Address)


class DualStackThreadingHTTPServer(http.server.ThreadingHTTPServer):
    address_family = socket.AF_INET6

    def server_bind(self) -> None:
        if hasattr(socket, "IPV6_V6ONLY"):
            try:
                self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            except OSError:
                # Some platforms do not allow changing this.  IPv6 access still
                # works; IPv4 may need a separate bind on those systems.
                pass
        super().server_bind()


def create_threading_http_server(host: str, port: int,
                                 handler: type[http.server.BaseHTTPRequestHandler]
                                 ) -> http.server.ThreadingHTTPServer:
    host = host.strip() or "127.0.0.1"
    if host == "::0":
        host = "::"
    if _host_is_ipv6(host):
        return DualStackThreadingHTTPServer((host, port), handler)
    return http.server.ThreadingHTTPServer((host, port), handler)


def browser_access_host(bind_host: str, port: int = 0) -> str:
    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        return "localhost"
    return bind_host


def browser_access_url(bind_host: str, port: int) -> str:
    bind_host = bind_host.strip()
    bind_address = _ip_address_from_text(bind_host)
    bind_is_tailscale = bool(bind_address and bind_address in ipaddress.ip_network("100.64.0.0/10"))
    if bind_is_tailscale:
        tailscale_host = tailscale_https_host()
        if tailscale_host:
            return f"https://{tailscale_host}/"

    browser_host = browser_access_host(bind_host, port)
    if ":" in browser_host and not browser_host.startswith("["):
        browser_host = f"[{browser_host}]"
    return f"http://{browser_host}:{port}/"


def tailscale_access_url(bind_host: str, port: int, path: str = "/") -> str:
    path = path if path.startswith("/") else f"/{path}"
    tailscale_ip = tailscale_ipv4()
    if not tailscale_ip:
        return ""

    tailscale_host = tailscale_https_host()
    if tailscale_host:
        return f"https://{tailscale_host}{path}"

    bind_host = bind_host.strip()
    bind_address = _ip_address_from_text(bind_host)
    bind_accepts_tailscale = (
        bind_host in ("0.0.0.0", "::", "::0") or
        bool(bind_address and bind_address in ipaddress.ip_network("100.64.0.0/10"))
    )
    if not bind_accepts_tailscale:
        return ""

    url_host = tailscale_magicdns_host() or tailscale_ip
    if ":" in url_host and not url_host.startswith("["):
        url_host = f"[{url_host}]"
    return f"http://{url_host}:{port}{path}"


def mobile_access_url(bind_host: str, port: int, host_header: str = "") -> str:
    return str(mobile_access_details(bind_host, port, host_header)["url"])


def mobile_access_details(bind_host: str, port: int, host_header: str = "",
                          control_allowed: bool = False) -> dict[str, object]:
    funnel = tailscale_funnel_enabled()
    request_host = _host_from_header(host_header)
    if request_host and not _is_loopback_or_wildcard_host(request_host):
        tailscale_host = tailscale_https_host()
        magicdns_host = tailscale_magicdns_host()
        request_is_tailscale = (
            request_host.startswith("100.") or
            (tailscale_host and request_host.lower() == tailscale_host.lower()) or
            (magicdns_host and request_host.lower() == magicdns_host.lower())
        )
        if request_is_tailscale:
            url_host = tailscale_host or request_host
            scheme = "https" if tailscale_host else "http"
            url_port = "" if tailscale_host else f":{port}"
            return {
                "url": f"{scheme}://{url_host}{url_port}/",
                "title": "Tailscale access",
                "hint": "Scan from a device connected to Tailscale.",
                "funnel": funnel,
                "tailscale": True,
                "control": control_allowed,
            }
        return {
            "url": f"http://{request_host}:{port}/",
            "title": "WiFi access",
            "hint": "Scan from a phone on the same WiFi.",
            "funnel": False,
            "tailscale": False,
            "control": False,
        }

    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        lan_host = local_mdns_host() or local_lan_ipv4()
        if lan_host:
            return {
                "url": f"http://{lan_host}:{port}/",
                "title": "WiFi access",
                "hint": "Scan from a phone on the same WiFi.",
                "funnel": False,
                "tailscale": False,
                "control": False,
            }
        bind_host = tailscale_ipv4()
        if bind_host:
            tailscale_host = tailscale_https_host()
            scheme = "https" if tailscale_host else "http"
            tailscale_host = tailscale_host or tailscale_magicdns_host() or bind_host
            url_port = "" if scheme == "https" else f":{port}"
            return {
                "url": f"{scheme}://{tailscale_host}{url_port}/",
                "title": "Tailscale access",
                "hint": "Scan from a device connected to Tailscale.",
                "funnel": funnel,
                "tailscale": True,
                "control": control_allowed,
            }

    if _is_loopback_or_wildcard_host(bind_host):
        return {
            "url": "",
            "title": "Mobile access",
            "hint": "No mobile URL is available right now.",
            "funnel": False,
            "tailscale": False,
            "control": False,
        }

    if ":" in bind_host and not bind_host.startswith("["):
        bind_host = f"[{bind_host}]"
    title = "Tailscale access" if bind_host.startswith("100.") else "WiFi access"
    hint = "Scan from a phone connected to Tailscale." if bind_host.startswith("100.") else "Scan from a phone on the same WiFi."
    return {
        "url": f"http://{bind_host}:{port}/",
        "title": title,
        "hint": hint,
        "funnel": False,
        "tailscale": bind_host.startswith("100."),
        "control": control_allowed and bind_host.startswith("100."),
    }


def _qr_gf_tables() -> tuple[list[int], list[int]]:
    exp = [0] * 512
    log = [0] * 256
    x = 1
    for i in range(255):
        exp[i] = x
        log[x] = i
        x <<= 1
        if x & 0x100:
            x ^= 0x11D
    for i in range(255, 512):
        exp[i] = exp[i - 255]
    return exp, log


_QR_GF_EXP, _QR_GF_LOG = _qr_gf_tables()


def _qr_gf_mul(a: int, b: int) -> int:
    if a == 0 or b == 0:
        return 0
    return _QR_GF_EXP[_QR_GF_LOG[a] + _QR_GF_LOG[b]]


def _qr_rs_generator(degree: int) -> list[int]:
    poly = [1]
    for i in range(degree):
        next_poly = [0] * (len(poly) + 1)
        root = _QR_GF_EXP[i]
        for j, coef in enumerate(poly):
            next_poly[j] ^= coef
            next_poly[j + 1] ^= _qr_gf_mul(coef, root)
        poly = next_poly
    return poly


_QR_RS_GENERATOR = _qr_rs_generator(QR_EC_CODEWORDS)


def _qr_rs_remainder(data: list[int]) -> list[int]:
    result = [0] * QR_EC_CODEWORDS
    for value in data:
        factor = value ^ result[0]
        result = result[1:] + [0]
        for i in range(QR_EC_CODEWORDS):
            result[i] ^= _qr_gf_mul(_QR_RS_GENERATOR[i + 1], factor)
    return result


def _qr_data_codewords(text: str) -> list[int]:
    payload = text.encode("utf-8")
    bits: list[int] = []

    def append(value: int, width: int) -> None:
        for shift in range(width - 1, -1, -1):
            bits.append((value >> shift) & 1)

    append(0b0100, 4)  # byte mode
    append(len(payload), 8)
    for byte in payload:
        append(byte, 8)

    capacity = QR_DATA_CODEWORDS * 8
    if len(bits) > capacity:
        raise ValueError("mobile URL is too long for the built-in QR code")

    bits.extend([0] * min(4, capacity - len(bits)))
    while len(bits) % 8:
        bits.append(0)

    codewords = [
        sum(bits[i + bit] << (7 - bit) for bit in range(8))
        for i in range(0, len(bits), 8)
    ]
    pad = 0
    while len(codewords) < QR_DATA_CODEWORDS:
        codewords.append(0xEC if pad % 2 == 0 else 0x11)
        pad += 1
    return codewords


def _qr_format_bits() -> int:
    data = (QR_EC_LEVEL_L << 3) | QR_MASK_PATTERN
    rem = data
    for _ in range(10):
        rem = (rem << 1) ^ (0x537 if (rem >> 9) & 1 else 0)
    return ((data << 10) | (rem & 0x3FF)) ^ 0x5412


def _qr_make_matrix(text: str) -> list[list[int]]:
    data = _qr_data_codewords(text)
    codewords = data + _qr_rs_remainder(data)
    data_bits = [
        (codeword >> shift) & 1
        for codeword in codewords
        for shift in range(7, -1, -1)
    ]
    size = QR_SIZE
    modules: list[list[int | None]] = [[None for _ in range(size)] for _ in range(size)]
    reserved = [[False for _ in range(size)] for _ in range(size)]

    def set_module(x: int, y: int, dark: bool, reserve: bool = True) -> None:
        if 0 <= x < size and 0 <= y < size:
            modules[y][x] = 1 if dark else 0
            if reserve:
                reserved[y][x] = True

    def add_finder(x0: int, y0: int) -> None:
        for y in range(y0 - 1, y0 + 8):
            for x in range(x0 - 1, x0 + 8):
                set_module(x, y, False)
        for y in range(7):
            for x in range(7):
                dark = x in (0, 6) or y in (0, 6) or (2 <= x <= 4 and 2 <= y <= 4)
                set_module(x0 + x, y0 + y, dark)

    def add_alignment(cx: int, cy: int) -> None:
        if reserved[cy][cx]:
            return
        for y in range(-2, 3):
            for x in range(-2, 3):
                dark = max(abs(x), abs(y)) != 1
                set_module(cx + x, cy + y, dark)

    add_finder(0, 0)
    add_finder(size - 7, 0)
    add_finder(0, size - 7)
    add_alignment(30, 30)

    for i in range(size):
        if not reserved[6][i]:
            set_module(i, 6, i % 2 == 0)
        if not reserved[i][6]:
            set_module(6, i, i % 2 == 0)

    # Reserve format cells before data placement, then fill them below.
    for x, y in (
        [(8, i) for i in range(6)]
        + [(8, 7), (8, 8), (7, 8)]
        + [(i, 8) for i in range(6)]
        + [(size - 1 - i, 8) for i in range(8)]
        + [(8, size - 15 + i) for i in range(8, 15)]
    ):
        set_module(x, y, False)
    set_module(8, size - 8, True)

    bit_index = 0
    upward = True
    x = size - 1
    while x > 0:
        if x == 6:
            x -= 1
        for row in range(size):
            y = size - 1 - row if upward else row
            for dx in (0, 1):
                xx = x - dx
                if reserved[y][xx]:
                    continue
                bit = data_bits[bit_index] if bit_index < len(data_bits) else 0
                if (xx + y) % 2 == 0:
                    bit ^= 1
                set_module(xx, y, bool(bit), reserve=False)
                bit_index += 1
        upward = not upward
        x -= 2

    fmt = _qr_format_bits()

    def fmt_bit(i: int) -> bool:
        return ((fmt >> i) & 1) != 0

    for i in range(6):
        set_module(8, i, fmt_bit(i))
    set_module(8, 7, fmt_bit(6))
    set_module(8, 8, fmt_bit(7))
    set_module(7, 8, fmt_bit(8))
    for i in range(9, 15):
        set_module(14 - i, 8, fmt_bit(i))

    for i in range(8):
        set_module(size - 1 - i, 8, fmt_bit(i))
    for i in range(8, 15):
        set_module(8, size - 15 + i, fmt_bit(i))

    return [[1 if value else 0 for value in row] for row in modules]


def qr_svg(text: str) -> str:
    if not text:
        return ""

    try:
        matrix = _qr_make_matrix(text)
    except ValueError:
        return ""

    quiet = 4
    size = len(matrix) + quiet * 2
    path = []
    for y, row in enumerate(matrix):
        for x, dark in enumerate(row):
            if dark:
                path.append(f"M{x + quiet},{y + quiet}h1v1h-1z")

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" '
        'role="img" aria-label="Mobile access QR code">'
        f'<rect width="{size}" height="{size}" fill="#fff"/>'
        f'<path fill="#0f2f5f" d="{"".join(path)}"/>'
        "</svg>"
    )


def mobile_qr_svg(url: str, include_control_token: bool) -> str:
    url = str(url or "").strip()
    if not url:
        return ""

    if include_control_token:
        svg = qr_svg(_control_url(url))
        if svg:
            return svg

    return qr_svg(url)


def _compact_long_text_value(
    value: str,
    limit: int = COMPACT_BINDING_VALUE_LIMIT,
    keep: int = COMPACT_BINDING_VALUE_KEEP,
) -> str:
    value = str(value or "").strip()
    if "..." in value:
        return value
    if len(value) <= limit:
        return value
    return compact_long_numeric_tokens(value)


def compact_long_numeric_tokens(text: str) -> str:
    if not text:
        return text

    def compact_match(match: re.Match[str]) -> str:
        number_text = match.group(2)
        if len(number_text) <= COMPACT_BINDING_VALUE_LIMIT or "..." in number_text:
            return match.group(0)
        scientific = re.match(r"^(.*?)([Ee][+-]?\d+)$", number_text)
        if scientific:
            mantissa = scientific.group(1)
            exponent = scientific.group(2)
            return (
                match.group(1)
                + mantissa[:COMPACT_BINDING_VALUE_KEEP]
                + "..."
                + exponent
            )
        return match.group(1) + number_text[:COMPACT_BINDING_VALUE_KEEP] + "..."

    return re.sub(
        r"(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)",
        compact_match,
        text,
    )


def precision_numeric_tokens(text: str, precision: int) -> str:
    if not text:
        return text

    text = decimalize_long_terminating_rational_tokens(text)

    def precision_match(match: re.Match[str]) -> str:
        return match.group(1) + format_number_text_for_precision(match.group(2), precision)

    return re.sub(
        r"(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)",
        precision_match,
        text,
    )


def _decimalize_long_terminating_rational_token(token: str) -> str:
    token = str(token or "").strip()
    if "/" not in token:
        return token

    sign = ""
    if token.startswith(("+", "-")):
        sign = token[0]
        token = token[1:]

    try:
        numer_text, denom_text = token.split("/", 1)
        numer = int(numer_text, 10)
        denom = int(denom_text, 10)
    except (TypeError, ValueError):
        return (sign + token) if sign else token

    if denom == 0:
        return (sign + token) if sign else token

    if denom < 0:
        numer = -numer
        denom = -denom

    twos = 0
    fives = 0
    den_work = denom
    while den_work % 2 == 0:
        den_work //= 2
        twos += 1
    while den_work % 5 == 0:
        den_work //= 5
        fives += 1

    scale = max(twos, fives)
    if den_work != 1 or scale < 12:
        return (sign + token) if sign else token

    if twos < scale:
        numer *= 5 ** (scale - twos)
    if fives < scale:
        numer *= 2 ** (scale - fives)

    neg = numer < 0
    digits = str(abs(numer))
    if scale >= len(digits):
        digits = "0" * (scale - len(digits) + 1) + digits

    point = len(digits) - scale
    decimal_text = digits[:point] + "." + digits[point:]
    decimal_text = decimal_text.rstrip("0").rstrip(".")
    if not decimal_text:
        decimal_text = "0"
    if neg:
        decimal_text = "-" + decimal_text
    if sign == "+" and not decimal_text.startswith(("+", "-")):
        decimal_text = "+" + decimal_text
    return decimal_text


def decimalize_long_terminating_rational_tokens(text: str) -> str:
    if not text:
        return text

    return re.sub(
        r"(?<![A-Za-z0-9_.])([+-]?\d+/\d+)(?![A-Za-z0-9_.])",
        lambda match: _decimalize_long_terminating_rational_token(match.group(1)),
        text,
    )


def precision_limit_result_fields(fields: dict[str, str], precision: int) -> None:
    for key in ("expression", "unbound", "tex", "function"):
        value = fields.get(key, "")
        if not value:
            continue
        fields[f"raw_{key}"] = value
        fields[key] = precision_numeric_tokens(value, precision)


def compact_display_text(text: str) -> str:
    return compact_long_numeric_tokens(compact_binding_values_text(text))


def normalize_multiline_display_text(text: str) -> str:
    lines = str(text or "").splitlines()
    return "\n".join(line.strip() for line in lines if line.strip())


def compact_binding_values_text(text: str) -> str:
    if not text:
        return text

    def compact_after_pipe(match: re.Match[str]) -> str:
        value = match.group(2).strip()
        if not value:
            return match.group(0)
        return match.group(1) + _compact_long_text_value(value)

    return re.sub(r"(=\s*)(.*?)(?=(?:\\right|[,;}\n]|$))", compact_after_pipe, text)


def compact_function_text(text: str) -> str:
    if not text:
        return text

    compacted: list[str] = []
    for line in text.splitlines():
        match = re.match(r"^(\s*[^=\s][^=]*=\s*)(.+)$", line)
        if match and "(" not in match.group(1):
            compacted.append(match.group(1) + _compact_long_text_value(match.group(2)))
        else:
            compacted.append(compact_long_numeric_tokens(line))
    return compact_long_numeric_tokens("\n".join(compacted))


def find_free_port(host: str) -> int:
    family = socket.AF_INET6 if _host_is_ipv6(host) else socket.AF_INET
    bind_host = "::" if host.strip() == "::0" else host
    with socket.socket(family, socket.SOCK_STREAM) as sock:
        if family == socket.AF_INET6 and hasattr(socket, "IPV6_V6ONLY"):
            try:
                sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            except OSError:
                pass
        sock.bind((bind_host, 0))
        return int(sock.getsockname()[1])


def ensure_mars_lab(binary: Path) -> None:
    if binary.exists() and os.access(binary, os.X_OK):
        return

    subprocess.run(
        ["make", DEFAULT_SCRATCH_TARGET],
        cwd=ROOT,
        check=True,
        text=True,
    )

    if not binary.exists():
        raise RuntimeError(f"scratch binary was not created at {binary}")


def ensure_scratch_binary(binary: Path, target: str) -> None:
    if binary.exists() and os.access(binary, os.X_OK):
        return

    subprocess.run(
        ["make", target],
        cwd=ROOT,
        check=True,
        text=True,
    )

    if not binary.exists():
        raise RuntimeError(f"{target} binary was not created at {binary}")


def parse_keyed_output(
    output: str,
    patterns: dict[str, str],
    multiline_fields: set[str] | None = None,
) -> dict[str, str]:
    fields: dict[str, str] = {}
    current_multiline_key: str | None = None
    multiline_fields = multiline_fields or set()

    for line in output.splitlines():
        matched = False
        for key, pattern in patterns.items():
            match = re.match(pattern, line)
            if match:
                value = match.group(1).rstrip()
                if key in fields and key not in multiline_fields:
                    fields[key] = fields[key] + "\n" + value
                else:
                    fields[key] = value
                current_multiline_key = key if key in multiline_fields else None
                matched = True
                break
        if not matched and current_multiline_key:
            fields[current_multiline_key] += "\n" + line.rstrip()
    return fields


def parse_mars_lab_output(output: str) -> dict[str, str]:
    patterns = {
        "input": r"^input\s+(.*)$",
        "expression": r"^expression\s{2,}(.*)$",
        "unbound": r"^unbound\s+(.*)$",
        "function": r"^function\s+(.*)$",
        "tex": r"^tex\s+(.*)$",
        "differentiable": r"^differentiable\s+(.*)$",
        "value": r"^value\s+(.*)$",
        "residual": r"^residual\s+(.*)$",
        "iterations": r"^iterations\s+(.*)$",
        "complex": r"^complex\s+(.*)$",
        "derivative": r"^derivative\s+(.*)$",
        "derivative_tex": r"^derivative_tex\s*(.*)$",
        "derivative_value": r"^d value\s+(.*)$",
    }
    return parse_keyed_output(output, patterns, {"function"})


def parse_matrix_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "input": r"^input\s+(.*)$",
            "operation": r"^operation\s+(.*)$",
            "operand": r"^operand\s+(.*)$",
            "kind": r"^kind\s+(.*)$",
            "rows": r"^rows\s+(.*)$",
            "cols": r"^cols\s+(.*)$",
            "result": r"^result\s+(.*)$",
            "pretty": r"^pretty\s+(.*)$",
            "tex": r"^tex\s+(.*)$",
            "error": r"^error\s+(.*)$",
        },
        {"pretty"},
    )


def parse_integrator_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "input": r"^input\s+(.*)$",
            "expression": r"^expression\s+(.*)$",
            "dimensions": r"^dimensions\s+(.*)$",
            "bound": r"^bound\s+(.*)$",
            "bound_var": r"^bound_var\s+(.*)$",
            "bound_lower": r"^bound_lower\s*(.*)$",
            "bound_upper": r"^bound_upper\s*(.*)$",
            "tex": r"^tex\s+(.*)$",
            "symbolic_tex": r"^symbolic_tex\s*(.*)$",
            "symbolic_value": r"^symbolic_value\s*(.*)$",
            "antiderivative_tex": r"^antiderivative_tex\s*(.*)$",
            "antiderivative": r"^antiderivative\s+(.*)$",
            "symbolic": r"^symbolic\s+(.*)$",
            "value": r"^value\s*(.*)$",
            "error": r"^error\s+(.*)$",
            "error": r"^error\s+(.*)$",
            "work_units": r"^work_units\s+(.*)$",
            "work_cap": r"^work_cap\s+(.*)$",
            "intervals": r"^intervals\s+(.*)$",
            "max_intervals": r"^max_intervals\s+(.*)$",
            "status": r"^status\s+(.*)$",
        },
    )


def parse_equation_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "input": r"^input\s+(.*)$",
            "equation": r"^equation\s+(.*)$",
            "unbound": r"^unbound\s+(.*)$",
            "tex": r"^tex\s+(.*)$",
            "residual": r"^residual\s+(.*)$",
            "value": r"^value\s+(.*)$",
            "status": r"^status\s+(.*)$",
            "solutions_tex": r"^solutions_tex\s*(.*)$",
            "solutions": r"^solutions\s+(.*)$",
        },
        {"solutions"},
    )


def _trim_decimal_tail(text: str) -> str:
    mantissa, sep, exponent = text.partition("E")

    if "." in mantissa:
        mantissa = mantissa.rstrip("0").rstrip(".")
    if mantissa in ("-0", "+0"):
        mantissa = "0"
    return mantissa + (sep + exponent if sep else "")


def format_number_text_for_precision(
    text: str,
    precision: int,
    zero_subprecision: bool = False,
) -> str:
    text = str(text or "").strip()
    if not text:
        return text

    upper = text.upper()
    if upper in {"NAN", "+NAN", "-NAN", "INF", "+INF", "-INF", "INFINITY", "+INFINITY", "-INFINITY"}:
        return text

    match = re.match(r"^(.+?)\s+([+-])\s+(.+)i$", text)
    if match:
        real = format_number_text_for_precision(
            match.group(1), precision, zero_subprecision)
        imag = format_number_text_for_precision(
            match.group(3), precision, zero_subprecision)
        return f"{real} {match.group(2)} {imag}i"

    try:
        with localcontext() as ctx:
            ctx.prec = max(1, min(MAX_VALUE_PRECISION_DIGITS, int(precision)))
            rounded = +Decimal(text)
    except (InvalidOperation, ValueError):
        return text

    if zero_subprecision and rounded and rounded.copy_abs().adjusted() < -int(precision):
        return "0"
    return _trim_decimal_tail(format(rounded, "g").replace("e", "E"))


def render_tex_to_svg(tex: str) -> tuple[str | None, str | None]:
    if not tex:
        return None, None

    missing_tools = [
        command for command in ("latex", "dvisvgm")
        if shutil.which(command) is None
    ]
    if missing_tools:
        return (
            None,
            "Missing TeX rendering tool(s): "
            + ", ".join(missing_tools)
            + ". On Debian/Ubuntu, install: sudo apt install texlive-latex-base dvisvgm",
        )

    document = rf"""\documentclass{{article}}
\pagestyle{{empty}}
\usepackage{{amsmath}}
\begin{{document}}
\[
{tex}
\]
\end{{document}}
"""

    with tempfile.TemporaryDirectory(prefix="mars-expr-tex-") as tmp_name:
        tmp = Path(tmp_name)
        tex_file = tmp / "expr.tex"
        dvi_file = tmp / "expr.dvi"
        svg_file = tmp / "expr.svg"
        tex_file.write_text(document, encoding="utf-8")

        latex = subprocess.run(
            ["latex", "-interaction=nonstopmode", "-halt-on-error", tex_file.name],
            cwd=tmp,
            text=True,
            capture_output=True,
            timeout=10,
        )
        if latex.returncode != 0:
            return None, latex.stdout + latex.stderr

        dvisvgm = subprocess.run(
            ["dvisvgm", "--no-fonts", "--exact-bbox", str(dvi_file), "-o", str(svg_file)],
            cwd=tmp,
            text=True,
            capture_output=True,
            timeout=10,
        )
        if dvisvgm.returncode != 0:
            return None, dvisvgm.stdout + dvisvgm.stderr

        try:
            return svg_file.read_text(encoding="utf-8"), None
        except OSError as exc:
            return None, str(exc)


def split_top_level_text(text: str, separator: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0

    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        elif ch == separator and depth == 0:
            parts.append(text[start:i])
            start = i + 1

    parts.append(text[start:])
    return parts


def index_top_level_text(text: str, needle: str) -> int:
    depth = 0

    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        elif ch == needle and depth == 0:
            return i

    return -1


def last_index_top_level_text(text: str, needle: str) -> int:
    depth = 0
    found = -1

    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        elif ch == needle and depth == 0:
            found = i

    return found


def parse_expression_body(expression: str) -> tuple[str, str, str]:
    text = expression.strip()

    if text.startswith("{") and text.endswith("}"):
        text = text[1:-1].strip()

    pipe = last_index_top_level_text(text, "|")
    if pipe < 0:
        return text, "", ""

    body = text[:pipe].strip()
    bindings = text[pipe + 1:].strip()
    semi = index_top_level_text(bindings, ";")
    if semi < 0:
        return body, bindings.strip(), ""

    return body, bindings[:semi].strip(), bindings[semi + 1:].strip()


def parse_binding_assignments(bindings: str) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []

    for part in split_top_level_text(bindings, ","):
        eq = index_top_level_text(part, "=")
        if eq < 0:
            continue
        name = part[:eq].strip()
        value = part[eq + 1:].strip()
        if name:
            out.append((name, value))

    return out


def binding_name_sort_key(name: str) -> tuple[str, str]:
    text = str(name or "").strip()
    return text.casefold(), text


def sorted_binding_assignments(assignments: list[tuple[str, str]]) -> list[tuple[str, str]]:
    return sorted(assignments, key=lambda item: binding_name_sort_key(item[0]))


def sorted_assignment_parts(parts: list[str]) -> list[str]:
    def part_key(part: str) -> tuple[str, str]:
        eq = index_top_level_text(part, "=")
        name = part[:eq].strip() if eq >= 0 else part.strip()
        return binding_name_sort_key(name)

    return sorted(parts, key=part_key)


def expression_with_sorted_constants(expression: str) -> str:
    text = str(expression or "").strip()
    if not text:
        return text

    body, var_text, const_text = parse_expression_body(text)
    if not const_text:
        return text

    var_parts = [
        part.strip()
        for part in split_top_level_text(var_text, ",")
        if part.strip()
    ]
    const_parts = sorted_assignment_parts([
        part.strip()
        for part in split_top_level_text(const_text, ",")
        if part.strip()
    ])

    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}"


def restore_compact_binding_values(expression: str, source_expression: str) -> str:
    if "..." not in expression or not source_expression or "..." in source_expression:
        return expression

    body, var_text, const_text = parse_expression_body(expression)
    _, source_var_text, source_const_text = parse_expression_body(source_expression)
    source_values = {
        name: value
        for name, value in (
            parse_binding_assignments(source_var_text)
            + parse_binding_assignments(source_const_text)
        )
    }
    if not source_values:
        return expression

    changed = False

    def restore_assignments(assignments: str) -> list[str]:
        nonlocal changed
        out: list[str] = []
        for part in split_top_level_text(assignments, ","):
            eq = index_top_level_text(part, "=")
            if eq < 0:
                stripped = part.strip()
                if stripped:
                    out.append(stripped)
                continue

            name = part[:eq].strip()
            value = part[eq + 1:].strip()
            cached = source_values.get(name)
            if cached and value.endswith("...") and cached.startswith(value[:-3]):
                changed = True
                value = cached
            out.append(f"{name} = {value}")
        return out

    var_parts = restore_assignments(var_text)
    const_parts = sorted_assignment_parts(restore_assignments(const_text))
    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}" if changed else expression


def expression_with_binding_value(expression: str, target_name: str, value_text: str) -> str | None:
    body, var_text, const_text = parse_expression_body(expression)
    changed = False

    def replace_assignments(assignments: str) -> list[str]:
        nonlocal changed
        out: list[str] = []
        for part in split_top_level_text(assignments, ","):
            eq = index_top_level_text(part, "=")
            if eq < 0:
                stripped = part.strip()
                if stripped:
                    out.append(stripped)
                continue

            name = part[:eq].strip()
            value = part[eq + 1:].strip()
            if name == target_name:
                value = value_text
                changed = True
            out.append(f"{name} = {value}")
        return out

    var_parts = replace_assignments(var_text)
    const_parts = sorted_assignment_parts(replace_assignments(const_text))
    if not changed:
        return None

    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}"


def binding_syntax_error_details(raw: str) -> tuple[str, str] | None:
    first_line = str(raw or "").strip().splitlines()[0] if str(raw or "").strip() else ""
    match = re.match(r"^incorrect syntax for ([^:]+):\s*(.*)$", first_line)
    if not match:
        return None
    return match.group(1).strip(), match.group(2).strip()


def run_mars_lab_fields(
    binary: Path,
    expression: str,
    precision: int,
    wrt: str = "x",
) -> tuple[dict[str, str], str, int]:
    command = [str(binary), expression, wrt, str(max(17, precision))]
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_mars_lab_output(raw), raw, completed.returncode


def run_matrix_lab_fields(
    binary: Path,
    matrix_text: str,
    operation: str,
    precision: int,
    operand: str = "",
) -> tuple[dict[str, str], str, int]:
    command = [str(binary), matrix_text, operation, str(max(17, precision))]
    operand = str(operand or "").strip()
    if operand:
        command.append(operand)

    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_matrix_lab_output(raw), raw, completed.returncode


def run_integrator_lab_fields(
    binary: Path,
    expression: str,
    bounds: list[dict[str, str]],
    precision: int,
    max_intervals: int | None = None,
) -> tuple[dict[str, str], str, int]:
    effective_cap = max(MIN_INTEGRATOR_INTERVAL_CAP, min(MAX_INTEGRATOR_INTERVAL_CAP, int(max_intervals))) if max_intervals is not None else DEFAULT_INTEGRATOR_INTERVAL_CAP
    command = [str(binary)]
    if max_intervals is not None:
        command.extend(["--max-intervals", str(effective_cap)])
    command.extend([expression, str(max(17, precision))])
    for bound in bounds:
        command.extend([
            str(bound.get("name", "")).strip(),
            str(bound.get("lo", "")).strip(),
            str(bound.get("hi", "")).strip(),
        ])

    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=min(120, max(10, effective_cap // 500)),
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    if completed.returncode == 0:
        return parse_integrator_lab_output(raw), raw, completed.returncode

    fallback = integrator_endpoint_log_fallback(expression, bounds, precision, max_intervals)
    if fallback is not None:
        fields = fallback
        return fields, "Integration recovered via analytic endpoint-limit fallback", 0

    return parse_integrator_lab_output(raw), raw, completed.returncode


def run_equation_lab_fields(
    binary: Path,
    equation_text: str,
    precision: int,
) -> tuple[dict[str, str], str, int]:
    command = [
        str(binary),
        equation_text,
        str(max(17, precision)),
    ]
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_equation_lab_output(raw), raw, completed.returncode


def _parse_decimal_bound(text: str) -> Decimal | None:
    try:
        return Decimal(str(text).strip())
    except (InvalidOperation, ValueError):
        return None


def integrator_endpoint_log_fallback(
    expression: str,
    bounds: list[dict[str, str]],
    precision: int,
    max_intervals: int | None = None,
) -> dict[str, str] | None:
    if len(bounds) != 1:
        return None

    expr_text = str(expression or "").strip()
    match = re.fullmatch(r"(ln|log|lg|log10)\(\s*([A-Za-z][A-Za-z0-9_]*)\s*\)", expr_text)
    if not match:
        return None

    func_name, var_name = match.groups()
    bound = bounds[0]
    if str(bound.get("name", "")).strip() != var_name:
        return None

    lo_text = str(bound.get("lo", "")).strip()
    hi_text = str(bound.get("hi", "")).strip()
    lo = _parse_decimal_bound(lo_text)
    hi = _parse_decimal_bound(hi_text)
    if lo is None or hi is None:
        return None

    if lo == hi:
        value = Decimal(0)
    else:
        with localcontext() as ctx:
            ctx.prec = max(precision + 20, 60)

            def zero_endpoint_log_integral(upper: Decimal) -> Decimal | None:
                if upper <= 0:
                    return None
                value_out = upper * upper.ln() - upper
                if func_name != "ln":
                    value_out /= Decimal(10).ln()
                return +value_out

            if lo.is_zero() and hi > 0:
                computed = zero_endpoint_log_integral(hi)
            elif hi.is_zero() and lo > 0:
                computed = zero_endpoint_log_integral(lo)
                computed = -computed if computed is not None else None
            else:
                return None

            if computed is None:
                return None
            value = +computed

    log_tex = "\\ln" if func_name == "ln" else "\\log"
    return {
        "input": expr_text,
        "expression": expr_text,
        "dimensions": "1",
        "bound": f"{var_name} in [{lo_text}, {hi_text}]",
        "tex": rf"\int_{{{lo_text}}}^{{{hi_text}}} {log_tex}({var_name})\, d{var_name}",
        "value": str(value),
        "error": "0",
        "intervals": "0",
        "max_intervals": str(max_intervals or DEFAULT_INTEGRATOR_INTERVAL_CAP),
        "status": "analytic endpoint limit",
    }


def matrix_failure_hint(
    binary: Path,
    matrix_text: str,
    operation: str,
    precision: int,
    operand: str = "",
) -> str:
    if operation != "inverse":
        return ""

    try:
        eval_fields, _, eval_rc = run_matrix_lab_fields(
            binary, matrix_text, "eval", precision, ""
        )
    except Exception:
        return ""

    if eval_rc != 0:
        return "This matrix still has unresolved symbolic entries. Bind variables first or use Evaluate."

    rows = str(eval_fields.get("rows", "")).strip()
    cols = str(eval_fields.get("cols", "")).strip()
    if rows and cols and rows != cols:
        return (
            f"This matrix is {rows}x{cols}, so it has no inverse. "
            "Only square n x n matrices can be inverted."
        )

    try:
        det_fields, _, det_rc = run_matrix_lab_fields(
            binary, matrix_text, "det", precision, ""
        )
    except Exception:
        return ""

    if det_rc == 0 and str(det_fields.get("value", "")).strip() == "0":
        return "This matrix is singular: det(A) = 0, so it has no inverse. Equivalently, 0 is an eigenvalue."

    return "The inverse operation failed for this matrix."


def expression_variable_binding_values(
    expression: str,
    precision: int | None = None,
) -> list[dict[str, str]]:
    _, var_text, const_text = parse_expression_body(expression)
    values: list[dict[str, str]] = []

    for name, value in parse_binding_assignments(var_text):
        if not value or value == "?" or value.upper() == "NAN":
            values.append({
                "name": name,
                "value": value or "?",
                "display": "",
                "kind": "variable",
            })
            continue

        display_value = precision_numeric_tokens(value, precision) if precision is not None else value
        values.append({
            "name": name,
            "value": display_value,
            "display": _compact_long_text_value(display_value),
            "kind": "variable",
        })

    for name, value in sorted_binding_assignments(parse_binding_assignments(const_text)):
        display_value = value or "?"
        display = ""
        if display_value != "?" and display_value.upper() != "NAN":
            display = _compact_long_text_value(display_value)
        values.append({
            "name": name,
            "value": display_value,
            "display": display,
            "kind": "constant",
        })

    return values


def goal_seek_expression(
    binary: Path,
    expression: str,
    target_text: str,
    precision: int,
    start_values: object = None,
) -> tuple[str, dict[str, str]]:
    command = [
        str(binary),
        "--goal-seek",
        expression,
        target_text,
        str(max(17, precision)),
    ]

    if start_values:
        if not isinstance(start_values, dict):
            raise ValueError("Start values must be supplied as binding assignments")
        for name, value in start_values.items():
            name_text = str(name).strip()
            value_text = str(value).strip()
            if not name_text or not value_text:
                continue
            command.append(f"{name_text}={value_text}")

    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr

    fields = parse_mars_lab_output(raw)
    if completed.returncode != 0:
        raise ValueError(raw or f"mars_lab exited with {completed.returncode}")

    expression_out = fields.get("expression", "").strip()
    if not expression_out:
        raise ValueError(raw or "Goal seek did not return an expression")
    return expression_out, fields


def prepare_evaluation_fields(
    binary: Path,
    fields: dict[str, str],
    expression: str,
    precision: int,
    save_expression: bool,
    wrt: str = "x",
) -> dict[str, object]:
    if fields.get("value"):
        fields["value"] = format_number_text_for_precision(
            fields["value"], precision, zero_subprecision=True)
    if fields.get("derivative_value"):
        fields["derivative_value"] = format_number_text_for_precision(
            fields["derivative_value"], precision, zero_subprecision=True
        )

    precision_limit_result_fields(fields, precision)
    fields["editor_expression"] = editor_expression_from_fields(fields)
    if save_expression and fields.get("expression"):
        save_state_expression(expression_for_editor(expression))

    fields["full_display_expression"] = expression_for_display(fields.get("expression", ""))
    fields["full_display_tex"] = tex_for_display(fields.get("tex", ""))
    fields["full_display_function"] = function_for_display(fields.get("function", ""))
    fields["display_expression"] = compact_display_text(str(fields["full_display_expression"]))
    fields["display_tex"] = compact_display_text(str(fields["full_display_tex"]))
    fields["display_function"] = compact_function_text(str(fields["full_display_function"]))
    derivative_tex = tex_for_display(str(fields.get("derivative_tex") or ""))
    fields["derivative_tex"] = derivative_tex
    if derivative_tex:
        derivative_svg, derivative_render_error = render_tex_to_svg(derivative_tex)
        if derivative_svg:
            fields["derivative_svg"] = derivative_svg
        elif derivative_render_error:
            fields["derivative_render_error"] = derivative_render_error
    fields["binding_values"] = expression_variable_binding_values(
        fields.get("expression", "") or expression,
        precision,
    )

    svg, render_error = render_tex_to_svg(str(fields.get("display_tex", "")))
    if svg:
        fields["svg"] = svg
    elif render_error:
        fields["render_error"] = render_error

    return fields


def prepare_matrix_fields(fields: dict[str, str]) -> dict[str, object]:
    result_text = str(fields.get("result") or fields.get("value") or "").strip()
    pretty_text = str(fields.get("pretty") or "").strip()
    tex = str(fields.get("tex") or "").strip()
    operation = str(fields.get("operation") or "eval").strip()
    kind = str(fields.get("kind") or "").strip()
    rows = str(fields.get("rows") or "").strip()
    cols = str(fields.get("cols") or "").strip()

    svg = None
    render_error = None
    if tex and tex != "(null)":
        svg, render_error = render_tex_to_svg(tex)

    summary_parts = []
    if kind:
        summary_parts.append(kind)
    if rows and cols:
        summary_parts.append(f"{rows}x{cols}")
    if operation:
        summary_parts.append(operation)

    payload: dict[str, object] = {
        "ok": True,
        "mode": "matrix",
        "operation": operation,
        "result": result_text,
        "pretty": pretty_text,
        "tex": "" if tex == "(null)" else tex,
        "summary": " · ".join(summary_parts),
    }
    if svg:
        payload["svg"] = svg
    elif render_error:
        payload["render_error"] = render_error
    return payload


def prepare_integrator_fields(fields: dict[str, str], precision: int) -> dict[str, object]:
    if fields.get("value"):
        fields["value"] = format_number_text_for_precision(
            str(fields["value"]), precision, zero_subprecision=True)
    if fields.get("error"):
        fields["error"] = format_number_text_for_precision(
            str(fields["error"]), precision, zero_subprecision=True)
    if fields.get("symbolic_value"):
        fields["symbolic_value"] = format_number_text_for_precision(
            str(fields["symbolic_value"]), precision, zero_subprecision=True)
    if fields.get("error"):
        fields["error"] = format_number_text_for_precision(
            str(fields["error"]),
            min(int(precision), INTEGRATOR_ERROR_DISPLAY_DIGITS),
            zero_subprecision=False,
        )
    precision_limit_result_fields(fields, precision)

    tex = str(fields.get("tex") or "").strip()
    bounds = str(fields.get("bound") or "").strip()
    tex = integrator_tex_for_display(tex)

    svg = None
    render_error = None
    if tex and tex != "(null)":
        svg, render_error = render_tex_to_svg(tex)

    payload: dict[str, object] = {
        "ok": True,
        "mode": "integrator",
        "expression": str(fields.get("expression") or "").strip(),
        "tex": "" if tex == "(null)" else tex,
        "antiderivative": str(fields.get("antiderivative") or "").strip(),
        "antiderivative_tex": str(fields.get("antiderivative_tex") or "").strip(),
        "symbolic": str(fields.get("symbolic") or "").strip(),
        "symbolic_tex": str(fields.get("symbolic_tex") or "").strip(),
        "symbolic_value": str(fields.get("symbolic_value") or "").strip(),
        "value": str(fields.get("value") or fields.get("symbolic_value") or "").strip(),
        "error": str(fields.get("error") or "").strip(),
        "error": str(fields.get("error") or "").strip(),
        "intervals": str(fields.get("intervals") or "").strip(),
        "max_intervals": str(fields.get("max_intervals") or "").strip(),
        "work_units": str(fields.get("work_units") or fields.get("intervals") or "").strip(),
        "work_cap": str(fields.get("work_cap") or fields.get("max_intervals") or "").strip(),
        "status": str(fields.get("status") or "").strip(),
        "dimensions": str(fields.get("dimensions") or "").strip(),
        "bound": bounds,
        "bound_var": str(fields.get("bound_var") or "").strip().splitlines()[0] if str(fields.get("bound_var") or "").strip() else "",
        "bound_lower": str(fields.get("bound_lower") or "").strip().splitlines()[0] if str(fields.get("bound_lower") or "").strip() else "",
        "bound_upper": str(fields.get("bound_upper") or "").strip().splitlines()[0] if str(fields.get("bound_upper") or "").strip() else "",
    }
    if svg:
        payload["svg"] = svg
    elif render_error:
        payload["render_error"] = render_error
    return payload


def prepare_equation_fields(fields: dict[str, str], precision: int) -> dict[str, object]:
    if fields.get("value"):
        fields["value"] = format_number_text_for_precision(
            str(fields["value"]), precision, zero_subprecision=True)

    for key in ("equation", "unbound", "tex", "residual", "solutions", "solutions_tex"):
        value = str(fields.get(key) or "")
        if not value:
            continue
        fields[f"raw_{key}"] = value
        fields[key] = precision_numeric_tokens(value, precision)

    source_equation_text = str(fields.get("input") or "").strip()
    equation_text = expression_with_sorted_constants(str(fields.get("equation") or "").strip())
    unbound_text = str(fields.get("unbound") or "").strip()
    solutions_text = normalize_multiline_display_text(fields.get("solutions") or "")
    equation_tex = str(fields.get("tex") or "").strip()
    solutions_tex = str(fields.get("solutions_tex") or "").strip()
    render_tex = solutions_tex or equation_tex
    display_tex = compact_display_text(render_tex)
    solution_lines = [line.strip() for line in solutions_text.splitlines() if line.strip()]

    svg = None
    render_error = None
    if display_tex and display_tex != "(null)":
        svg, render_error = render_tex_to_svg(display_tex)

    payload: dict[str, object] = {
        "ok": True,
        "mode": "equation",
        "equation": equation_text,
        "unbound": unbound_text,
        "tex": "" if render_tex == "(null)" else render_tex,
        "equation_tex": "" if equation_tex == "(null)" else equation_tex,
        "solutions_tex": "" if solutions_tex == "(null)" else solutions_tex,
        "residual": str(fields.get("residual") or "").strip(),
        "value": str(fields.get("value") or "").strip(),
        "status": str(fields.get("status") or "").strip(),
        "solutions": solutions_text,
        "solution_count": len(solution_lines),
        "full_display_equation": expression_for_display(unbound_text or equation_text),
        "display_equation": compact_display_text(expression_for_display(unbound_text or equation_text)),
        "full_display_tex": render_tex,
        "display_tex": display_tex,
        "binding_values": expression_variable_binding_values(source_equation_text or equation_text, precision),
    }
    if svg:
        payload["svg"] = svg
    elif render_error:
        payload["render_error"] = render_error
    return payload


def integrator_tex_for_display(tex: str) -> str:
    tex = str(tex or "").strip()
    if not tex:
        return ""

    binding_wrapper = re.compile(
        r"\\left\\\{\s*(.*?)\s*\\;\\middle\|\\;.*?\\right\\\}"
    )

    def replace_wrapper(match: re.Match[str]) -> str:
        body = match.group(1).strip()
        return body or match.group(0)

    previous = None
    while previous != tex:
        previous = tex
        tex = binding_wrapper.sub(replace_wrapper, tex)
    return tex


class MarsLabHandler(http.server.BaseHTTPRequestHandler):
    binary: Path = DEFAULT_BIN
    equation_binary: Path = DEFAULT_EQUATION_BIN
    matrix_binary: Path = DEFAULT_MATRIX_BIN
    integrator_binary: Path = DEFAULT_INTEGRATOR_BIN
    server_host: str = "127.0.0.1"
    server_port: int = 0
    mobile_url: str = ""

    def log_message(self, fmt: str, *args: object) -> None:
        try:
            print(f"mars_lab: {fmt % args}", file=sys.stderr)
        except OSError:
            pass

    def send_json(self, status: int, payload: dict[str, object]) -> None:
        data = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_file(self, path: Path, content_type: str) -> None:
        try:
            data = path.read_bytes()
        except OSError:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def request_allowed(self) -> bool:
        if request_uses_public_funnel_host(self.headers.get("Host", "")):
            self.send_error(403, f"{LAB_APP_NAME} is private. Use WiFi or Tailscale.")
            return False
        if request_allows_lab_access(str(self.client_address[0])):
            return True
        self.send_error(403, f"{LAB_APP_NAME} is only available on this machine, local WiFi, or Tailscale.")
        return False

    def do_GET(self) -> None:
        if not self.request_allowed():
            return
        parsed_path = urllib.parse.urlparse(self.path)
        path = parsed_path.path
        if path == "/state":
            self.send_json(200, load_state_data())
            return

        if path == "/mobile-access":
            control_allowed = request_allows_funnel_control(
                self.headers,
                str(self.client_address[0]),
                _control_token_from_query(self.path),
            )
            details = mobile_access_details(
                self.server_host,
                self.server_port,
                self.headers.get("Host", ""),
                control_allowed,
            )
            details["qr"] = mobile_qr_svg(
                str(details["url"]),
                bool(details.get("control")),
            ) if details.get("url") else ""
            self.send_json(200, details)
            return

        if path == "/favicon.svg":
            self.send_file(LAB_FAVICON_FILE, "image/svg+xml")
            return

        if path == "/apple-touch-icon.png":
            self.send_file(LAB_TOUCH_ICON_FILE, "image/png")
            return

        if path == "/icon-192.png":
            self.send_file(LAB_ICON_192_FILE, "image/png")
            return

        if path == "/icon-512.png":
            self.send_file(LAB_ICON_512_FILE, "image/png")
            return

        if path == "/manifest.webmanifest":
            self.send_json(200, WEB_MANIFEST)
            return

        if path not in ("/", "/index.html"):
            self.send_error(404)
            return

        control_allowed = request_allows_funnel_control(
            self.headers,
            str(self.client_address[0]),
            _control_token_from_query(self.path),
        )
        mobile_details = mobile_access_details(
            self.server_host,
            self.server_port,
            self.headers.get("Host", ""),
            control_allowed,
        )
        mobile_url = str(mobile_details["url"])
        mobile_qr = mobile_qr_svg(
            mobile_url,
            bool(mobile_details.get("control")),
        ) if mobile_url else ""
        page = (
            INDEX_HTML.replace(
                "__INITIAL_EXPRESSION__",
                html.escape(load_state_expression(), quote=False),
            )
            .replace("__MOBILE_TITLE__", html.escape(mobile_details["title"], quote=False))
            .replace("__MOBILE_HINT__", html.escape(mobile_details["hint"], quote=False))
            .replace("__MOBILE_URL__", html.escape(mobile_url, quote=False))
            .replace("__MOBILE_QR_SVG__", mobile_qr)
            .replace("__MOBILE_CARD_CLASS__", "")
            .replace("__MOBILE_TAILSCALE_CLASS__", "" if mobile_details.get("tailscale") else "hidden")
            .replace("__DEFAULT_EXPRESSION__", json.dumps(DEFAULT_EXPRESSION))
            .replace("__DEFAULT_EQUATION__", json.dumps(DEFAULT_EQUATION))
            .replace("__DEFAULT_EQUATION_VARIABLE__", json.dumps(DEFAULT_EQUATION_VARIABLE))
            .replace("__DEFAULT_MATRIX__", json.dumps(DEFAULT_MATRIX))
            .replace("__DEFAULT_INTEGRATOR__", json.dumps(DEFAULT_INTEGRATOR_EXPRESSION))
            .replace("__DEFAULT_INTEGRATOR_BOUNDS__", json.dumps(DEFAULT_INTEGRATOR_BOUNDS))
            .replace("__DEFAULT_INTEGRATOR_INTERVAL_CAP__", json.dumps(DEFAULT_INTEGRATOR_INTERVAL_CAP))
            .replace("__CONTROL_TOKEN__", json.dumps(CONTROL_TOKEN if control_allowed else ""))
        )
        data = page.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        if _control_token_from_query(self.path) == CONTROL_TOKEN:
            self.send_header(
                "Set-Cookie",
                f"{CONTROL_COOKIE}={urllib.parse.quote(CONTROL_TOKEN)}; Path=/; SameSite=Lax",
            )
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self) -> None:
        if not self.request_allowed():
            return
        path = urllib.parse.urlparse(self.path).path
        if path == "/funnel-toggle":
            self.send_json(410, {"ok": False, "error": "Public access switching is disabled in MARS Lab."})
            return

        if path == "/state":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                updates: dict[str, object] = {}

                expression = str(payload.get("expression", "")).strip()
                if expression and "..." not in expression:
                    updates["expression"] = expression

                matrix = str(payload.get("matrix", "")).strip()
                if matrix and "..." not in matrix:
                    updates["matrix"] = matrix

                lab_mode = str(payload.get("lab_mode", "")).strip()
                if lab_mode in {"expression", "equation", "matrix", "integrator"}:
                    updates["lab_mode"] = lab_mode

                equation = str(payload.get("equation", "")).strip()
                if equation and "..." not in equation:
                    updates["equation"] = equation

                equation_variable = str(payload.get("equation_variable", "")).strip()
                if equation_variable:
                    updates["equation_variable"] = equation_variable

                matrix_operation = str(payload.get("matrix_operation", "")).strip()
                if matrix_operation in {
                    "eval",
                    "inverse",
                    "eigenvalues",
                    "eigendecompose",
                    "charpoly",
                    "det",
                    "trace",
                    "rank",
                    "simplify",
                    "solve",
                }:
                    updates["matrix_operation"] = matrix_operation

                if "matrix_operand" in payload:
                    updates["matrix_operand"] = str(payload.get("matrix_operand", "")).strip()

                integrator_expression = str(payload.get("integrator_expression", "")).strip()
                if integrator_expression and "..." not in integrator_expression:
                    updates["integrator_expression"] = integrator_expression

                integrator_bounds = str(payload.get("integrator_bounds", "")).strip()
                if integrator_bounds:
                    updates["integrator_bounds"] = integrator_bounds

                if "integrator_interval_cap" in payload:
                    cap = int(payload.get("integrator_interval_cap", DEFAULT_INTEGRATOR_INTERVAL_CAP))
                    cap = max(MIN_INTEGRATOR_INTERVAL_CAP, min(MAX_INTEGRATOR_INTERVAL_CAP, cap))
                    if cap in INTEGRATOR_INTERVAL_CAP_CHOICES:
                        updates["integrator_interval_cap"] = cap

                if isinstance(payload.get("precision_bits"), dict):
                    saved_precision = load_state_data().get("precision_bits", {})
                    precision_bits = dict(saved_precision) if isinstance(saved_precision, dict) else {}
                    for mode in ("expression", "equation", "matrix", "integrator"):
                        if mode in payload["precision_bits"]:
                            bits = int(payload["precision_bits"][mode])
                            precision_bits[mode] = max(17, min(MAX_VALUE_PRECISION_BITS, bits))
                    updates["precision_bits"] = precision_bits
                elif "precision_bits" in payload:
                    bits = int(payload["precision_bits"])
                    updates["precision_bits"] = {
                        "expression": max(17, min(MAX_VALUE_PRECISION_BITS, bits)),
                        "equation": 256,
                        "matrix": 256,
                        "integrator": 17,
                    }

                if updates:
                    save_state_data(updates)
                self.send_json(200, {"ok": True})
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
            return

        if path == "/render_tex":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                tex = str(payload.get("tex", "")).strip()
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            svg, render_error = render_tex_to_svg(tex)
            if svg:
                self.send_json(200, {"ok": True, "svg": svg})
            else:
                self.send_json(422, {
                    "ok": False,
                    "error": render_error or "Could not render TeX",
                })
            return

        if path == "/equation-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                equation_text = str(payload.get("equation", "")).strip()
                precision = int(payload.get("precision", 96))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not equation_text:
                self.send_json(400, {"ok": False, "error": "Equation input is empty"})
                return

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                ensure_scratch_binary(self.equation_binary, "scratch/equation_lab")
                fields, raw, returncode = run_equation_lab_fields(
                    self.equation_binary,
                    equation_text,
                    precision,
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if returncode != 0:
                self.send_json(422, {"ok": False, "error": raw or "Equation solving failed"})
                return

            save_state_data({
                "equation": equation_text,
            })
            self.send_json(200, prepare_equation_fields(fields, precision))
            return

        if path == "/matrix-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                matrix_text = str(payload.get("matrix", "")).strip()
                operation = str(payload.get("operation", "eval")).strip() or "eval"
                operand = str(payload.get("operand", "")).strip()
                precision = int(payload.get("precision", 96))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not matrix_text:
                self.send_json(400, {"ok": False, "error": "Matrix input is empty"})
                return

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                ensure_scratch_binary(self.matrix_binary, "scratch/matrix_lab")
                fields, raw, returncode = run_matrix_lab_fields(
                    self.matrix_binary,
                    matrix_text,
                    operation,
                    precision,
                    operand,
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if returncode != 0:
                hint = matrix_failure_hint(
                    self.matrix_binary,
                    matrix_text,
                    operation,
                    precision,
                    operand,
                )
                message = hint or raw or "Matrix evaluation failed"
                self.send_json(422, {"ok": False, "error": message})
                return

            save_state_data({
                "matrix": matrix_text,
                "matrix_operation": operation,
                "matrix_operand": operand,
            })
            self.send_json(200, prepare_matrix_fields(fields))
            return

        if path == "/integrator-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                expression = str(payload.get("expression", "")).strip()
                bounds = payload.get("bounds", [])
                precision = int(payload.get("precision", 96))
                max_intervals = int(payload.get("max_intervals", DEFAULT_INTEGRATOR_INTERVAL_CAP))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not expression:
                self.send_json(400, {"ok": False, "error": "Integrand expression is empty"})
                return
            if not isinstance(bounds, list):
                self.send_json(400, {"ok": False, "error": "Bounds must be a list"})
                return
            if not bounds:
                bounds = [{"name": "x", "lo": "", "hi": ""}]

            cleaned_bounds: list[dict[str, str]] = []
            for item in bounds:
                if not isinstance(item, dict):
                    self.send_json(400, {"ok": False, "error": "Bounds must be name/lo/hi objects"})
                    return
                name = str(item.get("name", "")).strip()
                lo_text = str(item.get("lo", "")).strip()
                hi_text = str(item.get("hi", "")).strip()
                if not name:
                    self.send_json(400, {"ok": False, "error": "Every bound needs a variable name"})
                    return
                if lo_text and not hi_text:
                    self.send_json(400, {"ok": False, "error": "A one-sided bound should be entered as an upper value"})
                    return
                cleaned_bounds.append({"name": name, "lo": lo_text, "hi": hi_text})

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                max_intervals = max(MIN_INTEGRATOR_INTERVAL_CAP, min(MAX_INTEGRATOR_INTERVAL_CAP, max_intervals))
                ensure_scratch_binary(self.integrator_binary, "scratch/integrator_lab")
                fields, raw, returncode = run_integrator_lab_fields(
                    self.integrator_binary,
                    expression,
                    cleaned_bounds,
                    precision,
                    max_intervals,
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if returncode != 0:
                self.send_json(422, {"ok": False, "error": raw or "Integration failed"})
                return

            bounds_text = "\n".join(
                f"{item['name']} = {item['lo']} .. {item['hi']}"
                if item["lo"] and item["hi"]
                else (f"{item['name']} = {item['hi']}" if item["hi"] else item["name"])
                for item in cleaned_bounds
            )
            save_state_data({
                "integrator_expression": expression,
                "integrator_bounds": bounds_text,
                "integrator_interval_cap": max_intervals,
            })
            self.send_json(200, prepare_integrator_fields(fields, precision))
            return

        if path == "/goal_seek":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                expression = str(payload.get("expression", "")).strip()
                target = str(payload.get("target", "0")).strip() or "0"
                start = payload.get("start", {})
                precision = int(payload.get("precision", 96))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not expression:
                self.send_json(400, {"ok": False, "error": "Expression is empty"})
                return

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                expression = restore_compact_binding_values(expression, load_state_expression())
                solved, fields = goal_seek_expression(self.binary, expression, target, precision, start)
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            fields["ok"] = True
            fields["expression"] = solved
            fields["precision"] = precision
            precision_limit_result_fields(fields, precision)
            fields["editor_expression"] = editor_expression_from_fields(fields)
            save_state_expression(fields["editor_expression"])
            if fields.get("value"):
                fields["value"] = format_number_text_for_precision(
                    fields["value"], precision, zero_subprecision=True)
            if fields.get("residual"):
                fields["residual"] = format_number_text_for_precision(
                    fields["residual"], precision, zero_subprecision=True)
            fields["full_display_expression"] = expression_for_display(fields.get("expression", ""))
            fields["full_display_tex"] = tex_for_display(fields.get("tex", ""))
            fields["full_display_function"] = function_for_display(fields.get("function", ""))
            fields["display_expression"] = compact_display_text(fields["full_display_expression"])
            fields["display_tex"] = compact_display_text(fields["full_display_tex"])
            fields["display_function"] = compact_function_text(fields["full_display_function"])
            fields["binding_values"] = expression_variable_binding_values(
                fields.get("expression", "") or solved,
                precision,
            )
            svg, render_error = render_tex_to_svg(fields.get("display_tex", ""))
            if svg:
                fields["svg"] = svg
            elif render_error:
                fields["render_error"] = render_error
            self.send_json(200, fields)
            return

        if path != "/eval":
            self.send_error(404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length)
            payload = json.loads(body.decode("utf-8"))
            expression = str(payload.get("expression", "")).strip()
            wrt = str(payload.get("wrt", "")).strip() or "x"
            precision = int(payload.get("precision", 96))
        except Exception as exc:
            self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
            return

        if not expression:
            self.send_json(400, {"ok": False, "error": "Expression is empty"})
            return
        precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
        expression = restore_compact_binding_values(expression, load_state_expression())

        try:
            command = [str(self.binary), expression]
            command.extend([wrt, str(precision)])
            completed = subprocess.run(
                command,
                cwd=ROOT,
                text=True,
                capture_output=True,
                timeout=10,
            )
        except Exception as exc:
            self.send_json(500, {"ok": False, "error": str(exc)})
            return

        raw = completed.stdout
        if completed.stderr:
            raw = raw + ("\n" if raw else "") + completed.stderr

        fields = parse_mars_lab_output(raw)
        fields["ok"] = completed.returncode == 0
        if completed.returncode != 0:
            binding_error = binding_syntax_error_details(raw)
            if binding_error:
                binding_name, _ = binding_error
                fallback_expression = expression_with_binding_value(expression, binding_name, "NAN")
                if fallback_expression:
                    fallback_fields, fallback_raw, fallback_rc = run_mars_lab_fields(
                        self.binary,
                        fallback_expression,
                        precision,
                        wrt,
                    )
                    if fallback_rc == 0:
                        fallback_fields["ok"] = True
                        fallback_fields["partial_error"] = True
                        fallback_fields["error"] = raw.strip()
                        fallback_fields["raw"] = raw
                        fallback_fields["recovery_expression"] = fallback_expression
                        prepare_evaluation_fields(
                            self.binary,
                            fallback_fields,
                            fallback_expression,
                            precision,
                            save_expression=False,
                            wrt=wrt,
                        )
                        fallback_fields["binding_values"] = expression_variable_binding_values(
                            expression,
                            precision,
                        )
                        self.send_json(200, fallback_fields)
                        return
                    fields["recovery_raw"] = fallback_raw
            fields["raw"] = raw
            fields["error"] = raw or f"mars_lab exited with {completed.returncode}"
            self.send_json(422, fields)
            return

        prepare_evaluation_fields(
            self.binary,
            fields,
            expression,
            precision,
            save_expression=True,
            wrt=wrt,
        )

        self.send_json(200, fields)


def open_lab_url(url: str, browser: str = "") -> None:
    if browser:
        try:
            process = subprocess.Popen(
                [browser, url],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
            )
            try:
                if process.wait(timeout=0.5) == 0:
                    return
            except subprocess.TimeoutExpired:
                return
        except OSError:
            pass

    # Prefer desktop URI openers so the user's configured default browser is
    # used.  Drop BROWSER for these commands because some environments set it
    # to a stale Firefox path, which makes xdg-open look non-default and fail.
    opener_env = os.environ.copy()
    opener_env.pop("BROWSER", None)
    for command in (
        ("gio", "open", url),
        ("kde-open6", url),
        ("kde-open5", url),
        ("xdg-open", url),
    ):
        if shutil.which(command[0]):
            subprocess.Popen(
                list(command),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
                env=opener_env,
            )
            return

    webbrowser.open(url)


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Launch the local {LAB_APP_NAME}.")
    parser.add_argument("--host", default="::", help="host to bind")
    parser.add_argument("--port", type=int, default=0, help="port to bind, or 0 for auto")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser automatically")
    parser.add_argument("--browser", default="", help="browser executable to open the lab URL")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BIN, help="path to the scratch lab binary")
    parser.add_argument("--equation-binary", type=Path, default=DEFAULT_EQUATION_BIN, help="path to the equation scratch binary")
    args = parser.parse_args()

    binary = args.binary if args.binary.is_absolute() else ROOT / args.binary
    equation_binary = args.equation_binary if args.equation_binary.is_absolute() else ROOT / args.equation_binary
    ensure_mars_lab(binary)

    MarsLabHandler.binary = binary
    MarsLabHandler.equation_binary = equation_binary

    port = args.port or find_free_port(args.host)
    MarsLabHandler.server_host = args.host
    MarsLabHandler.server_port = port
    try:
        server = create_threading_http_server(args.host, port, MarsLabHandler)
    except OSError as exc:
        if exc.errno == errno.EADDRINUSE:
            ensure_tailscale_serve(args.host, port)
            MarsLabHandler.mobile_url = mobile_access_url(args.host, port)
            url = browser_access_url(args.host, port)
            print(f"{LAB_APP_NAME} already running at {url}")
            if not args.no_browser:
                open_lab_url(_control_url(url), args.browser)
            return 0
        raise

    ensure_tailscale_serve(args.host, port)
    MarsLabHandler.mobile_url = mobile_access_url(args.host, port)
    url = browser_access_url(args.host, port)
    print(f"{LAB_APP_NAME} running at {url}")
    if MarsLabHandler.mobile_url:
        print(f"Mobile access: {MarsLabHandler.mobile_url}")
    print("Press Ctrl+C to stop.")

    if not args.no_browser:
        threading.Timer(0.25, open_lab_url, args=(_control_url(url), args.browser)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print(f"\nStopping {LAB_APP_NAME}.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
