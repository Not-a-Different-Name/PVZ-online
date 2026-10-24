// @pvz-online: finalize seat-dependent rules once, without reconstructing the player's lawn.
bool LawnApp::FinalizePreparationSeats() {
    if(!mOnlineSession || !mOnlineSession->IsPreparationActive())return true;
    const auto phase=mOnlineSession->GetPreparationPhase();
    if(!mBoard || (phase!=PreparationWire::GO && phase!=PreparationWire::SEAL))return false;
    if(!mRunState)return true;
    const uint32_t revision=mOnlineSession->GetPreparationRevision();
    if(mPreparationConfiguredRevision!=revision) {
        mPreparationConfiguredRevision=revision;mPreparationRulesApplied=false;mPreparationSaved=false;
    }
    if(!mPreparationRulesApplied) {
        auto& admission=mRunState->mAdmission;
        // @pvz-online: a cancelled seal can change seats again; replace the unopened balance each revision.
        if(admission.phase==Admission::PREPARED)mBoard->mSunMoney=50+OnlineStartSunBonus();
        const int seat=mOnlineSession->GetLocalSeat();
        const std::string random=Sexy::GetRandState();
        if(mPreparationBuiltSeat!=seat || mPreparationBuiltMultiplier!=OnlineSeatMultiplier()) {
            // Endless keeps its existing sunlight; finite levels replace their initial bonus once.
            if(!mRunState->IsEndless()) {
                mBoard->mSunMoney+=OnlineStartSunBonus()-mPreparationBuiltSunBonus;
                mPreparationBuiltSunBonus=OnlineStartSunBonus();
            }
            // Rewards, health, cooldowns and seed selections remain in the existing objects.
            mBoard->InitZombieWaves();
        }
        // Removing an unused mower is not consumption, and must not trigger bonus-mower rewards.
        LawnMower* mower=nullptr;
#ifdef __ANDROID__
        for(LawnMower* mower:mBoard->mLawnMowers) {
#else
        while(mBoard->mLawnMowers.IterateNext(mower)) {
#endif
            RemoveReanimation(mower->mReanimID);
            mBoard->mLawnMowers.DataArrayFree(mower);
        }
        mBoard->InitLawnMowers();
        // Endless may already have played its roll-in animation before entering preparation.
        mower=nullptr;
#ifdef __ANDROID__
        for(LawnMower* mower:mBoard->mLawnMowers) {
#else
        while(mBoard->mLawnMowers.IterateNext(mower)) {
#endif
            mower->mVisible=true;
            if(mRunState->IsEndless())mower->mPosX=-21.0f;
        }
        Sexy::RestoreRandState(random);
        mPreparationBuiltSeat=seat;mPreparationBuiltMultiplier=OnlineSeatMultiplier();mPreparationRulesApplied=true;
    }
    if(!mPreparationSaved) {
        if(!EndlessLawn::Capture(mBoard,true) || !mRunState->Save(mPlayerInfo->mId)) {
            mOnlineSession->PostNotice(ModText::Tr("保存失败，请检查空间后重试。","Save failed. Check storage and retry."));
            return false;
        }
        mPreparationSaved=true;
    }
    return true;
}
