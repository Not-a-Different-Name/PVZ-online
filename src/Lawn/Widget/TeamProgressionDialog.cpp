#include "TeamProgressionDialog.h"
#include "NameplateDraw.h"
#include "NameplatePresentation.h"
#include "OnlineUiDraw.h"
#include "widget/WidgetManager.h"
#include <algorithm>

using namespace Sexy;
using namespace TeamProgressionView;

TeamProgressionDialog::TeamProgressionDialog(Controller& controller):mController(&controller)
{
    Resize(0,0,800,600);
    mHasAlpha=true;
    mWantsFocus=true;
    mSnapshot=controller.Read();
    mSelectedPeer=mSnapshot.support.target;
    mDesiredRow=mSnapshot.support.row;
    mDesiredColumn=mSnapshot.support.column;
    Layout();
}

bool TeamProgressionDialog::SameContext(const Context& left,const Context& right)
{
    return left.run==right.run&&left.player==right.player&&left.level==right.level&&
        left.attempt==right.attempt&&left.term==right.term&&left.revision==right.revision;
}

bool TeamProgressionDialog::SameControl(const Control& left,const Control& right)
{
    return left.kind==right.kind&&left.value==right.value&&left.extra==right.extra&&
        left.target==right.target&&left.quote==right.quote&&left.label==right.label;
}

const std::vector<std::string>& TeamProgressionDialog::Wrap(const std::string& value,int width,int size) const
{
    const auto key=std::make_tuple(value,width,size,ModText::IsChinese()?1:0);
    if(const auto found=mWrapped.find(key);found!=mWrapped.end())return found->second;
    if(mWrapped.size()>=64)mWrapped.clear();
    std::vector<std::string> lines;
    std::string line;
    for(size_t offset=0;offset<value.size();)
    {
        size_t next=offset+1;
        while(next<value.size()&&(static_cast<unsigned char>(value[next])&0xc0)==0x80)++next;
        const std::string glyph=value.substr(offset,next-offset);
        if(glyph=="\n") { lines.push_back(line);line.clear(); }
        else
        {
            if(!line.empty()&&SeatUi::Width(line+glyph,size)>width)
            {
                const size_t space=line.find_last_of(' ');
                if(space!=std::string::npos&&space>0) {lines.push_back(line.substr(0,space));line.erase(0,space+1);}
                else {lines.push_back(line);line.clear();}
            }
            if(!line.empty()||glyph!=" ")line+=glyph;
        }
        offset=next;
    }
    if(!line.empty())lines.push_back(line);
    return mWrapped.emplace(key,std::move(lines)).first->second;
}

void TeamProgressionDialog::CancelInput() { mCaptured=false;mKeyboard=-1;MarkDirty(); }
void TeamProgressionDialog::LostFocus() { Widget::LostFocus();CancelInput(); }
void TeamProgressionDialog::MouseLeave() { Widget::MouseLeave();CancelInput(); }

void TeamProgressionDialog::Refresh()
{
    Snapshot next=mController->Read();
    if(!SameContext(mSnapshot.context,next.context)||mSnapshot.page!=next.page)
    {
        CancelInput();mTextPage=0;mDetail=false;mCellPicker=false;
        if(mSnapshot.page!=next.page) {mDetail=false;mCellPicker=false;mListPage=0;mSelectedEntry=0;}
        if(mSnapshot.context.run!=next.context.run||mSnapshot.context.player!=next.context.player||
            mSnapshot.context.level!=next.context.level||mSnapshot.context.attempt!=next.context.attempt||mSnapshot.context.term!=next.context.term)
        {
            mSelectedPeer=next.support.target;mDesiredRow=next.support.row;mDesiredColumn=next.support.column;
            mDetail=false;mCellPicker=false;mListPage=0;mSelectedEntry=0;
        }
    }
    mSnapshot=std::move(next);
    if(mSelectedPeer&&!SelectedPeer())mSelectedPeer=0;
    if(!mSelectedPeer&&mSnapshot.support.target&&SelectedPeer()==nullptr)
    {
        for(const auto& peer:mSnapshot.peers)if(peer.player==mSnapshot.support.target)mSelectedPeer=peer.player;
    }
    Layout();
}

void TeamProgressionDialog::Update()
{
    Widget::Update();
    if(mClosed)return;
    Refresh();MarkDirty();
}

const Peer* TeamProgressionDialog::SelectedPeer() const
{
    for(const auto& peer:mSnapshot.peers)if(peer.player&&peer.player==mSelectedPeer)return &peer;
    return nullptr;
}

bool TeamProgressionDialog::CanAct() const
{
    return mSnapshot.canAct&&mSnapshot.phase!=Phase::RECOVERING&&mSnapshot.phase!=Phase::DISABLED;
}

bool TeamProgressionDialog::CanTarget() const
{
    const Peer* peer=SelectedPeer();
    return CanAct()&&peer&&peer->online&&peer->player!=mSnapshot.context.player;
}

bool TeamProgressionDialog::CanSupport() const
{
    const auto& support=mSnapshot.support;
    return CanTarget()&&support.enabled&&support.canConfirm&&!support.pending&&support.quoteId&&
        support.target==mSelectedPeer&&support.cost>=0&&support.cooldownSeconds<=0&&
        support.row==mDesiredRow&&support.column==mDesiredColumn&&support.row>=0&&support.row<6&&support.column>=0&&support.column<9;
}

int TeamProgressionDialog::EntryCount() const
{
    return static_cast<int>(mSnapshot.page==Page::GOALS?mSnapshot.goals.size():
        mPersonal?mSnapshot.personalEffects.size():mSnapshot.teamEffects.size());
}

std::string TeamProgressionDialog::EntryText() const
{
    const int index=std::clamp(mSelectedEntry,0,std::max(0,EntryCount()-1));
    if(EntryCount()==0)return ModText::Tr("暂无此类记录。","No records in this category.");
    if(mSnapshot.page==Page::GOALS)
    {
        const auto& goal=mSnapshot.goals[index];
        return goal.name+"\n"+(goal.earned?ModText::Tr("已达成","Completed"):ModText::Tr("进行中","In progress"))+"\n"+goal.progress+"\n"+goal.description;
    }
    const auto& effect=(mPersonal?mSnapshot.personalEffects:mSnapshot.teamEffects)[index];
    return effect.name+"\n"+ModText::Tr("层数：","Stacks: ")+std::to_string(effect.stacks)+"\n"+effect.source+"\n"+effect.description;
}

int TeamProgressionDialog::TextPages(const std::string& text) const
{
    return std::max(1,(static_cast<int>(Wrap(text,mDetail?656:408,mDetail?20:18).size())+8)/9);
}

void TeamProgressionDialog::Add(ControlKind kind,Rect rect,const std::string& label,int value,bool enabled,
    uint64_t target,uint64_t quote,int extra,bool selected)
{
    mControls.push_back({kind,rect,label,value,extra,target,quote,enabled,selected});
}

void TeamProgressionDialog::Layout()
{
    mControls.clear();
    if(mClosed)return;
    Add(ControlKind::CLOSE,Rect(660,36,92,48),ModText::Tr("收起","Close"));
    const char* labels[]={ModText::Tr("团队投票","Vote"),ModText::Tr("全队增益","Effects"),ModText::Tr("队友协作","Team"),ModText::Tr("本关简报","Brief"),ModText::Tr("目标进度","Goals")};
    for(int index=0;index<5;++index)Add(ControlKind::PAGE,Rect(48+index*142,98,136,44),labels[index],index,true,0,0,0,static_cast<int>(mSnapshot.page)==index);
    if(mDetail)
    {
        mTextPage=std::clamp(mTextPage,0,TextPages(mDetailText)-1);
        Add(ControlKind::TEXT_BACK,Rect(48,500,236,44),ModText::Tr("返回内容","Back"));
        Add(ControlKind::TEXT_PAGE,Rect(326,500,108,44),"<",-1,mTextPage>0);
        Add(ControlKind::TEXT_PAGE,Rect(644,500,108,44),">",1,mTextPage+1<TextPages(mDetailText));
        return;
    }
    Add(ControlKind::STATUS_DETAILS,Rect(664,154,88,44),ModText::Tr("说明","Info"));
    if(mCellPicker)
    {
        for(int row=0;row<6;++row)for(int column=0;column<9;++column)
            Add(ControlKind::CELL,Rect(72+column*73,208+row*46,64,44),std::to_string(row+1)+"-"+std::to_string(column+1),
                row,CanTarget()&&!mSnapshot.support.pending,mSelectedPeer,mSnapshot.support.quoteId,column,row==mDesiredRow&&column==mDesiredColumn);
        Add(ControlKind::TEXT_BACK,Rect(48,500,264,44),ModText::Tr("返回支援预览","Back to preview"));
        return;
    }
    if(mSnapshot.page==Page::VOTE)
    {
        const int count=std::min(3,static_cast<int>(mSnapshot.choices.size()));
        if(count&&mSnapshot.phase!=Phase::RECOVERING)
        {
            const int width=(704-(count-1)*16)/count;
            for(int index=0;index<count;++index)
            {
                const auto& choice=mSnapshot.choices[index];const int left=48+index*(width+16);
                Add(ControlKind::VOTE,Rect(left+8,382,width-104,44),count==1?ModText::Tr("确认领取","Claim"):ModText::Tr("投票","Vote"),choice.id,
                    mSnapshot.canAct&&mSnapshot.phase==Phase::VOTING&&choice.enabled);
                Add(ControlKind::CHOICE_DETAILS,Rect(left+width-88,382,80,44),ModText::Tr("详情","Info"),index);
            }
        }
        return;
    }
    if(mSnapshot.page==Page::TEAM_EFFECTS||mSnapshot.page==Page::GOALS)
    {
        if(mSnapshot.page==Page::TEAM_EFFECTS)
        {
            Add(ControlKind::MODE,Rect(48,208,116,44),ModText::Tr("全队","Team"),0,true,0,0,0,!mPersonal);
            Add(ControlKind::MODE,Rect(176,208,116,44),ModText::Tr("本人","Mine"),1,true,0,0,0,mPersonal);
        }
        const int count=EntryCount();const int pages=std::max(1,(count+3)/4);
        mListPage=std::clamp(mListPage,0,pages-1);mSelectedEntry=std::clamp(mSelectedEntry,0,std::max(0,count-1));
        if(mSelectedEntry/4!=mListPage)mSelectedEntry=mListPage*4;
        for(int index=0;index<4&&mListPage*4+index<count;++index)
        {
            const int entry=mListPage*4+index;
            const std::string name=mSnapshot.page==Page::GOALS?mSnapshot.goals[entry].name:(mPersonal?mSnapshot.personalEffects:mSnapshot.teamEffects)[entry].name;
            Add(ControlKind::ENTRY,Rect(48,264+index*54,244,46),SeatUi::Fit(name,220,17),entry,true,0,0,0,entry==mSelectedEntry);
        }
        mTextPage=std::clamp(mTextPage,0,TextPages(EntryText())-1);
        Add(ControlKind::LIST_PAGE,Rect(48,500,104,44),"<",-1,mListPage>0);
        Add(ControlKind::LIST_PAGE,Rect(188,500,104,44),">",1,mListPage+1<pages);
        Add(ControlKind::TEXT_PAGE,Rect(326,500,104,44),"<",-1,mTextPage>0);
        Add(ControlKind::TEXT_PAGE,Rect(648,500,104,44),">",1,mTextPage+1<TextPages(EntryText()));
        return;
    }
    if(mSnapshot.page==Page::TEAMMATES)
    {
        for(int index=0;index<std::min(6,static_cast<int>(mSnapshot.peers.size()));++index)
        {
            const auto& peer=mSnapshot.peers[index];
            Add(ControlKind::PEER,Rect(48+(index%3)*240,212+(index/3)*56,224,50),"",index,peer.player!=0,peer.player,0,0,peer.player==mSelectedPeer);
        }
        const char* modes[]={ModText::Tr("阳光","Sun"),ModText::Tr("借用","Loan"),ModText::Tr("观战","Watch"),ModText::Tr("樱桃支援","Cherry aid")};
        for(int index=0;index<4;++index)Add(ControlKind::MODE,Rect(48+index*178,332,166,44),modes[index],index,true,0,0,0,index==mMode);
        if(mMode==0)
        {
            Add(ControlKind::SUN_REQUEST,Rect(48,500,276,44),ModText::Tr("请求阳光","Request sun"),0,CanAct());
            Add(ControlKind::SUN_GIFT,Rect(476,500,276,44),ModText::Tr("赠送阳光","Give sun"),0,CanTarget(),mSelectedPeer);
        }
        if(mMode==1)Add(ControlKind::LOAN,Rect(440,500,312,44),ModText::Tr("打开借用目录","Open loan catalog"),0,CanTarget(),mSelectedPeer);
        if(mMode==2)Add(ControlKind::WATCH,Rect(440,500,312,44),ModText::Tr("查看队友草坪","Watch teammate"),0,CanTarget(),mSelectedPeer);
        if(mMode==3)
        {
            Add(ControlKind::PICK_CELL,Rect(48,500,196,44),ModText::Tr("选择格位","Choose cell"),0,CanTarget()&&!mSnapshot.support.pending);
            Add(ControlKind::SUPPORT_PREVIEW,Rect(256,500,196,44),ModText::Tr("核对花费","Check cost"),0,CanTarget()&&!mSnapshot.support.pending,mSelectedPeer);
            Add(ControlKind::SUPPORT_CONFIRM,Rect(464,500,288,44),mSnapshot.support.pending?ModText::Tr("待确认","Pending"):ModText::Tr("确认支援","Confirm aid"),mSnapshot.support.cost,CanSupport(),mSelectedPeer,mSnapshot.support.quoteId,
                std::clamp(mSnapshot.support.row,-1,5)*9+std::clamp(mSnapshot.support.column,-1,8));
            const auto own=std::find_if(mSnapshot.peers.begin(),mSnapshot.peers.end(),[this](const Peer& peer){return peer.player==mSnapshot.context.player;});
            if(own!=mSnapshot.peers.end())Add(ControlKind::PERMISSION,Rect(48,440,244,44),own->allowSupport?ModText::Tr("撤回我的许可","Revoke my consent"):ModText::Tr("允许支援我","Allow aid to me"),own->allowSupport?0:1,CanAct(),mSnapshot.context.player);
        }
        return;
    }
    Add(ControlKind::STATUS_DETAILS,Rect(48,500,276,44),ModText::Tr("查看完整简报","Full brief"));
}

const TeamProgressionDialog::Control* TeamProgressionDialog::Hit(int pointerX,int pointerY) const
{
    for(const auto& control:mControls)if(control.enabled&&control.rect.Contains(pointerX,pointerY))return &control;
    return nullptr;
}

void TeamProgressionDialog::MouseDown(int pointerX,int pointerY,int clicks)
{
    Refresh();CancelInput();
    if(clicks<=0||mClosed)return;
    if(const auto* control=Hit(pointerX,pointerY)) {mPressed=*control;mPressContext=mSnapshot.context;mCaptured=true;MarkDirty();}
}

void TeamProgressionDialog::MouseDrag(int pointerX,int pointerY)
{
    if(mCaptured&&!mPressed.rect.Contains(pointerX,pointerY))CancelInput();
}

void TeamProgressionDialog::MouseUp(int pointerX,int pointerY,int clicks)
{
    const bool captured=mCaptured;const Control pressed=mPressed;const Context context=mPressContext;
    CancelInput();Refresh();
    const auto* hit=Hit(pointerX,pointerY);
    if(!captured||clicks<=0||mClosed||!SameContext(context,mSnapshot.context)||!hit||!SameControl(pressed,*hit))return;
    Activate(*hit);
}

void TeamProgressionDialog::Submit(Action action,int value,uint64_t target,uint64_t quote,int row,int column)
{
    Intent intent;intent.action=action;intent.context=mSnapshot.context;intent.value=value;
    intent.target=target;intent.quoteId=quote;intent.row=row;intent.column=column;
    if(action==Action::VOTE)intent.choice=value;
    mController->Submit(intent);
}

void TeamProgressionDialog::Activate(Control control)
{
    CancelInput();
    switch(control.kind)
    {
    case ControlKind::CLOSE: mClosed=true;SetVisible(false);Submit(Action::CLOSE);return;
    case ControlKind::PAGE: Submit(Action::SELECT_PAGE,control.value);return;
    case ControlKind::VOTE: Submit(Action::VOTE,control.value);return;
    case ControlKind::PEER: mSelectedPeer=control.target;mDesiredRow=mDesiredColumn=-1;Submit(Action::SELECT_PEER,0,control.target);return;
    case ControlKind::MODE:
        if(mSnapshot.page==Page::TEAM_EFFECTS){mPersonal=control.value==1;mListPage=mSelectedEntry=mTextPage=0;}
        else mMode=control.value;
        break;
    case ControlKind::LIST_PAGE:mListPage=std::max(0,mListPage+control.value);mSelectedEntry=mListPage*4;mTextPage=0;break;
    case ControlKind::TEXT_PAGE:mTextPage=std::max(0,mTextPage+control.value);break;
    case ControlKind::ENTRY:mSelectedEntry=control.value;mTextPage=0;break;
    case ControlKind::CHOICE_DETAILS:
        if(control.value>=0&&control.value<static_cast<int>(mSnapshot.choices.size()))
        {
            const auto& choice=mSnapshot.choices[control.value];
            mDetailText=choice.name+"\n"+choice.description+"\n"+choice.unavailableReason;mDetail=true;mTextPage=0;
        }
        break;
    case ControlKind::STATUS_DETAILS:
        mDetailText=mSnapshot.status+"\n"+mSnapshot.briefTitle+"\n"+mSnapshot.briefSummary+"\n"+mSnapshot.damageSummary;
        if(mSnapshot.page==Page::TEAMMATES)mDetailText+="\n"+mSnapshot.support.reason;
        for(const auto& peer:mSnapshot.peers)mDetailText+="\nP"+std::to_string(peer.seat)+" "+peer.name+"\n"+peer.phase+" / "+peer.voteStatus;
        mDetail=true;mTextPage=0;break;
    case ControlKind::TEXT_BACK:mDetail=false;mCellPicker=false;mTextPage=0;break;
    case ControlKind::PICK_CELL:mCellPicker=true;break;
    case ControlKind::CELL:
        mDesiredRow=control.value;mDesiredColumn=control.extra;
        Submit(Action::SELECT_CELL,0,control.target,control.quote,control.value,control.extra);return;
    case ControlKind::SUN_REQUEST:Submit(Action::SUN_REQUEST);return;
    case ControlKind::SUN_GIFT:Submit(Action::SUN_GIFT,0,control.target);return;
    case ControlKind::LOAN:Submit(Action::LOAN,0,control.target);return;
    case ControlKind::WATCH:Submit(Action::WATCH,0,control.target);return;
    case ControlKind::SUPPORT_PREVIEW:Submit(Action::SUPPORT_PREVIEW,0,control.target,0,mDesiredRow,mDesiredColumn);return;
    case ControlKind::SUPPORT_CONFIRM:
        Submit(Action::SUPPORT_CONFIRM,0,control.target,control.quote,mSnapshot.support.row,mSnapshot.support.column);return;
    case ControlKind::PERMISSION:Submit(Action::SUPPORT_PERMISSION,control.value,control.target);return;
    }
    Layout();MarkDirty();
}

void TeamProgressionDialog::KeyDown(KeyCode key)
{
    if(mClosed)return;
    Refresh();
    if(key==KEYCODE_ESCAPE) {Control close;close.kind=ControlKind::CLOSE;Activate(close);return;}
    if(key==KEYCODE_TAB)
    {
        for(size_t count=0;count<mControls.size();++count)
        {
            mKeyboard=(mKeyboard+1)%static_cast<int>(mControls.size());
            if(mControls[mKeyboard].enabled)break;
        }
        MarkDirty();return;
    }
    if((key==KEYCODE_RETURN||key==KEYCODE_SPACE)&&mKeyboard>=0&&mKeyboard<static_cast<int>(mControls.size())&&mControls[mKeyboard].enabled)
    {
        if(!mWidgetManager||!mWidgetManager->mIsKeyRepeat)Activate(mControls[mKeyboard]);
    }
}

void TeamProgressionDialog::MouseWheel(int delta)
{
    if(mClosed||!delta)return;
    Refresh();
    if(mDetail||mSnapshot.page==Page::TEAM_EFFECTS||mSnapshot.page==Page::GOALS)
    {
        const int pages=TextPages(mDetail?mDetailText:EntryText());
        mTextPage=std::clamp(mTextPage+(delta<0?1:-1),0,pages-1);Layout();MarkDirty();
    }
}

void TeamProgressionDialog::Lines(Graphics* graphics,const std::string& value,Rect box,int size,Color color,int first) const
{
    Graphics clipped(*graphics);clipped.ClipRect(box);
    const auto& lines=Wrap(value,box.mWidth,size);
    const int height=size+6;
    for(int index=std::max(0,first);index<static_cast<int>(lines.size())&&(index-first+1)*height<=box.mHeight;++index)
        SeatUi::Text(&clipped,lines[index],box.mX,box.mY+size+(index-first)*height,box.mWidth,size,color);
}

void TeamProgressionDialog::DrawMembers(Graphics* graphics,bool voting) const
{
    for(int index=0;index<std::min(6,static_cast<int>(mSnapshot.peers.size()));++index)
    {
        const auto& peer=mSnapshot.peers[index];
        Rect box(48+(index%3)*240,(voting?444:212)+(index/3)*(voting?44:56),224,voting?40:50);
        if(voting)SeatUi::Strip(graphics,box);
        const int nameplate=NameplatePresentation::ForPlayer(gLawnApp,peer.player);
        if(nameplate)NameplateDraw::Draw(graphics,Rect(box.mX,box.mY,box.mWidth,26),nameplate,"P"+std::to_string(peer.seat)+" "+peer.name);
        else SeatUi::Text(graphics,"P"+std::to_string(peer.seat)+" "+peer.name,box.mX+8,box.mY+18,box.mWidth-16,16,Color(245,230,165));
        SeatUi::Text(graphics,voting?peer.voteStatus:peer.phase,box.mX+8,box.mY+(voting?35:40),box.mWidth-16,14,Color(215,215,195));
    }
}

void TeamProgressionDialog::DrawVote(Graphics* graphics) const
{
    const int count=std::min(3,static_cast<int>(mSnapshot.choices.size()));
    if(!count||mSnapshot.phase==Phase::RECOVERING)
        Lines(graphics,mSnapshot.status,Rect(72,242,656,170),22,Color(235,230,210));
    else
    {
        const int width=(704-(count-1)*16)/count;
        for(int index=0;index<count;++index)
        {
            const auto& choice=mSnapshot.choices[index];const int left=48+index*(width+16);
            SeatUi::Strip(graphics,Rect(left,218,width,218));
            Lines(graphics,choice.name,Rect(left+10,226,width-20,52),20,Color(245,230,165));
            SeatUi::Text(graphics,ModText::Tr("层数 ", "Stacks ")+std::to_string(choice.stacks),left+10,301,width-20,16,Color(215,215,195));
            // Full rules remain behind the existing Info button; keep this three-line preview visible.
            const char* previews[]={
                ModText::Tr("共享植物与模仿者。\n不含个人增益或借卡。\n最多1层。", "Share plants + Imitater.\nNo personal buffs or loans.\nMax 1 stack."),
                ModText::Tr("首人通关：每层+1000阳光。\n战斗队友刷新卡片。\n每关一次。", "First clear: 1000/stack.\nFighting allies: sun +\ncard refresh. Once/level."),
                ModText::Tr("每层1爆，只付1次樱桃费。\n冷却20秒，须队友允许。\n使用本人伤害与增益。", "1 blast/stack; pay once.\nCherry, your dmg/buffs.\n20s CD; needs consent.")};
            const std::string preview=choice.id>=0&&choice.id<3?previews[choice.id]:choice.description;
            Lines(graphics,!choice.enabled&&!choice.unavailableReason.empty()?choice.unavailableReason:preview,
                Rect(left+10,310,width-20,66),14,Color(235,230,210));
        }
    }
    DrawMembers(graphics,true);
}

void TeamProgressionDialog::DrawEntries(Graphics* graphics) const
{
    SeatUi::Strip(graphics,Rect(312,208,440,282));
    Lines(graphics,EntryText(),Rect(326,220,408,216),18,Color(235,230,210),mTextPage*9);
    if(mSnapshot.page==Page::GOALS&&EntryCount()>0)
    {
        const auto& goal=mSnapshot.goals[std::clamp(mSelectedEntry,0,EntryCount()-1)];
        graphics->SetColor(Color(35,42,38));graphics->FillRect(326,468,408,10);
        graphics->SetColor(Color(135,175,85));graphics->FillRect(326,468,408*std::clamp(goal.progressPermille,0,1000)/1000,10);
    }
    SeatUi::Text(graphics,std::to_string(mListPage+1)+"/"+std::to_string(std::max(1,(EntryCount()+3)/4)),154,530,32,16,Color(235,230,210),true);
    SeatUi::Text(graphics,std::to_string(mTextPage+1)+"/"+std::to_string(TextPages(EntryText())),440,530,198,16,Color(235,230,210),true);
}

void TeamProgressionDialog::DrawPeers(Graphics* graphics) const
{
    if(mMode==3)
    {
        const auto& support=mSnapshot.support;
        const std::string quote=ModText::Tr("消耗阳光：","Sun cost: ")+std::to_string(support.cost)+
            ModText::Tr(" · 剩余冷却 "," · Cooldown ")+std::to_string(support.cooldownSeconds);
        Lines(graphics,quote,Rect(312,388,440,48),18,Color(245,230,165));
        Lines(graphics,support.reason,Rect(312,440,440,48),16,Color(225,225,205));
        SeatUi::Text(graphics,mDesiredRow>=0&&mDesiredColumn>=0?ModText::Tr("格位 ","Cell ")+std::to_string(mDesiredRow+1)+"-"+std::to_string(mDesiredColumn+1):ModText::Tr("尚未选格位","No cell selected"),48,412,244,18,Color(235,230,210));
    }
    else
    {
        const char* descriptions[]={ModText::Tr("选队友再点“赠送阳光”：付100，预计到账90。也可点“请求阳光”。", "Choose a teammate and Give sun: pay 100, expected delivery 90. Or choose Request sun."),
            ModText::Tr("选队友，再打开借用目录申请植物。", "Choose a teammate, then open the loan catalog."),
            ModText::Tr("选择队友查看草坪；你自己的战斗仍会继续。","Choose a teammate to watch. Your own battle continues.")};
        Lines(graphics,descriptions[mMode],Rect(48,404,704,80),20,Color(235,230,210));
    }
}

void TeamProgressionDialog::Draw(Graphics* graphics)
{
    if(mClosed)return;
    SeatUi::Panel(graphics,Rect(20,20,760,560));
    const char* titles[]={ModText::Tr("团队成长","Team growth"),ModText::Tr("本局增益","Run effects"),ModText::Tr("队友协作","Teamwork"),ModText::Tr("本关简报","Stage brief"),ModText::Tr("目标进度","Goal progress")};
    const int page=std::clamp(static_cast<int>(mSnapshot.page),0,4);
    SeatUi::Text(graphics,titles[page],48,70,588,27,Color(245,230,165));
    if(mDetail)
    {
        Lines(graphics,mDetailText,Rect(72,218,656,234),20,Color(235,230,210),mTextPage*9);
        SeatUi::Text(graphics,std::to_string(mTextPage+1)+"/"+std::to_string(TextPages(mDetailText)),444,530,192,16,Color(235,230,210),true);
    }
    else
    {
        Lines(graphics,mSnapshot.status,Rect(48,151,604,mSnapshot.page==Page::TEAMMATES?23:48),17,Color(215,215,195));
        if(mCellPicker)SeatUi::Text(graphics,ModText::Tr("选择位置，再点“返回支援预览”核对花费。", "Choose a cell, then Back to preview to check the cost."),48,195,604,17,Color(235,230,210));
        else if(mSnapshot.page==Page::VOTE)DrawVote(graphics);
        else if(mSnapshot.page==Page::TEAM_EFFECTS||mSnapshot.page==Page::GOALS)DrawEntries(graphics);
        else if(mSnapshot.page==Page::TEAMMATES)
        {
            if(const auto* peer=SelectedPeer())SeatUi::Text(graphics,"P"+std::to_string(peer->seat)+" "+peer->name,48,199,604,17,Color(245,230,165));
            DrawPeers(graphics);
        }
        else
        {
            Lines(graphics,mSnapshot.briefTitle,Rect(48,220,704,60),26,Color(245,230,165));
            Lines(graphics,mSnapshot.briefSummary,Rect(48,292,704,108),20,Color(235,230,210));
            Lines(graphics,mSnapshot.damageSummary,Rect(48,416,704,72),18,Color(235,230,210));
        }
    }
    for(size_t index=0;index<mControls.size();++index)
    {
        const auto& control=mControls[index];
        SeatUi::Button(graphics,control.rect,control.label.c_str(),control.enabled,
            control.selected||(mCaptured&&SameControl(mPressed,control))||static_cast<int>(index)==mKeyboard);
    }
    if(!mDetail&&!mCellPicker&&mSnapshot.page==Page::TEAMMATES)DrawMembers(graphics,false);
    SeatUi::Text(graphics,ModText::Tr("查看时战斗继续", "Battle continues while viewing"),48,567,704,15,Color(185,185,170));
}
