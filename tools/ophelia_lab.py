#!/usr/bin/env python3
from __future__ import annotations

import argparse
import calendar
import cgi
import csv
import datetime as dt
import errno
import html
import http.server
import json
import io
import math
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
    "outlier_mode": "flag",
    "outlier_dates": "",
    "xreg_path": "sample_data/Monthly Population.csv",
    "xreg_display_name": "Monthly Sample Population.csv",
    "xreg_date_column": "DATE",
    "xreg_columns": "All",
    "models": "sarimax",
    "frequency": "monthly",
    "year_type": "fiscal",
    "forecast_end_date": "",
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

MODEL_OPTIONS: list[tuple[str, str]] = [
    ("regression", "Regression"),
    ("arima", "ARIMA"),
    ("arimax", "ARIMAX"),
    ("sarima", "SARIMA"),
    ("sarimax", "SARIMAX"),
    ("auto-arima", "Auto-ARIMA"),
]

LATEST_SUMMARY_TEXT = ""
LATEST_FORECAST_CSV = ""

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
      overflow: visible;
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

    .panel-head-actions {
      display: flex;
      align-items: center;
      gap: 0.55rem;
      flex-wrap: wrap;
      justify-content: flex-end;
    }

    .panel-body {
      padding: 1.05rem 1.25rem 1.25rem;
      min-width: 0;
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
      min-width: 0;
    }

    .field-block {
      display: grid;
      gap: 0.34rem;
      font-size: 0.9rem;
      color: var(--muted);
    }

    .field-title {
      display: block;
      min-height: 2.4rem;
      align-content: start;
    }

    .field-title code {
      font-size: 0.92em;
      white-space: nowrap;
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

    .detected-meta {
      display: grid;
      gap: 0.25rem;
      padding: 0.72rem 0.85rem;
      border-radius: 14px;
      border: 1px solid rgba(142, 96, 150, 0.14);
      background: rgba(255,255,255,0.68);
      color: var(--muted);
      font-size: 0.8rem;
      line-height: 1.4;
      min-width: 0;
    }

    .detected-meta strong {
      color: var(--ink);
    }

    .seasonality-line {
      margin-top: 0.08rem;
    }

    .seasonality-badge {
      display: inline-block;
      margin-top: 0.16rem;
      padding: 0.22rem 0.52rem;
      border-radius: 999px;
      border: 1px solid rgba(142, 96, 150, 0.18);
      font-weight: 600;
      line-height: 1.3;
      color: var(--ink);
      background: rgba(255,255,255,0.82);
    }

    .seasonality-badge.seasonality-none {
      background: rgba(248, 203, 173, 0.46);
      border-color: rgba(210, 122, 70, 0.28);
    }

    .seasonality-badge.seasonality-weak {
      background: rgba(255, 223, 156, 0.5);
      border-color: rgba(204, 150, 44, 0.28);
    }

    .seasonality-badge.seasonality-moderate {
      background: rgba(245, 232, 156, 0.5);
      border-color: rgba(181, 160, 51, 0.28);
    }

    .seasonality-badge.seasonality-strong {
      background: rgba(191, 233, 183, 0.52);
      border-color: rgba(84, 152, 68, 0.26);
    }

    .seasonality-badge.seasonality-very-strong {
      background: rgba(157, 221, 164, 0.58);
      border-color: rgba(50, 128, 63, 0.28);
    }

    .setup-advisory {
      margin-top: 0.15rem;
      padding: 0.72rem 0.85rem;
      border-radius: 14px;
      border: 1px solid rgba(142, 96, 150, 0.14);
      background: rgba(255,255,255,0.68);
      color: var(--muted);
      font-size: 0.8rem;
      line-height: 1.42;
    }

    .setup-advisory.hidden {
      display: none;
    }

    .setup-advisory strong {
      color: var(--ink);
    }

    .setup-advisory.rating-good {
      background: rgba(191, 233, 183, 0.34);
      border-color: rgba(84, 152, 68, 0.22);
    }

    .setup-advisory.rating-mediocre {
      background: rgba(255, 223, 156, 0.34);
      border-color: rgba(204, 150, 44, 0.22);
    }

    .setup-advisory.rating-poor {
      background: rgba(248, 203, 173, 0.34);
      border-color: rgba(210, 122, 70, 0.22);
    }

    .outlier-panel {
      margin-top: 0;
    }

    .outlier-panel-head {
      display: flex;
      flex-wrap: wrap;
      gap: 0.3rem 0.55rem;
      align-items: baseline;
    }

    .outlier-panel .field-hint {
      margin-top: 0;
    }

    .outlier-panel.no-outliers .outlier-controls {
      display: none;
    }

    .outlier-panel.no-outliers .field-hint {
      display: none;
    }

    .outlier-list {
      height: 4.1rem;
      display: grid;
      grid-auto-rows: max-content;
      align-content: start;
      gap: 0.14rem;
    }

    .outlier-list .multi-select-item {
      min-height: 0;
      padding: 0.1rem 0.24rem;
      gap: 0.22rem;
      align-items: center;
    }

    .outlier-list .multi-select-item span {
      display: block;
      flex: 1 1 auto;
      min-width: 0;
      font-size: 0.8rem;
      line-height: 1.1;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .target-right-stack {
      display: grid;
      gap: 0.7rem;
      align-content: start;
      min-width: 0;
    }

    .picker-status {
      color: var(--muted);
      font-size: 0.8rem;
      line-height: 1.35;
    }

    .compact-field {
      gap: 0.26rem;
    }

    .criteria-grid > .field-block,
    .criteria-grid > label {
      align-self: start;
      min-width: 0;
    }

    .criteria-grid > .field-block,
    .criteria-grid > label {
      grid-template-rows: minmax(2.4rem, auto) auto minmax(4.7rem, auto);
    }

    .criteria-grid .field-hint {
      min-height: 0;
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
      padding: 0.42rem;
      min-width: 0;
    }

    .multi-select-list {
      display: grid;
      gap: 0.24rem;
      height: 10.25rem;
      overflow-y: auto;
      overflow-x: hidden;
      padding-right: 0.14rem;
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
      align-items: center;
      gap: 0.34rem;
      padding: 0.18rem 0.32rem;
      border-radius: 10px;
      background: rgba(255,255,255,0.74);
      border: 1px solid rgba(142, 96, 150, 0.1);
      color: var(--ink);
      min-height: 1.5rem;
    }

    .multi-select-item input[type="checkbox"] {
      width: auto;
      margin-top: 0;
      padding: 0;
      accent-color: var(--accent);
    }

    .multi-select-item span {
      font-size: 0.8rem;
      line-height: 1.1;
      word-break: break-word;
    }

    .multi-select-item.is-disabled {
      opacity: 0.55;
      cursor: not-allowed;
    }

    .multi-select-item.is-disabled span {
      text-decoration: line-through;
      text-decoration-color: rgba(142, 96, 150, 0.28);
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

    .actions {
      position: relative;
      z-index: 40;
      pointer-events: auto;
      isolation: isolate;
    }

    button {
      position: relative;
      z-index: 50;
      pointer-events: auto;
      display: inline-flex;
      align-items: center;
      justify-content: center;
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

    .button-link {
      position: relative;
      z-index: 50;
      pointer-events: auto;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      border-radius: 999px;
      padding: 0.85rem 1.1rem;
      font-weight: 700;
      text-decoration: none;
      cursor: pointer;
    }

    .button-link.secondary {
      color: var(--ink);
      background: rgba(255,255,255,0.9);
      box-shadow: none;
      border: 1px solid rgba(142, 96, 150, 0.14);
    }

    .button-link.disabled,
    .button-link[aria-disabled="true"] {
      opacity: 0.55;
      cursor: not-allowed;
      pointer-events: none;
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

    .help-pane,
    .chart-pane {
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

    .summary-driver-list {
      border-radius: 16px;
      border: 1px solid rgba(142, 96, 150, 0.12);
      background: rgba(255,255,255,0.72);
      overflow: hidden;
    }

    .summary-driver-head,
    .summary-driver-row {
      display: grid;
      grid-template-columns: minmax(180px, 1.3fr) minmax(120px, 0.9fr) minmax(120px, 0.9fr) minmax(110px, 0.85fr) minmax(170px, 1.2fr);
      gap: 0.8rem;
      align-items: center;
      padding: 0.72rem 0.88rem;
    }

    .summary-driver-head {
      background: linear-gradient(135deg, rgba(255,255,255,0.9), rgba(250, 236, 247, 0.82));
      border-bottom: 1px solid rgba(142, 96, 150, 0.12);
      font-size: 0.8rem;
      font-weight: 700;
      color: var(--muted);
      letter-spacing: 0.01em;
      text-transform: uppercase;
    }

    .summary-driver-scroll {
      max-height: 15rem;
      overflow: auto;
    }

    .summary-driver-row {
      border-bottom: 1px solid rgba(142, 96, 150, 0.08);
      font-size: 0.92rem;
    }

    .summary-driver-row.rating-exceptional {
      background: linear-gradient(135deg, rgba(163, 241, 182, 0.94), rgba(111, 210, 132, 0.84));
    }

    .summary-driver-row.rating-excellent {
      background: linear-gradient(135deg, rgba(193, 244, 181, 0.94), rgba(145, 225, 126, 0.84));
    }

    .summary-driver-row.rating-very-good {
      background: linear-gradient(135deg, rgba(223, 244, 176, 0.94), rgba(199, 228, 118, 0.84));
    }

    .summary-driver-row.rating-good {
      background: linear-gradient(135deg, rgba(255, 239, 176, 0.95), rgba(245, 205, 110, 0.84));
    }

    .summary-driver-row.rating-mediocre {
      background: linear-gradient(135deg, rgba(255, 217, 166, 0.95), rgba(245, 164, 102, 0.84));
    }

    .summary-driver-row.rating-poor {
      background: linear-gradient(135deg, rgba(255, 198, 189, 0.96), rgba(239, 118, 118, 0.86));
    }

    .summary-driver-row.rating-comparison {
      background: linear-gradient(135deg, rgba(237, 240, 248, 0.94), rgba(214, 221, 238, 0.84));
    }

    .summary-driver-row:last-child {
      border-bottom: none;
    }

    .summary-driver-name {
      font-weight: 700;
      color: rgba(49, 20, 61, 0.94);
    }

    .summary-driver-number {
      font-family: "SFMono-Regular", "Menlo", "Consolas", monospace;
      color: var(--ink);
    }

    .summary-driver-impact {
      color: var(--ink);
      line-height: 1.35;
    }

    @media (max-width: 720px) {
      .summary-driver-head {
        display: none;
      }

      .summary-driver-row {
        grid-template-columns: 1fr;
        gap: 0.2rem;
      }

      .summary-driver-name::before {
        content: "Variable: ";
        font-weight: 600;
        color: var(--muted);
      }
    }

    .summary-friendly {
      display: grid;
      gap: 0.9rem;
    }

    .summary-overall {
      padding: 0.95rem 1rem;
      border-radius: 18px;
      border: 1px solid rgba(142, 96, 150, 0.12);
      background: rgba(255,255,255,0.72);
    }

    .summary-overall h4,
    .summary-section-title {
      margin: 0 0 0.45rem;
      font-family: var(--font-display);
      font-size: 1.02rem;
      color: var(--ink);
    }

    .summary-overall p,
    .summary-guidance p {
      margin: 0;
      line-height: 1.5;
      color: var(--ink);
    }

    .summary-overall .summary-v {
      font-family: inherit;
    }

    .summary-metric-grid {
      display: grid;
      gap: 0.7rem;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    }

    .summary-metric-card {
      padding: 0.85rem 0.9rem;
      border-radius: 16px;
      border: 1px solid rgba(142, 96, 150, 0.12);
      background: rgba(255,255,255,0.7);
      display: grid;
      gap: 0.24rem;
    }

    .summary-metric-label {
      font-size: 0.83rem;
      color: var(--muted);
      line-height: 1.3;
    }

    .summary-metric-value {
      font-weight: 700;
      color: var(--ink);
      line-height: 1.35;
    }

    .summary-guidance {
      padding: 0.9rem 0.95rem;
      border-radius: 16px;
      border: 1px solid rgba(142, 96, 150, 0.12);
      background: linear-gradient(135deg, rgba(255,255,255,0.82), rgba(241, 247, 255, 0.72));
    }

    .summary-guidance ul {
      margin: 0;
      padding-left: 1.1rem;
    }

    .summary-guidance li {
      margin: 0.3rem 0;
      color: var(--ink);
      line-height: 1.45;
    }

    .summary-driver-section {
      display: grid;
      gap: 0.55rem;
    }

    .model-list {
      max-height: 14.5rem;
    }

    .comparison-stack {
      display: grid;
      gap: 1rem;
    }

    .comparison-run {
      display: grid;
      gap: 0.9rem;
      padding: 1rem;
      border-radius: 22px;
      border: 1px solid rgba(142, 96, 150, 0.14);
      background: rgba(255,255,255,0.78);
    }

    .comparison-tabs {
      display: grid;
      gap: 0.8rem;
    }

    .comparison-tab-list {
      display: flex;
      gap: 0.45rem;
      overflow-x: auto;
      padding: 0.15rem 0.1rem 0.25rem;
      scrollbar-width: thin;
    }

    .comparison-tab {
      border: 1px solid rgba(142, 96, 150, 0.16);
      background: rgba(255,255,255,0.78);
      color: var(--ink);
      border-radius: 999px;
      padding: 0.58rem 0.9rem;
      font-weight: 700;
      white-space: nowrap;
      cursor: pointer;
      box-shadow: 0 10px 22px rgba(43, 32, 68, 0.05);
    }

    .comparison-tab.is-active {
      border-color: rgba(226, 92, 174, 0.38);
      background: linear-gradient(135deg, rgba(247, 101, 190, 0.9), rgba(142, 182, 255, 0.86));
      color: #fff;
    }

    .comparison-tab-panel {
      display: none;
    }

    .comparison-tab-panel.is-active {
      display: block;
    }

    .comparison-run-head {
      display: flex;
      justify-content: space-between;
      gap: 0.8rem;
      align-items: center;
      flex-wrap: wrap;
    }

    .comparison-run-head h4 {
      margin: 0;
      font-family: var(--font-display);
      color: var(--ink);
      font-size: 1.15rem;
    }

    .comparison-primary-badge {
      padding: 0.28rem 0.7rem;
      border-radius: 999px;
      background: rgba(247, 168, 217, 0.22);
      color: var(--ink);
      font-size: 0.82rem;
      border: 1px solid rgba(142, 96, 150, 0.14);
    }

    .comparison-metrics {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 0.7rem;
    }

    .comparison-metric {
      padding: 0.7rem 0.8rem;
      border-radius: 16px;
      border: 1px solid rgba(142, 96, 150, 0.12);
      background: rgba(255,255,255,0.74);
    }

    .comparison-metric span {
      display: block;
      font-size: 0.8rem;
      color: var(--muted);
      margin-bottom: 0.2rem;
    }

    .comparison-metric strong {
      font-size: 1rem;
      color: var(--ink);
    }

    .comparison-table-wrap {
      height: 18rem;
    }

    .forecast-chart-box {
      display: grid;
      gap: 0.75rem;
    }

    .chart-head {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 1rem;
      flex-wrap: wrap;
    }

    .chart-head h3 {
      margin: 0;
      font-family: var(--font-display);
      color: var(--ink);
      font-size: 1.05rem;
    }

    .chart-head p {
      margin: 0.2rem 0 0;
      color: var(--muted);
      font-size: 0.9rem;
      line-height: 1.4;
    }

    .forecast-chart-body {
      position: relative;
      min-height: min(64vh, 42rem);
      border: 1px solid rgba(142, 96, 150, 0.12);
      border-radius: 18px;
      background: linear-gradient(135deg, rgba(255,255,255,0.82), rgba(242, 249, 255, 0.66));
      overflow: hidden;
    }

    .forecast-chart-empty {
      display: grid;
      place-items: center;
      min-height: 13rem;
      padding: 1rem;
      color: var(--muted);
      text-align: center;
    }

    .forecast-chart-svg {
      display: block;
      width: 100%;
      height: auto;
      min-height: 18rem;
    }

    .chart-grid-line {
      stroke: rgba(142, 96, 150, 0.12);
      stroke-width: 1;
    }

    .chart-axis {
      stroke: rgba(49, 20, 61, 0.25);
      stroke-width: 1.2;
    }

    .chart-axis-label {
      fill: rgba(105, 72, 111, 0.9);
      font-size: 12px;
    }

    .chart-forecast-end-line {
      stroke: rgba(226, 92, 174, 0.52);
      stroke-width: 1.6;
      stroke-dasharray: 6 6;
    }

    .chart-forecast-end-label {
      fill: rgba(105, 72, 111, 0.95);
      font-size: 12px;
      font-weight: 700;
    }

    .chart-line {
      fill: none;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    .chart-actual-line {
      stroke: rgba(49, 20, 61, 0.9);
      stroke-width: 2.9;
    }

    .chart-model-line {
      stroke-width: 2.2;
      opacity: 0.95;
    }

    .chart-confidence-line {
      stroke-width: 1.9;
      stroke-dasharray: 7 5;
      opacity: 0.9;
    }

    .chart-confidence-upper {
      stroke-dasharray: 10 5;
    }

    .chart-point {
      stroke: rgba(255,255,255,0.94);
      stroke-width: 2;
    }

    .chart-hover-path {
      fill: none;
      stroke: transparent;
      stroke-width: 18;
      stroke-linecap: round;
      stroke-linejoin: round;
      pointer-events: stroke;
      cursor: crosshair;
    }

    .chart-hover-marker {
      pointer-events: none;
      stroke: rgba(255,255,255,0.95);
      stroke-width: 2.2;
      filter: drop-shadow(0 4px 8px rgba(49, 20, 61, 0.16));
    }

    .chart-tooltip {
      position: absolute;
      z-index: 12;
      min-width: 9rem;
      max-width: 16rem;
      padding: 0.62rem 0.72rem;
      border-radius: 14px;
      border: 1px solid rgba(142, 96, 150, 0.16);
      background: rgba(255,255,255,0.94);
      box-shadow: 0 14px 30px rgba(49, 20, 61, 0.16);
      color: var(--ink);
      font-size: 0.88rem;
      line-height: 1.35;
      pointer-events: none;
      transform: translate(12px, -50%);
    }

    .chart-tooltip.hidden {
      display: none;
    }

    .chart-tooltip-title {
      font-weight: 800;
      margin-bottom: 0.2rem;
    }

    .chart-tooltip-meta {
      color: var(--muted);
    }

    .chart-legend {
      display: flex;
      flex-wrap: wrap;
      gap: 0.45rem 0.75rem;
      align-items: center;
      padding: 0.75rem 0.95rem;
      border-bottom: 1px solid rgba(142, 96, 150, 0.1);
      background: rgba(255,255,255,0.7);
    }

    .chart-legend-title {
      padding: 0.26rem 0.62rem;
      border-radius: 999px;
      background: rgba(49, 20, 61, 0.9);
      color: #fff;
      font-size: 0.78rem;
      font-weight: 800;
      letter-spacing: 0.02em;
      text-transform: uppercase;
    }

    .chart-legend-item {
      display: inline-flex;
      align-items: center;
      gap: 0.38rem;
      color: var(--ink);
      font-size: 0.9rem;
      font-weight: 650;
      line-height: 1.2;
    }

    .chart-legend-item input {
      width: auto;
      margin: 0;
      padding: 0;
      accent-color: var(--accent);
    }

    .chart-legend-item.is-off {
      opacity: 0.58;
    }

    .chart-legend-swatch {
      width: 1.1rem;
      height: 0.22rem;
      border-radius: 999px;
      background: var(--swatch);
      box-shadow: 0 0 0 1px rgba(255,255,255,0.8);
    }

    .layout.chart-mode {
      grid-template-columns: 1fr;
    }

    .layout.chart-mode .setup-panel {
      display: none;
    }

    .layout.chart-mode .output-panel {
      grid-column: 1 / -1;
    }

    .layout.chart-mode .forecast-chart-body {
      min-height: calc(100vh - 19rem);
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
        <span class="badge">Suggested settings available</span>
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
          <form id="forecast-form" novalidate action="__BASE_PATH__/forecast-form" method="post" target="forecast-transport" onsubmit="return window.__opheliaPrepareForecastSubmit ? window.__opheliaPrepareForecastSubmit() : true;">
            <input type="hidden" name="target_display_name" value="__TARGET_DISPLAY_NAME__">
            <input type="hidden" name="xreg_display_name" value="__XREG_DISPLAY_NAME__">
            <div class="grid-2">
              <div class="field-block"><span>Target CSV</span>
                <div class="upload-stack">
                  <input type="hidden" name="target_path" value="__TARGET_PATH__">
                  <input id="target-path-readout" class="path-readout" value="__TARGET_DISPLAY_NAME__" title="__TARGET_DISPLAY_NAME__" readonly>
                  <div class="upload-row">
                    <input id="target-upload" type="file" accept=".csv,text/csv">
                    <label class="upload-trigger" for="target-upload">Choose CSV</label>
                    <span id="target-upload-name" class="upload-filename">__TARGET_UPLOAD_LABEL__</span>
                  </div>
                </div>
                <div id="target-meta" class="detected-meta">__TARGET_META_HTML__</div>
                <span class="field-hint">Upload the file from your machine. Ophelia will save it locally for this lab and read the header row for you.</span>
              </div>
              <div class="target-right-stack">
                <label class="compact-field">Target value column
                  <select name="target_value_column" title="__TARGET_VALUE_COLUMN__">
                    __TARGET_VALUE_OPTIONS__
                  </select>
                  <span class="helper-row">
                    <span class="field-hint">Choose the series to forecast from the uploaded CSV header.</span>
                    <span id="target-picker-status" class="picker-status">__TARGET_PICKER_STATUS__</span>
                  </span>
                </label>
                <div class="detected-meta outlier-panel __OUTLIER_PANEL_CLASS__">
                  <div class="outlier-panel-head">
                    <strong>Target outliers</strong>
                    <span id="outlier-status" class="picker-status">__OUTLIER_STATUS__</span>
                  </div>
                  <div id="outlier-controls" class="outlier-controls">
                    <label>Outlier handling
                      <select name="outlier_mode">
                        <option value="none">None</option>
                        <option value="flag">Flag only</option>
                        <option value="cap">Cap extreme values</option>
                        <option value="exclude">Exclude from fit (interpolate)</option>
                      </select>
                    </label>
                    <input type="hidden" name="outlier_dates" value="__OUTLIER_DATES__">
                    <div class="multi-select-list-frame">
                      <div id="outlier-list" class="multi-select-list outlier-list">__OUTLIER_PICKER_HTML__</div>
                    </div>
                  </div>
                  <span class="field-hint">Tick any flagged points you want Ophelia to treat specially. Excluding points keeps the dates and interpolates only for fitting.</span>
                </div>
              </div>
            </div>

            <div class="grid-2">
              <label>Target date column
                <select name="target_date_column">
                  __TARGET_DATE_OPTIONS__
                </select>
                <span class="field-hint">Choose the target date column from the likely candidates Ophelia found in the uploaded file.</span>
              </label>
              <label>Forecast until
                <select name="forecast_end_date">
                  __FORECAST_END_OPTIONS__
                </select>
                <span class="field-hint">Choose the last month, quarter, year, or day you want the forecast to reach. Ophelia lists only valid period ends, defaults to the furthest possible one, and remembers the choice when you come back.</span>
              </label>
            </div>

            <div class="grid-2">
              <div class="field-block"><span>Exogenous CSV</span>
                <div class="upload-stack">
                  <input type="hidden" name="xreg_path" value="__XREG_PATH__">
                  <input id="xreg-path-readout" class="path-readout" value="__XREG_DISPLAY_NAME__" title="__XREG_DISPLAY_NAME__" readonly>
                  <div class="upload-row">
                    <input id="xreg-upload" type="file" accept=".csv,text/csv">
                    <label class="upload-trigger" for="xreg-upload">Choose CSV</label>
                    <span id="xreg-upload-name" class="upload-filename">__XREG_UPLOAD_LABEL__</span>
                  </div>
                </div>
                <div id="xreg-meta" class="detected-meta">__XREG_META_HTML__</div>
                <span class="field-hint">Optional supporting file with outside drivers such as population.</span>
              </div>
              <label>Exogenous columns
                <input type="hidden" name="xreg_columns" value="__XREG_COLUMNS__">
                <div class="multi-select-shell">
                  <div id="xreg-columns-summary" class="multi-select-summary">__XREG_SUMMARY_HTML__</div>
                  <div class="multi-select-list-frame">
                    <div id="xreg-columns-list" class="multi-select-list">
                      __XREG_PICKER_HTML__
                    </div>
                  </div>
                </div>
                <span class="field-hint">Choose one or more numeric driver columns from the supporting CSV.</span>
                <span id="xreg-columns-status" class="picker-status">__XREG_PICKER_STATUS__</span>
                <span id="xreg-columns-debug" class="picker-status">Will send: __XREG_COLUMNS__</span>
              </label>
            </div>

            <div class="grid-2">
              <label>Exogenous date column
                <select name="xreg_date_column">
                  __XREG_DATE_OPTIONS__
                </select>
                <span class="field-hint">Choose the exogenous date column from the likely candidates Ophelia found in the supporting file.</span>
              </label>
              <label>Model
                <input type="hidden" name="models" value="__MODELS__">
                <div id="model-summary" class="multi-select-summary">Selected: __MODEL_SUMMARY__</div>
                <div class="multi-select-list-frame">
                  <div id="model-list" class="multi-select-list model-list">__MODEL_PICKER_HTML__</div>
                </div>
                <span class="field-hint">Tick one or more model families. If unsure, start with <code>SARIMAX</code> for monthly data with an outside driver, or <code>ARIMA</code> without one. Suggested settings respond to the selected model set.</span>
              </label>
            </div>

            <div class="grid-4 criteria-grid">
              <div class="field-block"><span class="field-title">Detected frequency</span>
                <input id="detected-frequency-readout" value="__DETECTED_FREQUENCY_LABEL__" readonly>
                <input type="hidden" name="frequency" value="__FREQUENCY__">
                <span class="field-hint">Ophelia detects this from the target dates and uses it for the forecast cadence.</span>
              </div>
              <label><span class="field-title">Reporting year</span>
                <select name="year_type">
                  <option value="calendar">Calendar</option>
                  <option value="fiscal">Fiscal Apr-Mar</option>
                </select>
                <span class="field-hint">Choose Fiscal Apr-Mar if you want reporting aligned to the UK financial year.</span>
              </label>
              <label><span class="field-title">Model comparison rule</span>
                <select name="criterion" title="AIC compares models fitted to the same data by balancing fit against complexity. Lower is better.">
                  <option value="aic">AIC</option>
                  <option value="aicc">AICc</option>
                  <option value="bic">BIC</option>
                </select>
                <span class="field-hint">Used when comparing models. AIC is the usual starting choice. It balances fit against complexity, and lower is better.</span>
              </label>
              <label><span class="field-title">Confidence level</span>
                <input name="level" value="__LEVEL__">
                <span class="field-hint">Confidence level for the forecast range. <code>0.95</code> is the normal choice.</span>
              </label>
            </div>

            <div class="actions">
              <button id="suggest-button" class="secondary" type="button">Use suggested settings</button>
            </div>

            <div class="grid-4">
              <label><span class="field-title">Recent history <code>(p)</code></span><input name="p" type="number" min="0" value="__P__"><span class="field-hint">How much the model leans on recent past values from the series itself. A common starting value is <code>1</code>.</span></label>
              <label><span class="field-title">Trend removal <code>(d)</code></span><input name="d" type="number" min="0" value="__D_LOWER__"><span class="field-hint">How much trend needs removing before forecasting. For many monthly series, <code>1</code> is a sensible first try.</span></label>
              <label><span class="field-title">Short-term error adjustment <code>(q)</code></span><input name="q" type="number" min="0" value="__Q__"><span class="field-hint">How much the model corrects short-term error patterns. Start with <code>0</code> or <code>1</code>.</span></label>
              <label><span class="field-title">Season length</span><select name="season_period">__SEASON_PERIOD_OPTIONS__</select><span class="field-hint">For <code>SARIMA</code>/<code>SARIMAX</code> and potentially <code>Auto-ARIMA</code>.</span></label>
            </div>

            <div class="grid-3">
              <label><span class="field-title">Yearly repeating pattern <code>(P)</code></span><input name="P" type="number" min="0" value="__P_SEASONAL__"><span class="field-hint">Seasonal version of recent history. Start with <code>1</code> if the yearly pattern repeats.</span></label>
              <label><span class="field-title">Seasonal trend removal <code>(D)</code></span><input name="D" type="number" min="0" value="__D_SEASONAL__"><span class="field-hint">Seasonal version of trend removal. Try <code>0</code> first, then <code>1</code> if seasonality is strong.</span></label>
              <label><span class="field-title">Seasonal error adjustment <code>(Q)</code></span><input name="Q" type="number" min="0" value="__Q_SEASONAL__"><span class="field-hint">Seasonal version of short-term error adjustment. Usually start with <code>0</code>.</span></label>
            </div>

            <div id="seasonal-setup-warning" class="setup-advisory hidden"></div>

            <div class="actions">
              <button id="run-button" type="submit">Run forecast</button>
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
          <div class="panel-head-actions">
            <button id="chart-toggle" class="secondary" type="button">Chart</button>
            <button id="help-toggle" class="secondary" type="button">Help</button>
          </div>
        </div>
        <div class="panel-body result-grid">
          <div class="card-grid">
            <div class="metric"><span>Model used</span><strong id="metric-model">Not run yet</strong></div>
            <div class="metric"><span>Historic rows used</span><strong id="metric-fit-rows">-</strong></div>
            <div class="metric"><span>Series stability check</span><strong id="metric-stationary">-</strong></div>
            <div class="metric"><span>Error-pattern check</span><strong id="metric-invertible">-</strong></div>
          </div>

          <div class="download-row">
            <a id="download-summary" class="button-link secondary disabled" href="__BASE_PATH__/download-summary" download aria-disabled="true">Download summary.txt</a>
            <a id="download-forecast" class="button-link secondary disabled" href="__BASE_PATH__/download-forecast" download aria-disabled="true">Download forecast.csv</a>
          </div>

          <div class="result-box summary-box">
            <h3>Summary</h3>
            <p class="summary-note">Plain-language ratings are shown in brackets. If something looks poor or mediocre, the summary explains what a better result would usually look like.</p>
            <p id="drivers-used-note" class="summary-note">The target and any drivers used in this run will appear here.</p>
            <p class="summary-note">If the first few historic fitted rows show <code>NAN</code>, that is usually expected for ARIMA-family models. It means the model needs a little earlier history before it can start calculating fitted values.</p>
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

        </div>
        <div class="panel-body chart-pane hidden" id="chart-pane">
          <div class="result-box forecast-chart-box">
            <div class="chart-head">
              <div>
                <h3>Actuals and forecasts</h3>
                <p>Actual values are plotted against each selected model's fitted and forecast mean. The table remains the source of the exact numbers.</p>
              </div>
            </div>
            <div id="forecast-chart-body" class="forecast-chart-body forecast-chart-empty">Run a forecast to draw the chart.</div>
          </div>
        </div>
        <div class="panel-body help-pane hidden" id="help-pane">
          <div class="help-card">
            <h3>Quick Start</h3>
            <ul>
              <li>Start with your current file choices and press <code>Use suggested settings</code> if you want a sensible first setup.</li>
              <li>Use <code>Target CSV</code> for the numbers you want to predict, such as monthly demand, caseload, or activity.</li>
              <li>Use <code>Exogenous CSV</code> for outside drivers that may help explain changes, such as population.</li>
              <li>Tick one or more model families. If you choose several, Ophelia compares them side by side.</li>
              <li>Use <code>Chart</code> after a run to open a full-width plot of actuals and each selected model's forecast line.</li>
              <li>The app returns a summary, a forecast table, and downloads you can open in Excel.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>What Each Main Field Means</h3>
            <ul>
              <li><code>Target CSV</code>: the file holding the historic figures you want to forecast.</li>
              <li><code>Target value column</code>: the exact column name containing the values to predict.</li>
              <li><code>Target date column</code>: the date column for the target file. Leave this blank if the first column contains the dates.</li>
              <li><code>Forecast until</code>: the last date you want the forecast to cover. Ophelia works out the number of future periods for you.</li>
              <li><code>Exogenous CSV</code>: a second file with related information that may help the forecast.</li>
              <li><code>Exogenous columns</code>: tick one or more supporting columns from the scrollable list.</li>
              <li><code>Exogenous date column</code>: the date column in the exogenous file, usually <code>DATE</code>.</li>
              <li><code>Models</code>: tick one or more model families. The output table shows a mean and standard error for each selected model.</li>
              <li><code>Detected frequency</code>: how often the target data appears to be recorded. Most council reporting here will usually be <code>Monthly</code>.</li>
              <li><code>Year type</code>: choose <code>Fiscal Apr-Mar</code> if you want the forecast to align with the UK financial year.</li>
              <li><code>Confidence level</code>: the confidence level for the forecast bands. <code>0.95</code> is the usual choice.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>Which Model Should I Pick?</h3>
            <p>If you are unsure, start simple and only move to a more complex model if it clearly helps. You can also tick several models and compare them in one run.</p>
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
              <li><code>Recent history (p)</code>: how much the forecast uses recent past values from the series itself. A good starting value is often <code>1</code>.</li>
              <li><code>Trend removal (d)</code>: how much trend-removal is needed before forecasting. For many monthly service series, <code>1</code> is a sensible first try.</li>
              <li><code>Short-term error adjustment (q)</code>: how much the model corrects for short-term error patterns. Start with <code>0</code> or <code>1</code>.</li>
              <li><code>Yearly repeating pattern (P)</code>: the seasonal version of <code>p</code>. For monthly data, this looks back roughly one year at a time.</li>
              <li><code>Seasonal trend removal (D)</code>: the seasonal version of <code>d</code>. Use this if the same months behave similarly year after year.</li>
              <li><code>Seasonal error adjustment (Q)</code>: the seasonal version of <code>q</code>. Start at <code>0</code> unless you have a reason to add it.</li>
              <li><code>Season length</code>: the repeating pattern to use for <code>SARIMA</code>, <code>SARIMAX</code>, and potentially <code>Auto-ARIMA</code>. Choose labels such as <code>None</code>, <code>Quarterly</code>, <code>Every 4 months</code>, <code>Half-yearly</code>, or <code>Yearly</code>; Ophelia converts the choice to the model's numeric setting.</li>
            </ul>
            <p>Ophelia can suggest a sensible first set of these settings for you from the detected frequency and chosen model. Use <code>Use suggested settings</code> if you want a good practical starting point.</p>
            <p>Good practical starting points for monthly council data are often:</p>
            <ul>
              <li><code>ARIMA</code>: <code>p=1, d=1, q=0</code></li>
              <li><code>SARIMA</code>: <code>p=1, d=1, q=0</code>, with <code>P=1</code> only when a strong repeating pattern is detected.</li>
              <li><code>SARIMAX</code>: the same as <code>SARIMA</code>, with selected exogenous drivers included.</li>
            </ul>
          </div>
          <div class="help-card">
            <h3>How Driver Checks Work</h3>
            <p>Driver evidence is model-specific. A population column can look very strong in plain regression but weaker in <code>SARIMAX</code> if the target's own history, trend removal, or seasonal pattern already explains much of the movement.</p>
            <ul>
              <li>For <code>Regression</code>, Ophelia checks drivers against the target levels directly.</li>
              <li>For <code>ARIMAX</code>, <code>SARIMAX</code>, and <code>Auto-ARIMA</code> runs that use differencing, Ophelia checks the drivers on the same transformed scale as the target, so changes are compared with changes.</li>
              <li>The table headed <code>How the drivers look in this model</code> is therefore about that run only. It is not a permanent verdict on whether a column is good or bad.</li>
              <li>If a driver looks weak in a seasonal model but strong in regression, compare the forecast table and try a simpler model or fewer seasonal terms before throwing the driver away.</li>
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
            <p>If the first few fitted rows in the historic part of the table show <code>NAN</code>, that is usually not a fault. It means the model needs one or more earlier periods to get started, so those first rows are real dates with real actuals but not yet enough prior history for a fitted value.</p>
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
              <li><code>Download forecast.csv</code> gives you <code>date</code>, <code>actual</code>, and the forecast columns. With one model, it includes <code>mean</code>, <code>stderr</code>, <code>lower</code>, and <code>upper</code>. With several models, it gives each model's <code>mean</code> and <code>stderr</code> side by side.</li>
              <li>The QR/mobile card in the header points to this same lab, including the separate <code>/ophelia/</code> public path.</li>
            </ul>
          </div>
        </div>
      </section>
    </section>

  </main>

  <iframe name="forecast-transport" title="forecast transport" hidden></iframe>

  <script>
    window.addEventListener('message', (event) => {
      const data = event && event.data;
      if (!data || data.kind !== 'ophelia-forecast-result') return;
      if (typeof window.__opheliaApplyForecastResult === 'function') {
        window.__opheliaApplyForecastResult(data);
      }
    });

    window.__opheliaRunForecast = async function () {
      const basePath = "__BASE_PATH__";
      const form = document.getElementById('forecast-form');
      const status = document.getElementById('status');
      const button = document.getElementById('run-button');
      const summaryBox = document.getElementById('summary-box');
      const metricModel = document.getElementById('metric-model');
    const metricFitRows = document.getElementById('metric-fit-rows');
    const metricStationary = document.getElementById('metric-stationary');
    const metricInvertible = document.getElementById('metric-invertible');
    const forecastTableWrap = document.querySelector('.forecast-table-wrap');
    const forecastHead = document.getElementById('forecast-head');
      const forecastBody = document.getElementById('forecast-body');
      const downloadSummary = document.getElementById('download-summary');
      const downloadForecast = document.getElementById('download-forecast');
      if (!form) {
        if (status) {
          status.textContent = 'Forecast form not found.';
          status.className = 'status error';
        }
        return false;
      }
      if (typeof window.syncXregColumnsFromChecks === 'function') window.syncXregColumnsFromChecks();
      if (typeof window.syncOutlierDatesFromChecks === 'function') window.syncOutlierDatesFromChecks();
      const payload = {};
      for (const element of form.elements) {
        if (!element.name) continue;
        payload[element.name] = element.value;
      }
      if (button) button.disabled = true;
      if (status) {
        status.textContent = 'Submitting forecast...';
        status.className = 'status';
      }
      fetch(`${basePath}/state`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      }).catch(() => {});
      try {
        const response = await fetch(`${basePath}/forecast`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload),
        });
        if (status) status.textContent = 'Reading forecast result...';
        const data = await response.json();
        if (!response.ok || !data.ok) {
          throw new Error(data.error || 'Forecast failed');
        }
        if (typeof window.__opheliaApplyForecastResult === 'function') {
          window.__opheliaApplyForecastResult(data);
        }
        return true;
      } catch (error) {
        if (status) {
          status.textContent = error.message || String(error);
          status.className = 'status error';
        }
        return false;
      } finally {
        if (button) button.disabled = false;
      }
    };
  </script>

  <script>
    const basePath = __BASE_PATH_JSON__;
    const defaults = __INITIAL_STATE__;
    const initialTargetColumns = __INITIAL_TARGET_COLUMNS_JSON__;
    const initialTargetMetaMap = __INITIAL_TARGET_META_MAP_JSON__;
    const initialXregColumns = __INITIAL_XREG_COLUMNS_JSON__;
    const initialTargetMeta = __INITIAL_TARGET_META_JSON__;
    const initialXregMeta = __INITIAL_XREG_META_JSON__;
    const controlToken = __CONTROL_TOKEN__;
    const form = document.getElementById('forecast-form');
    const statusNode = document.getElementById('status');
    const runButton = document.getElementById('run-button');
    const suggestButton = document.getElementById('suggest-button');
    const chartToggle = document.getElementById('chart-toggle');
    const helpToggle = document.getElementById('help-toggle');
    const summaryBox = document.getElementById('summary-box');
    const forecastChartBody = document.getElementById('forecast-chart-body');
    const forecastHead = document.getElementById('forecast-head');
    const forecastBody = document.getElementById('forecast-body');
    const metricModel = document.getElementById('metric-model');
    const metricFitRows = document.getElementById('metric-fit-rows');
    const metricStationary = document.getElementById('metric-stationary');
    const metricInvertible = document.getElementById('metric-invertible');
    const downloadSummary = document.getElementById('download-summary');
    const downloadForecast = document.getElementById('download-forecast');
    const layout = document.querySelector('.layout');
    const resultGrid = document.querySelector('.result-grid');
    const chartPane = document.getElementById('chart-pane');
    const helpPane = document.getElementById('help-pane');
    const mobileTitle = document.getElementById('mobile-title');
    const mobileStatus = document.getElementById('mobile-status');
    const mobileUrl = document.getElementById('mobile-url');
    const mobileHint = document.getElementById('mobile-hint');
    const qrBox = document.getElementById('qr-box');
    const funnelToggle = document.getElementById('funnel-toggle');
    const refreshMobile = document.getElementById('refresh-mobile');
    const targetUpload = document.getElementById('target-upload');
    const targetUploadName = document.getElementById('target-upload-name');
    const targetPathReadout = document.getElementById('target-path-readout');
    const targetMetaBox = document.getElementById('target-meta');
    const xregUpload = document.getElementById('xreg-upload');
    const xregUploadName = document.getElementById('xreg-upload-name');
    const xregPathReadout = document.getElementById('xreg-path-readout');
    const xregMetaBox = document.getElementById('xreg-meta');
    const xregColumnsInput = form.elements.namedItem('xreg_columns');
    const xregColumnsSummary = document.getElementById('xreg-columns-summary');
    const xregColumnsList = document.getElementById('xreg-columns-list');
    const xregColumnsStatus = document.getElementById('xreg-columns-status');
    const xregColumnsDebug = document.getElementById('xreg-columns-debug');
    const modelSummary = document.getElementById('model-summary');
    const modelList = document.getElementById('model-list');
    const targetPickerStatus = document.getElementById('target-picker-status');
    const targetValueSelect = form.elements.namedItem('target_value_column');
    const forecastEndInput = form.elements.namedItem('forecast_end_date');
    const detectedFrequencyReadout = document.getElementById('detected-frequency-readout');
    const seasonalSetupWarning = document.getElementById('seasonal-setup-warning');
    const outlierPanel = document.querySelector('.outlier-panel');
    const outlierList = document.getElementById('outlier-list');
    const outlierStatus = document.getElementById('outlier-status');
    const outlierModeSelect = form.elements.namedItem('outlier_mode');
    const outlierDatesInput = form.elements.namedItem('outlier_dates');
    const driversUsedNote = document.getElementById('drivers-used-note');
    const targetDateSelect = form.elements.namedItem('target_date_column');
    const xregDateSelect = form.elements.namedItem('xreg_date_column');
    const modelsField = form.elements.namedItem('models');
    let forecastEndOptionsHydrated = false;
    let targetMetaRequestId = 0;
    const targetMetaCache = Object.assign({}, initialTargetMetaMap || {});
    let latestSummary = '';
    let latestForecastCsv = '';
    window.__opheliaLatestSummary = '';
    window.__opheliaLatestForecastCsv = '';
    let saveTimer = null;
    let initialHydrating = true;
    let currentOutliers = __INITIAL_OUTLIERS_JSON__;
    let seasonPeriodManuallyEdited = false;
    const modelOrderEdited = {
      p: false,
      d: false,
      q: false,
      P: false,
      D: false,
      Q: false,
    };
    const modelLabels = {
      'regression': 'Regression',
      'arima': 'ARIMA',
      'arimax': 'ARIMAX',
      'sarima': 'SARIMA',
      'sarimax': 'SARIMAX',
      'auto-arima': 'Auto-ARIMA',
    };

    function seasonPeriodOptionsForFrequency(frequency) {
      const token = String(frequency || '').trim().toLowerCase();
      if (token === 'monthly') {
        return [
          { value: '0', label: 'None' },
          { value: '1', label: 'Weak / unclear' },
          { value: '3', label: 'Quarterly' },
          { value: '4', label: 'Every 4 months' },
          { value: '6', label: 'Half-yearly' },
          { value: '12', label: 'Yearly' },
        ];
      }
      if (token === 'quarterly') {
        return [
          { value: '0', label: 'None' },
          { value: '1', label: 'Weak / unclear' },
          { value: '2', label: 'Half-yearly' },
          { value: '4', label: 'Yearly' },
        ];
      }
      if (token === 'yearly') {
        return [
          { value: '0', label: 'None' },
          { value: '1', label: 'Weak / unclear' },
        ];
      }
      return [
        { value: '0', label: 'None' },
        { value: '1', label: 'Weak / unclear' },
      ];
    }

    function renderSeasonPeriodOptions(frequency, selectedValue='') {
      const seasonField = form.elements.namedItem('season_period');
      const options = seasonPeriodOptionsForFrequency(frequency);
      const selected = String(selectedValue || '').trim();
      const fallback = options.length ? options[0].value : '0';
      const chosen = options.some((option) => option.value === selected) ? selected : fallback;
      seasonField.innerHTML = options.map((option) => {
        const selectedAttr = option.value === chosen ? ' selected' : '';
        return `<option value="${escapeHtml(option.value)}"${selectedAttr}>${escapeHtml(option.label)}</option>`;
      }).join('');
      seasonField.value = chosen;
    }

    function updateSeasonalSetupWarning() {
      if (!seasonalSetupWarning) return;
      const selectedModels = currentSelectedModels();
      const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
      const label = String(targetMetaBox && targetMetaBox.dataset.seasonalityLabel || '').trim();
      const lag = Number.parseInt(String(targetMetaBox && targetMetaBox.dataset.seasonalityLag || '').trim(), 10);
      const seasonPeriod = String(form.elements.namedItem('season_period').value || '').trim();
      const P = String(form.elements.namedItem('P').value || '').trim();
      const D = String(form.elements.namedItem('D').value || '').trim();
      const Q = String(form.elements.namedItem('Q').value || '').trim();
      const seasonalModel = selectedModels.some((model) => isSeasonalModel(model));

      let level = '';
      let title = '';
      let message = '';

      if (seasonalModel) {
        if (strength === 'none') {
          if (seasonPeriod !== '0' || P !== '0' || D !== '0' || Q !== '0') {
            level = 'rating-mediocre';
            title = 'Seasonal settings check';
            message = 'No seasonality was detected, so seasonal settings may be adding unnecessary complexity. A safer first try is Season length = None and P, D, Q all set to 0.';
          } else {
            level = 'rating-good';
            title = 'Seasonal settings check';
            message = 'No seasonality was detected, and your seasonal settings are currently turned off. That is a sensible starting point.';
          }
        } else if (strength === 'weak') {
          if (seasonPeriod !== '1' || P !== '0') {
            level = 'rating-mediocre';
            title = 'Seasonal settings check';
            message = 'Only a weak seasonal signal was detected. Keep the seasonal structure light: Weak / unclear season length and usually P = 0 are safer starting choices.';
          } else {
            level = 'rating-good';
            title = 'Seasonal settings check';
            message = 'Only a weak seasonal signal was detected, and your seasonal settings are staying light. That is a sensible starting point.';
          }
        } else if (lag > 1) {
          if (seasonPeriod !== String(lag) || P !== '1') {
            level = 'rating-poor';
            title = 'Seasonal settings check';
            message = `${label || 'A strong repeating seasonal pattern was detected.'} Your current seasonal settings do not match that pattern closely. A better first try is Season length = ${lag} and P = 1.`;
          } else {
            level = 'rating-good';
            title = 'Seasonal settings check';
            message = `${label || 'A repeating seasonal pattern was detected.'} Your current seasonal settings match that pattern reasonably well for a first run.`;
          }
        }
      } else if ((strength === 'moderate' || strength === 'strong' || strength === 'very strong') && lag > 1) {
        level = 'rating-mediocre';
        title = 'Seasonality note';
        message = `${label || 'A repeating seasonal pattern was detected.'} The selected models are non-seasonal, so they will not use Season length or P, D, Q. Consider SARIMA or SARIMAX if you want the model to use that seasonal pattern.`;
      }

      if (!message) {
        seasonalSetupWarning.innerHTML = '';
        seasonalSetupWarning.className = 'setup-advisory hidden';
        return;
      }
      seasonalSetupWarning.innerHTML = `<strong>${escapeHtml(title)}:</strong> ${escapeHtml(message)}`;
      seasonalSetupWarning.className = `setup-advisory ${level}`.trim();
    }

    function suggestedSeasonLength(frequency) {
      const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
      const lag = Number.parseInt(String(targetMetaBox && targetMetaBox.dataset.seasonalityLag || '').trim(), 10);
      if ((strength === 'very strong' || strength === 'strong' || strength === 'moderate')
          && Number.isFinite(lag) && lag > 1) {
        return String(lag);
      }
      if (strength === 'none') return '0';
      if (strength === 'weak') return '1';
      const token = String(frequency || '').trim().toLowerCase();
      if (token === 'monthly') return '12';
      if (token === 'quarterly') return '4';
      if (token === 'yearly') return '1';
      return '';
    }

    function suggestedSeasonalLookback() {
      const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
      if (strength === 'none' || strength === 'weak') return '0';
      if (strength === 'moderate' || strength === 'strong' || strength === 'very strong') return '1';
      return '0';
    }

    function hasClearSeasonalitySuggestion() {
      const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
      const lag = Number.parseInt(String(targetMetaBox && targetMetaBox.dataset.seasonalityLag || '').trim(), 10);
      return (strength === 'moderate' || strength === 'strong' || strength === 'very strong')
        && Number.isFinite(lag)
        && lag > 1
        && currentSelectedModels().some((model) => isSeasonalModel(model));
    }

    function suggestedModelSettings() {
      const model = preferredSuggestionModel();
      const frequency = String(form.elements.namedItem('frequency').value || '').trim().toLowerCase();
      const seasonPeriod = suggestedSeasonLength(frequency) || String(form.elements.namedItem('season_period').value || '').trim() || '0';
      const seasonalLookback = suggestedSeasonalLookback();
      const suggested = {
        criterion: 'aic',
        level: '0.95',
        season_period: seasonPeriod,
        p: '1',
        d: '1',
        q: '0',
        P: '0',
        D: '0',
        Q: '0',
      };

      if (model === 'regression') {
        suggested.p = '0';
        suggested.d = '0';
        suggested.q = '0';
        suggested.P = '0';
        suggested.D = '0';
        suggested.Q = '0';
      } else if (model === 'sarima' || model === 'sarimax') {
        suggested.P = seasonalLookback;
        suggested.D = '0';
        suggested.Q = '0';
      } else if (model === 'auto-arima') {
        suggested.p = '2';
        suggested.d = '1';
        suggested.q = '1';
        suggested.P = seasonalLookback;
        suggested.D = '0';
        suggested.Q = '0';
      }

      return suggested;
    }

    function applySuggestedModelSettings(force=false) {
      const suggested = suggestedModelSettings();
      const seasonField = form.elements.namedItem('season_period');
      renderSeasonPeriodOptions(form.elements.namedItem('frequency').value, String(seasonField.value || '').trim());
      const clearSeasonalitySuggestion = hasClearSeasonalitySuggestion();
      let changed = false;
      const setFieldValue = (field, value) => {
        const next = String(value ?? '');
        if (String(field.value || '') !== next) {
          field.value = next;
          changed = true;
        }
      };
      const applyValue = (name, value, edited=false) => {
        const field = form.elements.namedItem(name);
        if (!field) return;
        const current = String(field.value || '').trim();
        const canReplaceUntouchedDefault = clearSeasonalitySuggestion
          && (current === '' || current === '0');
        if (force || canReplaceUntouchedDefault || !edited || !current) {
          setFieldValue(field, value);
        }
      };

      applyValue('criterion', suggested.criterion);
      applyValue('level', suggested.level);
      applyValue('p', suggested.p, modelOrderEdited.p);
      applyValue('d', suggested.d, modelOrderEdited.d);
      applyValue('q', suggested.q, modelOrderEdited.q);
      applyValue('P', suggested.P, modelOrderEdited.P);
      applyValue('D', suggested.D, modelOrderEdited.D);
      applyValue('Q', suggested.Q, modelOrderEdited.Q);
      if (seasonField && (force || clearSeasonalitySuggestion || !seasonPeriodManuallyEdited || !String(seasonField.value || '').trim())) {
        setFieldValue(seasonField, suggested.season_period);
      }
      updateSeasonalSetupWarning();
      if (changed && !initialHydrating) {
        scheduleStateSave();
      }
    }

    function applyState(state) {
      for (const [key, value] of Object.entries(state || {})) {
        const field = form.elements.namedItem(key);
        if (!field) continue;
        field.value = value;
      }
    }

    function renderModelChecksFromState(selectedText) {
      const selected = splitSelectedModels(selectedText);
      const set = new Set(selected);
      const inputs = modelList
        ? Array.from(modelList.querySelectorAll('input[name="model-choice"]'))
        : [];
      if (!inputs.length) return;
      let checkedCount = 0;
      inputs.forEach((input, index) => {
        const shouldCheck = set.size ? set.has(String(input.value || '').trim().toLowerCase()) : index === 0;
        input.checked = shouldCheck;
        if (shouldCheck) checkedCount += 1;
      });
      if (!checkedCount) {
        inputs[0].checked = true;
      }
    }

    function collectState() {
      const payload = {};
      for (const element of form.elements) {
        if (!element.name) continue;
        payload[element.name] = element.value;
      }
      const selectedModels = currentSelectedModels();
      payload.models = selectedModels.join(', ');
      payload.xreg_columns = Array.from(
        xregColumnsList.querySelectorAll('input[name="xreg-column-choice"]:checked')
      ).map((input) => input.value).join(', ');
      payload.outlier_dates = Array.from(
        outlierList.querySelectorAll('input[name="outlier-choice"]:checked')
      ).map((input) => input.value).join(', ');
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

    function renderSeriesMeta(meta) {
      const data = meta && typeof meta === 'object' ? meta : {};
      const frequency = String(data.detected_frequency_label || 'Unknown').trim() || 'Unknown';
      const start = String(data.start_date || '').trim() || 'Unknown';
      const end = String(data.end_date || '').trim() || 'Unknown';
      const usableStart = String(data.usable_start_date || '').trim();
      const usableEnd = String(data.usable_end_date || '').trim();
      const dateColumn = String(data.date_column || '').trim() || 'Unknown';
      const valueColumn = String(data.value_column || '').trim();
      const seasonality = String(data.seasonality_label || '').trim();
      const seasonalityStrength = String(data.seasonality_strength || '').trim() || 'none';
      const seasonalityScore = String(data.seasonality_score ?? '').trim();
      const seasonalityLag = String(data.seasonality_lag ?? '').trim();
      const seasonalityClass = `seasonality-${seasonalityStrength.replace(/\s+/g, '-').toLowerCase()}`;
      const seasonalityDetail = [];
      if (seasonalityScore) seasonalityDetail.push(`score ${seasonalityScore}`);
      if (seasonalityLag && seasonalityLag !== '0') seasonalityDetail.push(`lag ${seasonalityLag}`);
      const seasonalityText = seasonalityDetail.length
        ? `${seasonality} (${seasonalityDetail.join(', ')})`
        : seasonality;
      return ``
        + `<div><strong>Detected frequency:</strong> ${escapeHtml(frequency)}</div>`
        + `<div><strong>Date column:</strong> ${escapeHtml(dateColumn)}</div>`
        + (valueColumn ? `<div><strong>Series:</strong> ${escapeHtml(valueColumn)}</div>` : '')
        + `<div><strong>File date range:</strong> ${escapeHtml(start)} to ${escapeHtml(end)}</div>`
        + `<div><strong>Usable series range:</strong> ${escapeHtml(usableStart || start)} to ${escapeHtml(usableEnd || end)}</div>`
        + (seasonality
            ? `<div class="seasonality-line"><strong>Seasonality:</strong> <span class="seasonality-badge ${escapeHtml(seasonalityClass)}">${escapeHtml(seasonalityText)}</span></div>`
            : '');
    }

    function applyTargetMeta(meta) {
      const data = meta && typeof meta === 'object' ? meta : {};
      const valueColumn = String(data.value_column || '').trim();
      if (valueColumn) {
        targetMetaCache[valueColumn] = data;
      }
      targetMetaBox.innerHTML = renderSeriesMeta(data);
      targetMetaBox.dataset.endDate = String(data.usable_end_date_iso || data.end_date_iso || '').trim();
      targetMetaBox.dataset.seasonalityLabel = String(data.seasonality_label || '').trim();
      targetMetaBox.dataset.seasonalityStrength = String(data.seasonality_strength || '').trim().toLowerCase();
      targetMetaBox.dataset.seasonalityLag = String(data.seasonality_lag || '').trim();
      renderDateColumnPicker(targetDateSelect, data.date_candidates || [], targetDateSelect.value, data.date_column || '');
      renderOutlierPicker(data.outliers || [], outlierDatesInput.value);
      const token = String(data.detected_frequency || '').trim();
      if (token) {
        form.elements.namedItem('frequency').value = token;
        detectedFrequencyReadout.value = String(data.detected_frequency_label || token);
        detectedFrequencyReadout.title = String(data.detected_frequency_label || token);
        renderSeasonPeriodOptions(token, String(form.elements.namedItem('season_period').value || '').trim());
        if (!initialHydrating) {
          applySuggestedModelSettings(false);
        }
      }
      updateSeasonalSetupWarning();
      buildForecastEndOptions();
    }

    function applyXregMeta(meta) {
      const data = meta && typeof meta === 'object' ? meta : {};
      xregMetaBox.innerHTML = renderSeriesMeta(data);
      xregMetaBox.dataset.endDate = String(data.usable_end_date_iso || data.end_date_iso || '').trim();
      renderDateColumnPicker(xregDateSelect, data.date_candidates || [], xregDateSelect.value, data.date_column || '');
      buildForecastEndOptions();
    }

    function parseIsoDate(text) {
      const raw = String(text || '').trim();
      if (!raw) return null;
      const value = new Date(`${raw}T00:00:00`);
      return Number.isNaN(value.getTime()) ? null : value;
    }

    function formatIsoDate(value) {
      if (!(value instanceof Date) || Number.isNaN(value.getTime())) return '';
      const year = value.getFullYear();
      const month = String(value.getMonth() + 1).padStart(2, '0');
      const day = String(value.getDate()).padStart(2, '0');
      return `${year}-${month}-${day}`;
    }

    function periodEndDate(value, frequency, yearType) {
      const out = new Date(value.getTime());
      if (frequency === 'daily') {
        return out;
      }
      if (frequency === 'monthly') {
        return new Date(out.getFullYear(), out.getMonth() + 1, 0);
      }
      if (frequency === 'quarterly') {
        const month = out.getMonth() + 1;
        const quarterEnds = yearType === 'fiscal' ? [6, 9, 12, 3] : [3, 6, 9, 12];
        for (const endMonth of quarterEnds) {
          let year = out.getFullYear();
          if (yearType === 'fiscal' && endMonth === 3 && month >= 4) {
            year += 1;
          }
          const candidate = new Date(year, endMonth, 0);
          if (candidate >= out) return candidate;
        }
        return new Date(out.getFullYear(), 12, 0);
      }
      if (frequency === 'yearly') {
        if (yearType === 'fiscal') {
          const year = out.getMonth() + 1 <= 3 ? out.getFullYear() : out.getFullYear() + 1;
          return new Date(year, 3, 0);
        }
        return new Date(out.getFullYear(), 12, 0);
      }
      return out;
    }

    function addPeriods(value, frequency, count, yearType) {
      const out = new Date(value.getTime());
      if (frequency === 'daily') {
        out.setDate(out.getDate() + count);
        return out;
      }
      if (frequency === 'monthly') {
        return new Date(out.getFullYear(), out.getMonth() + count + 1, 0);
      }
      if (frequency === 'quarterly') {
        return periodEndDate(new Date(out.getFullYear(), out.getMonth() + (count * 3), 1), frequency, yearType);
      }
      if (frequency === 'yearly') {
        if (yearType === 'fiscal') {
          const startYear = out.getMonth() + 1 <= 3 ? out.getFullYear() - 1 : out.getFullYear();
          return new Date(startYear + count + 1, 3, 0);
        }
        return new Date(out.getFullYear() + count, 12, 0);
      }
      return out;
    }

    function formatUkDate(value) {
      if (!(value instanceof Date) || Number.isNaN(value.getTime())) return '';
      const day = String(value.getDate()).padStart(2, '0');
      const month = String(value.getMonth() + 1).padStart(2, '0');
      const year = value.getFullYear();
      return `${day}/${month}/${year}`;
    }

    function buildForecastEndOptions() {
      if (!forecastEndOptionsHydrated && forecastEndInput && forecastEndInput.options && forecastEndInput.options.length > 1) {
        forecastEndOptionsHydrated = true;
        return;
      }
      forecastEndOptionsHydrated = true;
      const targetEnd = parseIsoDate(targetMetaBox.dataset.endDate || '');
      const xregEnd = parseIsoDate(xregMetaBox.dataset.endDate || '');
      const frequency = String(form.elements.namedItem('frequency').value || '').trim();
      const yearType = String(form.elements.namedItem('year_type').value || '').trim();
      if (!targetEnd || !frequency) {
        forecastEndInput.innerHTML = '<option value="">Choose a forecast end date</option>';
        return;
      }
      let limit = addPeriods(targetEnd, frequency, 24, yearType);
      if (xregEnd && xregEnd > targetEnd) limit = xregEnd;
      const current = String(forecastEndInput.value || '').trim();
      const options = [];
      let cursor = periodEndDate(addPeriods(targetEnd, frequency, 1, yearType), frequency, yearType);
      while (cursor <= limit && options.length < 1200) {
        const iso = formatIsoDate(cursor);
        options.push({ value: iso, label: formatUkDate(cursor) });
        cursor = periodEndDate(addPeriods(cursor, frequency, 1, yearType), frequency, yearType);
      }
      const selected = options.some((item) => item.value === current)
        ? current
        : (options.length ? options[options.length - 1].value : '');
      if (!options.length) {
        forecastEndInput.innerHTML = '<option value="">Choose a forecast end date</option>';
        return;
      }
      const grouped = new Map();
      for (const item of options) {
        const year = item.value.slice(0, 4) || 'Other';
        if (!grouped.has(year)) grouped.set(year, []);
        grouped.get(year).push(item);
      }
      const chunks = [];
      for (const year of Array.from(grouped.keys()).sort()) {
        chunks.push(`<optgroup label="${escapeHtml(year)}">`);
        for (const item of grouped.get(year)) {
          chunks.push(`<option value="${escapeHtml(item.value)}"${item.value === selected ? ' selected' : ''}>${escapeHtml(item.label)}</option>`);
        }
        chunks.push('</optgroup>');
      }
      forecastEndInput.innerHTML = chunks.join('');
    }

    function splitSelectedColumns(text) {
      return String(text || '')
        .split(',')
        .map((value) => value.trim())
        .filter(Boolean);
    }

    function splitSelectedModels(text) {
      return String(text || '')
        .split(',')
        .map((value) => value.trim().toLowerCase())
        .filter(Boolean);
    }

    function currentSelectedModels() {
      const checked = modelList
        ? Array.from(modelList.querySelectorAll('input[name="model-choice"]:checked'))
            .map((input) => String(input.value || '').trim().toLowerCase())
            .filter(Boolean)
        : [];
      if (checked.length) return checked;
      const hidden = splitSelectedModels(modelsField && modelsField.value);
      if (hidden.length) return hidden;
      return [];
    }

    function isSeasonalModel(model) {
      const token = String(model || '').trim().toLowerCase();
      return token === 'sarima' || token === 'sarimax' || token === 'auto-arima';
    }

    function isTimeSeriesModel(model) {
      const token = String(model || '').trim().toLowerCase();
      return token === 'arima' || token === 'arimax' || token === 'sarima' || token === 'sarimax' || token === 'auto-arima';
    }

    function preferredSuggestionModel() {
      const selected = currentSelectedModels();
      if (!selected.length) return 'sarimax';
      const strength = String(targetMetaBox && targetMetaBox.dataset.seasonalityStrength || '').trim().toLowerCase();
      if (strength === 'moderate' || strength === 'strong' || strength === 'very strong') {
        const seasonal = selected.find((model) => isSeasonalModel(model));
        if (seasonal) return seasonal;
      }
      const timeSeries = selected.find((model) => isTimeSeriesModel(model));
      if (timeSeries) return timeSeries;
      return selected[0];
    }

    function formatModelSummary(values) {
      if (!values.length) return '<strong>none yet</strong>';
      return escapeHtml(values.map((value) => modelLabels[value] || value).join(', '));
    }

    function syncModelsFromChecks() {
      const selected = Array.from(
        modelList.querySelectorAll('input[name="model-choice"]:checked')
      ).map((input) => input.value);
      if (!selected.length) {
        const first = modelList.querySelector('input[name="model-choice"]');
        if (first) {
          first.checked = true;
          selected.push(first.value);
        }
      }
      modelsField.value = selected.join(', ');
      modelSummary.innerHTML = `Selected: ${formatModelSummary(selected)}`;
      modelSummary.title = selected.join(', ');
    }

    function formatXregSummary(values) {
      if (!values.length) return '<strong>none yet</strong>';
      if (values.length === 1) return `<strong>1 column</strong>: ${escapeHtml(values[0])}`;
      if (values.length <= 3) return `<strong>${values.length} columns</strong>: ${escapeHtml(values.join(', '))}`;
      return `<strong>${values.length} columns</strong>: ${escapeHtml(values.slice(0, 3).join(', '))} + ${values.length - 3} more`;
    }

    function splitSelectedDates(text) {
      return String(text || '')
        .split(',')
        .map((value) => value.trim())
        .filter(Boolean);
    }

    function renderOutlierPicker(outliers, selectedText) {
      currentOutliers = Array.isArray(outliers) ? outliers : [];
      const selected = new Set(splitSelectedDates(selectedText));
      if (!currentOutliers.length) {
        outlierPanel.classList.add('no-outliers');
        outlierList.innerHTML = '<div class="multi-select-placeholder">No target points are currently flagged here.</div>';
        outlierDatesInput.value = '';
        outlierStatus.textContent = 'No outliers detected.';
        return;
      }
      outlierPanel.classList.remove('no-outliers');
      if (!selected.size) {
        for (const item of currentOutliers) {
          const iso = String(item.date_iso || '').trim();
          if (iso) selected.add(iso);
        }
      }
      outlierList.innerHTML = currentOutliers.map((item) => {
        const iso = String(item.date_iso || '').trim();
        const checked = selected.has(iso) ? ' checked' : '';
        const score = Math.abs(Number(item.score || 0));
        const likelihood = score >= 20 ? 'extremely likely' : (score >= 10 ? 'very likely' : (score >= 6 ? 'likely' : 'possibly'));
        const label = `${item.date || ''}: ${item.value || ''} (${likelihood})`;
        return `<label class="multi-select-item" title="${escapeHtml(label)}"><input type="checkbox" name="outlier-choice" value="${escapeHtml(iso)}"${checked}><span>${escapeHtml(label)}</span></label>`;
      }).join('');
      syncOutlierDatesFromChecks();
    }

    function syncOutlierDatesFromChecks() {
      const selected = Array.from(
        outlierList.querySelectorAll('input[name="outlier-choice"]:checked')
      ).map((input) => input.value);
      outlierDatesInput.value = selected.join(', ');
      outlierStatus.textContent = selected.length
        ? `${selected.length} outlier${selected.length === 1 ? '' : 's'} selected for possible handling.`
        : (currentOutliers.length ? 'No outliers selected for handling.' : 'No outliers detected.');
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
      if (xregColumnsDebug) {
        xregColumnsDebug.textContent = `Will send: ${selected.length ? selected.join(', ') : '(none)'}`;
      }
    }

    function renderXregColumnsPicker(columns, selectedText) {
      const selected = new Set(splitSelectedColumns(selectedText));
      const options = (Array.isArray(columns) ? columns : [])
        .map((column) => {
          if (column && typeof column === 'object') {
            return {
              name: String(column.name || '').trim(),
              usable: Boolean(column.usable),
              reason: String(column.reason || '').trim(),
            };
          }
          return {
            name: String(column || '').trim(),
            usable: true,
            reason: '',
          };
        })
        .filter((column) => column.name);
      if (!options.length) {
        xregColumnsList.innerHTML = '<div class="multi-select-placeholder">Upload or choose an exogenous CSV to load a scrollable list of driver columns here.</div>';
        xregColumnsInput.value = '';
        xregColumnsSummary.innerHTML = 'Selected: <strong>none yet</strong>';
        xregColumnsSummary.title = 'No exogenous columns selected';
        xregColumnsStatus.textContent = (Array.isArray(columns) ? columns.length : 0)
          ? 'No selectable driver columns were found in this CSV.'
          : 'Upload or choose an exogenous CSV to load its available columns.';
        return;
      }

      xregColumnsList.innerHTML = options.map((column) => {
        const checked = column.usable && selected.has(column.name) ? ' checked' : '';
        const disabled = column.usable ? '' : ' disabled';
        const disabledClass = column.usable ? '' : ' is-disabled';
        const title = column.reason || column.name;
        return `<label class="multi-select-item${disabledClass}" title="${escapeHtml(title)}"><input type="checkbox" name="xreg-column-choice" value="${escapeHtml(column.name)}"${checked}${disabled}><span>${escapeHtml(column.name)}</span></label>`;
      }).join('');
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

    function renderDateColumnPicker(selectNode, columns, selected, fallback='') {
      const items = Array.isArray(columns)
        ? columns.map((column) => String(column || '').trim()).filter(Boolean)
        : [];
      let chosen = String(selected || '').trim() || String(fallback || '').trim();
      if (!items.length) {
        selectNode.innerHTML = chosen
          ? `<option value="${escapeHtml(chosen)}" selected>${escapeHtml(chosen)}</option>`
          : '<option value="">No date columns found</option>';
        return;
      }
      if (items.length === 1) chosen = items[0];
      if (!chosen || !items.includes(chosen)) chosen = items[0];
      selectNode.innerHTML = items.map((column) => {
        const selectedAttr = column === chosen ? ' selected' : '';
        return `<option value="${escapeHtml(column)}"${selectedAttr}>${escapeHtml(column)}</option>`;
      }).join('');
      selectNode.value = chosen;
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
        Object.keys(targetMetaCache).forEach((key) => { delete targetMetaCache[key]; });
        applyTargetMeta(data);

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
        applyXregMeta(data);
        renderXregColumnsPicker(
          data.value_column_details || data.value_columns || [],
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
        renderXregColumnsPicker([], xregColumnsInput.value);
        return;
      }

      try {
        const dateColumn = String(form.elements.namedItem('xreg_date_column').value || '').trim();
        const selectedColumns = String(xregColumnsInput.value || '').trim();
        const response = await fetch(`${basePath}/target-columns?path=${encodeURIComponent(currentPath)}&date_column=${encodeURIComponent(dateColumn)}&selected_columns=${encodeURIComponent(selectedColumns)}`, {
          cache: 'no-store',
        });
        const data = await response.json();
        if (!response.ok || !data.ok) {
          applyXregMeta({});
          renderXregColumnsPicker([], xregColumnsInput.value);
          return;
        }
        applyXregMeta(data);
        renderXregColumnsPicker(
          data.value_column_details || data.value_columns || [],
          xregColumnsInput.value
        );
      } catch (_) {
        applyXregMeta({});
        renderXregColumnsPicker([], xregColumnsInput.value);
      }
    }

    async function loadExistingTargetColumns(requestedValue='') {
      const currentPath = String(form.elements.namedItem('target_path').value || '').trim();
      if (!currentPath) {
        return;
      }

      try {
        const dateColumn = String(form.elements.namedItem('target_date_column').value || '').trim();
        const valueColumn = String(requestedValue || form.elements.namedItem('target_value_column').value || '').trim();
        if (valueColumn && targetMetaCache[valueColumn]) {
          applyTargetMeta(targetMetaCache[valueColumn]);
          return;
        }
        if (valueColumn) {
          targetMetaBox.innerHTML = ''
            + `<div><strong>Series:</strong> ${escapeHtml(valueColumn)}</div>`
            + '<div class="summary-note">Refreshing seasonality and outlier checks for the selected target series...</div>';
        }
        const requestId = ++targetMetaRequestId;
        const response = await fetch(`${basePath}/target-columns?path=${encodeURIComponent(currentPath)}&date_column=${encodeURIComponent(dateColumn)}&value_column=${encodeURIComponent(valueColumn)}`, {
          cache: 'no-store',
        });
        const data = await response.json();
        if (requestId !== targetMetaRequestId) {
          return;
        }
        if (valueColumn && String(data.value_column || '').trim() && String(data.value_column || '').trim() !== valueColumn) {
          return;
        }
        if (!response.ok || !data.ok) {
          applyTargetMeta({});
          return;
        }
        applyTargetMeta(data);
      } catch (_) {
        applyTargetMeta({});
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
      chartPane.classList.add('hidden');
      helpPane.classList.add('hidden');
      if (layout) layout.classList.remove('chart-mode');
      if (chartToggle) chartToggle.textContent = 'Chart';
      helpToggle.textContent = 'Help';
      setStatus('Ready', '');
    }

    function showHelp() {
      resultGrid.classList.add('hidden');
      chartPane.classList.add('hidden');
      helpPane.classList.remove('hidden');
      if (layout) layout.classList.remove('chart-mode');
      if (chartToggle) chartToggle.textContent = 'Chart';
      helpToggle.textContent = 'Results';
      setStatus('Help', '');
    }

    function showChart() {
      resultGrid.classList.add('hidden');
      helpPane.classList.add('hidden');
      chartPane.classList.remove('hidden');
      if (layout) layout.classList.add('chart-mode');
      if (chartToggle) chartToggle.textContent = 'Results';
      helpToggle.textContent = 'Help';
      setStatus('Chart', '');
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

    function coefficientRatingClassFromRatio(ratio) {
      if (!isFinite(ratio)) return 'rating-comparison';
      if (ratio >= 4.0) return 'rating-exceptional';
      if (ratio >= 3.0) return 'rating-excellent';
      if (ratio >= 2.0) return 'rating-very-good';
      if (ratio >= 1.5) return 'rating-good';
      if (ratio >= 1.0) return 'rating-mediocre';
      return 'rating-poor';
    }

    function coefficientSupportText(ratio) {
      if (!isFinite(ratio)) return 'comparison only';
      if (ratio >= 4.0) return 'extremely likely helping';
      if (ratio >= 3.0) return 'very likely helping';
      if (ratio >= 2.0) return 'likely helping';
      if (ratio >= 1.5) return 'possibly helping';
      if (ratio >= 1.0) return 'weak evidence';
      return 'little evidence';
    }

    function selectedXregLabels() {
      return String(xregColumnsInput && xregColumnsInput.value || '')
        .split(',')
        .map((part) => part.trim())
        .filter(Boolean);
    }

    function coefficientDisplayName(index) {
      const labels = selectedXregLabels();
      if (index === 0) return 'Intercept';
      if (index - 1 < labels.length) return `Driver: ${labels[index - 1]}`;
      return `Coefficient ${index}`;
    }

    function parseCoefficientLine(line) {
      const betaMatch = line.match(/^beta\[(\d+)\] = ([^\s]+)\s+stderr = ([^\s]+)$/);
      const phiMatch = line.match(/^phi\[(\d+)\] = ([^\s]+)$/);
      if (betaMatch) {
        const index = Number(betaMatch[1]);
        const estimate = Number(betaMatch[2]);
        const stderr = Number(betaMatch[3]);
        const ratio = stderr ? Math.abs(estimate) / Math.abs(stderr) : Number.POSITIVE_INFINITY;
        return {
          type: 'beta',
          index,
          title: coefficientDisplayName(index),
          estimate,
          stderr,
          ratio,
        };
      }
      if (phiMatch) {
        return {
          type: 'phi',
          index: Number(phiMatch[1]),
          title: `AR term ${phiMatch[1]}`,
          estimate: Number(phiMatch[2]),
          stderr: NaN,
          ratio: NaN,
        };
      }
      return null;
    }

    function renderCoefficientCard(line) {
      const parsed = parseCoefficientLine(line.trim());
      if (!parsed) {
        return `<div class="summary-line"><span class="summary-v">${escapeHtml(line)}</span></div>`;
      }
      const ratingClass = coefficientRatingClassFromRatio(parsed.ratio);
      const supportText = parsed.type === 'beta'
        ? coefficientSupportText(parsed.ratio)
        : 'model term';
      const estimateText = Number.isFinite(parsed.estimate) ? parsed.estimate.toFixed(4) : String(parsed.estimate);
      const stderrText = Number.isFinite(parsed.stderr) ? parsed.stderr.toFixed(4) : '';
      const meta = parsed.type === 'beta'
        ? `Estimate ${escapeHtml(estimateText)} · StdErr ${escapeHtml(stderrText)}`
        : `Estimate ${escapeHtml(estimateText)}`;
      const note = parsed.type === 'beta'
        ? supportText
        : 'Autoregressive carry-over term';
      return `<div class="summary-coeff-card ${ratingClass}">
        <div class="summary-coeff-title">${escapeHtml(parsed.title)}</div>
        <div class="summary-coeff-value">${meta}</div>
        <div class="summary-coeff-meta">${escapeHtml(note)}</div>
      </div>`;
    }

    function renderSummaryText(text) {
      const source = normalizeSummaryText((text || '').trim());
      if (!source) {
        summaryBox.innerHTML = '<div class="summary-line note-line">No summary returned.</div>';
        return;
      }
      const lines = source.split(/\\r?\\n/).filter(line => line.length > 0);
      let inCoeffSection = false;
      let coeffCards = [];

      const html = lines.map((line, index) => {
        const trimmed = line.trim();
        let classes = ['summary-line'];
        let content = escapeHtml(line);
        const parsedCoeff = parseCoefficientLine(trimmed);

        if (/^Coefficients$/i.test(trimmed) || /^AR parameters$/i.test(trimmed)) {
          inCoeffSection = true;
          coeffCards = [];
        } else if (parsedCoeff && inCoeffSection) {
          coeffCards.push(renderCoefficientCard(trimmed));
          return '';
        } else if (inCoeffSection && coeffCards.length) {
          const grid = `<div class="summary-coeff-grid">${coeffCards.join('')}</div>`;
          coeffCards = [];
          inCoeffSection = false;
          return `${grid}${renderSummaryTextLine(line, index)}`;
        }

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

      const trailingCoeffGrid = coeffCards.length
        ? `<div class="summary-coeff-grid">${coeffCards.join('')}</div>`
        : '';

      summaryBox.innerHTML = (html + trailingCoeffGrid) || '<div class="summary-line note-line">No summary returned.</div>';
    }

    function normalizeSummaryText(text) {
      let source = String(text || '').trim();
      const markers = [
        'Overall assessment:',
        'Overall fit score (R2):',
        'Adjusted fit score (Adj R2):',
        'Typical forecast error (RMSE):',
        'Model comparison score (AIC):',
        'Model comparison score (BIC):',
        'Unexplained variation left in the model (Sigma2):',
        'How to read this:',
        'Coefficients',
        'AR parameters',
        'Model:'
      ];

      if (!source) return source;
      if (!/[\\r\\n]/.test(source)) {
        markers.forEach((marker, index) => {
          const replacement = index === 0 && source.startsWith(marker) ? marker : `\\n${marker}`;
          source = source.replaceAll(` ${marker}`, replacement);
        });
        source = source.replace(/ (beta\[\d+\] = )/g, '\\n$1');
        source = source.replace(/ (phi\[\d+\] = )/g, '\\n$1');
        source = source.replace(/ How to read this: - /g, '\\nHow to read this:\\n- ');
        source = source.replace(/ - (?=[A-Z])/g, '\\n- ');
      }

      source = source.replace(/\\r\\n/g, '\\n');
      source = source.replace(/\\n{3,}/g, '\\n\\n');
      return source;
    }

    function renderSummaryTextLine(line, index) {
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
      }
      return `<div class="${classes.join(' ')}">${content}</div>`;
    }

    window.__opheliaRenderSummary = renderSummaryText;

    function renderForecastTable(csvText) {
      const source = String(csvText || '').trim();
      if (!source) {
        forecastHead.innerHTML = '<tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr>';
        forecastBody.innerHTML = '<tr><td colspan="6">No forecast rows returned.</td></tr>';
        return;
      }

      const lines = source.split(/\\r?\\n/).filter(Boolean);
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

    function forecastTableMarkup(csvText) {
      const source = String(csvText || '').trim();
      if (!source) {
        return (
          '<div class="table-wrap comparison-table-wrap">'
          + '<div class="table-caption">No forecast rows returned.</div>'
          + '<div class="table-scroll"><table><thead><tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr></thead>'
          + '<tbody><tr><td colspan="6">No forecast rows returned.</td></tr></tbody></table></div></div>'
        );
      }
      const lines = source.split(/\\r?\\n/).filter(Boolean);
      const header = (lines[0] || '').split(',');
      const rows = lines.slice(1).map(line => line.split(','));
      const head = `<tr>${header.map(cell => `<th>${escapeHtml(cell || '')}</th>`).join('')}</tr>`;
      const body = rows.length
        ? rows.map(row => `<tr>${header.map((_, index) => `<td>${escapeHtml(row[index] || '')}</td>`).join('')}</tr>`).join('')
        : `<tr><td colspan="${header.length || 1}">No forecast rows returned.</td></tr>`;
      return (
        '<div class="table-wrap comparison-table-wrap">'
        + '<div class="table-caption">Actual vs forecast comparison for this model.</div>'
        + `<div class="table-scroll"><table><thead>${head}</thead><tbody>${body}</tbody></table></div></div>`
      );
    }

    function parseForecastCsvRowsForComparison(csvText) {
      const source = String(csvText || '').trim();
      if (!source) return [];
      const lines = source.split(/\\r?\\n/).filter(Boolean);
      if (!lines.length) return [];
      const header = (lines[0] || '').split(',').map((cell) => String(cell || '').trim().toLowerCase());
      return lines.slice(1).map((line) => {
        const cells = line.split(',');
        const row = {};
        header.forEach((key, index) => {
          row[key] = String(cells[index] || '').trim();
        });
        return row;
      });
    }

    const chartPalette = [
      '#2f6fed',
      '#e0529c',
      '#1f9d78',
      '#ef8a17',
      '#7757d6',
      '#c94c4c',
      '#0f7c95',
      '#7a6a18',
    ];
    const chartLowerColor = '#cc6b19';
    const chartUpperColor = '#b33951';

    function parseChartNumber(value) {
      const text = String(value ?? '').trim();
      if (!text || /^nan$/i.test(text)) return null;
      const parsed = Number(text.replace(/,/g, ''));
      return Number.isFinite(parsed) ? parsed : null;
    }

    function formatChartNumber(value) {
      const absolute = Math.abs(value);
      if (absolute >= 1000) return value.toLocaleString(undefined, { maximumFractionDigits: 0 });
      if (absolute >= 100) return value.toLocaleString(undefined, { maximumFractionDigits: 1 });
      if (absolute >= 10) return value.toLocaleString(undefined, { maximumFractionDigits: 2 });
      return value.toLocaleString(undefined, { maximumFractionDigits: 3 });
    }

    function chartDateMillis(label) {
      const text = String(label || '').trim();
      if (!text) return null;
      let match = text.match(/^(\\d{1,2})\\/(\\d{1,2})\\/(\\d{4})$/);
      if (match) {
        const day = Number(match[1]);
        const month = Number(match[2]);
        const year = Number(match[3]);
        const value = Date.UTC(year, month - 1, day);
        return Number.isFinite(value) ? value : null;
      }
      match = text.match(/^(\\d{4})-(\\d{1,2})-(\\d{1,2})$/);
      if (match) {
        const year = Number(match[1]);
        const month = Number(match[2]);
        const day = Number(match[3]);
        const value = Date.UTC(year, month - 1, day);
        return Number.isFinite(value) ? value : null;
      }
      const parsed = Date.parse(text);
      return Number.isFinite(parsed) ? parsed : null;
    }

    function formatChartAxisDate(value) {
      const date = new Date(value);
      if (Number.isNaN(date.getTime())) return '';
      const day = String(date.getUTCDate()).padStart(2, '0');
      const month = String(date.getUTCMonth() + 1).padStart(2, '0');
      const year = date.getUTCFullYear();
      return `${day}/${month}/${year}`;
    }

    function selectedForecastEndLabel() {
      if (!forecastEndInput) return '';
      const selected = forecastEndInput.options && forecastEndInput.selectedIndex >= 0
        ? forecastEndInput.options[forecastEndInput.selectedIndex]
        : null;
      return String((selected && selected.textContent) || forecastEndInput.value || '').trim();
    }

    function closestChartTickIndexes(dateValues, desiredCount) {
      const usable = dateValues
        .map((value, index) => ({ value, index }))
        .filter((item) => Number.isFinite(item.value));
      if (!usable.length) return [];
      if (usable.length <= desiredCount) {
        return usable.map((item) => item.index);
      }
      const start = usable[0].value;
      const end = usable[usable.length - 1].value;
      if (start === end) return [usable[0].index];
      const ticks = [];
      for (let slot = 0; slot < desiredCount; slot += 1) {
        const target = start + ((end - start) * slot / (desiredCount - 1));
        let best = usable[0];
        let bestDistance = Math.abs(best.value - target);
        usable.forEach((item) => {
          const distance = Math.abs(item.value - target);
          if (distance < bestDistance) {
            best = item;
            bestDistance = distance;
          }
        });
        if (!ticks.includes(best.index)) ticks.push(best.index);
      }
      return ticks.sort((a, b) => a - b);
    }

    function chartPath(values, xForIndex, yForValue) {
      let path = '';
      let active = false;
      values.forEach((value, index) => {
        if (!Number.isFinite(value)) {
          active = false;
          return;
        }
        const x = xForIndex(index).toFixed(2);
        const y = yForValue(value).toFixed(2);
        path += `${active ? 'L' : 'M'}${x},${y}`;
        active = true;
      });
      return path;
    }

    function lastFiniteIndex(values) {
      for (let index = values.length - 1; index >= 0; index -= 1) {
        if (Number.isFinite(values[index])) return index;
      }
      return -1;
    }

    function nearestChartPoint(points, x, y) {
      let best = null;
      let bestDistance = Infinity;
      (Array.isArray(points) ? points : []).forEach((point) => {
        const dx = Number(point.x) - x;
        const dy = Number(point.y) - y;
        const distance = (dx * dx) + (dy * dy * 0.35);
        if (distance < bestDistance) {
          best = point;
          bestDistance = distance;
        }
      });
      return best;
    }

    function attachChartTooltip(hoverSeries) {
      if (!forecastChartBody) return;
      const svg = forecastChartBody.querySelector('.forecast-chart-svg');
      const tooltip = forecastChartBody.querySelector('.chart-tooltip');
      const marker = svg ? svg.querySelector('.chart-hover-marker') : null;
      if (!svg || !tooltip || !marker) return;

      const hideTooltip = () => {
        tooltip.classList.add('hidden');
        marker.setAttribute('visibility', 'hidden');
      };

      svg.querySelectorAll('.chart-hover-path').forEach((path) => {
        path.addEventListener('mousemove', (event) => {
          const seriesIndex = Number(path.getAttribute('data-series-index'));
          const seriesInfo = hoverSeries[seriesIndex];
          if (!seriesInfo || !Array.isArray(seriesInfo.points) || !seriesInfo.points.length) {
            hideTooltip();
            return;
          }
          const matrix = svg.getScreenCTM();
          if (!matrix) {
            hideTooltip();
            return;
          }
          const svgPoint = svg.createSVGPoint();
          svgPoint.x = event.clientX;
          svgPoint.y = event.clientY;
          const cursor = svgPoint.matrixTransform(matrix.inverse());
          const point = nearestChartPoint(seriesInfo.points, cursor.x, cursor.y);
          if (!point) {
            hideTooltip();
            return;
          }
          marker.setAttribute('visibility', 'visible');
          marker.setAttribute('cx', String(point.x));
          marker.setAttribute('cy', String(point.y));
          marker.setAttribute('fill', seriesInfo.color);
          tooltip.innerHTML = (
            `<div class="chart-tooltip-title">${escapeHtml(seriesInfo.label)}</div>`
            + `<div><strong>${escapeHtml(point.date || '')}</strong></div>`
            + `<div class="chart-tooltip-meta">Value: ${escapeHtml(formatChartNumber(point.value))}</div>`
          );
          const bodyRect = forecastChartBody.getBoundingClientRect();
          const left = event.clientX - bodyRect.left;
          const top = event.clientY - bodyRect.top;
          tooltip.style.left = `${Math.min(Math.max(left, 8), Math.max(bodyRect.width - 180, 8))}px`;
          tooltip.style.top = `${Math.min(Math.max(top, 34), Math.max(bodyRect.height - 34, 34))}px`;
          tooltip.classList.remove('hidden');
        });
        path.addEventListener('mouseleave', hideTooltip);
      });
      svg.addEventListener('mouseleave', hideTooltip);
    }

    function chartToggleChecked(seriesId) {
      if (!forecastChartBody) return false;
      let found = null;
      forecastChartBody.querySelectorAll('input[name="chart-series-toggle"]').forEach((input) => {
        if (input.getAttribute('data-series-id') === seriesId) found = input;
      });
      return found ? found.checked : false;
    }

    function attachChartControls(context=null) {
      if (!forecastChartBody) return;
      const updateVisibility = () => {
        const lowerVisible = chartToggleChecked('confidence-lower');
        const upperVisible = chartToggleChecked('confidence-upper');
        forecastChartBody.querySelectorAll('[data-chart-series-id]').forEach((element) => {
          const confidenceKind = element.getAttribute('data-confidence-kind') || '';
          const parentId = element.getAttribute('data-chart-parent-id') || '';
          const seriesId = element.getAttribute('data-chart-series-id') || '';
          let visible;
          if (confidenceKind === 'lower') {
            visible = lowerVisible && chartToggleChecked(parentId);
          } else if (confidenceKind === 'upper') {
            visible = upperVisible && chartToggleChecked(parentId);
          } else {
            visible = chartToggleChecked(seriesId);
          }
          element.style.display = visible ? '' : 'none';
        });
      };
      forecastChartBody.querySelectorAll('input[name="chart-series-toggle"]').forEach((input) => {
        input.addEventListener('change', () => {
          const visible = input.checked;
          const item = input.closest('.chart-legend-item');
          if (item) item.classList.toggle('is-off', !visible);
          updateVisibility();
          const marker = forecastChartBody.querySelector('.chart-hover-marker');
          const tooltip = forecastChartBody.querySelector('.chart-tooltip');
          if (marker) marker.setAttribute('visibility', 'hidden');
          if (tooltip) tooltip.classList.add('hidden');
        });
      });
      updateVisibility();
    }

    function chartDataFromSingleCsv(csvText, modelLabel, forecastEndLabel='') {
      const rows = parseForecastCsvRowsForComparison(csvText);
      return {
        labels: rows.map((row) => row.date || ''),
        forecastEndLabel: String(forecastEndLabel || '').trim() || selectedForecastEndLabel(),
        actual: rows.map((row) => parseChartNumber(row.actual)),
        series: [{
          label: modelLabel || 'Forecast',
          values: rows.map((row) => parseChartNumber(row.mean)),
          lower: rows.map((row) => parseChartNumber(row.lower)),
          upper: rows.map((row) => parseChartNumber(row.upper)),
        }],
      };
    }

    function chartDataFromMultiResults(results, forecastEndLabel='') {
      const successful = (Array.isArray(results) ? results : []).filter((result) => result && result.ok && result.forecast_csv);
      const models = uniqueModelLabels(successful);
      const order = [];
      const byDate = new Map();
      successful.forEach((result, index) => {
        const model = models[index];
        const rows = parseForecastCsvRowsForComparison(result.forecast_csv || '');
        rows.forEach((row) => {
          const date = String(row.date || '').trim();
          if (!date) return;
          if (!byDate.has(date)) {
            byDate.set(date, { actual: '', values: {} });
            order.push(date);
          }
          const entry = byDate.get(date);
          if (!entry.actual && row.actual) entry.actual = row.actual;
          entry.values[model] = {
            mean: row.mean || '',
            lower: row.lower || '',
            upper: row.upper || '',
          };
        });
      });
      return {
        labels: order,
        forecastEndLabel: String(forecastEndLabel || '').trim()
          || String(successful[0] && (successful[0].forecast_end_label || successful[0].forecast_end_date) || '').trim()
          || selectedForecastEndLabel(),
        actual: order.map((date) => parseChartNumber(byDate.get(date).actual)),
        series: models.map((model) => ({
          label: model,
          values: order.map((date) => parseChartNumber((byDate.get(date).values[model] || {}).mean)),
          lower: order.map((date) => parseChartNumber((byDate.get(date).values[model] || {}).lower)),
          upper: order.map((date) => parseChartNumber((byDate.get(date).values[model] || {}).upper)),
        })),
      };
    }

    function renderForecastChartEmpty(message) {
      if (!forecastChartBody) return;
      forecastChartBody.className = 'forecast-chart-body forecast-chart-empty';
      forecastChartBody.innerHTML = escapeHtml(message || 'Run a forecast to draw the chart.');
    }

    function renderForecastChart(chartData) {
      if (!forecastChartBody) return;
      const labels = Array.isArray(chartData && chartData.labels) ? chartData.labels : [];
      const actual = Array.isArray(chartData && chartData.actual) ? chartData.actual : [];
      const series = Array.isArray(chartData && chartData.series) ? chartData.series : [];
      const forecastEndLabel = String(chartData && chartData.forecastEndLabel || '').trim();
      const allValues = []
        .concat(actual)
        .concat(series.flatMap((item) => Array.isArray(item.values) ? item.values : []))
        .concat(series.flatMap((item) => Array.isArray(item.lower) ? item.lower : []))
        .concat(series.flatMap((item) => Array.isArray(item.upper) ? item.upper : []))
        .filter((value) => Number.isFinite(value));
      if (!labels.length || !allValues.length) {
        renderForecastChartEmpty('No plottable forecast values were returned.');
        return;
      }

      const width = 1040;
      const height = 430;
      const margin = { top: 24, right: 28, bottom: 52, left: 68 };
      const plotWidth = width - margin.left - margin.right;
      const plotHeight = height - margin.top - margin.bottom;
      const dateValues = labels.map((label) => chartDateMillis(label));
      const forecastEndValue = chartDateMillis(forecastEndLabel);
      const useTimeAxis = dateValues.length === labels.length
        && dateValues.every((value) => Number.isFinite(value))
        && Math.max(...dateValues) > Math.min(...dateValues);
      const axisStartValue = useTimeAxis ? Math.min(...dateValues) : null;
      const axisEndValue = useTimeAxis
        ? Math.max(Math.max(...dateValues), Number.isFinite(forecastEndValue) ? forecastEndValue : Math.max(...dateValues))
        : null;
      let yMin = Math.min(...allValues);
      let yMax = Math.max(...allValues);
      if (yMin === yMax) {
        const pad = Math.max(Math.abs(yMin) * 0.1, 1);
        yMin -= pad;
        yMax += pad;
      } else {
        const pad = (yMax - yMin) * 0.08;
        yMin -= pad;
        yMax += pad;
      }
      const xForIndex = (index) => {
        if (useTimeAxis) {
          return margin.left + ((dateValues[index] - axisStartValue) * plotWidth / (axisEndValue - axisStartValue));
        }
        return labels.length > 1
          ? margin.left + (index * plotWidth / (labels.length - 1))
          : margin.left + (plotWidth / 2);
      };
      const yForValue = (value) => margin.top + ((yMax - value) * plotHeight / (yMax - yMin));

      const yTicks = Array.from({ length: 5 }, (_, index) => yMax - ((yMax - yMin) * index / 4));
      const yGrid = yTicks.map((value) => {
        const y = yForValue(value);
        return (
          `<line class="chart-grid-line" x1="${margin.left}" y1="${y.toFixed(2)}" x2="${width - margin.right}" y2="${y.toFixed(2)}"></line>`
          + `<text class="chart-axis-label" x="${margin.left - 10}" y="${(y + 4).toFixed(2)}" text-anchor="end">${escapeHtml(formatChartNumber(value))}</text>`
        );
      }).join('');
      const targetTickCount = Math.min(6, labels.length);
      let xTickEntries;
      if (useTimeAxis) {
        const forecastEndX = margin.left + ((axisEndValue - axisStartValue) * plotWidth / (axisEndValue - axisStartValue));
        xTickEntries = closestChartTickIndexes(dateValues, targetTickCount)
          .map((index) => ({ x: xForIndex(index), label: labels[index] || '' }))
          .filter((item) => Math.abs(item.x - forecastEndX) > 1);
        xTickEntries.push({
          x: forecastEndX,
          label: forecastEndLabel || formatChartAxisDate(axisEndValue),
          forecastEnd: true,
        });
        xTickEntries.sort((a, b) => a.x - b.x);
      } else {
        xTickEntries = Array.from(new Set(
          Array.from({ length: targetTickCount }, (_, index) => (
            targetTickCount > 1
              ? Math.round((labels.length - 1) * index / (targetTickCount - 1))
              : 0
          ))
        ))
          .filter((index) => index >= 0 && index < labels.length)
          .map((index) => ({ x: xForIndex(index), label: labels[index] || '' }));
      }
      const xTicks = xTickEntries.map((item, tickIndex) => {
        const anchor = tickIndex === 0 ? 'start' : (tickIndex === xTickEntries.length - 1 ? 'end' : 'middle');
        return (
          `<line class="chart-grid-line" x1="${item.x.toFixed(2)}" y1="${margin.top}" x2="${item.x.toFixed(2)}" y2="${height - margin.bottom}"></line>`
          + `<text class="chart-axis-label" x="${item.x.toFixed(2)}" y="${height - 18}" text-anchor="${anchor}">${escapeHtml(item.label)}</text>`
        );
      }).join('');
      const forecastEndMarker = useTimeAxis
        ? (
          `<line class="chart-forecast-end-line" x1="${(width - margin.right).toFixed(2)}" y1="${margin.top}" x2="${(width - margin.right).toFixed(2)}" y2="${height - margin.bottom}"></line>`
          + `<text class="chart-forecast-end-label" x="${(width - margin.right - 8).toFixed(2)}" y="${(margin.top + 16).toFixed(2)}" text-anchor="end">Forecast until</text>`
        )
        : '';
      const actualPath = chartPath(actual, xForIndex, yForValue);
      const hoverSeries = [];
      const actualColor = 'rgba(49, 20, 61, 0.9)';
      const actualMarkup = actualPath
        ? (() => {
            const seriesIndex = hoverSeries.length;
            const seriesId = 'actual';
            hoverSeries.push({
              label: 'Actual',
              color: actualColor,
              points: actual.map((value, index) => (
                Number.isFinite(value)
                  ? { date: labels[index] || '', value, x: xForIndex(index), y: yForValue(value) }
                  : null
              )).filter(Boolean),
            });
            return (
              `<path class="chart-line chart-actual-line" data-chart-series-id="${seriesId}" d="${actualPath}"></path>`
              + `<path class="chart-hover-path" data-chart-series-id="${seriesId}" data-series-index="${seriesIndex}" d="${actualPath}"></path>`
            );
          })()
        : '';
      const modelMarkup = series.map((item, index) => {
        const color = chartPalette[index % chartPalette.length];
        const seriesId = `model-${index}`;
        const values = Array.isArray(item.values) ? item.values : [];
        const path = chartPath(values, xForIndex, yForValue);
        const lastIndex = lastFiniteIndex(values);
        const marker = lastIndex >= 0
          ? `<circle class="chart-point" data-chart-series-id="${seriesId}" cx="${xForIndex(lastIndex).toFixed(2)}" cy="${yForValue(values[lastIndex]).toFixed(2)}" r="4.4" fill="${color}"></circle>`
          : '';
        if (!path) return '';
        const seriesIndex = hoverSeries.length;
        hoverSeries.push({
          label: item.label || `Forecast ${index + 1}`,
          color,
          points: values.map((value, valueIndex) => (
            Number.isFinite(value)
              ? { date: labels[valueIndex] || '', value, x: xForIndex(valueIndex), y: yForValue(value) }
              : null
          )).filter(Boolean),
        });
        return (
          `<path class="chart-line chart-model-line" data-chart-series-id="${seriesId}" style="stroke:${color}" d="${path}"></path>${marker}`
          + `<path class="chart-hover-path" data-chart-series-id="${seriesId}" data-series-index="${seriesIndex}" d="${path}"></path>`
        );
      }).join('');
      const confidenceMarkup = series.map((item, index) => {
        const color = chartPalette[index % chartPalette.length];
        const parentId = `model-${index}`;
        const label = item.label || `Forecast ${index + 1}`;
        const parts = [];
        [
          { kind: 'lower', label: `${label} lower confidence`, values: Array.isArray(item.lower) ? item.lower : [], className: 'chart-confidence-line' },
          { kind: 'upper', label: `${label} upper confidence`, values: Array.isArray(item.upper) ? item.upper : [], className: 'chart-confidence-line chart-confidence-upper' },
        ].forEach((band) => {
          const path = chartPath(band.values, xForIndex, yForValue);
          if (!path) return;
          const seriesIndex = hoverSeries.length;
          hoverSeries.push({
            label: band.label,
            color,
            points: band.values.map((value, valueIndex) => (
              Number.isFinite(value)
                ? { date: labels[valueIndex] || '', value, x: xForIndex(valueIndex), y: yForValue(value) }
                : null
            )).filter(Boolean),
          });
          parts.push(
            `<path class="chart-line ${band.className}" data-chart-series-id="confidence-${band.kind}" data-confidence-kind="${band.kind}" data-chart-parent-id="${parentId}" style="stroke:${color};display:none;" d="${path}"></path>`
            + `<path class="chart-hover-path" data-chart-series-id="confidence-${band.kind}" data-confidence-kind="${band.kind}" data-chart-parent-id="${parentId}" data-series-index="${seriesIndex}" style="display:none;" d="${path}"></path>`
          );
        });
        return parts.join('');
      }).join('');
      const legendItems = [{ id: 'actual', label: 'Actual', color: actualColor, checked: true }]
        .concat(series.map((item, index) => ({
          id: `model-${index}`,
          label: item.label || `Forecast ${index + 1}`,
          color: chartPalette[index % chartPalette.length],
          checked: true,
        })))
        .concat([
          { id: 'confidence-lower', label: 'Lower confidence', color: chartLowerColor, checked: false },
          { id: 'confidence-upper', label: 'Upper confidence', color: chartUpperColor, checked: false },
        ]);
      const legend = (
        '<div class="chart-legend">'
        + '<span class="chart-legend-title">Legend</span>'
        + legendItems.map((item) => (
          `<label class="chart-legend-item${item.checked ? '' : ' is-off'}">`
          + `<input type="checkbox" name="chart-series-toggle" data-series-id="${escapeHtml(item.id)}"${item.checked ? ' checked' : ''}>`
          + `<span class="chart-legend-swatch" style="--swatch:${item.color}"></span>${escapeHtml(item.label)}`
          + '</label>'
        )).join('')
        + '</div>'
      );
      forecastChartBody.className = 'forecast-chart-body';
      forecastChartBody.innerHTML = (
        legend
        + `<svg class="forecast-chart-svg" viewBox="0 0 ${width} ${height}" role="img" aria-label="Actual and forecast chart">`
        + `<rect x="0" y="0" width="${width}" height="${height}" fill="rgba(255,255,255,0.42)"></rect>`
        + yGrid
        + xTicks
        + `<line class="chart-axis" x1="${margin.left}" y1="${height - margin.bottom}" x2="${width - margin.right}" y2="${height - margin.bottom}"></line>`
        + `<line class="chart-axis" x1="${margin.left}" y1="${margin.top}" x2="${margin.left}" y2="${height - margin.bottom}"></line>`
        + forecastEndMarker
        + confidenceMarkup
        + modelMarkup
        + actualMarkup
        + '<circle class="chart-hover-marker" r="5.5" visibility="hidden"></circle>'
        + '</svg>'
        + '<div class="chart-tooltip hidden"></div>'
      );
      attachChartTooltip(hoverSeries);
      attachChartControls();
    }

    function renderSingleForecastChart(csvText, modelLabel, forecastEndLabel='') {
      renderForecastChart(chartDataFromSingleCsv(csvText, modelLabel, forecastEndLabel));
    }

    function renderMultiForecastChart(results, forecastEndLabel='') {
      renderForecastChart(chartDataFromMultiResults(results, forecastEndLabel));
    }

    function uniqueModelLabels(results) {
      const counts = new Map();
      return (Array.isArray(results) ? results : []).map((result, index) => {
        const raw = String(result && result.model || `Forecast ${index + 1}`).trim() || `Forecast ${index + 1}`;
        const seen = counts.get(raw) || 0;
        counts.set(raw, seen + 1);
        return seen ? `${raw} ${seen + 1}` : raw;
      });
    }

    function comparisonTableMarkup(results) {
      const successful = (Array.isArray(results) ? results : []).filter((result) => result && result.ok && result.forecast_csv);
      if (!successful.length) {
        return (
          '<div class="table-wrap comparison-table-wrap">'
          + '<div class="table-caption">No forecast rows returned for the selected models.</div>'
          + '<div class="table-scroll"><table><thead><tr><th>Date</th><th>Actual</th></tr></thead><tbody><tr><td colspan="2">No comparison rows returned.</td></tr></tbody></table></div></div>'
        );
      }
      const models = uniqueModelLabels(successful);
      const order = [];
      const byDate = new Map();
      successful.forEach((result, index) => {
        const model = models[index];
        const rows = parseForecastCsvRowsForComparison(result.forecast_csv || '');
        rows.forEach((row) => {
          const date = String(row.date || '').trim();
          if (!date) return;
          if (!byDate.has(date)) {
            byDate.set(date, { date, actual: '', values: {} });
            order.push(date);
          }
          const entry = byDate.get(date);
          if (!entry.actual && row.actual) entry.actual = row.actual;
          entry.values[model] = {
            mean: String(row.mean || '').trim(),
            stderr: String(row.stderr || '').trim(),
          };
        });
      });
      const head = (
        '<tr><th>Date</th><th>Actual</th>'
        + models.map((model) => `<th>${escapeHtml(model)} mean</th><th>${escapeHtml(model)} stderr</th>`).join('')
        + '</tr>'
      );
      const body = order.length
        ? order.map((date) => {
            const entry = byDate.get(date);
            return (
              '<tr>'
              + `<td>${escapeHtml(date)}</td>`
              + `<td>${escapeHtml(entry.actual || '')}</td>`
              + models.map((model) => {
                  const cell = entry.values[model] || { mean: '', stderr: '' };
                  return `<td>${escapeHtml(cell.mean || '')}</td><td>${escapeHtml(cell.stderr || '')}</td>`;
                }).join('')
              + '</tr>'
            );
          }).join('')
        : `<tr><td colspan="${2 + (models.length * 2)}">No comparison rows returned.</td></tr>`;
      return (
        '<div class="table-wrap comparison-table-wrap">'
        + '<div class="table-caption">Actual vs forecast comparison across the selected models. Lower and upper bands are left out here so the model means and standard errors are easier to compare side by side.</div>'
        + `<div class="table-scroll"><table><thead>${head}</thead><tbody>${body}</tbody></table></div></div>`
      );
    }

    function renderComparisonResults(data) {
      const results = Array.isArray(data.results) ? data.results : [];
      if (!results.length) {
        return '<div class="summary-line note-line">No comparison results returned.</div>';
      }
      const structuredSummary = String(data.summary_html || '').trim();
      if (structuredSummary) {
        return (
          '<div class="comparison-stack">'
          + structuredSummary
          + comparisonTableMarkup(results)
          + '</div>'
        );
      }
      return (
        '<div class="comparison-stack">'
        + results.map((result) => {
          const model = escapeHtml(result.model || 'Forecast');
          const note = `Target modelled: ${result.target_value_column_used || '(none)'} | Drivers used: ${result.xreg_columns_used || '(none)'}`;
          const fitRows = escapeHtml(String(result.fit_rows ?? '-'));
          const stable = result.stationary ? 'Yes' : 'No';
          const invertible = result.invertible ? 'Yes' : 'No';
          const status = result.ok ? '' : ' rating-poor';
          const summary = result.ok
            ? (result.summary_html || '<div class="summary-line note-line">No summary returned.</div>')
            : `<div class="summary-overall rating-poor"><h4>${model}</h4><p>${escapeHtml(result.error || 'Run failed.')}</p></div>`;
          return (
            `<section class="comparison-run${status}">`
            + `<div class="comparison-run-head"><h4>${model}</h4></div>`
            + `<p class="summary-note">${escapeHtml(note)}</p>`
            + '<div class="comparison-metrics">'
            + `<div class="comparison-metric"><span>Historic rows</span><strong>${fitRows}</strong></div>`
            + `<div class="comparison-metric"><span>Stationary</span><strong>${stable}</strong></div>`
            + `<div class="comparison-metric"><span>Invertible</span><strong>${invertible}</strong></div>`
            + `<div class="comparison-metric"><span>Run status</span><strong>${result.ok ? 'OK' : 'Failed'}</strong></div>`
            + '</div>'
            + summary
            + '</section>'
          );
        }).join('')
        + comparisonTableMarkup(results)
        + '</div>'
      );
    }

    function activateComparisonTab(button) {
      const tabs = button && button.closest ? button.closest('.comparison-tabs') : null;
      if (!tabs) return;
      const targetId = button.getAttribute('data-tab-target') || '';
      tabs.querySelectorAll('.comparison-tab').forEach((tab) => {
        const active = tab === button;
        tab.classList.toggle('is-active', active);
        tab.setAttribute('aria-selected', active ? 'true' : 'false');
      });
      tabs.querySelectorAll('.comparison-tab-panel').forEach((panel) => {
        panel.classList.toggle('is-active', panel.id === targetId);
      });
    }

    if (summaryBox) {
      summaryBox.addEventListener('click', (event) => {
        const target = event.target;
        const button = target && target.closest ? target.closest('.comparison-tab') : null;
        if (!button || !summaryBox.contains(button)) return;
        event.preventDefault();
        activateComparisonTab(button);
      });
    }

    function applyForecastResult(data) {
      if (runButton) runButton.disabled = false;
      if (!data || !data.ok) {
        renderForecastChartEmpty('Run a forecast successfully to draw the chart.');
        setStatus((data && data.error) || 'Forecast failed.', 'error');
        return;
      }
      latestSummary = data.summary_text || '';
      latestForecastCsv = data.forecast_csv || '';
      window.__opheliaLatestSummary = latestSummary;
      window.__opheliaLatestForecastCsv = latestForecastCsv;
      if (data.multi) {
        renderMultiForecastChart(data.results || [], data.forecast_end_label || data.forecast_end_date || '');
        if (metricModel) metricModel.textContent = `${(data.results || []).length} models`;
        if (metricFitRows) metricFitRows.textContent = 'Varies';
        if (metricStationary) metricStationary.textContent = 'Mixed';
        if (metricInvertible) metricInvertible.textContent = 'Mixed';
        if (summaryBox) summaryBox.innerHTML = renderComparisonResults(data);
        if (forecastTableWrap) forecastTableWrap.classList.add('hidden');
        if (driversUsedNote) {
          driversUsedNote.textContent = `Target modelled in this run: ${data.target_value_column_used || '(none)'} | Drivers used in this run: ${data.xreg_columns_used || '(none)'} | Downloads contain the combined multi-model summary and comparison CSV.`;
        }
      } else {
        renderSingleForecastChart(latestForecastCsv, data.model || 'Forecast', data.forecast_end_label || data.forecast_end_date || '');
        if (metricModel) metricModel.textContent = data.model || 'Forecast';
        if (metricFitRows) metricFitRows.textContent = String(data.fit_rows ?? '-');
        if (metricStationary) metricStationary.textContent = data.stationary ? 'Yes' : 'No';
        if (metricInvertible) metricInvertible.textContent = data.invertible ? 'Yes' : 'No';
        if (summaryBox) {
          if (data.summary_html) summaryBox.innerHTML = data.summary_html;
          else renderSummaryText(latestSummary);
        }
        renderForecastTable(latestForecastCsv);
        if (forecastTableWrap) forecastTableWrap.classList.remove('hidden');
        if (driversUsedNote) {
          driversUsedNote.textContent = `Target modelled in this run: ${data.target_value_column_used || '(none)'} | Drivers used in this run: ${data.xreg_columns_used || '(none)'}`;
        }
      }
      if (downloadSummary) {
        downloadSummary.classList.toggle('disabled', !latestSummary);
        downloadSummary.setAttribute('aria-disabled', latestSummary ? 'false' : 'true');
      }
      if (downloadForecast) {
        downloadForecast.classList.toggle('disabled', !latestForecastCsv);
        downloadForecast.setAttribute('aria-disabled', latestForecastCsv ? 'false' : 'true');
      }
      setStatus('Forecast complete.', 'ok');
    }

    window.__opheliaApplyForecastResult = applyForecastResult;

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

    function prepareForecastSubmit() {
      try {
        if (!form) {
          throw new Error('Forecast form not found.');
        }
        syncXregColumnsFromChecks();
        syncOutlierDatesFromChecks();
        const payload = collectState();
        for (const [key, value] of Object.entries(payload)) {
          const field = form.elements.namedItem(key);
          if (!field || !('value' in field)) continue;
          field.value = value;
        }
        if (driversUsedNote) {
          driversUsedNote.textContent = `Target selected for this run: ${payload.target_value_column || '(none)'} | Models selected for this run: ${payload.models || payload.model || '(none)'} | Drivers selected for this run: ${payload.xreg_columns || '(none)'}`;
        }
        runButton.disabled = true;
        setStatus('Submitting forecast...', '');
        saveState().catch(() => {});
        return true;
      } catch (error) {
        setStatus(error.message || String(error), 'error');
        runButton.disabled = false;
        return false;
      }
    }

    function runForecast() {
      if (!prepareForecastSubmit()) return false;
      if (typeof form.requestSubmit === 'function') {
        form.requestSubmit(runButton);
      } else {
        form.submit();
      }
      return true;
    }

    window.__opheliaPrepareForecastSubmit = prepareForecastSubmit;
    window.__opheliaUiRunForecast = runForecast;

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

    form.addEventListener('input', scheduleStateSave);
    form.addEventListener('change', scheduleStateSave);

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
      const chosen = String(targetValueSelect.value || '').trim();
      syncTargetValueTitle();
      targetPickerStatus.textContent = chosen
        ? `Chosen target: ${chosen}`
        : 'Choose the series you want to forecast.';
      loadExistingTargetColumns(chosen);
      scheduleStateSave();
    });
    forecastEndInput.addEventListener('change', () => {
      saveState().catch(() => {});
    });
    outlierModeSelect.addEventListener('change', () => {
      scheduleStateSave();
    });
    ['p', 'd', 'q', 'P', 'D', 'Q'].forEach((name) => {
      const field = form.elements.namedItem(name);
      if (!field) return;
      field.addEventListener('input', () => {
        modelOrderEdited[name] = true;
        updateSeasonalSetupWarning();
        scheduleStateSave();
      });
    });
    form.elements.namedItem('season_period').addEventListener('change', () => {
      seasonPeriodManuallyEdited = true;
      updateSeasonalSetupWarning();
      scheduleStateSave();
    });
    modelList.addEventListener('change', (event) => {
      const input = event.target;
      if (!(input instanceof HTMLInputElement) || input.name !== 'model-choice') {
        return;
      }
      syncModelsFromChecks();
      applySuggestedModelSettings(false);
      updateSeasonalSetupWarning();
      saveState().catch(() => {});
    });
    outlierList.addEventListener('change', (event) => {
      const input = event.target;
      if (!(input instanceof HTMLInputElement) || input.name !== 'outlier-choice') {
        return;
      }
      syncOutlierDatesFromChecks();
      scheduleStateSave();
    });
    xregColumnsList.addEventListener('change', (event) => {
      const input = event.target;
      if (!(input instanceof HTMLInputElement) || input.name !== 'xreg-column-choice') {
        return;
      }
      syncXregColumnsFromChecks();
      saveState().catch(() => {});
    });
    form.elements.namedItem('target_date_column').addEventListener('change', () => {
      loadExistingTargetColumns();
      scheduleStateSave();
    });
    form.elements.namedItem('xreg_date_column').addEventListener('change', () => {
      loadExistingXregColumns();
      scheduleStateSave();
    });

    suggestButton.addEventListener('click', () => {
      Object.keys(modelOrderEdited).forEach((key) => {
        modelOrderEdited[key] = false;
      });
      seasonPeriodManuallyEdited = false;
      applySuggestedModelSettings(true);
      scheduleStateSave();
      setStatus('Suggested settings applied.', 'ok');
    });

    helpToggle.addEventListener('click', () => {
      if (helpPane.classList.contains('hidden')) showHelp();
      else showResults();
    });

    chartToggle.addEventListener('click', () => {
      if (chartPane.classList.contains('hidden')) showChart();
      else showResults();
    });

    refreshMobile.addEventListener('click', copyMobileUrl);
    funnelToggle.addEventListener('click', toggleFunnel);
    applyState(defaults);
    renderModelChecksFromState(
      form.elements.namedItem('models').value
    );
    syncModelsFromChecks();
    syncTargetUploadName(
      form.elements.namedItem('target_path').value,
      form.elements.namedItem('target_display_name').value
    );
    syncXregUploadName(
      form.elements.namedItem('xreg_path').value,
      form.elements.namedItem('xreg_display_name').value
    );
    applyTargetMeta(initialTargetMeta);
    applyXregMeta(initialXregMeta);
    syncTargetValueTitle();
    renderTargetColumnPicker(initialTargetColumns, form.elements.namedItem('target_value_column').value);
    renderXregColumnsPicker(initialXregColumns, xregColumnsInput.value);
    renderSeasonPeriodOptions(form.elements.namedItem('frequency').value, form.elements.namedItem('season_period').value);
    updateSeasonalSetupWarning();
    if (!initialTargetColumns.length) loadExistingTargetColumns();
    if (!initialXregColumns.length) loadExistingXregColumns();
    form.elements.namedItem('models').value = '__MODELS__';
    renderModelChecksFromState(
      form.elements.namedItem('models').value
    );
    syncModelsFromChecks();
    form.elements.namedItem('frequency').value = '__FREQUENCY__';
    form.elements.namedItem('year_type').value = '__YEAR_TYPE__';
    form.elements.namedItem('criterion').value = '__CRITERION__';
    initialHydrating = false;
    applySuggestedModelSettings(false);
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
        return normalize_state_models(state)
    if isinstance(data, dict):
        for key, value in data.items():
            if key in state:
                state[key] = value
    return normalize_state_models(state)


def save_state(update: dict[str, object]) -> None:
    state = load_state()
    for key in state:
        if key in update:
            state[key] = update[key]
    normalize_state_models(state)
    persisted = {key: value for key, value in state.items() if key not in {"model", "horizon"}}
    try:
        STATE_FILE.write_text(json.dumps(persisted, indent=2) + "\n", encoding="utf-8")
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


def parse_date_text(text: object) -> dt.date | None:
    raw = str(text or "").strip()
    if not raw:
        return None
    for fmt in ("%d/%m/%Y", "%Y-%m-%d", "%d-%m-%Y", "%d/%m/%y", "%Y/%m/%d"):
        try:
            return dt.datetime.strptime(raw, fmt).date()
        except ValueError:
            continue
    return None


def parse_float_text(text: object) -> float | None:
    raw = str(text or "").strip()
    if not raw:
        return None
    try:
        return float(raw)
    except ValueError:
        return None


def format_date_uk(value: dt.date | None) -> str:
    return value.strftime("%d/%m/%Y") if value else ""


def format_date_iso(value: dt.date | None) -> str:
    return value.isoformat() if value else ""


def add_months(value: dt.date, months: int) -> dt.date:
    month_index = (value.year * 12 + (value.month - 1)) + months
    year = month_index // 12
    month = (month_index % 12) + 1
    day = min(value.day, calendar.monthrange(year, month)[1])
    return dt.date(year, month, day)


def add_period(value: dt.date, frequency: str) -> dt.date:
    if frequency == "daily":
        return value + dt.timedelta(days=1)
    if frequency == "monthly":
        return add_months(value, 1)
    if frequency == "quarterly":
        return add_months(value, 3)
    if frequency == "yearly":
        return add_months(value, 12)
    return value


def period_end_for_date(value: dt.date, frequency: str, year_type: str) -> dt.date:
    if frequency == "daily":
        return value
    if frequency == "monthly":
        return dt.date(value.year, value.month, calendar.monthrange(value.year, value.month)[1])
    if frequency == "quarterly":
        quarter_ends = [3, 6, 9, 12] if year_type != "fiscal" else [6, 9, 12, 3]
        for month in quarter_ends:
            year = value.year
            if year_type == "fiscal" and month == 3 and value.month >= 4:
                year = value.year + 1
            candidate = dt.date(year, month, calendar.monthrange(year, month)[1])
            start_month = month - 2 if month > 3 else (month + 9 if year_type == "fiscal" else 1)
            if candidate >= value:
                return candidate
        return dt.date(value.year, 12, 31)
    if frequency == "yearly":
        if year_type == "fiscal":
            year = value.year if value.month <= 3 else value.year + 1
            return dt.date(year, 3, 31)
        return dt.date(value.year, 12, 31)
    return value


def infer_frequency_from_dates(values: list[dt.date]) -> str:
    dates = sorted(set(values))
    if len(dates) < 2:
        return "unknown"
    day_steps = [(dates[i + 1] - dates[i]).days for i in range(len(dates) - 1)]
    month_steps = [((dates[i + 1].year - dates[i].year) * 12) + (dates[i + 1].month - dates[i].month)
                   for i in range(len(dates) - 1)]
    if all(step == 1 for step in day_steps):
        return "daily"
    if all(step == 1 for step in month_steps):
        return "monthly"
    if all(step == 3 for step in month_steps):
        return "quarterly"
    if all(step == 12 for step in month_steps):
        return "yearly"
    return "unknown"


def frequency_label(frequency: str) -> str:
    return {
        "daily": "Daily",
        "monthly": "Monthly",
        "quarterly": "Quarterly",
        "yearly": "Yearly",
        "unknown": "Unknown",
    }.get(str(frequency or "").strip().lower(), "Unknown")


def suggested_forecast_end(target_meta: dict[str, object],
                           xreg_meta: dict[str, object] | None,
                           year_type: str,
                           current_value: object = "") -> str:
    current = parse_date_text(current_value)
    target_end = parse_date_text(target_meta.get("usable_end_date_iso", "")) or parse_date_text(target_meta.get("end_date_iso", ""))
    target_freq = str(target_meta.get("detected_frequency", "unknown"))
    if current:
        return format_date_iso(period_end_for_date(current, target_freq, year_type))
    if not target_end or target_freq == "unknown":
        return ""
    candidate = target_end
    for _ in range(6):
        candidate = add_period(candidate, target_freq)
    candidate = period_end_for_date(candidate, target_freq, year_type)
    xreg_end = parse_date_text((xreg_meta or {}).get("usable_end_date_iso", "")) or parse_date_text((xreg_meta or {}).get("end_date_iso", ""))
    if xreg_end:
        xreg_limit = period_end_for_date(xreg_end, target_freq, year_type)
        if xreg_limit > target_end and xreg_limit < candidate:
            candidate = xreg_limit
    return format_date_iso(candidate)


def last_possible_forecast_end(target_meta: dict[str, object],
                               xreg_meta: dict[str, object] | None,
                               year_type: str) -> dt.date | None:
    target_end = parse_date_text(target_meta.get("usable_end_date_iso", "")) or parse_date_text(target_meta.get("end_date_iso", ""))
    if not target_end:
        return None
    target_freq = str(target_meta.get("detected_frequency", "unknown"))
    if target_freq == "unknown":
        return None
    xreg_path = resolve_lab_path((xreg_meta or {}).get("path", ""))
    xreg_date_column = str((xreg_meta or {}).get("date_column", "")).strip() or None
    selected_columns = [item.strip() for item in str((xreg_meta or {}).get("selected_columns_text", "") or "").split(",") if item.strip()]
    if xreg_path and xreg_path.exists() and selected_columns:
        future_dates = usable_future_period_ends(
            xreg_path,
            xreg_date_column,
            selected_columns,
            True,
            target_end,
            target_freq,
            year_type,
        )
        if future_dates:
            return future_dates[-1]
    xreg_end = parse_date_text((xreg_meta or {}).get("usable_end_date_iso", "")) or parse_date_text((xreg_meta or {}).get("end_date_iso", ""))
    if xreg_end:
        xreg_limit = period_end_for_date(xreg_end, target_freq, year_type)
        if xreg_limit > target_end:
            return xreg_limit
    candidate = target_end
    for _ in range(24):
        candidate = add_period(candidate, target_freq)
    return period_end_for_date(candidate, target_freq, year_type)


def forecast_end_options(target_meta: dict[str, object],
                         xreg_meta: dict[str, object] | None,
                         year_type: str,
                         current_value: object = "") -> list[tuple[str, str]]:
    target_end = parse_date_text(target_meta.get("usable_end_date_iso", "")) or parse_date_text(target_meta.get("end_date_iso", ""))
    target_freq = str(target_meta.get("detected_frequency", "unknown"))
    end_limit = last_possible_forecast_end(target_meta, xreg_meta, year_type)
    if not target_end or not end_limit or target_freq == "unknown" or end_limit <= target_end:
        return []
    options: list[tuple[str, str]] = []
    using_actual_future_dates = False
    xreg_path = resolve_lab_path((xreg_meta or {}).get("path", ""))
    xreg_date_column = str((xreg_meta or {}).get("date_column", "")).strip() or None
    selected_columns = [item.strip() for item in str((xreg_meta or {}).get("selected_columns_text", "") or "").split(",") if item.strip()]
    if xreg_path and xreg_path.exists() and selected_columns:
        future_dates = usable_future_period_ends(
            xreg_path,
            xreg_date_column,
            selected_columns,
            True,
            target_end,
            target_freq,
            year_type,
        )
        options = [(format_date_iso(item), format_date_uk(item)) for item in future_dates[:1200] if item <= end_limit]
        using_actual_future_dates = True
    else:
        cursor = period_end_for_date(add_period(target_end, target_freq), target_freq, year_type)
        while cursor <= end_limit and len(options) < 1200:
            options.append((format_date_iso(cursor), format_date_uk(cursor)))
            cursor = period_end_for_date(add_period(cursor, target_freq), target_freq, year_type)
    current = str(current_value or "").strip()
    if current and current not in {value for value, _ in options} and not using_actual_future_dates:
        parsed = parse_date_text(current)
        if parsed and parsed > target_end and parsed <= end_limit:
            normalized = period_end_for_date(parsed, target_freq, year_type)
            iso = format_date_iso(normalized)
            if iso not in {value for value, _ in options}:
                options.append((iso, format_date_uk(normalized)))
                options.sort(key=lambda item: item[0])
    return options


def periods_between(start: dt.date, end: dt.date, frequency: str, year_type: str) -> int:
    if end <= start:
        return 0
    count = 0
    cursor = start
    normalized_end = period_end_for_date(end, frequency, year_type)
    while cursor < normalized_end and count < 100000:
        cursor = period_end_for_date(add_period(cursor, frequency), frequency, year_type)
        count += 1
    return count if cursor >= normalized_end else 0


def existing_details(path_text: object, date_column: object = "") -> dict[str, object]:
    path = resolve_lab_path(path_text)
    if not path or not path.exists():
        return {}
    try:
        return csv_header_details(path, str(date_column or "").strip() or None)
    except Exception:
        return {}


def detect_date_candidates(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        raw_columns = [str(item).strip() for item in header]
        if not any(raw_columns):
            return []
        sample_rows = []
        for _, row in zip(range(24), reader):
            sample_rows.append(row)
    scored: list[tuple[int, int, str]] = []
    for idx, name in enumerate(raw_columns):
        if not name:
            continue
        seen = 0
        parsed = 0
        for row in sample_rows:
            if idx >= len(row):
                continue
            text = str(row[idx]).strip()
            if not text:
                continue
            seen += 1
            if parse_date_text(text):
                parsed += 1
        header_hint = 1 if any(token in name.lower() for token in ("date", "month", "period", "time", "year")) else 0
        if parsed > 0 and (seen == 0 or parsed / max(seen, 1) >= 0.6 or header_hint):
            scored.append((header_hint, parsed, name))
    scored.sort(key=lambda item: (-item[0], -item[1], item[2].lower()))
    names = [name for _, _, name in scored]
    if raw_columns and raw_columns[0] and raw_columns[0] not in names:
        first_samples = 0
        first_parsed = 0
        for row in sample_rows:
            if not row:
                continue
            text = str(row[0]).strip()
            if not text:
                continue
            first_samples += 1
            if parse_date_text(text):
                first_parsed += 1
        if first_parsed and (first_samples == 0 or first_parsed / max(first_samples, 1) >= 0.6):
            names.insert(0, raw_columns[0])
    return names


def series_date_metadata(path: Path, date_column: str | None = None) -> dict[str, object]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        raw_columns = [str(item).strip() for item in header]
        if not any(raw_columns):
            raise ValueError("The CSV does not have a readable header row.")
        date_candidates = detect_date_candidates(path)
        requested_date = str(date_column or "").strip()
        if requested_date and requested_date in raw_columns:
            date_index = raw_columns.index(requested_date)
        elif date_candidates:
            date_index = raw_columns.index(date_candidates[0])
        else:
            date_index = 0
        values: list[dt.date] = []
        for row in reader:
            if date_index >= len(row):
                continue
            parsed = parse_date_text(row[date_index])
            if parsed:
                values.append(parsed)
    start = min(values) if values else None
    end = max(values) if values else None
    detected = infer_frequency_from_dates(values)
    date_name = raw_columns[date_index] if raw_columns else ""
    value_columns = [column for idx, column in enumerate(raw_columns) if idx != date_index and column]
    return {
        "header": raw_columns,
        "date_column": date_name,
        "date_candidates": date_candidates,
        "date_column_in_first_position": date_index == 0,
        "value_columns": value_columns,
        "detected_frequency": detected,
        "detected_frequency_label": frequency_label(detected),
        "start_date": format_date_uk(start),
        "end_date": format_date_uk(end),
        "start_date_iso": format_date_iso(start),
        "end_date_iso": format_date_iso(end),
    }


def median(values: list[float]) -> float:
    ordered = sorted(values)
    n = len(ordered)
    if n == 0:
        return 0.0
    mid = n // 2
    if n % 2:
        return ordered[mid]
    return (ordered[mid - 1] + ordered[mid]) / 2.0


def pearson_correlation(x_values: list[float], y_values: list[float]) -> float:
    count = min(len(x_values), len(y_values))
    if count < 3:
        return 0.0
    x_mean = sum(x_values[:count]) / count
    y_mean = sum(y_values[:count]) / count
    sum_xy = 0.0
    sum_xx = 0.0
    sum_yy = 0.0
    for idx in range(count):
        dx = x_values[idx] - x_mean
        dy = y_values[idx] - y_mean
        sum_xy += dx * dy
        sum_xx += dx * dx
        sum_yy += dy * dy
    if sum_xx <= 0.0 or sum_yy <= 0.0:
        return 0.0
    return sum_xy / math.sqrt(sum_xx * sum_yy)


def seasonality_candidates_for_frequency(frequency: str) -> list[tuple[int, str]]:
    token = str(frequency or "").strip().lower()
    if token == "monthly":
        return [
            (3, "quarterly"),
            (4, "four-monthly"),
            (6, "bi-annual"),
            (12, "annual"),
        ]
    if token == "quarterly":
        return [
            (2, "bi-annual"),
            (4, "annual"),
        ]
    if token == "yearly":
        return [
            (2, "two-year"),
            (3, "three-year"),
        ]
    if token == "daily":
        return [
            (7, "weekly"),
            (14, "fortnightly"),
            (30, "monthly"),
            (365, "annual"),
        ]
    return []


def seasonality_strength_label(score: float) -> str:
    value = abs(score)
    if value >= 0.75:
        return "very strong"
    if value >= 0.55:
        return "strong"
    if value >= 0.35:
        return "moderate"
    if value >= 0.20:
        return "weak"
    return "none"


def detect_series_seasonality(path: Path,
                              date_column: str | None,
                              value_column: str | None,
                              detected_frequency: str) -> dict[str, object]:
    value_name = str(value_column or "").strip()
    if not value_name:
        return {"seasonality_label": "No seasonality detected.", "seasonality_score": 0.0}

    meta = series_date_metadata(path, date_column)
    if value_name not in meta.get("value_columns", []):
        return {"seasonality_label": "No seasonality detected.", "seasonality_score": 0.0}

    candidates = seasonality_candidates_for_frequency(detected_frequency)
    if not candidates:
        return {"seasonality_label": "No seasonality detected.", "seasonality_score": 0.0}

    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        raw_columns = [str(item).strip() for item in header]
        date_name = str(meta.get("date_column", "")).strip()
        date_index = raw_columns.index(date_name) if date_name in raw_columns else 0
        value_index = raw_columns.index(value_name)
        rows: list[tuple[dt.date, float]] = []
        for row in reader:
            if date_index >= len(row) or value_index >= len(row):
                continue
            date_value = parse_date_text(row[date_index])
            value = parse_float_text(row[value_index])
            if date_value is None or value is None:
                continue
            rows.append((date_value, value))

    if len(rows) < 6:
        return {"seasonality_label": "No seasonality detected.", "seasonality_score": 0.0}

    rows.sort(key=lambda item: item[0])
    values = [value for _, value in rows]
    best_name = ""
    best_score = 0.0
    best_lag = 0
    for lag, name in candidates:
        if lag <= 0 or len(values) < (lag * 2) + 2:
            continue
        lead = values[lag:]
        trail = values[:-lag]
        score = pearson_correlation(lead, trail)
        if score > best_score:
            best_score = score
            best_name = name
            best_lag = lag

    strength = seasonality_strength_label(best_score)
    if strength == "none" or not best_name:
        return {
            "seasonality_label": "No seasonality detected.",
            "seasonality_score": round(best_score, 3),
            "seasonality_pattern": "",
            "seasonality_strength": "none",
            "seasonality_lag": best_lag,
        }
    return {
        "seasonality_label": f"{strength.capitalize()} {best_name} seasonality detected.",
        "seasonality_score": round(best_score, 3),
        "seasonality_pattern": best_name,
        "seasonality_strength": strength,
        "seasonality_lag": best_lag,
    }


def detect_series_outliers(path: Path,
                           date_column: str | None,
                           value_column: str | None,
                           threshold: float = 3.5) -> list[dict[str, object]]:
    meta = series_date_metadata(path, date_column)
    value_name = str(value_column or "").strip()
    if not value_name or value_name not in meta.get("value_columns", []):
        return []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        raw_columns = [str(item).strip() for item in header]
        date_name = str(meta.get("date_column", "")).strip()
        date_index = raw_columns.index(date_name) if date_name in raw_columns else 0
        value_index = raw_columns.index(value_name)
        rows: list[tuple[dt.date, float]] = []
        for row in reader:
            if date_index >= len(row) or value_index >= len(row):
                continue
            date_value = parse_date_text(row[date_index])
            value = parse_float_text(row[value_index])
            if date_value is None or value is None:
                continue
            rows.append((date_value, value))
    values = [value for _, value in rows]
    if len(values) < 5:
        return []
    med = median(values)
    deviations = [abs(value - med) for value in values]
    mad = median(deviations)
    if mad <= 0.0:
        return []
    outliers: list[dict[str, object]] = []
    for date_value, value in rows:
        modified_z = 0.6745 * (value - med) / mad
        if abs(modified_z) >= threshold:
            outliers.append({
                "date": format_date_uk(date_value),
                "date_iso": format_date_iso(date_value),
                "value": value,
                "score": round(modified_z, 2),
            })
    return outliers


def outlier_likelihood_label(score: object) -> str:
    try:
        value = abs(float(score))
    except (TypeError, ValueError):
        return "possibly"
    if value >= 20.0:
        return "extremely likely"
    if value >= 10.0:
        return "very likely"
    if value >= 6.0:
        return "likely"
    return "possibly"


def usable_date_range(path: Path,
                      date_column: str | None,
                      value_columns: list[str],
                      require_all: bool = True) -> tuple[dt.date | None, dt.date | None]:
    if not value_columns:
        return None, None
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        raw_columns = [str(item).strip() for item in header]
        date_name = str(date_column or "").strip()
        date_candidates = detect_date_candidates(path)
        if date_name and date_name in raw_columns:
            date_index = raw_columns.index(date_name)
        elif date_candidates and date_candidates[0] in raw_columns:
            date_index = raw_columns.index(date_candidates[0])
        else:
            date_index = 0
        indexes = [raw_columns.index(name) for name in value_columns if name in raw_columns]
        if not indexes:
            return None, None
        usable_dates: list[dt.date] = []
        for row in reader:
            if date_index >= len(row):
                continue
            date_value = parse_date_text(row[date_index])
            if not date_value:
                continue
            flags = []
            for idx in indexes:
                value = parse_float_text(row[idx]) if idx < len(row) else None
                flags.append(value is not None)
            if (require_all and all(flags)) or ((not require_all) and any(flags)):
                usable_dates.append(date_value)
    if not usable_dates:
        return None, None
    return min(usable_dates), max(usable_dates)


def usable_future_period_ends(path: Path,
                              date_column: str | None,
                              value_columns: list[str],
                              require_all: bool,
                              after_date: dt.date,
                              frequency: str,
                              year_type: str) -> list[dt.date]:
    if not value_columns:
        return []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        raw_columns = [str(item).strip() for item in header]
        date_name = str(date_column or "").strip()
        date_candidates = detect_date_candidates(path)
        if date_name and date_name in raw_columns:
            date_index = raw_columns.index(date_name)
        elif date_candidates and date_candidates[0] in raw_columns:
            date_index = raw_columns.index(date_candidates[0])
        else:
            date_index = 0
        indexes = [raw_columns.index(name) for name in value_columns if name in raw_columns]
        if not indexes:
            return []
        usable_dates: set[dt.date] = set()
        for row in reader:
            if date_index >= len(row):
                continue
            date_value = parse_date_text(row[date_index])
            if not date_value or date_value <= after_date:
                continue
            flags = []
            for idx in indexes:
                value = parse_float_text(row[idx]) if idx < len(row) else None
                flags.append(value is not None)
            if (require_all and all(flags)) or ((not require_all) and any(flags)):
                usable_dates.add(period_end_for_date(date_value, frequency, year_type))
    return sorted(usable_dates)


def xreg_column_details(path: Path, date_column: str | None = None) -> list[dict[str, object]]:
    details = csv_header_details(path, date_column)
    raw_columns = [str(item).strip() for item in details.get("header", []) if str(item).strip()]
    value_columns = [str(item).strip() for item in details.get("value_columns", []) if str(item).strip()]
    if not raw_columns or not value_columns:
        return []

    date_name = str(date_column or "").strip()
    date_candidates = detect_date_candidates(path)
    if date_name and date_name in raw_columns:
        date_index = raw_columns.index(date_name)
    elif date_candidates and date_candidates[0] in raw_columns:
        date_index = raw_columns.index(date_candidates[0])
    else:
        date_index = 0

    column_indexes = {name: raw_columns.index(name) for name in value_columns if name in raw_columns}
    observations: dict[str, list[tuple[dt.date, bool]]] = {name: [] for name in value_columns}

    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        next(reader, [])
        for row in reader:
            if date_index >= len(row):
                continue
            date_value = parse_date_text(row[date_index])
            if not date_value:
                continue
            for name, idx in column_indexes.items():
                value = parse_float_text(row[idx]) if idx < len(row) else None
                observations[name].append((date_value, value is not None))

    output: list[dict[str, object]] = []
    for name in value_columns:
        rows = observations.get(name, [])
        populated = [(date_value, has_value) for date_value, has_value in rows if has_value]
        usable = True
        reason = ""
        start = None
        end = None
        if not populated:
            usable = False
            reason = "Unusable: this column has no numeric values."
        else:
            start = populated[0][0]
            end = populated[-1][0]
            inside = [
                has_value
                for date_value, has_value in rows
                if start is not None and end is not None and start <= date_value <= end
            ]
            if any(not flag for flag in inside):
                usable = False
                reason = (
                    "Unusable: this column has gaps between "
                    f"{format_date_uk(start)} and {format_date_uk(end)}."
                )
        if usable and start and end:
            reason = f"Usable from {format_date_uk(start)} to {format_date_uk(end)}."
        output.append({
            "name": name,
            "usable": usable,
            "reason": reason,
            "start_date": format_date_uk(start),
            "end_date": format_date_uk(end),
            "start_date_iso": format_date_iso(start),
            "end_date_iso": format_date_iso(end),
        })
    return output


def csv_header_details(path: Path, date_column: str | None = None) -> dict[str, object]:
    return series_date_metadata(path, date_column)


def target_series_details(path_text: object,
                          date_column: object = "",
                          value_column: object = "") -> dict[str, object]:
    path = resolve_lab_path(path_text)
    if not path or not path.exists():
        return {}
    details = csv_header_details(path, str(date_column or "").strip() or None)
    value_name = str(value_column or "").strip()
    details["value_column"] = value_name
    usable_start, usable_end = usable_date_range(
        path,
        str(date_column or "").strip() or None,
        [value_name] if value_name else [],
        True,
    )
    details["usable_start_date"] = format_date_uk(usable_start)
    details["usable_end_date"] = format_date_uk(usable_end)
    details["usable_start_date_iso"] = format_date_iso(usable_start)
    details["usable_end_date_iso"] = format_date_iso(usable_end)
    details["outliers"] = detect_series_outliers(
        path,
        str(date_column or "").strip() or None,
        str(value_column or "").strip() or None,
    )
    details.update(
        detect_series_seasonality(
            path,
            str(date_column or "").strip() or None,
            str(value_column or "").strip() or None,
            str(details.get("detected_frequency", "unknown")),
        )
    )
    return details


def target_series_meta_map(path_text: object,
                           date_column: object = "") -> dict[str, dict[str, object]]:
    path = resolve_lab_path(path_text)
    if not path or not path.exists():
        return {}
    columns = initial_value_columns(path)
    output: dict[str, dict[str, object]] = {}
    for column in columns:
        name = str(column).strip()
        if not name:
            continue
        try:
            output[name] = target_series_details(path, date_column, name)
        except Exception:
            continue
    return output


def xreg_series_details(path_text: object,
                        date_column: object = "",
                        selected_columns_text: object = "") -> dict[str, object]:
    path = resolve_lab_path(path_text)
    if not path or not path.exists():
        return {}
    details = csv_header_details(path, str(date_column or "").strip() or None)
    selected_columns = [item.strip() for item in str(selected_columns_text or "").split(",") if item.strip()]
    details["value_column_details"] = xreg_column_details(path, str(date_column or "").strip() or None)
    usable_start, usable_end = usable_date_range(
        path,
        str(date_column or "").strip() or None,
        selected_columns,
        True,
    )
    details["usable_start_date"] = format_date_uk(usable_start)
    details["usable_end_date"] = format_date_uk(usable_end)
    details["usable_start_date_iso"] = format_date_iso(usable_start)
    details["usable_end_date_iso"] = format_date_iso(usable_end)
    details["path"] = str(path)
    details["selected_columns_text"] = ", ".join(selected_columns)
    return details


def analyse_xreg_collinearity(path_text: object,
                              date_column: object = "",
                              selected_columns_text: object = "") -> dict[str, object]:
    path = resolve_lab_path(path_text)
    selected_columns = [item.strip() for item in str(selected_columns_text or "").split(",") if item.strip()]
    if not path or not path.exists() or len(selected_columns) < 2:
        return {}

    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        rows = list(reader)
    if len(rows) < 2:
        return {}

    header = [str(item).strip() for item in rows[0]]
    requested_date = str(date_column or "").strip()
    date_index = header.index(requested_date) if requested_date and requested_date in header else 0
    selected_indexes: list[tuple[str, int]] = []
    for name in selected_columns:
        if name in header:
            selected_indexes.append((name, header.index(name)))
    if len(selected_indexes) < 2:
        return {}

    usable_rows: list[list[float]] = []
    usable_dates: list[dt.date] = []
    for row in rows[1:]:
        if date_index >= len(row):
            continue
        date_value = parse_date_text(row[date_index])
        if not date_value:
            continue
        values: list[float] = []
        usable = True
        for _, index in selected_indexes:
            if index >= len(row):
                usable = False
                break
            value = parse_float_text(row[index])
            if value is None:
                usable = False
                break
            values.append(value)
        if usable:
            usable_rows.append(values)
            usable_dates.append(date_value)

    if len(usable_rows) < 3:
        return {
            "level": "mediocre",
            "title": "Driver overlap check",
            "message": "Ophelia could not check driver overlap confidently because too few fully usable rows remained once all selected drivers were lined up.",
            "detail": "Try fewer drivers or check for missing values.",
            "action": "Proceed with caution and prefer a simpler driver set.",
        }

    columns: list[list[float]] = [[] for _ in selected_indexes]
    for row in usable_rows:
        for i, value in enumerate(row):
            columns[i].append(value)

    means = [sum(col) / len(col) for col in columns]
    stddevs: list[float] = []
    for col, mean in zip(columns, means):
        variance = sum((value - mean) ** 2 for value in col) / max(1, len(col) - 1)
        stddevs.append(math.sqrt(max(variance, 0.0)))

    near_constant = [
        name for (name, _), stddev in zip(selected_indexes, stddevs)
        if not math.isfinite(stddev) or stddev <= 1e-12
    ]
    max_corr = 0.0
    max_pair: tuple[str, str] | None = None
    for i in range(len(columns)):
        for j in range(i + 1, len(columns)):
            if stddevs[i] <= 1e-12 or stddevs[j] <= 1e-12:
                continue
            numerator = sum(
                (columns[i][k] - means[i]) * (columns[j][k] - means[j])
                for k in range(len(columns[i]))
            )
            denominator = (len(columns[i]) - 1) * stddevs[i] * stddevs[j]
            if denominator == 0.0:
                continue
            corr = numerator / denominator
            abs_corr = abs(corr)
            if abs_corr > max_corr:
                max_corr = abs_corr
                max_pair = (selected_indexes[i][0], selected_indexes[j][0])

    overlap_start = format_date_uk(usable_dates[0] if usable_dates else None)
    overlap_end = format_date_uk(usable_dates[-1] if usable_dates else None)
    overlap_detail = (
        f"Shared usable overlap: {overlap_start} to {overlap_end} across "
        f"{len(usable_rows)} fully usable row{'s' if len(usable_rows) != 1 else ''}."
        if usable_dates else
        ""
    )

    if near_constant:
        names = ", ".join(near_constant)
        return {
            "level": "poor",
            "title": "Driver overlap warning",
            "message": f"One or more selected drivers barely change over the shared period: {names}. That can make the regression unstable or uninformative.",
            "detail": overlap_detail,
            "action": "Remove the near-constant driver or replace it with a more informative one.",
        }
    if max_pair and max_corr >= 0.995:
        return {
            "level": "poor",
            "title": "Driver overlap warning",
            "message": f"The selected drivers {max_pair[0]} and {max_pair[1]} are almost duplicates over the shared period.",
            "detail": f"Strongest absolute correlation: {max_corr:.4f}. {overlap_detail}".strip(),
            "action": "Keep only one of those drivers, then run the model again.",
        }
    if max_pair and max_corr >= 0.98:
        return {
            "level": "mediocre",
            "title": "Driver overlap warning",
            "message": f"The selected drivers {max_pair[0]} and {max_pair[1]} overlap very strongly, so their separate effects may be hard to trust.",
            "detail": f"Strongest absolute correlation: {max_corr:.4f}. {overlap_detail}".strip(),
            "action": "Try a simpler run with one of those drivers removed and compare the error.",
        }
    if max_pair and max_corr >= 0.95:
        return {
            "level": "good",
            "title": "Driver overlap check",
            "message": f"The selected drivers {max_pair[0]} and {max_pair[1]} are fairly similar. The model may still be usable, but interpret individual driver effects carefully.",
            "detail": f"Strongest absolute correlation: {max_corr:.4f}. {overlap_detail}".strip(),
            "action": "If the driver list can be simpler, compare a run with one of them removed.",
        }
    return {
        "level": "excellent",
        "title": "Driver overlap check",
        "message": "The selected exogenous drivers do not appear to overlap strongly enough to raise an obvious stability warning.",
        "detail": f"Strongest absolute correlation: {max_corr:.4f}. {overlap_detail}".strip(),
        "action": "No special action needed.",
    }


def apply_outlier_handling(path_text: object,
                           date_column: object,
                           value_column: object,
                           outlier_mode: object,
                           outlier_dates_text: object) -> tuple[str, str]:
    mode = str(outlier_mode or "").strip().lower()
    if mode in ("", "none", "flag"):
        return str(path_text or ""), ""
    path = resolve_lab_path(path_text)
    value_name = str(value_column or "").strip()
    if not path or not path.exists() or not value_name:
        return str(path_text or ""), ""
    selected_dates = {item.strip() for item in str(outlier_dates_text or "").split(",") if item.strip()}
    if not selected_dates:
        return str(path_text or ""), ""

    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        rows = list(reader)
    if not rows:
        return str(path_text or ""), ""

    header = [str(item).strip() for item in rows[0]]
    requested_date = str(date_column or "").strip()
    date_index = header.index(requested_date) if requested_date and requested_date in header else 0
    if value_name not in header:
        return str(path_text or ""), ""
    value_index = header.index(value_name)

    data_rows = rows[1:]
    parsed_dates: list[dt.date | None] = []
    parsed_values: list[float | None] = []
    flagged_indexes: list[int] = []
    for idx, row in enumerate(data_rows):
        date_value = parse_date_text(row[date_index]) if date_index < len(row) else None
        value = parse_float_text(row[value_index]) if value_index < len(row) else None
        parsed_dates.append(date_value)
        parsed_values.append(value)
        if date_value and format_date_iso(date_value) in selected_dates:
            flagged_indexes.append(idx)
    if not flagged_indexes:
        return str(path_text or ""), ""

    non_null_values = [value for value in parsed_values if value is not None]
    if not non_null_values:
        return str(path_text or ""), ""
    med = median(non_null_values)
    mad = median([abs(value - med) for value in non_null_values]) or 1.0
    scale = 1.4826 * mad
    lower = med - (3.5 * scale)
    upper = med + (3.5 * scale)

    adjusted = list(parsed_values)
    if mode == "cap":
        for idx in flagged_indexes:
            value = adjusted[idx]
            if value is None:
                continue
            adjusted[idx] = min(max(value, lower), upper)
    elif mode == "exclude":
        flagged_set = set(flagged_indexes)
        for idx in flagged_indexes:
            left = idx - 1
            while left >= 0 and (left in flagged_set or adjusted[left] is None):
                left -= 1
            right = idx + 1
            while right < len(adjusted) and (right in flagged_set or adjusted[right] is None):
                right += 1
            if left >= 0 and right < len(adjusted):
                span = right - left
                fraction = (idx - left) / span if span else 0.0
                adjusted[idx] = adjusted[left] + ((adjusted[right] - adjusted[left]) * fraction)
            elif left >= 0:
                adjusted[idx] = adjusted[left]
            elif right < len(adjusted):
                adjusted[idx] = adjusted[right]
    else:
        return str(path_text or ""), ""

    ensure_upload_dir()
    output_path = UPLOAD_DIR / safe_upload_name(f"{Path(str(path_text or 'target')).stem}_outlier_fit.csv")
    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(rows[0])
        for idx, row in enumerate(data_rows):
            updated = list(row)
            if value_index < len(updated) and adjusted[idx] is not None:
                updated[value_index] = f"{adjusted[idx]:.10g}"
            writer.writerow(updated)
    note = (
        f"Outlier handling: {mode} applied to {len(flagged_indexes)} selected point"
        f"{'' if len(flagged_indexes) == 1 else 's'}."
    )
    return relative_display_path(output_path), note


def initial_value_columns(path_text: object) -> list[str]:
    path = resolve_lab_path(path_text)
    if not path or not path.exists():
        return []
    try:
        details = csv_header_details(path)
    except Exception:
        return []
    values = details.get("value_columns", [])
    if not isinstance(values, list):
        return []
    return [str(value).strip() for value in values if str(value).strip()]


def render_select_options(columns: list[str], selected: object) -> str:
    items = [str(column).strip() for column in columns if str(column).strip()]
    chosen = str(selected or "").strip()
    if not items:
        return '<option value="">Choose a column</option>'
    parts: list[str] = []
    for column in items:
        selected_attr = ' selected' if column == chosen else ''
        escaped = html.escape(column, quote=True)
        parts.append(f'<option value="{escaped}"{selected_attr}>{escaped}</option>')
    return "".join(parts)


def normalize_model_selection(text: object, fallback: object = "") -> list[str]:
    selected = [item.strip().lower() for item in str(text or "").split(",") if item.strip()]
    allowed = {value for value, _ in MODEL_OPTIONS}
    ordered = [value for value, _ in MODEL_OPTIONS if value in selected]
    if ordered:
        return ordered
    fallback_model = str(fallback or "").strip().lower()
    if fallback_model in allowed:
        return [fallback_model]
    return [MODEL_OPTIONS[-1][0]]


def normalize_state_models(state: dict[str, object]) -> dict[str, object]:
    selected = normalize_model_selection(
        state.get("models", ""),
        DEFAULT_STATE["models"],
    )
    state["models"] = ", ".join(selected)
    return state


def render_model_picker_html(selected_text: object, fallback: object = "") -> tuple[str, str]:
    selected = normalize_model_selection(selected_text, fallback)
    summary = ", ".join(label for value, label in MODEL_OPTIONS if value in selected)
    items: list[str] = []
    for value, label in MODEL_OPTIONS:
        checked = ' checked' if value in selected else ''
        escaped_value = html.escape(value, quote=True)
        escaped_label = html.escape(label, quote=False)
        items.append(
            f'<label class="multi-select-item" title="{escaped_label}">'
            f'<input type="checkbox" name="model-choice" value="{escaped_value}"{checked}>'
            f'<span>{escaped_label}</span></label>'
        )
    return summary, "".join(items)


def selected_models_from_payload(payload: dict[str, object]) -> list[str]:
    return normalize_model_selection(
        payload.get("models", ""),
        DEFAULT_STATE["models"],
    )


def active_model_from_payload(payload: dict[str, object]) -> str:
    selected = selected_models_from_payload(payload)
    return selected[0] if selected else str(MODEL_OPTIONS[-1][0])


def render_date_select_options(columns: list[str], selected: object, fallback: object = "") -> str:
    items = [str(column).strip() for column in columns if str(column).strip()]
    chosen = str(selected or "").strip() or str(fallback or "").strip()
    if not items:
        if chosen:
            escaped = html.escape(chosen, quote=True)
            return f'<option value="{escaped}" selected>{escaped}</option>'
        return '<option value="">No date columns found</option>'
    if len(items) == 1:
        chosen = items[0]
    if chosen not in items:
        chosen = items[0]
    parts: list[str] = []
    for column in items:
        selected_attr = ' selected' if column == chosen else ''
        escaped = html.escape(column, quote=True)
        parts.append(f'<option value="{escaped}"{selected_attr}>{escaped}</option>')
    return "".join(parts)


def season_period_options_for_frequency(frequency: object) -> list[tuple[str, str]]:
    token = str(frequency or "").strip().lower()
    if token == "monthly":
        return [
            ("0", "None"),
            ("1", "Weak / unclear"),
            ("3", "Quarterly"),
            ("4", "Every 4 months"),
            ("6", "Half-yearly"),
            ("12", "Yearly"),
        ]
    if token == "quarterly":
        return [
            ("0", "None"),
            ("1", "Weak / unclear"),
            ("2", "Half-yearly"),
            ("4", "Yearly"),
        ]
    if token == "yearly":
        return [
            ("0", "None"),
            ("1", "Weak / unclear"),
        ]
    return [
        ("0", "None"),
        ("1", "Weak / unclear"),
    ]


def render_season_period_options(frequency: object, selected: object) -> str:
    options = season_period_options_for_frequency(frequency)
    chosen = str(selected or "").strip()
    if not any(value == chosen for value, _ in options):
        chosen = options[0][0]
    parts: list[str] = []
    for value, label in options:
        selected_attr = ' selected' if value == chosen else ''
        parts.append(
            f'<option value="{html.escape(value, quote=True)}"{selected_attr}>'
            f'{html.escape(label, quote=False)}</option>'
        )
    return "".join(parts)


def render_forecast_end_options(options: list[tuple[str, str]], selected: object) -> str:
    chosen = str(selected or "").strip()
    if not options:
        return '<option value="">Choose a forecast end date</option>'
    values = {value for value, _ in options}
    if chosen not in values:
        chosen = options[-1][0]
    groups: dict[str, list[tuple[str, str]]] = {}
    for value, label in options:
        year = value[:4] if len(value) >= 4 else "Other"
        groups.setdefault(year, []).append((value, label))
    parts: list[str] = []
    for year in sorted(groups.keys()):
        parts.append(f'<optgroup label="{html.escape(year, quote=True)}">')
        for value, label in groups[year]:
            selected_attr = ' selected' if value == chosen else ''
            escaped_value = html.escape(value, quote=True)
            escaped_label = html.escape(label, quote=False)
            parts.append(f'<option value="{escaped_value}"{selected_attr}>{escaped_label}</option>')
        parts.append('</optgroup>')
    return "".join(parts)


def render_series_meta_html(meta: dict[str, object]) -> str:
    frequency = html.escape(str(meta.get("detected_frequency_label") or "Unknown"), quote=False)
    date_column = html.escape(str(meta.get("date_column") or "Unknown"), quote=False)
    value_column = html.escape(str(meta.get("value_column") or ""), quote=False)
    start = html.escape(str(meta.get("start_date") or "Unknown"), quote=False)
    end = html.escape(str(meta.get("end_date") or "Unknown"), quote=False)
    usable_start = html.escape(str(meta.get("usable_start_date") or meta.get("start_date") or "Unknown"), quote=False)
    usable_end = html.escape(str(meta.get("usable_end_date") or meta.get("end_date") or "Unknown"), quote=False)
    seasonality = html.escape(str(meta.get("seasonality_label") or ""), quote=False)
    seasonality_score = str(meta.get("seasonality_score") or "").strip()
    seasonality_lag = str(meta.get("seasonality_lag") or "").strip()
    seasonality_strength = str(meta.get("seasonality_strength") or "none").strip().lower().replace(" ", "-")
    seasonality_class = html.escape(f"seasonality-{seasonality_strength}", quote=True)
    seasonality_bits: list[str] = []
    if seasonality_score:
        seasonality_bits.append(f"score {seasonality_score}")
    if seasonality_lag and seasonality_lag != "0":
        seasonality_bits.append(f"lag {seasonality_lag}")
    seasonality_text = seasonality
    if seasonality and seasonality_bits:
        seasonality_text = html.escape(f"{str(meta.get('seasonality_label') or '')} ({', '.join(seasonality_bits)})", quote=False)
    return (
        f"<div><strong>Detected frequency:</strong> {frequency}</div>"
        + f"<div><strong>Date column:</strong> {date_column}</div>"
        + (f"<div><strong>Series:</strong> {value_column}</div>" if value_column else "")
        + f"<div><strong>File date range:</strong> {start} to {end}</div>"
        + f"<div><strong>Usable series range:</strong> {usable_start} to {usable_end}</div>"
        + (
            f'<div class="seasonality-line"><strong>Seasonality:</strong> '
            f'<span class="seasonality-badge {seasonality_class}">{seasonality_text}</span></div>'
            if seasonality else ""
        )
    )


def render_outlier_picker_html(outliers: list[dict[str, object]], selected_text: object) -> tuple[str, str]:
    selected = {item.strip() for item in str(selected_text or "").split(",") if item.strip()}
    if not outliers:
        return (
            '<div class="multi-select-placeholder">No target points are currently flagged here.</div>',
            'No outliers detected.',
        )
    if not selected:
        selected = {str(item.get("date_iso", "")).strip() for item in outliers if str(item.get("date_iso", "")).strip()}
    rows: list[str] = []
    count = 0
    for item in outliers:
        date_iso = str(item.get("date_iso", "")).strip()
        label = f"{item.get('date', '')}: {item.get('value', '')} ({outlier_likelihood_label(item.get('score'))})"
        checked = ' checked' if date_iso in selected else ''
        rows.append(
            f'<label class="multi-select-item" title="{html.escape(label, quote=True)}"><input type="checkbox" name="outlier-choice" value="{html.escape(date_iso, quote=True)}"{checked}><span>{html.escape(label, quote=False)}</span></label>'
        )
        if date_iso in selected:
            count += 1
    return "".join(rows), f"{count} outlier{'s' if count != 1 else ''} selected."


def upload_label(display_name: object, path_text: object) -> str:
    display = str(display_name or "").strip()
    if display:
        return display
    raw = str(path_text or "").strip()
    if not raw:
        return "No file chosen"
    return Path(raw).name or raw


def render_xreg_picker_html(columns: list[object], selected_text: object) -> tuple[str, str, str]:
    normalized: list[dict[str, object]] = []
    for column in columns:
        if isinstance(column, dict):
            name = str(column.get("name", "")).strip()
            if not name:
                continue
            normalized.append({
                "name": name,
                "usable": bool(column.get("usable", False)),
                "reason": str(column.get("reason", "")).strip(),
            })
        else:
            name = str(column).strip()
            if not name:
                continue
            normalized.append({"name": name, "usable": True, "reason": ""})
    items = [str(column.get("name", "")).strip() for column in normalized if str(column.get("name", "")).strip()]
    selected = [value for value in (str(selected_text or "").split(",")) if value.strip()]
    selected = [value.strip() for value in selected]
    usable_names = {str(column.get("name", "")).strip() for column in normalized if bool(column.get("usable", False))}
    selected_set = {value for value in selected if value in usable_names}
    if not normalized:
        return (
            'Selected: <strong>none yet</strong>',
            '<div class="multi-select-placeholder">Upload or choose an exogenous CSV to load a scrollable list of driver columns here.</div>',
            'Upload or choose an exogenous CSV to load its available columns.',
        )

    count = sum(1 for item in items if item in selected_set)
    if count <= 0:
        summary = 'Selected: <strong>none yet</strong>'
        status = 'Choose one or more driver columns from the list.' if usable_names else 'No selectable driver columns were found in this CSV.'
    elif count == 1:
        first = next(item for item in items if item in selected_set)
        summary = f'Selected: <strong>1 column</strong>: {html.escape(first, quote=False)}'
        status = '1 exogenous column selected.'
    else:
        chosen = [item for item in items if item in selected_set]
        preview = ", ".join(chosen[:3])
        if len(chosen) > 3:
            summary = f'Selected: <strong>{len(chosen)} columns</strong>: {html.escape(preview, quote=False)} + {len(chosen) - 3} more'
        else:
            summary = f'Selected: <strong>{len(chosen)} columns</strong>: {html.escape(preview, quote=False)}'
        status = f'{len(chosen)} exogenous columns selected.'

    rows: list[str] = []
    for column in normalized:
        name = str(column.get("name", "")).strip()
        usable = bool(column.get("usable", False))
        reason = str(column.get("reason", "")).strip()
        checked_attr = ' checked' if usable and name in selected_set else ''
        disabled_attr = '' if usable else ' disabled'
        escaped = html.escape(name, quote=True)
        label = html.escape(name, quote=False)
        title = html.escape(reason or name, quote=True)
        disabled_class = '' if usable else ' is-disabled'
        rows.append(
            f'<label class="multi-select-item{disabled_class}" title="{title}"><input type="checkbox" name="xreg-column-choice" value="{escaped}"{checked_attr}{disabled_attr}><span>{label}</span></label>'
        )
    return summary, "".join(rows), status


def normalize_summary_text(text: object) -> str:
    source = str(text or "").strip()
    markers = [
        "Overall assessment:",
        "Overall fit score (R2):",
        "Adjusted fit score (Adj R2):",
        "Typical forecast error (RMSE):",
        "Model comparison score (AIC):",
        "Model comparison score (BIC):",
        "Unexplained variation left in the model (Sigma2):",
        "How to read this:",
        "Coefficients",
        "AR parameters",
        "Model:",
    ]
    if not source:
        return source
    if "\n" not in source and "\r" not in source:
        for index, marker in enumerate(markers):
            replacement = marker if index == 0 and source.startswith(marker) else f"\n{marker}"
            source = source.replace(f" {marker}", replacement)
        source = re.sub(r" (beta\[\d+\] = )", r"\n\1", source)
        source = re.sub(r" (phi\[\d+\] = )", r"\n\1", source)
        source = source.replace(" How to read this: - ", "\nHow to read this:\n- ")
        source = re.sub(r" - (?=[A-Z])", "\n- ", source)
    source = source.replace("\r\n", "\n")
    source = re.sub(r"\n{3,}", "\n\n", source)
    return source


def summary_rating_class(line: str) -> str:
    lower = line.lower()
    if "(exceptional" in lower:
        return "rating-exceptional"
    if "(excellent" in lower:
        return "rating-excellent"
    if "(very good" in lower:
        return "rating-very-good"
    if "(good" in lower:
        return "rating-good"
    if "(mediocre" in lower:
        return "rating-mediocre"
    if "(poor" in lower:
        return "rating-poor"
    if "(comparison only" in lower:
        return "rating-comparison"
    return ""


def coefficient_rating_class_from_ratio(ratio: float) -> str:
    if not math.isfinite(ratio):
        return "rating-comparison"
    if ratio >= 4.0:
        return "rating-exceptional"
    if ratio >= 3.0:
        return "rating-excellent"
    if ratio >= 2.0:
        return "rating-very-good"
    if ratio >= 1.5:
        return "rating-good"
    if ratio >= 1.0:
        return "rating-mediocre"
    return "rating-poor"


def coefficient_support_text(ratio: float) -> str:
    if not math.isfinite(ratio):
        return "comparison only"
    if ratio >= 4.0:
        return "extremely likely helping"
    if ratio >= 3.0:
        return "very likely helping"
    if ratio >= 2.0:
        return "likely helping"
    if ratio >= 1.5:
        return "possibly helping"
    if ratio >= 1.0:
        return "weak evidence"
    return "little evidence"


def coefficient_display_name(index: int, xreg_columns_text: object) -> str:
    labels = [item.strip() for item in str(xreg_columns_text or "").split(",") if item.strip()]
    if index == 0:
        return "Intercept"
    if index - 1 < len(labels):
        return f"Driver: {labels[index - 1]}"
    return f"Coefficient {index}"


def parse_summary_coefficient_line(line: str, xreg_columns_text: object) -> dict[str, object] | None:
    beta_match = re.match(r"^beta\[(\d+)\] = ([^\s]+)\s+stderr = ([^\s]+)\s+p = ([^\s]+)$", line)
    phi_match = re.match(r"^phi\[(\d+)\] = ([^\s]+)$", line)
    if beta_match:
        index = int(beta_match.group(1))
        estimate = float(beta_match.group(2))
        stderr = float(beta_match.group(3))
        p_value = float(beta_match.group(4))
        ratio = abs(estimate) / abs(stderr) if stderr else math.inf
        return {
            "type": "beta",
            "index": index,
            "title": coefficient_display_name(index, xreg_columns_text),
            "estimate": estimate,
            "stderr": stderr,
            "p_value": p_value,
            "ratio": ratio,
        }
    if phi_match:
        index = int(phi_match.group(1))
        estimate = float(phi_match.group(2))
        return {
            "type": "phi",
            "index": index,
            "title": f"AR term {index}",
            "estimate": estimate,
            "stderr": math.nan,
            "p_value": math.nan,
            "ratio": math.nan,
        }
    return None


def render_summary_line_html(line: str, index: int) -> str:
    trimmed = line.strip()
    classes = ["summary-line"]
    content = html.escape(line)
    if index == 0 and re.search(r"summary$", trimmed, flags=re.IGNORECASE):
        classes.append("title-line")
    elif re.match(r"^(How to read this:|Coefficients|AR parameters|Model:)", trimmed, flags=re.IGNORECASE):
        classes.append("section-line")
    elif trimmed.startswith("- "):
        classes.append("note-line")
    else:
        rating = summary_rating_class(trimmed)
        if rating:
            classes.append(rating)

    if ": " in line and not trimmed.lower().startswith("model:") and not trimmed.lower().startswith("how to read this:"):
        split_at = line.index(": ")
        key = html.escape(line[:split_at + 1])
        value = html.escape(line[split_at + 2:])
        content = f'<span class="summary-k">{key}</span> <span class="summary-v">{value}</span>'
    return f'<div class="{" ".join(classes)}">{content}</div>'


def render_summary_coefficient_card(line: str, xreg_columns_text: object) -> str:
    parsed = parse_summary_coefficient_line(line.strip(), xreg_columns_text)
    if not parsed:
        return f'<div class="summary-line"><span class="summary-v">{html.escape(line)}</span></div>'
    rating_class = coefficient_rating_class_from_ratio(float(parsed["ratio"]))
    if parsed["type"] == "beta":
        estimate = f'{float(parsed["estimate"]):.4f}'
        stderr = f'{float(parsed["stderr"]):.4f}'
        p_value = f'{float(parsed["p_value"]):.4g}'
        note = coefficient_support_text(float(parsed["ratio"]))
    else:
        estimate = f'{float(parsed["estimate"]):.4f}'
        stderr = "n/a"
        p_value = "n/a"
        note = "Autoregressive carry-over term"
    return (
        f'<div class="summary-driver-row {rating_class}">'
        f'<div class="summary-driver-name">{html.escape(str(parsed["title"]))}</div>'
        f'<div class="summary-driver-number">{html.escape(estimate)}</div>'
        f'<div class="summary-driver-number">{html.escape(stderr)}</div>'
        f'<div class="summary-driver-number">{html.escape(p_value)}</div>'
        f'<div class="summary-driver-impact">{html.escape(note)}</div>'
        f'</div>'
    )


def collinearity_rating_class(level: object) -> str:
    value = str(level or "").strip().lower()
    if value == "excellent":
        return "rating-excellent"
    if value == "good":
        return "rating-good"
    if value == "mediocre":
        return "rating-mediocre"
    if value == "poor":
        return "rating-poor"
    return "rating-comparison"


def parse_forecast_csv_rows(text: object) -> list[dict[str, str]]:
    source = str(text or "").strip()
    if not source:
        return []
    reader = csv.DictReader(io.StringIO(source))
    rows: list[dict[str, str]] = []
    for row in reader:
        rows.append({str(key or "").strip().lower(): str(value or "").strip() for key, value in row.items()})
    return rows


def build_multi_forecast_csv(results: list[dict[str, object]]) -> str:
    successful = [item for item in results if item.get("ok")]
    if not successful:
        return ""

    by_date: dict[str, dict[str, object]] = {}
    order: list[str] = []
    labels = unique_result_labels(successful)

    for result, label in zip(successful, labels):
        for row in parse_forecast_csv_rows(result.get("forecast_csv", "")):
            date_text = str(row.get("date", "")).strip()
            if not date_text:
                continue
            entry = by_date.get(date_text)
            if entry is None:
                entry = {"actual": "", "values": {}}
                by_date[date_text] = entry
                order.append(date_text)
            actual_text = str(row.get("actual", "")).strip()
            if actual_text and not entry["actual"]:
                entry["actual"] = actual_text
            entry["values"][label] = {
                "mean": str(row.get("mean", "")).strip(),
                "stderr": str(row.get("stderr", "")).strip(),
            }

    if not order:
        return ""

    csv_buffer = io.StringIO()
    writer = csv.writer(csv_buffer, lineterminator="\n")
    header = ["date", "actual"]
    for label in labels:
        header.extend([f"{label} mean", f"{label} stderr"])
    writer.writerow(header)
    for date_text in order:
        entry = by_date.get(date_text, {"actual": "", "values": {}})
        row = [date_text, str(entry.get("actual", ""))]
        values = entry.get("values", {})
        for label in labels:
            cell = values.get(label, {})
            row.extend([str(cell.get("mean", "")), str(cell.get("stderr", ""))])
        writer.writerow(row)
    return csv_buffer.getvalue().strip()


def unique_result_labels(results: list[dict[str, object]]) -> list[str]:
    counts: dict[str, int] = {}
    labels: list[str] = []
    for index, result in enumerate(results):
        raw = str(result.get("model", "") or f"Forecast {index + 1}").strip() or f"Forecast {index + 1}"
        seen = counts.get(raw, 0)
        counts[raw] = seen + 1
        labels.append(f"{raw} {seen + 1}" if seen else raw)
    return labels


def build_multi_summary_text(results: list[dict[str, object]]) -> str:
    successful = [item for item in results if item.get("ok")]
    if not successful:
        return ""

    sections: list[str] = ["Multi-model comparison summary"]
    for result in results:
        label = str(result.get("model", "Forecast")).strip() or "Forecast"
        sections.append("")
        sections.append(f"=== {label} ===")
        sections.append(f"Run status: {'OK' if result.get('ok') else 'Failed'}")
        sections.append(f"Target modelled: {str(result.get('target_value_column_used', '')).strip() or '(none)'}")
        sections.append(f"Drivers used: {str(result.get('xreg_columns_used', '')).strip() or '(none)'}")
        if result.get("ok"):
            sections.append(f"Historic rows used: {result.get('fit_rows', '-')}")
            sections.append(f"Stationary: {'Yes' if result.get('stationary') else 'No'}")
            sections.append(f"Invertible: {'Yes' if result.get('invertible') else 'No'}")
            summary_text = str(result.get("summary_text", "") or "").strip()
            if summary_text:
                sections.append("")
                sections.append(summary_text)
        else:
            sections.append(str(result.get("error", "Run failed.")))
    return "\n".join(sections).strip()


def render_multi_summary_html(results: list[dict[str, object]]) -> str:
    if not results:
        return '<div class="summary-line note-line">No comparison results returned.</div>'

    labels = unique_result_labels(results)
    tabs: list[str] = []
    panels: list[str] = []
    for index, (result, label) in enumerate(zip(results, labels)):
        escaped_label = html.escape(label, quote=False)
        panel_id = f"comparison-summary-panel-{index + 1}"
        active_class = " is-active" if index == 0 else ""
        selected = "true" if index == 0 else "false"
        fit_rows = html.escape(str(result.get("fit_rows", "-")), quote=False)
        stable = "Yes" if result.get("stationary") else "No"
        invertible = "Yes" if result.get("invertible") else "No"
        target = html.escape(str(result.get("target_value_column_used", "") or "(none)"), quote=False)
        drivers = html.escape(str(result.get("xreg_columns_used", "") or "(none)"), quote=False)
        status = "OK" if result.get("ok") else "Failed"
        status_class = "" if result.get("ok") else " rating-poor"
        if result.get("ok"):
            summary_html = str(result.get("summary_html", "") or '<div class="summary-line note-line">No summary returned.</div>')
        else:
            error = html.escape(str(result.get("error", "Run failed.")), quote=False)
            summary_html = f'<div class="summary-overall rating-poor"><h4>{escaped_label}</h4><p>{error}</p></div>'
        tabs.append(
            f'<button type="button" class="comparison-tab{active_class}" '
            f'role="tab" aria-selected="{selected}" data-tab-target="{panel_id}">{escaped_label}</button>'
        )
        panels.append(
            f'<div id="{panel_id}" class="comparison-tab-panel{active_class}" role="tabpanel">'
            f'<section class="comparison-run{status_class}">'
            f'<div class="comparison-run-head"><h4>{escaped_label}</h4></div>'
            f'<p class="summary-note">Target modelled: {target} | Drivers used: {drivers}</p>'
            '<div class="comparison-metrics">'
            f'<div class="comparison-metric"><span>Historic rows</span><strong>{fit_rows}</strong></div>'
            f'<div class="comparison-metric"><span>Stationary</span><strong>{stable}</strong></div>'
            f'<div class="comparison-metric"><span>Invertible</span><strong>{invertible}</strong></div>'
            f'<div class="comparison-metric"><span>Run status</span><strong>{status}</strong></div>'
            '</div>'
            f'{summary_html}'
            '</section>'
            '</div>'
        )
    return (
        '<div class="comparison-tabs">'
        f'<div class="comparison-tab-list" role="tablist">{"".join(tabs)}</div>'
        f'{"".join(panels)}'
        '</div>'
    )


def limit_forecast_outputs(forecast_csv_text: object,
                           forecast_text: object,
                           end_date: dt.date) -> tuple[str, str]:
    rows = parse_forecast_csv_rows(forecast_csv_text)
    if not rows:
        return str(forecast_csv_text or ""), str(forecast_text or "")

    filtered_rows: list[dict[str, str]] = []
    for row in rows:
        row_date = parse_date_text(row.get("date", ""))
        actual_text = str(row.get("actual", "")).strip()
        is_future = not actual_text
        if is_future and row_date and row_date > end_date:
            continue
        filtered_rows.append(row)

    if len(filtered_rows) == len(rows):
        return str(forecast_csv_text or ""), str(forecast_text or "")

    csv_buffer = io.StringIO()
    writer = csv.writer(csv_buffer, lineterminator="\n")
    header = ["date", "actual", "mean", "stderr", "lower", "upper"]
    writer.writerow(header)
    for row in filtered_rows:
        writer.writerow([row.get(key, "") for key in header])
    csv_text = csv_buffer.getvalue().strip()

    text_lines: list[str] = []
    for row in filtered_rows:
        date_text = row.get("date", "")
        actual_text = row.get("actual", "")
        mean_text = row.get("mean", "")
        stderr_text = row.get("stderr", "")
        lower_text = row.get("lower", "")
        upper_text = row.get("upper", "")
        if actual_text:
            text_lines.append(f"{date_text}: actual {actual_text}, mean {mean_text}")
        else:
            extras = []
            if stderr_text:
                extras.append(f"stderr {stderr_text}")
            if lower_text:
                extras.append(f"lower {lower_text}")
            if upper_text:
                extras.append(f"upper {upper_text}")
            suffix = f", {', '.join(extras)}" if extras else ""
            text_lines.append(f"{date_text}: actual n/a, mean {mean_text}{suffix}")
    return csv_text, "\n".join(text_lines)


def analyse_forecast_plausibility(forecast_csv_text: object, model_name: object = "") -> dict[str, object] | None:
    rows = parse_forecast_csv_rows(forecast_csv_text)
    if not rows:
        return None
    model_label = str(model_name or "").strip().upper() or "forecast"

    historical_actuals: list[float] = []
    future_means: list[float] = []
    future_stderr: list[float] = []
    future_lowers: list[float] = []
    future_uppers: list[float] = []

    for row in rows:
        actual = parse_float_text(row.get("actual", ""))
        mean = parse_float_text(row.get("mean", ""))
        stderr = parse_float_text(row.get("stderr", ""))
        lower = parse_float_text(row.get("lower", ""))
        upper = parse_float_text(row.get("upper", ""))
        if actual is not None:
            historical_actuals.append(actual)
        else:
            if mean is not None:
                future_means.append(mean)
            if stderr is not None:
                future_stderr.append(stderr)
            if lower is not None:
                future_lowers.append(lower)
            if upper is not None:
                future_uppers.append(upper)

    if not future_means:
        return None

    recent_actuals = historical_actuals[-12:] if historical_actuals else []
    recent_level = median([value for value in recent_actuals if math.isfinite(value)]) if recent_actuals else 0.0
    nonnegative_history = bool(historical_actuals) and min(historical_actuals) >= -1e-9
    negative_future_count = sum(1 for value in future_means if value < -1e-9)
    clipped_zero_count = sum(1 for value in future_lowers if abs(value) <= 1e-9)
    median_upper = median([value for value in future_uppers if math.isfinite(value)]) if future_uppers else 0.0
    median_stderr = median([value for value in future_stderr if math.isfinite(value)]) if future_stderr else 0.0
    final_mean = future_means[-1]
    median_future = median([value for value in future_means if math.isfinite(value)])

    issues: list[str] = []
    detail_parts: list[str] = []
    action_parts: list[str] = []
    level = "good"

    if nonnegative_history and negative_future_count:
        level = "poor"
        issues.append(f"This {model_label} forecast is dropping below zero even though the historical series is non-negative.")
        action_parts.append("Treat this model as implausible and try a simpler specification or a different model family.")

    if (
        nonnegative_history
        and recent_level > 0.0
        and median_future < 0.35 * recent_level
        and median_stderr > 0.35 * recent_level
    ):
        level = "poor" if level != "poor" else level
        issues.append("The forecast level is collapsing far below the recent history while uncertainty stays very wide.")
        action_parts.append("Prefer a simpler model, fewer seasonal terms, or a regression-style model if that stays more realistic.")

    if (
        nonnegative_history
        and future_lowers
        and clipped_zero_count >= max(2, len(future_lowers) // 2)
        and recent_level > 0.0
        and median_upper > 1.8 * recent_level
    ):
        if level != "poor":
            level = "mediocre"
        issues.append("Many lower bounds are being clipped at zero while the upper bounds stay very wide.")
        action_parts.append("Use caution: this usually means the model is struggling to produce a believable range.")

    if not issues:
        return None

    if recent_actuals:
        detail_parts.append(
            f"Recent typical level: about {recent_level:.2f}. "
            f"Median future mean: about {median_future:.2f}. "
            f"Final forecast mean: {final_mean:.2f}."
        )
    if negative_future_count:
        detail_parts.append(f"{negative_future_count} future mean value{'s' if negative_future_count != 1 else ''} fell below zero.")
    if clipped_zero_count:
        detail_parts.append(
            f"{clipped_zero_count} future lower bound{'s were' if clipped_zero_count != 1 else ' was'} clipped to zero."
        )

    return {
        "title": "Forecast plausibility warning",
        "message": " ".join(issues),
        "detail": " ".join(detail_parts).strip(),
        "action": " ".join(dict.fromkeys(action_parts)).strip(),
        "level": level,
    }


def render_summary_html(text: object,
                        xreg_columns_text: object = "",
                        collinearity_warning: dict[str, object] | None = None,
                        plausibility_warning: dict[str, object] | None = None) -> str:
    source = normalize_summary_text(text)
    if not source:
        return '<div class="summary-line note-line">No summary returned.</div>'
    lines = [line for line in source.splitlines() if line.strip()]
    title = ""
    assessment = ""
    metric_lines: list[str] = []
    guidance_lines: list[str] = []
    coeff_cards: list[str] = []
    in_guidance = False
    in_coeff_section = False

    for line in lines:
        trimmed = line.strip()
        parsed_coeff = parse_summary_coefficient_line(trimmed, xreg_columns_text)
        if not title and re.search(r"summary$", trimmed, flags=re.IGNORECASE):
            title = trimmed
            continue
        if trimmed.startswith("Overall assessment:"):
            assessment = trimmed
            continue
        if trimmed.startswith("How to read this:"):
            in_guidance = True
            in_coeff_section = False
            continue
        if re.match(r"^(Coefficients|AR parameters)$", trimmed, flags=re.IGNORECASE):
            in_coeff_section = True
            in_guidance = False
            continue
        if parsed_coeff and in_coeff_section:
            coeff_cards.append(render_summary_coefficient_card(trimmed, xreg_columns_text))
            continue
        if trimmed.startswith("- ") and in_guidance:
            guidance_lines.append(trimmed[2:].strip())
            continue
        if re.match(r"^(Overall fit score \(R2\):|Adjusted fit score \(Adj R2\):|Typical forecast error \(RMSE\):|Model comparison score \(AIC\):|Model comparison score \(BIC\):|Unexplained variation left in the model \(Sigma2\):)", trimmed):
            metric_lines.append(trimmed)
            continue

    if not title:
        title = "Forecast summary"

    blocks: list[str] = [f'<div class="summary-line title-line">{html.escape(title)}</div>']

    if assessment:
        assessment_text = assessment.split(": ", 1)[1] if ": " in assessment else assessment
        rating = summary_rating_class(assessment)
        blocks.append(
            f'<div class="summary-overall {rating}">'
            '<h4>Overall assessment</h4>'
            f'<p>{html.escape(assessment_text)}</p>'
            '</div>'
        )

    if metric_lines:
        metric_cards = []
        for line in metric_lines:
            split_at = line.index(": ")
            label = html.escape(line[:split_at])
            value = html.escape(line[split_at + 2:])
            rating = summary_rating_class(line)
            metric_cards.append(
                f'<div class="summary-metric-card {rating}">'
                f'<div class="summary-metric-label">{label}</div>'
                f'<div class="summary-metric-value">{value}</div>'
                f'</div>'
            )
        blocks.append(f'<div class="summary-metric-grid">{"".join(metric_cards)}</div>')

    if guidance_lines:
        items = "".join(f"<li>{html.escape(item)}</li>" for item in guidance_lines)
        blocks.append(
            '<div class="summary-guidance">'
            '<h4 class="summary-section-title">How to read this</h4>'
            f'<ul>{items}</ul>'
            '</div>'
        )

    if collinearity_warning:
        rating = collinearity_rating_class(collinearity_warning.get("level"))
        title_text = html.escape(str(collinearity_warning.get("title") or "Driver overlap check"))
        message_text = html.escape(str(collinearity_warning.get("message") or ""))
        detail_text = html.escape(str(collinearity_warning.get("detail") or ""))
        action_text = html.escape(str(collinearity_warning.get("action") or ""))
        detail_html = f'<p>{detail_text}</p>' if detail_text else ''
        action_html = f'<p><strong>What to do:</strong> {action_text}</p>' if action_text else ''
        blocks.append(
            f'<div class="summary-overall {rating}">'
            f'<h4>{title_text}</h4>'
            f'<p>{message_text}</p>'
            f'{detail_html}'
            f'{action_html}'
            '</div>'
        )

    if plausibility_warning:
        rating = collinearity_rating_class(plausibility_warning.get("level"))
        title_text = html.escape(str(plausibility_warning.get("title") or "Forecast plausibility warning"))
        message_text = html.escape(str(plausibility_warning.get("message") or ""))
        detail_text = html.escape(str(plausibility_warning.get("detail") or ""))
        action_text = html.escape(str(plausibility_warning.get("action") or ""))
        detail_html = f'<p>{detail_text}</p>' if detail_text else ''
        action_html = f'<p><strong>What to do:</strong> {action_text}</p>' if action_text else ''
        blocks.append(
            f'<div class="summary-overall {rating}">'
            f'<h4>{title_text}</h4>'
            f'<p>{message_text}</p>'
            f'{detail_html}'
            f'{action_html}'
            '</div>'
        )

    if coeff_cards:
        blocks.append(
            '<div class="summary-driver-section">'
            '<h4 class="summary-section-title">How the drivers look in this model</h4>'
            '<div class="summary-driver-list">'
            '<div class="summary-driver-head">'
            '<div>Variable</div>'
            '<div>Estimate</div>'
            '<div>StdErr</div>'
            '<div>p value</div>'
            '<div>How much it seems to matter</div>'
            '</div>'
            '<div class="summary-driver-scroll">'
            f'{"".join(coeff_cards)}'
            '</div>'
            '</div>'
            '</div>'
        )

    return "".join(blocks) or '<div class="summary-line note-line">No summary returned.</div>'


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
    if completed.returncode != 0:
        return False
    try:
        status = json.loads(completed.stdout)
        web = status.get("Web", {}) if isinstance(status, dict) else {}
        allowed = status.get("AllowFunnel", {}) if isinstance(status, dict) else {}
        for host_key, config in web.items():
            handlers = (config or {}).get("Handlers", {}) if isinstance(config, dict) else {}
            if APP_BASE_PATH in handlers and bool(allowed.get(host_key)):
                return True
    except Exception:
        pass
    return APP_BASE_PATH in completed.stdout


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
    should_be_public = shared.tailscale_public_mode() or ophelia_tailscale_funnel_enabled()
    ophelia_set_tailscale_funnel_enabled(port, should_be_public)


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
    initial_target_columns = initial_value_columns(state.get("target_path", ""))
    initial_target_meta_map = target_series_meta_map(
        state.get("target_path", ""),
        state.get("target_date_column", ""),
    )
    initial_xreg_columns = initial_value_columns(state.get("xreg_path", ""))
    initial_target_value_column = str(state.get("target_value_column", "")).strip()
    if not initial_target_value_column and initial_target_columns:
        initial_target_value_column = initial_target_columns[0]
    initial_target_meta = target_series_details(
        state.get("target_path", ""),
        state.get("target_date_column", ""),
        initial_target_value_column,
    )
    initial_xreg_meta = xreg_series_details(
        state.get("xreg_path", ""),
        state.get("xreg_date_column", ""),
        state.get("xreg_columns", ""),
    )
    target_value_options = render_select_options(initial_target_columns, initial_target_value_column)
    target_date_options = render_date_select_options(
        list(initial_target_meta.get("date_candidates", [])) if isinstance(initial_target_meta, dict) else [],
        state.get("target_date_column", ""),
        initial_target_meta.get("date_column", "") if isinstance(initial_target_meta, dict) else "",
    )
    xreg_date_options = render_date_select_options(
        list(initial_xreg_meta.get("date_candidates", [])) if isinstance(initial_xreg_meta, dict) else [],
        state.get("xreg_date_column", ""),
        initial_xreg_meta.get("date_column", "") if isinstance(initial_xreg_meta, dict) else "",
    )
    target_picker_status = (
        f"Chosen target: {initial_target_value_column}"
        if initial_target_columns
        else "Upload a CSV to load the available variables."
    )
    xreg_summary_html, xreg_picker_html, xreg_picker_status = render_xreg_picker_html(
        list(initial_xreg_meta.get("value_column_details", [])) if isinstance(initial_xreg_meta, dict) else initial_xreg_columns,
        state.get("xreg_columns", ""),
    )
    model_summary, model_picker_html = render_model_picker_html(
        state.get("models", ""),
        DEFAULT_STATE["models"],
    )
    outlier_picker_html, outlier_status = render_outlier_picker_html(
        initial_target_meta.get("outliers", []) if isinstance(initial_target_meta, dict) else [],
        state.get("outlier_dates", ""),
    )
    outlier_panel_class = ""
    if not (initial_target_meta.get("outliers", []) if isinstance(initial_target_meta, dict) else []):
        outlier_panel_class = "no-outliers"
    forecast_end_options_list = forecast_end_options(
        initial_target_meta,
        initial_xreg_meta,
        str(state.get("year_type", "fiscal")),
        state.get("forecast_end_date", ""),
    )
    forecast_end_date = str(state.get("forecast_end_date", "")).strip()
    if not forecast_end_date and forecast_end_options_list:
        forecast_end_date = forecast_end_options_list[-1][0]
    page = INDEX_HTML
    replacements = {
        "__BASE_PATH__": app_base_url(),
        "__BASE_PATH_JSON__": json.dumps(app_base_url()),
        "__TARGET_PATH__": html.escape(str(state["target_path"]), quote=True),
        "__TARGET_DISPLAY_NAME__": html.escape(str(state["target_display_name"]), quote=True),
        "__TARGET_UPLOAD_LABEL__": html.escape(upload_label(state.get("target_display_name"), state.get("target_path")), quote=False),
        "__TARGET_META_HTML__": render_series_meta_html(initial_target_meta or {}),
        "__TARGET_DATE_COLUMN__": html.escape(str(state["target_date_column"]), quote=True),
        "__TARGET_DATE_OPTIONS__": target_date_options,
        "__TARGET_VALUE_COLUMN__": html.escape(initial_target_value_column, quote=True),
        "__OUTLIER_DATES__": html.escape(str(state.get("outlier_dates", "")), quote=True),
        "__OUTLIER_PANEL_CLASS__": outlier_panel_class,
        "__OUTLIER_PICKER_HTML__": outlier_picker_html,
        "__OUTLIER_STATUS__": html.escape(outlier_status, quote=False),
        "__XREG_PATH__": html.escape(str(state["xreg_path"]), quote=True),
        "__XREG_DISPLAY_NAME__": html.escape(str(state["xreg_display_name"]), quote=True),
        "__XREG_UPLOAD_LABEL__": html.escape(upload_label(state.get("xreg_display_name"), state.get("xreg_path")), quote=False),
        "__XREG_META_HTML__": render_series_meta_html(initial_xreg_meta or {}),
        "__XREG_DATE_COLUMN__": html.escape(str(state["xreg_date_column"]), quote=True),
        "__XREG_DATE_OPTIONS__": xreg_date_options,
        "__XREG_COLUMNS__": html.escape(str(state["xreg_columns"]), quote=True),
        "__TARGET_VALUE_OPTIONS__": target_value_options,
        "__TARGET_PICKER_STATUS__": html.escape(target_picker_status, quote=False),
        "__XREG_SUMMARY_HTML__": xreg_summary_html,
        "__XREG_PICKER_HTML__": xreg_picker_html,
        "__XREG_PICKER_STATUS__": html.escape(xreg_picker_status, quote=False),
        "__MODELS__": html.escape(str(state.get("models", DEFAULT_STATE["models"])), quote=True),
        "__MODEL_SUMMARY__": html.escape(model_summary, quote=False),
        "__MODEL_PICKER_HTML__": model_picker_html,
        "__FORECAST_END_OPTIONS__": render_forecast_end_options(forecast_end_options_list, forecast_end_date),
        "__LEVEL__": html.escape(str(state["level"]), quote=True),
        "__SEASON_PERIOD_OPTIONS__": render_season_period_options(state.get("frequency", ""), state.get("season_period", "")),
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
        "__INITIAL_TARGET_COLUMNS_JSON__": json.dumps(initial_target_columns),
        "__INITIAL_TARGET_META_MAP_JSON__": json.dumps(initial_target_meta_map),
        "__INITIAL_XREG_COLUMNS_JSON__": json.dumps(
            list(initial_xreg_meta.get("value_column_details", [])) if isinstance(initial_xreg_meta, dict) else initial_xreg_columns
        ),
        "__INITIAL_TARGET_META_JSON__": json.dumps(initial_target_meta),
        "__INITIAL_XREG_META_JSON__": json.dumps(initial_xreg_meta),
        "__INITIAL_OUTLIERS_JSON__": json.dumps(initial_target_meta.get("outliers", []) if isinstance(initial_target_meta, dict) else []),
        "__FREQUENCY__": html.escape(str((initial_target_meta or {}).get("detected_frequency", state["frequency"])), quote=True),
        "__DETECTED_FREQUENCY_LABEL__": html.escape(str((initial_target_meta or {}).get("detected_frequency_label", frequency_label(str(state["frequency"])))), quote=True),
        "__YEAR_TYPE__": html.escape(str(state["year_type"]), quote=True),
        "__CRITERION__": html.escape(str(state["criterion"]), quote=True),
        "__CONTROL_TOKEN__": json.dumps(shared.CONTROL_TOKEN if control_allowed else ""),
    }
    for key, value in replacements.items():
        page = page.replace(key, value)
    return page


def run_forecast(binary: Path, payload: dict[str, object]) -> dict[str, object]:
    global LATEST_SUMMARY_TEXT, LATEST_FORECAST_CSV
    shared.ensure_scratch_binary(binary, DEFAULT_SCRATCH_TARGET)
    active_model = active_model_from_payload(payload)
    original_target_path = str(payload.get("target_path", DEFAULT_STATE["target_path"]))
    xreg_path = str(payload.get("xreg_path", DEFAULT_STATE["xreg_path"]))
    target_date_column = str(payload.get("target_date_column", DEFAULT_STATE["target_date_column"]))
    xreg_date_column = str(payload.get("xreg_date_column", DEFAULT_STATE["xreg_date_column"]))
    year_type = str(payload.get("year_type", DEFAULT_STATE["year_type"]))
    adjusted_target_path, outlier_note = apply_outlier_handling(
        original_target_path,
        target_date_column,
        payload.get("target_value_column", DEFAULT_STATE["target_value_column"]),
        payload.get("outlier_mode", DEFAULT_STATE["outlier_mode"]),
        payload.get("outlier_dates", DEFAULT_STATE["outlier_dates"]),
    )
    target_path = adjusted_target_path or original_target_path
    target_meta = target_series_details(
        target_path,
        target_date_column,
        payload.get("target_value_column", DEFAULT_STATE["target_value_column"]),
    )
    xreg_meta = xreg_series_details(
        xreg_path,
        xreg_date_column,
        payload.get("xreg_columns", DEFAULT_STATE["xreg_columns"]),
    )
    collinearity_warning = analyse_xreg_collinearity(
        xreg_path,
        xreg_date_column,
        payload.get("xreg_columns", DEFAULT_STATE["xreg_columns"]),
    )
    detected_frequency = str(target_meta.get("detected_frequency") or payload.get("frequency", DEFAULT_STATE["frequency"]))
    if detected_frequency == "unknown":
        detected_frequency = str(payload.get("frequency", DEFAULT_STATE["frequency"]))
    target_end = parse_date_text(target_meta.get("usable_end_date_iso", "")) or parse_date_text(target_meta.get("end_date_iso", ""))
    requested_end = parse_date_text(payload.get("forecast_end_date", "")) or parse_date_text(
        suggested_forecast_end(target_meta, xreg_meta, year_type, payload.get("forecast_end_date", ""))
    )
    if not target_end:
        raise RuntimeError("Ophelia could not detect the end date of the target series.")
    if not requested_end:
        raise RuntimeError("Choose the last date you want the forecast to cover.")
    normalized_end = period_end_for_date(requested_end, detected_frequency, year_type)
    if normalized_end <= target_end:
        raise RuntimeError(
            f"The forecast end date must be after the target series end date of {format_date_uk(target_end)}."
        )
    if xreg_path.strip() and str(payload.get("xreg_columns", "")).strip():
        allowed_end_dates = {
            value for value, _ in forecast_end_options(target_meta, xreg_meta, year_type, "")
        }
        if allowed_end_dates:
            normalized_end_iso = format_date_iso(normalized_end)
            if normalized_end_iso not in allowed_end_dates:
                raise RuntimeError(
                    f"{format_date_uk(normalized_end)} is not a usable exogenous period end for the current driver selection."
                )
        xreg_end = parse_date_text(xreg_meta.get("usable_end_date_iso", "")) or parse_date_text(xreg_meta.get("end_date_iso", ""))
        if xreg_end:
            xreg_limit = period_end_for_date(xreg_end, detected_frequency, year_type)
            if xreg_limit < normalized_end:
                raise RuntimeError(
                    f"The exogenous data ends on {format_date_uk(xreg_limit)}, so forecast until {format_date_uk(normalized_end)} is not possible."
                )
    computed_horizon = periods_between(target_end, normalized_end, detected_frequency, year_type)
    if computed_horizon <= 0:
        raise RuntimeError("Ophelia could not work out a positive forecast length from the chosen end date.")
    command = [
        str(binary),
        "--target", target_path,
        "--target-date-column", target_date_column,
        "--target-value-column", str(payload.get("target_value_column", DEFAULT_STATE["target_value_column"])),
        "--xreg", xreg_path,
        "--xreg-date-column", xreg_date_column,
        "--xreg-cols", str(payload.get("xreg_columns", DEFAULT_STATE["xreg_columns"])),
        "--model", active_model,
        "--frequency", detected_frequency,
        "--year-type", year_type,
        "--horizon", str(computed_horizon),
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
    if data.get("ok"):
        limited_csv, limited_text = limit_forecast_outputs(
            data.get("forecast_csv", ""),
            data.get("forecast_text", ""),
            normalized_end,
        )
        data["forecast_csv"] = limited_csv
        data["forecast_text"] = limited_text
    if data.get("ok") and outlier_note:
        summary = str(data.get("summary_text", "") or "")
        data["summary_text"] = f"{summary}\n{outlier_note}".strip()
    if data.get("ok") and collinearity_warning:
        summary = str(data.get("summary_text", "") or "")
        overlap_note = (
            f"{collinearity_warning.get('title', 'Driver overlap check')}: "
            f"{collinearity_warning.get('message', '')}"
        ).strip()
        if collinearity_warning.get("action"):
            overlap_note = f"{overlap_note} What to do: {collinearity_warning.get('action')}"
        data["summary_text"] = f"{summary}\n{overlap_note}".strip()
    plausibility_warning = analyse_forecast_plausibility(
        data.get("forecast_csv", ""),
        active_model,
    )
    if data.get("ok") and plausibility_warning:
        summary = str(data.get("summary_text", "") or "")
        plausibility_note = (
            f"{plausibility_warning.get('title', 'Forecast plausibility warning')}: "
            f"{plausibility_warning.get('message', '')}"
        ).strip()
        if plausibility_warning.get("action"):
            plausibility_note = f"{plausibility_note} What to do: {plausibility_warning.get('action')}"
        data["summary_text"] = f"{summary}\n{plausibility_note}".strip()
    data["xreg_columns_used"] = ", ".join(
        item.strip() for item in str(payload.get("xreg_columns", "")).split(",") if item.strip()
    )
    data["target_value_column_used"] = str(payload.get("target_value_column", "")).strip()
    data["forecast_end_date"] = format_date_iso(normalized_end)
    data["forecast_end_label"] = format_date_uk(normalized_end)
    data["collinearity_warning"] = collinearity_warning
    data["plausibility_warning"] = plausibility_warning
    data["summary_html"] = render_summary_html(
        data.get("summary_text", ""),
        payload.get("xreg_columns", DEFAULT_STATE["xreg_columns"]),
        collinearity_warning,
        plausibility_warning,
    )
    if data.get("ok"):
        LATEST_SUMMARY_TEXT = str(data.get("summary_text", "") or "")
        LATEST_FORECAST_CSV = str(data.get("forecast_csv", "") or "")
    return data


def run_forecasts(binary: Path, payload: dict[str, object]) -> dict[str, object]:
    global LATEST_SUMMARY_TEXT, LATEST_FORECAST_CSV
    models = selected_models_from_payload(payload)
    if len(models) <= 1:
        single_payload = dict(payload)
        single_payload["models"] = ", ".join(models) if models else str(DEFAULT_STATE["models"])
        return run_forecast(binary, single_payload)

    results: list[dict[str, object]] = []
    for model in models:
        model_payload = dict(payload)
        model_payload["models"] = model
        try:
            result = run_forecast(binary, model_payload)
        except Exception as exc:
            result = {
                "ok": False,
                "model": next((label for value, label in MODEL_OPTIONS if value == model), model),
                "error": str(exc),
                "fit_rows": "-",
                "stationary": False,
                "invertible": False,
                "summary_html": "",
                "summary_text": "",
                "forecast_csv": "",
                "forecast_end_date": str(payload.get("forecast_end_date", "")).strip(),
                "forecast_end_label": str(payload.get("forecast_end_date", "")).strip(),
                "target_value_column_used": str(payload.get("target_value_column", "")).strip(),
                "xreg_columns_used": ", ".join(
                    item.strip() for item in str(payload.get("xreg_columns", "")).split(",") if item.strip()
                ),
            }
        results.append(result)

    successful = [item for item in results if item.get("ok")]
    if not successful:
        errors = [str(item.get("error", "Forecast failed.")) for item in results if not item.get("ok")]
        return {
            "ok": False,
            "multi": True,
            "error": "None of the selected models completed successfully. " + " ".join(errors),
            "results": results,
        }

    combined_summary = build_multi_summary_text(results)
    combined_csv = build_multi_forecast_csv(results)
    combined = dict(successful[0])
    combined["multi"] = True
    combined["results"] = results
    combined["model"] = f"{len(results)} selected"
    combined["fit_rows"] = "Varies"
    combined["stationary"] = False
    combined["invertible"] = False
    combined["summary_text"] = combined_summary
    combined["summary_html"] = render_multi_summary_html(results)
    combined["forecast_csv"] = combined_csv
    combined["forecast_end_date"] = str(successful[0].get("forecast_end_date", "") or "").strip()
    combined["forecast_end_label"] = str(successful[0].get("forecast_end_label", "") or "").strip()
    combined["target_value_column_used"] = str(payload.get("target_value_column", "")).strip()
    combined["xreg_columns_used"] = ", ".join(
        item.strip() for item in str(payload.get("xreg_columns", "")).split(",") if item.strip()
    )
    LATEST_SUMMARY_TEXT = combined_summary
    LATEST_FORECAST_CSV = combined_csv
    return combined


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

    def send_download_text(self, filename: str, payload: str, content_type: str) -> None:
        data = payload.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_file(self, path: Path, content_type: str) -> None:
        try:
            data = path.read_bytes()
        except OSError:
            self.send_error(404)
            return
        self.send_bytes(200, data, content_type)

    def send_transport_result(self, payload: dict[str, object]) -> None:
        message = json.dumps(payload)
        page = (
            "<!doctype html><html><body><script>"
            f"const data={message};"
            "const doc=window.parent && window.parent.document;"
            "if(doc){"
            "let applied=false;"
            "if(window.parent && typeof window.parent.__opheliaApplyForecastResult==='function'){"
            "try{window.parent.__opheliaApplyForecastResult(data);applied=true;}catch(_err){applied=false;}"
            "}"
            "if(!applied){"
            "const status=doc.getElementById('status');"
            "const button=doc.getElementById('run-button');"
            "const summaryBox=doc.getElementById('summary-box');"
            "const metricModel=doc.getElementById('metric-model');"
            "const metricFitRows=doc.getElementById('metric-fit-rows');"
            "const metricStationary=doc.getElementById('metric-stationary');"
            "const metricInvertible=doc.getElementById('metric-invertible');"
            "const forecastHead=doc.getElementById('forecast-head');"
            "const forecastBody=doc.getElementById('forecast-body');"
            "const downloadSummary=doc.getElementById('download-summary');"
            "const downloadForecast=doc.getElementById('download-forecast');"
            "const driversUsedNote=doc.getElementById('drivers-used-note');"
            "if(window.parent){window.parent.latestSummary=data.summary_text||'';window.parent.latestForecastCsv=data.forecast_csv||'';window.parent.__opheliaLatestSummary=data.summary_text||'';window.parent.__opheliaLatestForecastCsv=data.forecast_csv||'';}"
            "if(button) button.disabled=false;"
            "if(!data.ok){if(status){status.textContent=data.error||'Forecast failed.';status.className='status error';}}"
            "else {"
            "if(metricModel) metricModel.textContent=data.model||'Forecast';"
            "if(metricFitRows) metricFitRows.textContent=String(data.fit_rows ?? '-');"
            "if(metricStationary) metricStationary.textContent=data.stationary?'Yes':'No';"
            "if(metricInvertible) metricInvertible.textContent=data.invertible?'Yes':'No';"
            "if(summaryBox){if(data.summary_html) summaryBox.innerHTML=data.summary_html; else summaryBox.textContent=data.summary_text||'No summary returned.';}"
            "if(driversUsedNote) driversUsedNote.textContent='Drivers used in this run: '+(data.xreg_columns_used||'(none)');"
            "if(forecastHead && forecastBody){"
            "const text=String(data.forecast_csv||'').trim();"
            "if(!text){forecastHead.innerHTML='<tr><th>Date</th><th>Actual</th><th>Mean</th><th>StdErr</th><th>Lower</th><th>Upper</th></tr>';forecastBody.innerHTML='<tr><td colspan=\"6\">No forecast rows returned.</td></tr>';}"
            "else {const lines=text.split(/\\r?\\n/).filter(Boolean);const header=(lines[0]||'').split(',');const rows=lines.slice(1).map(line=>line.split(','));forecastHead.innerHTML='<tr>'+header.map(cell=>'<th>'+cell+'</th>').join('')+'</tr>';forecastBody.innerHTML=rows.length?rows.map(row=>'<tr>'+header.map((_,i)=>'<td>'+(row[i]||'')+'</td>').join('')+'</tr>').join(''):'<tr><td colspan=\"'+(header.length||1)+'\">No forecast rows returned.</td></tr>';}"
            "}"
            "if(downloadSummary){downloadSummary.classList.toggle('disabled',!(data.summary_text||''));downloadSummary.setAttribute('aria-disabled',(data.summary_text||'')?'false':'true');}"
            "if(downloadForecast){downloadForecast.classList.toggle('disabled',!(data.forecast_csv||''));downloadForecast.setAttribute('aria-disabled',(data.forecast_csv||'')?'false':'true');}"
            "if(status){status.textContent='Forecast complete.';status.className='status ok';}"
            "}"
            "}"
            "}"
            "</script></body></html>"
        ).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(page)))
        self.end_headers()
        self.wfile.write(page)

    def do_GET(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        path = request_path(parsed.path)
        if path == "/state":
            self.send_json(200, load_state())
            return
        if path == "/target-columns":
            params = urllib.parse.parse_qs(parsed.query, keep_blank_values=True)
            csv_path = resolve_lab_path(params.get("path", [""])[0])
            date_column = params.get("date_column", [""])[0]
            value_column = params.get("value_column", [""])[0]
            selected_columns = params.get("selected_columns", [""])[0]
            if not csv_path or not csv_path.exists():
                self.send_json(404, {"ok": False, "error": "Target CSV not found."})
                return
            try:
                if value_column:
                    self.send_json(200, {"ok": True, **target_series_details(csv_path, date_column or None, value_column)})
                elif selected_columns:
                    self.send_json(200, {"ok": True, **xreg_series_details(csv_path, date_column or None, selected_columns)})
                else:
                    self.send_json(200, {"ok": True, **csv_header_details(csv_path, date_column or None)})
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
        if path == "/download-summary":
            if not LATEST_SUMMARY_TEXT:
                self.send_error(404, "No summary is available yet.")
                return
            self.send_download_text("ophelia-summary.txt", LATEST_SUMMARY_TEXT, "text/plain; charset=utf-8")
            return
        if path == "/download-forecast":
            if not LATEST_FORECAST_CSV:
                self.send_error(404, "No forecast is available yet.")
                return
            self.send_download_text("ophelia-forecast.csv", LATEST_FORECAST_CSV, "text/csv; charset=utf-8")
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
        if path == "/forecast-form":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                raw = self.rfile.read(length).decode("utf-8")
                parsed = urllib.parse.parse_qs(raw, keep_blank_values=True)
                payload = {key: values[-1] if values else "" for key, values in parsed.items()}
                if "xreg-column-choice" in parsed:
                    payload["xreg_columns"] = ", ".join(
                        value.strip() for value in parsed.get("xreg-column-choice", []) if value.strip()
                    )
                if "outlier-choice" in parsed:
                    payload["outlier_dates"] = ", ".join(
                        value.strip() for value in parsed.get("outlier-choice", []) if value.strip()
                    )
            except Exception as exc:
                self.send_transport_result({"ok": False, "error": f"Bad request: {exc}"})
                return
            save_state(payload)
            try:
                data = run_forecasts(self.binary, payload)
            except Exception as exc:
                self.send_transport_result({"ok": False, "error": str(exc)})
                return
            self.send_transport_result(data)
            return

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
                if path == "/upload-xreg":
                    header_info = xreg_series_details(upload_path, "", "")
                else:
                    header_info = csv_header_details(upload_path)
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": str(exc)})
                return

            self.send_json(200, {
                "ok": True,
                "path": relative_display_path(upload_path),
                "original_name": Path(getattr(field, "filename", upload_path.name)).name,
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
            data = run_forecasts(self.binary, payload)
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
