// Problem 065 - A Naïve but Block-based Allocator
// The OJ provides definitions of getNewBlock/freeBlock in its driver.
// We only declare them here and implement a block-based int allocator
// that carves allocations from 4096-byte blocks obtained via getNewBlock.

#pragma once

#include <cstddef>
#include <list>
#include <vector>
#include <algorithm>

// Provided by the judge (do not define here)
int* getNewBlock(int n);
void freeBlock(const int* block, int n);

class Allocator {
public:
    Allocator() = default;

    ~Allocator() {
        // Release all blocks we ever acquired (if any)
        for (auto& blk : blocks_) {
            if (blk.base) {
                freeBlock(blk.base, blk.blocks);
                blk.base = nullptr;
                blk.blocks = 0;
                blk.capacity = 0;
                blk.top = 0;
                blk.used_live = 0;
            }
        }
        // Clear free segments
        free_list_.clear();
        last_block_ = nullptr;
    }

    // Allocate a sequence of memory space of n int
    int* allocate(int n) {
        if (n <= 0) return nullptr;

        // Prefer carving from the tail of the last obtained block
        if (last_block_ && tailAvailable(*last_block_) >= n) {
            int* p = last_block_->base + last_block_->top;
            last_block_->top += n;
            last_block_->used_live += n;
            return p;
        }

        // Otherwise, try to reuse any free segments (first-fit)
        for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
            if (it->len >= n) {
                int* p = it->ptr;
                it->ptr += n;
                it->len -= n;
                it->blk->used_live += n;
                if (it->len == 0) free_list_.erase(it);
                return p;
            }
        }

        // Try to reuse an entirely empty block that is large enough
        Block* reusable = findReusableEmptyBlock(n);
        if (reusable) {
            reusable->top = 0;  // bump pointer reset
            int* p = reusable->base;
            reusable->top = n;
            reusable->used_live = n;
            last_block_ = reusable; // treat it as the most recently obtained block
            return p;
        }

        // Need to obtain new block(s)
        const int ints_per_block = kIntsPerBlock;
        int blocks_needed = (n + ints_per_block - 1) / ints_per_block;
        if (blocks_needed <= 0) blocks_needed = 1;
        int* base = getNewBlock(blocks_needed);
        if (!base) return nullptr;

        Block blk;
        blk.base = base;
        blk.blocks = blocks_needed;
        blk.capacity = blocks_needed * ints_per_block;
        blk.top = n;
        blk.used_live = n;
        blocks_.push_back(blk);
        last_block_ = &blocks_.back();
        return base;
    }

    // Deallocate the memory previously allocated by allocate(n)
    void deallocate(int* pointer, int n) {
        if (!pointer || n <= 0) return;

        Block* blk = findBlock(pointer);
        if (!blk) return; // undefined behaviour per problem; just ignore

        // If this is the most recent allocation from the last block's tail,
        // roll back the bump pointer to make space immediately reusable.
        if (blk == last_block_) {
            int* expect = blk->base + (blk->top - n);
            if (pointer == expect) {
                blk->top -= n;
                blk->used_live -= n;
                // If the block becomes entirely empty, keep it for reuse
                return;
            }
        }

        // Otherwise, record as a free segment inside its block
        FreeSeg seg;
        seg.ptr = pointer;
        seg.len = n;
        seg.blk = blk;
        free_list_.push_back(seg);
        blk->used_live -= n;

        // Optional: if the whole block is free and it's not the last block
        // we keep it reusable (do not necessarily call freeBlock now).
    }

private:
    struct Block {
        int* base = nullptr;   // base address of this block region
        int  blocks = 0;       // count of 4096-byte blocks backing this region
        int  capacity = 0;     // capacity in ints
        int  top = 0;          // bump-pointer (ints used from start, ignoring interior frees)
        int  used_live = 0;    // total ints currently allocated and not deallocated
    };

    struct FreeSeg {
        int* ptr = nullptr; // start of free segment
        int  len = 0;       // length in ints
        Block* blk = nullptr; // owning block
    };

    static constexpr int kBlockBytes = 4096;
    static constexpr int kIntsPerBlock = kBlockBytes / static_cast<int>(sizeof(int));

    // Storage of all blocks we have ever obtained or reused
    std::list<Block> blocks_;
    // Free segments available for reuse (across any block)
    std::list<FreeSeg> free_list_;
    // The last obtained block (new or reused empty block)
    Block* last_block_ = nullptr;

    static int tailAvailable(const Block& b) {
        return b.capacity - b.top;
    }

    Block* findBlock(int* ptr) {
        for (auto& blk : blocks_) {
            if (ptr >= blk.base && ptr < blk.base + blk.capacity) {
                return &blk;
            }
        }
        return nullptr;
    }

    Block* findReusableEmptyBlock(int need_ints) {
        for (auto& blk : blocks_) {
            if (&blk == last_block_) continue; // if last_block_ insufficient, skip; we'll request new or others
            if (blk.used_live == 0 && blk.capacity >= need_ints) {
                return &blk;
            }
        }
        return nullptr;
    }
};

