#pragma once

// Called only after AddPlant succeeded. Preview/save restoration paths never
// trigger this placement effect; an Imitater chosen as Garlic triggers once.
namespace RunGarlicPulse {
inline void OnPlanted(Board* board, Plant* plant)
{
    if (!board || !plant || board->mApp->RunExtraBuffCount(RUN_BUFF2_GARLIC_PULSE) <= 0)
        return;
    // Morph rebuilds Garlic through AddPlant with SEED_IMITATER as its marker.
    // The original Imitater already pulsed on placement; rebuilding must not pulse again.
    if (plant->mImitaterType == SeedType::SEED_IMITATER) return;
    const bool garlic = plant->mSeedType == SeedType::SEED_GARLIC ||
        (plant->mSeedType == SeedType::SEED_IMITATER && plant->mImitaterType == SeedType::SEED_GARLIC);
    if (!garlic) return;

    Zombie* zombie = nullptr;
    while (board->IterateZombies(zombie))
    {
        if (zombie->mRow != plant->mRow || zombie->IsDeadOrDying() || zombie->mMindControlled ||
            zombie->mControlImmune || board->IsRunBossZombie(zombie) || zombie->mZombieType == ZombieType::ZOMBIE_BOSS)
            continue;

        // Keep the normal pool/land boundaries and avoid SwitchLanes' assert
        // when a custom board has no legal adjacent row.
        const bool pool = board->mPlantRow[zombie->mRow] == PlantRowType::PLANTROW_POOL;
        bool canSwitch = false;
        for (int direction : {-1, 1})
        {
            const int row = zombie->mRow + direction;
            if (board->RowCanHaveZombies(row) &&
                (board->mPlantRow[row] == PlantRowType::PLANTROW_POOL) == pool)
                canSwitch = true;
        }
        if (canSwitch) zombie->SwitchLanes();
    }
}
}
