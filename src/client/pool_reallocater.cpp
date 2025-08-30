// db_pool_create_hook.cpp
#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "utils/hook.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>

// TODO: set this to the real address for your exe.
// This should point to the engine function that creates a pool:
//   void DB_CreateXAssetPool(int type, int itemSize, int itemCount)
// After it returns, DB_XAssetPool[type] is initialized with base pool.
#define DB_CREATE_POOL_ADDR REBASE(0x1494073F0) // <— UPDATE THIS

namespace
{
    // Focus types likely to blow up first; you can expand this list as needed.
    static const game::XAssetType kTargetTypes[] = {
        game::ASSET_TYPE_IMAGE,
        game::ASSET_TYPE_MATERIAL,
        game::ASSET_TYPE_XMODEL,
        game::ASSET_TYPE_XANIMPARTS,
        game::ASSET_TYPE_UI_MAP,
        game::ASSET_TYPE_RAWFILE,
        game::ASSET_TYPE_STRINGTABLE,
        game::ASSET_TYPE_STRUCTURED_TABLE,
        game::ASSET_TYPE_SCRIPTPARSETREE,
        game::ASSET_TYPE_WEAPON,
        game::ASSET_TYPE_WEAPON_VARIANT,
        game::ASSET_TYPE_WEAPON_FULL,
        game::ASSET_TYPE_FX,
        game::ASSET_TYPE_TAGFX,
        game::ASSET_TYPE_SOUND,
        game::ASSET_TYPE_PLAYERSOUNDS,
        game::ASSET_TYPE_LOCALIZE_ENTRY,
        game::ASSET_TYPE_FONT,
        game::ASSET_TYPE_FONTICON,
    };

    static bool is_target(game::XAssetType t)
    {
        for (auto x : kTargetTypes) if (x == t) return true;
        return false;
    }

    static int floor_for(game::XAssetType t)
    {
        switch (t)
        {
        case game::ASSET_TYPE_IMAGE:               return 8192;
        case game::ASSET_TYPE_MATERIAL:            return 8192;
        case game::ASSET_TYPE_XMODEL:              return 4096;
        case game::ASSET_TYPE_XANIMPARTS:          return 4096;
        case game::ASSET_TYPE_UI_MAP:              return 1024;
        case game::ASSET_TYPE_RAWFILE:             return 4096;
        case game::ASSET_TYPE_STRINGTABLE:         return 2048;
        case game::ASSET_TYPE_STRUCTURED_TABLE:    return 2048;
        case game::ASSET_TYPE_SCRIPTPARSETREE:     return 4096;
        case game::ASSET_TYPE_WEAPON:              return 1024;
        case game::ASSET_TYPE_WEAPON_VARIANT:      return 2048;
        case game::ASSET_TYPE_WEAPON_FULL:         return 1024;
        case game::ASSET_TYPE_FX:                  return 4096;
        case game::ASSET_TYPE_TAGFX:               return 2048;
        case game::ASSET_TYPE_SOUND:               return 4096;
        case game::ASSET_TYPE_PLAYERSOUNDS:        return 1024;
        case game::ASSET_TYPE_LOCALIZE_ENTRY:      return 4096;
        case game::ASSET_TYPE_FONT:                return 512;
        case game::ASSET_TYPE_FONTICON:            return 1024;
        default:                                   return 2048;
        }
    }

    static constexpr int kGrowFactor = 4;

    static void build_free_list(void* base, int count, int stride, game::AssetLink** out_head)
    {
        auto* head = reinterpret_cast<game::AssetLink*>(base);
        auto* prev = static_cast<game::AssetLink*>(nullptr);
        auto* p = reinterpret_cast<std::uint8_t*>(base);

        for (int i = 0; i < count; ++i)
        {
            auto* link = reinterpret_cast<game::AssetLink*>(p);
            if (prev) prev->next = link;
            prev = link;
            p += stride;
        }
        if (prev) prev->next = nullptr;
        *out_head = head;
    }

    // Signature: void(int type, int itemSize, int itemCount)
    using db_create_pool_fn = void(*)(int, int, int);
    static utils::hook::detour g_dbCreatePool;

    static void db_create_pool_hook(int type, int itemSize, int itemCount)
    {
        // Call original to let the engine set the initial pool struct up
        g_dbCreatePool.invoke<void>(type, itemSize, itemCount);

        const auto t = static_cast<game::XAssetType>(type);
        if (!is_target(t)) return;

        auto& pool = game::DB_XAssetPool[type];

        // Sanity: if engine hasn’t fully set fields, bail
        if (pool.itemSize <= 0 || pool.itemSize != itemSize) return;
        if (pool.itemCount <= 0) return;

        // At creation time this should still be zero; if not, don’t touch
        if (pool.itemAllocCount != 0) return;

        // Decide new capacity
        const int minFloor = floor_for(t);
        const int grown = std::max(pool.itemCount * kGrowFactor, minFloor);
        if (grown <= pool.itemCount) return;

        const int stride = pool.itemSize;
        const std::size_t bytes = static_cast<std::size_t>(grown) * static_cast<std::size_t>(stride);

        // Allocate a single, larger contiguous block
        void* newBlock = std::calloc(1, bytes);
        if (!newBlock) return;

        game::AssetLink* head = nullptr;
        build_free_list(newBlock, grown, stride, &head);

        // Swap in the larger pool; do NOT append blocks
        pool.pool = newBlock;
        pool.freeHead = head;
        pool.itemCount = grown;
        // itemSize, isSingleton[], itemAllocCount remain unchanged (allocCount should still be 0)

        // Optional: a tiny confirmation ping in the in-game console (safe & visible)
        game::Com_Printf(game::CON_CHANNEL_INFO, game::CON_LABEL_DEFAULT,
            "[T7O] DB_CreateXAssetPool: type=%d size=%d old=%d new=%d\n",
            type, pool.itemSize, itemCount, grown);
    }

    class component final : public component_interface
    {
    public:
        void post_start() override
        {
            // Install the detour immediately at DLL init — very early, before FE/MP mounts.
            g_dbCreatePool.create(DB_CREATE_POOL_ADDR, &db_create_pool_hook);
        }

        void pre_destroy() override
        {
            g_dbCreatePool.clear();
        }
    };
}

REGISTER_COMPONENT(component)
