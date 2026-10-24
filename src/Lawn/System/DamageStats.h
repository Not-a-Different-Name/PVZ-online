#pragma once
#include "../Online/DamageStatsWire.h"
#include <string>
#include <unordered_set>
#include <vector>

class LawnApp;
class Board;
class Zombie;
class RunState;

class DamageStats
{
public:
    struct Row
    {
        uint64_t mPlayer = 0, mSequence = 0, mUpdated = 0;
        uint8_t mSeat = 0;
        bool mPartial = false;
        DamageStatsWire::Counters mLevelDamage, mRunDamage;
        std::string mName;
    };
    class Source
    {
        DamageStats& mStats;
        bool mPrevious;
    public:
        Source(DamageStats& stats, bool credited) : mStats(stats), mPrevious(stats.mCredited) { stats.mCredited = credited; }
        ~Source() { mStats.mCredited = mPrevious; }
        Source(const Source&) = delete;
        Source& operator=(const Source&) = delete;
    };
    class RemoteSource {
        DamageStats& stats;
        uint64_t player,strike;
        bool credited;
    public:
        RemoteSource(DamageStats& s,uint64_t p,uint64_t id):stats(s),player(s.mRemotePlayer),strike(s.mRemoteStrike),credited(s.mCredited){
            if(p){s.mRemotePlayer=p;s.mRemoteStrike=id;s.mCredited=true;}
        }
        ~RemoteSource(){stats.mRemotePlayer=player;stats.mRemoteStrike=strike;stats.mCredited=credited;}
    };
    void AddSupportDamage(uint64_t all,uint64_t boss);
    static constexpr int DEFAULT_SETTING = 6;
    bool mSettingsLoaded = false;
    int mSetting = DEFAULT_SETTING;
    int mOpen = 0, mPage = 0, mPressed = -1;
    bool mCaptured = false;
    bool mSettingsError = false;
    bool mPartial = false;
    bool mActive = false;
    int mLevel = -1, mMode = -1;
    uint32_t mRun = 0;
    uint64_t mLocalPlayer = 0, mAttempt = 0;
    std::vector<Row> mRows;

    void LoadSettings();
    bool SaveSettings(int setting);
    bool WholeRun() const { return (mSetting & 1) != 0; }
    bool ShowTotal() const { return (mSetting & 2) != 0; }
    bool ShowBoss() const { return (mSetting & 4) != 0; }
    void Reset();
    void BeginLevel(Board* board);
    void Detach(Board* board);
    void Update(LawnApp* app);
    void Flush();
    void NoteFailure();
    void Record(Zombie* zombie, int amount);
    void RecordKill(Zombie* zombie);
    std::vector<unsigned char> SaveSnapshot() const;
    std::vector<unsigned char> SaveTrailer() const;
    void LoadSaveTrailer(Board* board, const unsigned char* data, size_t size);
    bool LoadSnapshot(Board* board, const unsigned char* data, size_t size);
    void CaptureCheckpoint(const RunState& run) const;
    const Row* Local() const;
    bool IsLocal(uint64_t player) const { return player == mLocalPlayer; }

private:
    LawnApp* mApp = nullptr;
    Board* mBoard = nullptr;
    bool mCredited = false;
    uint64_t mRemotePlayer=0,mRemoteStrike=0;
    bool mFailed = false;
    std::vector<unsigned char> mRetrySnapshot;
    std::unordered_set<uint32_t> mInstantKills;
    uint64_t mLastSend = 0, mSequence = 0;
    int mLastScene = -1;
    Row* Find(uint64_t player);
    Row& LocalRow();
    void AlignLevel(int level);
    void RefreshIdentity();
    void Receive();
};
