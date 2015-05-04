#include "../pchdef.h"
#include "playerbot.h"
#include "PlayerbotFactory.h"
#include "../../server/game/Guilds/GuildMgr.h"
#include "../ItemPrototype.h"
#include "PlayerbotAIConfig.h"
#include "../../shared/DataStores/DBCStore.h"
#include "../Miscellaneous/SharedDefines.h"
#include "../ahbot/AhBot.h"
#include "../Entities/Pet/Pet.h"
#include "RandomPlayerbotFactory.h"

using namespace ai;
using namespace std;

uint32 PlayerbotFactory::tradeSkills[] =
{
    SKILL_ALCHEMY,
    SKILL_ENCHANTING,
    SKILL_SKINNING,
    SKILL_JEWELCRAFTING,
    SKILL_INSCRIPTION,
    SKILL_TAILORING,
    SKILL_LEATHERWORKING,
    SKILL_ENGINEERING,
    SKILL_HERBALISM,
    SKILL_MINING,
    SKILL_BLACKSMITHING,
    SKILL_COOKING,
    SKILL_FIRST_AID,
    SKILL_FISHING
};

void PlayerbotFactory::Randomize()
{
    Randomize(true);
}

void PlayerbotFactory::Refresh()
{
    Prepare();
    InitEquipment(true);
    InitAmmo();
    InitFood();
    InitPotions();

    uint32 money = urand(level * 1000, level * 5 * 1000);
    if (bot->GetMoney() < money)
        bot->SetMoney(money);
    bot->SaveToDB();
}

void PlayerbotFactory::CleanRandomize()
{
    Randomize(false);
}

// FEYZEE: new functions used by init=high80 command
void PlayerbotFactory::AddEquipment(uint8 Slot, uint32 ItemId)
{
    uint16 dest;
    if (!CanEquipUnseenItem(Slot, dest, ItemId))
        return;
    Item* newItem = bot->EquipNewItem(dest, ItemId, true);
    if (newItem)
    {
        newItem->AddToWorld();
        newItem->AddToUpdateQueueOf(bot);
        bot->AutoUnequipOffhandIfNeed();
    }
}

void PlayerbotFactory::InitHunterPet()
{
    if (bot->getClass() != CLASS_HUNTER)
        return;
    Pet* pet = bot->GetPet();

    if (!pet)
    {
        Map* map = bot->GetMap();
        if (!map)
            return;

        vector<uint32> ids;
        CreatureTemplateContainer const* creatureTemplateContainer = sObjectMgr->GetCreatureTemplates();
        for (CreatureTemplateContainer::const_iterator i = creatureTemplateContainer->begin(); i != creatureTemplateContainer->end(); ++i)
        {
            CreatureTemplate const& co = i->second;
            if (!co.IsTameable(false))
                continue;

            if (co.minlevel > bot->getLevel())
                continue;

            PetLevelInfo const* petInfo = sObjectMgr->GetPetLevelInfo(co.Entry, bot->getLevel());
            if (!petInfo)
                continue;

            ids.push_back(i->first);
        }

        if (ids.empty())
        {
            sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "No pets available for bot %s (%d level)", bot->GetName().c_str(), bot->getLevel());
            return;
        }

        for (int i = 0; i < 100; i++)
        {
            int index = urand(0, ids.size() - 1);
            CreatureTemplate const* co = sObjectMgr->GetCreatureTemplate(ids[index]);

            PetLevelInfo const* petInfo = sObjectMgr->GetPetLevelInfo(co->Entry, bot->getLevel());
            if (!petInfo)
                continue;

            uint32 guid = sObjectMgr->GenerateLowGuid(HIGHGUID_PET);
            pet = new Pet(bot, HUNTER_PET);
            if (!pet->Create(guid, map, 0, ids[index], 0))
            {
                delete pet;
                pet = NULL;
                continue;
            }

            pet->SetPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
            pet->setFaction(bot->getFaction());
            pet->SetLevel(bot->getLevel());
            bot->SetPetGUID(pet->GetGUID());
            bot->GetMap()->AddToMap(pet->ToCreature());
            bot->SetMinion(pet, true);
            pet->InitTalentForLevel();
            bot->PetSpellInitialize();
            bot->InitTamedPet(pet, bot->getLevel(), 0);

            sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,  "Bot %s: assign pet %d (%d level)", bot->GetName().c_str(), co->Entry, bot->getLevel());
            pet->SavePetToDB(PET_SAVE_AS_CURRENT);
            break;
        }
    }

    if (!pet)
    {
        sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "Cannot create pet for bot %s", bot->GetName().c_str());
        return;
    }

    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if(itr->second.state == PETSPELL_REMOVED)
            continue;

        uint32 spellId = itr->first;
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo->IsPassive())
            continue;

        pet->ToggleAutocast(spellInfo, true);
    }
}

void PlayerbotFactory::CleanBuild()
{
    uint8 BotClass;
    uint8 BotRace;
    BotClass = bot->getClass();
    BotRace = bot->getRace();
    if (!itemQuality)
        itemQuality = urand(ITEM_QUALITY_RARE, ITEM_QUALITY_EPIC);
    if (bot->isDead())
        bot->ResurrectPlayer(1.0f, false);
    bot->CombatStop(true);
    bot->SetLevel(80);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
    bot->ResetTalents(true);
    ClearSpells();
    ClearInventory();
    bot->SaveToDB();
    InitQuests();
    // quest rewards boost bot level, so reduce back
    bot->SetLevel(80);
    ClearInventory();
    bot->SetUInt32Value(PLAYER_XP, 0);
    CancelAuras();
    bot->SaveToDB();
    InitAvailableSpells();
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_PALADIN:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_MAGE:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            //bot->SetSkill(SKILL_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            //bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_PRIEST:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            //bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            //bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_WARLOCK:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            //bot->SetSkill(SKILL_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            //bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_HUNTER:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            //bot->SetSkill(SKILL_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_ROGUE:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            //bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_DEATH_KNIGHT:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_SHAMAN:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            //bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            //bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
        case CLASS_DRUID:
            bot->SetSkill(SKILL_DEFENSE, 0, 400, 400);
            //bot->SetSkill(SKILL_SWORDS, 0, 400, 400);
            //bot->SetSkill(SKILL_AXES, 0, 400, 400);
            //bot->SetSkill(SKILL_BOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_GUNS, 0, 400, 400);
            bot->SetSkill(SKILL_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_SWORDS, 0, 400, 400);
            bot->SetSkill(SKILL_STAVES, 0, 400, 400);
            bot->SetSkill(SKILL_2H_MACES, 0, 400, 400);
            //bot->SetSkill(SKILL_2H_AXES, 0, 400, 400);
            bot->SetSkill(SKILL_DAGGERS, 0, 400, 400);
            //bot->SetSkill(SKILL_THROWN, 0, 400, 400);
            //bot->SetSkill(SKILL_CROSSBOWS, 0, 400, 400);
            //bot->SetSkill(SKILL_WANDS, 0, 400, 400);
            bot->SetSkill(SKILL_POLEARMS, 0, 400, 400);
            //bot->SetSkill(SKILL_FIST_WEAPONS, 0, 400, 400);
            bot->SetSkill(SKILL_UNARMED, 0, 400, 400);
            break;
    }
    bot->SetSkill(SKILL_RIDING, 0, 300, 300);
    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            bot->SetSkill(SKILL_PLATE_MAIL, 0, 1, 1);
            break;
        case CLASS_SHAMAN:
        case CLASS_HUNTER:
            bot->SetSkill(SKILL_MAIL, 0, 1, 1);
            break;
    }
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        bot->SetSkill(tradeSkills[i], 0, 0, 0);
    }
    bot->SetSkill(SKILL_FIRST_AID, 0, 450, 450);
    bot->SetSkill(SKILL_FISHING, 0, 450, 450);
    bot->SetSkill(SKILL_COOKING, 0, 450, 450);
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(1); // Fury
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(0); // Arms
            }
            break;
        case CLASS_PALADIN:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(1); // Protection
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(2); // Retribution
            }
            break;
        case CLASS_MAGE:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(0); // Arcane
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(1); // Fire
            }
            break;
        case CLASS_PRIEST:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(1); // Holy
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(0); // Discipline
            }
            break;
        case CLASS_WARLOCK:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(0); // Affliction
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(2); // Destruction
            }
            break;
        case CLASS_HUNTER:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(1); // Marksmanship
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(0); // Beast Mastery
            }
            break;
        case CLASS_ROGUE:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(1); // Combat
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(0); // Assassination
            }
            break;
        case CLASS_DEATH_KNIGHT:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(1); // Frost
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(0); // Blood
            }
            break;
        case CLASS_SHAMAN:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(2); // Restoration
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(1); // Enhancement
            }
            break;
        case CLASS_DRUID:
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() <= 6)
                    break;
                InitTalents(2); // Restoration
            }
            for (uint8 i = 0; i < 15; i++)
            {
                if (bot->GetFreeTalentPoints() == 0)
                    break;
                InitTalents(0); // Balance
            }
            break;
    }
    InitAvailableSpells();
    // Cold Weather Flying
    bot->LearnSpell(54197, false);
    // Unlearn Fade
    if (bot->getClass() == CLASS_PRIEST)
    {
        bot->RemoveSpell(586, false, false);
    }
    // Learn Death Gate
    if (bot->getClass() == CLASS_DEATH_KNIGHT)
    {
        bot->LearnSpell(50977, false);
    }
    // Mounts
    switch (bot->getRace())
    {
        case RACE_HUMAN:
            bot->LearnSpell(23228, false);
            bot->LearnSpell(32242, false);
            break;
        case RACE_ORC:
            bot->LearnSpell(23251, false);
            bot->LearnSpell(32295, false);
            break;
        case RACE_DWARF:
            bot->LearnSpell(23240, false);
            bot->LearnSpell(32242, false);
            break;
        case RACE_NIGHTELF:
            bot->LearnSpell(23221, false);
            bot->LearnSpell(32242, false);
            break;
        case RACE_UNDEAD_PLAYER:
            bot->LearnSpell(17465, false);
            bot->LearnSpell(32295, false);
            break;
        case RACE_TAUREN:
            bot->LearnSpell(23247, false);
            bot->LearnSpell(32295, false);
            break;
        case RACE_GNOME:
            bot->LearnSpell(23223, false);
            bot->LearnSpell(32242, false);
            break;
        case RACE_TROLL:
            bot->LearnSpell(23243, false);
            bot->LearnSpell(32295, false);
            break;
        case RACE_BLOODELF:
            bot->LearnSpell(35025, false);
            bot->LearnSpell(32295, false);
            break;
        case RACE_DRAENEI:
            bot->LearnSpell(35713, false);
            bot->LearnSpell(32242, false);
            break;
    }
    UpdateTradeSkills();
    bot->SaveToDB();
    ClearAllInventory();
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51227); // Sanctified Ymirjar Lord's Helmet
            AddEquipment(EQUIPMENT_SLOT_NECK, 50647); // Ahn'kahar Onyx Neckguard
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51229); // Sanctified Ymirjar Lord's Shoulderplates
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51225); // Sanctified Ymirjar Lord's Battleplate
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50620); // Coldwraith Links
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51228); // Sanctified Ymirjar Lord's Legplates
            AddEquipment(EQUIPMENT_SLOT_FEET, 54578); // Apocalypse's Advance
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50659); // Polar Bear Claw Bracers
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51226); // Sanctified Ymirjar Lord's Gauntlets
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50657); // Skeleton Lord's Circle
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50693); // Might of Blight
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 40684); // Mirror of Truth
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 47214); // Banner of Victory
            AddEquipment(EQUIPMENT_SLOT_BACK, 50677); // Winding Sheet
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50730); // Glorenzelg, High-Blade of the Silver Hand
            //AddEquipment(EQUIPMENT_SLOT_OFFHAND, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_RANGED, 51535); // Wrathful Gladiator's War Edge
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_PALADIN:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51266); // Sanctified Lightsworn Faceguard
            AddEquipment(EQUIPMENT_SLOT_NECK, 50627); // Noose of Malachite
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51269); // Sanctified Lightsworn Shoulderguards
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51265); // Sanctified Lightsworn Chestguard
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50691); // Belt of Broken Bones
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51268); // Sanctified Lightsworn Legguards
            AddEquipment(EQUIPMENT_SLOT_FEET, 54579); // Treads of Impending Resurrection
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50611); // Bracers of Dark Reckoning
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51267); // Sanctified Lightsworn Handguards
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50622); // Devium's Eternally Cold Ring
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50642); // Juggernaut Band
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 50356); // Corroded Skeleton Key
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 47735); // Glyph of Indomitability
            AddEquipment(EQUIPMENT_SLOT_BACK, 50718); // Royal Crimson Cloak
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 51947); // Troggbane, Axe of the Frostborne King
            AddEquipment(EQUIPMENT_SLOT_OFFHAND, 50729); // Icecrown Glacial Wall
            AddEquipment(EQUIPMENT_SLOT_RANGED, 50461); // Libram of the Eternal Tower
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_MAGE:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51281); // Sanctified Bloodmage Hood
            AddEquipment(EQUIPMENT_SLOT_NECK, 50700); // Holiday's Grace
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51284); // Sanctified Bloodmage Shoulderpads
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51283); // Sanctified Bloodmage Robe
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50613); // Crushing Coldwraith Belt
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51282); // Sanctified Bloodmage Leggings
            AddEquipment(EQUIPMENT_SLOT_FEET, 51850); // Pale Corpse Boots
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50651); // The Lady's Brittle Bracers
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51280); // Sanctified Bloodmage Gloves
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50610); // Marrowgar's Frigid Eye
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50644); // Ring of Maddening Whispers
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 45490); // Pandora's Plea
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 45929); // Sif's Remembrance
            AddEquipment(EQUIPMENT_SLOT_BACK, 54583); // Cloak of Burning Dusk
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50704); // Rigormortis
            AddEquipment(EQUIPMENT_SLOT_OFFHAND, 50719); // Shadow Silk Spindle
            AddEquipment(EQUIPMENT_SLOT_RANGED, 50631); // Nightmare Ender
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_PRIEST:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51261); // Sanctified Crimson Acolyte Hood
            AddEquipment(EQUIPMENT_SLOT_NECK, 50700); // Holiday's Grace
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51264); // Sanctified Crimson Acolyte Shoulderpads
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51263); // Sanctified Crimson Acolyte Robe
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50702); // Lingering Illness
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51262); // Sanctified Crimson Acolyte Leggings
            AddEquipment(EQUIPMENT_SLOT_FEET, 51850); // Pale Corpse Boots
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50686); // Death Surgeon's Sleeves
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51260); // Sanctified Crimson Acolyte Gloves
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 54585); // Ring of Phased Regeneration
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50610); // Marrowgar's Frigid Eye
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 45490); // Pandora's Plea
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 45929); // Sif's Remembrance
            AddEquipment(EQUIPMENT_SLOT_BACK, 50668); // Greatcloak of the Turned Champion
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50685); // Trauma
            AddEquipment(EQUIPMENT_SLOT_OFFHAND, 50635); // Sundial of Eternal Dusk
            AddEquipment(EQUIPMENT_SLOT_RANGED, 50631); // Nightmare Ender
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_WARLOCK:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51231); // Sanctified Dark Coven Hood
            AddEquipment(EQUIPMENT_SLOT_NECK, 50700); // Holiday's Grace
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51234); // Sanctified Dark Coven Shoulderpads
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51233); // Sanctified Dark Coven Robe
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50613); // Crushing Coldwraith Belt
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51232); // Sanctified Dark Coven Leggings
            AddEquipment(EQUIPMENT_SLOT_FEET, 51850); // Pale Corpse Boots
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50651); // The Lady's Brittle Bracers
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51230); // Sanctified Dark Coven Gloves
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50610); // Marrowgar's Frigid Eye
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50644); // Ring of Maddening Whispers
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 45490); // Pandora's Plea
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 45929); // Sif's Remembrance
            AddEquipment(EQUIPMENT_SLOT_BACK, 54583); // Cloak of Burning Dusk
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50704); // Rigormortis
            AddEquipment(EQUIPMENT_SLOT_OFFHAND, 50719); // Shadow Silk Spindle
            AddEquipment(EQUIPMENT_SLOT_RANGED, 50631); // Nightmare Ender
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_HUNTER:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51286); // Sanctified Ahn'Kahar Blood Hunter's Headpiece
            AddEquipment(EQUIPMENT_SLOT_NECK, 50633); // Sindragosa's Cruel Claw
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51288); // Sanctified Ahn'Kahar Blood Hunter's Spaulders
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51289); // Sanctified Ahn'Kahar Blood Hunter's Tunic
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50688); // Nerub'ar Stalker's Cord
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51287); // Sanctified Ahn'Kahar Blood Hunter's Legguards
            AddEquipment(EQUIPMENT_SLOT_FEET, 54577); // Returning Footfalls
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50655); // Scourge Hunter's Vambraces
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51285); // Sanctified Ahn'Kahar Blood Hunter's Handguards
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50618); // Frostbrood Sapphire Ring
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50678); // Seal of Many Mouths
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 40684); // Mirror of Truth
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 50355); // Herkuml War Token
            AddEquipment(EQUIPMENT_SLOT_BACK, 50653); // Shadowvault Slayer's Cloak
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 47515); // Decimation
            //AddEquipment(EQUIPMENT_SLOT_OFFHAND, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_RANGED, 50733); // Fal'inrush, Defender of Quel'thalas
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_ROGUE:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51252); // Sanctified Shadowblade Helmet
            AddEquipment(EQUIPMENT_SLOT_NECK, 50633); // Sindragosa's Cruel Claw
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51254); // Sanctified Shadowblade Pauldrons
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51250); // Sanctified Shadowblade Breastplate
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50707); // Astrylian's Sutured Cinch
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51253); // Sanctified Shadowblade Legplates
            AddEquipment(EQUIPMENT_SLOT_FEET, 50607); // Frostbitten Fur Boots
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 54580); // Umbrage Armbands
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51251); // Sanctified Shadowblade Gauntlets
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50618); // Frostbrood Sapphire Ring
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50678); // Seal of Many Mouths
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 40684); // Mirror of Truth
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 50355); // Herkuml War Token
            AddEquipment(EQUIPMENT_SLOT_BACK, 50653); // Shadowvault Slayer's Cloak
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 51942); // Stormfury, Black Blade of the Betrayer
            AddEquipment(EQUIPMENT_SLOT_OFFHAND, 51942); // Stormfury, Black Blade of the Betrayer
            AddEquipment(EQUIPMENT_SLOT_RANGED, 51535); // Wrathful Gladiator's War Edge
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_DEATH_KNIGHT:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51312); // Sanctified Scourgelord Helmet
            AddEquipment(EQUIPMENT_SLOT_NECK, 50647); // Ahn'kahar Onyx Neckguard
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51314); // Sanctified Scourgelord Shoulderplates
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51310); // Sanctified Scourgelord Battleplate
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50620); // Coldwraith Links
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51313); // Sanctified Scourgelord Legplates
            AddEquipment(EQUIPMENT_SLOT_FEET, 54578); // Apocalypse's Advance
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50659); // Polar Bear Claw Bracers
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51311); // Sanctified Scourgelord Gauntlets
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 50657); // Skeleton Lord's Circle
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50693); // Might of Blight
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 40684); // Mirror of Truth
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 47214); // Banner of Victory
            AddEquipment(EQUIPMENT_SLOT_BACK, 50677); // Winding Sheet
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50730); // Glorenzelg, High-Blade of the Silver Hand
            //AddEquipment(EQUIPMENT_SLOT_OFFHAND, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_RANGED, 47673); // Sigil of Virulence
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_SHAMAN:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51247); // Sanctified Frost Witch's Headpiece
            AddEquipment(EQUIPMENT_SLOT_NECK, 50700); // Holiday's Grace
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51245); // Sanctified Frost Witch's Spaulders
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51249); // Sanctified Frost Witch's Tunic
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50671); // Belt of the Blood Nova
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51246); // Sanctified Frost Witch's Legguards
            AddEquipment(EQUIPMENT_SLOT_FEET, 50652); // Necrophotic Greaves
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 50655); // Scourge Hunter's Vambraces
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51248); // Sanctified Frost Witch's Handguards
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 54585); // Ring of Phased Regeneration
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50610); // Marrowgar's Frigid Eye
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 45490); // Pandora's Plea
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 45929); // Sif's Remembrance
            AddEquipment(EQUIPMENT_SLOT_BACK, 50668); // Greatcloak of the Turned Champion
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50685); // Trauma
            AddEquipment(EQUIPMENT_SLOT_OFFHAND, 50635); // Sundial of Eternal Dusk
            AddEquipment(EQUIPMENT_SLOT_RANGED, 47665); // Totem of Calming Tides
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
        case CLASS_DRUID:
            AddEquipment(EQUIPMENT_SLOT_HEAD, 51302); // Sanctified Lasherweave Helmet
            AddEquipment(EQUIPMENT_SLOT_NECK, 50700); // Holiday's Grace
            AddEquipment(EQUIPMENT_SLOT_SHOULDERS, 51304); // Sanctified Lasherweave Pauldrons
            //AddEquipment(EQUIPMENT_SLOT_BODY, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_CHEST, 51300); // Sanctified Lasherweave Robes
            AddEquipment(EQUIPMENT_SLOT_WAIST, 50705); // Professor's Bloodied Smock
            AddEquipment(EQUIPMENT_SLOT_LEGS, 51303); // Sanctified Lasherweave Legplates
            AddEquipment(EQUIPMENT_SLOT_FEET, 50665); // Boots of Unnatural Growth
            AddEquipment(EQUIPMENT_SLOT_WRISTS, 54584); // Phaseshifter's Bracers
            AddEquipment(EQUIPMENT_SLOT_HANDS, 51301); // Sanctified Lasherweave Gauntlets
            AddEquipment(EQUIPMENT_SLOT_FINGER1, 54585); // Ring of Phased Regeneration
            AddEquipment(EQUIPMENT_SLOT_FINGER2, 50610); // Marrowgar's Frigid Eye
            AddEquipment(EQUIPMENT_SLOT_TRINKET1, 45490); // Pandora's Plea
            AddEquipment(EQUIPMENT_SLOT_TRINKET2, 45929); // Sif's Remembrance
            AddEquipment(EQUIPMENT_SLOT_BACK, 50668); // Greatcloak of the Turned Champion
            AddEquipment(EQUIPMENT_SLOT_MAINHAND, 50731); // Archus, Greatstaff of Antonidas
            //AddEquipment(EQUIPMENT_SLOT_OFFHAND, XXXXX); // Empty
            AddEquipment(EQUIPMENT_SLOT_RANGED, 47670); // Idol of Lunar Fury
            //AddEquipment(EQUIPMENT_SLOT_TABARD, XXXXX); // Empty
            break;
    }
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (oldItem)
        {
            bot->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
            oldItem->DestroyForPlayer(bot, false);
        }
        uint32 newItemId = 38082; // Gigantique Bag
        uint16 dest;
        if (!CanEquipUnseenItem(slot, dest, newItemId))
            continue;
        Item* newItem = bot->EquipNewItem(dest, newItemId, true);
        if (newItem)
        {
            newItem->AddToWorld();
            newItem->AddToUpdateQueueOf(bot);
        }
    }
    uint32 itemId;
    ItemTemplate const* proto;
    itemId = 43000; // Drangonfin Filet
    proto = sObjectMgr->GetItemTemplate(itemId);
    bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
    bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
    bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
    itemId = 33447; // Runic Healing Potion
    proto = sObjectMgr->GetItemTemplate(itemId);
    bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
    itemId = 34722; // Heavy Frostweave Bandage
    proto = sObjectMgr->GetItemTemplate(itemId);
    bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            break;
        case CLASS_PALADIN:
            itemId = 21177; // Symbol of Kings
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17033; // Symbol of Divinity
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            break;
        case CLASS_MAGE:
            itemId = 17020; // Arcane Powder
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17032; // Rune of Portals
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17031; // Rune of Teleportation
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17056; // Light Feather
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            break;
        case CLASS_PRIEST:
            itemId = 17028; // Holy Candle
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17029; // Sacred Candle
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17056; // Light Feather
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 44615; // Devout Candle
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            break;
        case CLASS_WARLOCK:
            itemId = 6265; // Soul Shard
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, 20);
            break;
        case CLASS_HUNTER:
            itemId = 52021; // Iceblade Arrow
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, 1000);
            bot->StoreNewItemInBestSlots(itemId, 1000);
            bot->StoreNewItemInBestSlots(itemId, 1000);
            bot->StoreNewItemInBestSlots(itemId, 1000);
            bot->StoreNewItemInBestSlots(itemId, 1000);
            bot->SetAmmo(itemId);
            break;
        case CLASS_ROGUE:
            break;
        case CLASS_DEATH_KNIGHT:
            itemId = 6948; // Hearthstone
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, 1);
            break;
        case CLASS_SHAMAN:
            itemId = 17057; // Shiny Fish Scales
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17058; // Fish Oil
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17030; // Ankh
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            //itemId = 5175; // Earth Totem
            //proto = sObjectMgr->GetItemTemplate(itemId);
            //bot->StoreNewItemInBestSlots(itemId, 1);
            //itemId = 5176; // Fire Totem
            //proto = sObjectMgr->GetItemTemplate(itemId);
            //bot->StoreNewItemInBestSlots(itemId, 1);
            //itemId = 5177; // Water Totem
            //proto = sObjectMgr->GetItemTemplate(itemId);
            //bot->StoreNewItemInBestSlots(itemId, 1);
            //itemId = 5178; // Air Totem
            //proto = sObjectMgr->GetItemTemplate(itemId);
            //bot->StoreNewItemInBestSlots(itemId, 1);
            break;
        case CLASS_DRUID:
            itemId = 17034; // Maple Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17035; // Stranglethorn Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17036; // Ashwood Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17037; // Hornbeam Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17038; // Ironwood Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 22147; // Flintweed Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 44614; // Starleaf Seed
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17021; // Wild Berries
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 17026; // Wild Thornroot
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 22148; // Wild Quillvine
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            itemId = 44605; // Wild Spineleaf
            proto = sObjectMgr->GetItemTemplate(itemId);
            bot->StoreNewItemInBestSlots(itemId, proto->GetMaxStackSize());
            break;
    }
    for (uint32 slotIndex = 0; slotIndex < MAX_GLYPH_SLOT_INDEX; ++slotIndex)
    {
        bot->SetGlyph(slotIndex, 0);
    }
    InitHunterPet();
    bot->SetMoney(5000000);
    bot->DurabilityRepairAll(false, 1.0f, false);
    bot->SetFullHealth();
    bot->SetPvP(true);
    if (bot->GetMaxPower(POWER_MANA) > 0)
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
    if (bot->GetMaxPower(POWER_ENERGY) > 0)
        bot->SetPower(POWER_ENERGY, bot->GetMaxPower(POWER_ENERGY));
    bot->SaveToDB();
}

void PlayerbotFactory::Prepare()
{
    if (!itemQuality)
    {
        if (level <= 10)
            itemQuality = urand(ITEM_QUALITY_NORMAL, ITEM_QUALITY_UNCOMMON);
        else if (level <= 20)
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_RARE);
        else if (level <= 40)
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_EPIC);
        else if (level < 60)
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_EPIC);
        else
            itemQuality = urand(ITEM_QUALITY_RARE, ITEM_QUALITY_EPIC);
    }

    if (bot->isDead())
        bot->ResurrectPlayer(1.0f, false);

    bot->CombatStop(true);
    bot->SetLevel(level);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

void PlayerbotFactory::Randomize(bool incremental)
{
    Prepare();

    bot->ResetTalents(true);
    ClearSpells();
    ClearInventory();
    bot->SaveToDB();

    InitQuests();
    // quest rewards boost bot level, so reduce back
    bot->SetLevel(level);
    ClearInventory();
    bot->SetUInt32Value(PLAYER_XP, 0);
    CancelAuras();
    bot->SaveToDB();

    InitAvailableSpells();
    InitSkills();
    InitTradeSkills();
    InitTalents();
    InitAvailableSpells();
    InitSpecialSpells();
    InitMounts();
    UpdateTradeSkills();
    bot->SaveToDB();

    InitEquipment(incremental);
    InitBags();
    InitAmmo();
    InitFood();
    InitPotions();
    InitSecondEquipmentSet();
    InitInventory();
    InitGlyphs();
    InitGuild();
    bot->SetMoney(urand(level * 1000, level * 5 * 1000));
    bot->SaveToDB();

    InitPet();
    bot->SaveToDB();
}

void PlayerbotFactory::InitPet()
{
    Pet* pet = bot->GetPet();
    if (!pet)
    {
        if (bot->getClass() != CLASS_HUNTER)
            return;

        Map* map = bot->GetMap();
        if (!map)
            return;

		vector<uint32> ids;
	    CreatureTemplateContainer const* creatureTemplateContainer = sObjectMgr->GetCreatureTemplates();
	    for (CreatureTemplateContainer::const_iterator i = creatureTemplateContainer->begin(); i != creatureTemplateContainer->end(); ++i)
	    {
	        CreatureTemplate const& co = i->second;
            if (!co.IsTameable(false))
                continue;

            if (co.minlevel > bot->getLevel())
                continue;

			PetLevelInfo const* petInfo = sObjectMgr->GetPetLevelInfo(co.Entry, bot->getLevel());
            if (!petInfo)
                continue;

			ids.push_back(i->first);
		}

        if (ids.empty())
        {
            sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "No pets available for bot %s (%d level)", bot->GetName().c_str(), bot->getLevel());
            return;
        }

		for (int i = 0; i < 100; i++)
		{
			int index = urand(0, ids.size() - 1);
			CreatureTemplate const* co = sObjectMgr->GetCreatureTemplate(ids[index]);

            PetLevelInfo const* petInfo = sObjectMgr->GetPetLevelInfo(co->Entry, bot->getLevel());
            if (!petInfo)
                continue;

            uint32 guid = sObjectMgr->GenerateLowGuid(HIGHGUID_PET);
            pet = new Pet(bot, HUNTER_PET);
            if (!pet->Create(guid, map, 0, ids[index], 0))
            {
                delete pet;
                pet = NULL;
                continue;
            }

            pet->SetPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
            pet->setFaction(bot->getFaction());
            pet->SetLevel(bot->getLevel());
            bot->SetPetGUID(pet->GetGUID());
            bot->GetMap()->AddToMap(pet->ToCreature());
            bot->SetMinion(pet, true);
            pet->InitTalentForLevel();
            bot->PetSpellInitialize();
            bot->InitTamedPet(pet, bot->getLevel(), 0);

            sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,  "Bot %s: assign pet %d (%d level)", bot->GetName().c_str(), co->Entry, bot->getLevel());
            pet->SavePetToDB(PET_SAVE_AS_CURRENT);
            break;
        }
    }

    if (!pet)
    {
        sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "Cannot create pet for bot %s", bot->GetName().c_str());
        return;
    }

    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if(itr->second.state == PETSPELL_REMOVED)
            continue;

        uint32 spellId = itr->first;
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo->IsPassive())
            continue;

        pet->ToggleAutocast(spellInfo, true);
    }
}

void PlayerbotFactory::ClearSpells()
{
    list<uint32> spells;
    for(PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if(itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled || spellInfo->IsPassive())
            continue;

        spells.push_back(spellId);
    }

    for (list<uint32>::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        bot->RemoveSpell(*i, false, false);
    }
}

void PlayerbotFactory::InitSpells()
{
    for (int i = 0; i < 15; i++)
        InitAvailableSpells();
}

void PlayerbotFactory::InitTalents()
{
    uint32 point = urand(0, 100);
    uint8 cls = bot->getClass();
    uint32 p1 = sPlayerbotAIConfig.specProbability[cls][0];
    uint32 p2 = p1 + sPlayerbotAIConfig.specProbability[cls][1];

    uint32 specNo = (point < p1 ? 0 : (point < p2 ? 1 : 2));
    InitTalents(specNo);

    if (bot->GetFreeTalentPoints())
        InitTalents(2 - specNo);
}


class DestroyItemsVisitor : public IterateItemsVisitor
{
public:
    DestroyItemsVisitor(Player* bot) : IterateItemsVisitor(), bot(bot) {}

    virtual bool Visit(Item* item)
    {
        uint32 id = item->GetTemplate()->ItemId;
        if (CanKeep(id))
        {
            keep.insert(id);
            return true;
        }

        bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        return true;
    }

private:
    bool CanKeep(uint32 id)
    {
        if (keep.find(id) != keep.end())
            return false;

        if (sPlayerbotAIConfig.IsInRandomQuestItemList(id))
            return true;


        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(id);
        if (proto->Class == ITEM_CLASS_MISC && (proto->SubClass == ITEM_SUBCLASS_JUNK_REAGENT || proto->SubClass == ITEM_SUBCLASS_JUNK))
            return true;

        return false;
    }

private:
    Player* bot;
    set<uint32> keep;

};

bool PlayerbotFactory::CanEquipArmor(ItemTemplate const* proto)
{
    if (bot->HasSkill(SKILL_SHIELD) && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
        return true;

    if (bot->HasSkill(SKILL_PLATE_MAIL))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_PLATE)
            return false;
    }
    else if (bot->HasSkill(SKILL_MAIL))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_MAIL)
            return false;
    }
    else if (bot->HasSkill(SKILL_LEATHER))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_LEATHER)
            return false;
    }

    if (proto->Quality <= ITEM_QUALITY_NORMAL)
        return true;

    uint8 sp = 0, ap = 0, tank = 0;
    for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
    {
        // for ItemStatValue != 0
        if(!proto->ItemStat[j].ItemStatValue)
            continue;

        AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
    }

    return CheckItemStats(sp, ap, tank);
}

bool PlayerbotFactory::CheckItemStats(uint8 sp, uint8 ap, uint8 tank)
{
    switch (bot->getClass())
    {
    case CLASS_PRIEST:
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (!sp || ap > sp || tank > sp)
            return false;
        break;
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
        if ((!ap && !tank) || sp > ap || sp > tank)
            return false;
        break;
    case CLASS_HUNTER:
    case CLASS_ROGUE:
        if (!ap || sp > ap || sp > tank)
            return false;
        break;
    }

    return sp || ap || tank;
}

void PlayerbotFactory::AddItemStats(uint32 mod, uint8 &sp, uint8 &ap, uint8 &tank)
{
    switch (mod)
    {
    case ITEM_MOD_HIT_RATING:
    case ITEM_MOD_CRIT_RATING:
    case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_HEALTH_REGEN:
    case ITEM_MOD_MANA:
    case ITEM_MOD_INTELLECT:
    case ITEM_MOD_SPIRIT:
    case ITEM_MOD_MANA_REGENERATION:
    case ITEM_MOD_SPELL_POWER:
    case ITEM_MOD_SPELL_PENETRATION:
    case ITEM_MOD_HIT_SPELL_RATING:
    case ITEM_MOD_CRIT_SPELL_RATING:
    case ITEM_MOD_HASTE_SPELL_RATING:
        sp++;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_HIT_RATING:
    case ITEM_MOD_CRIT_RATING:
    case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_HEALTH_REGEN:
    case ITEM_MOD_DEFENSE_SKILL_RATING:
    case ITEM_MOD_DODGE_RATING:
    case ITEM_MOD_PARRY_RATING:
    case ITEM_MOD_BLOCK_RATING:
    case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
    case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
    case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
    case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
    case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
    case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
    case ITEM_MOD_HIT_TAKEN_RATING:
    case ITEM_MOD_CRIT_TAKEN_RATING:
    case ITEM_MOD_RESILIENCE_RATING:
    case ITEM_MOD_BLOCK_VALUE:
        tank++;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_HEALTH_REGEN:
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
    case ITEM_MOD_HIT_MELEE_RATING:
    case ITEM_MOD_HIT_RANGED_RATING:
    case ITEM_MOD_CRIT_MELEE_RATING:
    case ITEM_MOD_CRIT_RANGED_RATING:
    case ITEM_MOD_HASTE_MELEE_RATING:
    case ITEM_MOD_HASTE_RANGED_RATING:
    case ITEM_MOD_HIT_RATING:
    case ITEM_MOD_CRIT_RATING:
    case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_EXPERTISE_RATING:
    case ITEM_MOD_ATTACK_POWER:
    case ITEM_MOD_RANGED_ATTACK_POWER:
    case ITEM_MOD_ARMOR_PENETRATION_RATING:
        ap++;
        break;
    }
}

bool PlayerbotFactory::CanEquipWeapon(ItemTemplate const* proto)
{
    switch (bot->getClass())
    {
    case CLASS_PRIEST:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE)
            return false;
        break;
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_WARRIOR:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
            return false;
        break;
    case CLASS_PALADIN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_SHAMAN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_DRUID:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_HUNTER:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW)
            return false;
        break;
    case CLASS_ROGUE:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
            return false;
        break;
    }

    return true;
}

bool PlayerbotFactory::CanEquipItem(ItemTemplate const* proto, uint32 desiredQuality)
{
    if (proto->Duration & 0x80000000)
        return false;

    if (proto->Quality != desiredQuality)
        return false;

    if (proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_WHEN_USE)
        return false;

    if (proto->Class == ITEM_CLASS_CONTAINER)
        return true;

    uint32 requiredLevel = proto->RequiredLevel;
    if (!requiredLevel)
        return false;

    uint32 level = bot->getLevel();
    uint32 delta = 2;
    if (level < 15)
        delta = urand(7, 15);
    else if (proto->Class == ITEM_CLASS_WEAPON || proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
        delta = urand(2, 3);
    else if (!(level % 10) || (level % 10) == 9)
        delta = 2;
    else if (level < 40)
        delta = urand(5, 10);
    else if (level < 60)
        delta = urand(3, 7);
    else if (level < 70)
        delta = urand(2, 5);
    else if (level < 80)
        delta = urand(2, 4);

    if (desiredQuality > ITEM_QUALITY_NORMAL &&
            (requiredLevel > level || requiredLevel < level - delta))
        return false;

    for (uint32 gap = 60; gap <= 80; gap += 10)
    {
        if (level > gap && requiredLevel <= gap)
            return false;
    }

    return true;
}

void PlayerbotFactory::InitEquipment(bool incremental)
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor, ITERATE_ALL_ITEMS);

    map<uint8, vector<uint32> > items;
    for(uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
            continue;

        uint32 desiredQuality = itemQuality;
        if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
            desiredQuality--;
        }

        do
        {
            ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
            for (ItemTemplateContainer::const_iterator i = itemTemplates->begin(); i != itemTemplates->end(); ++i)
            {
                uint32 itemId = i->first;
                ItemTemplate const* proto = &i->second;
                if (!proto)
                    continue;

                if (proto->Class != ITEM_CLASS_WEAPON &&
                    proto->Class != ITEM_CLASS_ARMOR &&
                    proto->Class != ITEM_CLASS_CONTAINER &&
                    proto->Class != ITEM_CLASS_PROJECTILE)
                    continue;

                if (!CanEquipItem(proto, desiredQuality))
                    continue;

                if (proto->Class == ITEM_CLASS_ARMOR && (
                    slot == EQUIPMENT_SLOT_HEAD ||
                    slot == EQUIPMENT_SLOT_SHOULDERS ||
                    slot == EQUIPMENT_SLOT_CHEST ||
                    slot == EQUIPMENT_SLOT_WAIST ||
                    slot == EQUIPMENT_SLOT_LEGS ||
                    slot == EQUIPMENT_SLOT_FEET ||
                    slot == EQUIPMENT_SLOT_WRISTS ||
                    slot == EQUIPMENT_SLOT_HANDS) && !CanEquipArmor(proto))
                        continue;

                if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(proto))
                    continue;

                if (slot == EQUIPMENT_SLOT_OFFHAND && bot->getClass() == CLASS_ROGUE && proto->Class != ITEM_CLASS_WEAPON)
                    continue;

                uint16 dest = 0;
                if (CanEquipUnseenItem(slot, dest, itemId))
                    items[slot].push_back(itemId);
            }
        } while (items[slot].empty() && desiredQuality-- > ITEM_QUALITY_NORMAL);
    }

    for(uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
            continue;

        vector<uint32>& ids = items[slot];
        if (ids.empty())
        {
            sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,  "%s: no items to equip for slot %d", bot->GetName().c_str(), slot);
            continue;
        }

        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];
            Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

            if (incremental && !IsDesiredReplacement(oldItem)) {
                continue;
            }

            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
                continue;

            if (oldItem)
            {
                bot->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                oldItem->DestroyForPlayer(bot, false);
            }

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                bot->AutoUnequipOffhandIfNeed();
                EnchantItem(newItem);
                break;
            }
        }
    }
}

bool PlayerbotFactory::IsDesiredReplacement(Item* item)
{
    if (!item)
        return true;

    ItemTemplate const* proto = item->GetTemplate();
    int delta = 1 + (80 - bot->getLevel()) / 10;
    return (int)bot->getLevel() - (int)proto->RequiredLevel > delta;
}

void PlayerbotFactory::InitSecondEquipmentSet()
{
    if (bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_WARLOCK || bot->getClass() == CLASS_PRIEST)
        return;

    map<uint32, vector<uint32> > items;

    uint32 desiredQuality = itemQuality;
    while (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
        desiredQuality--;
    }

    do
    {
        ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
        for (ItemTemplateContainer::const_iterator i = itemTemplates->begin(); i != itemTemplates->end(); ++i)
        {
            uint32 itemId = i->first;
            ItemTemplate const* proto = &i->second;
            if (!proto)
                continue;

            if (!CanEquipItem(proto, desiredQuality))
                continue;

            if (proto->Class == ITEM_CLASS_WEAPON)
            {
                if (!CanEquipWeapon(proto))
                    continue;

                Item* existingItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (existingItem)
                {
                    switch (existingItem->GetTemplate()->SubClass)
                    {
                    case ITEM_SUBCLASS_WEAPON_AXE:
                    case ITEM_SUBCLASS_WEAPON_DAGGER:
                    case ITEM_SUBCLASS_WEAPON_FIST:
                    case ITEM_SUBCLASS_WEAPON_MACE:
                    case ITEM_SUBCLASS_WEAPON_SWORD:
                        if (proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE || proto->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER ||
                            proto->SubClass == ITEM_SUBCLASS_WEAPON_FIST || proto->SubClass == ITEM_SUBCLASS_WEAPON_MACE ||
                            proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD)
                            continue;
                        break;
                    default:
                        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE && proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                            continue;
                        break;
                    }
                }
            }
            else if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
            {
                if (!CanEquipArmor(proto))
                    continue;

                Item* existingItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (existingItem && existingItem->GetTemplate()->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
                    continue;
            }
            else
                continue;

            items[proto->Class].push_back(itemId);
        }
    } while (items[ITEM_CLASS_ARMOR].empty() && items[ITEM_CLASS_WEAPON].empty() && desiredQuality-- > ITEM_QUALITY_NORMAL);

    for (map<uint32, vector<uint32> >::iterator i = items.begin(); i != items.end(); ++i)
    {
        vector<uint32>& ids = i->second;
        if (ids.empty())
        {
            sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,  "%s: no items to make second equipment set for slot %d", bot->GetName().c_str(), i->first);
            continue;
        }

        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];

            ItemPosCountVec sDest;
            Item* newItem = StoreItem(newItemId, 1);
            if (newItem)
            {
                EnchantItem(newItem);
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                break;
            }
        }
    }
}

void PlayerbotFactory::InitBags()
{
    vector<uint32> ids;

    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    for (ItemTemplateContainer::const_iterator i = itemTemplates->begin(); i != itemTemplates->end(); ++i)
    {
        uint32 itemId = i->first;
        ItemTemplate const* proto = &i->second;
        if (!proto || proto->Class != ITEM_CLASS_CONTAINER)
            continue;

        if (!CanEquipItem(proto, ITEM_QUALITY_NORMAL))
            continue;

        ids.push_back(itemId);
    }

    if (ids.empty())
    {
        sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "%s: no bags found", bot->GetName().c_str());
        return;
    }

    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];

            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
                continue;

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                break;
            }
        }
    }
}

void PlayerbotFactory::EnchantItem(Item* item)
{
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance)
        return;

    if (bot->getLevel() < urand(40, 50))
        return;

    ItemTemplate const* proto = item->GetTemplate();
    int32 itemLevel = proto->ItemLevel;

    vector<uint32> ids;
    for (int id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellInfo const *entry = sSpellMgr->GetSpellInfo(id);
        if (!entry)
            continue;

        int32 requiredLevel = (int32)entry->BaseLevel;
        if (requiredLevel && (requiredLevel > itemLevel || requiredLevel < itemLevel - 35))
            continue;

        if (entry->MaxLevel && level > entry->MaxLevel)
            continue;

        uint32 spellLevel = entry->SpellLevel;
        if (spellLevel && (spellLevel > level || spellLevel < level - 10))
            continue;

        for (int j = 0; j < 3; ++j)
        {
            if (entry->Effects[j].Effect != SPELL_EFFECT_ENCHANT_ITEM)
                continue;

            uint32 enchant_id = entry->Effects[j].MiscValue;
            if (!enchant_id)
                continue;

            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
            if (!enchant || enchant->slot != PERM_ENCHANTMENT_SLOT)
                continue;

            if (enchant->requiredLevel && enchant->requiredLevel > level)
                continue;

            uint8 sp = 0, ap = 0, tank = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (enchant->type[i] != ITEM_ENCHANTMENT_TYPE_STAT)
                    continue;

                AddItemStats(enchant->spellid[i], sp, ap, tank);
            }

            if (!CheckItemStats(sp, ap, tank))
                continue;

            if (enchant->EnchantmentCondition && !bot->EnchantmentFitsRequirements(enchant->EnchantmentCondition, -1))
                continue;

            if (!item->IsFitToSpellRequirements(entry))
                continue;

            ids.push_back(enchant_id);
        }
    }

    if (ids.empty())
    {
        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG,  "%s: no enchantments found for item %d", bot->GetName().c_str(), item->GetTemplate()->ItemId);
        return;
    }

    int index = urand(0, ids.size() - 1);
    uint32 id = ids[index];

    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(id);
    if (!enchant)
        return;

    bot->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
    item->SetEnchantment(PERM_ENCHANTMENT_SLOT, id, 0, 0);
    bot->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);
}

bool PlayerbotFactory::CanEquipUnseenItem(uint8 slot, uint16 &dest, uint32 item)
{
    dest = 0;
    Item *pItem = Item::CreateItem(item, 1, bot);
    if (pItem)
    {
        InventoryResult result = bot->CanEquipItem(slot, dest, pItem, true, false);
        pItem->RemoveFromUpdateQueueOf(bot);
        delete pItem;
        return result == EQUIP_ERR_OK;
    }

    return false;
}

void PlayerbotFactory::InitTradeSkills()
{
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        bot->SetSkill(tradeSkills[i], 0, 0, 0);
    }

    vector<uint32> firstSkills;
    vector<uint32> secondSkills;
    switch (bot->getClass())
    {
    case CLASS_WARRIOR:
    case CLASS_PALADIN:
        firstSkills.push_back(SKILL_MINING);
        secondSkills.push_back(SKILL_BLACKSMITHING);
        secondSkills.push_back(SKILL_ENGINEERING);
        break;
    case CLASS_SHAMAN:
    case CLASS_DRUID:
    case CLASS_HUNTER:
    case CLASS_ROGUE:
        firstSkills.push_back(SKILL_SKINNING);
        secondSkills.push_back(SKILL_LEATHERWORKING);
        break;
    default:
        firstSkills.push_back(SKILL_TAILORING);
        secondSkills.push_back(SKILL_ENCHANTING);
    }

    SetRandomSkill(SKILL_FIRST_AID);
    SetRandomSkill(SKILL_FISHING);
    SetRandomSkill(SKILL_COOKING);

    switch (urand(0, 3))
    {
    case 0:
        SetRandomSkill(SKILL_HERBALISM);
        SetRandomSkill(SKILL_ALCHEMY);
        break;
    case 1:
        SetRandomSkill(SKILL_HERBALISM);
        SetRandomSkill(SKILL_INSCRIPTION);
        break;
    case 2:
        SetRandomSkill(SKILL_MINING);
        SetRandomSkill(SKILL_JEWELCRAFTING);
        break;
    case 3:
        SetRandomSkill(firstSkills[urand(0, firstSkills.size() - 1)]);
        SetRandomSkill(secondSkills[urand(0, secondSkills.size() - 1)]);
        break;
    }
}

void PlayerbotFactory::UpdateTradeSkills()
{
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        if (bot->GetSkillValue(tradeSkills[i]) == 1)
            bot->SetSkill(tradeSkills[i], 0, 0, 0);
    }
}

void PlayerbotFactory::InitSkills()
{
    uint32 maxValue = level * 5;
    // FEYZEE: add skills based on class
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            SetRandomSkill(SKILL_BOWS);
            SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_THROWN);
            SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_PALADIN:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_2H_SWORDS);
            //SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            //SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_MAGE:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            //SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            //SetRandomSkill(SKILL_MACES);
            //SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            //SetRandomSkill(SKILL_2H_MACES);
            //SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            SetRandomSkill(SKILL_WANDS);
            //SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_PRIEST:
            SetRandomSkill(SKILL_DEFENSE);
            //SetRandomSkill(SKILL_SWORDS);
            //SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            //SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            //SetRandomSkill(SKILL_2H_MACES);
            //SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            SetRandomSkill(SKILL_WANDS);
            //SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_WARLOCK:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            //SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            //SetRandomSkill(SKILL_MACES);
            //SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            //SetRandomSkill(SKILL_2H_MACES);
            //SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            SetRandomSkill(SKILL_WANDS);
            //SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_HUNTER:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            SetRandomSkill(SKILL_BOWS);
            SetRandomSkill(SKILL_GUNS);
            //SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            //SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_THROWN);
            SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_ROGUE:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            SetRandomSkill(SKILL_BOWS);
            SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            //SetRandomSkill(SKILL_2H_SWORDS);
            //SetRandomSkill(SKILL_STAVES);
            //SetRandomSkill(SKILL_2H_MACES);
            //SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_THROWN);
            SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            //SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_DEATH_KNIGHT:
            SetRandomSkill(SKILL_DEFENSE);
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_2H_SWORDS);
            //SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            //SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_SHAMAN:
            SetRandomSkill(SKILL_DEFENSE);
            //SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            //SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            //SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
        case CLASS_DRUID:
            SetRandomSkill(SKILL_DEFENSE);
            //SetRandomSkill(SKILL_SWORDS);
            //SetRandomSkill(SKILL_AXES);
            //SetRandomSkill(SKILL_BOWS);
            //SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            //SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            //SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            //SetRandomSkill(SKILL_THROWN);
            //SetRandomSkill(SKILL_CROSSBOWS);
            //SetRandomSkill(SKILL_WANDS);
            SetRandomSkill(SKILL_POLEARMS);
            //SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_UNARMED);
            break;
    }

    if (bot->getLevel() >= 70)
        bot->SetSkill(SKILL_RIDING, 0, 300, 300);
    else if (bot->getLevel() >= 60)
        bot->SetSkill(SKILL_RIDING, 0, 225, 225);
    else if (bot->getLevel() >= 40)
        bot->SetSkill(SKILL_RIDING, 0, 150, 150);
    else if (bot->getLevel() >= 20)
        bot->SetSkill(SKILL_RIDING, 0, 75, 75);
    else
        bot->SetSkill(SKILL_RIDING, 0, 0, 0);

    uint32 skillLevel = bot->getLevel() < 40 ? 0 : 1;
    switch (bot->getClass())
    {
    case CLASS_DEATH_KNIGHT:
    case CLASS_WARRIOR:
    case CLASS_PALADIN:
        bot->SetSkill(SKILL_PLATE_MAIL, 0, skillLevel, skillLevel);
        break;
    case CLASS_SHAMAN:
    case CLASS_HUNTER:
        bot->SetSkill(SKILL_MAIL, 0, skillLevel, skillLevel);
    }
}

void PlayerbotFactory::SetRandomSkill(uint16 id)
{
    uint32 maxValue = level * 5;
    uint32 curValue = urand(maxValue - level, maxValue);
    bot->SetSkill(id, 0, curValue, maxValue);

}

void PlayerbotFactory::InitAvailableSpells()
{
    bot->LearnDefaultSkills();

    CreatureTemplateContainer const* creatureTemplateContainer = sObjectMgr->GetCreatureTemplates();
    for (CreatureTemplateContainer::const_iterator i = creatureTemplateContainer->begin(); i != creatureTemplateContainer->end(); ++i)
    {
        CreatureTemplate const& co = i->second;
        if (co.trainer_type != TRAINER_TYPE_TRADESKILLS && co.trainer_type != TRAINER_TYPE_CLASS)
            continue;

        if (co.trainer_type == TRAINER_TYPE_CLASS && co.trainer_class != bot->getClass())
            continue;

		uint32 trainerId = co.Entry;

		TrainerSpellData const* trainer_spells = sObjectMgr->GetNpcTrainerSpells(trainerId);
        if (!trainer_spells)
            trainer_spells = sObjectMgr->GetNpcTrainerSpells(trainerId);

        if (!trainer_spells)
            continue;

        for (TrainerSpellMap::const_iterator itr =  trainer_spells->spellList.begin(); itr !=  trainer_spells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            if (!tSpell)
                continue;

            if (!tSpell->learnedSpell[0] && !bot->IsSpellFitByClassAndRace(tSpell->learnedSpell[0]))
                continue;

            TrainerSpellState state = bot->GetTrainerSpellState(tSpell);
            if (state != TRAINER_SPELL_GREEN)
                continue;

            if (tSpell->learnedSpell)
                bot->LearnSpell(tSpell->learnedSpell[0], false);
            else
                ai->CastSpell(tSpell->spell, bot);
        }
    }
}

void PlayerbotFactory::InitSpecialSpells()
{
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotSpellIds.begin(); i != sPlayerbotAIConfig.randomBotSpellIds.end(); ++i)
    {
        uint32 spellId = *i;
        bot->LearnSpell(spellId, false);
    }
}

void PlayerbotFactory::InitTalents(uint32 specNo)
{
    uint32 classMask = bot->getClassMask();

    map<uint32, vector<TalentEntry const*> > spells;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if(!talentInfo)
            continue;

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry( talentInfo->TalentTab );
        if(!talentTabInfo || talentTabInfo->tabpage != specNo)
            continue;

        if( (classMask & talentTabInfo->ClassMask) == 0 )
            continue;

        spells[talentInfo->Row].push_back(talentInfo);
    }

    uint32 freePoints = bot->GetFreeTalentPoints();
    for (map<uint32, vector<TalentEntry const*> >::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        vector<TalentEntry const*> &spells = i->second;
        if (spells.empty())
        {
            sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "%s: No spells for talent row %d", bot->GetName().c_str(), i->first);
            continue;
        }

        int attemptCount = 0;
        while (!spells.empty() && (int)freePoints - (int)bot->GetFreeTalentPoints() < 5 && attemptCount++ < 3 && bot->GetFreeTalentPoints())
        {
            int index = urand(0, spells.size() - 1);
            TalentEntry const *talentInfo = spells[index];
            int maxRank = 0;
            for (int rank = 0; rank < min((uint32)MAX_TALENT_RANK, bot->GetFreeTalentPoints()); ++rank)
            {
                uint32 spellId = talentInfo->RankID[rank];
                if (!spellId)
                    continue;

                maxRank = rank;
            }

            bot->LearnTalent(talentInfo->TalentID, maxRank);
			spells.erase(spells.begin() + index);
        }

        freePoints = bot->GetFreeTalentPoints();
    }

    for (uint32 i = 0; i < MAX_TALENT_SPECS; ++i)
    {
        for (PlayerTalentMap::iterator itr = bot->GetTalentMap(i).begin(); itr != bot->GetTalentMap(i).end(); ++itr)
        {
            if (itr->second->state != PLAYERSPELL_REMOVED)
                itr->second->state = PLAYERSPELL_CHANGED;
        }
    }
}

ObjectGuid PlayerbotFactory::GetRandomBot()
{
    vector<ObjectGuid> guids;
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        if (!sAccountMgr->GetCharactersCount(accountId))
            continue;

        QueryResult result = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE account = '%u'", accountId);
        if (!result)
            continue;

        do
        {
            Field* fields = result->Fetch();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            if (!sObjectMgr->GetPlayerByLowGUID(guid))
                guids.push_back(guid);
        } while (result->NextRow());
    }

    if (guids.empty())
        return ObjectGuid();

    int index = urand(0, guids.size() - 1);
    return guids[index];
}

void PlayerbotFactory::InitQuests()
{
    QueryResult results = WorldDatabase.PQuery("SELECT Id, RequiredClasses, RequiredRaces FROM quest_template where Level = -1 and MinLevel <= '%u'",
            bot->getLevel());

    list<uint32> ids;
    do
    {
        Field* fields = results->Fetch();
        uint32 questId = fields[0].GetUInt32();
        uint16 requiredClasses = fields[1].GetUInt16();
        uint16 requiredRaces = fields[2].GetUInt16();
        if ((requiredClasses & bot->getClassMask()) && (requiredRaces & bot->getRaceMask()))
            ids.push_back(questId);
    } while (results->NextRow());

    for (int i = 0; i < 15; i++)
    {
        for (list<uint32>::iterator i = ids.begin(); i != ids.end(); ++i)
        {
			uint32 questId = *i;
            Quest const *quest = sObjectMgr->GetQuestTemplate(questId);

            bot->SetQuestStatus(questId, QUEST_STATUS_NONE);

            if (!bot->SatisfyQuestClass(quest, false) ||
                    !bot->SatisfyQuestRace(quest, false) ||
                    !bot->SatisfyQuestStatus(quest, false))
                continue;

            if (quest->IsDailyOrWeekly() || quest->IsRepeatable() || quest->IsMonthly())
                continue;

            bot->SetQuestStatus(questId, QUEST_STATUS_COMPLETE);
            bot->RewardQuest(quest, 0, bot, false);
            ClearInventory();
        }
    }
}

void PlayerbotFactory::ClearInventory()
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor);
}

// FEYZEE: new functions used by init=high80 command
void PlayerbotFactory::ClearAllInventory()
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor, ITERATE_ALL_ITEMS);
}

void PlayerbotFactory::InitAmmo()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE && bot->getClass() != CLASS_WARRIOR)
        return;

    Item* const pItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!pItem)
        return;

    uint32 subClass = 0;
    switch (pItem->GetTemplate()->SubClass)
    {
    case ITEM_SUBCLASS_WEAPON_GUN:
        subClass = ITEM_SUBCLASS_BULLET;
        break;
    case ITEM_SUBCLASS_WEAPON_BOW:
    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
        subClass = ITEM_SUBCLASS_ARROW;
        break;
    }

    if (!subClass)
        return;

    QueryResult results = WorldDatabase.PQuery("select max(entry), max(RequiredLevel) from item_template where class = '%u' and subclass = '%u' and RequiredLevel <= '%u'",
            ITEM_CLASS_PROJECTILE, subClass, bot->getLevel());

    Field* fields = results->Fetch();
    if (fields)
    {
        uint32 entry = fields[0].GetUInt32();
        for (int i = 0; i < 5; i++)
        {
            bot->StoreNewItemInBestSlots(entry, 1000);
        }
        bot->SetAmmo(entry);
    }
}

void PlayerbotFactory::InitMounts()
{
    map<uint32, map<int32, vector<uint32> > > allSpells;

    for (uint32 spellId = 0; spellId < sSpellStore.GetNumRows(); ++spellId)
    {
        SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || spellInfo->Effects[0].ApplyAuraName != SPELL_AURA_MOUNTED)
            continue;

        if (spellInfo->GetDuration() != -1)
            continue;

        int32 effect = max(spellInfo->Effects[1].BasePoints, spellInfo->Effects[2].BasePoints);
        if (effect < 50)
            continue;

        uint32 index = (spellInfo->Effects[1].ApplyAuraName == SPELL_AURA_MOD_MOUNTED_FLIGHT_SPEED_ALWAYS ||
                spellInfo->Effects[2].ApplyAuraName == SPELL_AURA_MOD_MOUNTED_FLIGHT_SPEED_ALWAYS) ? 1 : 0;
        allSpells[index][effect].push_back(spellId);
    }

    for (uint32 type = 0; type < 2; ++type)
    {
        map<int32, vector<uint32> >& spells = allSpells[type];
        for (map<int32, vector<uint32> >::iterator i = spells.begin(); i != spells.end(); ++i)
        {
            int32 effect = i->first;
            vector<uint32>& ids = i->second;
            uint32 index = urand(0, ids.size() - 1);
            if (index >= ids.size())
                continue;

            bot->LearnSpell(ids[index], false);
        }
    }
}

void PlayerbotFactory::InitPotions()
{
    map<uint32, vector<uint32> > items;
    ItemTemplateContainer const* itemTemplateContainer = sObjectMgr->GetItemTemplateStore();
    for (ItemTemplateContainer::const_iterator i = itemTemplateContainer->begin(); i != itemTemplateContainer->end(); ++i)
    {
        ItemTemplate const& itemTemplate = i->second;
        uint32 itemId = i->first;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;

        if (proto->Class != ITEM_CLASS_CONSUMABLE ||
            proto->SubClass != ITEM_SUBCLASS_POTION ||
            proto->Spells[0].SpellCategory != 4 ||
            proto->Bonding != NO_BIND)
            continue;

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
            continue;

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
            continue;

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
            continue;

        for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
        {
            const SpellInfo* const spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[j].SpellId);
            if (!spellInfo)
                continue;

            for (int i = 0 ; i < 3; i++)
            {
                if (spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL || spellInfo->Effects[i].Effect == SPELL_EFFECT_ENERGIZE)
                {
                    items[spellInfo->Effects[i].Effect].push_back(itemId);
                    break;
                }
            }
        }
    }

    uint32 effects[] = { SPELL_EFFECT_HEAL, SPELL_EFFECT_ENERGIZE };
    for (int i = 0; i < sizeof(effects) / sizeof(uint32); ++i)
    {
        uint32 effect = effects[i];
        vector<uint32>& ids = items[effect];
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
            continue;

        uint32 itemId = ids[index];
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        bot->StoreNewItemInBestSlots(itemId, urand(1, proto->GetMaxStackSize()));
   }
}

void PlayerbotFactory::InitFood()
{
    map<uint32, vector<uint32> > items;
    ItemTemplateContainer const* itemTemplateContainer = sObjectMgr->GetItemTemplateStore();
    for (ItemTemplateContainer::const_iterator i = itemTemplateContainer->begin(); i != itemTemplateContainer->end(); ++i)
    {
        ItemTemplate const& itemTemplate = i->second;
        uint32 itemId = i->first;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;

        if (proto->Class != ITEM_CLASS_CONSUMABLE ||
            proto->SubClass != ITEM_SUBCLASS_FOOD ||
            (proto->Spells[0].SpellCategory != 11 && proto->Spells[0].SpellCategory != 59) ||
            proto->Bonding != NO_BIND)
            continue;

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
            continue;

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
            continue;

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
            continue;

        items[proto->Spells[0].SpellCategory].push_back(itemId);
    }

    uint32 categories[] = { 11, 59 };
    for (int i = 0; i < sizeof(categories) / sizeof(uint32); ++i)
    {
        uint32 category = categories[i];
        vector<uint32>& ids = items[category];
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
            continue;

        uint32 itemId = ids[index];
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        bot->StoreNewItemInBestSlots(itemId, urand(1, proto->GetMaxStackSize()));
   }
}


void PlayerbotFactory::CancelAuras()
{
    bot->RemoveAllAuras();
}

void PlayerbotFactory::InitInventory()
{
    InitInventoryTrade();
    InitInventoryEquip();
    InitInventorySkill();
}

void PlayerbotFactory::InitInventorySkill()
{
    if (bot->HasSkill(SKILL_MINING)) {
        StoreItem(2901, 1); // Mining Pick
    }
    if (bot->HasSkill(SKILL_JEWELCRAFTING)) {
        StoreItem(20815, 1); // Jeweler's Kit
        StoreItem(20824, 1); // Simple Grinder
    }
    if (bot->HasSkill(SKILL_BLACKSMITHING) || bot->HasSkill(SKILL_ENGINEERING)) {
        StoreItem(5956, 1); // Blacksmith Hammer
    }
    if (bot->HasSkill(SKILL_ENGINEERING)) {
        StoreItem(6219, 1); // Arclight Spanner
    }
    if (bot->HasSkill(SKILL_ENCHANTING)) {
        StoreItem(44452, 1); // Runed Titanium Rod
    }
    if (bot->HasSkill(SKILL_INSCRIPTION)) {
        StoreItem(39505, 1); // Virtuoso Inking Set
    }
    if (bot->HasSkill(SKILL_SKINNING)) {
        StoreItem(7005, 1); // Skinning Knife
    }
}

Item* PlayerbotFactory::StoreItem(uint32 itemId, uint32 count)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    ItemPosCountVec sDest;
    InventoryResult msg = bot->CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, sDest, itemId, count);
    if (msg != EQUIP_ERR_OK)
        return NULL;

    return bot->StoreNewItem(sDest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
}

void PlayerbotFactory::InitInventoryTrade()
{
    vector<uint32> ids;
    ItemTemplateContainer const* itemTemplateContainer = sObjectMgr->GetItemTemplateStore();
    for (ItemTemplateContainer::const_iterator i = itemTemplateContainer->begin(); i != itemTemplateContainer->end(); ++i)
    {
        ItemTemplate const& itemTemplate = i->second;
        uint32 itemId = i->first;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;

        if (proto->Class != ITEM_CLASS_TRADE_GOODS || proto->Bonding != NO_BIND)
            continue;

        if (proto->ItemLevel < bot->getLevel())
            continue;

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
            continue;

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
            continue;

        ids.push_back(itemId);
    }

    if (ids.empty())
    {
        // FEYZEE: hide error caused by no trade items available
        //sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "No trade items available for bot %s (%d level)", bot->GetName().c_str(), bot->getLevel());
        return;
    }

    uint32 index = urand(0, ids.size() - 1);
    if (index >= ids.size())
        return;

    uint32 itemId = ids[index];
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return;

    uint32 count = 1, stacks = 1;
    switch (proto->Quality)
    {
    case ITEM_QUALITY_NORMAL:
        count = proto->GetMaxStackSize();
        stacks = urand(1, 7) / auctionbot.GetRarityPriceMultiplier(proto);
        break;
    case ITEM_QUALITY_UNCOMMON:
        stacks = 1;
        count = urand(1, proto->GetMaxStackSize());
        break;
    case ITEM_QUALITY_RARE:
        stacks = 1;
        count = urand(1, min(uint32(3), proto->GetMaxStackSize()));
        break;
    }

    for (uint32 i = 0; i < stacks; i++)
        StoreItem(itemId, count);
}

void PlayerbotFactory::InitInventoryEquip()
{
    vector<uint32> ids;

    uint32 desiredQuality = itemQuality;
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
        desiredQuality--;
    }

    ItemTemplateContainer const* itemTemplateContainer = sObjectMgr->GetItemTemplateStore();
    for (ItemTemplateContainer::const_iterator i = itemTemplateContainer->begin(); i != itemTemplateContainer->end(); ++i)
    {
        ItemTemplate const& itemTemplate = i->second;
        uint32 itemId = i->first;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;

        if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON || (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_WHEN_USE))
            continue;

        if (proto->Class == ITEM_CLASS_ARMOR && !CanEquipArmor(proto))
            continue;

        if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(proto))
            continue;

        if (!CanEquipItem(proto, desiredQuality))
            continue;

        ids.push_back(itemId);
    }

    int maxCount = urand(0, 3);
    int count = 0;
    for (int attempts = 0; attempts < 15; attempts++)
    {
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
            continue;

        uint32 itemId = ids[index];
        if (StoreItem(itemId, 1) && count++ >= maxCount)
            break;
   }
}

void PlayerbotFactory::InitGlyphs()
{
    bot->InitGlyphsForLevel();

    for (uint32 slotIndex = 0; slotIndex < MAX_GLYPH_SLOT_INDEX; ++slotIndex)
    {
        bot->SetGlyph(slotIndex, 0);
    }

    uint32 level = bot->getLevel();
    uint32 maxSlot = 0;
    if (level >= 15)
        maxSlot = 2;
    if (level >= 30)
        maxSlot = 3;
    if (level >= 50)
        maxSlot = 4;
    if (level >= 70)
        maxSlot = 5;
    if (level >= 80)
        maxSlot = 6;

    if (!maxSlot)
        return;

    list<uint32> glyphs;
    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    for (ItemTemplateContainer::const_iterator i = itemTemplates->begin(); i != itemTemplates->end(); ++i)
    {
        uint32 itemId = i->first;
        ItemTemplate const* proto = &i->second;
        if (!proto)
            continue;

        if (proto->Class != ITEM_CLASS_GLYPH)
            continue;

        if ((proto->AllowableClass & bot->getClassMask()) == 0 || (proto->AllowableRace & bot->getRaceMask()) == 0)
            continue;

        for (uint32 spell = 0; spell < MAX_ITEM_PROTO_SPELLS; spell++)
        {
            uint32 spellId = proto->Spells[spell].SpellId;
            SpellInfo const *entry = sSpellMgr->GetSpellInfo(spellId);
            if (!entry)
                continue;

            for (uint32 effect = 0; effect <= EFFECT_2; ++effect)
            {
                if (entry->Effects[effect].Effect != SPELL_EFFECT_APPLY_GLYPH)
                    continue;

                uint32 glyph = entry->Effects[effect].MiscValue;
                glyphs.push_back(glyph);
            }
        }
    }

    if (glyphs.empty())
    {
        sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "No glyphs found for bot %s", bot->GetName().c_str());
        return;
    }

    set<uint32> chosen;
    for (uint32 slotIndex = 0; slotIndex < maxSlot; ++slotIndex)
    {
        uint32 slot = bot->GetGlyphSlot(slotIndex);
        GlyphSlotEntry const *gs = sGlyphSlotStore.LookupEntry(slot);
        if (!gs)
            continue;

        vector<uint32> ids;
        for (list<uint32>::iterator i = glyphs.begin(); i != glyphs.end(); ++i)
        {
            uint32 id = *i;
            GlyphPropertiesEntry const *gp = sGlyphPropertiesStore.LookupEntry(id);
            if (!gp || gp->TypeFlags != gs->TypeFlags)
                continue;

            ids.push_back(id);
        }

        int maxCount = urand(0, 3);
        int count = 0;
        bool found = false;
        for (int attempts = 0; attempts < 15; ++attempts)
        {
            uint32 index = urand(0, ids.size() - 1);
            if (index >= ids.size())
                continue;

            uint32 id = ids[index];
            if (chosen.find(id) != chosen.end())
                continue;

            chosen.insert(id);

            bot->SetGlyph(slotIndex, id);
            found = true;
            break;
        }
        if (!found)
            sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "No glyphs found for bot %s index %d slot %d", bot->GetName().c_str(), slotIndex, slot);
    }
}

void PlayerbotFactory::InitGuild()
{
    if (bot->GetGuildId())
        return;

    if (sPlayerbotAIConfig.randomBotGuilds.empty())
        RandomPlayerbotFactory::CreateRandomGuilds();

    vector<uint32> guilds;
    for(list<uint32>::iterator i = sPlayerbotAIConfig.randomBotGuilds.begin(); i != sPlayerbotAIConfig.randomBotGuilds.end(); ++i)
        guilds.push_back(*i);

    if (guilds.empty())
    {
        sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "No random guilds available");
        return;
    }

    int index = urand(0, guilds.size() - 1);
    uint32 guildId = guilds[index];
    Guild* guild = sGuildMgr->GetGuildById(guildId);
    if (!guild)
    {
        sLog->outMessage("playerbot", LOG_LEVEL_ERROR, "Invalid guild %u", guildId);
        return;
    }

    if (guild->GetMemberCount() < 10)
        guild->AddMember(bot->GetGUID(), urand(GR_OFFICER, GR_INITIATE));
}