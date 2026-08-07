# KronoUniverse

> Sandbox 2D pixel art de exploração espacial multiverso com física real.

## Status: Fase 0 — Fundação técnica (em progresso)

### Stack
- **Linguagem**: C++20
- **Renderização**: SDL2 + OpenGL 3.3
- **ECS**: Próprio (sparse sets, flea-style)
- **Física**: Próprio (corpo rígido 2D, colisão AABB)
- **Procedural**: FastNoiseLite (Perlin/Simplex)
- **Save**: SQLite (deltas por chunk)
- **Build**: CMake 3.20+

### Arquitetura
```
include/
├── engine/         ECS, gameloop, components
├── physics/        Physics system (gravidade, colisão, impulsos)
├── procedural/     Noise, geração de terreno
└── render/         SDL2 + OpenGL renderer
src/
├── main.cpp        Entry point + Fase 0 demo
├── engine/         ECS implementation
├── physics/        Physics implementation
└── procedural/     Noise implementation
tests/              Unit tests (ECS, physics, noise)
third_party/        FastNoiseLite (single-header)
```

### Build
```bash
mkdir build && cd build
cmake ..
make
./kronouniverse        # Run game
./test_ecs             # Run tests
./test_physics
./test_noise
```

### Documentação
- `CONTEXT.md` — contexto permanente do agente (LEIA PRIMEIRO ao retomar)
- `DECISIONS.md` — log de decisões de arquitetura
- `PROMPT_MESTRE.md` — especificação técnica completa

### Fases (Parte 8 do prompt mestre)
- [x] Fase 0 — Fundação técnica (ECS, gameloop, física base, render)
- [ ] Fase 1 — Personagem e movimento
- [ ] Fase 2 — Terreno e geração de planeta
- [ ] Fase 3 — Naves
- [ ] Fase 4 — Inventário e itens
- [ ] Fase 5 — NPCs e diálogo
- [ ] Fase 6 — Geração de universo (Camadas 0-1)
- [ ] Fase 7 — Poderes e stats sociais
- [ ] Fase 8 — Guerra, crime e política
- [ ] Fase 9 — Endgame do Deus
- [ ] Fase 10 — Polimento e build final

### Créditos
Feito por **Darlan (NATSKY)** — [@Kronos1027](https://github.com/Kronos1027)
