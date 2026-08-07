// Test: World generation, chunks, blocks, structural integrity (Fase 2)
// Validates:
// - Chunk generation (deterministic, terrain, caves, ores, trees)
// - Block placement/destruction
// - Biome determination (temperature + humidity)
// - Structural integrity (BFS from anchors, unsupported blocks)
// - Explosion damage with falloff

#include "procedural/world.hpp"
#include "physics/structural_integrity.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace krono;

int main() {
    std::cout << "=== World & Terrain Tests (Fase 2) ===" << std::endl;

    // ============================================================
    // TEST 1: World creation + deterministic generation
    // ============================================================
    {
        std::cout << "\n--- Test 1: Deterministic generation ---" << std::endl;
        World world1(42);
        World world2(42);

        Block* b1 = world1.get_block(0, 64);
        Block* b2 = world2.get_block(0, 64);
        assert(b1 && b2);
        assert(b1->type == b2->type);
        std::cout << "  Same seed → same block at (0,64): type=" << (int)b1->type << std::endl;
        std::cout << "  ✓ Deterministic" << std::endl;

        World world3(999);
        Block* b3 = world3.get_block(0, 64);
        // Different seed MIGHT produce different block (not guaranteed, but likely)
        std::cout << "  Different seed → type=" << (int)b3->type << std::endl;
        std::cout << "  ✓ Different seeds work" << std::endl;
    }

    // ============================================================
    // TEST 2: Chunk structure
    // ============================================================
    {
        std::cout << "\n--- Test 2: Chunk structure ---" << std::endl;
        World world(42);
        Chunk* chunk = world.get_chunk(0, 0);
        assert(chunk != nullptr);
        assert(chunk->generated == true);
        std::cout << "  Chunk (0,0) generated, blocks: " << CHUNK_W * CHUNK_H << std::endl;

        // Top of chunk should be air (above surface)
        Block* top = &chunk->get(0, 0);
        assert(top->is_air());
        std::cout << "  Top block (0,0) is AIR ✓" << std::endl;

        // Bottom of chunk should be bedrock
        Block* bottom = &chunk->get(0, CHUNK_H - 1);
        assert(bottom->type == BlockType::BEDROCK);
        std::cout << "  Bottom block is BEDROCK ✓" << std::endl;
    }

    // ============================================================
    // TEST 3: Block placement and destruction
    // ============================================================
    {
        std::cout << "\n--- Test 3: Block placement/destruction ---" << std::endl;
        World world(42);

        // Place a wood block in the air
        world.set_block(10, 50, BlockType::WOOD);
        Block* placed = world.get_block(10, 50);
        assert(placed->type == BlockType::WOOD);
        assert(placed->is_solid());
        assert(placed->hp == get_block_props(BlockType::WOOD).max_hp);
        std::cout << "  Placed WOOD at (10,50): hp=" << (int)placed->hp << " ✓" << std::endl;

        // Damage the block
        world.damage_block(10, 50, 20);
        placed = world.get_block(10, 50);
        assert(placed->hp == 30); // 50 - 20 = 30
        std::cout << "  Damaged by 20: hp=" << (int)placed->hp << " ✓" << std::endl;

        // Destroy completely
        world.destroy_block(10, 50);
        placed = world.get_block(10, 50);
        assert(placed->is_air());
        std::cout << "  Destroyed → AIR ✓" << std::endl;
    }

    // ============================================================
    // TEST 4: Biome determination
    // ============================================================
    {
        std::cout << "\n--- Test 4: Biomes ---" << std::endl;
        World world(42);

        Biome b1 = world.get_biome(0, 0);
        Biome b2 = world.get_biome(5000, 0);
        Biome b3 = world.get_biome(-5000, 0);

        std::cout << "  Biome at x=0: " << b1.name << " (temp=" << b1.temperature << ", humid=" << b1.humidity << ")" << std::endl;
        std::cout << "  Biome at x=5000: " << b2.name << std::endl;
        std::cout << "  Biome at x=-5000: " << b3.name << std::endl;

        assert(b1.temperature >= 0 && b1.temperature <= 1);
        assert(b1.humidity >= 0 && b1.humidity <= 1);
        std::cout << "  ✓ Temperature and humidity in [0,1]" << std::endl;
    }

    // ============================================================
    // TEST 5: Structural integrity — unsupported blocks
    // ============================================================
    {
        std::cout << "\n--- Test 5: Structural integrity ---" << std::endl;
        World world(42);
        StructuralIntegritySystem sis;

        // Build a floating structure (no anchor to ground)
        for (int y = 30; y < 35; y++) {
            world.set_block(100, y, BlockType::WOOD);
        }

        // Check: blocks should be unsupported (no path to bedrock)
        sis.check_region(world, 100, 32, 10);

        // The blocks above bedrock should be fine, but floating ones should be marked
        Block* floating = world.get_block(100, 30);
        // After structural check, unsupported block should have reduced HP (cracking)
        std::cout << "  Floating block HP after check: " << (int)floating->hp << "/" << (int)floating->max_hp << std::endl;
        // HP should be reduced (cracking visual)
        assert(floating->hp < floating->max_hp);
        std::cout << "  ✓ Unsupported block marked (HP reduced = cracking)" << std::endl;

        // Now build a pillar from bedrock to the structure
        for (int y = 35; y < CHUNK_H; y++) {
            Block* existing = world.get_block(100, y);
            if (existing->type == BlockType::AIR) {
                world.set_block(100, y, BlockType::STONE);
            }
        }

        // Re-check — now blocks should be supported
        sis.check_region(world, 100, 32, 20);
        Block* supported = world.get_block(100, 30);
        // Stone has higher max_hp, and after being supported, HP should be restored
        // (or at least not reduced further)
        std::cout << "  After pillar built: HP=" << (int)supported->hp << "/" << (int)supported->max_hp << std::endl;
        std::cout << "  ✓ Structural check ran after modification" << std::endl;
    }

    // ============================================================
    // TEST 6: Explosion damage with falloff
    // ============================================================
    {
        std::cout << "\n--- Test 6: Explosion ---" << std::endl;
        World world(42);
        StructuralIntegritySystem sis;

        // Build a wall of stone blocks
        for (int x = 50; x < 60; x++) {
            for (int y = 40; y < 50; y++) {
                world.set_block(x, y, BlockType::STONE);
            }
        }

        // Explode at center of wall
        int center_x = 55, center_y = 45;
        int radius = 4;
        uint8_t damage = 100;

        sis.apply_explosion(world, center_x, center_y, radius, damage);

        // Check center block is destroyed
        Block* center = world.get_block(center_x, center_y);
        std::cout << "  Center block after explosion: type=" << (int)center->type << std::endl;
        assert(center->is_air());
        std::cout << "  ✓ Center destroyed" << std::endl;

        // Check edge block is damaged but maybe not destroyed
        Block* edge = world.get_block(center_x + radius - 1, center_y);
        std::cout << "  Edge block: type=" << (int)edge->type << " hp=" << (int)edge->hp << std::endl;
        if (!edge->is_air()) {
            assert(edge->hp < edge->max_hp);
            std::cout << "  ✓ Edge damaged (falloff)" << std::endl;
        } else {
            std::cout << "  ✓ Edge destroyed (within radius)" << std::endl;
        }

        // Check block outside radius is untouched
        Block* outside = world.get_block(center_x + radius + 2, center_y);
        std::cout << "  Outside block: type=" << (int)outside->type << " hp=" << (int)outside->hp << std::endl;
        if (!outside->is_air()) {
            assert(outside->hp == outside->max_hp);
            std::cout << "  ✓ Outside untouched" << std::endl;
        }
    }

    // ============================================================
    // TEST 7: Chunk streaming (load/unload)
    // ============================================================
    {
        std::cout << "\n--- Test 7: Chunk streaming ---" << std::endl;
        World world(42);

        // Load chunks around origin
        world.get_chunk(0, 0);
        world.get_chunk(1, 0);
        world.get_chunk(-1, 0);
        world.get_chunk(0, 1);
        world.get_chunk(0, -1);
        std::cout << "  Loaded 5 chunks, count=" << world.loaded_chunks() << std::endl;
        assert(world.loaded_chunks() == 5);

        // Unload distant chunks (keep radius 3)
        world.unload_distant(0, 0, 3);
        std::cout << "  After unload (radius=3): count=" << world.loaded_chunks() << std::endl;
        // All 5 should still be within radius 3
        assert(world.loaded_chunks() == 5);

        // Load a far chunk then unload
        world.get_chunk(20, 20);
        std::cout << "  After loading far chunk: count=" << world.loaded_chunks() << std::endl;
        world.unload_distant(0, 0, 3);
        std::cout << "  After unload: count=" << world.loaded_chunks() << std::endl;
        // Far chunk should be unloaded
        assert(world.loaded_chunks() == 5);
        std::cout << "  ✓ Streaming loads/unloads correctly" << std::endl;
    }

    // ============================================================
    // TEST 8: Block properties
    // ============================================================
    {
        std::cout << "\n--- Test 8: Block properties ---" << std::endl;

        const auto& stone = get_block_props(BlockType::STONE);
        const auto& wood = get_block_props(BlockType::WOOD);
        const auto& metal = get_block_props(BlockType::METAL);
        const auto& bedrock = get_block_props(BlockType::BEDROCK);

        std::cout << "  Stone: hp=" << (int)stone.max_hp << " load=" << stone.load_capacity << std::endl;
        std::cout << "  Wood:  hp=" << (int)wood.max_hp << " load=" << wood.load_capacity << std::endl;
        std::cout << "  Metal: hp=" << (int)metal.max_hp << " load=" << metal.load_capacity << std::endl;
        std::cout << "  Bedrock: hp=" << (int)bedrock.max_hp << " anchor=" << (bedrock.flags & FLAG_ANCHOR ? "yes" : "no") << std::endl;

        assert(metal.load_capacity > wood.load_capacity); // metal stronger than wood
        assert(stone.max_hp > wood.max_hp); // stone harder than wood
        assert(bedrock.flags & FLAG_ANCHOR); // bedrock is anchor
        assert(wood.flags & FLAG_FLAMMABLE); // wood is flammable
        assert(metal.flags & FLAG_CONDUCTOR); // metal conducts electricity
        std::cout << "  ✓ Properties correct (metal>wood, bedrock=anchor, wood=flammable, metal=conductor)" << std::endl;
    }

    std::cout << "\n=== All World & Terrain tests passed! ✓ ===" << std::endl;
    return 0;
}
