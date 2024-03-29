// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemManager.h"
#include "DGRG_GameInstance.h"
#include "Datas/DGRG_Struct.h"
#include "OptionManager.h"

UItemManager::UItemManager()
{
	
}

UItemManager::UItemManager(const FObjectInitializer& ObjectInitializer)
{
	m_DefaultRoll = FItemTierRollRatio();
	m_ItemSelect[(int)E_ITEMTYPE::NORMAL] = &UItemManager::CreateNormalItem;
	m_ItemSelect[(int)E_ITEMTYPE::EQUIP] = &UItemManager::CreateEquipItem;
	m_ItemSelect[(int)E_ITEMTYPE::MATERIAL] = &UItemManager::CreateMaterialItem;
	m_ItemSelect[(int)E_ITEMTYPE::ENCHANT] = &UItemManager::CreateNormalItem;
	m_ItemSelect[(int)E_ITEMTYPE::GEM] = &UItemManager::CreateNormalItem;
}

void UItemManager::Init(UDGRG_GameInstance * gameInstance)
{
	Super::Init(gameInstance);
	SetItemTierRollPtr(&m_DefaultRoll);
	SetCurrentItemTier(E_ITEMTIER::NORMAL);
	m_CurrentItemLevel = 1;
}

bool UItemManager::ItemDropRoll(const FDropItemData & itemData)
{
	auto fResult = FMath::RandRange(0.0f, 1.0f);

	if (itemData.m_DropPercentOne < fResult)
	{
		return false;
	}

	return true;
}

bool UItemManager::GetRandomNavPoint(const FVector& monsterLocation, float radius, FVector& result)
{
	
	UNavigationSystemBase* NavSystems = (m_GameInstance->GetWorld()->GetNavigationSystem());
	UNavigationSystemV1* NavV1 = Cast<UNavigationSystemV1>(NavSystems);
	FNavLocation NavDataArea;

	bool SuccessResult = NavV1->GetRandomPointInNavigableRadius(monsterLocation, radius, NavDataArea);

	result = NavDataArea.Location;

	return SuccessResult;
}

ADroppedItemActor * UItemManager::CreateDropItemActor( UStoredItem * itemInstance, FVector* point)
{
	ADroppedItemActor* DroppedActor = Cast<ADroppedItemActor>(m_GameInstance->GetWorld()->SpawnActor(m_ClassDropItem, point));

	DroppedActor->SetItemData(itemInstance);

	return DroppedActor;
}

ADroppedGoldActor * UItemManager::CreateDropGoldActor(int goldAmount, FVector * point)
{
	AActor* Created = m_GameInstance->GetWorld()->SpawnActor(m_ClassGoldItem, point);

	ADroppedGoldActor* DroppedActor = Cast<ADroppedGoldActor>(Created);

	DroppedActor->SetGoldAmount(goldAmount);

	return DroppedActor;
}

UStoredItem * UItemManager::CreateNormalItem(const FBaseItemData & itemData)
{
	auto* CreatedItem= NewObject<UStoredItem>();
	CreatedItem->SetItemData(&itemData);
	return CreatedItem;
}

UStoredItem * UItemManager::CreateEquipItem(const FBaseItemData & itemData)
{
	auto* CreatedItem = NewObject<UStoredEquipItem>();
	CreatedItem->SetItemData(&itemData);

	CreatedItem->SetItemTier(&RollItemTier(m_CurrentItemTierRoll));
	CreatedItem->SetRandomItemStat(GetCurrentItemLevel());

	m_GameInstance->GetOptionManager()->SetRandomOption(CreatedItem);

	return CreatedItem;
}

UStoredItem * UItemManager::CreateMaterialItem(const FBaseItemData & itemData)
{
	auto* CreatedItem = NewObject< UStoredMaterialItem>();
	CreatedItem->SetItemData(&itemData);
	return CreatedItem;
}

UStoredItem * UItemManager::PerformCreateItemFuncPtr(const FBaseItemData& itemData)
{
	return	(this->*(m_ItemSelect[(int)itemData.m_ItemType]))(itemData);
}

void UItemManager::SetCurrentItemLevel(int nlvl)
{
	m_CurrentItemLevel = nlvl;
}

void UItemManager::SetItemTierRollPtr(const FItemTierRollRatio* rollPtr)
{
	m_CurrentItemTierRoll = rollPtr;
}

int UItemManager::GetCurrentItemLevel()
{
	return m_CurrentItemLevel;
}

UStoredItem * UItemManager::CreateItemInstance(const FBaseItemData & itemData)
{
	return PerformCreateItemFuncPtr(itemData);
}

const FItemTier & UItemManager::RollItemTier(const FItemTierRollRatio * rollPtr)
{
	SetItemTierRollPtr(rollPtr);
	auto ResultEnum = m_CurrentItemTierRoll->SetRandomTierRoll();
	SetCurrentItemTier(ResultEnum);
	return GetCurrentItemTier();
}

void UItemManager::SetCurrentItemTier(E_ITEMTIER tier)
{
	m_CurrentItemTier=&m_GameInstance->GetItemTier(tier);
}

const FItemTier & UItemManager::GetCurrentItemTier() const
{
	return *m_CurrentItemTier;
}


void UItemManager::SetItemTierRollNull()
{
	SetItemTierRollPtr(&m_DefaultRoll);
}

void UItemManager::SpawnDropItemFromMonster(const AMonsterBase * monsterActor)
{
	FVector WantLoca;

	SetItemTierRollPtr(&monsterActor->GetMonsterData().m_ItemTierRoll);
	SetCurrentItemLevel(monsterActor->GetCurrentLevel());

	DropGoldFromMonster(monsterActor);
	DropItemFromMonster(monsterActor);

	SetCurrentItemLevel(1);
	SetItemTierRollNull();
}

void UItemManager::DropGoldFromMonster(const AMonsterBase * monsterActor)
{
	int MaxGoldRoll = (int)monsterActor->GetCurrentTier();

	int GoldRoll = FMath::RandRange(0,MaxGoldRoll);
	int ScaledGoldAmount = monsterActor->GetCurrentLevel()*monsterActor->GetMonsterData().m_DropGoldAmount;
	for (int i = 0; i < GoldRoll; i++)
	{
		CreateAndDropGoldInRandomGround(
			monsterActor->GetActorLocation(),
			monsterActor->GetCurrentDropRadius(),
			ScaledGoldAmount);
	}
}

void UItemManager::DropItemFromMonster(const AMonsterBase * monsterActor)
{
	auto& DropItemDatas = monsterActor->GetMonsterData().m_DropItemDatas;

	for (int i = 0; i < DropItemDatas.Num(); i++)
	{
		auto MonsterData = monsterActor->GetMonsterData().m_DropItemDatas[i];

		if (!ItemDropRoll(MonsterData))
		{
			return;
		}

		UStoredItem* Item = CreateItemInstance(*DropItemDatas[i].m_DropItemID.GetRow<FBaseItemData>(""));
		CreateAndDropItemInRandomGround(monsterActor->GetActorLocation(), monsterActor->GetCurrentDropRadius(), Item);
	}
}

bool UItemManager::CreateAndDropItemInRandomGround(FVector origin, float radius, UStoredItem * item)
{
	FVector WantLoca;
	if (GetRandomNavPoint(origin, radius, WantLoca))
	{
		FVector Temp = origin;
		ADroppedItemActor * DroppedActor = CreateDropItemActor(item, &Temp);

		DroppedActor->DropToGround(WantLoca);

		return true;
	}

	return false;
}

bool UItemManager::CreateAndDropGoldInRandomGround(FVector origin, float radius, int goldAmount)
{
	FVector WantLoca;
	if (GetRandomNavPoint(origin, radius, WantLoca))
	{
		FVector Temp = origin;
		ADroppedGoldActor * DroppedActor = CreateDropGoldActor(goldAmount, &Temp);

		DroppedActor->DropToGround(WantLoca);

		return true;
	}

	return false;
}
