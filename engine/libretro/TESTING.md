# Validação — revisões r2 a r5 (4–5 de outubro de 2026)

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
Na r2, save states, rewind, netplay libretro, rumble e WebM não eram implementados.
WebM foi acrescentado na r5; os demais recursos continuam indisponíveis.

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

## Revisão r5 — WebM (5 de outubro de 2026)

Player libretro síncrono com nestegg, libvpx e libvorbis. Não depende do relógio
real nem do mixer SDL: entrega vídeo XRGB8888 e áudio estéreo pelo frontend.
O áudio é convertido para a frequência de saída do core e limitado a 16 bits;
100% de volume de música preserva o ganho original do vídeo.

Os seis WebMs do PAK X-Men foram decodificados até o fim, em 1920×1080,
VP8/Vorbis estéreo 48 kHz. Todos retornaram sucesso:

| Arquivo | Quadros | Duração reproduzida |
| --- | ---: | ---: |
| xintro | 2118 | 35,317 s |
| xko1 | 1205 | 20,100 s |
| xko2 | 1025 | 17,100 s |
| xko2a | 281 | 4,717 s |
| xko3 | 997 | 16,633 s |
| xko4 | 2134 | 35,600 s |

O frontend mínimo executou a introdução completa diretamente do PAK (2.500
chamadas de `retro_run`). Em outra execução, pulou cenas, voltou de 1920×1080
para 396×200 e entrou na fase, incluindo recarga/reset (1.800 + 1.800 quadros).
A DLL r5 final passou no descarregamento durante o vídeo e reabertura
(500 + 500 quadros), sem shutdown inesperado. Captura visual de um quadro da
introdução conferida; evidências locais em `test-run/webm`.

CTest: seis testes aprovados. O novo `webm_regression` usa mídias sintéticas
incluídas nos fontes e cobre cores, duração/EOF, estéreo, mono, conversão de
frequência, ganho de 50%, vídeo sem áudio, skip/noskip, arquivo ausente/inválido
e limpeza ao interromper a reprodução. Não depende de PAKs nem de FFmpeg no CI.

As importações da DLL do motor continuam limitadas a bibliotecas do Windows/UCRT.
O workflow instala libvpx e mantém a publicação por tags `v*` após os testes.
O workflow remoto e a reprodução dentro da sala do EmuVR não foram executados
nesta revisão. A sincronização auditiva no equipamento do usuário ainda deve
ser conferida. Save state/netplay não são suportados.

O ZIP de fontes foi extraído em uma pasta independente e compilado do zero;
os seis testes passaram sem depender da pasta `.local-archive` ou dos PAKs.

## Revisão r6 — Pocket Dimensional Clash 2 (5 de outubro de 2026)

Reproduzido com o PAK original de 81.175.046 bytes em `C:/EmuVR/Games/openbor`.
A r5 encerrava a carga com status 1: o script `grabscript_complete.c` consultava
`openborconstant("PLAYER_MIN_Z")`, removido da tabela atual. Depois dessa
correção, a próxima falha era `openborvariant("cheats")`, também removido.

Restauradas as consultas aos limites Z e `FRONTPANEL_Z`, avaliadas a cada chamada
para acompanhar a fase atual. Restaurado `cheats` como booleano de trapaças
ativas, sem considerar os bits que apenas liberam menus. A revisão oficial
antiga `a9d25aa16f25c88246394b7e5d5eab490cdf5939` confirmou que a propriedade
consultava `is_cheat_actived()`. Nenhuma alteração no PAK foi necessária.

Teste no frontend mínimo: 7.200 quadros até o tutorial, depois 10.800 quadros
com ciclo de botões, recarga/reset e outros 10.800 quadros, todos sem shutdown
inesperado. Capturas mostram o tutorial e o personagem no cenário de treino.
Não foi completada uma campanha, nem testada esta revisão dentro do EmuVR.

O novo teste de constantes usa o código real e verifica que a compilação não
congela os limites: muda `PLAYER_MIN_Z`/`PLAYER_MAX_Z` entre duas chamadas e
confere os resultados, incluindo `FRONTPANEL_Z`, maiúsculas/minúsculas e uma
constante estática. Os oito testes CTest passaram. A DLL identificada como r6
também passou no teste de inicialização de 1.800 quadros.

## Base r6 restaurada — 6 de outubro de 2026

Código restaurado do pacote fonte r6. O protótipo isolado state_codec e seu
teste foram retirados. O motor e o backend mantêm o código do pacote r6.
Save state/netplay não estão em desenvolvimento nesta base.

## Revisão r7 — pose de vitória após chefe

Corrigida a condição de tryvictorypose: o motor exigia inpain diferente de
IN_PAIN_NONE para iniciar a vitória. Um personagem normal nunca entrava na
animação e check_victory_pose mantinha endgame em zero, prolongando a câmera
lenta pós-chefe. A condição agora exige ausência de dor. Permanecem as demais
restrições: personagem vivo, parado, no chão, sem queda ou levantamento.

O teste usa as funções reais do controlador com o efeito de animação substituído.
Falhou antes da correção e passou depois. Cobre entrada, espera da animação,
liberação do encerramento, estados impedidos e ausência de animação de vitória.
Oito testes aprovados. Inspeção do PAK original RGA2 confirmou poses victory
sem loop nos personagens. O frontend mínimo executou 18.000 quadros com áudio,
vídeo e movimento sem shutdown; chegou à primeira fase, mas a automação não
alcançou o chefe. A transição após esse chefe ainda precisa de teste jogado.
Nenhuma alteração no PAK e nenhuma mudança de save state foi incluída.


## r8 — Carregamento do save nativo

Bad Ass Babes Episode one: com cópias dos arquivos .sav, .s00 e .cfg do usuário,
o frontend mínimo fechava por acesso inválido no quadro 1502, dentro de
load_select_screen_info (incorporada em selectplayer pelo compilador).
O laço consultava GET_ARG antes de ParseArgs inicializar a lista de argumentos.
Agora cada linha salva é interpretada antes de consultar seus argumentos.
O mesmo teste carregou a partida e completou 6000 quadros, com áudio e vídeo,
sem exceção ou shutdown. Os arquivos originais do usuário foram preservados.
Para reproduzir, use OPENBOR_TEST_LOAD_SAVE=1 no frontend smoke, com os saves
em OpenBOR sob seu diretório de trabalho e pelo menos 6000 quadros.
Com OPENBOR_COMBAT_SWEEP=1, o segundo fechamento foi reproduzido no quadro
2311, na fase street.txt do save. O efeito magic nasce sem animação idle e usa
bindentity com flag 4. adjust_bind ignorava BIND_CONFIG_ANIMATION_REMOVE ao
escolher a animação; check_edge recebia o efeito com animation nula.
A flag de remoção agora participa da seleção da animação, e uma animação nula
é inicializada mesmo quando animnum já coincide com o destino.
O teste binding_regression usa o controlador real e cobre flag 4 com animação
presente/ausente, inicialização sem animação, flag 0 e sincronização de frame
com flag 6. Nove testes aprovados.
Referência da API antiga: https://www.chronocrash.com/forum/threads/bindentity-issue.3835/


Após ambas as correções, o teste com movimentação/ataques completou 36000
quadros, com 36000 saídas de vídeo e 35614 lotes de áudio não silencioso,
shutdown=0. Concluiu street.txt e carregou disco.txt. O teste foi executado
no frontend mínimo, não na interface do RetroArch/EmuVR.


## r9 — Avengers United Battle Force / DOT legado

Reprodução: o PAK original encerrava no quadro 3 com shutdown=1 durante a
compilação de takedamagescript do Deadpool: propriedade dot não reconhecida.
O script data/scripts/antivenon.c usa slot 1 e campos owner, force, mode,
rate e time. A compatibilidade agora acessa os efeitos atuais pelo índice,
com leitura e escrita nomeada e escrita posicional antiga. Modos 0–5 são
convertidos para as flags atuais; time mantém o valor absoluto do relógio.
Slots inválidos são rejeitados e a leitura de slot vazio não aloca memória.

Referência consultada: código oficial OpenBOR, commit
bbfdbf8298b80b85736fec5e1c88d56ebaedd351, engine/openborscript.c.
Exemplo do autor do script:
https://www.chronocrash.com/forum/threads/revert-to-default-palette.2781/

O teste legacy_dot_regression cobre ambas as formas, leitura, índices,
conversão de modos e isolamento entre slots. dot_expiry_regression exercita
o controlador real com nós expirados consecutivos seguidos de efeito ativo;
a travessia guarda o próximo nó antes de liberar o atual. Onze testes passaram.
O frontend mínimo iniciou a primeira fase (data/levels/fase1.txt) e completou
7200 quadros, com 7200 saídas de vídeo, 6725 lotes audíveis e shutdown=0.
Isso valida inicialização e início da partida, não uma campanha completa.
Nenhum PAK ou save original foi alterado; save state/netplay continuam ausentes.


## r10 — Street of Rages X

Três obstáculos reproduzidos em sequência com o PAK original:
1. O core rejeitava a assinatura 0x7F14111D antes de iniciar o motor. O leitor
   packfile.c já documenta essa assinatura; a validação do core agora aceita
   esse identificador e PACK, ambos com versão zero e índice validado.
2. A compilação falhava em GLOBAL_CONFIG_PROPERTY_CHEATS, agora traduzida
   para _GLOBAL_CONFIG_CHEATS, sem alterar a configuração ou ativar cheats.
3. loading.c chama shutdown quando não consegue ler Paks/SORX.pak ou
   Paks/1.0.0.pak. No backend libretro, a leitura de Paks/<nome>.pak agora
   resolve para o arquivo selecionado pelo frontend, inclusive com outro nome.
   Não altera o PAK, os scripts ou o diretório de trabalho.

pak_regression testa as duas assinaturas, versão inválida, índice truncado,
limites do conteúdo e nomes sem terminador, além do escopo dos aliases de
caminho. legacy_constants_regression cobre a constante acrescentada.
Doze testes aprovados. O frontend mínimo reproduziu os vídeos em 1920x1080,
retornou a 480x272, iniciou data/levels/sor1/st1a.txt e completou 7200 quadros,
7200 saídas de vídeo e 6521 lotes audíveis, shutdown=0.
A campanha completa e a interface do EmuVR não foram testadas.
Saves de teste ficaram separados; nenhum PAK ou save original foi modificado.
