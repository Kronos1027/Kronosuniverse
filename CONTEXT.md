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
- **Status**: EM PROGRESSO
- **Data início**: 2026-08-07
- **Tarefas**:
  - [x] Criar estrutura de diretórios do projeto
  - [x] Configurar CMakeLists.txt (C++20, SDL2, OpenGL)
  - [x] Implementar ECS base (Entity, Component, System)
  - [x] Implementar loop de jogo (fixed timestep + render interpolation)
  - [x] Implementar função central `aplicar_impulso(corpo, vetor, ponto)`
  - [x] Implementar colisão básica (AABB + resolução)
  - [x] Compilar e testar no Linux
  - [ ] Testar janela SDL2 com render de pixel básico
- **Relatório**: Ver seção 5 abaixo

### Fase 1 — Personagem e movimento
- **Status**: PENDENTE

### Fase 2 — Terreno e geração de planeta
- **Status**: PENDENTE

### Fase 3 — Naves
- **Status**: PENDENTE

### Fase 4+ — (ver PROMPT_MESTRE.md Parte 8)
- **Status**: PENDENTE

## 5. ERROS ENCONTRADOS E COMO FORAM RESOLVIDOS

### Erro 1: (ainda nenhum — projeto começando)

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
