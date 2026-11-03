#include "Trail.h"
#include "TodDebug.h"
#include "Attachment.h"
#include "Reanimator.h"
#include "TodParticle.h"
#include "EffectSystem.h"
#include "graphics/Graphics.h"

Attachment::Attachment()
{
	mNumEffects = 0;
	mDead = false;
}

Attachment::~Attachment()
{
	AttachmentDie();
}

//0x404490
void Attachment::Update()
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		bool isEmpty = true;
		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem && !aParticleSystem->mDead)
			{
				aParticleSystem->Update();
				isEmpty = false;
			}
			break;
		}

		case EffectType::EFFECT_TRAIL:
		{
			Trail* aTrail = gEffectSystem->mTrailHolder->mTrails.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aTrail && !aTrail->mDead)
			{
				aTrail->Update();
				isEmpty = false;
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation && !aReanimation->mDead)
			{
				aReanimation->Update();
				isEmpty = false;
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->Update();
				isEmpty = false;
			}
			break;
		}

		default:
			TOD_ASSERT();
			break;
		}

		if (isEmpty)
		{
			int aNumEffectsRemaining = mNumEffects - i - 1;
			if (aNumEffectsRemaining > 0)
			{
				memmove(aAttachEffect, aAttachEffect + 1, aNumEffectsRemaining * sizeof(AttachEffect));
				--i;
			}
			mNumEffects--;
		}
	}

	if (mNumEffects == 0)
	{
		mDead = true;
	}
}

//0x404610
void Attachment::SetPosition(const SexyVector2& thePosition)
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		SexyVector2 aNewPos = aAttachEffect->mOffset * thePosition;

		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->SystemMove(aNewPos.x, aNewPos.y);
			}
			break;
		}

		case EffectType::EFFECT_TRAIL:
		{
			Trail* aTrail = gEffectSystem->mTrailHolder->mTrails.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aTrail)
			{
				aTrail->AddPoint(aNewPos.x, aNewPos.y);
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				aReanimation->SetPosition(aNewPos.x, aNewPos.y);
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->SetPosition(aNewPos);
			}
			break;
		}

		case EffectType::EFFECT_OTHER:
			break;
		}
	}
}

//0x404780
void Attachment::OverrideColor(const Color& theColor)
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->OverrideColor("", theColor);
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				aReanimation->mColorOverride = theColor;
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->OverrideColor(theColor);
			}
			break;
		}
		case EffectType::EFFECT_TRAIL:
		case EffectType::EFFECT_OTHER:
			break;
		}
	}
}

//0x404890
void Attachment::PropogateColor(const Color& theColor, bool theEnableAdditiveColor, const Color& theAdditiveColor, bool theEnableOverlayColor, const Color& theOverlayColor)
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		if (aAttachEffect->mDontPropogateColor)
		{
			continue;
		}

		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->OverrideColor(nullptr, theColor);
				aParticleSystem->OverrideExtraAdditiveDraw(nullptr, theEnableAdditiveColor);
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				aReanimation->mColorOverride = theColor;
				aReanimation->mExtraAdditiveColor = theAdditiveColor;
				aReanimation->mEnableExtraAdditiveDraw = theEnableAdditiveColor;
				aReanimation->mExtraOverlayColor = theOverlayColor;
				aReanimation->mEnableExtraOverlayDraw = theEnableOverlayColor;
				aReanimation->PropogateColorToAttachments();
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->PropogateColor(theColor, theEnableAdditiveColor, theAdditiveColor, theEnableOverlayColor, theOverlayColor);
			}
			break;
		}
		case EffectType::EFFECT_TRAIL:
		case EffectType::EFFECT_OTHER:
			break;
		}
	}
}

//0x404A40
void Attachment::OverrideScale(float theScale)
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->OverrideScale(nullptr, theScale);
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				aReanimation->OverrideScale(theScale, theScale);
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->OverrideScale(theScale);
			}
			break;
		}
		case EffectType::EFFECT_TRAIL:
		case EffectType::EFFECT_OTHER:
			break;
		}
	}
}

//0x404B20
void Attachment::CrossFade(const char* theCrossFadeName)
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		if (aAttachEffect->mEffectType == EffectType::EFFECT_PARTICLE)
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->CrossFade(theCrossFadeName);
			}
		}
	}
}

//0x404B80
void Attachment::SetMatrix(const SexyTransform2D& theMatrix)
{
	TOD_ASSERT(gEffectSystem);

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		SexyTransform2D aPosition = theMatrix * aAttachEffect->mOffset;

		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->SystemMove(aPosition.m02, aPosition.m12);
			}
			break;
		}

		case EffectType::EFFECT_TRAIL:
		{
			Trail* aTrail = gEffectSystem->mTrailHolder->mTrails.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aTrail)
			{
				aTrail->mTrailCenter = SexyVector2(aPosition.m02, aPosition.m12);
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				aReanimation->mOverlayMatrix = aPosition;
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->SetMatrix(aPosition);
			}
			break;
		}
		case EffectType::EFFECT_OTHER:
			break;
		}
	}
}

//0x404D10
void Attachment::Draw(Graphics* g, bool theParentHidden)
{
	TOD_ASSERT(gEffectSystem);

	DataArray<TodParticleSystem>& aParticleSystems = gEffectSystem->mParticleHolder->mParticleSystems;
	DataArray<Trail>& aTrails = gEffectSystem->mTrailHolder->mTrails;
	DataArray<Reanimation>& aReanimations = gEffectSystem->mReanimationHolder->mReanimations;
	DataArray<Attachment>& aAttachments = gEffectSystem->mAttachmentHolder->mAttachments;

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		if (theParentHidden && aAttachEffect->mDontDrawIfParentHidden)
		{
			continue;
		}

		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = aParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				aParticleSystem->Draw(g);
			}
			break;
		}

		case EffectType::EFFECT_TRAIL:
		{
			Trail* aTrail = aTrails.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aTrail)
			{
				aTrail->Draw(g);
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = aReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				aReanimation->Draw(g);
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = aAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->Draw(g, theParentHidden);
			}
			break;
		}

		case EffectType::EFFECT_OTHER:
			break;
		}
	}
}

//0x404E80
void Attachment::Detach()
{
	TOD_ASSERT(gEffectSystem);

	DataArray<TodParticleSystem>& aParticleSystems = gEffectSystem->mParticleHolder->mParticleSystems;
	DataArray<Trail>& aTrails = gEffectSystem->mTrailHolder->mTrails;
	DataArray<Reanimation>& aReanimations = gEffectSystem->mReanimationHolder->mReanimations;
	DataArray<Attachment>& aAttachments = gEffectSystem->mAttachmentHolder->mAttachments;

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = aParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem)
			{
				TOD_ASSERT(aParticleSystem->mIsAttachment);
				aParticleSystem->mIsAttachment = false;
			}
			break;
		}

		case EffectType::EFFECT_TRAIL:
		{
			Trail* aTrail = aTrails.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aTrail)
			{
				TOD_ASSERT(aTrail->mIsAttachment);
				aTrail->mIsAttachment = false;
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = aReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				TOD_ASSERT(aReanimation->mIsAttachment);
				aReanimation->mIsAttachment = false;
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = aAttachments.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aAttachment)
			{
				aAttachment->Detach();
			}
			break;
		}

		case EffectType::EFFECT_OTHER:
			break;
		}

		aAttachEffect->mEffectID = 0U;
	}

	mNumEffects = 0;
	mDead = true;
}

//0x404FC0
void Attachment::AttachmentDie()
{
	TOD_ASSERT(gEffectSystem);

	// @Minerscale Fix null pointer derefrence due to some sort of circular dependency...
	// The problems when freeing the data were probably actually bad so this fix is frankly irresponsible
	// TODO: put todo here so I know to come here when I break everything. !FIXME! !ACHTUNG!
	DataArray<TodParticleSystem> *aParticleSystems = nullptr;
	DataArray<Trail> *aTrails = nullptr;
	DataArray<Reanimation> *aReanimations = nullptr;
	DataArray<Attachment> *aAttachments = nullptr;
	if (gEffectSystem->mParticleHolder) aParticleSystems = &gEffectSystem->mParticleHolder->mParticleSystems;
	if (gEffectSystem->mTrailHolder) aTrails = &gEffectSystem->mTrailHolder->mTrails;
	if (gEffectSystem->mReanimationHolder) aReanimations = &gEffectSystem->mReanimationHolder->mReanimations;
	if (gEffectSystem->mAttachmentHolder) aAttachments = &gEffectSystem->mAttachmentHolder->mAttachments;

	for (int i = 0; i < mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &mEffectArray[i];
		switch (aAttachEffect->mEffectType)
		{
		case EffectType::EFFECT_PARTICLE:
		{
			TodParticleSystem* aParticleSystem = aParticleSystems?aParticleSystems->DataArrayTryToGet(aAttachEffect->mEffectID):nullptr;
			if (aParticleSystem)
			{
				aParticleSystem->ParticleSystemDie();
			}
			break;
		}

		case EffectType::EFFECT_TRAIL:
		{
			Trail* aTrail = aTrails?aTrails->DataArrayTryToGet(aAttachEffect->mEffectID):nullptr;
			if (aTrail)
			{
				aTrail->mDead = true;
			}
			break;
		}

		case EffectType::EFFECT_REANIM:
		{
			Reanimation* aReanimation = aReanimations?aReanimations->DataArrayTryToGet(aAttachEffect->mEffectID):nullptr;
			if (aReanimation)
			{
				aReanimation->ReanimationDie();
			}
			break;
		}

		case EffectType::EFFECT_ATTACHMENT:
		{
			Attachment* aAttachment = aAttachments?aAttachments->DataArrayTryToGet(aAttachEffect->mEffectID):nullptr;
			if (aAttachment)
			{
				aAttachment->AttachmentDie();
			}
			break;
		}

		case EffectType::EFFECT_OTHER:
			break;
		}

		aAttachEffect->mEffectID = 0U;
	}

	mNumEffects = 0;
	mDead = true;
}

AttachmentHolder::AttachmentHolder()
{
	//InitializeHolder();
}

//0x405120
AttachmentHolder::~AttachmentHolder()
{
	DisposeHolder();
}

void AttachmentHolder::InitializeHolder()
{
	mAttachments.DataArrayInitialize(4096U, "attachments");
}

void AttachmentHolder::DisposeHolder()
{
	mAttachments.DataArrayDispose();
}

Attachment* AttachmentHolder::AllocAttachment()
{
	return mAttachments.DataArrayTryAlloc();
}

//0x4051B0
void AttachmentUpdateAndSetMatrix(AttachmentID& theAttachmentID, SexyTransform2D& theMatrix)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->Update();
		aAttachment->SetMatrix(theMatrix);
	}
	else
	{
		theAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	}
}

//0x405200
void AttachmentUpdateAndMove(AttachmentID& theAttachmentID, float theX, float theY)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->Update();
		aAttachment->SetPosition(SexyVector2(theX, theY));
	}
	else
	{
		theAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	}
}

void AttachmentOverrideColor(AttachmentID& theAttachmentID, const Color& theColor)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->OverrideColor(theColor);
	}
}

void AttachmentOverrideScale(AttachmentID& theAttachmentID, float theScale)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->OverrideScale(theScale);
	}
}

//0x405270
void AttachmentReanimTypeDie(AttachmentID& theAttachmentID, ReanimationType theReanimType)
{
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment == nullptr)
	{
		return;
	}

	for (int i = 0; i < aAttachment->mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &aAttachment->mEffectArray[i];
		if (aAttachEffect->mEffectType == EffectType::EFFECT_REANIM)
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation && aReanimation->mReanimationType == theReanimType)
			{
				aReanimation->ReanimationDie();
			}
		}
	}
}

//0x405300
void AttachmentDetachCrossFadeParticleType(AttachmentID& theAttachmentID, ParticleEffect theParticleEffect, const char* theCrossFadeName)
{
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment == nullptr)
	{
		return;
	}

	TOD_ASSERT(theParticleEffect >= 0 && theParticleEffect < gParticleDefCount);
	TodParticleDefinition* aDefinition = &gParticleDefArray[(int)theParticleEffect];

	bool aDetached = false;
	for (int i = 0; i < aAttachment->mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &aAttachment->mEffectArray[i];
		if (aAttachEffect->mEffectType == EffectType::EFFECT_PARTICLE)
		{
			TodParticleSystem* aParticleSystem = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aParticleSystem && aParticleSystem->mParticleDef == aDefinition)
			{
				if (theCrossFadeName)
				{
					aParticleSystem->mIsAttachment = false;
					aParticleSystem->CrossFade(theCrossFadeName);
				}
				else
				{
					aParticleSystem->ParticleSystemDie();
				}
				// @pvz-online: 已处理的粒子移出效果表，下面收尾时不会再碰它（交叉淡出的那份要继续活着）。
				int aNumEffectsRemaining = aAttachment->mNumEffects - i - 1;
				if (aNumEffectsRemaining > 0)
					memmove(aAttachEffect, aAttachEffect + 1, aNumEffectsRemaining * sizeof(AttachEffect));
				aAttachment->mNumEffects--;
				i--;
				aDetached = true;
			}
		}
	}

	// @pvz-online: 原实现只把持有者的 ID 清零，attachment 本身既不标死也不回收——之后没人再调它的
	// Update，它永远发现不了粒子已死，成为孤儿常驻池中并随存档累积（实测地刨僵尸破土/丢镐时每只漏一个，
	// 无尽长局可把 4096 的池顶满，此后新僵尸挂不上任何特效）。ID 清零后表上剩下的效果本来也不会再被
	// 绘制或更新，这里让它们随 attachment 一起死亡回收，画面不变。与 AttachmentDetach 清零即 Detach 对齐。
	if (aDetached)
	{
		theAttachmentID = AttachmentID::ATTACHMENTID_NULL;
		aAttachment->AttachmentDie();
	}
}

void AttachmentPropogateColor(AttachmentID& theAttachmentID, const Color& theColor, bool theEnableAdditiveColor, const Color& theAdditiveColor, bool theEnableOverlayColor, const Color& theOverlayColor)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->PropogateColor(theColor, theEnableAdditiveColor, theAdditiveColor, theEnableOverlayColor, theOverlayColor);
	}
}

void AttachmentCrossFade(AttachmentID& theAttachmentID, const char* theCrossFadeName)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->CrossFade(theCrossFadeName);
	}
}

//0x4053A0
void AttachmentDraw(AttachmentID& theAttachmentID, Graphics* g, bool theParentHidden)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment)
	{
		aAttachment->Draw(g, theParentHidden);
	}
}

//0x4053E0
void AttachmentDie(AttachmentID& theAttachmentID)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	theAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	if (aAttachment)
	{
		aAttachment->AttachmentDie();
	}
}

//0x405430
void AttachmentDetach(AttachmentID& theAttachmentID)
{
	if (theAttachmentID == AttachmentID::ATTACHMENTID_NULL)
		return;

	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	theAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	if (aAttachment)
	{
		aAttachment->Detach();
	}
}

//0x405480
Reanimation* FindReanimAttachment(AttachmentID& theAttachmentID)
{
	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < aAttachment->mNumEffects; i++)
	{
		AttachEffect* aAttachEffect = &aAttachment->mEffectArray[i];
		if (aAttachEffect->mEffectType == EffectType::EFFECT_REANIM)
		{
			Reanimation* aReanimation = gEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet(aAttachEffect->mEffectID);
			if (aReanimation)
			{
				return aReanimation;
			}
		}
	}

	return nullptr;
}

//0x405500
AttachEffect* FindFirstAttachment(AttachmentID& theAttachmentID)
{
	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment == nullptr)
	{
		return nullptr;
	}

	return (aAttachment->mNumEffects == 0) ? nullptr : &aAttachment->mEffectArray[0];
}

//0x405540
AttachEffect* CreateEffectAttachment(AttachmentID& theAttachmentID, EffectType theEffectType, unsigned int theDataID, float theOffsetX, float theOffsetY)
{
	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	if (aAttachment == nullptr || aAttachment->mDead)
	{
		aAttachment = gEffectSystem->mAttachmentHolder->AllocAttachment();
		if (aAttachment == nullptr)
		{
			theAttachmentID = AttachmentID::ATTACHMENTID_NULL;
			return nullptr;
		}
		theAttachmentID = (AttachmentID)gEffectSystem->mAttachmentHolder->mAttachments.DataArrayGetID(aAttachment);
	}

	// Both limits matter: total attachment slots and effects in this parent.
	if (aAttachment->mNumEffects < 0 || aAttachment->mNumEffects >= MAX_EFFECTS_PER_ATTACHMENT) return nullptr;
	TOD_ASSERT(!aAttachment->mDead);

	AttachEffect* aAttachEffect = &aAttachment->mEffectArray[aAttachment->mNumEffects];
	aAttachEffect->mEffectType = theEffectType;
	aAttachEffect->mEffectID = theDataID;
	aAttachEffect->mDontDrawIfParentHidden = false;
	aAttachEffect->mOffset.LoadIdentity();
	aAttachEffect->mOffset.m02 = theOffsetX;
	aAttachEffect->mOffset.m12 = theOffsetY;
	aAttachment->mNumEffects++;
	return aAttachEffect;
}

//0x4055D0
AttachEffect* AttachReanim(AttachmentID& theAttachmentID, Reanimation* theReanimation, float theOffsetX, float theOffsetY)
{
	if (theReanimation == nullptr) return nullptr;
	unsigned int aReanimId = gEffectSystem->mReanimationHolder->mReanimations.DataArrayGetID(theReanimation);
	AttachEffect* aAttachEffect = CreateEffectAttachment(theAttachmentID, EffectType::EFFECT_REANIM, aReanimId, theOffsetX, theOffsetY);
	if (aAttachEffect == nullptr)
	{
		// Keep essential body/head animation alive for its existing owner.
		// @pvz-online: 但纯装饰类 reanim（金币/银币/阳光/溅射等 MayOmit 白名单）没有外部
		// ID 持有者——它们的唯一引用就是这次挂载。挂不上若不销毁，就会永久驻留池中、
		// 每帧被 EffectSystem::Update() 真的 Update（mIsAttachment 未置位），累积成数千个。
		// 与 AttachParticle / AttachTrail 的既有做法对齐：失败即销毁无主装饰。
		if (!theReanimation->mIsAttachment && ReanimationMayOmit(theReanimation->mReanimationType))
			theReanimation->ReanimationDie();
		return nullptr;
	}

	TOD_ASSERT(!theReanimation->mIsAttachment);
	theReanimation->mIsAttachment = true;

	return aAttachEffect;
}

//0x405600
AttachEffect* AttachParticle(AttachmentID& theAttachmentID, TodParticleSystem* theParticleSystem, float theOffsetX, float theOffsetY)
{
	if (theParticleSystem == nullptr)
		return nullptr;

	unsigned int aParticleId = gEffectSystem->mParticleHolder->mParticleSystems.DataArrayGetID(theParticleSystem);
	AttachEffect* aAttachEffect = CreateEffectAttachment(theAttachmentID, EffectType::EFFECT_PARTICLE, aParticleId, theOffsetX, theOffsetY);
	if (aAttachEffect == nullptr)
	{
		if (!theParticleSystem->mIsAttachment) theParticleSystem->ParticleSystemDie();
		return nullptr;
	}

	TOD_ASSERT(!theParticleSystem->mIsAttachment);
	theParticleSystem->mIsAttachment = true;

	return aAttachEffect;
}

AttachEffect* AttachTrail(AttachmentID& theAttachmentID, Trail* theTrail, float theOffsetX, float theOffsetY)
{
	if (theTrail == nullptr) return nullptr;
	unsigned int aTrailId = gEffectSystem->mTrailHolder->mTrails.DataArrayGetID(theTrail);
	AttachEffect* aAttachEffect = CreateEffectAttachment(theAttachmentID, EffectType::EFFECT_TRAIL, aTrailId, theOffsetX, theOffsetY);
	if (aAttachEffect == nullptr)
	{
		if (!theTrail->mIsAttachment) theTrail->mDead = true;
		return nullptr;
	}

	TOD_ASSERT(!theTrail->mIsAttachment);
	theTrail->mIsAttachment = true;

	return aAttachEffect;
}

bool IsFullOfAttachments(AttachmentID& theAttachmentID)
{
	TOD_ASSERT(gEffectSystem);
	Attachment* aAttachment = gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet((unsigned int)theAttachmentID);
	return aAttachment && aAttachment->mNumEffects >= MAX_EFFECTS_PER_ATTACHMENT;
}
