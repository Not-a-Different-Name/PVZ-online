#include "RunHistory.h"
#include "RunState.h"
#include "RunCheckpointIO.h"
#include "../../LawnApp.h"
#include "../System/PlayerInfo.h"
#include "../Online/NetSession.h"
#include "../System/DamageStats.h"
#include <cstdio>
#include <ctime>
#include <cerrno>

namespace RunHistory {
bool& WriteFailed(){static bool failed=false;return failed;}
std::string Path(int profile){return Sexy::GetAppDataFolder()+"userdata/recent-runs"+std::to_string(profile)+".bin";}
namespace {
bool ReadFile(const std::string& path,std::vector<Record>& records,bool& exists) {
    errno=0;FILE* file=std::fopen(path.c_str(),"rb");
    if(!file){exists=errno!=ENOENT;return false;}exists=true;
    std::vector<unsigned char> bytes(12+CAPACITY*RECORD_BYTES+1);
    size_t n=std::fread(bytes.data(),1,bytes.size(),file);bool ok=!std::ferror(file);std::fclose(file);bytes.resize(n);
    return ok&&Decode(bytes,records);
}
}
ReadState Read(int profile,std::vector<Record>& records) {
    records.clear();bool primary=false,backup=false;
    if(ReadFile(Path(profile),records,primary))return OK;
    if(ReadFile(Path(profile)+".bak",records,backup))return RECOVERED;
    return primary||backup?DAMAGED:EMPTY;
}
bool Write(int profile,const Record& record) {
    if(!record.Valid())return false;
    std::vector<Record> records;const auto state=Read(profile,records);
    if(state==DAMAGED)return false;
    Sexy::MkDir(Sexy::GetAppDataFolder()+"userdata");
    // Only validated primary data may replace the last good backup.
    if(state==OK && !RunCheckpointIO::AtomicWrite(Path(profile)+".bak",Encode(records)))return false;
    Upsert(records,record);
    return RunCheckpointIO::AtomicWrite(Path(profile),Encode(records));
}
bool Capture(LawnApp* app,Status status) {
    if(!app || !app->mPlayerInfo || !app->mRunState)return true;
    auto& run=*app->mRunState;Record r;
    if((status==SAVED || status==LEFT) && app->mBoard && app->mBoardResult==BOARDRESULT_LOST)status=FAILED;
    r.seed=uint32_t(run.mRunSeed);r.team=run.mAdmission.run;r.started=r.updated=uint64_t(std::time(nullptr));
    r.stage=uint32_t(run.mLevelIndex+1);if(run.IsComplete())r.stage=run.GetLevelCount();
    r.joined=1;r.status=status;r.pool=uint16_t(run.mPool.size());r.buffs=uint16_t(run.mBuffs.size());
    int failures=run.mFinaleFailures;for(int n:run.mFailCounts)failures=std::min(1000000000,failures+n);r.failures=uint32_t(failures);
    const int settings[]={run.mMode,run.mEndlessScene,run.mDiff,run.mScale,run.mTempo,run.mZombotany,run.mBuffMult,
        run.mBossFlag,run.mLadder,run.mZombieBuffMode,run.mSpeedTier,run.mFirstWave,run.mEndlessOptions,0,0};
    for(int i=0;i<SETTING_COUNT;++i)r.settings[i]=uint8_t(settings[i]);
    if(app->mOnlineSession && app->mOnlineSession->GetLocalSeat()) {
        r.players=uint8_t(std::max(1,app->mOnlineSession->GetOccupiedSeatCount()));r.seat=app->mOnlineSession->GetLocalSeat();
    }
    if(run.mAdmission.phase!=Admission::NONE)r.joined=uint32_t(run.mAdmission.joined+1);
    const auto* row=app->mDamageStats.Local();
    if(row && app->mDamageStats.mRun==uint32_t(run.mRunSeed)) {
        r.totalDamage=row->mRunDamage.mAll;r.bossDamage=row->mRunDamage.mBoss;r.partial=row->mPartial;
    } else r.partial=1;
    const bool ok=Write(app->mPlayerInfo->mId,r);WriteFailed()=!ok;return ok;
}
}
