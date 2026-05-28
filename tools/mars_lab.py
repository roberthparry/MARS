#!/usr/bin/env python3
"""
Small local GUI for experimenting with MARS mathematics.

The app serves a single browser page on localhost and evaluates expressions by
delegating to build/release/scratch/mars_lab.  The browser gives us a proper
GUI surface without adding a desktop toolkit dependency to the project.
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
DEFAULT_BIN = ROOT / "build" / "release" / "scratch" / "mars_lab"
STATE_FILE = ROOT / ".mars_lab_state.json"
LAB_ICON_FILE = ROOT / "packaging" / "linux" / "mars-lab.svg"
LAB_FAVICON_FILE = LAB_ICON_FILE
LAB_TOUCH_ICON_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-180.png"
LAB_ICON_192_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-192.png"
LAB_ICON_512_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-512.png"
DEFAULT_EXPRESSION = "{e^(sin(x))|x=pi/2}"
MAX_VALUE_PRECISION_BITS = 1_048_576
MAX_VALUE_PRECISION_DIGITS = math.ceil(MAX_VALUE_PRECISION_BITS * math.log10(2))
COMPACT_BINDING_VALUE_LIMIT = 20
COMPACT_BINDING_VALUE_KEEP = 16
QR_VERSION = 4
QR_SIZE = 17 + 4 * QR_VERSION
QR_DATA_CODEWORDS = 80
QR_EC_CODEWORDS = 20
QR_EC_LEVEL_L = 1
QR_MASK_PATTERN = 0
CONTROL_TOKEN = os.environ.get("MARS_LAB_CONTROL_TOKEN") or secrets.token_urlsafe(24)
CONTROL_QUERY_PARAM = "mars_lab_control"
CONTROL_COOKIE = "mars_lab_control"


INDEX_HTML = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>MARS Lab</title>
  <link rel="icon" type="image/svg+xml" href="/favicon.svg">
  <link rel="apple-touch-icon" sizes="180x180" href="/apple-touch-icon.png">
  <link rel="icon" type="image/png" sizes="192x192" href="/icon-192.png">
  <link rel="icon" type="image/png" sizes="512x512" href="/icon-512.png">
  <link rel="manifest" href="/manifest.webmanifest">
  <meta name="theme-color" content="#071913">
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

    .mobile-funnel-toggle {
      width: 100%;
      min-width: 7.75rem;
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
      border: 2px solid rgba(233, 244, 239, 0.28);
      border-radius: 18px;
      background: rgba(8, 29, 22, 0.62);
      overflow: hidden;
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

    pre {
      margin: 0;
      padding: 0.9rem;
      overflow: auto;
      min-height: 2.75rem;
    }

    pre {
      color: var(--code);
      white-space: pre-wrap;
      font: 0.92rem/1.45 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    #value {
      overflow-x: hidden;
      overflow-y: visible;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      word-break: break-all;
    }

    .rendered {
      margin: 0;
      min-height: 12rem;
      padding: 2.1rem 1.6rem 3rem;
      overflow-x: auto;
      overflow-y: visible;
      font-size: 1.78rem;
    }

    .rendered svg {
      display: block;
      max-width: 100%;
      height: auto;
      overflow: visible;
      transform: scale(2);
      transform-origin: left top;
      margin-bottom: 5rem;
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
        font-size: 1.3rem;
      }

      .rendered svg {
        transform: scale(1.35);
        margin-bottom: 2.5rem;
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

      .mobile-funnel-toggle {
        width: auto;
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
        font-size: 1.12rem;
      }

      .rendered svg {
        transform: scale(1.12);
        margin-bottom: 2rem;
      }

      .help-pane {
        padding: 0.75rem;
      }

      .help-card {
        padding: 0.8rem;
      }
    }
  </style>
</head>
<body>
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
      <h1>MARS Lab</h1>
      <p class="subtitle">Type a MARS expression on the left. The tool evaluates it through your built scratch binary and renders the generated TeX on the right.</p>
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
            <button class="card-action mobile-funnel-toggle __MOBILE_TAILSCALE_CLASS__" id="funnelToggleButton" type="button">Make private</button>
          </div>
        </div>
      </details>
    </div>
  </header>
  <main>
    <section>
      <div class="panel-head">
        <h2>Expression</h2>
      </div>
      <textarea id="expr" spellcheck="false">__INITIAL_EXPRESSION__</textarea>
      <div class="target-row hidden" id="targetRow">
        <label for="goalTarget">Target</label>
        <input id="goalTarget" spellcheck="false" value="0">
        <div class="goal-start-fields" id="goalStartFields"></div>
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
        <div class="card">
          <div class="card-title expandable-title">
            <span>Rendered TeX</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="renderedMore">Show more digits</button>
            </span>
            <button class="card-action copy-result top-card-copy" id="renderedCopy" data-copy-target="rendered">Copy</button>
          </div>
          <div class="rendered" id="rendered"></div>
        </div>
        <div class="card mobile-result-extra">
          <div class="card-title expandable-title">
            <span>Expression</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="parsedMore">Show more digits</button>
            </span>
            <button class="card-action copy-result top-card-copy" data-copy-target="expression">Copy</button>
          </div>
          <pre id="parsed"></pre>
        </div>
        <div class="card mobile-result-extra">
          <div class="card-title expandable-title">
            <span>Function</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="functionMore">Show more digits</button>
            </span>
            <button class="card-action copy-result top-card-copy" data-copy-target="function">Copy</button>
          </div>
          <pre id="functionStyle"></pre>
        </div>
        <div class="card">
          <div class="card-title value-title">
            <span>Value</span>
            <span class="card-actions precision-actions">
              <button class="card-action" id="lessPrecision">Less precision</button>
              <button class="card-action" id="morePrecision">More precision</button>
            </span>
            <button class="card-action copy-result value-copy" data-copy-target="value">Copy</button>
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
            <li>Optional start boxes appear for each variable. Enter numeric constant expressions only, such as <code>3</code>, <code>pi/4</code>, <code>-2.5</code>, or <code>1e-6</code>.</li>
            <li>Use a start point when the solver needs a hint about which crossing or branch to search near, for example target <code>27</code> with expression <code>x^x</code> and start <code>3</code>.</li>
            <li>For several variables, fill the start box beside each variable you want to seed. Blank start boxes keep their current binding value, or use <code>1</code> for unknowns.</li>
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
            <li>Use the per-variable start boxes when goal seek needs better initial guesses.</li>
            <li><code>Goal seek</code> changes all variable bindings together to move the value towards that target.</li>
            <li>More/Less precision changes the displayed value precision without changing the expression.</li>
          </ul>
        </div>
      </div>
    </section>
  </main>

  <script>
    const expr = document.getElementById('expr');
    const run = document.getElementById('run');
    const back = document.getElementById('back');
    const forward = document.getElementById('forward');
    const help = document.getElementById('help');
    const goalSeek = document.getElementById('goalSeek');
    const clear = document.getElementById('clear');
    const targetRow = document.getElementById('targetRow');
    const goalTarget = document.getElementById('goalTarget');
    const goalStartFields = document.getElementById('goalStartFields');
    const lessPrecision = document.getElementById('lessPrecision');
    const morePrecision = document.getElementById('morePrecision');
    const derivativeButtons = document.getElementById('derivativeButtons');
    const variableValues = document.getElementById('variableValues');
    const mobileAccess = document.getElementById('mobileAccess');
    const mobileTitle = document.getElementById('mobileTitle');
    const mobileHint = document.getElementById('mobileHint');
    const mobileUrl = document.getElementById('mobileUrl');
    const mobileQr = document.getElementById('mobileQr');
    const funnelToggleButton = document.getElementById('funnelToggleButton');
    const controlToken = __CONTROL_TOKEN__;
    const statusEl = document.getElementById('status');
    const rightPaneTitle = document.getElementById('rightPaneTitle');
    const resultPane = document.getElementById('resultPane');
    const helpPane = document.getElementById('helpPane');
    const rendered = document.getElementById('rendered');
    const renderedCopy = document.getElementById('renderedCopy');
    const renderedMore = document.getElementById('renderedMore');
    const parsed = document.getElementById('parsed');
    const parsedMore = document.getElementById('parsedMore');
    const functionStyle = document.getElementById('functionStyle');
    const functionMore = document.getElementById('functionMore');
    const value = document.getElementById('value');
    const copyButtons = Array.from(document.querySelectorAll('.copy-result'));
    const moreDigitButtons = Array.from(document.querySelectorAll('.more-digits'));
    let lastTex = '';
    let lastDerivativeExpression = '';
    let currentVariables = [];
    let currentDifferentiable = true;
    let expressionHistory = [];
    let forwardHistory = [];
    let workingPrecisionBits = 256;
    let fullExpressionText = '';
    let displayedExpressionText = '';

    if (controlToken && window.location.search.includes('mars_lab_control=')) {
      window.history.replaceState(null, '', window.location.pathname + window.location.hash);
    }
    let lastEvaluationInputText = '';
    let bindingValueCache = new Map();
    const DOUBLE_PRECISION_BITS = 53;
    const DOUBLE_PRECISION_DIGITS = 17;
    const QFLOAT_PRECISION_BITS = 106;
    const MAX_PRECISION_BITS = 1048576;
    const START_FORBIDDEN_PATTERN = /[=,;|{}]/;
    const COMPACT_BINDING_VALUE_LIMIT = 20;
    const COMPACT_BINDING_VALUE_KEEP = 16;

    function precisionDigitsForBits(bits) {
      if (bits <= DOUBLE_PRECISION_BITS)
        return DOUBLE_PRECISION_DIGITS;
      return Math.ceil(bits * Math.LOG10E * Math.LN2);
    }

    function requestedPrecisionBits() {
      return Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, workingPrecisionBits));
    }

    function precisionStatusText() {
      const bits = requestedPrecisionBits();
      const digits = requestedValuePrecision();
      return `${digits} digits / ${bits} bits`;
    }

    function setStatus(text) {
      statusEl.textContent = `${text} · ${precisionStatusText()}`;
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

    function showTargetEntry() {
      const variables = variablesFromExpression(currentExpressionText());
      renderGoalStartFields(variables);
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

    function goalStartKeydown(event) {
      if (event.key === 'Enter') {
        event.preventDefault();
        goalSeek.click();
      } else if (event.key === 'Escape') {
        event.preventDefault();
        hideTargetEntry();
        expr.focus();
        setStatus('Ready');
      }
    }

    function renderGoalStartFields(variables) {
      goalStartFields.replaceChildren();
      variables.forEach((name) => {
        const id = `goalStart_${name.replace(/[^A-Za-z0-9_-]/g, '_')}`;
        const label = document.createElement('label');
        label.setAttribute('for', id);
        label.textContent = `Start ${name}`;

        const input = document.createElement('input');
        input.id = id;
        input.dataset.variable = name;
        input.inputMode = 'decimal';
        input.spellcheck = false;
        input.placeholder = 'optional value';
        input.addEventListener('keydown', goalStartKeydown);

        goalStartFields.append(label, input);
      });
    }

    function goalStartValues() {
      const values = {};
      for (const input of goalStartFields.querySelectorAll('input')) {
        const text = input.value.trim();
        if (!text)
          continue;
        if (START_FORBIDDEN_PATTERN.test(text)) {
          throw new Error(`Start for ${input.dataset.variable} must be a numeric constant expression`);
        }
        values[input.dataset.variable] = text;
      }
      return values;
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

    function canGoalSeek() {
      return variablesFromExpression(currentExpressionText()).length > 0;
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
        return splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return part.trim();

            const name = part.slice(0, eq).trim();
            const valueText = part.slice(eq + 1).trim();
            const compact = compactBindingValue(valueText);
            if (name) {
              bindingValues.push({name, value: valueText, display: compact.display, kind});
            }
            shortened = shortened || compact.shortened;
            return `${name} = ${compact.display}`;
          })
          .filter(Boolean);
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

    function fullValueForBinding(binding) {
      const value = String(binding.value || binding.display || '').trim();
      return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
    }

    function clearVariableValues() {
      variableValues.replaceChildren();
      variableValues.classList.add('hidden');
    }

    function refreshVariableValuesFromEditor() {
      const compact = compactExpressionForEditor(currentExpressionText());
      renderVariableValues(compact.bindings || []);
    }

    function renderVariableValues(bindings) {
      variableValues.replaceChildren();
      bindingValueCache = new Map();
      if (!bindings.length) {
        variableValues.classList.add('hidden');
        return;
      }

      bindings.forEach((binding) => {
        const kind = binding.kind || 'variable';
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
    }

    async function loadLastExpression() {
      try {
        const response = await fetch('/state');
        const data = await response.json();
        const saved = String(data.expression || '').trim();
        if (saved) {
          if (!saved.includes('...')) {
            setExpressionEditor(saved);
            return;
          }
          const localSaved = localStorage.getItem('mars.exprLab.lastExpression');
          if (localSaved && !localSaved.includes('...')) {
            setExpressionEditor(localSaved);
            return;
          }
          return;
        }
      } catch (_) {
        // Fall back to localStorage below.
      }

      try {
        const saved = localStorage.getItem('mars.exprLab.lastExpression');
        if (saved && !saved.includes('...'))
          setExpressionEditor(saved);
      } catch (_) {
        // Private browsing or locked-down webviews can disable localStorage.
      }
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

      fetch('/state', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({expression: text})
      }).catch(() => {
        // Persistence is helpful, not essential.
      });
    }

    function setBusy(isBusy) {
      run.disabled = isBusy;
      back.disabled = isBusy || expressionHistory.length === 0;
      forward.disabled = isBusy || forwardHistory.length === 0;
      goalSeek.disabled = isBusy || !canGoalSeek();
      goalSeek.title = goalSeek.disabled && !isBusy
        ? 'Goal seek needs at least one variable binding'
        : '';
      goalTarget.disabled = isBusy;
      Array.from(goalStartFields.querySelectorAll('input')).forEach((input) => {
        input.disabled = isBusy;
      });
      lessPrecision.disabled = isBusy || atMinimumPrecision();
      morePrecision.disabled = isBusy;
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
      back.disabled = expressionHistory.length === 0;
      forward.disabled = forwardHistory.length === 0;
      lessPrecision.disabled = atMinimumPrecision();
      goalSeek.disabled = !canGoalSeek();
      goalSeek.title = goalSeek.disabled
        ? 'Goal seek needs at least one variable binding'
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

    function setRequestedPrecisionBits(bits) {
      workingPrecisionBits = Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
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
      if (target === 'expression') return fullExpressionText || parsed.textContent;
      if (target === 'function') return functionStyle.dataset.fullText || functionStyle.textContent;
      if (target === 'value') return value.textContent;
      if (target === 'mobile') return mobileUrl ? mobileUrl.textContent.trim() : '';
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
        mobileAccess.classList.toggle('hidden', !url || !canControl);
        if (mobileTitle)
          mobileTitle.textContent = String(data.title || 'Mobile access');
        if (mobileHint)
          mobileHint.textContent = String(data.hint || '');
        mobileUrl.textContent = url;
        mobileQr.innerHTML = canControl ? String(data.qr || '') : '';
        if (funnelToggleButton) {
          funnelToggleButton.classList.toggle('hidden', !canControl || !Boolean(data.tailscale));
          funnelToggleButton.textContent = data.funnel ? 'Make private' : 'Make public';
        }
      } catch (err) {
        // Network state changes are expected; keep the last known QR until the next poll.
      }
    }

    if (funnelToggleButton) {
      funnelToggleButton.addEventListener('click', async () => {
        const wasPublic = funnelToggleButton.textContent.toLowerCase().includes('private');
        funnelToggleButton.disabled = true;
        funnelToggleButton.textContent = wasPublic ? 'Making private...' : 'Making public...';
        try {
          const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
          const response = await fetch('/funnel-toggle', {method: 'POST', headers});
          if (!response.ok)
            throw new Error('Funnel switch failed');
          await refreshMobileAccess();
        } catch (err) {
          funnelToggleButton.textContent = 'Could not switch';
          setTimeout(() => {
            refreshMobileAccess();
          }, 1600);
        } finally {
          funnelToggleButton.disabled = false;
        }
      });
    }

    function resetMoreDigitsButton(button, canExpand) {
      button.classList.toggle('hidden', !canExpand);
      button.textContent = 'Show more digits';
      button.dataset.expanded = 'false';
    }

    function hasAbbreviatedValue(text) {
      return String(text || '').includes('...');
    }

    function setRenderedContent(svg, fallbackText = '') {
      rendered.replaceChildren();
      if (svg) {
        rendered.innerHTML = svg;
      } else {
        rendered.textContent = fallbackText;
      }
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

    function setExpandableText(element, button, displayText, fullText) {
      element.textContent = displayText || fullText || '';
      element.dataset.displayText = displayText || '';
      element.dataset.fullText = fullText || '';
      resetMoreDigitsButton(
        button,
        !!fullText && !!displayText && fullText !== displayText && hasAbbreviatedValue(displayText)
      );
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
        element.textContent = element.dataset.displayText || element.textContent;
        button.textContent = 'Show more digits';
        button.dataset.expanded = 'false';
      } else {
        element.textContent = element.dataset.fullText || element.textContent;
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
      rendered.replaceChildren();
      rendered.textContent = '';
      clearRenderedError();
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails();
    }

    function clearResultDetails(options = {}) {
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
      const text = options.reuseLastInput && lastEvaluationInputText
        ? lastEvaluationInputText
        : currentExpressionText();
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
            data.expression,
            data.binding_values || null,
            data.editor_expression || null
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
          saveLastExpression(lastEvaluationInputText || fullExpressionText || expr.value.trim());
        lastDerivativeExpression = derivativeExpressionFromLine(data.derivative);
        currentVariables = variablesFromExpression(data.expression || '');
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

    async function runGoalSeek(sourceText, target, start, options = {}) {
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
      currentVariables = variablesFromExpression(solvedExpression);
      currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
      renderDerivativeButtons(currentVariables);
      expr.dataset.goalSeekSource = expressionForEditor(request.expression).trim();
      expr.dataset.goalSeekTarget = target;
      hideTargetEntry();
      setStatus(unchanged ? 'Goal already reached' : 'Goal reached');
      return true;
    }

    run.addEventListener('click', () => {
      forwardHistory = [];
      clearGoalSeekRequest();
      hideTargetEntry();
      evaluateExpression();
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

        if (!response.ok || !data.ok || !derivativeExpression) {
          setRenderedError(data.error || data.raw || `No derivative for ${wrt}`);
          resetMoreDigitsButton(renderedMore, false);
          setStatus('Error');
          return;
        }

        pushExpressionHistory(text);
        setExpressionEditor(derivativeExpression);
        await evaluateExpression();
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
      evaluateExpression();
    }

    expr.addEventListener('keydown', (event) => {
      if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
        event.preventDefault();
        evaluateFromKeyboard();
      }
    });

    expr.addEventListener('input', () => {
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
      const current = currentExpressionText();
      if (current)
        expressionHistory.push(current);
      forwardHistory = [];
      expr.value = '';
      clearExpressionSource();
      hideTargetEntry();
      clearResultPane();
      updateHistoryButtons();
      setStatus('Ready');
      expr.focus();
    });

    help.addEventListener('click', toggleHelp);

    goalSeek.addEventListener('click', async () => {
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
        const start = goalStartValues();
        await runGoalSeek(text, target, start);
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
      }
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
      setStatus('Precision changed');
      try {
        await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
      } finally {
        updateHistoryButtons();
      }
    });

    lessPrecision.addEventListener('click', async () => {
      setRequestedPrecisionBits(previousPrecisionStepBits(requestedPrecisionBits()));
      setStatus('Precision changed');
      try {
        await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
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

    setStatus('Ready');
    refreshMobileAccess();
    setInterval(refreshMobileAccess, 5000);
    loadLastExpression().finally(() => evaluateExpression());
  </script>
</body>
</html>
"""

WEB_MANIFEST = {
    "name": "MARS Lab",
    "short_name": "MARS Lab",
    "description": "Explore MARS mathematics with rendered TeX.",
    "start_url": "/",
    "scope": "/",
    "display": "standalone",
    "background_color": "#f6f0e5",
    "theme_color": "#0b4f8a",
    "icons": [
        {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
        {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png"}
    ]
}


def load_state_expression() -> str:
    try:
        data = json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return DEFAULT_EXPRESSION

    expression = str(data.get("expression", "")).strip()
    if "..." in expression:
        return DEFAULT_EXPRESSION
    return expression or DEFAULT_EXPRESSION


def save_state_expression(expression: str) -> None:
    if "..." in expression:
        return

    STATE_FILE.write_text(
        json.dumps({"expression": expression}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


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


def _request_looks_like_iphone(headers: http.client.HTTPMessage) -> bool:
    user_agent = headers.get("User-Agent", "").lower()
    return "iphone" in user_agent or "ipod" in user_agent


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
    iphone_trusted_hosts = {
        host for host in (
            tailscale_https_host().strip().lower(),
            tailscale_magicdns_host().strip().lower(),
            local_mdns_host().strip().lower(),
        ) if host
    }
    if _request_looks_like_iphone(headers) and request_host in iphone_trusted_hosts:
        return True

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

    try:
        completed = subprocess.run(
            ["hostname", "-I"],
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
    return os.environ.get("MARS_LAB_TAILSCALE_HOST", "mars").strip().strip(".")


def tailscale_https_host() -> str:
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


def tailscale_public_mode() -> bool:
    mode = os.environ.get("MARS_LAB_TAILSCALE_MODE", "private").strip().lower()
    return mode in ("1", "true", "yes", "on", "funnel", "public")


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

    set_tailscale_funnel_enabled(port, tailscale_public_mode())


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
    return f"{short_name}.local"


def browser_access_host(bind_host: str) -> str:
    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        if tailscale_ipv4():
            return tailscale_magicdns_host() or "mars"
        return local_mdns_host() or local_lan_ipv4() or "127.0.0.1"
    return bind_host


def browser_access_url(bind_host: str, port: int) -> str:
    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        return f"http://127.0.0.1:{port}/"
    browser_host = browser_access_host(bind_host)
    if ":" in browser_host and not browser_host.startswith("["):
        browser_host = f"[{browser_host}]"
    return f"http://{browser_host}:{port}/"


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
                "title": "Internet access" if funnel else "Tailscale access",
                "hint": (
                    "Funnel is on. Scan from any device."
                    if funnel
                    else "Funnel is off. Scan from a device connected to Tailscale."
                ),
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
        bind_host = tailscale_ipv4()
        if bind_host:
            tailscale_host = tailscale_https_host()
            scheme = "https" if tailscale_host else "http"
            tailscale_host = tailscale_host or tailscale_magicdns_host() or bind_host
            url_port = "" if scheme == "https" else f":{port}"
            return {
                "url": f"{scheme}://{tailscale_host}{url_port}/",
                "title": "Internet access" if funnel else "Tailscale access",
                "hint": (
                    "Funnel is on. Scan from any device."
                    if funnel
                    else "Funnel is off. Scan from a device connected to Tailscale."
                ),
                "funnel": funnel,
                "tailscale": True,
                "control": control_allowed,
            }
        mdns_host = local_mdns_host()
        if mdns_host:
            return {
                "url": f"http://{mdns_host}:{port}/",
                "title": "WiFi access",
                "hint": "Scan from a phone on the same WiFi.",
                "funnel": False,
                "tailscale": False,
                "control": False,
            }
        bind_host = local_lan_ipv4()
        if bind_host:
            return {
                "url": f"http://{bind_host}:{port}/",
                "title": "WiFi access",
                "hint": "Scan from a phone on the same WiFi.",
                "funnel": False,
                "tailscale": False,
                "control": False,
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
    title = "Internet access" if bind_host.startswith("100.") else "WiFi access"
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
    add_alignment(26, 26)

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
        return match.group(1) + number_text[:COMPACT_BINDING_VALUE_KEEP] + "..."

    return re.sub(
        r"(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)",
        compact_match,
        text,
    )


def precision_numeric_tokens(text: str, precision: int) -> str:
    if not text:
        return text

    def precision_match(match: re.Match[str]) -> str:
        return match.group(1) + format_number_text_for_precision(match.group(2), precision)

    return re.sub(
        r"(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)",
        precision_match,
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
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def ensure_mars_lab(binary: Path) -> None:
    if binary.exists() and os.access(binary, os.X_OK):
        return

    subprocess.run(
        ["make", "scratch/mars_lab"],
        cwd=ROOT,
        check=True,
        text=True,
    )

    if not binary.exists():
        raise RuntimeError(f"mars_lab binary was not created at {binary}")


def parse_mars_lab_output(output: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    current_multiline_key: str | None = None
    patterns = {
        "input": r"^input\s+(.*)$",
        "expression": r"^expression\s+(.*)$",
        "unbound": r"^unbound\s+(.*)$",
        "function": r"^function\s+(.*)$",
        "tex": r"^tex\s+(.*)$",
        "differentiable": r"^differentiable\s+(.*)$",
        "value": r"^value\s+(.*)$",
        "residual": r"^residual\s+(.*)$",
        "iterations": r"^iterations\s+(.*)$",
        "complex": r"^complex\s+(.*)$",
        "derivative": r"^derivative\s+(.*)$",
        "derivative_value": r"^d value\s+(.*)$",
    }

    for line in output.splitlines():
        matched = False
        for key, pattern in patterns.items():
            match = re.match(pattern, line)
            if match:
                fields[key] = match.group(1).rstrip()
                current_multiline_key = key if key == "function" else None
                matched = True
                break
        if not matched and current_multiline_key:
            fields[current_multiline_key] += "\n" + line.rstrip()
    return fields


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
    const_parts = restore_assignments(const_text)
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
    const_parts = replace_assignments(const_text)
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

    for name, value in parse_binding_assignments(const_text):
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
            raise ValueError("Start values must be entered in the per-variable start boxes")
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
        save_state_expression(str(fields["editor_expression"]))

    fields["full_display_expression"] = expression_for_display(fields.get("expression", ""))
    fields["full_display_tex"] = tex_for_display(fields.get("tex", ""))
    fields["full_display_function"] = function_for_display(fields.get("function", ""))
    fields["display_expression"] = compact_display_text(str(fields["full_display_expression"]))
    fields["display_tex"] = compact_display_text(str(fields["full_display_tex"]))
    fields["display_function"] = compact_function_text(str(fields["full_display_function"]))
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


class MarsLabHandler(http.server.BaseHTTPRequestHandler):
    binary: Path = DEFAULT_BIN
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

    def do_GET(self) -> None:
        parsed_path = urllib.parse.urlparse(self.path)
        path = parsed_path.path
        if path == "/state":
            self.send_json(200, {"expression": load_state_expression()})
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
            ) if details.get("control") else ""
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
        ) if mobile_details.get("control") else ""
        page = (
            INDEX_HTML.replace(
                "__INITIAL_EXPRESSION__",
                html.escape(load_state_expression(), quote=False),
            )
            .replace("__MOBILE_TITLE__", html.escape(mobile_details["title"], quote=False))
            .replace("__MOBILE_HINT__", html.escape(mobile_details["hint"], quote=False))
            .replace("__MOBILE_URL__", html.escape(mobile_url, quote=False))
            .replace("__MOBILE_QR_SVG__", mobile_qr)
            .replace("__MOBILE_CARD_CLASS__", "" if mobile_url and mobile_qr and mobile_details.get("control") else "hidden")
            .replace("__MOBILE_TAILSCALE_CLASS__", "" if mobile_details.get("control") and mobile_details.get("tailscale") else "hidden")
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
        path = urllib.parse.urlparse(self.path).path
        if path == "/funnel-toggle":
            control_allowed = request_allows_funnel_control(
                self.headers,
                str(self.client_address[0]),
                _control_token_from_query(self.path),
            )
            if not control_allowed:
                self.send_json(403, {"ok": False, "error": "Funnel control is only available from trusted devices."})
                return
            ok = set_tailscale_funnel_enabled(
                self.server_port,
                not tailscale_funnel_enabled(),
            )
            details = mobile_access_details(
                self.server_host,
                self.server_port,
                self.headers.get("Host", ""),
                control_allowed,
            )
            details["ok"] = ok
            details["qr"] = mobile_qr_svg(
                str(details.get("url", "")),
                bool(details.get("control")),
            ) if details.get("control") else ""
            self.send_json(200 if ok else 502, details)
            return

        if path == "/state":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                expression = str(payload.get("expression", "")).strip()
                if expression and "..." not in expression:
                    save_state_expression(expression)
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
        subprocess.Popen(
            [browser, url],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            close_fds=True,
        )
    elif shutil.which("xdg-open"):
        subprocess.Popen(
            ["xdg-open", url],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            close_fds=True,
        )
    else:
        webbrowser.open(url)


def main() -> int:
    parser = argparse.ArgumentParser(description="Launch the local MARS Lab.")
    parser.add_argument("--host", default="0.0.0.0", help="host to bind")
    parser.add_argument("--port", type=int, default=0, help="port to bind, or 0 for auto")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser automatically")
    parser.add_argument("--browser", default="", help="browser executable to open the lab URL")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BIN, help="path to mars_lab binary")
    args = parser.parse_args()

    binary = args.binary if args.binary.is_absolute() else ROOT / args.binary
    ensure_mars_lab(binary)

    MarsLabHandler.binary = binary

    port = args.port or find_free_port(args.host)
    MarsLabHandler.server_host = args.host
    MarsLabHandler.server_port = port
    try:
        server = http.server.ThreadingHTTPServer((args.host, port), MarsLabHandler)
    except OSError as exc:
        if exc.errno == errno.EADDRINUSE:
            ensure_tailscale_serve(args.host, port)
            MarsLabHandler.mobile_url = mobile_access_url(args.host, port)
            url = browser_access_url(args.host, port)
            print(f"MARS Lab already running at {url}")
            if not args.no_browser:
                open_lab_url(url, args.browser)
            return 0
        raise

    ensure_tailscale_serve(args.host, port)
    MarsLabHandler.mobile_url = mobile_access_url(args.host, port)
    url = browser_access_url(args.host, port)
    print(f"MARS Lab running at {url}")
    if MarsLabHandler.mobile_url:
        print(f"Mobile access: {MarsLabHandler.mobile_url}")
    print("Press Ctrl+C to stop.")

    if not args.no_browser:
        threading.Timer(0.25, open_lab_url, args=(url, args.browser)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping MARS Lab.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
