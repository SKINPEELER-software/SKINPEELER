//********************************************************************
//
//    FILENAME    : rg_BlockArena.h
//
//    DESCRIPTION :
//        A simple bump-pointer monotonic memory arena used as an
//        OPTIONAL backing store for rg_dList<T> nodes (see rg_dList.h).
//
//        It hands out fixed-size blocks of raw, aligned memory from
//        large preallocated chunks and releases them ALL AT ONCE in
//        reset() / the destructor -- there is no per-object free.
//
//        Node addresses handed out by allocate() stay stable for the
//        whole lifetime of the arena (an allocated block is never
//        moved or copied), which preserves the pointer-stability
//        contract that the 1997-era pointer-chasing geometry code
//        throughout V-ImageProcessing (rg_dList, WEDS, BetaUniverse2D,
//        ...) relies on.
//
//        NOT thread-safe. NOT a general-purpose allocator: it is only
//        meant to replace per-node `new`/`delete` churn in
//        rg_dList<T>::add()/removeAll() for lists whose lifetime is
//        scoped to a single owning object (e.g. BetaUniverse2D), where
//        individual node deletion never happens mid-life (only a bulk
//        clear at teardown). Freeing an individual block is therefore
//        NOT supported; allocate() is the only mutating entry point
//        besides reset().
//
//    AUTHOR      : perf/2a-lite experiment (2026-10-29)
//
//*********************************************************************

#ifndef _RG_BLOCKARENA_H
#define _RG_BLOCKARENA_H

#include <cstddef>
#include <vector>

class rg_BlockArena
{
public:
    // blockBytes: size (in bytes) of each underlying chunk the arena
    // requests from the system allocator. A single allocate() request
    // larger than blockBytes still succeeds -- it simply gets its own
    // oversized chunk.
    explicit rg_BlockArena(std::size_t blockBytes = (4u << 20)); // default 4 MiB chunks
    ~rg_BlockArena();

    // Arenas own raw memory chunks; copying would be meaningless and
    // every current owner (BetaUniverse2D) is itself non-copyable in
    // practice, so these are simply disabled.
    rg_BlockArena(const rg_BlockArena&)            = delete;
    rg_BlockArena& operator=(const rg_BlockArena&) = delete;

    // Return nbytes of raw, aligned memory from the arena. Never
    // returns a null pointer (throws std::bad_alloc on allocation
    // failure, same contract as `operator new`).
    void* allocate(std::size_t nbytes);

    // Release every chunk back to the system allocator. Any pointer
    // previously returned by allocate() becomes invalid. Safe to call
    // repeatedly (including on an already-empty arena).
    void reset();

    std::size_t numChunks()     const { return m_chunks.size(); }
    std::size_t bytesReserved() const;
    std::size_t bytesUsed()     const;

private:
    struct Chunk {
        char*       data;
        std::size_t capacity;
        std::size_t used;
    };

    void addChunk(std::size_t minBytes);

    std::size_t        m_chunkBytes;
    std::vector<Chunk> m_chunks;
};

#endif
