/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <settings/String.hpp>
#include "HookedMethods.hpp"

static settings::String ipc_name{ "name.ipc", "" };
static settings::String force_name{ "name.custom", "" };
static settings::Int namesteal{ "name.namesteal", "0" };

static std::string stolen_name;

// Func to get a new entity to steal name from and returns true if a target has
// been found
bool StolenName()
{

    // Array to store potential namestealer targets with a bookkeeper to tell
    // how full it is
    int potential_targets[32];
    int potential_targets_length = 0;

    // Go through entities looking for potential targets
    for (int i = 1; i < g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *ent = ENTITY(i);

        // Check if ent is a good target
        if (!ent)
            continue;
        if (ent == LOCAL_E)
            continue;
        if (ent->m_Type() != ENTITY_PLAYER)
            continue;
        if (ent->m_bEnemy())
            continue;

        // Check if name is current one
        player_info_s info;
        if (g_IEngine->GetPlayerInfo(ent->m_IDX, &info))
        {

            // If our name is the same as current, than change it
            if (std::string(info.name) == stolen_name)
            {
                // Since we found the ent we stole our name from and it is still
                // good, if user settings are passive, then we return true and
                // dont alter our name
                if ((int) namesteal == 1)
                {
                    return true;
                    // Otherwise we continue to change our name to something
                    // else
                }
                else
                    continue;
            }

            // a ent without a name is no ent we need, contine for a different
            // one
        }
        else
            continue;

        // Save the ent to our array
        potential_targets[potential_targets_length] = i;
        potential_targets_length++;

        // With our maximum amount of players reached, dont search for anymore
        if (potential_targets_length >= 32)
            break;
    }

    // Checks to prevent crashes
    if (potential_targets_length == 0)
        return false;

    // Get random number that we can use with our array
    int target_random_num =
        floor(RandFloatRange(0, potential_targets_length - 0.1F));

    // Get a idx from our random array position
    int new_target = potential_targets[target_random_num];

    // Grab username of user
    player_info_s info;
    if (g_IEngine->GetPlayerInfo(new_target, &info))
    {

        // If our name is the same as current, than change it and return true
        stolen_name = std::string(info.name);
        return true;
    }

    // Didnt get playerinfo
    return false;
}
const char *GetNamestealName(CSteamID steam_id)
{
#if ENABLE_IPC
    if (ipc::peer)
    {
        std::string namestr(*ipc_name);
        if (namestr.length() > 3)
        {
            ReplaceString(namestr, "%%", std::to_string(ipc::peer->client_id));
            ReplaceString(namestr, "\\n", "\n");
            ReplaceString(namestr, "\\015", "\015");
            ReplaceString(namestr, "\\u200F", "\u200F");
            return namestr.c_str();
        }
    }
#endif

    // Check User settings if namesteal is allowed
    if (namesteal && steam_id == g_ISteamUser->GetSteamID())
    {

        // We dont want to steal names while not in-game as there are no targets
        // to steal from. We want to be on a team as well to get teammates names
        if (g_IEngine->IsInGame() && g_pLocalPlayer->team)
        {

            // Check if we have a username to steal, func automaticly steals a
            // name in it.
            if (StolenName())
            {

                // Return the name that has changed from the func above
                return format(stolen_name, "\015").c_str();
            }
        }
    }

    if ((*force_name).size() > 1 && steam_id == g_ISteamUser->GetSteamID())
    {
        auto new_name = force_name.toString();
        ReplaceString(new_name, "\\n", "\n");
        ReplaceString(new_name, "\\015", "\015");
        ReplaceString(new_name, "\\u200F", "\u200F");

        return new_name.c_str();
    }
    return nullptr;
}
namespace hooked_methods
{

DEFINE_HOOKED_METHOD(GetFriendPersonaName, const char *, ISteamFriends *this_,
                     CSteamID steam_id)
{
    const char *new_name = GetNamestealName(steam_id);
    return (new_name ? new_name : original::GetFriendPersonaName(this_, steam_id));
}
static InitRoutine init([](){
    namesteal.installChangeCallback([](settings::VariableBase<int> &var, int new_val){
        if (new_val != 0)
        {
            const char *xd = GetNamestealName(g_ISteamUser->GetSteamID());
            if (CE_BAD(LOCAL_E) || !xd)
                return;
            NET_SetConVar setname("name", xd);
            INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
            if (ch)
            {
                setname.SetNetChannel(ch);
                setname.SetReliable(false);
                ch->SendNetMsg(setname, false);
            }
        }
    });
});
static Timer set_name{};
static HookedFunction CM(HookedFunctions_types::HF_CreateMove, "namesteal", 2, [](){
    if (!namesteal)
        return;
    if (!set_name.test_and_set(500000))
        return;
    const char *name = GetNamestealName(g_ISteamUser->GetSteamID());
    if (CE_BAD(LOCAL_E) || !name)
        return;
    NET_SetConVar setname("name", name);
    INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (ch)
    {
        setname.SetNetChannel(ch);
        setname.SetReliable(false);
        ch->SendNetMsg(setname, false);
    }
});
} // namespace hooked_methods
