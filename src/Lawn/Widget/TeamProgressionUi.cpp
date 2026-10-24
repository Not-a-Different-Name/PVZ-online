#include "../../LawnApp.h"
#include "TeamProgressionDialog.h"
#include "NextWaveControl.h"
#include "CoopHud.h"
#include "../Board.h"
#include "widget/WidgetManager.h"
#include <algorithm>

using namespace TeamProgressionView;
using namespace Sexy;

struct TeamProgressionUiState:Controller
{
    LawnApp* app=nullptr;
    Controller* source=nullptr;
    Board* board=nullptr;
    RunState* run=nullptr;
    Widget* previousFocus=nullptr;
    bool Valid() const
    {
        return source&&app&&app->mTeamProgressionController==source&&app->mBoard==board&&
            app->GetRunState()==run&&board&&run;
    }
    Snapshot Read() const override
    {
        if(!Valid())return {};
        Snapshot snapshot=source->Read();
        if(!Valid())return {};
        if(!app->mActive||app->mMinimized)snapshot.canAct=false;
        return snapshot;
    }
    void Submit(const Intent& intent) override
    {
        if(!Valid())return;
        Controller* destination=source;
        destination->Submit(intent);
    }
};

class HostedTeamProgressionDialog:public TeamProgressionDialog
{
    std::shared_ptr<TeamProgressionUiState> mState;
public:
    explicit HostedTeamProgressionDialog(std::shared_ptr<TeamProgressionUiState> state):
        TeamProgressionDialog(*state),mState(std::move(state)) {}
};

bool LawnApp::TeamProgressionAvailable() const
{
    return mTeamProgressionController&&mRunState&&mBoard;
}

bool LawnApp::TeamProgressionInputBlocked() const
{
    return mTeamProgressionDialog||mTeamProgressionInputFence;
}

void LawnApp::CloseTeamProgressionUi(bool restoreFocus)
{
    auto state=mTeamProgressionUi;
    if(state){state->source=nullptr;state->app=nullptr;}
    auto* dialog=mTeamProgressionDialog;
    mTeamProgressionDialog=nullptr;
    mTeamProgressionUi.reset();
    if(!dialog)return;
    mTeamProgressionInputFence=true;
    dialog->CancelInput();dialog->SetVisible(false);
    if(mWidgetManager)
    {
        mWidgetManager->RemoveWidget(dialog);
        if(restoreFocus&&!mWidgetManager->mBaseModalWidget&&!mWidgetManager->mFocusWidget)
        {
            Widget* focus=state?state->previousFocus:nullptr;
            const auto& widgets=mWidgetManager->mWidgets;
            if(std::find(widgets.begin(),widgets.end(),focus)==widgets.end())focus=nullptr;
            if(!focus&&std::find(widgets.begin(),widgets.end(),mBoard)!=widgets.end())focus=mBoard;
            if(focus)mWidgetManager->SetFocus(focus);
        }
    }
    SafeDeleteWidget(dialog);
}

void LawnApp::SetTeamProgressionController(Controller* controller)
{
    if(controller&&controller==mTeamProgressionController)return;
    mTeamProgressionController=nullptr;
    CloseTeamProgressionUi();
    mTeamProgressionController=controller;
}

void LawnApp::ShowTeamProgression(Page page)
{
    if(!TeamProgressionAvailable()||GetDialogCount()>0||mBoard->mPaused||!mActive||mMinimized)return;
    if(mTeamProgressionUi&&!mTeamProgressionUi->Valid())SetTeamProgressionController(nullptr);
    if(!TeamProgressionAvailable())return;
    if(mTeamProgressionDialog&&mTeamProgressionDialog->Closed())CloseTeamProgressionUi();
    auto state=mTeamProgressionUi;
    if(!state)
    {
        state=std::make_shared<TeamProgressionUiState>();
        state->app=this;state->source=mTeamProgressionController;state->board=mBoard;state->run=GetRunState();
        state->previousFocus=mWidgetManager->mFocusWidget;
        mTeamProgressionUi=state;
    }
    const Snapshot snapshot=state->Read();
    if(!state->Valid()||snapshot.phase==Phase::DISABLED)return;
    Intent select;select.action=Action::SELECT_PAGE;select.context=snapshot.context;select.value=static_cast<int>(page);
    state->Submit(select);
    if(mTeamProgressionUi!=state||!state->Valid())return;
    if(mTeamProgressionDialog)
    {
        mTeamProgressionDialog->Update();
        if(mTeamProgressionDialog)mWidgetManager->SetFocus(mTeamProgressionDialog);
        return;
    }
    auto* dialog=new HostedTeamProgressionDialog(state);
    if(mTeamProgressionUi!=state||!state->Valid()){SafeDeleteWidget(dialog);return;}
    mBoard->RefreshSeedPacketFromCursor();mBoard->ClearCursor();
    mBoard->CloseQuickChatPanel();mBoard->mSunGiftSelectFrames=0;
    mBoard->mCoopTouchCaptured=false;mBoard->mCoopTouchAction=-1;
    mBoard->mIgnoreMouseUp=true;
    CoopHud::StopWatch(mBoard);
    NextWaveControl::Suspend(mBoard);
    NextWave::For(mBoard).keyHeld=false;
#ifdef __ANDROID__
    mBoard->mTouchSeedDrag=false;mBoard->mTouchSeedDragMoved=false;mBoard->mCoopToolsOpen=false;
#endif
    mDamageStats.mOpen=0;mDamageStats.mCaptured=false;mDamageStats.mPressed=-1;
    mTeamProgressionDialog=dialog;mTeamProgressionInputFence=true;
    mWidgetManager->AddWidget(dialog);mWidgetManager->SetFocus(dialog);
}

void LawnApp::UpdateTeamProgressionUi()
{
    if(mTeamProgressionUi&&!mTeamProgressionUi->Valid())SetTeamProgressionController(nullptr);
    if(mTeamProgressionDialog&&mTeamProgressionDialog->Closed())CloseTeamProgressionUi();
    if(!mTeamProgressionDialog&&(!mWidgetManager||mWidgetManager->mActualDownButtons==0))mTeamProgressionInputFence=false;
}

namespace TeamPlayUi
{
void Install(LawnApp* app,Controller* controller){if(app)app->SetTeamProgressionController(controller);}
void Show(LawnApp* app,Page page){if(app)app->ShowTeamProgression(page);}
void Close(LawnApp* app){if(app)app->CloseTeamProgressionUi();}
}
