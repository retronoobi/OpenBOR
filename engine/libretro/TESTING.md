# Validação — revisões r2 a r4 (4–5 de outubro de 2026)

Windows x64, Release, OpenBOR 4.0 build 7533, revisão 7533-libretro-r2.

## Problemas reproduzidos e correções

No X-Men Arcade, a versão anterior parava de avançar na primeira fase:
`wait=1`, spawn 42/101, Sent2 com vida negativa e animação FALL em loop.
A conversão de `nodieblink 2` substituía a configuração `falldie 1`. A r2
preserva a política de queda/morte explicitamente selecionada por `falldie`,
permitindo que os inimigos sejam removidos e o avanço seja liberado.

No mixer, amostras de efeitos de 8 bits sem sinal eram centralizadas depois
do ganho. Isso transformava silêncio em deslocamento DC dependente do volume,
provocando transientes e saturação. A r2 centraliza a amostra antes do ganho.

## Regressões automatizadas

CTest executa diretamente o código do motor:

- `mixer_regression`: cinco valores de amostra em quatro volumes, incluindo
  silêncio e volume zero; verifica ganho e polaridade.
- `death_regression`: falldie 1/2 com nodieblink 0/1/2/3 nas duas ordens de
  declaração; verifica preservação da política de morte e queda.

Ambos passaram. Comando: `ctest --test-dir engine/libretro/build-win64 --output-on-failure`.

## Partidas com entrada simulada

Frontend mínimo baseado no cabeçalho libretro 1.7.5, com diagnóstico ativado
por `OPENBOR_LIBRETRO_TRACE` e movimentos via `OPENBOR_COMBAT_SWEEP`.
Cada jogo executou 72.000 quadros (20 minutos de tempo emulado), com saída 0.

- X-Men Arcade: passou do bloqueio, completou `konami1.txt` e carregou
  `data/levels/konami2a.txt`. 72.000 quadros de vídeo, 71.611 com áudio não nulo.
- Retro Gamer Adventure 1.0.3: completou `rua0.txt` e carregou
  `data/levels/fliperama.txt`. 72.000 quadros de vídeo, 71.394 com áudio não nulo.

Evidências locais em `engine/libretro/test-run/xmen-sweep` e `rga-sweep`.
As execuções também verificam recusa de conteúdo nulo, ausente ou truncado
 e descarregamento antes do primeiro quadro. Os PAKs não foram alterados.

## Integração e limites

A versão inicial foi validada no RetroArch 1.7.5 x64 da instalação local do
EmuVR (commit 826c77d523), com caminho contendo espaços/acentos fora de Paks,
arquivos externos conflitantes, configuração isolada, vídeo OpenGL e áudio null.

Os testes de PCM e mixer não substituem a avaliação auditiva no EmuVR.
Não foram completados os jogos nem validados controles físicos, quatro jogadores
simultâneos ou a execução na sala virtual. O ponto exato relatado pelo usuário
no Retro Gamer Adventure não foi confirmado; a automação passou da primeira fase.
Save states, rewind, netplay libretro, rumble e WebM não são implementados.

## DLL final r2

Após identificar a revisão como r2, os dois testes CTest passaram novamente.
O pacote final também passou por 6.000 quadros, descarga, recarga, reset e
mais 6.000 quadros usando o PAK em caminho com espaços e acentos fora de Paks.
Resultado: `frames=6000 video=12001 audible=11058 polls=12001 shutdown=0`, saída 0.
Log: `engine/libretro/test-run/r2-final/smoke.log`.
A identificação de versão foi a única mudança no motor após os testes longos.
O SHA-256 da DLL atualmente distribuída está em `SHA256SUMS.txt`.

## Revisão r3 — defesa presa (4 de outubro de 2026)

A imagem fornecida corresponde à animação BLOCK da Dazzler (`blo1.gif` e
sequência), que usa `loop 1` e `holdblock 1` no PAK original.
Reprodução usando seleção e botões do frontend, sem alterar o PAK:
`OPENBOR_TEST_BLOCK=1` seleciona Dazzler, segura defesa nos quadros 2400–2699,
solta todos os botões até 3300 e depois retoma movimento/ataque.

Na r2, o registro do quadro 3000 ainda mostra `anim=14` (BLOCK), 300 quadros
após soltar o botão. Um golpe posterior pode interromper esse estado; isso
não torna a saída normal da defesa funcional. Com a correção, o mesmo teste
sai de BLOCK e a personagem volta a reagir e se movimentar.
Evidências locais: `test-run/block-r2` e `test-run/block-fixed`.

Causa: `inpain & ~IN_PAIN_BLOCK` não reconhecia o estado sem dor (zero).
A condição agora testa a ausência do bit de blockstun. Animações de defesa
em loop não precisam terminar para liberar o personagem. A animação de
reação a um golpe bloqueado continua sendo respeitada.

O teste `block_regression` compila a função real `common_block` extraída
na configuração do CMake, usando os tipos reais do motor e substitutos apenas
para os efeitos das chamadas de animação. Verifica manter defesa enquanto
pressionada, soltar defesa em loop, esperar blockstun, completar animação de
saída sem reiniciá-la e preservar defesa sem holdblock.

Os três testes CTest passaram. As correções da r2 estão incluídas.
Não foi reproduzida a luta específica contra Juggernaut; o bloqueio foi
reproduzido com a mesma personagem já na primeira fase.

A DLL final identificada como `7533-libretro-r3` passou novamente nos três
CTest e em 6.000 quadros, descarga, recarga, reset e mais 6.000 quadros com
X-Men/Dazzler e a sequência de defesa. Resultado: saída 0,
`frames=6000 video=12001 audible=11276 polls=12001 shutdown=0`.
Log: `test-run/r3-final/smoke.log`.

## Revisão r4 — seleção e troca de personagens (5 de outubro de 2026)

PAKs lidos diretamente de `C:/EmuVR/Games/openbor`, sem alterações e com saves
isolados nos diretórios de teste do projeto.

### Dungeons & Dragons

Reproduzido na r3: usando repetidamente Ataque 4, Kids/Hank muda para Bobby,
mas as próximas trocas continuam em Bobby. `dropweapon(2)` aplicava o modelo
de perda de arma antes da troca explícita, voltando à lista do personagem
original. O índice zero vindo de `weaploss 3` acionava esse comportamento.

A r4 só aplica o modelo de perda durante uma perda real, preservando a lista
do modelo atual nas trocas explícitas. Testes de 9.000 quadros percorreram:

- Ataque 4: Kids/Hank → Bobby → Sheila → Presto → Diana → Eric → Kids/Hank.
- Ataque 3: Kids/Hank → Eric → Diana → Presto → Sheila → Bobby → Kids/Hank.

Os ciclos se repetiram. Logs: `test-run/dnd-r3`, `test-run/dnd-fixed` e
`test-run/r4-dnd-reverse`. Não foram testados todos os grupos/modos do PAK.

### Jaspion The Game

O arquivo `data/levels.txt` começa com cenas e depois define `skipselect` e
`select data/levels/select.txt`. Esse seletor restringe a lista com
`allowselect jaspion`. A seleção genérica inserida antes das cenas não aplicava
essa restrição; não era evidência de desbloqueio legítimo de personagens.

Para jogos novos, o motor consulta a primeira entrada após as cenas para
respeitar o seletor explícito, mantendo as cenas na ordem original. A alteração
não faz essa busca ao carregar um save existente.

Capturas da r4 confirmam introdução antes da seleção. Após vários comandos
para a direita, a seleção permaneceu em Jaspion. O teste também entrou na
primeira fase. Evidência: `test-run/r4-jaspion-select/sequence.png`.
Os botões simulados podem pular cenas; não foi verificada a duração integral
nem o conteúdo de cada cena ou os modos posteriores.

### Regressão e pacote final

Cinco testes CTest passaram: áudio, morte, defesa, ordem de seleção e perda/troca
de armas. Os dois novos testes usam o código real do motor; o teste de armas
substitui apenas efeitos externos para observar as chamadas de troca.

DLL final `7533-libretro-r4`:

- Dungeons & Dragons: 9.000 quadros, ciclo inverso completo.
- Jaspion: 3.600 quadros, introdução, seleção restrita e entrada na fase.
- X-Men/Dazzler: 6.000 quadros, recarga/reset e mais 6.000; teste de defesa.
- Retro Gamer Adventure: 6.000 quadros, PAK em caminho com espaços e acentos.

Todos encerraram com código 0 e sem pedido inesperado de shutdown.
A r4 não foi novamente executada na sala virtual do EmuVR. Os testes desta
revisão usaram o frontend mínimo da API 1.7.5. Não foram completados os jogos.

## Nome e organização do projeto (5 de outubro de 2026)

O nome exposto pela API e pelo arquivo `.info` passou a ser `OpenBOR`.
A identificação técnica `7533-libretro-r4` permanece para identificar o motor.
Os fontes compilados estão listados explicitamente em `Sources.cmake`; ports,
SDKs, ferramentas externas e diagnósticos foram retirados da árvore ativa.
O frontend mínimo está em `engine/libretro/tests/smoke.c` e agora é compilado
pelo CMake junto dos testes.

Compilação local do zero: passou, com cinco testes CTest aprovados.
Smoke com X-Men: 2.400 quadros, vídeo/áudio presentes, saída 0 e nome `OpenBOR`.
O workflow GitHub Actions foi preparado, mas não foi executado remotamente
nesta sessão. Ele chama o mesmo `build.ps1` usado na validação local.
