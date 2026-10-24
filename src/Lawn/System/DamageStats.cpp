#include "DamageStats.h"
#include "../TeamPlay.h"
#include "../../LawnApp.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Online/RoomPresentation.h"
#include "../Run/RunState.h"
#include "../Run/RunCheckpointIO.h"
#include "../ModText.h"
#include "Common.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <utility>

namespace
{
uint64_t Now()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t NewIdentity()
{
    static uint64_t previous = 0;
    const auto current = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    previous = std::max(current, previous + 1);
    return previous;
}

bool ValidSetting(int setting) { return setting >= 0 && setting <= 7; }

DamageStatsWire::Snapshot Message(const DamageStats& stats, const DamageStats::Row& row, uint8_t operation)
{
    DamageStatsWire::Snapshot message;
    message.mOp = operation; message.mSeat = row.mSeat;
    message.mMode = static_cast<uint8_t>(stats.mMode); message.mPartial = row.mPartial ? 1 : 0;
    message.mRun = stats.mRun; message.mLevel = static_cast<uint32_t>(stats.mLevel);
    message.mAttempt = stats.mAttempt; message.mPlayer = row.mPlayer; message.mSequence = row.mSequence;
    message.mLevelDamage = row.mLevelDamage; message.mRunDamage = row.mRunDamage;
    DamageStatsWire::Writer name;
    name.Text(row.mName);
    std::memcpy(message.mName, name.mData.data(), sizeof(message.mName));
    return message;
}
}

void DamageStats::LoadSettings()
{
    if (mSettingsLoaded) return;
    int setting = DEFAULT_SETTING;
#ifdef __ANDROID__
    std::ifstream file(Sexy::PathFromU8(Sexy::GetAppDataPath("userdata/android-damage-stats.txt")));
    if (!(file >> setting)) setting = DEFAULT_SETTING;
#else
    ModText::RegReadDword("ModDamageStats", &setting);
#endif
    mSetting = ValidSetting(setting) ? setting : DEFAULT_SETTING;
    mSettingsLoaded = true;
}

bool DamageStats::SaveSettings(int setting)
{
    if (!ValidSetting(setting)) return false;
    if (mSettingsLoaded && setting == mSetting) return true;
#ifdef __ANDROID__
    const std::string text = std::to_string(setting) + "\n";
    if (!RunCheckpointIO::AtomicWrite(Sexy::GetAppDataPath("userdata/android-damage-stats.txt"),
        std::vector<unsigned char>(text.begin(), text.end()))) return false;
#else
    if (!ModText::RegWriteDword("ModDamageStats", setting)) return false;
#endif
    mSettingsLoaded = true;
    mSetting = setting;
    if ((mOpen == 1 && !ShowTotal()) || (mOpen == 2 && !ShowBoss())) mOpen = 0;
    return true;
}

void DamageStats::Reset()
{
    const int setting = mSetting;
    const bool loaded = mSettingsLoaded;
    *this = DamageStats();
    mSetting = setting;
    mSettingsLoaded = loaded;
}

DamageStats::Row* DamageStats::Find(uint64_t player)
{
    for (auto& row : mRows) if (row.mPlayer == player) return &row;
    return nullptr;
}

DamageStats::Row& DamageStats::LocalRow()
{
    if (!mLocalPlayer) mLocalPlayer = NewIdentity();
    if (auto* row = Find(mLocalPlayer)) return *row;
    if (mRows.size() >= DamageStatsWire::MAX_ROWS)
    {
        mRows.erase(mRows.begin());
        mPartial = true;
    }
    Row row;
    row.mPlayer = mLocalPlayer;
    row.mPartial = mPartial;
    mRows.push_back(row);
    return mRows.back();
}

const DamageStats::Row* DamageStats::Local() const
{
    for (const auto& row : mRows) if (row.mPlayer == mLocalPlayer) return &row;
    return nullptr;
}

void DamageStats::RefreshIdentity()
{
    if (!mApp) return;
    auto* session = RoomPresentation::Session(mApp);
    if (session && mApp->IsOnlineGame() && session->mCoopToken)
    {
        if (mLocalPlayer != session->mCoopToken)
        {
            if (auto* row = Find(mLocalPlayer)) row->mPlayer = session->mCoopToken;
            mLocalPlayer = session->mCoopToken;
        }
        auto& row = LocalRow();
        row.mSeat = session->GetLocalSeat();
        row.mName = session->GetSeatName(row.mSeat);
    }
    else
    {
        auto& row = LocalRow();
        row.mSeat = 1;
        row.mName = ModText::Tr("自己", "You");
    }
}

void DamageStats::AlignLevel(int level)
{
    if (mLevel == level) return;
    for (auto& row : mRows)
    {
        if (level < mLevel)
        {
            row.mRunDamage.mAll -= row.mLevelDamage.mAll;
            row.mRunDamage.mBoss -= row.mLevelDamage.mBoss;
            row.mPartial = true;
        }
        row.mLevelDamage = {};
        row.mSequence = 0;
        row.mUpdated = 0;
    }
    mLevel = level;
}

void DamageStats::BeginLevel(Board* board)
{
    LoadSettings();
    auto* app = board->mApp;
    auto* run = app->GetRunState();
    if (!app->IsRunMode() || !run)
    {
        mActive = false; mBoard = nullptr; mOpen = 0; mCaptured = false; return;
    }
    const bool sameRun = mActive && mRun == static_cast<uint32_t>(run->mRunSeed) && mMode == run->mMode;
    if (!sameRun)
    {
        mRows.clear(); mLocalPlayer = 0;
        mPartial = run->mLevelIndex > 0;
        mLevel = run->mLevelIndex;
    }
    mApp = app; mBoard = board; mActive = true;
    mRun = static_cast<uint32_t>(run->mRunSeed); mMode = run->mMode;
    if (!run->mDamageHistory.empty())
    {
        if (!LoadSnapshot(board, run->mDamageHistory.data(), run->mDamageHistory.size()))
        {
            mRows.clear(); mLocalPlayer = 0; mLevel = run->mLevelIndex; mPartial = true;
        }
    }
    else if (sameRun && mLevel == run->mLevelIndex)
    {
        for (auto& row : mRows)
        {
            row.mRunDamage.mAll -= row.mLevelDamage.mAll;
            row.mRunDamage.mBoss -= row.mLevelDamage.mBoss;
            row.mLevelDamage = {};
        }
    }
    AlignLevel(run->mLevelIndex);
    RefreshIdentity();
    mFailed = false;
    mInstantKills.clear();
    for (auto& row : mRows) row.mSequence = 0;
    mSequence = 0; mLastSend = 0; mLastScene = -1;
    mOpen = 0; mCaptured = false; mPressed = -1; mPage = 0;
    auto* session = RoomPresentation::Session(app);
    if (session) session->mDamageInbox.clear();
    mAttempt = !app->IsOnlineGame() || (session && session->GetRole() == NetSession::Role::HOST) ? NewIdentity() : 0;
    mRetrySnapshot = SaveSnapshot();
}

void DamageStats::Detach(Board* board)
{
    if (mBoard != board) return;
    Receive();
    Flush();
    mBoard = nullptr;
    mOpen = 0; mCaptured = false; mPressed = -1;
}

void DamageStats::Record(Zombie* zombie, int amount)
{
    if (!mCredited || !mActive || !mApp || amount <= 0 || zombie->mBoard != mBoard ||
        mApp->mBoard != mBoard || mApp->mGameScene != SCENE_PLAYING || zombie->mMindControlled ||
        zombie->mDead || zombie->IsDeadOrDying() || !zombie->IsOnBoard()) return;
    const bool boss = mBoard->IsRunBossZombie(zombie);
    if(mRemotePlayer){TeamPlay::NoteDamage(mApp,mRemotePlayer,mRemoteStrike,amount,boss);return;}
    auto& row = LocalRow();
    const uint64_t accepted = std::min(static_cast<uint64_t>(amount), DamageStatsWire::LIMIT - row.mRunDamage.mAll);
    row.mLevelDamage.Add(accepted, boss);
    row.mRunDamage.Add(accepted, boss);
    row.mPartial = row.mPartial || mPartial;
    row.mUpdated = Now();
}

void DamageStats::AddSupportDamage(uint64_t all,uint64_t boss)
{
    if(!mActive||boss>all||all>DamageStatsWire::LIMIT)return;
    auto&row=LocalRow();
    const auto accepted=std::min(all,DamageStatsWire::LIMIT-row.mRunDamage.mAll);
    const auto acceptedBoss=std::min(boss,accepted);
    row.mRunDamage.mAll+=accepted;row.mRunDamage.mBoss+=acceptedBoss;
    row.mLevelDamage.mAll+=accepted;row.mLevelDamage.mBoss+=acceptedBoss;row.mUpdated=Now();
}
void DamageStats::RecordKill(Zombie* zombie)
{
    if (!mCredited || !mActive || !mApp || zombie->mBoard != mBoard ||
        mApp->mBoard != mBoard || mApp->mGameScene != SCENE_PLAYING || zombie->mMindControlled ||
        zombie->mBodyHealth <= 0 || zombie->mDead || zombie->IsDeadOrDying() || !zombie->IsOnBoard()) return;
    if (!mInstantKills.insert(static_cast<uint32_t>(mBoard->ZombieGetID(zombie))).second) return;
    Record(zombie, std::max(0, zombie->mBodyHealth));
    if (zombie->mHelmType != HELMTYPE_NONE) Record(zombie, std::max(0, zombie->mHelmHealth));
    if (zombie->mShieldType != SHIELDTYPE_NONE) Record(zombie, std::max(0, zombie->mShieldHealth));
    if (zombie->IsFlying()) Record(zombie, std::max(0, zombie->mFlyingHealth));
}

void DamageStats::Receive()
{
    auto* session = RoomPresentation::Session(mApp);
    if (!session) return;
    DamageStatsWire::Snapshot message;
    while (session->TakeDamageStats(message))
    {
        if (!mActive || message.mRun != mRun || message.mMode != mMode ||
            message.mLevel != static_cast<uint32_t>(mLevel)) continue;
        const bool host = session->GetRole() == NetSession::Role::HOST;
        if (message.mOp == DamageStatsWire::BEGIN)
        {
            if (host || message.mSrc != session->GetHostSeat()) continue;
            if (mAttempt != message.mAttempt)
            {
                mAttempt = message.mAttempt;
                for (auto& row : mRows) if (!IsLocal(row.mPlayer)) { row.mSequence = 0; row.mUpdated = 0; }
                mLastSend = 0;
            }
            continue;
        }
        if (!mAttempt || message.mAttempt != mAttempt) continue;
        if (host)
        {
            if (message.mOp != DamageStatsWire::REPORT || message.mSrc != message.mSeat ||
                session->mCoopPeers[message.mSrc] != message.mPlayer) continue;
        }
        else if (message.mOp != DamageStatsWire::RELAY || message.mSrc != session->GetHostSeat()) continue;
        if (message.mPlayer == mLocalPlayer) continue;
        Row* row = Find(message.mPlayer);
        if (!row)
        {
            if (mRows.size() >= DamageStatsWire::MAX_ROWS) continue;
            mRows.push_back(Row());
            row = &mRows.back(); row->mPlayer = message.mPlayer;
        }
        if (message.mSequence <= row->mSequence) continue;
        row->mSequence = message.mSequence;
        row->mSeat = message.mSeat;
        row->mLevelDamage = message.mLevelDamage; row->mRunDamage = message.mRunDamage;
        row->mPartial = message.mPartial != 0;
        row->mName = host ? session->GetSeatName(message.mSrc) : std::string(message.mName);
        row->mUpdated = Now();
    }
}

void DamageStats::Flush()
{
    if (!mActive || !mApp) return;
    auto* session = RoomPresentation::Session(mApp);
    if (!session || !session->IsConnected() || !mApp->IsOnlineGame()) return;
    RefreshIdentity();
    auto& local = LocalRow();
    local.mSequence = ++mSequence;
    local.mUpdated = Now();
    if (session->GetRole() == NetSession::Role::HOST)
    {
        if (!mAttempt) mAttempt = NewIdentity();
        auto begin = Message(*this, local, DamageStatsWire::BEGIN);
        session->SendDamageStats(begin);
        for (auto& row : mRows)
        {
            const auto seat = row.mPlayer == mLocalPlayer ? session->GetLocalSeat() : session->FindCoopSeat(row.mPlayer);
            if (seat) row.mSeat = seat;
            if (!row.mSequence) continue;
            session->SendDamageStats(Message(*this, row, DamageStatsWire::RELAY));
        }
    }
    else if (mAttempt)
    {
        session->SendDamageStats(Message(*this, local, DamageStatsWire::REPORT), session->GetHostSeat());
    }
    mLastSend = Now();
}

void DamageStats::Update(LawnApp* app)
{
    mApp = app;
    LoadSettings();
    if (!mActive) return;
    Receive();
    const auto now = Now();
    const int scene = static_cast<int>(app->mGameScene);
    if (now - mLastSend >= 1000 || scene != mLastScene) Flush();
    mLastScene = scene;
    if (mCaptured && (!app->mActive || app->mMinimized || !app->mBoard || app->mBoard->mPaused))
    {
        mPressed = -1;
    }
}

std::vector<unsigned char> DamageStats::SaveSnapshot() const
{
    if (!mActive || mLevel < 0 || mRows.size() > DamageStatsWire::MAX_ROWS) return {};
    DamageStatsWire::Writer writer;
    writer.Put(0x31545344u, 4); writer.Put(1, 4);
    writer.Put(mRun, 4); writer.Put(static_cast<uint32_t>(mMode), 4);
    writer.Put(static_cast<uint32_t>(mLevel), 4); writer.Put(mLocalPlayer, 8);
    writer.Put(mPartial ? 1 : 0, 1); writer.Put(mRows.size(), 1);
    for (const auto& row : mRows)
    {
        writer.Put(row.mPlayer, 8); writer.Put(row.mSequence, 8);
        writer.Put(row.mSeat, 1); writer.Put(row.mPartial ? 1 : 0, 1);
        writer.Put(row.mLevelDamage.mAll, 8); writer.Put(row.mLevelDamage.mBoss, 8);
        writer.Put(row.mRunDamage.mAll, 8); writer.Put(row.mRunDamage.mBoss, 8);
        writer.Text(row.mName);
    }
    const auto hash = DamageStatsWire::Hash(writer.mData.data(), writer.mData.size());
    writer.Put(hash, 4);
    return writer.mData;
}

bool DamageStats::LoadSnapshot(Board* board, const unsigned char* data, size_t size)
{
    auto* run = board->mApp->GetRunState();
    if (!run || size < 34 || size > 8192) return false;
    DamageStatsWire::Reader checksum(data + size - 4, 4);
    if (checksum.Get(4) != DamageStatsWire::Hash(data, size - 4)) return false;
    DamageStatsWire::Reader reader(data, size - 4);
    if (reader.Get(4) != 0x31545344u || reader.Get(4) != 1 ||
        reader.Get(4) != static_cast<uint32_t>(run->mRunSeed) || reader.Get(4) != static_cast<uint32_t>(run->mMode)) return false;
    const uint64_t level = reader.Get(4), local = reader.Get(8);
    const uint64_t partial = reader.Get(1), count = reader.Get(1);
    if (level > static_cast<uint32_t>(run->mLevelIndex) || !local || partial > 1 || count > DamageStatsWire::MAX_ROWS) return false;
    std::vector<Row> rows;
    for (uint64_t index = 0; index < count; ++index)
    {
        Row row;
        row.mPlayer = reader.Get(8); row.mSequence = reader.Get(8);
        row.mSeat = static_cast<uint8_t>(reader.Get(1));
        const uint64_t rowPartial = reader.Get(1);
        row.mPartial = rowPartial != 0;
        row.mLevelDamage.mAll = reader.Get(8); row.mLevelDamage.mBoss = reader.Get(8);
        row.mRunDamage.mAll = reader.Get(8); row.mRunDamage.mBoss = reader.Get(8);
        row.mName = reader.Text();
        if (!row.mPlayer || row.mSeat < 1 || row.mSeat > 6 || rowPartial > 1 ||
            !row.mLevelDamage.Valid() || !row.mRunDamage.Valid() ||
            row.mLevelDamage.mAll > row.mRunDamage.mAll || row.mLevelDamage.mBoss > row.mRunDamage.mBoss ||
            row.mRunDamage.mBoss - row.mLevelDamage.mBoss > row.mRunDamage.mAll - row.mLevelDamage.mAll) return false;
        for (const auto& previous : rows) if (previous.mPlayer == row.mPlayer) return false;
        rows.push_back(row);
    }
    if (!reader.mOk || reader.mOffset != size - 4 ||
        std::none_of(rows.begin(), rows.end(), [local](const Row& row) { return row.mPlayer == local; })) return false;
    mApp = board->mApp; mBoard = board; mActive = true;
    mRun = static_cast<uint32_t>(run->mRunSeed); mMode = run->mMode;
    mLevel = static_cast<int>(level); mLocalPlayer = local; mPartial = partial != 0; mRows = std::move(rows);
    AlignLevel(run->mLevelIndex);
    RefreshIdentity();
    return true;
}

std::vector<unsigned char> DamageStats::SaveTrailer() const
{
    DamageStatsWire::Writer writer;
    writer.mData = SaveSnapshot();
    if (writer.mData.empty()) return {};
    const size_t size = writer.mData.size();
    writer.Put(size, 4); writer.Put(0x54585344u, 4);
    return writer.mData;
}

void DamageStats::LoadSaveTrailer(Board* board, const unsigned char* data, size_t size)
{
    bool restored = false;
    if (size >= 8)
    {
        DamageStatsWire::Reader footer(data + size - 8, 8);
        const uint64_t length = footer.Get(4), magic = footer.Get(4);
        if (magic == 0x54585344u && length <= 8192 && length <= size - 8)
            restored = LoadSnapshot(board, data + size - 8 - static_cast<size_t>(length), static_cast<size_t>(length));
    }
    if (!restored)
    {
        BeginLevel(board);
        mPartial = true;
        if (mActive) LocalRow().mPartial = true;
    }
    mSequence = 0; mLastSend = 0;
    mFailed = false;
    mInstantKills.clear();
    for (auto& row : mRows) row.mSequence = 0;
    auto* session = RoomPresentation::Session(board->mApp);
    mAttempt = !board->mApp->IsOnlineGame() || (session && session->GetRole() == NetSession::Role::HOST) ? NewIdentity() : 0;
    mRetrySnapshot = SaveSnapshot();
}

void DamageStats::NoteFailure()
{
    mFailed = true;
    Flush();
}

void DamageStats::CaptureCheckpoint(const RunState& run) const
{
    if (mActive && mRun == static_cast<uint32_t>(run.mRunSeed) && mMode == run.mMode && mLevel <= run.mLevelIndex)
    {
        const bool boundaryOnly = mLevel == run.mLevelIndex && (!run.IsNewEndless() || (mApp && mApp->IsOnlineGame()));
        run.mDamageHistory = mFailed || boundaryOnly ? mRetrySnapshot : SaveSnapshot();
    }
}
