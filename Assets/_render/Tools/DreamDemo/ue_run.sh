#!/usr/bin/env bash
# Launch helpers for the DreamDemo Unreal scripts (Git Bash).
#   ue_run.sh cmd    <script.py> [log name]      headless commandlet (asset work, no rendering)
#   ue_run.sh editor <script.py> [log name]      full editor, tick-driven script (screenshots)
#   ue_run.sh game   <map> [extra args...]       standalone game window
# Paths are resolved from this file; override the engine with UE_ROOT.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -W 2>/dev/null || pwd)"
PROJECT_DIR="$(cd "$HERE/../.." && pwd -W 2>/dev/null || pwd)"
UPROJECT="$PROJECT_DIR/TDKProps.uproject"
UE_ROOT="${UE_ROOT:-C:/Program Files/Epic Games/UE_5.8}"
MODE="${1:?mode}"; shift

case "$MODE" in
  cmd)
    SCRIPT="$(cd "$(dirname "$1")" && pwd -W)/$(basename "$1")"; LOG="${2:-dd_cmd}"
    "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$UPROJECT" -run=pythonscript \
      -script="$SCRIPT" -unattended -nosplash -nullrhi -log="$LOG.log" > /dev/null 2>&1
    ;;
  editor)
    SCRIPT="$(cd "$(dirname "$1")" && pwd -W)/$(basename "$1")"; LOG="${2:-dd_editor}"
    UE_PYTHONPATH="$HERE/ue/autorun" DREAMDEMO_AUTORUN="$SCRIPT" \
      "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor.exe" "$UPROJECT" -nosplash -unattended \
      -log="$LOG.log" > /dev/null 2>&1
    ;;
  game)
    MAP="$1"; shift
    # MSYS would rewrite /Game/... into a Windows path under the Git install.
    MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor.exe" "$UPROJECT" "$MAP" -game -windowed \
      -ResX=1920 -ResY=1080 -nosplash "$@" > /dev/null 2>&1
    ;;
  *) echo "unknown mode $MODE"; exit 2 ;;
esac
echo "exit $?"
