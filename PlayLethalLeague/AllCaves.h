#pragma once
#include "CodeCave.h"

DEFINE_CODECAVE(DevCave);
DEFINE_CODECAVE_KEEPORIG(BallCave);
DEFINE_CODECAVE(GameRulesCave);
DEFINE_CODECAVE(StageCave)
DEFINE_CODECAVE(PlayerCave);
DEFINE_CODECAVE(PlayerSpawnCave);
DEFINE_CODECAVE(ResetCave);

// stuff relating to inputs
DEFINE_CODECAVE_KEEPORIG(InputHeldCave);
DEFINE_CODECAVE_KEEPORIG(InputPressedCave);

// Just delete these (nop)
DEFINE_CODECAVE(WindowUnfocusCave);
DEFINE_CODECAVE(WindowRefocusCave);