# KronoUniverse — Contexto de Desenvolvimento (GLM Agent Memory)

> **ESTE ARQUIVO É O CONTEXTO PERMANENTE DO GLM.**
> Toda decisão, erro, solução e progresso deve ser registrado aqui.
> Ao retomar o trabalho (após restart de sessão), LEIA ESTE ARQUIVO PRIMEIRO.
> Última atualização: 2026-08-08 (correção de auditoria)

---

## 1. RESUMO DO PROJETO

**KronoUniverse** — sandbox 2D pixel art de exploração espacial multiverso.
- Multiverso procedural com bilhões de planetas (3 camadas de LOD)
- Física real (corpo rígido, naves modulares, destruição estrutural)
- Sem telas de carregamento (streaming de terreno contínuo)
- ECS de larga escala (NPCs, itens, blocos, naves, projéteis)
- Meta: derrotar o "Deus" do universo e ascender

## 2. STACK TECNOLÓGICA ESCOLHIDA

| Camada | Tecnologia | Motivo |
|--------|-----------|--------|
| Linguagem | **C++20** | Performance máxima + controle de memória |
| Build | **CMake 3.20+** | Multiplataforma, integra com C/C++ |
| Renderização | **SDL2 + OpenGL 3.3** | Leve, multiplataforma, controle total |
| ECS | **Próprio** (sparse sets) | Nenhuma ECS existente atende streaming + LOD |
| Ruído | **FastNoiseLite** | Perlin/Simplex rápido, MIT |
| Física | **Próprio** (corpo rígido 2D) | Box2D pesado para destruição estrutural |
| Áudio | **SDL_mixer** | Simples, suficiente |
| Save | **SQLite** | Deltas por chunk |
| Serialização | **JSON** (nlohmann/json) | Legeível, debug-friendly |

## 3. ARQUIVOS DE REFERÊNCIA

- `PROMPT_MESTRE.md` — especificação técnica completa
- `DECISIONS.md` — log de decisões de arquitetura (4 ADRs)
- `CONTEXT.md` — ESTE ARQUIVO (contexto permanente)

## 4. PROGRESSO POR FASE (status verificado)

### Fase 0 — Fundação técnica
- **Status**: COMPLETA ✅
- **Data**: 2026-08-07
- **Implementado**:
  - ECS próprio (sparse sets, flea-style) com entity recycling
  - GameLoop fixed timestep 60Hz + interpolação visual
  - PhysicsSystem: gravidade, add_impulse (função central), colisão AABB com restitution
  - Renderer SDL2 + OpenGL 3.3 (immediate mode)
  - FastNoiseLite integrado (src/procedural/noise.c com #define FNL_IMPL)
- **Testes**: 16 (8 ECS + 5 Physics + 3 Noise)
- **Relatório**: Todos passando. INVALID_ENTITY=UINT64_MAX. each() usa template Fn.

### Fase 1 — Personagem e movimento
- **Status**: COMPLETA ✅
- **Data**: 2026-08-07 a 08
- **Implementado**:
  - CharacterController, Species, InventoryWeight, Surface, FallDamageTracker, AnimationState
  - InputSystem (teclado, edge-triggered jump)
  - MovementSystem: andar, correr, pular, nadar, voar, escalar magnético, agachar
  - Salto: v = sqrt(2*g*h) — altura consistente entre gravidades
  - Dano de queda: max(0, (v_impacto - 400) * mult_espécie)
  - Encumbrance: peso reduz speed 50%, jump 70%
  - Atrito: ice=0.05, mud=0.95, metal=0.6, stone=0.7
  - Coyote time (100ms) + jump buffering (100ms)
  - State machine: IDLE, WALK, RUN, JUMP, FALL, SWIM, FLY, CLIMB, CROUCH
- **Testes**: 8 (jump formula, fall damage, encumbrance, friction, walk/run, jump, states, animation)
- **Relatório**: Earth v=280, Moon v=114 → ambos h=80px. Human dmg=20, Dwarf dmg=10.

### Fase 2 — Terreno e geração de planeta
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - World class com chunks 64×128, 14 tipos de bloco, 9 biomas
  - Geração multi-camada: terreno (Perlin FBM), cavernas (ridged), minérios (cellular), árvores
  - Streaming: load/unload por distância
  - StructuralIntegritySystem: BFS from anchors, cracking, explosões com falloff
  - BlockProperties: HP, load_capacity, self_weight, friction, flags
- **Testes**: 16 (8 world + 8 structural)
- **Relatório**: Floating block HP 25/50 (cracked). Explosion center destroyed, edge hp=23. Bedrock immune.

### Fase 3 — Naves
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - ShipController, ShipModuleComp, HullSegment, AtmosphereEntry
  - ShipPhysicsSystem: propulsão, steering, inertia dampener (arcade vs sim)
  - Combustível: consumo = base * throttle² * (1 + carga/capacidade)
  - Reentrada: drag = 0.5 * ρ * v² * Cd * A, calor scales com v²
  - Thermal shield previne dano de calor
  - Out of fuel = sem propulsão
- **Testes**: 8 (thrust, fuel, steering, dampener, no-fuel, drag, heat damage, thermal shield)
- **Relatório**: Full cargo consome 2x fuel. Sim speed > arcade speed. Heat 193 → HP 500→472.

### Fase 4 — Inventário e itens
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - Inventory com stacking, peso/volume, encumbrance ratio
  - 6 raridades (Common→Anomalous) com cores
  - 8 categorias (Material, Tool, Weapon, Armor, Consumable, Tech, Biological, Anomalous)
  - Crafting com Recipe (ingredientes + resultado)
  - 6 poderes de item (FLIGHT, TELEKINESIS, SHIELD, CLOAK, REGENERATE, TIME_DILATION)
- **Testes**: 8 (stacking, weight limits, remove, encumbrance, rarities, crafting, powers, find)
- **Relatório**: 50+60 dirt → 99+11 stacks. Overweight rejected. Crafting: 2 iron + 1 coal → steel.

### Fase 5 — NPCs e diálogo
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - NPCComponent: AI states (IDLE, PATROL, FLEE, FOLLOW, ATTACK, TALK, TRADE)
  - NPCAISystem: transições por proximidade do player, medo/agressão
  - DialogueTree com branching nodes + disposition gates
  - FactionData com relations (-100 a +100), territory, military, tech, population
- **Testes**: 8 (idle, flee, attack, follow, dialogue, factions, disposition, return-to-patrol)
- **Relatório**: NPC fear=80 → FLEE. Aggression=80, range=50 → ATTACK, player HP 100→85. Disposition -50 → 1/2 options visible.

### Fase 6 — Geração de universo (Camadas 0-1)
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - Hash determinístico: hash_seed(seed, galaxy_x, y, z, system, planet)
  - 8 tipos de planeta (Terrestrial, Gas Giant, Ice, Desert, Volcanic, Ocean, Crystal, Artificial)
  - Parâmetros procedural: gravity, atmosphere, temperature, radius, tech_level, population, resources
  - Nomes procedural (silabas: "Helkaron", "Noxvirdralun")
  - Simulação de facções em ticks (tech progression, population growth)
- **Testes**: 8 (deterministic, different planets, valid params, counts, simulation, types, volcanic temp, names)
- **Relatório**: Planet "Helkaron" type=4 g=1.04 temp=200°C. 100 planets → 8/8 unique types. Volcanic temp >100°C.

### Fase 7 — Poderes e stats sociais
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - 3 origens (TECHNOLOGICAL, BIOLOGICAL, ANOMALOUS), 10 tipos de poder
  - SocialStats: hunger, thirst, fatigue, reputation, respect, faith, hatred, fear, notoriety, morality
  - Starvation damage (hunger>80 → health drain)
  - Fatigue modifiers: speed (min 30%), jump (min 30%)
  - Regeneration power heals HP
  - Power toggle + cooldown
- **Testes**: 8 (degrade, starvation, fatigue speed, fatigue jump, toggle, regen, origins, morality)
- **Relatório**: After 10s: hunger=5, thirst=7, fatigue=3. Exhausted speed=0.7. Regen: HP 50→70.

### Fase 8+9 — Guerra, crime, política + Deus
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**:
  - CrimeRecord: crimes com bounty, severity, resolved
  - WarSystem: declare/simulate/peace, at_war check
  - PoliticalSystem: 8 actions (gift, trade, alliance, embargo, war, peace, annex, independence)
  - GodSystem: 100k HP, requisitos de desafio, god_attack=5000, defense=1000, ascensão
- **Testes**: 8 (crime, war declare, war simulation, peace, politics, annex, god challenge, god combat)
- **Relatório**: War 100 vs 50 strength → defender loses after 200 ticks. God: 200 raw - 100 def = 100 actual. God defeated → can_ascend().

### Fase 10 — Polimento final
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- **Implementado**: Validação completa de todas as fases. 83/83 testes passando.

## 5. ERROS ENCONTRADOS E COMO FORAM RESOLVIDOS

### Erro 1: INVALID_ENTITY = 0
- Problema: Primeiro entity (ID=0) era considerado inválido
- Solução: UINT64_MAX

### Erro 2: each() não aceitava lambdas
- Problema: std::function não faz type deduction de lambdas
- Solução: template<typename... Components, typename Fn> void each(Fn fn)

### Erro 3: emplace() usava . em vez de ->
- Solução: get_or_create_set<T>()->insert(...)

### Erro 4: FastNoiseLite define errado
- Problema: Header usa #define FNL_IMPL (não FNL_IMPLEMENTATION)
- Solução: src/procedural/noise.c com #define FNL_IMPL

### Erro 5: fnl_state não inicializada
- Problema: memset não seta defaults
- Solução: fnlCreateState()

### Erro 6: Noise retornava 0
- Problema: Coordenadas (100,200) com freq=0.01 = zero matemático
- Solução: freq=0.1, coords (13.7, 7.3)

### Erro 7: emplace<T>(e) sem valor não compila
- Problema: emplace exige T&& componente
- Solução: emplace<T>(e, T{}) ou emplace<T>(e, std::move(var))

### Erro 8: fall_damage_threshold não existia em CharacterController
- Solução: Hardcoded 400 no MovementSystem

### Erro 9: .gitignore com "test_*" bloqueava tests/*.cpp
- Problema: Padrão glob "test_*" sem / inicial casa com qualquer arquivo começando com "test_"
- Solução: Trocado por binários específicos (/test_ecs, /test_physics, etc.)

### Erro 10: src/procedural/noise.cpp usava macro errada
- Problema: noise.cpp tinha #define FNL_IMPLEMENTATION (errado), noise.c tinha #define FNL_IMPL (correto)
- Solução: Removido noise.cpp. CMakeLists agora inclui noise.c explicitamente no executável principal.

### Erro 11: CMakeLists glob não pegava .c
- Problema: file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.cc") não inclui .c
- Solução: list(APPEND SOURCES src/procedural/noise.c) adicionado explicitamente

## 6. DECISÕES DE ARQUITETURA (ver DECISIONS.md para ADRs)

1. Engine própria C++20 + SDL2 + OpenGL (ADR-001)
2. ECS sparse sets (ADR-002)
3. Fixed timestep 60Hz (ADR-003)
4. Thread pool para geração assíncrona (ADR-004)
5. SQLite para deltas de save

## 7. AMBIENTE DE DESENVOLVIMENTO

- OS: Linux (Debian 13)
- Compilador: GCC 14+ (C++20)
- Build: CMake 3.20+
- Deps: SDL2, SDL2_image, SDL2_mixer, OpenGL, SQLite3, FastNoiseLite
- Caminho: /home/z/my-project/work/kronosuniverse/Kronosuniverse/

## 8. INSTRUÇÕES PARA RETOMAR TRABALHO

1. Leia este arquivo (CONTEXT.md) completamente
2. Leia DECISIONS.md para decisões detalhadas
3. Verifique o progresso da fase atual (seção 4)
4. Rode TODOS os testes: compile cada tests/*.cpp individualmente
5. Continue de onde parou
6. Após cada mudança, atualize este arquivo
7. NUNCA use "test_*" no .gitignore — use "/test_*" (caminho raiz)

## 9. IDENTIDADE GIT

- Nome: Darlan
- Email: 84149831+Kronos1027@users.noreply.github.com
- NUNCA fazer commits com identidade diferente desta
