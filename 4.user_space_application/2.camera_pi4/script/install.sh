#!/bin/bash
set -e

echo "[1/3] Install system dependencies"
sudo apt update
sudo apt install -y \
    python3-pip \
    python3-dev \
    libopenblas-dev \
    liblapack-dev \
    libjpeg-dev \
    libtiff5-dev

echo "[2/3] Upgrade pip"
python3 -m pip install --upgrade pip setuptools wheel

echo "[3/3] Install Python requirements"
python3 -m pip install -r requirements.txt
