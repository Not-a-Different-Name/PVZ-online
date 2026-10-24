#pragma once
#include "widget/Widget.h"
#include "../Run/TeamProgressionView.h"
#include <vector>
#include <map>
#include <tuple>

class TeamProgressionDialog : public Sexy::Widget
{
public:
    enum class ControlKind { CLOSE, PAGE, VOTE, CHOICE_DETAILS, PEER, MODE, LIST_PAGE,
        TEXT_PAGE, TEXT_BACK, STATUS_DETAILS, SUN_REQUEST, SUN_GIFT, LOAN, WATCH,
        SUPPORT_PREVIEW, SUPPORT_CONFIRM, PICK_CELL, CELL, PERMISSION, ENTRY };
    struct Control
    {
        ControlKind kind;
        Sexy::Rect rect;
        std::string label;
        int value=0,extra=0;
        uint64_t target=0,quote=0;
        bool enabled=true,selected=false;
    };
    explicit TeamProgressionDialog(TeamProgressionView::Controller& controller);
    void Update() override;
    void Draw(Sexy::Graphics* graphics) override;
    void MouseDown(int pointerX,int pointerY,int clicks) override;
    void MouseUp(int pointerX,int pointerY,int clicks) override;
    void MouseDrag(int pointerX,int pointerY) override;
    void MouseLeave() override;
    void LostFocus() override;
    void KeyDown(Sexy::KeyCode key) override;
    void MouseWheel(int delta) override;
    const std::vector<Control>& Controls() const { return mControls; }
    const TeamProgressionView::Snapshot& PresentedSnapshot() const { return mSnapshot; }
    bool Closed() const { return mClosed; }
    void CancelInput();

private:
    TeamProgressionView::Controller* mController;
    TeamProgressionView::Snapshot mSnapshot;
    std::vector<Control> mControls;
    Control mPressed{};
    TeamProgressionView::Context mPressContext;
    bool mCaptured=false,mClosed=false,mPersonal=false,mCellPicker=false,mDetail=false;
    int mMode=0,mListPage=0,mSelectedEntry=0,mTextPage=0,mKeyboard=-1;
    uint64_t mSelectedPeer=0;
    int mDesiredRow=-1,mDesiredColumn=-1;
    std::string mDetailText;
    mutable std::map<std::tuple<std::string,int,int,int>,std::vector<std::string>> mWrapped;
    static bool SameContext(const TeamProgressionView::Context& left,const TeamProgressionView::Context& right);
    static bool SameControl(const Control& left,const Control& right);
    const std::vector<std::string>& Wrap(const std::string& value,int width,int size) const;
    void Refresh();
    void Layout();
    void Add(ControlKind kind,Sexy::Rect rect,const std::string& label,int value=0,
        bool enabled=true,uint64_t target=0,uint64_t quote=0,int extra=0,bool selected=false);
    const Control* Hit(int pointerX,int pointerY) const;
    const TeamProgressionView::Peer* SelectedPeer() const;
    bool CanAct() const;
    bool CanTarget() const;
    bool CanSupport() const;
    int EntryCount() const;
    int TextPages(const std::string& text) const;
    std::string EntryText() const;
    void Activate(Control control);
    void Submit(TeamProgressionView::Action action,int value=0,uint64_t target=0,
        uint64_t quote=0,int row=-1,int column=-1);
    void Lines(Sexy::Graphics* graphics,const std::string& value,Sexy::Rect box,int size,Sexy::Color color,int first=0) const;
    void DrawMembers(Sexy::Graphics* graphics,bool voting) const;
    void DrawVote(Sexy::Graphics* graphics) const;
    void DrawEntries(Sexy::Graphics* graphics) const;
    void DrawPeers(Sexy::Graphics* graphics) const;
};
