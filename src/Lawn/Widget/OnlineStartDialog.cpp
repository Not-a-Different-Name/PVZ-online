#include "MobileCoopDraw.h"
#include "OnlineStartDialog.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../ModText.h"
#include "../../PvzpLib/PvzpStringFile.h"
#include "widget/ButtonWidget.h"
#include "graphics/Graphics.h"

OnlineStartDialog::OnlineStartDialog(LawnApp* app, const char* title, const char* body,
    const char* yes, const char* no, Notify notify, bool, int id)
    : Dialog(nullptr, nullptr, id, true, "", "", "", BUTTONS_NONE),
      mApp(app), mNotify(notify), mTitle(title ? title : ""), mBody(body ? body : "")
{
    if(mBody.find('\f')!=std::string::npos){
        size_t start=0;
        for(;;){auto end=mBody.find('\f',start);mPages.push_back(mBody.substr(start,end==std::string::npos?end:end-start));if(end==std::string::npos)break;start=end+1;}
    }
    const bool paged=!mPages.empty();
    Resize(0, 0, 660, paged?460:310);
    const int count = (yes != nullptr) + (no != nullptr);
    auto add = [&](int buttonId, const char* label, int index) {
        auto b = std::make_unique<MobileStoneButton>(buttonId, this);
        b->mLabel = label;
        b->SetFont(ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18, 26));
        b->SetLabelColor(Sexy::Color(245, 230, 165));
        b->SetBkgColor(Sexy::Color(37, 83, 55));
        b->Resize(count == 1 ? 200 : 75 + index * 300, paged?385:235, 260, 55);
        AddWidget(b.get());
        mButtons.push_back(std::move(b));
    };
    if (yes) add(ID_YES, yes, 0);
    if (no) add(ID_NO, no, yes ? 1 : 0);
    if(paged){
        add(10001,ModText::Tr("上一页","Previous"),0);mButtons.back()->Resize(30,385,150,55);mButtons.back()->SetDisabled(true);
        add(10002,ModText::Tr("下一页","Next"),1);mButtons.back()->Resize(480,385,150,55);
    }
}
OnlineStartDialog::~OnlineStartDialog() { for (auto& b : mButtons) RemoveWidget(b.get()); }
void OnlineStartDialog::SetBody(const char* body) { mBody = body ? body : ""; MarkDirty(); }
void OnlineStartDialog::Draw(Sexy::Graphics* g)
{
    DrawMobilePanel(g, Sexy::Rect(0,0,mWidth,mHeight));
    g->SetColor(Sexy::Color(245, 230, 165));
    g->SetFont(ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT24, 34));
    g->DrawString(mTitle, 30, 52);
    PvzpDrawStringWrapped(g, mPages.empty()?mBody:mPages[mPage], Sexy::Rect(30, 78, 600, mPages.empty()?145:260),
        ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18, mPages.empty()?24:22),
        Sexy::Color(245, 230, 165), DrawStringJustification::DS_ALIGN_LEFT);
    if(!mPages.empty()){
        g->SetFont(ModText::FontFor(Sexy::FONT_DWARVENTODCRAFT18,18));
        g->DrawString(std::to_string(mPage+1)+" / "+std::to_string(mPages.size()),300,365);
    }
}
void OnlineStartDialog::ButtonDepress(int id)
{
    if(!mPages.empty()){
        if(id==10001||id==10002){
            if(id==10001&&mPage>0)--mPage;
            if(id==10002&&mPage+1<mPages.size())++mPage;
            for(auto& b:mButtons){if(b->mId==10001)b->SetDisabled(mPage==0);if(b->mId==10002)b->SetDisabled(mPage+1==mPages.size());}
            MarkDirty();return;
        }
        if(mNotify==NOTIFY_NONE){mApp->KillDialog(mId);return;}
    }
    // Copy before notification: the app may remove this dialog immediately.
    auto* app = mApp; const auto notify = mNotify;
    if (notify == NOTIFY_INVITE_ANSWER) app->OnlineStartPromptAnswer(id == ID_YES);
    else if (notify == NOTIFY_WAIT_CANCEL) app->OnlineStartWaitCancelled();
    else if (notify == NOTIFY_RECONNECT_CANCEL) app->OnlineReconnectCancelled();
    else Dialog::ButtonDepress(id);
}
void OnlineStartDialog::KeyDown(Sexy::KeyCode key)
{
    if (key == Sexy::KEYCODE_ESCAPE && !mButtons.empty())
        ButtonDepress(mPages.empty()?mButtons.back()->mId:ID_YES);
}
