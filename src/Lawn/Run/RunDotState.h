#pragma once
// Kept outside raw Zombie/Board save suffixes; optional trailer preserves old saves.
struct RunDotState {
    int butterFrames=0, butterTick=0, scorchTick=0, emberFrames=0, emberTick=0;
    unsigned long long scorchPlayer=0,scorchStrike=0;
    int scorchStacks=0,scorchMultiplier=1000;
};
