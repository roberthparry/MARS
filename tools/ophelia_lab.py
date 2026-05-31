#!/usr/bin/env python3
from __future__ import annotations

import argparse
import cgi
import csv
import errno
import html
import http.server
import json
import os
from pathlib import Path
import re
import subprocess
import threading
import time
import urllib.parse

import mars_lab as shared


ROOT = shared.ROOT
LAB_APP_NAME = os.environ.get("MARS_LAB_APP_NAME", "Ophelia Lab").strip() or "Ophelia Lab"
STATE_FILE = ROOT / os.environ.get("MARS_LAB_STATE_FILE", ".ophelia_lab_state.json")
DEFAULT_BIN = ROOT / os.environ.get("MARS_LAB_BINARY", "build/release/scratch/ophelia_lab")
DEFAULT_SCRATCH_TARGET = os.environ.get("MARS_LAB_SCRATCH_TARGET", "scratch/ophelia_lab").strip() or "scratch/ophelia_lab"
LAB_ICON_FILE = ROOT / "packaging" / "linux" / "ophelia-lab.svg"
UPLOAD_DIR = ROOT / ".ophelia_uploads"
APP_BASE_PATH = "/" + os.environ.get("MARS_LAB_PUBLIC_PATH", "/ophelia").strip().strip("/")

if APP_BASE_PATH == "/":
    APP_BASE_PATH = ""

DEFAULT_STATE: dict[str, object] = {
    "target_path": "sample_data/Monthly Target Numbers.csv",
    "target_display_name": "Monthly Sample LD Numbers.csv",
    "target_date_column": "",
    "target_value_column": "18-24 LD No Health contrib Tot.",
    "xreg_path": "sample_data/Monthly Population.csv",
    "xreg_display_name": "Monthly Sample Population.csv",
    "xreg_date_column": "DATE",
    "xreg_columns": "All",
    "model": "sarimax",
    "frequency": "monthly",
    "year_type": "fiscal",
    "horizon": 6,
    "p": 1,
    "d": 1,
    "q": 0,
    "P": 1,
    "D": 0,
    "Q": 0,
    "season_period": 12,
    "criterion": "aic",
    "level": "0.95",
}

WEB_MANIFEST = {
    "name": LAB_APP_NAME,
    "short_name": "Ophelia",
    "description": "Forecasting lab for MARS time-series work.",
    "start_url": f"{APP_BASE_PATH}/" if APP_BASE_PATH else "/",
    "scope": f"{APP_BASE_PATH}/" if APP_BASE_PATH else "/",
    "display": "standalone",
    "background_color": "#fff7fd",
    "theme_color": "#f7a8d9",
    "icons": [
        {"src": "/favicon.svg", "sizes": "any", "type": "image/svg+xml"},
        {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
        {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png"},
    ],
}

INDEX_HTML = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Ophelia Lab</title>
  <meta name="theme-color" content="#f7a8d9">
  <meta name="description" content="Forecasting and time-series workbench for MARS.">
  <link rel="manifest" href="__BASE_PATH__/manifest.webmanifest">
  <link rel="icon" href="__BASE_PATH__/favicon.svg" type="image/svg+xml">
  <link rel="apple-touch-icon" sizes="180x180" href="__BASE_PATH__/apple-touch-icon.png">
  <style>
    :root {
      --ink: #31143d;
      --muted: #73526d;
      --rose: #f7a8d9;
      --lilac: #d8b8ff;
      --sky: #b7ecff;
      --butter: #ffe69e;
      --paper: rgba(255, 255, 255, 0.86);
      --panel: rgba(255, 255, 255, 0.72);
      --line: rgba(125, 78, 132, 0.16);
      --shadow: 0 22px 60px rgba(125, 78, 132, 0.18);
      --accent: #d454a3;
      --accent-2: #4a96c7;
      --ok: #2c8a68;
      --danger: #c54a6f;
      --font-display: "Georgia", "Iowan Old Style", "Palatino Linotype", serif;
      --font-body: "Avenir Next", "Trebuchet MS", "Segoe UI", sans-serif;
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      color: var(--ink);
      font-family: var(--font-body);
      background:
        radial-gradient(circle at top left, rgba(255,255,255,0.96), transparent 18rem),
        radial-gradient(circle at top right, rgba(255,222,245,0.72), transparent 22rem),
        radial-gradient(circle at 75% 70%, rgba(183,236,255,0.58), transparent 20rem),
        linear-gradient(160deg, #fff7fd 0%, #ffe6f5 28%, #f3e8ff 58%, #daf4ff 78%, #fff6d7 100%);
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background:
        radial-gradient(circle at 14% 20%, rgba(255,255,255,0.95) 0 2rem, transparent 2.1rem),
        radial-gradient(circle at 84% 18%, rgba(255,255,255,0.8) 0 1.6rem, transparent 1.7rem),
        radial-gradient(circle at 72% 34%, rgba(255,255,255,0.66) 0 0.7rem, transparent 0.8rem),
        radial-gradient(circle at 28% 68%, rgba(255,255,255,0.72) 0 0.8rem, transparent 0.9rem),
        radial-gradient(circle at 62% 76%, rgba(255,255,255,0.54) 0 0.8rem, transparent 0.9rem);
      opacity: 0.8;
    }

    .shell {
      width: min(1920px, calc(100vw - 1rem));
      margin: 0.45rem auto 1rem;
      position: relative;
      z-index: 1;
    }

    .hero {
      display: grid;
      gap: 1rem;
      padding: 1.4rem;
      border: 1px solid rgba(255,255,255,0.7);
      border-radius: 30px;
      background: linear-gradient(145deg, rgba(255,255,255,0.76), rgba(255,245,252,0.76));
      box-shadow: var(--shadow);
      overflow: visible;
      position: relative;
    }

    .hero::after {
      content: "";
      position: absolute;
      inset: auto -8% -38% auto;
      width: 18rem;
      height: 18rem;
      border-radius: 50%;
      background:
        radial-gradient(circle at 50% 50%, rgba(255,255,255,0.94) 0 1rem, transparent 1.05rem),
        radial-gradient(circle at 50% 50%, transparent 0 4.2rem, rgba(255,184,221,0.86) 4.3rem 4.7rem, transparent 4.8rem),
        conic-gradient(from 0deg, rgba(255,184,221,0.92) 0deg 30deg, transparent 30deg 60deg, rgba(183,236,255,0.92) 60deg 90deg, transparent 90deg 120deg, rgba(255,230,158,0.9) 120deg 150deg, transparent 150deg 180deg, rgba(216,184,255,0.9) 180deg 210deg, transparent 210deg 240deg, rgba(255,184,221,0.92) 240deg 270deg, transparent 270deg 300deg, rgba(183,236,255,0.92) 300deg 330deg, transparent 330deg 360deg);
      opacity: 0.88;
      filter: drop-shadow(0 1rem 2rem rgba(212, 84, 163, 0.18));
    }

    .hero h1 {
      margin: 0;
      font-family: var(--font-display);
      font-size: clamp(2.2rem, 5vw, 4rem);
      line-height: 0.95;
      letter-spacing: -0.04em;
    }

    .hero p {
      margin: 0;
      max-width: 68rem;
      color: var(--muted);
      font-size: 1rem;
      line-height: 1.55;
    }

    .hero-top {
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 1rem;
      position: relative;
      z-index: 1;
    }

    .hero-copy {
      display: grid;
      gap: 0.7rem;
      min-width: 0;
    }

    .header-side {
      display: flex;
      flex-direction: column;
      align-items: flex-end;
      gap: 0.7rem;
      position: relative;
      z-index: 3;
      overflow: visible;
    }

    .hero-badges {
      display: flex;
      flex-wrap: wrap;
      gap: 0.65rem;
    }

    .badge {
      padding: 0.55rem 0.8rem;
      border-radius: 999px;
      font-size: 0.85rem;
      border: 1px solid rgba(255,255,255,0.8);
      background: rgba(255,255,255,0.72);
      backdrop-filter: blur(8px);
    }

    .mobile-card {
      position: relative;
      overflow: visible;
    }

    .mobile-card summary {
      display: inline-flex;
      align-items: center;
      gap: 0.45rem;
      list-style: none;
      border-radius: 999px;
      padding: 0.42rem 0.78rem;
      color: #6a4b0b;
      background: linear-gradient(135deg, #ffe39d, #f5b95f);
      font: 0.76rem/1.1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      font-weight: 700;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      cursor: pointer;
      user-select: none;
      box-shadow: 0 10px 24px rgba(212, 84, 163, 0.18);
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
      border: 2px solid rgba(255, 255, 255, 0.42);
      border-radius: 18px;
      background:
        linear-gradient(135deg, rgba(65, 20, 74, 0.96), rgba(113, 33, 96, 0.95) 42%, rgba(53, 91, 116, 0.95));
      box-shadow: 0 20px 58px rgba(85, 33, 77, 0.34);
    }

    .mobile-copy {
      display: grid;
      gap: 0.28rem;
      min-width: 0;
    }

    .mobile-copy strong {
      color: #fff0ba;
      font: 0.82rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }

    .mobile-copy span {
      color: rgba(255,255,255,0.78);
      font-size: 0.92rem;
    }

    .mobile-copy code,
    .mobile-copy a {
      min-width: 0;
      overflow-wrap: anywhere;
      color: #ffffff;
      text-decoration: none;
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
      border: 2px solid rgba(255, 255, 255, 0.24);
    }

    .mobile-qr svg {
      display: block;
      width: 100%;
      height: 100%;
    }

    .layout {
      display: grid;
      grid-template-columns: minmax(420px, 0.78fr) minmax(980px, 1.72fr);
      gap: 1.2rem;
      margin-top: 1.1rem;
      align-items: start;
    }

    .panel {
      border: 1px solid var(--line);
      border-radius: 26px;
      background: var(--panel);
      box-shadow: var(--shadow);
      backdrop-filter: blur(10px);
      overflow: hidden;
    }

    .panel-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      padding: 1rem 1.2rem 0.5rem;
    }

    .panel-head h2 {
      margin: 0;
      font-family: var(--font-display);
      font-size: 1.4rem;
      font-weight: 600;
    }

    .panel-head p {
      margin: 0;
      color: var(--muted);
      font-size: 0.92rem;
    }

    .panel-body {
      padding: 1.05rem 1.25rem 1.25rem;
    }

    form {
      display: grid;
      gap: 0.9rem;
    }

    .grid-2,
    .grid-3,
    .grid-4 {
      display: grid;
      gap: 0.8rem;
      align-items: start;
    }

    .grid-2 { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .grid-3 { grid-template-columns: repeat(3, minmax(0, 1fr)); }
    .grid-4 { grid-template-columns: repeat(4, minmax(0, 1fr)); }

    label {
      display: grid;
      gap: 0.34rem;
      font-size: 0.9rem;
      color: var(--muted);
    }

    .field-block {
      display: grid;
      gap: 0.34rem;
      font-size: 0.9rem;
      color: var(--muted);
    }

    .field-hint {
      margin-top: -0.08rem;
      color: rgba(115, 82, 109, 0.9);
      font-size: 0.78rem;
      line-height: 1.35;
    }

    .upload-stack {
      display: grid;
      gap: 0.55rem;
    }

    .upload-row {
      display: flex;
      flex-wrap: wrap;
      gap: 0.65rem;
      align-items: center;
    }

    .upload-row {
      align-items: center;
    }

    .upload-row input[type="file"] {
      position: absolute;
      width: 1px;
      height: 1px;
      padding: 0;
      margin: -1px;
      overflow: hidden;
      clip: rect(0, 0, 0, 0);
      white-space: nowrap;
      border: 0;
    }

    .upload-trigger {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 2.7rem;
      padding: 0.72rem 1rem;
      border-radius: 999px;
      border: 1px solid rgba(142, 96, 150, 0.14);
      background: rgba(255,255,255,0.92);
      color: var(--ink);
      font-weight: 700;
      cursor: pointer;
      box-shadow: none;
    }

    .upload-trigger:hover {
      transform: translateY(-1px);
      box-shadow: 0 10px 22px rgba(212, 84, 163, 0.12);
    }

    .upload-filename {
      min-width: 0;
      padding: 0.2rem 0;
      color: var(--muted);
      font-size: 0.86rem;
      line-height: 1.35;
      overflow-wrap: anywhere;
    }

    .path-readout {
      background: rgba(255,255,255,0.84);
      color: var(--ink);
    }

    .picker-status {
      color: var(--muted);
      font-size: 0.8rem;
      line-height: 1.35;
    }

    .compact-field {
      gap: 0.26rem;
    }

    .helper-row {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      gap: 0.35rem 0.75rem;
      align-items: baseline;
      margin-top: -0.04rem;
    }

    .helper-row .field-hint,
    .helper-row .picker-status {
      margin-top: 0;
      font-size: 0.76rem;
      line-height: 1.3;
    }

    .helper-row .field-hint {
      flex: 1 1 14rem;
    }

    .helper-row .picker-status {
      flex: 0 1 auto;
      text-align: right;
    }

    .multi-select-shell {
      display: grid;
      gap: 0.45rem;
    }

    .multi-select-summary {
      width: 100%;
      padding: 0.12rem 0.08rem 0;
      color: var(--muted);
      min-height: 0;
      line-height: 1.4;
      font-size: 0.84rem;
    }

    .multi-select-summary strong {
      color: var(--ink);
      font-weight: 700;
    }

    .multi-select-list-frame {
      border-radius: 18px;
      border: 1px solid rgba(142, 96, 150, 0.16);
      background: rgba(255,255,255,0.72);
      padding: 0.65rem;
    }

    .multi-select-list {
      display: grid;
      gap: 0.42rem;
      height: 11.5rem;
      overflow-y: auto;
      overflow-x: hidden;
      padding-right: 0.22rem;
    }

    .multi-select-placeholder {
      display: grid;
      place-items: center;
      height: 100%;
      padding: 0.85rem;
      border-radius: 14px;
      border: 1px dashed rgba(142, 96, 150, 0.22);
      color: var(--muted);
      background: rgba(255,255,255,0.62);
      text-align: center;
      line-height: 1.45;
    }

    .multi-select-item {
      display: flex;
      align-items: flex-start;
      gap: 0.55rem;
      padding: 0.48rem 0.52rem;
      border-radius: 12px;
      background: rgba(255,255,255,0.74);
      border: 1px solid rgba(142, 96, 150, 0.1);
      color: var(--ink);
    }

    .multi-select-item input[type="checkbox"] {
      width: auto;
      margin-top: 0.15rem;
      padding: 0;
      accent-color: var(--accent);
    }

    .multi-select-item span {
      font-size: 0.88rem;
      line-height: 1.35;
      word-break: break-word;
    }

    input, select, textarea, button {
      font: inherit;
    }

    input, select, textarea {
      width: 100%;
      padding: 0.78rem 0.88rem;
      border-radius: 16px;
      border: 1px solid rgba(142, 96, 150, 0.18);
      background: rgba(255,255,255,0.88);
      color: var(--ink);
      outline: none;
      transition: transform 180ms ease, box-shadow 180ms ease, border-color 180ms ease;
    }

    input:focus, select:focus, textarea:focus {
      border-color: rgba(212, 84, 163, 0.36);
      box-shadow: 0 0 0 4px rgba(212, 84, 163, 0.12);
      transform: translateY(-1px);
    }

    textarea {
      min-height: 11rem;
      resize: vertical;
      line-height: 1.45;
      font-family: "SFMono-Regular", "Menlo", "Consolas", monospace;
      font-size: 0.9rem;
    }

    .actions, .download-row, .mobile-actions {
      display: flex;
      flex-wrap: wrap;
      gap: 0.7rem;
      align-items: center;
    }

    button {
      border: none;
      border-radius: 999px;
      padding: 0.85rem 1.1rem;
      cursor: pointer;
      font-weight: 700;
      color: white;
      background: linear-gradient(135deg, var(--accent), #ff8ccf 54%, #8bc8ff);
      box-shadow: 0 14px 28px rgba(212, 84, 163, 0.24);
    }

    button.secondary {
      color: var(--ink);
      background: rgba(255,255,255,0.9);
      box-shadow: none;
      border: 1px solid rgba(142, 96, 150, 0.14);
    }

    button:disabled {
      opacity: 0.55;
      cursor: wait;
    }

    .status {
      min-height: 1.3rem;
      font-size: 0.92rem;
      color: var(--muted);
    }

    .status.error { color: var(--danger); }
    .status.ok { color: var(--ok); }

    .result-grid {
      display: grid;
      gap: 1rem;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      grid-template-areas:
        "metrics metrics"
        "downloads downloads"
        "summary summary"
        "table table"
        "raw raw";
    }

    .card-grid {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 0.8rem;
      margin-bottom: 1rem;
      grid-area: metrics;
    }

    .download-row { grid-area: downloads; }
    .summary-box { grid-area: summary; }
    .forecast-table-wrap { grid-area: table; }
    .forecast-raw-box { grid-area: raw; }

    .help-pane {
      display: grid;
      gap: 0.9rem;
    }

    .help-card {
      padding: 1rem 1.05rem;
      border: 1px solid rgba(142, 96, 150, 0.14);
      border-radius: 20px;
      background: rgba(255,255,255,0.74);
    }

    .help-card h3 {
      margin: 0 0 0.55rem;
      color: var(--ink);
      font-family: var(--font-display);
      font-size: 1.1rem;
    }

    .help-card p {
      margin: 0 0 0.7rem;
      color: var(--muted);
      line-height: 1.5;
    }

    .help-card ul {
      margin: 0;
      padding-left: 1.1rem;
    }

    .help-card li {
      margin: 0.35rem 0;
      color: var(--ink);
      line-height: 1.45;
    }

    .help-card code {
      color: #7a2461;
      background: rgba(216, 184, 255, 0.18);
      border-radius: 6px;
      padding: 0.08rem 0.28rem;
      font: 0.9rem/1.35 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .setup-panel .panel-head p,
    .output-panel .panel-head p {
      max-width: 52rem;
    }

    .setup-panel form {
      gap: 0.8rem;
    }

    .setup-panel {
      position: sticky;
      top: 0.75rem;
      align-self: start;
    }

    @media (min-width: 1440px) {
      .shell {
        width: min(1960px, calc(100vw - 0.75rem));
      }

      .hero {
        padding: 1.35rem 1.5rem 1.2rem;
      }

      .layout {
        grid-template-columns: minmax(430px, 0.74fr) minmax(1080px, 1.86fr);
      }
    }

    .metric {
      padding: 0.9rem;
      border-radius: 20px;
      background: rgba(255,255,255,0.74);
      border: 1px solid rgba(142, 96, 150, 0.12);
    }

    .metric span {
      display: block;
      font-size: 0.8rem;
      color: var(--muted);
      margin-bottom: 0.3rem;
    }

    .metric strong {
      font-size: 1rem;
      font-family: var(--font-display);
    }

    pre {
      margin: 0;
      white-space: pre-wrap;
      line-height: 1.5;
      font-family: "SFMono-Regular", "Menlo", "Consolas", monospace;
      font-size: 0.9rem;
    }

    .result-box {
      border-radius: 20px;
      background: rgba(255,255,255,0.74);
      border: 1px solid rgba(142, 96, 150, 0.12);
      padding: 1rem;
    }

    .summary-note {
      margin: 0 0 0.8rem;
      color: var(--muted);
      line-height: 1.45;
      font-size: 0.92rem;
    }

    .summary-report {
      display: grid;
      gap: 0.55rem;
      font-size: 0.95rem;
      line-height: 1.45;
    }

    .summary-line {
      padding: 0.72rem 0.85rem;
      border-radius: 14px;
      border: 1px solid rgba(142, 96, 150, 0.12);
      background: rgba(255,255,255,0.68);
      overflow-wrap: anywhere;
    }

    .summary-line.title-line {
      font-family: var(--font-display);
      font-size: 1.2rem;
      font-weight: 700;
      color: var(--ink);
      background: linear-gradient(135deg, rgba(255,255,255,0.88), rgba(250, 236, 247, 0.82));
    }

    .summary-line.section-line {
      font-family: var(--font-display);
      font-size: 1.05rem;
      font-weight: 700;
      background: linear-gradient(135deg, rgba(255,255,255,0.82), rgba(239, 232, 255, 0.72));
    }

    .summary-line.note-line {
      font-size: 0.93rem;
      color: var(--muted);
      background: linear-gradient(135deg, rgba(255,255,255,0.82), rgba(241, 247, 255, 0.72));
    }

    .summary-line.rating-exceptional {
      background: linear-gradient(135deg, rgba(170, 245, 192, 0.88), rgba(120, 218, 147, 0.76));
      border-color: rgba(74, 150, 95, 0.26);
    }

    .summary-line.rating-excellent {
      background: linear-gradient(135deg, rgba(201, 246, 190, 0.88), rgba(156, 228, 136, 0.76));
      border-color: rgba(95, 164, 77, 0.24);
    }

    .summary-line.rating-very-good {
      background: linear-gradient(135deg, rgba(229, 246, 183, 0.88), rgba(205, 231, 124, 0.76));
      border-color: rgba(146, 164, 66, 0.24);
    }

    .summary-line.rating-good {
      background: linear-gradient(135deg, rgba(255, 241, 184, 0.9), rgba(244, 214, 120, 0.76));
      border-color: rgba(184, 142, 58, 0.24);
    }

    .summary-line.rating-mediocre {
      background: linear-gradient(135deg, rgba(255, 219, 171, 0.9), rgba(245, 175, 115, 0.78));
      border-color: rgba(187, 116, 54, 0.26);
    }

    .summary-line.rating-poor {
      background: linear-gradient(135deg, rgba(255, 196, 187, 0.92), rgba(241, 126, 126, 0.8));
      border-color: rgba(186, 84, 84, 0.28);
    }

    .summary-line.rating-comparison {
      background: linear-gradient(135deg, rgba(232, 235, 245, 0.9), rgba(209, 217, 236, 0.8));
      border-color: rgba(110, 122, 160, 0.2);
    }

    .summary-k {
      font-weight: 700;
      color: rgba(49, 20, 61, 0.94);
    }

    .summary-v {
      font-family: "SFMono-Regular", "Menlo", "Consolas", monospace;
    }

    .table-wrap {
      overflow: hidden;
      height: 28rem;
      border-radius: 20px;
      background: rgba(255,255,255,0.76);
      border: 1px solid rgba(142, 96, 150, 0.12);
    }

    .table-caption {
      padding: 0.9rem 1rem 0;
      color: var(--muted);
      font-size: 0.9rem;
      line-height: 1.45;
    }

    .table-scroll {
      height: calc(100% - 2.95rem);
      overflow: auto;
      border-radius: 0 0 20px 20px;
    }

    table {
      width: 100%;
      border-collapse: collapse;
      min-width: 520px;
    }

    th, td {
      padding: 0.78rem 0.82rem;
      text-align: left;
      border-bottom: 1px solid rgba(142, 96, 150, 0.1);
      font-size: 0.92rem;
    }

    th {
      position: sticky;
      top: 0;
      background: rgba(255,246,252,0.94);
      font-family: var(--font-display);
      font-size: 0.95rem;
    }

    .hidden { display: none !important; }

    @media (max-width: 1020px) {
      .hero-top {
        flex-direction: column;
      }

      .header-side {
        align-items: flex-start;
      }

      .mobile-panel {
        right: auto;
        left: 0;
      }

      .layout {
        grid-template-columns: 1fr;
      }

      .setup-panel {
        position: static;
      }

      .result-grid {
        grid-template-columns: 1fr;
        grid-template-areas:
          "metrics"
          "downloads"
          "summary"
          "table"
          "raw";
      }

      .card-grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }
    }

    @media (max-width: 640px) {
      .shell {
        width: min(100vw - 1rem, 100%);
      }

      .hero, .panel-body, .panel-head {
        padding-left: 0.95rem;
        padding-right: 0.95rem;
      }

      .mobile-panel {
        position: static;
        width: 100%;
        margin-top: 0.6rem;
        grid-template-columns: 1fr;
      }

      .mobile-actions {
        width: 100%;
      }

      .grid-2, .grid-3, .grid-4, .card-grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <main class="shell">
    <section class="hero">
      <div class="hero-top">
        <div class="hero-copy">
          <h1>Ophelia Lab</h1>
          <p>Forecast from real dated series, pair it with exogenous drivers, and export the results without dropping back into the expression playground. This lab is for regression, ARIMA, ARIMAX, SARIMA, and SARIMAX workflows.</p>
        </div>
        <div class="header-side">
          <div id="mobile-status" class="status">Ready</div>
          <details class="mobile-card" id="mobileAccess">
            <summary>Mobile</summary>
            <div class="mobile-panel">
              <div class="mobile-copy">
                <strong id="mobile-title">__MOBILE_TITLE__</strong>
                <span id="mobile-hint">__MOBILE_HINT__</span>
                <a id="mobile-url" href="__MOBILE_URL__" target="_blank" rel="noreferrer">__MOBILE_URL__</a>
              </div>
              <div class="mobile-actions">
                <div class="mobile-qr" id="qr-box" aria-label="QR code for mobile access">__MOBILE_QR_SVG__</div>
                <button id="refresh-mobile" class="secondary" type="button">Copy URL</button>
                <button id="funnel-toggle" class="secondary mobile-funnel-toggle" type="button">Make public</button>
              </div>
            </div>
          </details>
        </div>
      </div>
      <div class="hero-badges">
        <span class="badge">Forecasting-first scratch app</span>
        <span class="badge">Phone-ready with QR access</span>
        <span class="badge">Public/private toggle preserved</span>
        <span class="badge">Sample presets loaded</span>
      </div>
    </section>

    <section class="layout">
      <section class="panel setup-panel">
        <div class="panel-head">
          <div>
            <h2>Forecast Setup</h2>
            <p>Point this at your dated CSVs, choose the model family, and let Ophelia return a summary plus a downloadable forecast table.</p>
          </div>
        </div>
        <div class="panel-body">
          <form id="forecast-form">
            <input type="hidden" name="target_display_name" value="__TARGET_DISPLAY_NAME__">
            <input type="hidden" name="xreg_display_name" value="__XREG_DISPLAY_NAME__">
            <div class="grid-2">
              <div class="field-block"><span>Target CSV</span>
                <div class="upload-stack">
                  <input type="hidden" name="target_path" value="__TARGET_PATH__">
                  <input id="target-path-readout" class="path-readout" value="__TARGET_DISPLAY_NAME__" title="__TARGET_DISPLAY_NAME__" readonly>
                  <div class="upload-row">
                    <input id="target-upload" type="file" accept=".csv,text/csv">
                    <button id="target-upload-trigger" class="upload-trigger" type="button">Choose CSV</button>
                    <span id="target-upload-name" class="upload-filename">No file chosen</span>
                  </div>
                </div>
                <span class="field-hint">Upload the file from your machine. Ophelia will save it locally for this lab and read the header row for you.</span>
              </div>
              <label class="compact-field">Target value column
                <select name="target_value_column" title="__TARGET_VALUE_COLUMN__">
                  <option value="__TARGET_VALUE_COLUMN__">__TARGET_VALUE_COLUMN__</option>
                </select>
                <span class="helper-row">
                  <span class="field-hint">Choose the series to forecast from the uploaded CSV header.</span>
                  <span id="target-picker-status" class="picker-status">Upload a CSV to load the available variables.</span>
                </span>
              </label>
            </div>

            <div class="grid-2">
              <label>Target date column
                <input name="target_date_column" value="__TARGET_DATE_COLUMN__" placeholder="Blank means first column">
                <span class="field-hint">Leave blank if the first column in the file contains the dates.</span>
              </label>
              <label>Forecast horizon
                <input name="horizon" type="number" min="1" value="__HORIZON__">
                <span class="field-hint">For monthly data, 6 means 6 months ahead and 12 means 12 months ahead.</span>
              </label>
            </div>

            <div class="grid-2">
              <div class="field-block"><span>Exogenous CSV</span>
                <div class="upload-stack">
                  <input type="hidden" name="xreg_path" value="__XREG_PATH__">
                  <input id="xreg-path-readout" class="path-readout" value="__XREG_DISPLAY_NAME__" title="__XREG_DISPLAY_NAME__" readonly>
                  <div class="upload-row">
                    <input id="xreg-upload" type="file" accept=".csv,text/csv">
                    <button id="xreg-upload-trigger" class="upload-trigger" type="button">Choose CSV</button>
                    <span id="xreg-upload-name" class="upload-filename">No file chosen</span>
                  </div>
                </div>
                <span class="field-hint">Optional supporting file with outside drivers such as population.</span>
              </div>
              <label>Exogenous columns
                <input type="hidden" name="xreg_columns" value="__XREG_COLUMNS__">
                <div class="multi-select-shell">
                  <div id="xreg-columns-summary" class="multi-select-summary">Selected: <strong>none yet</strong></div>
                  <div class="multi-select-list-frame">
                    <div id="xreg-columns-list" class="multi-select-list">
                      <div class="multi-select-placeholder">Upload or choose an exogenous CSV to load a scrollable list of driver columns here.</div>
                    </div>
                  </div>
                </div>
                <span class="field-hint">Choose one or more numeric driver columns from the supporting CSV.</span>
                <span id="xreg-columns-status" class="picker-status">Upload or choose an exogenous CSV to load its available columns.</span>
              </label>
            </div>

            <div class="grid-2">
              <label>Exogenous date column
                <input name="xreg_date_column" value="__XREG_DATE_COLUMN__">
                <span class="field-hint">This should be the date column in the exogenous file, often <code>DATE</code>.</span>
              </label>
              <label>Model
                <select name="model">
                  <option value="regression">Regression</option>
                  <option value="arima">ARIMA</option>
                  <option value="arimax">ARIMAX</option>
                  <option value="sarima">SARIMA</option>
                  <option value="sarimax">SARIMAX</option>
                  <option value="auto-arima">Auto-ARIMA</option>
                </select>
                <span class="field-hint">If unsure, start with <code>SARIMAX</code> for monthly data with an outside driver, or <code>ARIMA</code> without one.</span>
              </label>
            </div>

            <div class="grid-4">
              <label>Frequency
                <select name="frequency">
                  <option value="daily">Daily</option>
                  <option value="monthly">Monthly</option>
                  <option value="quarterly">Quarterly</option>
                  <option value="yearly">Yearly</option>
                </select>
                <span class="field-hint">How often the data is recorded. Most council series here will be monthly.</span>
              </label>
              <label>Year type
                <select name="year_type">
                  <option value="calendar">Calendar</option>
                  <option value="fiscal">Fiscal Apr-Mar</option>
                </select>
                <span class="field-hint">Choose Fiscal Apr-Mar if you want reporting aligned to the UK financial year.</span>
              </label>
              <label>Criterion
                <select name="criterion">
                  <option value="aic">AIC</option>
                  <option value="aicc">AICc</option>
                  <option value="bic">BIC</option>
                </select>
                <span class="field-hint">Used when comparing models. Lower is better.</span>
              </label>
              <label>Level
                <input name="level" value="__LEVEL__">
                <span class="field-hint">Confidence level for the forecast range. <code>0.95</code> is the normal choice.</span>
              </label>
            </div>

            <div class="grid-4">
              <label>p <input name="p" type="number" min="0" value="__P__"><span class="field-hint">How strongly the model looks at recent past values. A common starting value is <code>1</code>.</span></label>
              <label>d <input name="d" type="number" min="0" value="__D_LOWER__"><span class="field-hint">Trend removal. For many monthly series, <code>1</code> is a sensible first try.</span></label>
              <label>q <input name="q" type="number" min="0" value="__Q__"><span class="field-hint">Short-term error adjustment. Start with <code>0</code> or <code>1</code>.</span></label>
              <label>Season period <input name="season_period" type="number" min="0" value="__SEASON_PERIOD__"><span class="field-hint">For monthly data use <code>12</code>; for quarterly use <code>4</code>.</span></label>
            </div>

            <div class="grid-3">
              <label>P <input name="P" type="number" min="0" value="__P_SEASONAL__"><span class="field-hint">Seasonal version of p. Start with <code>1</code> if the yearly pattern repeats.</span></label>
              <label>D <input name="D" type="number" min="0" value="__D_SEASONAL__"><span class="field-hint">Seasonal version of d. Try <code>0</code> first, then <code>1</code> if seasonality is strong.</span></label>
              <label>Q <input name="Q" type="number" min="0" value="__Q_SEASONAL__"><span class="field-hint">Seasonal version of q. Usually start with <code>0</code>.</span></label>
            </div>

            <div class="actions">
              <button id="run-button" type="submit">Run forecast</button>
              <button id="sample-button" class="secondary" type="button">Restore Sample preset</button>
            </div>
            <div id="status" class="status">Ready when you are.</div>
          </form>
        </div>
      </section>

      <section class="panel output-panel">
        <div class="panel-head">
          <div>
            <h2>Forecast Output</h2>
            <p>Summary statistics, forecast rows, and downloadable text/CSV come back here.</p>
          </div>
          <button id="help-toggle" class="secondary" type="button">Help</button>
        </div>
        <div class="panel-body result-grid">
          <div class="card-grid">
            <div class="metric"><span>Model used</span><strong id="metric-model">Not run yet</strong></div>
            <div class="metric"><span>Historic rows used</span><strong id="metric-fit-rows">-</strong></div>
            <div class="metric"><span>Series stability check</span><strong id="metric-stationary">-</strong></div>
            <div class="metric"><span>Error-pattern check</span><strong id="metric-invertible">-</strong></div>
          </div>

          <div class="download-row">
            <button id="download-summary" class="secondary" type="button" disabled>Download summary.txt</button>
            <button id="download-forecast" class="secondary" type="button" disabled>Download forecast.csv</button>
          </div>

          <div class="result-box summary-box">
            <h3>Summary</h3>
            <p class="summary-note">Plain-language ratings are shown in brackets. If something looks poor or mediocre, the summary explains what a better result would usually look like.</p>
            <div id="summary-box" class="summary-report">
              <div class="summary-line note-line">Run a forecast to see model statistics here.</div>
            </div>
          </div>

          <div class="table-wrap forecast-table-wrap">
            <div class="table-caption">Actual vs forecast comparison. Historic rows show actual figures beside fitted values, and future rows continue as far as the exogenous data allows.</div>
            <div class="table-scroll">
              <table id="forecast-table">
                <thead id="forecast-head"><tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr></thead>
                <tbody id="forecast-body">
                  <tr><td colspan="6">Run a forecast to populate this scrollable actual-versus-forecast table.</td></tr>
                </tbody>
              </table>
            </div>
          </div>

          <div class="result-box forecast-raw-box">
            <h3>Raw forecast text</h3>
            <pre id="forecast-text-box">Forecast rows will appear here too.</pre>
          </div>
        </div>
        <div class="panel-body help-pane hidden" id="help-pane">
          <div class="help-card">
            <h3>Quick Start</h3>
            <ul>
              <li>Leave the Sample preset in place and press <code>Run forecast</code> to see a working example first.</li>
              <li>Use <code>Target CSV</code> for the numbers you want to predict, such as monthly demand, caseload, or activity.</li>
              <li>Use <code>Exogenous CSV</code> for outside drivers that may help explain changes, such as population.</li>
              <li>The app returns a summary, a forecast table, and downloads you can open in Excel.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>What Each Main Field Means</h3>
            <ul>
              <li><code>Target CSV</code>: the file holding the historic figures you want to forecast.</li>
              <li><code>Target value column</code>: the exact column name containing the values to predict.</li>
              <li><code>Target date column</code>: the date column for the target file. Leave this blank if the first column contains the dates.</li>
              <li><code>Forecast horizon</code>: how many future periods you want back. For monthly data, <code>6</code> means 6 months ahead and <code>12</code> means 12 months ahead.</li>
              <li><code>Exogenous CSV</code>: a second file with related information that may help the forecast.</li>
              <li><code>Exogenous columns</code>: tick one or more supporting columns from the scrollable list.</li>
              <li><code>Exogenous date column</code>: the date column in the exogenous file, usually <code>DATE</code>.</li>
              <li><code>Frequency</code>: how often the data is recorded. Most council reporting here will usually be <code>Monthly</code>.</li>
              <li><code>Year type</code>: choose <code>Fiscal Apr-Mar</code> if you want the forecast to align with the UK financial year.</li>
              <li><code>Level</code>: the confidence level for the forecast bands. <code>0.95</code> is the usual choice.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>Which Model Should I Pick?</h3>
            <p>If you are unsure, start simple and only move to a more complex model if it clearly helps.</p>
            <ul>
              <li><code>Regression</code>: best when you mainly believe another dataset, such as population, explains the future pattern.</li>
              <li><code>ARIMA</code>: best when you want the model to use the history of the target series by itself.</li>
              <li><code>ARIMAX</code>: use this when you want both the target history and outside drivers such as population.</li>
              <li><code>SARIMA</code>: use this when the pattern repeats over the year, for example monthly seasonal swings.</li>
              <li><code>SARIMAX</code>: usually the strongest practical choice for monthly council data when you have both seasonality and outside drivers.</li>
              <li><code>Auto-ARIMA</code>: helpful if you do not want to choose all the order settings by hand, but still check the results rather than assuming the search is always best.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>What The ARIMA Variables Mean</h3>
            <p>These settings control how much the model learns from recent history, trend, and repeating seasonal patterns.</p>
            <ul>
              <li><code>p</code>: how much the forecast uses recent past values from the series itself. A good starting value is often <code>1</code>.</li>
              <li><code>d</code>: how much trend-removal is needed before forecasting. For many monthly service series, <code>1</code> is a sensible first try.</li>
              <li><code>q</code>: how much the model corrects for short-term error patterns. Start with <code>0</code> or <code>1</code>.</li>
              <li><code>P</code>: the seasonal version of <code>p</code>. For monthly data, this looks back roughly one year at a time.</li>
              <li><code>D</code>: the seasonal version of <code>d</code>. Use this if the same months behave similarly year after year.</li>
              <li><code>Q</code>: the seasonal version of <code>q</code>. Start at <code>0</code> unless you have a reason to add it.</li>
              <li><code>Season period</code>: how many periods make up one full seasonal cycle. For monthly data this is usually <code>12</code>, for quarterly data usually <code>4</code>.</li>
            </ul>
            <p>Good practical starting points for monthly council data are often:</p>
            <ul>
              <li><code>ARIMA</code>: <code>p=1, d=1, q=0</code></li>
              <li><code>SARIMA</code>: <code>p=1, d=1, q=0, P=1, D=0, Q=0, season period=12</code></li>
              <li><code>SARIMAX</code>: the same as above, with the population column included as an exogenous driver</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>What Usually Makes A Better Forecast?</h3>
            <ul>
              <li>Use as much clean historic data as you can, especially if it covers several years.</li>
              <li>Make sure the dates line up properly between the target file and any exogenous file.</li>
              <li>For monthly local authority data, try a seasonal model before assuming the pattern is non-seasonal.</li>
              <li>Keep the model as simple as possible at first. A very complicated model is not automatically a better one.</li>
              <li>If you have a sensible outside driver, such as population, test a model with and without it and compare the outputs.</li>
              <li>Be cautious with very long horizons. A 6- to 12-month forecast is usually easier to trust than a much longer one.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>How To Judge Whether The Result Looks Reasonable</h3>
            <ul>
              <li>The forecast should broadly follow the shape of the historical series unless there is a strong reason for a sudden change.</li>
              <li>The confidence range should usually widen as the forecast moves further into the future.</li>
              <li>If the numbers jump wildly, go negative unexpectedly, or look implausible, try a simpler model or different settings.</li>
              <li>If several models give similar answers, prefer the simpler one.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>What Counts As Exceptional, Excellent, Good, Or Poor?</h3>
            <p>Before using the score bands below, it helps to know what each measure is trying to tell you in plain English.</p>
            <ul>
              <li><code>R²</code>: how much of the movement in the historic data the model manages to explain. Higher is usually better. Broadly, above <code>0.80</code> is very good, around <code>0.65</code> to <code>0.80</code> is often good, and below <code>0.40</code> is usually poor.</li>
              <li><code>Adj R²</code>: similar to <code>R²</code>, but stricter. It adjusts for model complexity, so it is often a better guide than plain <code>R²</code> when comparing regression-style models. The same rough good and bad bands as <code>R²</code> are useful here.</li>
              <li><code>RMSE</code>: the typical size of the model's mistakes. Lower is better. This is often the easiest measure for non-specialists to understand. As a rule of thumb, under about <code>10%</code> of the usual monthly level is excellent, under <code>25%</code> is often good, and above <code>40%</code> is usually poor.</li>
              <li><code>AIC</code>: a model comparison score that balances fit against complexity. Lower is better, but only when comparing models built on the same series. A lower AIC is good; a higher AIC is worse. The actual raw number is not “good” or “bad” by itself.</li>
              <li><code>BIC</code>: similar to <code>AIC</code>, but it penalises extra complexity even more strongly. Lower is better, again only when comparing models on the same series. Like AIC, the useful question is which model has the lower score, not whether the number looks big or small on its own.</li>
            </ul>
            <p>There is no single universal score that always means “good”. Different services and datasets behave differently. Use the guidance below as a practical rule of thumb, not a rigid rule.</p>
            <ul>
              <li><code>R²</code> or <code>Adj R²</code> for regression:
                Exceptional: above <code>0.95</code>.
                Excellent: <code>0.90</code> to <code>0.95</code>.
                Very good: <code>0.80</code> to <code>0.90</code>.
                Good: <code>0.65</code> to <code>0.80</code>.
                Mediocre: <code>0.40</code> to <code>0.65</code>.
                Poor: below <code>0.40</code>.
              </li>
              <li><code>RMSE</code>, <code>MSE</code>, and forecast error measures:
                lower is better, but there is no universal “good” number.
                Judge them against the scale of the service.
                As a rough guide, if the typical error is under about <code>5%</code> of the usual monthly value, that is exceptional;
                <code>5%–10%</code> is excellent;
                <code>10%–15%</code> is very good;
                <code>15%–25%</code> is good;
                <code>25%–40%</code> is mediocre;
                above <code>40%</code> is poor.
              </li>
              <li><code>AIC</code>, <code>AICc</code>, and <code>BIC</code>:
                these are only useful for comparing models fitted to the same target data.
                Smaller is better.
                Do not judge them by their raw size.
                A difference of less than about <code>2</code> is usually very small,
                <code>2–6</code> is a meaningful improvement,
                <code>6–10</code> is strong,
                and more than <code>10</code> is very strong.
              </li>
              <li><code>Sigma²</code>:
                lower is better, but again only in comparison with other models on the same series.
                There is no fixed “excellent” sigma² across different services.
              </li>
              <li>Forecast intervals:
                narrower intervals are better only if the forecast still looks realistic.
                Very narrow bands on a volatile series may be overconfident.
              </li>
            </ul>
            <p>For council use, a practical summary is:
            Exceptional means the forecast is accurate, stable, and easy to explain.
            Excellent and very good are usually strong enough for planning.
            Good may still be useful if the service is volatile.
            Mediocre means use caution.
            Poor means revisit the data, the model, or both.</p>
          </div>
          <div class="help-card">
            <h3>Exports</h3>
            <ul>
              <li><code>Download summary.txt</code> gives you the readable model summary.</li>
              <li><code>Download forecast.csv</code> gives you <code>date</code>, <code>actual</code>, <code>mean</code>, <code>stderr</code>, <code>lower</code>, and <code>upper</code>.</li>
              <li>The QR/mobile card in the header points to this same lab, including the separate <code>/ophelia/</code> public path.</li>
            </ul>
          </div>
        </div>
      </section>
    </section>

  </main>

  <script>
    const basePath = __BASE_PATH_JSON__;
    const defaults = __INITIAL_STATE__;
    const controlToken = __CONTROL_TOKEN__;
    const form = document.getElementById('forecast-form');
    const statusNode = document.getElementById('status');
    const runButton = document.getElementById('run-button');
    const sampleButton = document.getElementById('sample-button');
    const helpToggle = document.getElementById('help-toggle');
    const summaryBox = document.getElementById('summary-box');
    const forecastTextBox = document.getElementById('forecast-text-box');
    const forecastHead = document.getElementById('forecast-head');
    const forecastBody = document.getElementById('forecast-body');
    const metricModel = document.getElementById('metric-model');
    const metricFitRows = document.getElementById('metric-fit-rows');
    const metricStationary = document.getElementById('metric-stationary');
    const metricInvertible = document.getElementById('metric-invertible');
    const downloadSummary = document.getElementById('download-summary');
    const downloadForecast = document.getElementById('download-forecast');
    const resultGrid = document.querySelector('.result-grid');
    const helpPane = document.getElementById('help-pane');
    const mobileTitle = document.getElementById('mobile-title');
    const mobileStatus = document.getElementById('mobile-status');
    const mobileUrl = document.getElementById('mobile-url');
    const mobileHint = document.getElementById('mobile-hint');
    const qrBox = document.getElementById('qr-box');
    const funnelToggle = document.getElementById('funnel-toggle');
    const refreshMobile = document.getElementById('refresh-mobile');
    const targetUpload = document.getElementById('target-upload');
    const targetUploadTrigger = document.getElementById('target-upload-trigger');
    const targetUploadName = document.getElementById('target-upload-name');
    const targetPathReadout = document.getElementById('target-path-readout');
    const xregUpload = document.getElementById('xreg-upload');
    const xregUploadTrigger = document.getElementById('xreg-upload-trigger');
    const xregUploadName = document.getElementById('xreg-upload-name');
    const xregPathReadout = document.getElementById('xreg-path-readout');
    const xregColumnsInput = form.elements.namedItem('xreg_columns');
    const xregColumnsSummary = document.getElementById('xreg-columns-summary');
    const xregColumnsList = document.getElementById('xreg-columns-list');
    const xregColumnsStatus = document.getElementById('xreg-columns-status');
    const targetPickerStatus = document.getElementById('target-picker-status');
    const targetValueSelect = form.elements.namedItem('target_value_column');
    let latestSummary = '';
    let latestForecastCsv = '';
    let saveTimer = null;

    function applyState(state) {
      for (const [key, value] of Object.entries(state || {})) {
        const field = form.elements.namedItem(key);
        if (!field) continue;
        field.value = value;
      }
    }

    function collectState() {
      const payload = {};
      for (const element of form.elements) {
        if (!element.name) continue;
        payload[element.name] = element.value;
      }
      return payload;
    }

    function targetFilenameFromPath(pathText) {
      const raw = String(pathText || '').trim();
      if (!raw) return 'No file chosen';
      const parts = raw.split(/[\\\\/]/).filter(Boolean);
      return parts.length ? parts[parts.length - 1] : raw;
    }

    function syncTargetUploadName(pathText, displayName='') {
      const label = String(displayName || '').trim() || targetFilenameFromPath(pathText);
      form.elements.namedItem('target_path').value = String(pathText || '').trim();
      form.elements.namedItem('target_display_name').value = String(displayName || '').trim();
      targetPathReadout.value = label;
      targetPathReadout.title = label;
      targetUploadName.textContent = label;
    }

    function syncXregUploadName(pathText, displayName='') {
      const label = String(displayName || '').trim() || targetFilenameFromPath(pathText);
      form.elements.namedItem('xreg_path').value = String(pathText || '').trim();
      form.elements.namedItem('xreg_display_name').value = String(displayName || '').trim();
      xregPathReadout.value = label;
      xregPathReadout.title = label;
      xregUploadName.textContent = label;
    }

    function syncTargetValueTitle() {
      targetValueSelect.title = String(targetValueSelect.value || '').trim();
    }

    function splitSelectedColumns(text) {
      return String(text || '')
        .split(',')
        .map((value) => value.trim())
        .filter(Boolean);
    }

    function formatXregSummary(values) {
      if (!values.length) return '<strong>none yet</strong>';
      if (values.length === 1) return `<strong>1 column</strong>: ${escapeHtml(values[0])}`;
      if (values.length <= 3) return `<strong>${values.length} columns</strong>: ${escapeHtml(values.join(', '))}`;
      return `<strong>${values.length} columns</strong>: ${escapeHtml(values.slice(0, 3).join(', '))} + ${values.length - 3} more`;
    }

    function syncXregColumnsFromChecks() {
      const selected = Array.from(
        xregColumnsList.querySelectorAll('input[name="xreg-column-choice"]:checked')
      ).map((input) => input.value);
      xregColumnsInput.value = selected.join(', ');
      xregColumnsSummary.innerHTML = `Selected: ${formatXregSummary(selected)}`;
      xregColumnsSummary.title = selected.join(', ');
      xregColumnsStatus.textContent = selected.length
        ? `${selected.length} exogenous column${selected.length === 1 ? '' : 's'} selected.`
        : 'Choose one or more driver columns from the list.';
    }

    function renderXregColumnsPicker(header, dateColumn, selectedText) {
      const selected = new Set(splitSelectedColumns(selectedText));
      const rawHeader = Array.isArray(header) ? header : [];
      const activeDateColumn = String(dateColumn || '').trim();
      const options = rawHeader.filter((column, index) => {
        const text = String(column || '').trim();
        if (!text) return false;
        if (activeDateColumn) return text !== activeDateColumn;
        return index !== 0;
      });

      if (!options.length) {
        xregColumnsList.innerHTML = '<div class="multi-select-placeholder">Upload or choose an exogenous CSV to load a scrollable list of driver columns here.</div>';
        xregColumnsInput.value = '';
        xregColumnsSummary.innerHTML = 'Selected: <strong>none yet</strong>';
        xregColumnsSummary.title = 'No exogenous columns selected';
        xregColumnsStatus.textContent = rawHeader.length
          ? 'No selectable driver columns were found in this CSV.'
          : 'Upload or choose an exogenous CSV to load its available columns.';
        return;
      }

      xregColumnsList.innerHTML = options.map((column) => {
        const checked = selected.has(column) ? ' checked' : '';
        return `<label class="multi-select-item"><input type="checkbox" name="xreg-column-choice" value="${escapeHtml(column)}"${checked}><span>${escapeHtml(column)}</span></label>`;
      }).join('');

      if (!xregColumnsList.querySelector('input[name="xreg-column-choice"]:checked') && options.includes('All')) {
        const fallback = xregColumnsList.querySelector('input[value="All"]');
        if (fallback) fallback.checked = true;
      }
      syncXregColumnsFromChecks();
    }

    function renderTargetColumnPicker(columns, selected) {
      const items = Array.isArray(columns) ? columns : [];
      const chosen = String(selected || '').trim();

      if (!items.length) {
        targetValueSelect.innerHTML = '<option value="">Choose a column</option>';
        targetPickerStatus.textContent = 'Upload a CSV to load the available variables.';
        return;
      }

      targetValueSelect.innerHTML = items.map((column) => {
        const isSelected = column === chosen ? ' selected' : '';
        return `<option value="${escapeHtml(column)}"${isSelected}>${escapeHtml(column)}</option>`;
      }).join('');
      if (chosen && items.includes(chosen)) {
        targetValueSelect.value = chosen;
      } else {
        targetValueSelect.value = items[0];
      }
      syncTargetValueTitle();
      targetPickerStatus.textContent = `Chosen target: ${targetValueSelect.value}`;
    }

    async function uploadTargetCsv() {
      const file = targetUpload.files && targetUpload.files[0];
      if (!file) {
        setStatus('Choose a CSV file first.', 'error');
        return;
      }

      setStatus('Uploading target CSV and reading its headers...', '');
      targetPickerStatus.textContent = 'Reading headers...';

      try {
        const body = new FormData();
        body.append('file', file);
        const response = await fetch(`${basePath}/upload-target`, {
          method: 'POST',
          body,
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error || 'Could not upload the CSV');
        }

        form.elements.namedItem('target_path').value = data.path || '';
        syncTargetUploadName(data.path || '', data.original_name || '');
        if (data.date_column_in_first_position) {
          form.elements.namedItem('target_date_column').value = '';
        } else if (data.date_column) {
          form.elements.namedItem('target_date_column').value = data.date_column;
        }

        renderTargetColumnPicker(data.value_columns || [], targetValueSelect.value);
        scheduleStateSave();
        await saveState();
        setStatus('Target CSV uploaded. Choose the series to forecast from the dropdown.', 'ok');
      } catch (error) {
        targetValueSelect.innerHTML = '<option value="">Choose a column</option>';
        targetPickerStatus.textContent = 'Upload failed.';
        setStatus(error.message || String(error), 'error');
      } finally {
        targetUpload.value = '';
      }
    }

    async function uploadXregCsv() {
      const file = xregUpload.files && xregUpload.files[0];
      if (!file) {
        setStatus('Choose an exogenous CSV file first.', 'error');
        return;
      }

      setStatus('Uploading exogenous CSV...', '');
      try {
        const body = new FormData();
        body.append('file', file);
        const response = await fetch(`${basePath}/upload-xreg`, {
          method: 'POST',
          body,
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error || 'Could not upload the exogenous CSV');
        }

        form.elements.namedItem('xreg_path').value = data.path || '';
        syncXregUploadName(data.path || '', data.original_name || '');
        renderXregColumnsPicker(
          data.header || [],
          form.elements.namedItem('xreg_date_column').value,
          xregColumnsInput.value
        );
        scheduleStateSave();
        await saveState();
        setStatus('Exogenous CSV uploaded.', 'ok');
      } catch (error) {
        setStatus(error.message || String(error), 'error');
      } finally {
        xregUpload.value = '';
      }
    }

    async function loadExistingXregColumns() {
      const currentPath = String(form.elements.namedItem('xreg_path').value || '').trim();
      if (!currentPath) {
        renderXregColumnsPicker([], form.elements.namedItem('xreg_date_column').value, xregColumnsInput.value);
        return;
      }

      try {
        const response = await fetch(`${basePath}/target-columns?path=${encodeURIComponent(currentPath)}`, {
          cache: 'no-store',
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          renderXregColumnsPicker([], form.elements.namedItem('xreg_date_column').value, xregColumnsInput.value);
          return;
        }
        renderXregColumnsPicker(
          data.header || [],
          form.elements.namedItem('xreg_date_column').value,
          xregColumnsInput.value
        );
      } catch (_) {
        renderXregColumnsPicker([], form.elements.namedItem('xreg_date_column').value, xregColumnsInput.value);
      }
    }

    async function loadExistingTargetColumns() {
      const currentPath = String(form.elements.namedItem('target_path').value || '').trim();
      if (!currentPath) {
        renderTargetColumnPicker([], form.elements.namedItem('target_value_column').value);
        return;
      }

      try {
        const response = await fetch(`${basePath}/target-columns?path=${encodeURIComponent(currentPath)}`, {
          cache: 'no-store',
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          renderTargetColumnPicker([], form.elements.namedItem('target_value_column').value);
          return;
        }
        renderTargetColumnPicker(data.value_columns || [], form.elements.namedItem('target_value_column').value);
      } catch (_) {
        renderTargetColumnPicker([], form.elements.namedItem('target_value_column').value);
      }
    }

    async function saveState() {
      const payload = collectState();
      await fetch(`${basePath}/state`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
    }

    function scheduleStateSave() {
      if (saveTimer) window.clearTimeout(saveTimer);
      saveTimer = window.setTimeout(() => {
        saveState().catch(() => {});
      }, 250);
    }

    function setStatus(text, kind='') {
      statusNode.textContent = text;
      statusNode.className = kind ? `status ${kind}` : 'status';
    }

    function showResults() {
      resultGrid.classList.remove('hidden');
      helpPane.classList.add('hidden');
      helpToggle.textContent = 'Help';
      setStatus('Ready', '');
    }

    function showHelp() {
      resultGrid.classList.add('hidden');
      helpPane.classList.remove('hidden');
      helpToggle.textContent = 'Results';
      setStatus('Help', '');
    }

    function csvToRows(text) {
      const rows = text.trim().split(/\\r?\\n/).map(line => line.split(','));
      if (rows.length < 2) return [];
      return rows.slice(1);
    }

    function escapeHtml(text) {
      return String(text)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    }

    function summaryRatingClass(line) {
      const lower = line.toLowerCase();
      if (lower.includes('(exceptional')) return 'rating-exceptional';
      if (lower.includes('(excellent')) return 'rating-excellent';
      if (lower.includes('(very good')) return 'rating-very-good';
      if (lower.includes('(good')) return 'rating-good';
      if (lower.includes('(mediocre')) return 'rating-mediocre';
      if (lower.includes('(poor')) return 'rating-poor';
      if (lower.includes('(comparison only')) return 'rating-comparison';
      return '';
    }

    function renderSummaryText(text) {
      const source = (text || '').trim();
      if (!source) {
        summaryBox.innerHTML = '<div class="summary-line note-line">No summary returned.</div>';
        return;
      }

      const html = source.split(/\\r?\\n/).filter(line => line.length > 0).map((line, index) => {
        const trimmed = line.trim();
        let classes = ['summary-line'];
        let content = escapeHtml(line);

        if (index === 0 && /summary$/i.test(trimmed)) {
          classes.push('title-line');
        } else if (/^How to read this:/i.test(trimmed) || /^Coefficients$/i.test(trimmed) || /^AR parameters$/i.test(trimmed) || /^Model:/i.test(trimmed)) {
          classes.push('section-line');
        } else if (/^- /.test(trimmed)) {
          classes.push('note-line');
        } else {
          const ratingClass = summaryRatingClass(trimmed);
          if (ratingClass) classes.push(ratingClass);
        }

        if (line.includes(': ') && !/^Model:/i.test(trimmed) && !/^How to read this:/i.test(trimmed)) {
          const splitAt = line.indexOf(': ');
          const key = escapeHtml(line.slice(0, splitAt + 1));
          const value = escapeHtml(line.slice(splitAt + 2));
          content = `<span class="summary-k">${key}</span> <span class="summary-v">${value}</span>`;
        } else if (/^beta\\[\\d+\\] = /.test(trimmed) || /^phi\\[\\d+\\] = /.test(trimmed)) {
          content = `<span class="summary-v">${escapeHtml(line)}</span>`;
        }

        return `<div class="${classes.join(' ')}">${content}</div>`;
      }).join('');

      summaryBox.innerHTML = html || '<div class="summary-line note-line">No summary returned.</div>';
    }

    function renderForecastTable(csvText) {
      const source = String(csvText || '').trim();
      if (!source) {
        forecastHead.innerHTML = '<tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr>';
        forecastBody.innerHTML = '<tr><td colspan="6">No forecast rows returned.</td></tr>';
        return;
      }

      const lines = source.split(/\r?\n/).filter(Boolean);
      const header = (lines[0] || '').split(',');
      const rows = lines.slice(1).map(line => line.split(','));
      const colCount = header.length || 1;

      forecastHead.innerHTML = `<tr>${header.map(cell => `<th>${escapeHtml(cell || '')}</th>`).join('')}</tr>`;
      if (!rows.length) {
        forecastBody.innerHTML = `<tr><td colspan="${colCount}">No forecast rows returned.</td></tr>`;
        return;
      }
      forecastBody.innerHTML = rows.map(row => (
        `<tr>${header.map((_, index) => `<td>${escapeHtml(row[index] || '')}</td>`).join('')}</tr>`
      )).join('');
    }

    function downloadText(filename, content, type) {
      const blob = new Blob([content], { type });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = filename;
      document.body.appendChild(link);
      link.click();
      link.remove();
      URL.revokeObjectURL(url);
    }

    async function runForecast() {
      runButton.disabled = true;
      setStatus('Running forecast...', '');
      try {
        const payload = collectState();
        await saveState();
        const response = await fetch(`${basePath}/forecast`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload),
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error || 'Forecast failed');
        }
        latestSummary = data.summary_text || '';
        latestForecastCsv = data.forecast_csv || '';
        metricModel.textContent = data.model || 'Forecast';
        metricFitRows.textContent = String(data.fit_rows ?? '-');
        metricStationary.textContent = data.stationary ? 'Yes' : 'No';
        metricInvertible.textContent = data.invertible ? 'Yes' : 'No';
        renderSummaryText(latestSummary);
        forecastTextBox.textContent = data.forecast_text || 'No forecast text returned.';
        renderForecastTable(latestForecastCsv);
        downloadSummary.disabled = !latestSummary;
        downloadForecast.disabled = !latestForecastCsv;
        setStatus('Forecast complete.', 'ok');
      } catch (error) {
        setStatus(error.message || String(error), 'error');
      } finally {
        runButton.disabled = false;
      }
    }

    async function refreshMobileDetails() {
      mobileStatus.textContent = 'Refreshing phone access…';
      try {
        const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
        const response = await fetch(`${basePath}/mobile-access`, {cache: 'no-store', headers});
        const data = await response.json();
        mobileStatus.textContent = data.title || 'Phone access ready.';
        if (mobileTitle) mobileTitle.textContent = data.title || 'Phone access';
        mobileHint.textContent = data.hint || '';
        mobileUrl.textContent = data.url || '';
        mobileUrl.href = data.url || '#';
        qrBox.innerHTML = data.qr || '';
        const canControl = Boolean(data.control);
        const isTailscale = Boolean(data.tailscale);
        funnelToggle.classList.toggle('hidden', !isTailscale);
        funnelToggle.disabled = isTailscale && !canControl;
        funnelToggle.title = isTailscale && !canControl ? 'Restart Ophelia Lab from this machine to enable local Funnel control.' : '';
        funnelToggle.textContent = canControl ? (data.funnel ? 'Make private' : 'Make public') : 'Local control only';
      } catch (error) {
        mobileStatus.textContent = error.message || String(error);
      }
    }

    async function copyMobileUrl() {
      const url = (mobileUrl.textContent || '').trim();
      if (!url) return;
      try {
        await navigator.clipboard.writeText(url);
        mobileStatus.textContent = 'Copied phone URL.';
      } catch (error) {
        mobileStatus.textContent = error.message || String(error);
      }
    }

    async function toggleFunnel() {
      funnelToggle.disabled = true;
      try {
        const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
        const response = await fetch(`${basePath}/funnel-toggle`, { method: 'POST', headers });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error || 'Could not change public access');
        }
        await refreshMobileDetails();
      } catch (error) {
        mobileStatus.textContent = error.message || String(error);
      } finally {
        funnelToggle.disabled = false;
      }
    }

    form.addEventListener('submit', (event) => {
      event.preventDefault();
      runForecast();
    });

    form.addEventListener('input', scheduleStateSave);
    form.addEventListener('change', scheduleStateSave);

    targetUploadTrigger.addEventListener('click', () => {
      targetUpload.value = '';
      targetUpload.click();
    });
    targetUpload.addEventListener('change', () => {
      const file = targetUpload.files && targetUpload.files[0];
      if (file) {
        targetUploadName.textContent = file.name;
        setStatus(`Selected ${file.name}. Uploading it now...`, '');
        uploadTargetCsv();
      } else {
        targetUploadName.textContent = 'No file chosen';
      }
    });
    xregUploadTrigger.addEventListener('click', () => {
      xregUpload.value = '';
      xregUpload.click();
    });
    xregUpload.addEventListener('change', () => {
      const file = xregUpload.files && xregUpload.files[0];
      if (file) {
        xregUploadName.textContent = file.name;
        setStatus(`Selected ${file.name}. Uploading it now...`, '');
        uploadXregCsv();
      } else {
        xregUploadName.textContent = 'No file chosen';
      }
    });
    targetValueSelect.addEventListener('change', () => {
      syncTargetValueTitle();
      targetPickerStatus.textContent = targetValueSelect.value
        ? `Chosen target: ${targetValueSelect.value}`
        : 'Choose the series you want to forecast.';
      scheduleStateSave();
    });
    xregColumnsList.addEventListener('change', (event) => {
      const input = event.target;
      if (!(input instanceof HTMLInputElement) || input.name !== 'xreg-column-choice') {
        return;
      }
      syncXregColumnsFromChecks();
      scheduleStateSave();
    });
    form.elements.namedItem('xreg_date_column').addEventListener('change', () => {
      loadExistingXregColumns();
      scheduleStateSave();
    });

    sampleButton.addEventListener('click', () => {
      applyState(defaults);
      syncTargetUploadName(
        form.elements.namedItem('target_path').value,
        form.elements.namedItem('target_display_name').value
      );
      saveState();
      setStatus('Restored the Sample preset.', 'ok');
    });

    helpToggle.addEventListener('click', () => {
      if (helpPane.classList.contains('hidden')) showHelp();
      else showResults();
    });

    downloadSummary.addEventListener('click', () => {
      if (latestSummary) downloadText('ophelia-summary.txt', latestSummary, 'text/plain;charset=utf-8');
    });

    downloadForecast.addEventListener('click', () => {
      if (latestForecastCsv) downloadText('ophelia-forecast.csv', latestForecastCsv, 'text/csv;charset=utf-8');
    });

    refreshMobile.addEventListener('click', copyMobileUrl);
    funnelToggle.addEventListener('click', toggleFunnel);
    applyState(defaults);
    syncTargetUploadName(
      form.elements.namedItem('target_path').value,
      form.elements.namedItem('target_display_name').value
    );
    syncXregUploadName(
      form.elements.namedItem('xreg_path').value,
      form.elements.namedItem('xreg_display_name').value
    );
    syncTargetValueTitle();
    loadExistingTargetColumns();
    loadExistingXregColumns();
    form.elements.namedItem('model').value = '__MODEL__';
    form.elements.namedItem('frequency').value = '__FREQUENCY__';
    form.elements.namedItem('year_type').value = '__YEAR_TYPE__';
    form.elements.namedItem('criterion').value = '__CRITERION__';
    refreshMobileDetails();
  </script>
</body>
</html>
"""


def default_state() -> dict[str, object]:
    return dict(DEFAULT_STATE)


def app_base_url() -> str:
    return APP_BASE_PATH or ""


def app_url(path: str = "/") -> str:
    path = "/" + path.lstrip("/")
    return f"{APP_BASE_PATH}{path}" if APP_BASE_PATH else path


def request_path(path: str) -> str:
    if not APP_BASE_PATH:
        return path
    if path == APP_BASE_PATH or path == f"{APP_BASE_PATH}/":
        return "/"
    if path.startswith(f"{APP_BASE_PATH}/"):
        return path[len(APP_BASE_PATH):]
    return path


def load_state() -> dict[str, object]:
    state = default_state()
    try:
        data = json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return state
    if isinstance(data, dict):
        for key, value in data.items():
            if key in state:
                state[key] = value
    return state


def save_state(update: dict[str, object]) -> None:
    state = load_state()
    for key in state:
        if key in update:
            state[key] = update[key]
    try:
        STATE_FILE.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    except OSError:
        pass


def ensure_upload_dir() -> None:
    UPLOAD_DIR.mkdir(parents=True, exist_ok=True)


def relative_display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def resolve_lab_path(text: object) -> Path | None:
    raw = str(text or "").strip()
    if not raw:
        return None
    path = Path(raw)
    if not path.is_absolute():
        path = ROOT / path
    try:
        resolved = path.resolve(strict=False)
    except OSError:
        return None
    return resolved


def safe_upload_name(filename: str) -> str:
    name = Path(filename or "upload.csv").name
    stem = re.sub(r"[^A-Za-z0-9._-]+", "_", Path(name).stem).strip("._-") or "upload"
    suffix = Path(name).suffix.lower() or ".csv"
    if suffix != ".csv":
        suffix = ".csv"
    return f"{stem}-{int(time.time() * 1000)}{suffix}"


def csv_header_details(path: Path) -> dict[str, object]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
    raw_columns = [str(item).strip() for item in header]
    if not any(raw_columns):
        raise ValueError("The CSV does not have a readable header row.")
    date_column = raw_columns[0] if raw_columns else ""
    value_columns = [column for column in raw_columns[1:] if column]
    return {
        "header": raw_columns,
        "date_column": date_column,
        "date_column_in_first_position": True,
        "value_columns": value_columns,
    }


def ophelia_tailscale_target(port: int) -> str:
    return f"http://127.0.0.1:{port}"


def ophelia_tailscale_funnel_enabled() -> bool:
    if not APP_BASE_PATH:
        return shared.tailscale_funnel_enabled()
    try:
        completed = subprocess.run(
            ["tailscale", "funnel", "status", "--json"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
    except Exception:
        return False
    return completed.returncode == 0 and APP_BASE_PATH in completed.stdout


def ophelia_set_tailscale_funnel_enabled(port: int, enabled: bool) -> bool:
    if not shared.tailscale_https_host():
        return False
    path_flag = f"--set-path={APP_BASE_PATH or '/'}"
    target = ophelia_tailscale_target(port)
    try:
        if enabled:
            completed = subprocess.run(
                ["tailscale", "funnel", "--bg", "--https", "443", path_flag, target],
                text=True,
                capture_output=True,
                timeout=5,
                check=False,
            )
            return completed.returncode == 0

        subprocess.run(
            ["tailscale", "funnel", "--https=443", path_flag, "off"],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        completed = subprocess.run(
            ["tailscale", "serve", "--bg", "--https", "443", path_flag, target],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        return completed.returncode == 0
    except Exception:
        return False


def ophelia_ensure_tailscale_serve(bind_host: str, port: int) -> None:
    if os.environ.get("MARS_LAB_TAILSCALE_SERVE", "1").strip() in ("0", "false", "False", "no", "NO"):
        return
    if bind_host.strip() not in ("0.0.0.0", "::", "::0") or not shared.tailscale_ipv4():
        return
    if not shared.tailscale_https_host():
        return
    ophelia_set_tailscale_funnel_enabled(port, shared.tailscale_public_mode())


def ophelia_browser_access_url(bind_host: str, port: int) -> str:
    bind_host = bind_host.strip()
    bind_address = shared._ip_address_from_text(bind_host)
    bind_is_tailscale = bool(bind_address and bind_address in shared.ipaddress.ip_network("100.64.0.0/10"))
    if (bind_host in ("0.0.0.0", "::", "::0") and shared.tailscale_ipv4()) or bind_is_tailscale:
        tailscale_host = shared.tailscale_https_host()
        if tailscale_host:
            return f"https://{tailscale_host}{app_url('/')}"
    browser_host = shared.browser_access_host(bind_host)
    if ":" in browser_host and not browser_host.startswith("["):
        browser_host = f"[{browser_host}]"
    return f"http://{browser_host}:{port}{app_url('/')}"


def ophelia_mobile_access_details(bind_host: str, port: int, host_header: str = "",
                                  control_allowed: bool = False) -> dict[str, object]:
    funnel = ophelia_tailscale_funnel_enabled()
    request_host = shared._host_from_header(host_header)
    if request_host and not shared._is_loopback_or_wildcard_host(request_host):
        tailscale_host = shared.tailscale_https_host()
        magicdns_host = shared.tailscale_magicdns_host()
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
                "url": f"{scheme}://{url_host}{url_port}{app_url('/')}",
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
            "url": f"http://{request_host}:{port}{app_url('/')}",
            "title": "WiFi access",
            "hint": "Scan from a phone on the same WiFi.",
            "funnel": False,
            "tailscale": False,
            "control": False,
        }

    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        bind_host = shared.tailscale_ipv4()
        if bind_host:
            tailscale_host = shared.tailscale_https_host()
            scheme = "https" if tailscale_host else "http"
            tailscale_host = tailscale_host or shared.tailscale_magicdns_host() or bind_host
            url_port = "" if scheme == "https" else f":{port}"
            return {
                "url": f"{scheme}://{tailscale_host}{url_port}{app_url('/')}",
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
        mdns_host = shared.local_mdns_host()
        if mdns_host:
            return {
                "url": f"http://{mdns_host}:{port}{app_url('/')}",
                "title": "WiFi access",
                "hint": "Scan from a phone on the same WiFi.",
                "funnel": False,
                "tailscale": False,
                "control": False,
            }
        lan_host = shared.local_lan_ipv4()
        if lan_host:
            return {
                "url": f"http://{lan_host}:{port}{app_url('/')}",
                "title": "WiFi access",
                "hint": "Scan from a phone on the same WiFi.",
                "funnel": False,
                "tailscale": False,
                "control": False,
            }
    browser_host = shared.browser_access_host(bind_host)
    if ":" in browser_host and not browser_host.startswith("["):
        browser_host = f"[{browser_host}]"
    return {
        "url": f"http://{browser_host}:{port}{app_url('/')}",
        "title": "Local access",
        "hint": "Open this from the same device or the same network.",
        "funnel": False,
        "tailscale": False,
        "control": False,
    }


def render_index_html(state: dict[str, object], mobile_details: dict[str, object], control_allowed: bool) -> str:
    mobile_url = str(mobile_details.get("url", ""))
    mobile_qr = shared.mobile_qr_svg(mobile_url, bool(mobile_details.get("control"))) if mobile_url else ""
    page = INDEX_HTML
    replacements = {
        "__BASE_PATH__": app_base_url(),
        "__BASE_PATH_JSON__": json.dumps(app_base_url()),
        "__TARGET_PATH__": html.escape(str(state["target_path"]), quote=True),
        "__TARGET_DISPLAY_NAME__": html.escape(str(state["target_display_name"]), quote=True),
        "__TARGET_DATE_COLUMN__": html.escape(str(state["target_date_column"]), quote=True),
        "__TARGET_VALUE_COLUMN__": html.escape(str(state["target_value_column"]), quote=True),
        "__XREG_PATH__": html.escape(str(state["xreg_path"]), quote=True),
        "__XREG_DISPLAY_NAME__": html.escape(str(state["xreg_display_name"]), quote=True),
        "__XREG_DATE_COLUMN__": html.escape(str(state["xreg_date_column"]), quote=True),
        "__XREG_COLUMNS__": html.escape(str(state["xreg_columns"]), quote=True),
        "__HORIZON__": html.escape(str(state["horizon"]), quote=True),
        "__LEVEL__": html.escape(str(state["level"]), quote=True),
        "__P__": html.escape(str(state["p"]), quote=True),
        "__D_LOWER__": html.escape(str(state["d"]), quote=True),
        "__Q__": html.escape(str(state["q"]), quote=True),
        "__SEASON_PERIOD__": html.escape(str(state["season_period"]), quote=True),
        "__P_SEASONAL__": html.escape(str(state["P"]), quote=True),
        "__D_SEASONAL__": html.escape(str(state["D"]), quote=True),
        "__Q_SEASONAL__": html.escape(str(state["Q"]), quote=True),
        "__MOBILE_URL__": html.escape(mobile_url, quote=False),
        "__MOBILE_HINT__": html.escape(str(mobile_details.get("hint", "")), quote=False),
        "__MOBILE_QR_SVG__": mobile_qr,
        "__INITIAL_STATE__": json.dumps(state),
        "__MODEL__": html.escape(str(state["model"]), quote=True),
        "__FREQUENCY__": html.escape(str(state["frequency"]), quote=True),
        "__YEAR_TYPE__": html.escape(str(state["year_type"]), quote=True),
        "__CRITERION__": html.escape(str(state["criterion"]), quote=True),
        "__CONTROL_TOKEN__": json.dumps(shared.CONTROL_TOKEN if control_allowed else ""),
    }
    for key, value in replacements.items():
        page = page.replace(key, value)
    return page


def run_forecast(binary: Path, payload: dict[str, object]) -> dict[str, object]:
    shared.ensure_scratch_binary(binary, DEFAULT_SCRATCH_TARGET)
    command = [
        str(binary),
        "--target", str(payload.get("target_path", DEFAULT_STATE["target_path"])),
        "--target-date-column", str(payload.get("target_date_column", DEFAULT_STATE["target_date_column"])),
        "--target-value-column", str(payload.get("target_value_column", DEFAULT_STATE["target_value_column"])),
        "--xreg", str(payload.get("xreg_path", DEFAULT_STATE["xreg_path"])),
        "--xreg-date-column", str(payload.get("xreg_date_column", DEFAULT_STATE["xreg_date_column"])),
        "--xreg-cols", str(payload.get("xreg_columns", DEFAULT_STATE["xreg_columns"])),
        "--model", str(payload.get("model", DEFAULT_STATE["model"])),
        "--frequency", str(payload.get("frequency", DEFAULT_STATE["frequency"])),
        "--year-type", str(payload.get("year_type", DEFAULT_STATE["year_type"])),
        "--horizon", str(payload.get("horizon", DEFAULT_STATE["horizon"])),
        "--p", str(payload.get("p", DEFAULT_STATE["p"])),
        "--d", str(payload.get("d", DEFAULT_STATE["d"])),
        "--q", str(payload.get("q", DEFAULT_STATE["q"])),
        "--P", str(payload.get("P", DEFAULT_STATE["P"])),
        "--D", str(payload.get("D", DEFAULT_STATE["D"])),
        "--Q", str(payload.get("Q", DEFAULT_STATE["Q"])),
        "--season-period", str(payload.get("season_period", DEFAULT_STATE["season_period"])),
        "--criterion", str(payload.get("criterion", DEFAULT_STATE["criterion"])),
        "--level", str(payload.get("level", DEFAULT_STATE["level"])),
    ]
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=60,
    )
    raw = completed.stdout.strip() or completed.stderr.strip()
    if not raw:
        raise RuntimeError("Ophelia forecast runner returned no output.")
    try:
        data = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise RuntimeError(raw) from exc
    if completed.returncode != 0 and data.get("ok"):
        data["ok"] = False
        data["error"] = data.get("error") or f"Forecast runner exited with {completed.returncode}"
    return data


class OpheliaLabHandler(http.server.BaseHTTPRequestHandler):
    binary: Path = DEFAULT_BIN
    server_host: str = "127.0.0.1"
    server_port: int = 0

    def log_message(self, fmt: str, *args: object) -> None:
        try:
            print(f"ophelia_lab: {fmt % args}", file=os.sys.stderr)
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

    def send_bytes(self, status: int, payload: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def send_file(self, path: Path, content_type: str) -> None:
        try:
            data = path.read_bytes()
        except OSError:
            self.send_error(404)
            return
        self.send_bytes(200, data, content_type)

    def do_GET(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        path = request_path(parsed.path)
        if path == "/state":
            self.send_json(200, load_state())
            return
        if path == "/target-columns":
            params = urllib.parse.parse_qs(parsed.query, keep_blank_values=True)
            csv_path = resolve_lab_path(params.get("path", [""])[0])
            if not csv_path or not csv_path.exists():
                self.send_json(404, {"ok": False, "error": "Target CSV not found."})
                return
            try:
                self.send_json(200, {"ok": True, **csv_header_details(csv_path)})
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": str(exc)})
            return
        if path == "/mobile-access":
            control_allowed = shared.request_allows_funnel_control(
                self.headers,
                str(self.client_address[0]),
                shared._control_token_from_query(self.path),
            )
            details = ophelia_mobile_access_details(
                self.server_host,
                self.server_port,
                self.headers.get("Host", ""),
                control_allowed,
            )
            details["qr"] = shared.mobile_qr_svg(str(details["url"]), bool(details.get("control"))) if details.get("url") else ""
            self.send_json(200, details)
            return
        if path == "/favicon.svg":
            self.send_file(LAB_ICON_FILE, "image/svg+xml")
            return
        if path == "/apple-touch-icon.png":
            self.send_file(shared.LAB_TOUCH_ICON_FILE, "image/png")
            return
        if path == "/icon-192.png":
            self.send_file(shared.LAB_ICON_192_FILE, "image/png")
            return
        if path == "/icon-512.png":
            self.send_file(shared.LAB_ICON_512_FILE, "image/png")
            return
        if path == "/manifest.webmanifest":
            self.send_json(200, WEB_MANIFEST)
            return
        if path not in ("/", "/index.html"):
            self.send_error(404)
            return

        control_allowed = shared.request_allows_funnel_control(
            self.headers,
            str(self.client_address[0]),
            shared._control_token_from_query(self.path),
        )
        details = ophelia_mobile_access_details(
            self.server_host,
            self.server_port,
            self.headers.get("Host", ""),
            control_allowed,
        )
        page = render_index_html(load_state(), details, control_allowed).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        if shared._control_token_from_query(self.path) == shared.CONTROL_TOKEN:
            self.send_header(
                "Set-Cookie",
                f"{shared.CONTROL_COOKIE}={urllib.parse.quote(shared.CONTROL_TOKEN)}; Path=/; SameSite=Lax",
            )
        self.send_header("Content-Length", str(len(page)))
        self.end_headers()
        self.wfile.write(page)

    def do_POST(self) -> None:
        path = request_path(urllib.parse.urlparse(self.path).path)
        if path in ("/upload-target", "/upload-xreg"):
            try:
                ensure_upload_dir()
                form = cgi.FieldStorage(
                    fp=self.rfile,
                    headers=self.headers,
                    environ={
                        "REQUEST_METHOD": "POST",
                        "CONTENT_TYPE": self.headers.get("Content-Type", ""),
                    },
                )
                field = form["file"] if "file" in form else None
                if field is None or not getattr(field, "file", None):
                    raise ValueError("Choose a CSV file to upload.")

                payload = field.file.read()
                if not payload:
                    raise ValueError("The uploaded file was empty.")

                upload_path = UPLOAD_DIR / safe_upload_name(getattr(field, "filename", "target.csv"))
                upload_path.write_bytes(payload)
                header_info = csv_header_details(upload_path)
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": str(exc)})
                return

            self.send_json(200, {
                "ok": True,
                "path": relative_display_path(upload_path),
                "original_name": Path(getattr(field, "filename", upload_name)).name,
                **header_info,
            })
            return

        if path == "/state":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return
            if not isinstance(payload, dict):
                self.send_json(400, {"ok": False, "error": "State payload must be an object."})
                return
            save_state(payload)
            self.send_json(200, {"ok": True})
            return

        if path == "/funnel-toggle":
            control_allowed = shared.request_allows_funnel_control(
                self.headers,
                str(self.client_address[0]),
                shared._control_token_from_query(self.path),
            )
            if not control_allowed:
                self.send_json(403, {"ok": False, "error": "Funnel control is only available from trusted devices."})
                return
            target_enabled = not ophelia_tailscale_funnel_enabled()
            command_ok = ophelia_set_tailscale_funnel_enabled(self.server_port, target_enabled)
            current_enabled = ophelia_tailscale_funnel_enabled()
            ok = command_ok or current_enabled == target_enabled
            details = ophelia_mobile_access_details(
                self.server_host,
                self.server_port,
                self.headers.get("Host", ""),
                control_allowed,
            )
            details["ok"] = ok
            details["funnel"] = current_enabled
            details["qr"] = shared.mobile_qr_svg(str(details.get("url", "")), bool(details.get("control"))) if details.get("url") else ""
            self.send_json(200 if ok else 502, details)
            return

        if path != "/forecast":
            self.send_error(404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
        except Exception as exc:
            self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
            return
        if not isinstance(payload, dict):
            self.send_json(400, {"ok": False, "error": "Forecast payload must be an object."})
            return
        save_state(payload)
        try:
            data = run_forecast(self.binary, payload)
        except Exception as exc:
            self.send_json(422, {"ok": False, "error": str(exc)})
            return
        self.send_json(200 if data.get("ok") else 422, data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Launch the local Ophelia Lab forecasting app.")
    parser.add_argument("--host", default="0.0.0.0", help="host to bind")
    parser.add_argument("--port", type=int, default=0, help="port to bind, or 0 for auto")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser automatically")
    parser.add_argument("--browser", default="", help="browser executable to open the lab URL")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BIN, help="path to the forecast scratch binary")
    args = parser.parse_args()

    binary = args.binary if args.binary.is_absolute() else ROOT / args.binary
    shared.ensure_scratch_binary(binary, DEFAULT_SCRATCH_TARGET)

    OpheliaLabHandler.binary = binary
    port = args.port or shared.find_free_port(args.host)
    OpheliaLabHandler.server_host = args.host
    OpheliaLabHandler.server_port = port

    try:
        server = http.server.ThreadingHTTPServer((args.host, port), OpheliaLabHandler)
    except OSError as exc:
        if exc.errno == errno.EADDRINUSE:
            ophelia_ensure_tailscale_serve(args.host, port)
            url = ophelia_browser_access_url(args.host, port)
            print(f"{LAB_APP_NAME} already running at {url}")
            if not args.no_browser:
                shared.open_lab_url(shared._control_url(url), args.browser)
            return 0
        raise

    ophelia_ensure_tailscale_serve(args.host, port)
    url = ophelia_browser_access_url(args.host, port)
    mobile_url = str(ophelia_mobile_access_details(args.host, port).get("url", ""))
    print(f"{LAB_APP_NAME} running at {url}")
    if mobile_url:
        print(f"Mobile access: {mobile_url}")
    print("Press Ctrl+C to stop.")

    if not args.no_browser:
        threading.Timer(0.25, shared.open_lab_url, args=(shared._control_url(url), args.browser)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print(f"\nStopping {LAB_APP_NAME}.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
