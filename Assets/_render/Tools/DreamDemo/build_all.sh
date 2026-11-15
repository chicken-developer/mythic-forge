#!/usr/bin/env bash
# Rebuild every DreamDemo asset and the demo map, in dependency order.
# Step 3 rebuilds MF_DreamDarknessMask, and the materials of step 5 call that function,
# so step 5 must always be rerun after step 3. Step 8 must follow step 4.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$(cd "$HERE/../.." && pwd)"
for step in step3_assets_materials step4_lighting_states step5_dream_vfx step6_night step8_dream_gate step7_checks; do
  bash "$HERE/ue_run.sh" cmd "$HERE/ue/$step.py" "dd_$step" > /dev/null
  if grep -q "DREAMDEMO .* done\|DREAMDEMO step7 checks" "$PROJECT/Saved/Logs/dd_$step.log"; then
    echo "OK   $step"
  else
    echo "FAIL $step (see Saved/Logs/dd_$step.log)"
    grep -E "Traceback|Error:" "$PROJECT/Saved/Logs/dd_$step.log" | grep -v "LogInit\|LogTemp" | head -8
    exit 1
  fi
done
