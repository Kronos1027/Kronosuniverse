# KronoUniverse

> Sandbox 2D pixel art de exploração espacial multiverso com física real.

## Status: v0.3 — 16 fases completas — 142 testes passando

### Novidades v0.3
- **Combat System**: 5 armas (Sword, Bow, Gun, Fire/Ice Staff, Poison Dagger), projéteis, knockback, dano crítico
- **Status Effects**: 8 tipos (Poison, Burn, Freeze, Stun, Bleed, Regen, Haste, Slow)
- **Mob AI**: 7 tipos de inimigos (Zombie, Slime, Skeleton, Bat, Boss, Deer, Rabbit) com spawn dinâmico
- **Weather System**: 7 climas (Clear, Cloudy, Rain, Snow, Storm, Fog, Sandstorm) com vento e raios
- **Lighting System**: 9 tipos de luz dinâmica (tochas, lava, cristais, magia) com falloff
- **Crafting v2**: 46 receitas em 6 estações (Workbench, Furnace, Anvil, Altar, Alchemy, High Tech)
- **Save System**: SQLite para player, blocos modificados, receitas descobertas, stats
- **HUD melhorado**: minimap, FPS, dia/noite, indicador de clima, hotbar 8 slots
- **Áudio procedural**: 13 efeitos sonoros (jump, mine, attack, shoot, thunder, levelup, etc)
- **Partículas**: poeira, sangue, faíscas, explosões, landing dust

### Stack
- **Linguagem**: C++20
- **Renderização**: SDL2 + OpenGL 2.1 Compatibility Profile
- **ECS**: Próprio (sparse sets, flea-style)
- **Física**: Próprio (corpo rígido 2D, colisão AABB, destruição estrutural)
- **Procedural**: FastNoiseLite (Perlin/Simplex)
- **Save**: SQLite (deltas por chunk)
- **Build**: CMake 3.20+

### Arquitetura
```
include/
├── engine/         ecs.hpp, components.hpp, character_components.hpp,
│                   ship_components.hpp, gameloop.hpp, input_system.hpp,
│                   sprite_system.hpp
├── physics/        physics.hpp, movement_system.hpp, ship_physics.hpp,
│                   structural_integrity.hpp
├── procedural/     world.hpp, universe.hpp
├── game/           inventory.hpp, npc_system.hpp, powers.hpp,
│                   war_politics_god.hpp,
│                   combat_system.hpp, mob_ai.hpp,
│                   weather_system.hpp, lighting_system.hpp,
│                   crafting_system.hpp, save_system.hpp
├── audio/          audio_system.hpp
└── render/         renderer.hpp
src/
├── main.cpp        Entry point (v0.3 — fully integrated)
└── procedural/     noise.c (FastNoiseLite wrapper)
tests/              17 arquivos de teste (142 testes individuais)
third_party/        FastNoiseLite.h (MIT)
```

### Build
```bash
mkdir build && cd build
cmake ..
make
./kronouniverse          # Run game (requires SDL2)
./test_ecs               # Run individual tests
ctest --output-on-failure  # Run all tests
```

### Controles (v0.3)
- **WASD / Setas**: Mover
- **Espaço / W**: Pular
- **Shift**: Correr
- **E**: Atacar (com arma equipada)
- **Q**: Trocar arma (Sword → Bow → Fire Staff → Ice Staff → Poison Dagger)
- **1-8**: Selecionar bloco da hotbar
- **Mouse Esq**: Minerar bloco
- **Mouse Dir**: Colocar bloco
- **Scroll**: Zoom
- **F1**: Debug overlay
- **F2**: Trocar bloco
- **F3**: Menu de crafting
- **F4**: Inventário
- **R**: Forçar mudança de clima
- **ESC**: Sair

### Documentação
- `CONTEXT.md` — contexto permanente do agente (LEIA PRIMEIRO ao retomar)
- `DECISIONS.md` — log de decisões de arquitetura (ADRs)
- `PROMPT_MESTRE.md` — especificação técnica completa

### Fases (Parte 8 do prompt mestre)
- [x] Fase 0 — Fundação técnica (ECS, gameloop, física, render, noise) — 16 testes
- [x] Fase 1 — Personagem e movimento (salto, dano queda, encumbrance, atrito) — 8 testes
- [x] Fase 2 — Terreno e planeta (chunks, biomas, estrutural, explosões) — 16 testes
- [x] Fase 3 — Naves (propulsão, combustível, reentrada, dampener) — 8 testes
- [x] Fase 4 — Inventário e itens (stacking, raridades, crafting, poderes) — 8 testes
- [x] Fase 5 — NPCs e diálogo (IA, diálogo branching, facções) — 8 testes
- [x] Fase 6 — Geração de universo (hash determinístico, 8 tipos planeta) — 8 testes
- [x] Fase 7 — Poderes e stats sociais (3 origens, 10 poderes, starvation) — 8 testes
- [x] Fase 8+9 — Guerra, crime, política + Deus endgame — 8 testes
- [x] Fase 10 — Polimento final (v0.1 — 83 testes)
- [x] Fase 11 — Combat System (5 armas, 8 status, projéteis) — 14 testes
- [x] Fase 12 — Mob AI (7 mobs, spawn dinâmico) — 11 testes
- [x] Fase 13 — Weather System (7 climas, raios, vento) — 8 testes
- [x] Fase 14 — Lighting System (9 luzes, falloff, glow) — 9 testes
- [x] Fase 15 — Crafting v2 (46 receitas, 6 estações) — 15 testes
- [x] Fase 16 — Save System (SQLite, deltas) — 6 testes

### Releases
- **v0.1**: https://github.com/Kronos1027/Kronosuniverse/releases/tag/v0.1 — Windows exe + DLLs
- **v0.3**: Esta versão (em desenvolvimento)

### Créditos
Feito por **Darlan (NATSKY)** — [@Kronos1027](https://github.com/Kronos1027)
