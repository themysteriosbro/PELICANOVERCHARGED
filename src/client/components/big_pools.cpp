#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "components/scheduler.hpp"
#include <Windows.h>
#include <string>

namespace bigpools
{
    constexpr int kTargetSize = 50000; // raise if needed

    static void* va_alloc(size_t bytes)
    {
        return ::VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }

    // now non-static so Pelican_Bootstrap() can call it
    void install_big_pools()
    {
        for (int t = 0; t < game::XAssetType::ASSET_TYPE_COUNT; ++t)
        {
            auto& pool = game::DB_XAssetPool[t];
            const auto structSize = game::DB_GetXAssetTypeSize(static_cast<game::XAssetType>(t));

            // Skip only if type size is invalid OR we've already grown it
            if (structSize <= 0 || pool.itemCount >= kTargetSize)
                continue;

            // Allocate a brand new contiguous block even if pool.pool is currently null
            auto* base = static_cast<game::AssetLink*>(
                va_alloc(static_cast<size_t>(structSize) * kTargetSize)
                );
            if (!base)
            {
                const std::string msg = "VirtualAlloc failed for pool " + std::to_string(t);
                game::minlog.WriteLine(msg.c_str());
                continue;
            }

            // Build free list across the contiguous region
            for (int i = 0; i < kTargetSize - 1; ++i)
            {
                auto* cur = reinterpret_cast<game::AssetLink*>(
                    reinterpret_cast<uint8_t*>(base) + static_cast<size_t>(i) * structSize
                    );
                auto* next = reinterpret_cast<game::AssetLink*>(
                    reinterpret_cast<uint8_t*>(base) + static_cast<size_t>(i + 1) * structSize
                    );
                cur->next = next;
            }
            reinterpret_cast<game::AssetLink*>(
                reinterpret_cast<uint8_t*>(base) + static_cast<size_t>(kTargetSize - 1) * structSize
                )->next = nullptr;

            // Install the new pool (overwrite even if previous was null)
            pool.pool = base;
            pool.itemCount = kTargetSize;
        }

        game::minlog.WriteLine("Big contiguous XAsset pools installed.");
    }

    // Optional: keep component path too; harmless if bootstrap already called us
    class component final : public component_interface
    {
    public:
        void post_start() override
        {
            scheduler::on_game_initialized([] {
                install_big_pools();
                }, scheduler::pipeline::main, std::chrono::milliseconds(0));
        }
    };
}

REGISTER_COMPONENT(bigpools::component)
