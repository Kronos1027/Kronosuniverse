# DECISIONS.md — Log de Decisões de Arquitetura do KronoUniverse

> Formato: ADR (Architecture Decision Record)  
> Cada decisão recebe um número e é imutável após publicação.

---

## ADR-001: Engine própria em C++20 + SDL2 + OpenGL

**Data**: 2026-08-07  
**Status**: Aceito

### Contexto
O prompt mestre (Parte 4.2) pede avaliação entre engine existente adaptada vs engine própria. Os requisitos não-negociáveis são:
- Streaming de terreno em camadas LOD (Parte 7.3)
- ECS de larga escala (milhares de entidades)
- Física customizada (destruição estrutural, centro de massa dinâmico)
- 60fps em GPU integrada

### Avaliação
| Engine | Prós | Contras |
|--------|------|---------|
| Godot 4 | GDExtension C++, editor visual | Pesado para milhares de blocos dinâmicos, overhead de nodes |
| Bevy (Rust) | ECS nativo moderno | Curva de aprendizado Rust, ecossistema jovem |
| raylib | Simples, leve | Sem ECS, sem sistema de streaming |
| **Própria (C++20 + SDL2)** | **Controle total, leve, sem overhead** | **Mais trabalho inicial** |

### Decisão
Construir engine própria em C++20 + SDL2 + OpenGL 3.3.

### Consequências
- + Controle total sobre pipeline de streaming/geração
- + Performance otimizada para o caso de uso específico
- + Sem dependência de roadmap de terceiros
- - Mais código para escrever e manter
- - Sem editor visual (criar ferramentas próprias quando necessário)

---

## ADR-002: ECS flea-style (sparse sets) em vez de archetype-based

**Data**: 2026-08-07  
**Status**: Aceito

### Contexto
ECS archetype-based (estilo Entt/Bevy) é mais rápido para iteração em cache, mas complexo de implementar. Sparse sets (flea-style) são mais simples e suficientes.

### Decisão
Implementar ECS com sparse sets: cada component type tem um array denso de dados + um array esparso de entity IDs. Iteração é linear no array denso.

### Consequências
- + Implementação simples (~200 linhas)
- + Add/remove de component é O(1)
- + Suficiente para o volume de entidades do jogo
- - Iteração menos cache-friendly que archetype (aceitável para MVP)

---

## ADR-003: Fixed timestep 60Hz com interpolação visual

**Data**: 2026-08-07  
**Status**: Aceito

### Decisão
Loop de jogo com timestep fixo a 60Hz (16.67ms) para simulação física, com interpolação linear entre o estado anterior e atual para renderização a qualquer framerate.

### Consequências
- + Simulação determinística (reproduzível)
- + Física estável independente de framerate
- + Pode rodar simulação em thread separada no futuro
- - Pequeno overhead de interpolação na render

---

## ADR-004: Thread pool para geração procedural assíncrona

**Data**: 2026-08-07  
**Status**: Aceito

### Decisão
Usar thread pool (std::thread + queue de jobs) para gerar chunks de terreno em background, evitando travar o frame principal.

### Consequências
- + Sem hitching quando novos chunks carregam
- + Pode pré-gerar chunks na direção do movimento do jogador
- - Sincronização necessária (mutex nos resultados)
