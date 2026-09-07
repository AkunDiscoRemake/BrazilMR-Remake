#!/usr/bin/env bash
# BrazilMR — baixa o modelo de hand tracking do MediaPipe.
#
# O hand tracking é OPCIONAL: sem o modelo, a plataforma usa o ponteiro
# de cabeça (0DoF gaze) e controller — nada quebra.
#
# Uso:  tools/fetch_models.sh
set -euo pipefail

DEST="$(dirname "$0")/../app/src/main/assets"
URL="https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"

mkdir -p "$DEST"
echo "Baixando hand_landmarker.task (MediaPipe, ~7.8 MB)..."
if command -v curl >/dev/null 2>&1; then
  curl -L --fail -o "$DEST/hand_landmarker.task" "$URL"
elif command -v wget >/dev/null 2>&1; then
  wget -O "$DEST/hand_landmarker.task" "$URL"
else
  echo "ERRO: precisa de curl ou wget." >&2
  exit 1
fi

echo "OK: $DEST/hand_landmarker.task"
echo "O APK incluirá o modelo automaticamente (pasta assets/)."
