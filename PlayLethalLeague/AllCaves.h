#pragma once
#include "CodeCave.h"

DEFINE_CODECAVE(DevCave, "DevCave");
DEFINE_CODECAVE_KEEPORIG(BallCave, "BallCave");
DEFINE_CODECAVE_KEEPORIG(GameRulesCave, "GameRulesCave");
DEFINE_CODECAVE_KEEPORIG(StageCave, "StageCave")
DEFINE_CODECAVE(PlayerCave, "PlayerCave");
DEFINE_CODECAVE(PlayerSpawnCave, "PlayerSpawnCave");
DEFINE_CODECAVE_KEEPORIG(ResetCave, "ResetCave");

// stuff relating to inputs
DEFINE_CODECAVE_KEEPORIG(InputHeldCave, "InputHeldCave");
DEFINE_CODECAVE_KEEPORIG(InputPressedCave, "InputPressedCave");

// Just delete these (nop)
DEFINE_CODECAVE(WindowUnfocusCave, "WindowUnfocusCave");
DEFINE_CODECAVE(WindowRefocusCave, "WindowRefocusCave");

DEFINE_CODECAVE_KEEPORIG(StartOfFrameCave, "StartOfFrameCave");
DEFINE_CODECAVE(DeathCave, "DeathCave");

DEFINE_CODECAVE(OfflineInputsCave, "OfflineInputsCave");
DEFINE_CODECAVE_KEEPORIG(OnlineInputsCave, "OnlineInputsCave");

DEFINE_CODECAVE_KEEPORIG(Reset2Cave, "Reset2Cave");