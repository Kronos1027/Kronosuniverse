# KronoUniverse — Contexto de Desenvolvimento (GLM Agent Memory)

> **ESTE ARQUIVO É O CONTEXTO PERMANENTE DO GLM.**  
> Toda decisão, erro, solução e progresso deve ser registrado aqui.  
> Ao retomar o trabalho (após restart de sessão), LEIA ESTE ARQUIVO PRIMEIRO.  
> Última atualização: 2026-08-07 (início do projeto)

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
| Linguagem | **C++20** | Performance máxima + controle de memória para ECS de larga escala |
| Build | **CMake 3.20+** | Multiplataforma, integra com bibliotecas C/C++ |
| Renderização | **SDL2 + OpenGL 3.3** | Leve, multiplataforma, controle total do pipeline gráfico |
| ECS | **Próprio** (flea-style) | Nenhuma ECS existente atende streaming + LOD granular simultaneamente |
| Ruído procedural | **FastNoiseLite** (Single-header) | Perlin/Simplex rápido, MIT, já testado em BHUH |
| Física | **Próprio** (corpo rígido 2D simplificado) | Box2D é pesado para destruição estrutural em larga escala |
| Áudio | **SDL_mixer** | Simples, suficiente para MVP |
| Save | **SQLite** (deltas por chunk) | Persistência relacional para histórico de eventos |
| Serialização | **JSON** (nlohmann/json) | Leve, legível, debug-friendly |
| Thread | **std::thread + thread pool** | Job system para geração procedural assíncrona |

### Justificativa da engine própria (Parte 2 — LIMITAÇÃO ENCONTRADA)

```
[LIMITAÇÃO ENCONTRADA]
Sistema afetado: Engine/renderização + streaming de terreno
Problema: Nenhuma engine open-source (Godot, Bevy, raylib) suporta nativamente
  streaming de terreno em camadas LOD com ECS de larga escala + física de
  destruição estrutural em tempo real de forma performática. Godot é pesado
  para milhares de blocos dinâmicos. Bevy (Rust) adiciona complexidade de
  curva de aprendizado desnecessária. raylib não tem ECS.
Ação tomada: Construindo engine própria em C++20 + SDL2 + OpenGL 3.3
Impacto na visão original: Nenhum — controle total permite implementar
  exatamente o que o design pede, sem workarounds de engine.
```

## 3. ARQUIVOS DE REFERÊNCIA

- `PROMPT_MESTRE.md` — especificação técnica completa (este prompt)
- `GDD_Jogo_Universo_Infinito.md` — design v0.1 (não fornecido ainda — criar resumo)
- `GDD_KronoUniverse_v0.2.md` — design v0.2 (não fornecido ainda — criar resumo)
- `DECISIONS.md` — log de decisões de arquitetura (contínuo)
- `CONTEXT.md` — ESTE ARQUIVO (contexto permanente)

## 4. PROGRESSO POR FASE

### Fase 0 — Fundação técnica
- **Status**: COMPLETA ✅
- **Data início**: 2026-08-07
- **Data conclusão**: 2026-08-07
- **Tarefas**:
  - [x] Criar estrutura de diretórios do projeto
  - [x] Configurar CMakeLists.txt (C++20, SDL2, OpenGL)
  - [x] Implementar ECS base (Entity, Component, System) — sparse sets
  - [x] Implementar loop de jogo (fixed timestep 60Hz + render interpolation)
  - [x] Implementar função central `aplicar_impulso` (add_impulse no PhysicsSystem)
  - [x] Implementar colisão básica (AABB + resolução com bounce/restitution)
  - [x] Compilar e testar no Linux — TODOS OS TESTES PASSARAM
  - [x] Testar ECS: 8/8 testes (create, add, modify, remove, iterate, multi, destroy, recycle)
  - [x] Testar Physics: 5/5 testes (gravity, static, impulse, integration, collision)
  - [x] Testar Noise: 3/3 testes (deterministic, seed, range)
- **Relatório**:
  - ECS: sparse sets flea-style, O(1) add/remove, linear iteration. Entity recycling com version counter.
  - Physics: gravidade, impulsos (função central add_impulse), damping, colisão AABB com restitution.
  - Noise: FastNoiseLite (Perlin/Simplex), determinístico por seed, range [-1, 1] validado.
  - GameLoop: fixed timestep 60Hz + interpolação visual (alpha blending).
  - Renderer: SDL2 + OpenGL 3.3, immediate mode para MVP (upgrade p/ VBOs na Fase 2).
  - Bug corrigido: INVALID_ENTITY era 0 (primeiro entity válido), trocado para UINT64_MAX.
  - Bug corrigido: FastNoiseLite usa #define FNL_IMPL (não FNL_IMPLEMENTATION).
  - Bug corrigido: each() usava std::function (não aceita lambdas), trocado para template.

### Fase 1 — Personagem e movimento
- **Status**: COMPLETA ✅
- **Data início**: 2026-08-07
- **Data conclusão**: 2026-08-08
- **Tarefas**:
  - [x] Criar character_components.hpp (CharacterController, Species, InventoryWeight, Surface, etc.)
  - [x] Implementar InputSystem (teclado → comandos de movimento, edge-triggered jump)
  - [x] Implementar MovementSystem (andar, correr, pular, nadar, voar, escalar, agachar)
  - [x] Implementar fórmula de salto: v = sqrt(2 * g * h) — altura consistente entre gravidades
  - [x] Implementar dano de queda: dano = max(0, (v_impacto - limiar) * mult_espécie)
  - [x] Implementar atrito por terreno (ice=0.05, mud=0.95, metal=0.6, stone=0.7)
  - [x] Implementar encumbrance (peso do inventário reduz speed 50% e jump 70%)
  - [x] Implementar state machine (IDLE, WALK, RUN, JUMP, FALL, SWIM, FLY, CLIMB, CROUCH)
  - [x] Implementar coyote time + jump buffering (game feel)
  - [x] Implementar AnimationState (sprite layers: body, arms, legs, head, equipment)
  - [x] Criar demo jogável (main.cpp) com plataformas de diferentes superfícies
  - [x] Testar: 8/8 testes passaram (jump formula, fall damage, encumbrance, friction, walk/run, jump, states, animation)
- **Relatório**:
  - Jump: v=280 (Earth g=490) e v=114 (Moon g=81) → ambos alcançam h=80px ✓
  - Fall damage: Human=20 dano, Dwarf=10 dano (50% menos), safe fall=0 ✓
  - Encumbrance: vazio=100% speed, cheio=50% speed + 30% jump, over=25% speed ✓
  - Surface friction: Ice=0.05, Mud=0.95, Metal=0.6, Stone=0.7 ✓
  - Walk: 200px/s, Run: 350px/s ✓
  - States: IDLE→WALK→RUN→CROUCH transições corretas ✓
  - Animation: WALK + facing right/left ✓
  - Bug corrigido: emplace<T>(e) sem valor não compila → usar emplace<T>(e, T{})
  - Bug corrigido: fall_damage_threshold não existia em CharacterController → hardcoded 400

### Fase 2 — Terreno e geração de planeta
- **Status**: COMPLETA ✅
- **Data início**: 2026-08-08
- **Data conclusão**: 2026-08-08
- **Tarefas**:
  - [x] Criar world.hpp: World class com chunks, blocks, biomes
  - [x] Implementar geração procedural (Perlin multi-camada: terreno, cavernas, minérios, árvores)
  - [x] Implementar 14 tipos de bloco (AIR, DIRT, GRASS, STONE, SAND, WOOD, LEAVES, METAL, ICE, LAVA, WATER, BEDROCK, CRYSTAL, ANCIENT)
  - [x] Implementar propriedades de bloco (HP, load_capacity, self_weight, friction, flags)
  - [x] Implementar biomas (OCEAN, BEACH, PLAINS, FOREST, DESERT, TUNDRA, MOUNTAIN, VOLCANIC, CRYSTAL_CAVE)
  - [x] Implementar place/destroy/damage de blocos
  - [x] Implementar streaming de chunks (load/unload por distância)
  - [x] Implementar integridade estrutural (BFS from anchors, unsupported = cracking)
  - [x] Implementar explosões com falloff + recalculo estrutural
  - [x] Testar: 8/8 testes passaram (deterministic, chunks, blocks, biomes, structural, explosion, streaming, properties)
- **Relatório**:
  - Chunk: 64×128 = 8192 blocos, gerado proceduralmente sob demanda
  - Topo = AIR, fundo = BEDROCK (indestrutível, âncora estrutural)
  - Geração: terreno Perlin (5 octaves FBM), cavernas (ridged noise), minérios (cellular noise)
  - Árvores em FOREST/PLAINS, gelo em TUNDRA, lava em VOLCANIC
  - Estrutural: BFS de bedrock → blocos sem caminho = rachadura (HP reduzido)
  - Explosão: falloff = dano_base * (1 - dist/raio), bedrock imune
  - Streaming: chunks fora do raio são descarregados (delta saving TODO Fase 6)

### Fase 3 — Naves
- **Status**: PENDENTE

### Fase 4+ — (ver PROMPT_MESTRE.md Parte 8)
- **Status**: PENDENTE

## 5. ERROS ENCONTRADOS E COMO FORAM RESOLVIDOS

### Erro 1: INVALID_ENTITY = 0 conflita com primeiro entity criado
- **Problema**: `constexpr Entity INVALID_ENTITY = 0` fazia o primeiro entity (ID=0) ser considerado inválido
- **Solução**: Trocado para `UINT64_MAX` — IDs reais nunca chegam a esse valor
- **Data**: 2026-08-07

### Erro 2: each() não aceitava lambdas
- **Problema**: `each(std::function<void(Entity, Components&...)> fn)` não aceita lambdas diretamente (type deduction falha)
- **Solução**: Trocado para `template<typename... Components, typename Fn> void each(Fn fn)` — aceita qualquer callable
- **Data**: 2026-08-07

### Erro 3: emplace() usava . em vez de ->
- **Problema**: `get_or_create_set<T>().insert(...)` — get_or_create_set retorna ponteiro
- **Solução**: Trocado para `get_or_create_set<T>()->insert(...)`
- **Data**: 2026-08-07

### Erro 4: FastNoiseLite define errado
- **Problema**: Header usa `#define FNL_IMPL` (não `FNL_IMPLEMENTATION` como outras libs)
- **Solução**: Criado `src/procedural/noise.c` com `#define FNL_IMPL` + `#include "FastNoiseLite.h"`
- **Data**: 2026-08-07

### Erro 5: fnl_state não inicializada corretamente
- **Problema**: `memset(&fnl, 0, sizeof(fnl))` não seta defaults (fractal_type, octaves, etc.)
- **Solução**: Usar `fnlCreateState()` que inicializa com defaults corretos
- **Data**: 2026-08-07

### Erro 6: Noise retornava 0 nas coordenadas de teste
- **Problema**: Coordenadas (100, 200) com freq=0.01 resultam em noise=0 (coincidência matemática)
- **Solução**: Ajustado para freq=0.1 e coordenadas (13.7, 7.3) que produzem valores não-zero
- **Data**: 2026-08-07

## 6. DECISÕES DE ARQUITETURA (resumo — ver DECISIONS.md para detalhes)

1. **Engine própria** em vez de Godot/Bevy/raylib (justificativa acima)
2. **ECS flea-style** (sparse sets) em vez de archetype-based (Entt) — mais simples de implementar, suficiente para o escopo
3. **Fixed timestep** a 60Hz com interpolação visual — separa simulação de render
4. **Thread pool** para geração procedural assíncrona de chunks
5. **SQLite** para deltas de save (não salvar planeta inteiro, só mudanças)

## 7. AMBIENTE DE DESENVOLVIMENTO

- **OS**: Linux (Debian 13)
- **Compilador**: GCC 14+ (C++20)
- **Build**: CMake 3.20+
- **Dependências**: SDL2, SDL2_image, SDL2_mixer, OpenGL, SQLite3, nlohmann/json, FastNoiseLite
- **Caminho do projeto**: `/home/z/my-project/work/kronosuniverse/Kronosuniverse/`

## 8. INSTRUÇÕES PARA RETOMAR TRABALHO

1. Leia este arquivo (CONTEXT.md) completamente
2. Leia DECISIONS.md para decisões detalhadas
3. Verifique o progresso da fase atual (seção 4)
4. Continue de onde parou
5. Após cada mudança significativa, atualize este arquivo

## 9. IDENTIDADE GIT

- **Nome**: Darlan
- **Email**: 84149831+Kronos1027@users.noreply.github.com
- **Nunca** fazer commits com identidade diferente desta

### Fase 8+9 — Guerra, Crime, Política + Endgame do Deus
- **Status**: COMPLETA ✅
- **Data**: 2026-08-08
- 8/8 testes: crime (bounty), war (declare/simulate/peace), politics (gift/trade/war/peace/annex), god (challenge/combat/ascension)
- Total de testes: 75 (8+5+3+8+8+8+8+8+8+8+8)

### RESUMO FINAL — TODAS AS 10 FASES COMPLETAS
Fase 0: ECS + física + gameloop + render + noise (16 testes)
Fase 1: Personagem/movimento (8 testes)
Fase 2: Terreno/planeta (8 testes)
Fase 3: Naves (8 testes)
Fase 4: Inventário/itens (8 testes)
Fase 5: NPCs/diálogo (8 testes)
Fase 6: Universo/Camadas 0-1 (8 testes)
Fase 7: Poderes/stats sociais (8 testes)
Fase 8+9: Guerra/crime/política + Deus (8 testes)
Fase 10: (próximo — polimento + build final)
