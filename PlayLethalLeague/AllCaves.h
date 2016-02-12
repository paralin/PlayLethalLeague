#pragma once
#include "CodeCave.h"
#include "PatternScan.h"

DEFINE_PATTERNSCAN(InputUpdatePattern, "InputUpdate");

DEFINE_CODECAVE(DevCave, "DevCave");
DEFINE_CODECAVE_KEEPORIG(BallCave, "BallCave");
DEFINE_CODECAVE(GameRulesCave, "GameRulesCave");
DEFINE_CODECAVE(StageCave, "StageCave")
DEFINE_CODECAVE(PlayerCave, "PlayerCave");
DEFINE_CODECAVE(PlayerSpawnCave, "PlayerSpawnCave");
DEFINE_CODECAVE(ResetCave, "ResetCave");

// stuff relating to inputs
DEFINE_CODECAVE_KEEPORIG(InputHeldCave, "InputHeldCave");
DEFINE_CODECAVE_KEEPORIG(InputPressedCave, "InputPressedCave");

// Just delete these (nop)
DEFINE_CODECAVE(WindowUnfocusCave, "WindowUnfocusCave");
DEFINE_CODECAVE(WindowRefocusCave, "WindowRefocusCave");
