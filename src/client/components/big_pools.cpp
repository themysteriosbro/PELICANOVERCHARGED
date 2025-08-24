#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "scheduler.hpp"
#include <Windows.h>

namespace bigpools
{
    constexpr int kTargetSize = 50000; // raise if needed

    static void* va_alloc(size_t bytes)
    {
        return ::VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }

    static void install_big_pools()
    {
        for (int t = 0; t < game::XAssetType::ASSET_TYPE_COUNT; ++t)
        {
            auto& pool = game::DB_XAssetPool[t];
            const auto structSize = game::DB_GetXAssetTypeSize(static_cast<game::XAssetType>(t));
            if (!pool.pool || pool.itemCount >= kTargetSize) continue;

            auto* base = static_cast<game::AssetLink*>(va_alloc(static_cast<size_t>(structSize) * kTargetSize));
            if (!base) { game::minlog.WriteError("VirtualAlloc failed for pool %d", t); continue; }

            for (int i = 0; i < kTargetSize - 1; ++i)
            {
                auto* cur  = reinterpret_cast<game::AssetLink*>(reinterpret_cast<uint8_t*>(base) + static_cast<size_t>(i) * structSize);
                auto* next = reinterpret_cast<game::AssetLink*>(reinterpret_cast<uint8_t*>(base) + static_cast<size_t>(i + 1) * structSize);
                cur->next = next;
            }
            reinterpret_cast<game::AssetLink*>(reinterpret_cast<uint8_t*>(base) + static_cast<size_t>(kTargetSize - 1) * structSize)->next = nullptr;

            pool.pool = base;
            pool.itemCount = kTargetSize;
        }

        game::minlog.WriteLine("Big contiguous XAsset pools installed.");
    }

    class component final : public component_interface
    {
    public:
        void post_start() override
        {
            scheduler::on_game_initialized([]{
                install_big_pools();
            }, scheduler::pipeline::main, std::chrono::milliseconds(0));
        }
    };
}

REGISTER_COMPONENT(bigpools::component)
