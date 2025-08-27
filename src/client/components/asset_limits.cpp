#include <std_include.hpp>
#include "asset_limits.hpp"
#include "game/game.hpp"

namespace asset_limits
{
    void asset_limits::resize(game::XAssetType type, int /*newSize*/)
    {
        const int targetSize = 80000; // set your global size here

        const auto structSize = game::DB_GetXAssetTypeSize(type);
        auto* assetPool = &game::DB_XAssetPool[type];

        // Only grow, never shrink
        if (assetPool && assetPool->itemCount < targetSize)
        {
            // allocate exactly the delta needed to reach targetSize
            auto* newBlock = static_cast<game::AssetLink*>(
                calloc(static_cast<size_t>(targetSize - assetPool->itemCount), structSize)
                );
            if (!newBlock) return;

            // chain the new block to the end of the existing free list
            auto* blockPtr = static_cast<game::AssetLink*>(assetPool->pool);
            auto* nextBlockPtr = newBlock;

            // walk/build links until we’ve added (targetSize - currentCount) items
            auto remaining = targetSize - 1; // matches original traversal logic
            do
            {
                if (!blockPtr->next)
                {
                    blockPtr->next = nextBlockPtr;
                    blockPtr = nextBlockPtr;
                    nextBlockPtr = reinterpret_cast<game::AssetLink*>(
                        reinterpret_cast<uint8_t*>(nextBlockPtr) + structSize
                        );
                }
                else
                {
                    blockPtr = reinterpret_cast<game::AssetLink*>(
                        reinterpret_cast<uint8_t*>(blockPtr) + structSize
                        );
                }
                --remaining;
            } while (remaining);

            assetPool->itemCount = targetSize;
        }
    }

}
