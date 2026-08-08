# PROMPT MESTRE DE DESENVOLVIMENTO — KRONOUNIVERSE

> Documento de especificação técnica completa.  
> Este arquivo é a fonte de verdade para o escopo do projeto.  
> NÃO modificar sem reportar (Parte 2 — Protocolo de Limitações).

---

## PARTE 1 — PAPEL E MANDATO

O arquiteto-chefe é responsável pelo ciclo completo: arquitetura de engine,
programação de todos os sistemas, geração de assets, testes, debug,
otimização e produção até build final jogável.

Autoridade total de engenharia: escolher/construir engine, definir stack,
criar ferramentas internas. NUNCA simplificar sem reportar.

## PARTE 2 — PROTOCOLO DE LIMITAÇÕES

Antes de simplificar qualquer sistema:
```
[LIMITAÇÃO ENCONTRADA]
Sistema afetado: <nome>
Problema: <descrição técnica>
Ação tomada: <solução>
Impacto: <o que muda para o jogador>
```
Regras: nunca simplificar silenciosamente. Construir ferramentas se necessário.
Documentar em DECISIONS.md. Testar antes de declarar fase concluída.

## PARTE 3 — VISÃO GERAL

Sandbox 2D pixel art com física real, multiverso procedural com bilhões de
planetas em camadas de detalhe. Criar personagem, pilotar naves físicas sem
loading entre espaço/superfície, escavar até núcleo, interagir com facções
que evoluem independentemente, inventário quase infinito, derrotar o Deus.

## PARTE 4 — ARQUITETURA TÉCNICA

### Requisitos não-negociáveis
- 60fps em GPU integrada
- 2D pixel art com iluminação dinâmica em camadas
- Sem telas de carregamento (streaming de terreno)
- Determinístico por seed
- ECS (Entity Component System)

### Decisão: Engine própria (C++20 + SDL2 + OpenGL 3.3)
Ver DECISIONS.md ADR-001 para justificativa.

### ECS sugerido
```
Entity: ID
Components: Position, Velocity, Mass, Sprite, Health, Inventory, ...
Systems: PhysicsSystem, RenderSystem, AISystem, ProceduralGenSystem, ...
```
LOD: NPCs "abstratos" longe do jogador não têm components pesados ativos.

## PARTE 5 — FÍSICA (especificação detalhada)

### 5.1 Movimento do Personagem
- Corpo rígido 2D (cápsula), gravidade por planeta
- Massa afetada por inventário (peso reduz aceleração e salto)
- Atrito por terreno (gelo, lama, metal)
- Velocidade terminal de queda (dano por velocidade de impacto, não altura)
- Estados: andar, correr, nadar (flutuação por densidade), voar, escalar magnético
- Salto: `v = sqrt(2 * g * h)` — altura consistente entre gravidades
- Dano de queda: `dano = max(0, (v_impacto - limiar) * mult_espécie)`

### 5.2 Naves
- Centro de massa dinâmico (recalculado a cada modificação de módulo)
- Propulsão: cada motor aplica vetor de força na sua posição
- Amortecedor de inércia (damping coefficient, modo arcade vs simulação)
- Reentrada atmosférica: arrasto + calor de fricção + dano sem escudo térmico
- Combustível: `consumo = base * throttle² * (1 + carga/capacidade)`
- Colisão: HP por segmento (destruição parcial)

### 5.3 Estruturas e Blocos
- Grafo de suporte estrutural (BFS/DFS a cada alteração)
- Carga por material (metal > madeira)
- Destruição em combate recalcula grafo — tática "destruir pilar central"

### 5.4 Combate e Projéteis
- Projéteis reais (não hitscan) com gravidade local
- Dano por zona de impacto (cabeça, tronco, membros)
- Explosões com falloff + força de impulso física

### 5.5 Planetária e Ambiental
- Gravidade por planeta (procedural, afeta 5.1 e 5.2)
- Atmosfera com curva de densidade (arrasto + respiração)
- Clima por bioma + estação: vento, visibilidade, dano ambiental

### 5.6 Energia
- Grafo de rede elétrica (geradores + consumidores + condutores)
- Sobrecarga: contador de instabilidade → eventos físicos (terremoto)

### 5.7 Poderes com Componente Físico
- Voo: força vertical contínua, consome energia
- Telecinesia: aplica força a objetos (mesma função de impulso)
- Função central: `aplicar_impulso(corpo, vetor, ponto)` usada por tudo

## PARTE 6 — SISTEMAS DE GAMEPLAY

Implementar integralmente (usando física da Parte 5):
- Inventário, itens, raridades, crafting (peso/volume afeta física)
- Poderes de 3 origens (custo de energia + componente físico)
- Criação de personagem e modos de início
- Espécies com vantagens/desvantagens (parâmetros físicos reais)
- Facções, política, guerras, crimes, leis
- Stats sociais: fome, reputação, respeito, fé, ódio, medo
- Sistema do Deus e endgame de ascensão

## PARTE 7 — GERAÇÃO PROCEDURAL

### 7.1 Camada 0 — Seed Matemática
Hash determinístico da seed + coordenada → parâmetros base.
Custo zero, calculado sob demanda.

### 7.2 Camada 1 — Simulação Abstrata
Para planetas no raio de relevância: simulação de facções em ticks abstratos.
Atualiza relações, eventos, guerras, território.

### 7.3 Camada 2 — Geração de Detalhe Real (streaming)
1. Orbital: planeta visível como sprite de baixa resolução
2. Atmosfera: terreno em anéis concêntricos (Perlin/Simplex multi-camada)
3. NPCs e estruturas populam terreno a partir da Camada 1

### 7.4 Persistência
Salvar apenas deltas (mudanças do jogador) por chunk.
Eventos históricos salvos como "linha do tempo" do universo.

## PARTE 8 — METODOLOGIA

Fatias verticais funcionais, cada uma testada antes de avançar:

1. Fase 0 — Fundação técnica (ECS, gameloop, física, render)
2. Fase 1 — Personagem e movimento (5.1)
3. Fase 2 — Terreno e geração de planeta (5.3, 7.3)
4. Fase 3 — Naves (5.2, transição atmosfera-superfície)
5. Fase 4 — Inventário e itens
6. Fase 5 — NPCs e diálogo
7. Fase 6 — Geração de universo (Camadas 0-1)
8. Fase 7 — Poderes e stats sociais
9. Fase 8 — Guerra, crime e política
10. Fase 9 — Endgame do Deus
11. Fase 10 — Polimento e build final

Relatório ao final de cada fase: implementado, testado, bugs, limitações.

## PARTE 9 — INSTRUÇÃO FINAL

Tratar como especificação não-negociável. Flexibilidade apenas em COMO
implementar, nunca em QUANTO cortar sem reportar. Começar pela Fase 0.

---

Feito por Darlan (NATSKY) — github.com/Kronos1027
