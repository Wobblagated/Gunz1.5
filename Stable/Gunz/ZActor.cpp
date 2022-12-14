#include "stdafx.h"
#include "ZActor.h"
#include "ZApplication.h"
#include "ZGame.h"
#include "ZObjectManager.h"
#include "ZEnemy.h"
#include "ZEnvObject.h"
#include "ZGameClient.h"
#include "ZCharacter.h"
#include "ZModule_HPAP.h"
#include "ZModule_Resistance.h"
#include "ZModule_FireDamage.h"
#include "ZModule_ColdDamage.h"
#include "ZModule_LightningDamage.h"
#include "ZModule_PoisonDamage.h"
#include "ZModule_Skills.h"
#include "ZQuest.h"
#include "ZInput.h"
#include "ZBrain.h"

MImplementRTTI(ZActor, ZActorBase);

///////////////////////////////////////////////////////////////////////
ZActor::ZActor() 
: ZActorBase()
, m_nFlags(0)
, m_pNPCInfo(NULL)
, m_pModule_Skills(NULL), m_fSpeed(0.0f), m_pBrain(NULL)
{
	m_bIsNPC = true;		// ZObject?? false?? ?? ???? true?? ?ٲ???

	memset(m_nLastTime, 0, sizeof(m_nLastTime));

	SetPosition(rvector(0.0f, 0.0f, 0.0f));
	m_Direction = rvector(1.0f, 0.0f, 0.0f);
	m_TargetDir = rvector(1.0f, 0.0f, 0.0f);
	m_Accel = rvector(0.0f, 0.0f, 0.0f);

	m_Animation.Init(this);

	SetVisible(true);

	m_bInitialized = true;

	m_fDelayTime = 0.0f;
	m_fTC = 1.0f;
	m_nQL = 0;

	m_bReserveStandUp = false;


	// flag ????
	SetFlag(AF_LAND, true);

	m_bTestControl = false;

	m_timeDieAnimationDone = -1;

	m_vAddBlastVel = rvector(0.f,0.f,0.f);
	m_fAddBlastVelTime = 0.f;

//	RegisterModules();

	m_pModule_HPAP				= new ZModule_HPAP;
	m_pModule_Resistance		= new ZModule_Resistance;
	m_pModule_FireDamage		= new ZModule_FireDamage;
	m_pModule_ColdDamage		= new ZModule_ColdDamage;
	m_pModule_PoisonDamage		= new ZModule_PoisonDamage;
	m_pModule_LightningDamage	= new ZModule_LightningDamage;

	AddModule(m_pModule_HPAP);
	AddModule(m_pModule_Resistance);
	AddModule(m_pModule_FireDamage);
	AddModule(m_pModule_ColdDamage);
	AddModule(m_pModule_PoisonDamage);
	AddModule(m_pModule_LightningDamage);

	strcpy(m_szOwner,"unknown");

	m_TaskManager.SetOnFinishedCallback(OnTaskFinishedCallback);


	m_nDamageCount = 0;
}

//void ZActor::RegisterModules()
//{
//	ZObject::RegisterModules();
//	RegisterModule(&m_Module_Movable);
//}

ZActor::~ZActor()
{
	EmptyHistory();
	DestroyShadow();

	RemoveModule(m_pModule_HPAP);
	RemoveModule(m_pModule_Resistance);
	RemoveModule(m_pModule_FireDamage);
	RemoveModule(m_pModule_ColdDamage);
	RemoveModule(m_pModule_PoisonDamage);
	RemoveModule(m_pModule_LightningDamage);

	delete m_pModule_HPAP;
	delete m_pModule_Resistance;
	delete m_pModule_FireDamage;
	delete m_pModule_ColdDamage;
	delete m_pModule_PoisonDamage;
	delete m_pModule_LightningDamage;

	if(m_pModule_Skills) {
		RemoveModule(m_pModule_Skills);
		delete m_pModule_Skills;
	}

	if (m_pBrain)
	{
		delete m_pBrain; m_pBrain = NULL;
	}
}

void ZActor::InitProperty() 
{
}

void ZActor::InitStatus() 
{
	_ASSERT( NULL != m_pNPCInfo );

	const MTD_NPCINFO* pQuestNPCInfo = GetMyActorServerNPCInfo( m_pNPCInfo->nID );

	//?????? ?????? ?????ʹ? ?????? ?????? ?????? ?????? ?Ѵ?.
	SetClientNPCInfoFromServerNPCInfo( m_pNPCInfo, pQuestNPCInfo );

	// 2007-12-01 Ŭ???̾?Ʈ?? ?????ʹ? ???? ?? ???ٰ? ?Ǵ???.
	//int nMaxHP = m_pNPCInfo->nMaxHP;
	//int nMaxAP = m_pNPCInfo->nMaxAP;
	// ?????? ?????ͷ? ??????
	int nMaxHP = pQuestNPCInfo->m_nMaxHP;
	int nMaxAP = pQuestNPCInfo->m_nMaxAP;

//	nMaxHP = (int)((float)nMaxHP * m_fTC);
//	nMaxAP = (int)((float)nMaxAP * m_fTC);

	// ??ȹ?? ???濡 ???? ?????? - bird:20051508
	nMaxHP = ZActor::CalcMaxHP(m_nQL, nMaxHP);
	nMaxAP = ZActor::CalcMaxAP(m_nQL, nMaxAP);

	m_pModule_HPAP->SetMaxHP(nMaxHP);
	m_pModule_HPAP->SetMaxAP(nMaxAP);

	// ġƮ??
//#ifdef _DEBUG
//	if (GetQuestCheat(ZQUEST_CHEET_WEAKNPCS)) nMaxHP = 1;
//#endif
	m_pModule_HPAP->SetHP(nMaxHP);
	m_pModule_HPAP->SetAP(nMaxAP);
	m_pModule_HPAP->SetRealDamage(true);
	
	EmptyHistory();
}

void ZActor::SetMyControl(bool bMyControl)
{
	SetFlag(AF_MY_CONTROL, bMyControl);
	EmptyHistory();
}

#include "ZConfiguration.h"
extern sCharacterLight	g_CharLightList[NUM_LIGHT_TYPE];

bool ZActor::IsDieAnimationDone()
{
	if(m_Animation.GetCurrState() == ZA_ANIM_DIE) {
		return m_pVMesh->isOncePlayDone();
	}	
	return false;
}

void ZActor::OnDraw()
{
	if (m_pVMesh == NULL) return;

	Draw_SetLight(GetPosition() );

	if( IsDieAnimationDone() )
	{
#define TRAN_AFTER		0.5f	// ?? ?ð? ???ĺ???
#define VANISH_TIME		1.f		// ?? ?ð????? ??????????

		if(m_timeDieAnimationDone==-1) m_timeDieAnimationDone = ZGetGame()->GetTime();

//		float fOpacity = max(0.f,min(1.0f,(	VANISH_TIME-(m_pGame->GetTime()-GetDeadTime() - TRAN_AFTER))/VANISH_TIME));
		float fOpacity = max(0.f,min(1.0f,(	VANISH_TIME-(ZGetGame()->GetTime()-m_timeDieAnimationDone - TRAN_AFTER))/VANISH_TIME));

		m_pVMesh->SetVisibility(fOpacity);
	}
	else {
		if(!m_bHero) m_pVMesh->SetVisibility(1.f);
		m_timeDieAnimationDone = -1;
	}

	m_pVMesh->Render();
}

#include "ZActionDef.h"
#include "MEvent.h"

#define IsKeyDown(key) ((GetAsyncKeyState(key) & 0x8000)!=0)

void ZActor::TestControl(float fDelta)
{
	if (!IsKeyDown(VK_SHIFT)) return;

	rvector m_Accel=rvector(0,0,0);

	rvector right;
	rvector forward=rvector(1,0,0);
	forward.z=0;
	Normalize(forward);
	CrossProduct(&right,rvector(0,0,1),forward);

	if(ZIsActionKeyPressed(ZACTION_FORWARD)==true)	m_Accel+=forward;
	if(ZIsActionKeyPressed(ZACTION_BACK)==true)		m_Accel-=forward;
	if(ZIsActionKeyPressed(ZACTION_LEFT)==true)		m_Accel-=right;
	if(ZIsActionKeyPressed(ZACTION_RIGHT)==true)	m_Accel+=right;

	Normalize(m_Accel);

	m_Accel*=(ACCEL_SPEED*fDelta*5.0f);
	SetVelocity(m_Accel);


}

void ZActor::OnUpdate(float fDelta)
{
	if(m_pVMesh) {
		m_pVMesh->SetVisibility(1.f);
	}

	if (CheckFlag(AF_MY_CONTROL))
	{
		m_TaskManager.Run(fDelta);
		CheckDead(fDelta);

		ProcessNetwork(fDelta);

		// for test - bird
		if (m_bTestControl)
		{
			TestControl(fDelta);
		}
		else
		{
			__BP(60,"ZActor::OnUpdate::ProcessAI");
			if(isThinkAble())
				ProcessAI(fDelta);
			__EP(60);
		}

		ProcessMovement(fDelta);
	}
	
	ProcessMotion(fDelta);

	if (CheckFlag(AF_MY_CONTROL))
	{
		UpdateHeight(fDelta);
	}

	// ???? ?ֵ??? ???????? ???? ?ٽ? ???Ͼ?? ???°? ?߻??ϱ⿡ ???⼭ ???????ͽ??? ?????? ?Ǵ???.
	if ( (m_Animation.GetCurrState() == ZA_ANIM_BLAST_DROP) || (m_Animation.GetCurrState() == ZA_ANIM_BLAST_DAGGER_DROP))
	{
		if ( m_bReserveStandUp)
		{
			if ( timeGetTime() > m_dwStandUp)
			{
				m_bReserveStandUp = false;
				m_Animation.Input( ZA_EVENT_STANDUP);
			}
		}
		else
		{
			m_bReserveStandUp = true;
			m_dwStandUp = timeGetTime() + RandomNumber( 100, 2500);
		}
	}
	else
	{
		m_bReserveStandUp = false;
	}

	//kimyhwan : ?????? ???°? ?????? ?۵????? ?ʴ? ???? m_pModule_Movable->Update()?? ???????ӿ? ?ι? ?????Ǳ? ?????? ?? ????.
	// ???? ZActor?? m_pModule_Movable->Update()?? ???? ?????ϴµ?, ObjectManager?? ???? ĳ?????? ?????? ?? ??????Ʈ?ϱ? ??????
	// ?? ?????ӿ? ?ι? ??????Ʈ ?ϰ? ?ִ? ???̴?. ?׷??? ???? ???? npc?? ?ι??? ???? ?????̰? ?????? -_-
	// m_pModule_Movable->isLanding()?? ???? ???????? true?? ?Ǿ??ٰ? ???? ??????Ʈ???? ?ٽ? false?? ?Ǵ? ?÷????̹Ƿ?
	// ?????????? ???? isLanding()???? ?????ε? ???? ???? ???ϴ? ???찡 ?????? ???̴?.. ???ݿͼ? ??ġ?? ?ʹ? ?ʾ???. ???? ?????? ????.
}


//?????? ?????? ?????ʹ? ?????? ?????? ?????? ?????? ?Ѵ?.
void SetClientNPCInfoFromServerNPCInfo( MQuestNPCInfo* pClientNPCInfo, const MTD_NPCINFO* pServerNPCInfo )
{
	pClientNPCInfo->nMaxHP			= pServerNPCInfo->m_nMaxHP;
	pClientNPCInfo->nMaxAP			= pServerNPCInfo->m_nMaxAP;
	pClientNPCInfo->nIntelligence	= pServerNPCInfo->m_nInt;
	pClientNPCInfo->nAgility		= pServerNPCInfo->m_nAgility;
	pClientNPCInfo->fViewAngle		= pServerNPCInfo->m_fAngle;
	pClientNPCInfo->fDyingTime		= pServerNPCInfo->m_fDyingTime;

	pClientNPCInfo->fCollRadius		= pServerNPCInfo->m_fCollisonRadius;
	pClientNPCInfo->fCollHeight		= pServerNPCInfo->m_fCollisonHight;

	pClientNPCInfo->nNPCAttackTypes = pServerNPCInfo->m_nAttackType;
	pClientNPCInfo->fAttackRange	= pServerNPCInfo->m_fAttackRange;
	pClientNPCInfo->nWeaponItemID	= pServerNPCInfo->m_nWeaponItemID;
	pClientNPCInfo->fSpeed			= pServerNPCInfo->m_fDefaultSpeed;
}

void ZActor::InitFromNPCType(MQUEST_NPC nNPCType, float fTC, int nQL, ZBrain* pBrain)
{
	m_pNPCInfo = GetQuestNPCInfo(nNPCType);
	_ASSERT(m_pNPCInfo);

	const MTD_NPCINFO* pNPCInfo = GetMyActorServerNPCInfo( nNPCType );
	if( NULL == pNPCInfo )
	{
		_ASSERT( 0 );
		return;
	}

	SetClientNPCInfoFromServerNPCInfo( m_pNPCInfo, pNPCInfo );

	InitMesh(m_pNPCInfo->szMeshName, nNPCType);

	if(m_pNPCInfo->nNPCAttackTypes & NPC_ATTACK_MELEE ) {
		m_Items.EquipItem(MMCIP_MELEE, m_pNPCInfo->nWeaponItemID);
		m_Items.SelectWeapon(MMCIP_MELEE);
	}

	if(m_pNPCInfo->nNPCAttackTypes & NPC_ATTACK_RANGE ) {
		m_Items.EquipItem(MMCIP_PRIMARY, m_pNPCInfo->nWeaponItemID);
		m_Items.SelectWeapon(MMCIP_PRIMARY);
	}

	if(m_pNPCInfo->nSkills) {
		m_pModule_Skills = new ZModule_Skills;
		AddModule(m_pModule_Skills);
		m_pModule_Skills->Init(m_pNPCInfo->nSkills,m_pNPCInfo->nSkillIDs);
	}

	m_Collision.SetRadius(m_pNPCInfo->fCollRadius);
	m_Collision.SetHeight(m_pNPCInfo->fCollHeight);
	m_fTC = fTC;
	m_nQL = nQL;
	m_fSpeed = MakeSpeed(m_pNPCInfo->fSpeed);
	SetTremblePower(m_pNPCInfo->fTremble);
	

	// ?????? ????
	if (m_pVMesh && m_pNPCInfo)
	{
		rvector scale;
		scale.x = m_pNPCInfo->vScale.x;
		scale.y = m_pNPCInfo->vScale.y;
		scale.z = m_pNPCInfo->vScale.z;
		m_pVMesh->SetScale(scale);

		if (scale.z != 1.0f)
		{
			float fHeight = m_Collision.GetHeight();
			fHeight *= scale.z;
			m_Collision.SetHeight(fHeight);
		}
		if ((scale.x != 1.0f) || (scale.y != 1.0f))
		{
			float length = max(scale.x, scale.y);
			float fRadius = m_Collision.GetRadius();
			fRadius *= length;
			m_Collision.SetRadius(fRadius);
		}
	}


	
	// brain ???? - ?ܺο??? brain?? ?????ؼ? ????
	_ASSERT(pBrain);
	m_pBrain = pBrain;
	m_pBrain->Init(this);

	_ASSERT(m_pNPCInfo != NULL);
}

void ZActor::InitMesh(char* szMeshName, MQUEST_NPC nNPCType)
{
	_ASSERT(m_pVMesh == NULL);
	AllocObjectVMesh();
	m_pVMesh->InitNpcMesh(this, szMeshName);

	m_pVMesh->SetScale(rvector(15,15,15));

	m_Animation.Set(ZA_ANIM_RUN);

	LinkAniEventSet(nNPCType);
}

void ZActor::UpdateHeight(float fDelta)
{
	// ???????? ???߿? ???ְų? ???? ???ư??? ???̸? land ?÷??׸? ????
	if (GetDistToFloor() > 10.f || GetVelocity().z > 0.1f)
	{
        SetFlag(AF_LAND, false);
	}else {
		if(!CheckFlag(AF_LAND))
		{
			SetFlag(AF_LAND, true);
			m_Animation.Input(ZA_EVENT_REACH_GROUND);
		}
	}

	if(!CheckFlag(AF_LAND))
		m_pModule_Movable->UpdateGravity(fDelta);



	bool bJumpUp=(GetVelocity().z>0.0f);
	bool bJumpDown = false;


	if(m_pModule_Movable->isLanding())
	{
		SetFlag(AF_LAND, true);
		m_Animation.Input(ZA_EVENT_REACH_GROUND);
		SetVelocity(0,0,0);

		if(GetPosition().z + 100.f < m_pModule_Movable->GetFallHeight())
		{
//			fFallHeight = m_Position.z;
			float fSpeed=fabs(GetVelocity().z);


			// ???? ?????? ????..
			rvector vPos = GetPosition();
			rvector vDir = rvector(0.f,0.f,-1.f);
			vPos.z += 50.f;

			RBSPPICKINFO pInfo;
			if (ZGetGame()->PickWorld(vPos, vDir, &pInfo))
			{
				vPos = pInfo.PickPos;

				vDir.x = pInfo.pInfo->plane.a;
				vDir.y = pInfo.pInfo->plane.b;
				vDir.z = pInfo.pInfo->plane.c;

				AddLandingEffect(vPos, vDir);
			}
		}
	}

	// ???? ?????ų? ???????? ?ϴµ? ????????
	rvector diff=fDelta*GetVelocity();

	bool bUp = (diff.z>0.01f);
	bool bDownward= (diff.z<0.01f);

	if(GetDistToFloor()<0 || (bDownward && m_pModule_Movable->GetLastMove().z>=0))
	{
		if(GetVelocity().z<1.f && GetDistToFloor()<1.f)
		{
			if(GetVelocity().z<0)
				SetVelocity(GetVelocity().x,GetVelocity().y,0);
		}
	}

	// ???? ?????????? ?????? ?ö???????..
	if(GetDistToFloor()<0 && !IsDie())
	{
		float fAdjust=400.f*fDelta;	// ?ʴ? ?̸?ŭ
		rvector diff=rvector(0,0,min(-GetDistToFloor(),fAdjust));
		Move(diff);
	}
}

void ZActor::UpdatePosition(float fDelta)
{
	// ???? ?????̴? ?κ??? module_movable ?? ?Űܰ???

//	if (GetDistToFloor() > 0.1f || GetVelocity().z > 0.1f)
//	{
//		// ?߷??? ?????? ?޴´?.
//#define ENEMY_GRAVITY_CONSTANT		2500.f			// ?߷??? ????
//#define ENEMY_MAX_FALL_SPEED		3000.f			// ?ִ? ???ϼӵ?
//
//		float down=ENEMY_GRAVITY_CONSTANT*fDelta;
//
//		rvector newVelocity = GetVelocity();
//		newVelocity.z-=down;
//		newVelocity.z=max(GetVelocity().z,-ENEMY_MAX_FALL_SPEED);
//		SetVelocity(newVelocity);
//
////		m_bJumpUp=(GetVelocity().z>0);
//
//
//		// ?????????? ?????? ????
//		if ((CheckFlag(AF_BLAST) == true) && (GetCurrAni() == ZA_EVENT_BLAST) && (GetVelocity().z < 0.0f))
//		{
//			m_Animation.Input(ZA_EVENT_REACH_PEAK);
//		}
//		
//		SetFlag(AF_LAND, false);
//	}

	if( CheckFlag(AF_MY_CONTROL) ) 
	{
		if ((CheckFlag(AF_BLAST) == true) && (GetCurrAni() == ZA_ANIM_BLAST) && (GetVelocity().z < 0.0f))
		{
			m_Animation.Input(ZA_EVENT_REACH_PEAK);
		}

		if ((CheckFlag(AF_BLAST_DAGGER) == true) && (GetCurrAni() == ZA_ANIM_BLAST_DAGGER) )
		{
			if( Magnitude(GetVelocity()) < 20.f )
			{
				m_Animation.Input(ZA_EVENT_REACH_GROUND_DAGGER);
				SetFlag(AF_BLAST_DAGGER,false);
			}
		}
	}

	m_pModule_Movable->Update(fDelta);
	

}



#include "ZGame.h"


/*
void ZActor::Input(AI_INPUT_SET nInput)
{
//	AI_BEHAVIOR_STATE nNextState = AI_BEHAVIOR_STATE(m_Behavior.StateTransition(nInput));
//	SetBehaviorState(nNextState);
}
*/

/*
bool ZActor::SetBehaviorState(AI_BEHAVIOR_STATE nBehaviorState)
{
	if (m_Behavior.GetCurrState() == nBehaviorState) return false;

	OnBehaviorExit(AI_BEHAVIOR_STATE(m_Behavior.GetCurrState()));

	m_Behavior.SetState(nBehaviorState);

	OnBehaviorEnter(AI_BEHAVIOR_STATE(m_Behavior.GetCurrState()));
	
	return true;
}

void ZActor::OnBehaviorEnter(AI_BEHAVIOR_STATE nBehaviorState)
{

}

void ZActor::OnBehaviorExit(AI_BEHAVIOR_STATE nBehaviorState)
{

}
*/

void ZActor::OnBlast(rvector &dir)
{
	if (!CheckFlag(AF_MY_CONTROL)) return;

	if (m_pNPCInfo->bNeverBlasted) return;

	rvector act_dir = GetDirection();
	act_dir.x = -dir.x;
	act_dir.y = -dir.y;
	SetDirection(act_dir);


	SetVelocity(dir * 300.f + rvector(0,0, (float)RandomNumber( 1000, 2000)));
	m_fDelayTime =3.0f;

	SetFlag(AF_BLAST, true);
	SetFlag(AF_LAND, false);

	m_Animation.Input(ZA_EVENT_BLAST);
}

void ZActor::OnBlastDagger(rvector &dir,rvector& pos)
{
	if (!CheckFlag(AF_MY_CONTROL)) return;

	if (m_pNPCInfo->bNeverBlasted) return;

	rvector act_dir = GetDirection();
	act_dir.x = -dir.x;
	act_dir.y = -dir.y;
	SetDirection(act_dir);

	SetVelocity(dir * 300.f + rvector(0,0,100.f));

//	m_vAddBlastVel = dir;
	m_vAddBlastVel = GetPosition() - pos;
	m_vAddBlastVel.z = 0.f;

	Normalize(m_vAddBlastVel);

	m_fAddBlastVelTime = ZGetGame()->GetTime();

	m_fDelayTime = 3.0f;

	SetFlag(AF_BLAST_DAGGER, true);
	SetFlag(AF_LAND, false);

	m_Animation.Input(ZA_EVENT_BLAST_DAGGER);
}

void ZActor::ProcessAI(float fDelta)
{
	if (!CheckFlag(AF_DEAD)) 
	{
		if (m_pBrain) m_pBrain->Think(fDelta);
	}

}

ZActor* ZActor::CreateActor(MQUEST_NPC nNPCType, float fTC, int nQL, bool bForceCollRadius35)
{
	ZActor* pNewActor = new ZActor();

	if (pNewActor)
	{
		ZBrain* pBrain = new ZBrain();

		pNewActor->InitFromNPCType(nNPCType, fTC, nQL, pBrain);
		pNewActor->InitProperty();
		pNewActor->InitStatus();

		if( pNewActor->m_pNPCInfo && pNewActor->m_pNPCInfo->bShadow ) {
			pNewActor->CreateShadow();
		}

		if (bForceCollRadius35) {
			ZModule_Movable* pMovableModule = (ZModule_Movable*)pNewActor->GetModule(ZMID_MOVABLE);
			pMovableModule->ForceCollRadius35(true);
		}
	}


	return pNewActor;
}


void ZActor::PostBasicInfo()
{
	DWORD nNowTime = timeGetTime();
	if (GetInitialized() == false) return;

	// ?װ????? 5?ʰ? ?????? basicinfo?? ?????? ?ʴ´?.
	if(IsDie() && ZGetGame()->GetTime() - GetDeadTime()>5.f) return;
	int nMoveTick = ZGetGame()->GetCharacterBasicInfoTick();

	if ((int)(nNowTime - m_nLastTime[ACTOR_LASTTIME_BASICINFO]) >= nMoveTick)
	{
		m_nLastTime[ACTOR_LASTTIME_BASICINFO] = nNowTime;

		ZACTOR_BASICINFO pbi;
		pbi.fTime = ZGetGame()->GetTime();
		pbi.uidNPC = GetUID();

		pbi.posx = GetPosition().x;
		pbi.posy = GetPosition().y;
		pbi.posz = GetPosition().z;
		/*pbi.posx = (int(GetPosition().x * 0.1f)) * 10.f;
		pbi.posy = (int(GetPosition().y * 0.1f)) * 10.f;
		pbi.posz = (int(GetPosition().z * 0.1f)) * 10.f;*/	// ?????? ???? ?????????? ???? ?ϱ? ???? 1?? ?ڸ????? ?????? ????

		pbi.velx = GetVelocity().x;
		pbi.vely = GetVelocity().y;
		pbi.velz = GetVelocity().z;

		pbi.dirx = GetDirection().x*32000.0f;
		pbi.diry = GetDirection().y*32000.0f;
		pbi.dirz = GetDirection().z*32000.0f;

		pbi.anistate = GetCurrAni();


		ZPOSTCMD1(MC_QUEST_PEER_NPC_BASICINFO, MCommandParameterBlob(&pbi,sizeof(ZACTOR_BASICINFO)));


		// history ?? ????

#define ACTOR_HISTROY_COUNT 100

		ZBasicInfoItem *pitem=new ZBasicInfoItem;
		pitem->info.position = GetPosition();
		pitem->info.direction = GetDirection();
		pitem->info.velocity = GetVelocity();

		pitem->fSendTime = pitem->fReceivedTime = ZGetGame()->GetTime();
		m_BasicHistory.push_back(pitem);

		while(m_BasicHistory.size()>ACTOR_HISTROY_COUNT)
		{
			delete *m_BasicHistory.begin();
			m_BasicHistory.pop_front();
		}
	}
}
void ZActor::PostBossHpAp()
{
	// ???? ?????? ???? HP AP?? ???????ش?.
	if ((m_pNPCInfo->nGrade == NPC_GRADE_BOSS) || (m_pNPCInfo->nGrade == NPC_GRADE_LEGENDARY))
	{
		if (GetQuestBossUid() != GetUID()) return;
		
		DWORD nNowTime = timeGetTime();
		if (GetInitialized() == false) return;
		
		// ?װ????? 5?ʰ? ?????? basicinfo?? ?????? ?ʴ´?.
		if(IsDie() && ZGetGame()->GetTime() - GetDeadTime()>5.f) return;
		int nMoveTick = ZGetGame()->GetCharacterBasicInfoTick();

		if ((int)(nNowTime - m_nLastTime[ACTOR_LASTTIME_HPINFO]) >= nMoveTick)
		{
			m_nLastTime[ACTOR_LASTTIME_HPINFO] = nNowTime;
			
			ZPOSTCMD3(MC_QUEST_PEER_NPC_BOSS_HPAP, MCmdParamUID(GetUID()), 
				MCommandParameterFloat(m_pModule_HPAP->GetHP()), MCommandParameterFloat(m_pModule_HPAP->GetAP()));
		}
	}
}

void ZActor::InputBasicInfo(ZBasicInfo* pni, BYTE anistate)
{
	if (!CheckFlag(AF_MY_CONTROL))
	{
		SetPosition(pni->position);
		SetVelocity(pni->velocity);
		SetDirection(pni->direction);
		m_Animation.ForceAniState(anistate);
		m_fLastBasicInfo = ZGetGame()->GetTime();
	}
}

void ZActor::InputBossHpAp( float fHp, float fAp )
{
	if (!CheckFlag(AF_MY_CONTROL))
	{
		m_pModule_HPAP->SetHP(fHp);
		m_pModule_HPAP->SetAP(fAp);
	}
}

bool ZActor::ProcessMotion(float fDelta)
{
	if (!m_pVMesh) return false;

	m_pVMesh->Frame();

	rvector pos = GetPosition();
	rvector dir = m_Direction;
	dir.z = 0;

	D3DXMATRIX world;
	MakeWorldMatrix(&world,rvector(0,0,0), dir,rvector(0,0,1));

	rvector MeshPosition ;
	MeshPosition = pos;

	MakeWorldMatrix(&world,pos,dir,rvector(0,0,1));
	m_pVMesh->SetWorldMatrix(world);

	UpdatePosition(fDelta);

	if(IsActiveModule(ZMID_LIGHTNINGDAMAGE)==false) {// ????Ʈ?? ???ϸ??̼? ????..

		if (m_pVMesh->isOncePlayDone())
		{
			m_Animation.Input(ZA_ANIM_DONE);
		}
	}

	return true;
}


void ZActor::ProcessMovement(float fDelta)
{
	bool bMoving = CheckFlag(AF_MOVING);
	bool bLand = CheckFlag(AF_LAND);

	if ( CheckFlag(AF_MOVING) && CheckFlag(AF_LAND) &&
		((GetCurrAni() == ZA_ANIM_WALK) || (GetCurrAni() == ZA_ANIM_RUN)))
	{
		float fSpeed = m_fSpeed;
		if (GetCurrAni() == ZA_ANIM_RUN) fSpeed = m_fSpeed;

//		SetVelocity(m_Direction * fSpeed);

		const float fAccel = 10000.f;

		AddVelocity(m_Direction * fAccel * fDelta);

		rvector vel = GetVelocity();
		if (Magnitude(vel) > fSpeed) {
			Normalize(vel);
			vel *= fSpeed;
			SetVelocity(vel);
		}
		
		return;
	}

	if(CheckFlag(AF_BLAST_DAGGER)) {
		float fSpeed = m_fSpeed;
//		rvector vel = GetVelocity() * fDelta * 100;

#define BLAST_DAGGER_MAX_TIME 0.8f

		float fTime = max((1.f - (ZGetGame()->GetTime() - m_fAddBlastVelTime) / BLAST_DAGGER_MAX_TIME),0.0f);

		if( fTime < 0.4f )
			fTime = 0.f;

		float fPower = 400.f * fTime * fTime * fDelta * 80.f;

//		m_vAddBlastVel.z -= fDelta;

		if(fPower==0.f)
			SetFlag(AF_BLAST_DAGGER,false);//???? ?ٵȰ?

		rvector vel = m_vAddBlastVel * fPower;

		SetVelocity( vel );

		return;
	}

	if (ZActorAnimation::IsAttackAnimation(GetCurrAni()))
	{
		SetVelocity(rvector(0,0,0));
	}
	else
	{
		// ?극??ũ?? ????
		rvector dir=rvector(GetVelocity().x,GetVelocity().y,0);
		float fSpeed=Magnitude(dir);
		Normalize(dir);

		float fRatio = m_pModule_Movable->GetMoveSpeedRatio();

		float max_speed = 600.f	* fRatio;	// TODO : max?? module_movable ?? ?????ϵ??? ?Ѵ?

		// ?ִ??ӵ? ?̻??̸? ?ִ??ӵ??? ??????..
		if(fSpeed>max_speed)
			fSpeed=max_speed;

#define NPC_STOP_SPEED			2000.f			// ?ƹ?Ű?? ?ȴ??????? ???ӵ?..

		fSpeed = max(fSpeed-NPC_STOP_SPEED*fDelta,0);
		SetVelocity(dir.x*fSpeed, dir.y*fSpeed, GetVelocity().z);
	}
}

void ZActor::RunTo(rvector& dir)
{
	dir.z = 0.0f;
	Normalize(dir);

	SetDirection(dir);

	if (CheckFlag(AF_MOVING) == true) return;
	if (m_Animation.Input(ZA_INPUT_RUN) == false) return;



	SetFlag(AF_MOVING, true);
}

bool ZActor::IsDie() 
{ 
	if(CheckFlag(AF_MY_CONTROL))
		return CheckFlag(AF_DEAD); 

	// ?ڽ??? ?????ϴ? ?????Ͱ? ?ƴѰ??? ?׾????? ?˱??? ????..
	if(m_Animation.GetCurrState() == ZA_ANIM_DIE) {
		return true;
	}
	return false;
}

void ZActor::Stop(bool bWithAniStop)
{
	SetVelocity(0,0,0);
	SetFlag(AF_MOVING, false);

	if (bWithAniStop) m_Animation.Input(ZA_INPUT_WALK_DONE);
}

void ZActor::RotateTo(rvector& dir)
{
	SetDirection(dir);
	m_Animation.Input(ZA_INPUT_ROTATE);
}

void ZActor::Attack_Melee()
{
	m_Animation.Input(ZA_INPUT_ATTACK_MELEE);

	ZPOSTCMD1(MC_QUEST_PEER_NPC_ATTACK_MELEE, MCommandParameterUID(GetUID()));
}

void ZActor::Attack_Range(rvector& dir)
{
	m_Animation.Input(ZA_INPUT_ATTACK_RANGE);

	SetDirection(dir);
	// TODO ?ѱ???ġ?? Ȯ???Ǹ? ???ľ??Ѵ?
	// ' HACK Fix this function.
	rvector pos,to;
	pos = GetPosition()+rvector(0,0,100);
	ZPostNPCRangeShot(GetUID(),ZGetGame()->GetTime(),pos,pos+10000.f*dir,MMCIP_PRIMARY);
}

void ZActor::Skill(int nSkill)
{
	ZSkillDesc *pDesc = m_pModule_Skills->GetSkill(nSkill)->GetDesc();
	if(pDesc) {
		if( pDesc->nCastingAnimation == 1 )
			m_Animation.Input(ZA_EVENT_SPECIAL1);
		else if (pDesc->nCastingAnimation == 2 )
			m_Animation.Input(ZA_EVENT_SPECIAL2);
		else if (pDesc->nCastingAnimation == 3 )
			m_Animation.Input(ZA_EVENT_SPECIAL3);
		else if (pDesc->nCastingAnimation == 4 )
			m_Animation.Input(ZA_EVENT_SPECIAL4);
		else if (pDesc->nCastingAnimation == 0 )
			return;


		else { _ASSERT(FALSE); }
	}
}

void ZActor::DebugTest()
{
#ifndef _DEBUG
	return;
#endif

//	if (m_pBrain) m_pBrain->DebugTest();
}


ZOBJECTHITTEST ZActor::HitTest( const rvector& origin, const rvector& to, float fTime, rvector *pOutPos )
{
	return HitTest_Actor(m_pNPCInfo->bColPick, origin, to, fTime, pOutPos);
}


//void ZActor::OnDamage(int damage, float fRatio)
//{
//	// ???? ??Ʈ???ϴ? actor ?? damage?? ?ش?
//	if (CheckFlag(AF_MY_CONTROL))
//		ZObject::OnDamage(damage,fRatio);
//}

#include "ZScreenEffectManager.h"

void ZActor::OnDamaged(ZObject* pAttacker, rvector srcPos, ZDAMAGETYPE damageType, MMatchWeaponType weaponType, float fDamage, float fPiercingRatio, int nMeleeType)
{
	// ?´? ??????
	if (!CheckFlag(AF_SOUND_WOUNDED))
	{
		bool bMyKill = false;
		if (pAttacker)
		{
			bMyKill = (pAttacker == ZGetGame()->GetMyCharacter());
		}

		rvector pos_sound = GetPosition();
		pos_sound.z += m_Collision.GetHeight() - 10.0f;
		ZApplication::GetSoundEngine()->PlayNPCSound(m_pNPCInfo->nID, NPC_SOUND_WOUND, pos_sound, bMyKill);
		SetFlag(AF_SOUND_WOUNDED, true);		
	}

	// ???? ?????? ???? ???? ?????? ????
	if ((m_pNPCInfo->nGrade == NPC_GRADE_BOSS) || (m_pNPCInfo->nGrade == NPC_GRADE_LEGENDARY))
	{
		if (GetQuestBossUid() == GetUID())
		{
			ShockBossGauge(fDamage);
		}
	}

	if ((damageType == ZD_BULLET) || (damageType == ZD_BULLET_HEADSHOT))
	{
		m_nDamageCount++;
	}

	//m_nDamage += (int)fDamage;


	if(CheckFlag(AF_MY_CONTROL))
	{
		bool bSkipDamagedAnimation = false;

		if(m_pNPCInfo->bNeverAttackCancel ) { // ?????? ???ҵ??? ?ʴ? ?Ӽ??? ????????...

			bSkipDamagedAnimation = ZActorAnimation::IsSkippableDamagedAnimation(GetCurrAni());
		}

		if(bSkipDamagedAnimation==false) {
		
			if((damageType==ZD_MELEE) || (damageType==ZD_KATANA_SPLASH)) {

				ZCharacterObject* pCObj = MDynamicCast(ZCharacterObject, pAttacker);

				bool bLightningDamage = false;

				if(pCObj) {
					ZC_ENCHANT etype = pCObj->GetEnchantType();
					if( etype == ZC_ENCHANT_LIGHTNING )//????Ʈ???? ???? ?????? ???? ????..
						bLightningDamage = true;	
				}

				if(bLightningDamage && (damageType==ZD_KATANA_SPLASH)) {//????Ʈ?? ???????? ????
					m_Animation.Input(ZA_EVENT_LIGHTNING_DAMAGED);
				}
				else {
					if(nMeleeType%2)
						m_Animation.Input(ZA_EVENT_MELEE_DAMAGED1);
					else
						m_Animation.Input(ZA_EVENT_MELEE_DAMAGED2);

				}
				SetVelocity(0,0,0);
			}
			else {
				if( m_pNPCInfo->bNeverPushed == false)	// ???Ÿ? ???? Ÿ?ݾ???
				{
					if (m_nDamageCount >= 5)
					{
						m_Animation.Input(ZA_EVENT_RANGE_DAMAGED);
						m_nDamageCount = 0;
					}
				}
			}
		}
	}

	ZObject::OnDamaged(pAttacker,srcPos,damageType,weaponType,fDamage,fPiercingRatio,nMeleeType);

	GetBrain()->OnDamaged();
}

void ZActor::OnKnockback(rvector& dir, float fForce)
{
	// ???? ??Ʈ???ϴ? actor ?? knockback?? ?ش?
	if(!CheckFlag(AF_MY_CONTROL)) return;

	// npc ?? ?˹??? ?ణ ?????Ѵ?
#define NPC_KNOCKBACK_FACTOR	1.0f

	ZCharacterObject::OnKnockback(dir,NPC_KNOCKBACK_FACTOR*fForce);
}

void ZActor::CheckDead(float fDelta)
{
	if (!CheckFlag(AF_MY_CONTROL)) return;
		 
	if (!CheckFlag(AF_DEAD))
	{
		// ?????????? HP?? ?׾????? üũ
		if(m_pModule_HPAP->GetHP()<=0) {
			SetDeadTime(ZGetGame()->GetTime());
			m_Animation.Input(ZA_EVENT_DEATH);
			SetFlag(AF_DEAD, true);

			MUID uidKiller = this->GetLastAttacker();
			ZPostQuestPeerNPCDead(uidKiller, GetUID());

			m_TaskManager.Clear();
		}
	}
	else
	{
		if (!CheckFlag(AF_REQUESTED_DEAD))
		{
			if (ZGetGame()->GetTime()-GetDeadTime() > m_pNPCInfo->fDyingTime)
			{
				// ???߿? ???????? ?????ϰ? ?Ǹ? ZGame::CheckMyCharDead?? ???ļ? ?????? ?????Ѵ?.
				MUID uidAttacker = GetLastAttacker();
				if (uidAttacker == MUID(0, 0))
				{
					// ???? ?????? ?????? ?𸣸? ?׳? ?????? ?ڽ??? ???? ????
					//_ASSERT(0);
					uidAttacker = ZGetMyUID();
				}

				// ???⼭ ?׾??ٰ? ????????
				ZPostQuestRequestNPCDead(uidAttacker, GetUID(), GetPosition());
				SetFlag(AF_REQUESTED_DEAD, true);
			}
		}
	}
}



void ZActor::ProcessNetwork(float fDelta)
{
	PostBasicInfo();
	PostBossHpAp();
}


bool ZActor::IsAttackable()
{
	ZA_ANIM_STATE nAnimState = m_Animation.GetCurrState();
	if ( (nAnimState == ZA_ANIM_IDLE) || (nAnimState == ZA_ANIM_WALK) 
		|| (nAnimState == ZA_ANIM_RUN) 
//		|| (nAnimState == ZA_ANIM_ATTACK_MELEE)		// ?̹? ?????ϰ? ???????? ???????? ?ʴ´?
//		|| (nAnimState == ZA_ANIM_ATTACK_RANGE) 
		) return true;

	return false;
}

bool ZActor::IsCollideable()
{
	if (m_Collision.IsCollideable())
	{
		// ???? ?ִϸ??̼??̸? ?浹???? ???Ѵ?.
		ZA_ANIM_STATE nAnimState = m_Animation.GetCurrState();
		if (nAnimState == ZA_ANIM_DIE) return false;

		return (!IsDie());
	}

	return m_Collision.IsCollideable();
}

bool ZActor::isThinkAble()// ??ã???? ???º????? ?Ұ????? ????
{
	if (!CheckFlag(AF_MY_CONTROL))
		return false;

	if (CheckFlag(AF_BLAST_DAGGER))
		return false;

	ZA_ANIM_STATE nAnimState = m_Animation.GetCurrState();
	if ( (nAnimState == ZA_ANIM_IDLE) ||
		 (nAnimState == ZA_ANIM_WALK) ||
         (nAnimState == ZA_ANIM_RUN))
		return true;

	return false;
}


void ZActor::OnDie()
{
}

void ZActor::OnPeerDie(MUID& uidKiller)
{
	bool bMyKill = (ZGetMyUID() == uidKiller);

	rvector pos_sound = GetPosition();
	pos_sound.z += m_Collision.GetHeight() - 10.0f;
	ZApplication::GetSoundEngine()->PlayNPCSound(m_pNPCInfo->nID, NPC_SOUND_DEATH, pos_sound, bMyKill);

}

void ZActor::OnTaskFinishedCallback(ZActor* pActor, ZTASK_ID nLastID)
{
	if (pActor) pActor->OnTaskFinished(nLastID);
}

void ZActor::OnTaskFinished(ZTASK_ID nLastID)
{
	if (m_pBrain) 
	{
		m_pBrain->OnBody_OnTaskFinished(nLastID);
		m_pBrain->SetCurrentSkill(NULL);
	}
		
}


bool ZActor::CanSee(ZObject* pTarget)
{
	// ?þ߿? ???̴??? Ȯ??
	rvector vTargetDir = pTarget->GetPosition() - GetPosition();
	rvector vBodyDir = GetDirection();
	vBodyDir.z = vTargetDir.z = 0.0f;

	float angle = fabs(GetAngleOfVectors(vTargetDir, vBodyDir));
	if (angle <= m_pNPCInfo->fViewAngle) return true;

	return false;
}

bool ZActor::CanAttackRange(ZObject* pTarget)
{
	ZPICKINFO pickinfo;
	memset(&pickinfo,0,sizeof(ZPICKINFO));

	rvector pos,dir;
	pos = GetPosition() + rvector(0,0,50);
	dir = pTarget->GetPosition() - GetPosition();
	Normalize(dir);

	const DWORD dwPickPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_HIDE | RM_FLAG_PASSROCKET | RM_FLAG_PASSBULLET;

	if (ZGetGame()->Pick(this, pos, dir, &pickinfo, dwPickPassFlag))
	{
		if (pickinfo.pObject)
		{
			if (pickinfo.pObject == pTarget) return true;
		}
	}

	return false;
}

bool ZActor::CanAttackMelee(ZObject* pTarget, ZSkillDesc *pSkillDesc)
{
	if (pSkillDesc == NULL)
	{
		float dist = Magnitude(pTarget->GetPosition() - GetPosition());
		if (dist < m_pNPCInfo->fAttackRange) 
		{
			// ?þ߰??? ???̴??? Ȯ??
			if (CanSee(pTarget)) return true;
		}
	}
	else
	{
		rvector Pos = GetPosition();
		rvector Dir = GetDirection();
		Dir.z=0; 
		Normalize(Dir);

		float fDist = Magnitude(Pos - pTarget->GetPosition());
		float fColMinRange = pSkillDesc->fEffectAreaMin * 100.0f;
		float fColMaxRange = pSkillDesc->fEffectArea * 100.0f;
		if ((fDist < fColMaxRange + pTarget->GetCollRadius()) && (fDist >= fColMinRange))
		{
			// ???? ???? Ȯ??
			rvector vTargetDir = pTarget->GetPosition() - GetPosition();
			rvector vBodyDir = GetDirection();
			vBodyDir.z = vTargetDir.z = 0.0f;

			float angle = fabs(GetAngleOfVectors(vTargetDir, vBodyDir));
			if (angle <= pSkillDesc->fEffectAngle) return true;
		}
	}

	return false;
}

void ZActor::OnNeglect( int nNum)
{
	if ( nNum == 1)
		m_Animation.Input( ZA_EVENT_NEGLECT1);
	else if ( nNum == 2)
		m_Animation.Input( ZA_EVENT_NEGLECT2);
}

float ZActor::MakeSpeed( float fSpeed)
{
	MQuestNPCGlobalAIValue* pGlobalAIValue = ZGetQuest()->GetNPCCatalogue()->GetGlobalAIValue();
	float fShakingRatio = pGlobalAIValue->m_fSpeed_ShakingRatio;
	float fExtraValue = fSpeed * fShakingRatio;
	float fMinSpeed = max( (fSpeed - fExtraValue), 0.0f);
	float fMaxSpeed = fSpeed + fExtraValue;

	return RandomNumber( fMinSpeed, fMaxSpeed);
}


float ZActor::MakePathFindingUpdateTime( char nIntelligence)
{
	MQuestNPCGlobalAIValue* pGlobalAIValue = ZGetQuest()->GetNPCCatalogue()->GetGlobalAIValue();
	float fShakingRatio = pGlobalAIValue->m_fPathFinding_ShakingRatio;
	float fTime = pGlobalAIValue->m_fPathFindingUpdateTime[ nIntelligence - 1];
	float fExtraValue = fTime * fShakingRatio;
	float fMinTime = fTime - fExtraValue;
	if ( fMinTime < 0.0f)
		fMinTime = 0.0f;
	float fMaxTime = fTime + fExtraValue;

	return RandomNumber( fMinTime, fMaxTime);
}

float ZActor::MakeAttackUpdateTime( char nAgility)
{
	MQuestNPCGlobalAIValue* pGlobalAIValue = ZGetQuest()->GetNPCCatalogue()->GetGlobalAIValue();
	float fShakingRatio = pGlobalAIValue->m_fAttack_ShakingRatio;
	float fTime = pGlobalAIValue->m_fAttackUpdateTime[ nAgility - 1];
	float fExtraValue = fTime * fShakingRatio;
	float fMinTime = fTime - fExtraValue;
	if ( fMinTime < 0.0f)
		fMinTime = 0.0f;
	float fMaxTime = fTime + fExtraValue;

	return RandomNumber( fMinTime, fMaxTime);
}


float ZActor::MakeDefaultAttackCoolTime()
{
	if ( !m_pNPCInfo)
		return 0.0f;

	float fShakingRatio = 0.3f;
	float fCoolTime = m_pNPCInfo->fAttackCoolTime;
	float fExtraValue = fCoolTime * fShakingRatio;
	float fMinCoolTime = max( (fCoolTime - fExtraValue), 0.01f);
	float fMaxCoolTime = fCoolTime + fExtraValue;

	return RandomNumber( fMinCoolTime, fMaxCoolTime);
}


void ZActor::ShockBossGauge( float fDamage )
{
	ZGetScreenEffectManager()->ShockBossGauge(fDamage);
}

void ZActor::LinkAniEventSet( MQUEST_NPC nNPCType )
{
	//?ִϸ??̼? ?̺?Ʈ ?Ŵ??????? ?ش? npc?? ?´? ?̺?Ʈ ?ҷ?????
	RAniIDEventSet* pAniIdEventSet = ZGetAniEventMgr()->GetAniIDEventSet((int)nNPCType);
	//?????? ?޽??? ?ش? ?̺?Ʈ ?? ?????ϱ?
	m_pVMesh->GetFrameInfo(RAniMode(0))->SetAniIdEventSet(pAniIdEventSet);
}

void ZActor::AddLandingEffect( rvector vPos, rvector vDir )
{
	ZGetEffectManager()->AddLandingEffect(vPos,vDir);//???ο??? ?ɼǿ? ????~
}

const MTD_NPCINFO* ZActor::GetMyActorServerNPCInfo( const MQUEST_NPC nNPCID )
{
	if (!ZGetQuest()) return NULL;
	return ZGetQuest()->GetNPCInfoFromServerMgr().GetNPCInfo( nNPCID );
}

bool ZActor::GetQuestCheat( ZQuestCheetType cheat )
{
//#ifdef _DEBUG
//	return (ZGetQuest()->GetCheet(cheat) == true);
//#endif
	return false;
}

int ZActor::GetActualHP()
{
	return (int)((float)m_pModule_HPAP->GetHP() * m_fTC);
}

int ZActor::GetActualAP()
{
	return (int)((float)m_pModule_HPAP->GetAP() * m_fTC);
}

int ZActor::GetActualMaxHP()
{
	const MTD_NPCINFO* pSvrQuestNPCInfo = GetMyActorServerNPCInfo( m_pNPCInfo->nID );
	if( NULL == pSvrQuestNPCInfo )
	{
		return 0;
	}

	//return (int)((float)pSvrQuestNPCInfo->m_nMaxHP * m_fTC); // ???̵? ???? ????(m_fTC)?? ???? MaxHP?? ???? ?ȵ? ????
	// ???̵? ???? ????(m_fTC)?? ???? ?̹? MaxHP?? ?????? ???·? ó???ؾ? ?????? ?ٰ? ?????? ???µȴ?.
	return (int)((float)m_pModule_HPAP->GetMaxHP() * m_fTC);
}

int ZActor::GetActualMaxAP()
{
	const MTD_NPCINFO* pSvrQuestNPCInfo = GetMyActorServerNPCInfo( m_pNPCInfo->nID );
	if( NULL == pSvrQuestNPCInfo )
	{
		return 0;
	}

	//return (int)((float)pSvrQuestNPCInfo->m_nMaxAP * m_fTC); // ???̵? ???? ????(m_fTC)?? ???? MaxAP?? ???? ?ȵ? ????
	// ???̵? ???? ????(m_fTC)?? ???? ?̹? MaxAP?? ?????? ???·? ó???ؾ? ?????? ?ٰ? ?????? ???µȴ?.
	return (int)((float)m_pModule_HPAP->GetMaxAP() * m_fTC);
}

void ZActor::GetDebugInfoString(char* szBuf1, char* szBuf2, int index)
{
#ifndef _PUBLISH
	rvector pos = GetPosition();
	rvector dir = GetDirection();
	rvector vel = GetVelocity();

	MQuestNPCInfo* pNPCInfo = GetNPCInfo();
	int nTaskCount = GetTaskManager()->GetCount();
	ZTask* pCurrTask = GetTaskManager()->GetCurrTask();
	char szTaskName[256] = "None";
	if (pCurrTask)
	{
		strcpy(szTaskName, pCurrTask->GetTaskName());
	}

	sprintf(szBuf1, "[%02d], %s, HP(%02d)AP(%2d) pos(%.2f %.2f %.2f), dir(%.2f %.2f %.2f), vel(%.2f %.2f %.2f), Task: %d, CurrTask: %s",
		index, pNPCInfo->szName, 
		GetHP(), GetAP(),
		pos.x, pos.y, pos.z, dir.x, dir.y, dir.z, vel.x, vel.y, vel.z,
		nTaskCount, szTaskName);

	char szFlagLand[8] = "??";
	char szFlagBlast[8] = "??";
	char szFlagBlastDagger[8] = "??";
	char szFlagMoving[8] = "??";
	char szFlagDead[8] = "??";
	char szFlagRequestedDead[8] = "??";
	char szFlagMyControl[8] = "??";

	if (CheckFlag(AF_LAND)) strcpy(szFlagLand, "??");
	if (CheckFlag(AF_BLAST)) strcpy(szFlagBlast, "??");
	if (CheckFlag(AF_BLAST_DAGGER)) strcpy(szFlagBlastDagger, "??");
	if (CheckFlag(AF_MOVING)) strcpy(szFlagMoving, "??");
	if (CheckFlag(AF_DEAD)) strcpy(szFlagDead, "??");
	if (CheckFlag(AF_REQUESTED_DEAD)) strcpy(szFlagRequestedDead, "??");
	if (CheckFlag(AF_MY_CONTROL)) strcpy(szFlagMyControl, "??");

	sprintf(szBuf2, "     LAND(%s), BLAST(%s), BLASTDAGGER(%s), MOVING(%s), DEAD(%s), REQUESTED_DEAD(%s), MY_CONTROL(%s), DistFloor(%.2f)",
		szFlagLand, szFlagBlast, szFlagBlastDagger ,szFlagMoving, szFlagDead, szFlagRequestedDead, szFlagMyControl, GetDistToFloor());
#endif
}