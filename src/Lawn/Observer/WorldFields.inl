template<class IO> void Fields(IO& io,GameObject& v){
 io.template Scalar<int32_t>(v.mX);
 io.template Scalar<int32_t>(v.mY);
 io.template Scalar<int32_t>(v.mWidth);
 io.template Scalar<int32_t>(v.mHeight);
 io.template Scalar<bool>(v.mVisible);
 io.template Scalar<int32_t>(v.mRow);
 io.template Scalar<int32_t>(v.mRenderOrder);
}
template<class IO> void Fields(IO& io,Plant& v){
 Fields(io,static_cast<GameObject&>(v));
 io.Value(v.mSeedType);
 io.template Scalar<int32_t>(v.mPlantCol);
 io.template Scalar<int32_t>(v.mAnimCounter);
 io.template Scalar<int32_t>(v.mFrame);
 io.template Scalar<int32_t>(v.mFrameLength);
 io.template Scalar<int32_t>(v.mNumFrames);
 io.Value(v.mState);
 io.template Scalar<int32_t>(v.mPlantHealth);
 io.template Scalar<int32_t>(v.mPlantMaxHealth);
 io.template Scalar<int32_t>(v.mSubclass);
 io.template Scalar<int32_t>(v.mDisappearCountdown);
 io.template Scalar<int32_t>(v.mDoSpecialCountdown);
 io.template Scalar<int32_t>(v.mStateCountdown);
 io.template Scalar<int32_t>(v.mLaunchCounter);
 io.template Scalar<int32_t>(v.mLaunchRate);
 io.Value(v.mPlantRect);
 io.Value(v.mPlantAttackRect);
 io.template Scalar<int32_t>(v.mTargetX);
 io.template Scalar<int32_t>(v.mTargetY);
 io.template Scalar<int32_t>(v.mStartRow);
 if(io.reading)v.mParticleID=PARTICLESYSTEMID_NULL;
 io.template Scalar<int32_t>(v.mShootingCounter);
 io.Value(v.mBodyReanimID);
 io.Value(v.mHeadReanimID);
 io.Value(v.mHeadReanimID2);
 io.Value(v.mHeadReanimID3);
 io.Value(v.mBlinkReanimID);
 io.Value(v.mLightReanimID);
 io.Value(v.mSleepingReanimID);
 io.template Scalar<int32_t>(v.mBlinkCountdown);
 io.template Scalar<int32_t>(v.mRecentlyEatenCountdown);
 io.template Scalar<int32_t>(v.mEatenFlashCountdown);
 io.template Scalar<int32_t>(v.mBeghouledFlashCountdown);
 io.template Scalar<float>(v.mShakeOffsetX);
 io.template Scalar<float>(v.mShakeOffsetY);
 io.Value(v.mMagnetItems);
 io.Value(v.mTargetZombieID);
 io.template Scalar<int32_t>(v.mWakeUpCounter);
 io.Value(v.mOnBungeeState);
 io.Value(v.mImitaterType);
 io.template Scalar<int32_t>(v.mPottedPlantIndex);
 io.template Scalar<bool>(v.mAnimPing);
 io.template Scalar<bool>(v.mDead);
 io.template Scalar<bool>(v.mSquished);
 io.template Scalar<bool>(v.mIsAsleep);
 io.template Scalar<bool>(v.mIsOnBoard);
 io.template Scalar<bool>(v.mHighlighted);
 io.Value(v.mExtraTanglekelpIDs);
 io.template Scalar<int32_t>(v.mExtraTanglekelpCount);
 io.template Scalar<int32_t>(v.mSquashSmashesLeft);
 io.template Scalar<float>(v.mSquashFromX);
 io.template Scalar<float>(v.mSquashFromY);
 io.template Scalar<int32_t>(v.mRunDispatchCursor);
 io.template Scalar<int32_t>(v.mRunAliveTicks);
 io.template Scalar<int32_t>(v.mRunGarlicCooldown);
 io.template Scalar<int32_t>(v.mRunPurpleShots);
 io.template Scalar<int32_t>(v.mRunSunCollectTicks);
 io.template Scalar<int32_t>(v.mMineregrowsLeft);
 io.template Scalar<int32_t>(v.mChainBitesLeft);
}
template<class IO> void Fields(IO& io,Zombie& v){
 Fields(io,static_cast<GameObject&>(v));
 io.Value(v.mZombieType);
 io.Value(v.mZombiePhase);
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<float>(v.mVelX);
 io.template Scalar<int32_t>(v.mAnimCounter);
 io.template Scalar<int32_t>(v.mGroanCounter);
 io.template Scalar<int32_t>(v.mAnimTicksPerFrame);
 io.template Scalar<int32_t>(v.mAnimFrames);
 io.template Scalar<int32_t>(v.mFrame);
 io.template Scalar<int32_t>(v.mPrevFrame);
 io.template Scalar<bool>(v.mVariant);
 io.template Scalar<bool>(v.mIsEating);
 io.template Scalar<int32_t>(v.mJustGotShotCounter);
 io.template Scalar<int32_t>(v.mShieldJustGotShotCounter);
 io.template Scalar<int32_t>(v.mShieldRecoilCounter);
 io.template Scalar<int32_t>(v.mZombieAge);
 io.Value(v.mZombieHeight);
 io.template Scalar<int32_t>(v.mPhaseCounter);
 io.template Scalar<int32_t>(v.mFromWave);
 io.template Scalar<bool>(v.mDroppedLoot);
 io.template Scalar<int32_t>(v.mZombieFade);
 io.template Scalar<bool>(v.mFlatTires);
 io.template Scalar<int32_t>(v.mUseLadderCol);
 io.template Scalar<int32_t>(v.mTargetCol);
 io.template Scalar<float>(v.mAltitude);
 io.template Scalar<bool>(v.mHitUmbrella);
 io.Value(v.mZombieRect);
 io.Value(v.mZombieAttackRect);
 io.template Scalar<int32_t>(v.mChilledCounter);
 io.template Scalar<int32_t>(v.mButteredCounter);
 io.template Scalar<int32_t>(v.mIceTrapCounter);
 io.template Scalar<int32_t>(v.mRunBarbedCounter);
 io.template Scalar<int32_t>(v.mFetidSlowCounter);
 io.template Scalar<int32_t>(v.mScorchedCounter);
 io.template Scalar<int32_t>(v.mFrostArmorCounter);
 io.template Scalar<bool>(v.mIceShroomTrap);
 io.template Scalar<bool>(v.mControlImmune);
 io.template Scalar<bool>(v.mHeartguardUsed);
 io.template Scalar<bool>(v.mMindControlled);
 io.template Scalar<bool>(v.mBlowingAway);
 io.template Scalar<bool>(v.mHasHead);
 io.template Scalar<bool>(v.mHasArm);
 io.template Scalar<bool>(v.mHasObject);
 io.template Scalar<bool>(v.mInPool);
 io.template Scalar<bool>(v.mOnHighGround);
 io.template Scalar<bool>(v.mYuckyFace);
 io.template Scalar<int32_t>(v.mYuckyFaceCounter);
 io.Value(v.mHelmType);
 io.template Scalar<int32_t>(v.mBodyHealth);
 io.template Scalar<int32_t>(v.mBodyMaxHealth);
 io.template Scalar<int32_t>(v.mHelmHealth);
 io.template Scalar<int32_t>(v.mHelmMaxHealth);
 io.Value(v.mShieldType);
 io.template Scalar<int32_t>(v.mShieldHealth);
 io.template Scalar<int32_t>(v.mShieldMaxHealth);
 io.template Scalar<int32_t>(v.mFlyingHealth);
 io.template Scalar<int32_t>(v.mFlyingMaxHealth);
 io.template Scalar<bool>(v.mDead);
 io.Value(v.mRelatedZombieID);
 io.Value(v.mFollowerZombieID);
 io.template Scalar<bool>(v.mPlayingSong);
 io.template Scalar<int32_t>(v.mParticleOffsetX);
 io.template Scalar<int32_t>(v.mParticleOffsetY);
 io.Value(v.mAttachmentID);
 io.template Scalar<int32_t>(v.mSummonCounter);
 io.Value(v.mBodyReanimID);
 io.template Scalar<float>(v.mScaleZombie);
 io.template Scalar<float>(v.mVelZ);
 io.template Scalar<float>(v.mOriginalAnimRate);
 io.Value(v.mTargetPlantID);
 io.template Scalar<int32_t>(v.mBossMode);
 io.template Scalar<int32_t>(v.mTargetRow);
 io.template Scalar<int32_t>(v.mBossBungeeCounter);
 io.template Scalar<int32_t>(v.mBossStompCounter);
 io.template Scalar<int32_t>(v.mBossHeadCounter);
 io.Value(v.mBossFireBallReanimID);
 io.Value(v.mSpecialHeadReanimID);
 io.Value(v.mPaperHelmReanimID);
 io.Value(v.mZombatarHeadReanimID);
 io.template Scalar<int32_t>(v.mFireballRow);
 io.template Scalar<bool>(v.mIsFireBall);
 io.Value(v.mMoweredReanimID);
 io.template Scalar<int32_t>(v.mLastPortalX);
 bool hasZombatar=false;io.Value(hasZombatar);
 int zombatarIndex=-1;io.Value(zombatarIndex);
}
template<class IO> void Fields(IO& io,Projectile& v){
 Fields(io,static_cast<GameObject&>(v));
 io.template Scalar<int32_t>(v.mFrame);
 io.template Scalar<int32_t>(v.mNumFrames);
 io.template Scalar<int32_t>(v.mAnimCounter);
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<float>(v.mPosZ);
 io.template Scalar<float>(v.mVelX);
 io.template Scalar<float>(v.mVelY);
 io.template Scalar<float>(v.mVelZ);
 io.template Scalar<float>(v.mAccZ);
 io.template Scalar<float>(v.mShadowY);
 io.template Scalar<bool>(v.mDead);
 io.template Scalar<int32_t>(v.mAnimTicksPerFrame);
 io.Value(v.mMotionType);
 io.Value(v.mProjectileType);
 io.template Scalar<int32_t>(v.mProjectileAge);
 io.template Scalar<int32_t>(v.mClickBackoffCounter);
 io.template Scalar<float>(v.mRotation);
 io.template Scalar<float>(v.mRotationSpeed);
 io.template Scalar<bool>(v.mOnHighGround);
 io.template Scalar<int32_t>(v.mDamageRangeFlags);
 io.template Scalar<int32_t>(v.mHitTorchwoodGridX);
 io.Value(v.mAttachmentID);
 io.template Scalar<float>(v.mCobTargetX);
 io.template Scalar<int32_t>(v.mCobTargetRow);
 io.Value(v.mTargetZombieID);
 io.template Scalar<int32_t>(v.mLastPortalX);
 io.template Scalar<int32_t>(v.mPricklyHitsLeft);
 io.Value(v.mPricklyHitIds);
 io.template Scalar<int32_t>(v.mPricklyHitCount);
 io.template Scalar<bool>(v.mRunIceSource);
 io.template Scalar<bool>(v.mRunCactusSource);
 io.template Scalar<bool>(v.mRunScaredySource);
 io.template Scalar<int32_t>(v.mRunCrystalDamage);
 io.Value(v.mRunSourceSeed);
 io.template Scalar<int32_t>(v.mRunHuntHitsLeft);
 io.template Scalar<int32_t>(v.mRunLaunchDelay);
 io.template Scalar<bool>(v.mRunIceWave);
 if(v.mRunIceWave)io.Sparse(v.mRunWaveHitIds);else if(io.reading)for(auto&x:v.mRunWaveHitIds)x=ZOMBIEID_NULL;
}
template<class IO> void Fields(IO& io,Coin& v){
 Fields(io,static_cast<GameObject&>(v));
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<float>(v.mVelX);
 io.template Scalar<float>(v.mVelY);
 io.template Scalar<float>(v.mScale);
 io.template Scalar<bool>(v.mDead);
 io.template Scalar<int32_t>(v.mFadeCount);
 io.template Scalar<float>(v.mCollectX);
 io.template Scalar<float>(v.mCollectY);
 io.template Scalar<int32_t>(v.mGroundY);
 io.template Scalar<int32_t>(v.mCoinAge);
 io.template Scalar<bool>(v.mIsBeingCollected);
 io.template Scalar<int32_t>(v.mDisappearCounter);
 io.Value(v.mType);
 io.Value(v.mCoinMotion);
 io.Value(v.mAttachmentID);
 io.template Scalar<float>(v.mCollectionDistance);
 io.Value(v.mUsableSeedType);
 io.Value(v.mPottedPlantSpec);
 io.template Scalar<bool>(v.mNeedsBouncyArrow);
 io.template Scalar<bool>(v.mHasBouncyArrow);
 io.template Scalar<bool>(v.mHitGround);
 io.template Scalar<int32_t>(v.mTimesDropped);
 io.template Scalar<bool>(v.mVisualOnly);
 io.template Scalar<int32_t>(v.mRunSunBonus);
}
template<class IO> void Fields(IO& io,LawnMower& v){
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<int32_t>(v.mRenderOrder);
 io.template Scalar<int32_t>(v.mRow);
 io.template Scalar<int32_t>(v.mAnimTicksPerFrame);
 io.Value(v.mReanimID);
 io.template Scalar<int32_t>(v.mChompCounter);
 io.template Scalar<int32_t>(v.mRollingInCounter);
 io.template Scalar<int32_t>(v.mSquishedCounter);
 io.Value(v.mMowerState);
 io.template Scalar<bool>(v.mDead);
 io.template Scalar<bool>(v.mVisible);
 io.Value(v.mMowerType);
 io.template Scalar<float>(v.mAltitude);
 io.Value(v.mMowerHeight);
 io.template Scalar<int32_t>(v.mLastPortalX);
}
template<class IO> void Fields(IO& io,GridItem& v){
 io.Value(v.mGridItemType);
 io.Value(v.mGridItemState);
 io.template Scalar<int32_t>(v.mGridX);
 io.template Scalar<int32_t>(v.mGridY);
 io.template Scalar<int32_t>(v.mGridItemCounter);
 io.template Scalar<int32_t>(v.mRenderOrder);
 io.template Scalar<bool>(v.mDead);
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<float>(v.mGoalX);
 io.template Scalar<float>(v.mGoalY);
 io.Value(v.mGridItemReanimID);
 if(io.reading)v.mGridItemParticleID=PARTICLESYSTEMID_NULL;
 io.Value(v.mZombieType);
 io.Value(v.mSeedType);
 io.Value(v.mScaryPotType);
 io.template Scalar<bool>(v.mHighlighted);
 io.template Scalar<int32_t>(v.mTransparentCounter);
 io.template Scalar<int32_t>(v.mSunCount);
 io.Value(v.mMotionTrailFrames);
 io.template Scalar<int32_t>(v.mMotionTrailCount);
}
template<class IO> void Fields(IO& io,MagnetItem& v){
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<float>(v.mDestOffsetX);
 io.template Scalar<float>(v.mDestOffsetY);
 io.Value(v.mItemType);
}
template<class IO> void Fields(IO& io,MotionTrailFrame& v){
 io.template Scalar<float>(v.mPosX);
 io.template Scalar<float>(v.mPosY);
 io.template Scalar<float>(v.mAnimTime);
}
template<class IO> void SeedPacketFields(IO& io,SeedPacket& v){
 io.template Scalar<int32_t>(v.mRefreshCounter);
 io.template Scalar<int32_t>(v.mRefreshTime);
 io.template Scalar<int32_t>(v.mIndex);
 io.template Scalar<int32_t>(v.mOffsetX);
 io.Value(v.mPacketType);
 io.Value(v.mImitaterType);
 io.template Scalar<int32_t>(v.mSlotMachineCountDown);
 io.Value(v.mSlotMachiningNextSeed);
 io.template Scalar<float>(v.mSlotMachiningPosition);
 io.template Scalar<bool>(v.mActive);
 io.template Scalar<bool>(v.mRefreshing);
 io.template Scalar<int32_t>(v.mTimesUsed);
}
// Empty slots have no imitater/spin/cooldown state. Some normal seed-selection
// paths leave stale values there; never transmit them or reject a whole lawn
// because an unused slot carries them. Active cards still use strict validation.
inline void ClearEmptySeedPacket(SeedPacket& v){
 v.mRefreshCounter=0;v.mRefreshTime=0;v.mOffsetX=0;
 v.mImitaterType=SEED_NONE;v.mSlotMachineCountDown=0;
 v.mSlotMachiningNextSeed=SEED_NONE;v.mSlotMachiningPosition=0;
 v.mActive=false;v.mRefreshing=false;v.mTimesUsed=0;
}
template<class IO> void Fields(IO& io,SeedPacket& v){
 if(!io.reading&&v.mPacketType==SEED_NONE){
  SeedPacket empty;empty.mIndex=v.mIndex;ClearEmptySeedPacket(empty);
  SeedPacketFields(io,empty);return;
 }
 SeedPacketFields(io,v);
 if(io.reading&&io.ok&&v.mPacketType==SEED_NONE)ClearEmptySeedPacket(v);
}

template<class IO> void Fields(IO& io,SeedBank& v){
 io.template Scalar<int32_t>(v.mNumPackets);
 io.Value(v.mSeedPackets);
 io.template Scalar<int32_t>(v.mCutSceneDarken);
 io.template Scalar<int32_t>(v.mConveyorBeltCounter);
}
template<class IO> void Fields(IO& io,Challenge& v){
 io.template Scalar<int32_t>(v.mBeghouledMouseCapture);
 io.template Scalar<int32_t>(v.mBeghouledMouseDownX);
 io.template Scalar<int32_t>(v.mBeghouledMouseDownY);
 io.template Scalar<int32_t>(v.mBeghouledEated);
 io.template Scalar<int32_t>(v.mBeghouledPurcasedUpgrade);
 io.template Scalar<int32_t>(v.mBeghouledMatchesThisMove);
 io.Value(v.mChallengeState);
 io.template Scalar<int32_t>(v.mChallengeStateCounter);
 io.template Scalar<int32_t>(v.mConveyorBeltCounter);
 io.template Scalar<int32_t>(v.mChallengeScore);
 io.template Scalar<int32_t>(v.mShowBowlingLine);
 io.Value(v.mLastConveyorSeedType);
 io.template Scalar<int32_t>(v.mSurvivalStage);
 io.template Scalar<int32_t>(v.mSlotMachineRollCount);
 io.Value(v.mReanimChallenge);
 io.Value(v.mReanimClouds);
 io.template Scalar<int32_t>(v.mCloudsCounter);
 io.template Scalar<int32_t>(v.mChallengeGridX);
 io.template Scalar<int32_t>(v.mChallengeGridY);
 io.template Scalar<int32_t>(v.mScaryPotterPots);
 io.template Scalar<int32_t>(v.mRainCounter);
 io.template Scalar<int32_t>(v.mTreeOfWisdomTalkIndex);
}
template<class IO> void Fields(IO& io,Board& v){
 io.Value(v.mWaveHealth);
 io.Value(v.mWaveSpawnRetry);
 io.template Scalar<bool>(v.mIgnoreMouseUp);
 io.template Scalar<bool>(v.mPaused);
 io.Value(v.mGridSquareType);
 io.template Scalar<int32_t>(v.mGridCelLook);
 io.template Scalar<int32_t>(v.mGridCelOffset);
 io.template Scalar<int32_t>(v.mGridCelFog);
 io.template Scalar<bool>(v.mEnableGraveStones);
 io.template Scalar<int32_t>(v.mSpecialGraveStoneX);
 io.template Scalar<int32_t>(v.mSpecialGraveStoneY);
 io.template Scalar<float>(v.mFogOffset);
 io.template Scalar<int32_t>(v.mFogBlownCountDown);
 io.Value(v.mPlantRow);
 io.template Scalar<int32_t>(v.mWaveRowGotLawnMowered);
 io.template Scalar<int32_t>(v.mBonusLawnMowersRemaining);
 io.template Scalar<int32_t>(v.mIceMinX);
 io.template Scalar<int32_t>(v.mIceTimer);
 if(io.reading)for(auto&x:v.mIceParticleID)x=PARTICLESYSTEMID_NULL;
 io.Value(v.mRowPickingArray);
 io.Waves(v.mZombiesInWave,v.mNumWaves);
 io.template Scalar<bool>(v.mZombieAllowed);
 io.template Scalar<int32_t>(v.mSunCountDown);
 io.template Scalar<int32_t>(v.mNumSunsFallen);
 io.template Scalar<int32_t>(v.mShakeCounter);
 io.template Scalar<int32_t>(v.mShakeAmountX);
 io.template Scalar<int32_t>(v.mShakeAmountY);
 io.Value(v.mBackground);
 io.template Scalar<int32_t>(v.mLevel);
 io.template Scalar<int32_t>(v.mSodPosition);
 io.template Scalar<int32_t>(v.mSunMoney);
 io.template Scalar<int32_t>(v.mLastBungeeWave);
 io.template Scalar<int32_t>(v.mMainCounter);
 if(io.reading)v.mBoardUpdateCounter=v.mMainCounter;
 io.template Scalar<int32_t>(v.mEffectCounter);
 io.template Scalar<int32_t>(v.mRiseFromGraveCounter);
 io.template Scalar<int32_t>(v.mOutOfMoneyCounter);
 io.template Scalar<int32_t>(v.mCurrentWave);
 io.template Scalar<int32_t>(v.mTotalSpawnedWaves);
 io.Value(v.mTutorialState);
 if(io.reading)v.mTutorialParticleID=PARTICLESYSTEMID_NULL;
 io.template Scalar<int32_t>(v.mTutorialTimer);
 io.template Scalar<int32_t>(v.mZombieHealthToNextWave);
 io.template Scalar<int32_t>(v.mZombieHealthWaveStart);
 io.template Scalar<int32_t>(v.mZombieCountDown);
 io.template Scalar<int32_t>(v.mZombieCountDownStart);
 io.template Scalar<int32_t>(v.mHugeWaveCountDown);
 io.template Scalar<bool>(v.mHelpDisplayed);
 io.Value(v.mHelpIndex);
 io.template Scalar<bool>(v.mFinalBossKilled);
 io.template Scalar<bool>(v.mShowShovel);
 io.template Scalar<int32_t>(v.mCoinBankFadeCount);
 io.template Scalar<bool>(v.mLevelComplete);
 io.template Scalar<int32_t>(v.mBoardFadeOutCounter);
 io.template Scalar<int32_t>(v.mNextSurvivalStageCounter);
 io.template Scalar<int32_t>(v.mScoreNextMowerCounter);
 io.template Scalar<bool>(v.mLevelAwardSpawned);
 io.template Scalar<int32_t>(v.mProgressMeterWidth);
 io.template Scalar<int32_t>(v.mFlagRaiseCounter);
 io.template Scalar<int32_t>(v.mIceTrapCounter);
 io.template Scalar<int32_t>(v.mBoardRandSeed);
 if(io.reading)v.mPoolSparklyParticleID=PARTICLESYSTEMID_NULL;
 io.Value(v.mFwooshID);
 io.template Scalar<int32_t>(v.mFwooshCountDown);
 io.template Scalar<int32_t>(v.mTimeStopCounter);
 io.template Scalar<bool>(v.mDroppedFirstCoin);
 io.template Scalar<int32_t>(v.mFinalWaveSoundCounter);
 io.template Scalar<int32_t>(v.mCobCannonCursorDelayCounter);
 io.template Scalar<int32_t>(v.mCobCannonMouseX);
 io.template Scalar<int32_t>(v.mCobCannonMouseY);
 io.template Scalar<bool>(v.mKilledYeti);
 io.template Scalar<bool>(v.mMustacheMode);
 io.template Scalar<bool>(v.mSuperMowerMode);
 io.template Scalar<bool>(v.mFutureMode);
 io.template Scalar<bool>(v.mPinataMode);
 io.template Scalar<bool>(v.mDanceMode);
 io.template Scalar<bool>(v.mDaisyMode);
 io.template Scalar<bool>(v.mSukhbirMode);
 io.Value(v.mPrevBoardResult);
 io.template Scalar<bool>(v.mPeaShooterUsed);
 io.template Scalar<bool>(v.mCatapultPlantsUsed);
 io.template Scalar<int32_t>(v.mLevelCoinsCollected);
 io.template Scalar<int32_t>(v.mGargantuarsKillsByCornCob);
 io.template Scalar<bool>(v.mMushroomAndCoffeeBeansOnly);
 io.template Scalar<bool>(v.mMushroomsUsed);
 io.template Scalar<int32_t>(v.mTriggeredLawnMowers);
 io.template Scalar<int32_t>(v.mPlayTimeActiveLevel);
 io.template Scalar<int32_t>(v.mPlayTimeInactiveLevel);
 io.template Scalar<int32_t>(v.mMaxSunPlants);
 io.template Scalar<int32_t>(v.mGravesCleared);
 io.template Scalar<int32_t>(v.mPlantsEaten);
 io.template Scalar<int32_t>(v.mPlantsShoveled);
 io.template Scalar<int32_t>(v.mCoinsCollected);
 io.template Scalar<int32_t>(v.mDiamondsCollected);
 io.template Scalar<int32_t>(v.mPottedPlantsCollected);
 io.template Scalar<int32_t>(v.mChocolateCollected);
 io.Value(v.mRunBossZombieID);
}
template<class IO> void Fields(IO& io,RunState& v){
 io.template Scalar<int32_t>(v.mPendingPlantPicks);
 io.template Scalar<int32_t>(v.mPendingBuffPicks);
 io.Value(v.mPlantChoices);
 io.template Scalar<uint16_t>(v.mBuffChoices);
 io.template Scalar<uint32_t>(v.mPickCounter);
 io.template Scalar<uint32_t>(v.mPickSalt);
 io.template Scalar<bool>(v.mChoicesRolled);
 io.template Scalar<int32_t>(v.mPaidRerollCount);
 io.template Scalar<int32_t>(v.mRerollVouchers);
 io.template Scalar<int32_t>(v.mRerollAwardedLevel);
 io.template Scalar<uint16_t>(v.mPickSeenBuffs);
 io.template Scalar<int32_t>(v.mPickSeenBuffCount);
 io.template Scalar<uint16_t>(v.mPickCooldownBuffs);
 io.template Scalar<int32_t>(v.mPickCooldownBuffCount);
 io.template Scalar<uint16_t>(v.mPickSeenPlants);
 io.template Scalar<int32_t>(v.mPickSeenPlantCount);
 io.template Scalar<uint16_t>(v.mPickCooldownPlants);
 io.template Scalar<int32_t>(v.mPickCooldownPlantCount);
 io.template Scalar<int32_t>(v.mRunSeed);
 io.template Scalar<int32_t>(v.mMode);
 io.template Scalar<int32_t>(v.mDiff);
 io.template Scalar<int32_t>(v.mEndlessScene);
 io.template Scalar<int32_t>(v.mEndlessOptions);
 io.template Scalar<int32_t>(v.mEndlessVoteRounds);
 io.Value(v.mAdmission);
 io.Value(v.mTeam);
 io.template Scalar<uint64_t>(v.mEndlessFocusMask);
 io.template Scalar<int32_t>(v.mEndlessSnapshotLevel);
 io.template Scalar<int32_t>(v.mEndlessSnapshotPhase);
 io.template Scalar<int32_t>(v.mLevelIndex);
 io.Value(v.mPool);
 io.Value(v.mBuffs);
 io.template Scalar<int32_t>(v.mLoanPlant);
 io.template Scalar<int32_t>(v.mLoanLevel);
 io.template Scalar<int32_t>(v.mLentLevel);
 io.Value(v.mLoanBuffs);
 io.template Scalar<int32_t>(v.mFailCounts);
 io.template Scalar<uint32_t>(v.mMowerUsedRows);
 io.template Scalar<int32_t>(v.mScale);
 io.template Scalar<int32_t>(v.mTempo);
 io.template Scalar<int32_t>(v.mZombotany);
 io.template Scalar<int32_t>(v.mBossFlag);
 io.template Scalar<int32_t>(v.mBuffMult);
 io.template Scalar<int32_t>(v.mLadder);
 io.template Scalar<int32_t>(v.mSpeedTier);
 io.template Scalar<int32_t>(v.mFirstWave);
 io.template Scalar<int32_t>(v.mZombieBuffMode);
 io.template Scalar<uint8_t>(v.mZombieBuffStacks);
 io.template Scalar<uint16_t>(v.mZombieVotedLevel);
 io.template Scalar<int32_t>(v.mCatchUpLevel);
 io.template Scalar<int32_t>(v.mCrowdRule);
}
template<class IO> void Fields(IO& io,Reanimation& v){
 io.template Scalar<float>(v.mAnimTime);
 io.template Scalar<float>(v.mAnimRate);
 io.Value(v.mLoopType);
 io.template Scalar<bool>(v.mDead);
 io.template Scalar<int32_t>(v.mFrameStart);
 io.template Scalar<int32_t>(v.mFrameCount);
 io.template Scalar<int32_t>(v.mFrameBasePose);
 io.Value(v.mOverlayMatrix);
 io.Value(v.mColorOverride);
 io.template Scalar<int32_t>(v.mLoopCount);
 io.template Scalar<bool>(v.mIsAttachment);
 io.template Scalar<int32_t>(v.mRenderOrder);
 io.Value(v.mExtraAdditiveColor);
 io.template Scalar<bool>(v.mEnableExtraAdditiveDraw);
 io.Value(v.mExtraOverlayColor);
 io.template Scalar<bool>(v.mEnableExtraOverlayDraw);
 io.template Scalar<float>(v.mLastFrameTime);
 io.Value(v.mFilterEffect);
}
template<class IO> void Fields(IO&io,ReanimatorTrackInstance&v){
 // Blend/shake are transient decoration. Restore the stable visibility, image,
 // attachment and color state, then let the real animation produce new motion.
 uint64_t flags=0;if(!io.reading){if(v.mRenderGroup!=RENDER_GROUP_NORMAL)flags|=1;if(v.mImageOverride)flags|=2;if(v.mAttachmentID)flags|=4;if(v.mTrackColor!=Color::White)flags|=8;if(v.mIgnoreClipRect)flags|=16;if(!v.mTruncateDisappearingFrames)flags|=32;if(v.mIgnoreColorOverride)flags|=64;if(v.mIgnoreExtraAdditiveColor)flags|=128;}
 io.Bits(flags,1);if(flags&1)io.template Scalar<int32_t>(v.mRenderGroup);if(flags&2)io.Value(v.mImageOverride);if(flags&4)io.Value(v.mAttachmentID);if(flags&8)io.Value(v.mTrackColor);
 if(io.reading){v.mIgnoreClipRect=(flags&16)!=0;v.mTruncateDisappearingFrames=(flags&32)==0;v.mIgnoreColorOverride=(flags&64)!=0;v.mIgnoreExtraAdditiveColor=(flags&128)!=0;v.mBlendCounter=v.mBlendTime=0;v.mShakeX=v.mShakeY=v.mShakeOverride=0;}
}
template<class IO> void Fields(IO& io,ReanimatorTransform& v){
 io.Value(v.mImage);
 // Text on a decorative transform is not needed by the speculative battle.
 io.template Scalar<float>(v.mTransX);
 io.template Scalar<float>(v.mTransY);
 io.template Scalar<float>(v.mSkewX);
 io.template Scalar<float>(v.mSkewY);
 io.template Scalar<float>(v.mScaleX);
 io.template Scalar<float>(v.mScaleY);
 io.template Scalar<float>(v.mFrame);
 io.template Scalar<float>(v.mAlpha);
}
template<class IO> void Fields(IO& io,AttachEffect& v){
 io.template Scalar<uint32_t>(v.mEffectID);
 io.Value(v.mEffectType);
 io.Value(v.mOffset);
 io.template Scalar<bool>(v.mDontDrawIfParentHidden);
 io.template Scalar<bool>(v.mDontPropogateColor);
}
template<class IO> void Fields(IO& io,Attachment& v){
 io.Value(v.mEffectArray);
 io.template Scalar<int32_t>(v.mNumEffects);
 io.template Scalar<bool>(v.mDead);
}
template<class IO> void Fields(IO& io,PottedPlant& v){
 io.Value(v.mSeedType);
 io.Value(v.mWhichZenGarden);
 io.template Scalar<int32_t>(v.mX);
 io.template Scalar<int32_t>(v.mY);
 io.Value(v.mFacing);
 io.template Scalar<int64_t>(v.mLastWateredTime);
 io.Value(v.mDrawVariation);
 io.Value(v.mPlantAge);
 io.template Scalar<int32_t>(v.mTimesFed);
 io.template Scalar<int32_t>(v.mFeedingsPerGrow);
 io.Value(v.mPlantNeed);
 io.template Scalar<int64_t>(v.mLastNeedFulfilledTime);
 io.template Scalar<int64_t>(v.mLastFertilizedTime);
 io.template Scalar<int64_t>(v.mLastChocolateTime);
 io.template Scalar<int64_t>(v.mFutureAttribute);
}
template<class IO> void Fields(IO& io,PvzpSmoothArray& v){
 io.template Scalar<int32_t>(v.mItem);
 io.template Scalar<float>(v.mWeight);
 io.template Scalar<float>(v.mLastPicked);
 io.template Scalar<float>(v.mSecondLastPicked);
}
template<class IO> void Fields(IO& io,PvzpWeightedGridArray& v){
 io.template Scalar<int32_t>(v.mX);
 io.template Scalar<int32_t>(v.mY);
 io.template Scalar<int32_t>(v.mWeight);
}
