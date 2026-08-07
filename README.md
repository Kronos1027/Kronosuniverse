# KronoUniverse

> Sandbox 2D pixel art de exploração espacial multiverso com física real.

## Status: 10 Fases completas — 83 testes passando

### Stack
- **Linguagem**: C++20
- **Renderização**: SDL2 + OpenGL 3.3
- **ECS**: Próprio (sparse sets, flea-style)
- **Física**: Próprio (corpo rígido 2D, colisão AABB, destruição estrutural)
- **Procedural**: FastNoiseLite (Perlin/Simplex)
- **Save**: SQLite (deltas por chunk)
- **Build**: CMake 3.20+

### Arquitetura
```
include/
├── engine/         ecs.hpp, components.hpp, character_components.hpp,
│                   ship_components.hpp, gameloop.hpp, input_system.hpp
├── physics/        physics.hpp, movement_system.hpp, ship_physics.hpp,
│                   structural_integrity.hpp
├── procedural/     world.hpp, universe.hpp
├── game/           inventory.hpp, npc_system.hpp, powers.hpp,
│                    war_politics_god.hpp
└── render/         renderer.hpp
src/
├── main.cpp        Entry point + demo jogável
└── procedural/     noise.c (FastNoiseLite wrapper)
tests/              12 arquivos de teste (83 testes individuais)
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
- [x] Fase 10 — Polimento final (validação completa) — 83/83 testes

### Créditos
Feito por **Darlan (NATSKY)** — [@Kronos1027](https://github.com/Kronos1027)
