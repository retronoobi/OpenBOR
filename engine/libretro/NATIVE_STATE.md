# Estado nativo: implementação em andamento

Decisão do usuário: estado completo com carregamento rápido, sem reprodução
do histórico de comandos. Rewind não é prioridade. O core r5 continua sendo a
versão funcional; **save state e netplay ainda não estão habilitados**.

## Implementado e testado

`state_codec.c/.h` define os primeiros componentes do formato nativo:

- Inteiros de tamanho explícito, ordem little endian e ausência de padding C.
- Cabeçalho com versão do formato, build do motor, identidade SHA-256 do PAK
  fornecida pelo chamador, tamanho do conteúdo e CRC32.
- Valores reais `ScriptVariant`: vazio, inteiro com validação de faixa,
  double com preservação dos bits, string por conteúdo e referência por
  tipo/identificador/deslocamento. Endereços nativos não entram no arquivo.
- Referências desconhecidas são rejeitadas; destinos existentes não são
  alterados quando uma variável não pode ser decodificada.
- Testes com o cache real de strings do motor, mudança de endereço do objeto,
  referências compartilhadas, arquivos truncados/corrompidos e PAK diferente.

Esse código só é compilado pelo teste `state_codec_regression`. Não está
integrado ao motor nem às funções públicas `retro_serialize*`. O SHA-256 do PAK
ainda precisa ser calculado pela integração. Passar nesse teste não significa
que uma partida já possa ser salva.

## Trabalho necessário antes de habilitar o recurso

1. **Ponto de retomada explícito.** Hoje `video_copy_screen` suspende a fiber
   enquanto `update`, `playlevel`, `playgame`, menus ou cenas ainda possuem
   chamadas e variáveis locais pendentes. Em `update`, até `spriteq_clear`,
   `check_music` e `sound_update_music` ficam depois dessa suspensão. Salvar
   apenas entidades/globais perderia essa continuação. É preciso tornar o
   fluxo persistente e restaurável, sem gravar a pilha nativa do Windows.
2. **Registro completo de objetos.** Entidades, modelos/animações modificáveis,
   listas, metadados, arrays, recursos e funções de comportamento precisam de
   identidades estáveis. O par de callbacks do codec ainda não é esse registro.
3. **Mundo e scripts.** Serializar os jogadores, fase e fila de spawns, câmera,
   tempos/aleatoriedade, variáveis globais e locais, heap de scripts,
   interpretadores e alterações em modelos. Preservar ciclos e aliases.
   O `s_model` embutido em `entity` também contém referências e dados mutáveis;
   uma cópia bruta da estrutura não é um estado portátil.
4. **Áudio, cenas e I/O.** Restaurar canais, posições/fracionários do mixer,
   decodificadores Ogg/ADPCM/WebM, posição das cenas, entradas e relógio do
   backend. Reconstruir arquivos pelo identificador lógico e posição, sem
   carregar handles do Windows ou repetir gravações nos saves nativos.
5. **Restauração transacional.** Validar todas as seções e resolver objetos em
   uma área temporária antes de trocar a partida ativa. Erro não pode deixar
   uma partida parcialmente restaurada. Definir limites e tamanho estável
   para a API 1.7.5 e rejeitar conteúdo/revisões incompatíveis.
6. **Validação funcional e netplay.** Salvar, avançar, carregar e comparar os
   quadros/áudio posteriores; repetir em outra sessão/processo e testar
   estados danificados. Depois validar duas instâncias do RetroArch 1.7.5,
   entrada de jogadores, sincronização inicial e rollback sob atraso de rede.

## RetroArch 1.7.5

O código oficial foi consultado: `netplay_init_serialization` aloca buffers a
partir de `retro_serialize_size`. Em `netplay_sync.c`, falha de serialização
ativa `NETPLAY_QUIRK_NO_SAVESTATES` e `stateless_mode`: o frontend espera os
comandos remotos a cada quadro porque não consegue refazer estados anteriores.
Essa alternativa não equivale a disponibilizar save state completo e rollback.

Referências: [inicialização](https://github.com/libretro/RetroArch/blob/v1.7.5/network/netplay/netplay_init.c),
[sincronização](https://github.com/libretro/RetroArch/blob/v1.7.5/network/netplay/netplay_sync.c).
As cópias de pesquisa estão em `test-run/netplay`, fora do pacote de fontes.
