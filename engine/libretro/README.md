# OpenBOR / libretro — Windows x64

Core experimental baseado no OpenBOR 4.0 build 7533 desta árvore. A interface
foi compilada com o `libretro.h` oficial do RetroArch **1.7.5**, tag `v1.7.5`.

Revisão **r10**: aceita a assinatura alternativa de PAK usada em Street of
Rages X, preservando a validação do índice e dos limites dos arquivos. Expõe
GLOBAL_CONFIG_PROPERTY_CHEATS como alias da propriedade existente e resolve
leituras legadas de Paks/<nome>.pak para o conteúdo aberto pelo frontend.
Não exige renomear o PAK nem colocá-lo na pasta Paks. O alias só afeta leitura;
nomes com subdiretórios ou caminhos de outras pastas não são redirecionados.

A revisão **r9**: restaura a propriedade antiga `dot` de getentityproperty e
changeentityproperty, necessária para iniciar Avengers United Battle Force.
Os efeitos usam a estrutura atual do motor, com os modos e índices antigos.
Também corrige a leitura de um nó já liberado ao expirar efeitos de dano periódico.

A revisão **r8**: corrige um acesso a memória não inicializada ao restaurar
a seleção de personagens de um save nativo e restaura a opção antiga de sincronização/
remoção de efeitos vinculados (`bindentity`, flag 4). Ambos os fechamentos foram
reproduzidos em Bad Ass Babes Episode one.

A revisão **r7**: corrige a condição de entrada na pose de vitória, que impedia
o encerramento da fase após derrotar o chefe em jogos como Retro Gamer Adventure 2.

A revisão **r6**: restaura consultas antigas a `PLAYER_MIN_Z`, `PLAYER_MAX_Z`,
`FRONTPANEL_Z` e `openborvariant("cheats")`. Corrige o encerramento durante a
compilação dos scripts de Pocket Dimensional Clash 2, sem modificar o PAK.
Os limites da fase são consultados em tempo de execução; `cheats` indica
trapaças ativas, e não apenas a disponibilidade do menu.
Save state e netplay não são suportados nesta versão.

Inclui a **r5**: reproduz vídeos WebM VP8 com áudio Vorbis diretamente do PAK,
até 1920×1080. O frontend controla o tempo, o vídeo e o áudio; pular cenas
respeita a opção `noskip` do jogo. Ao terminar, a resolução do jogo é restaurada.
O áudio do vídeo usa o volume de música (100% = volume original), com conversão
para a frequência de saída do core. Não requer DLL adicional nem extrair vídeos.

Inclui a **r4**: corrige trocas sucessivas de personagens via `weaponframe`
(Dungeons & Dragons) e evita a seleção genérica antecipada quando o jogo
define sua própria seleção após cenas de introdução (Jaspion The Game).

Inclui a correção da **r3**: saída da defesa em personagens com animação de bloqueio
em loop, como a Dazzler no X-Men. Soltar o botão agora devolve o controle;
a reação a um golpe bloqueado ainda termina antes de liberar a defesa.

Inclui as correções da **r2**: conversão de efeitos sonoros de 8 bits e preservação de
`falldie` ao interpretar `nodieblink` em módulos antigos. Isso resolve o bloqueio
reproduzido no X-Men, em que Sentinelas derrotadas impediam o avanço da fase.
Os testes automatizados passaram da primeira fase no X-Men e no Retro Gamer
Adventure. Consulte `TESTING.md` para o alcance da validação.

## Instalação

1. Copie `openbor_libretro.dll` para `RetroArch/cores/`.
2. Copie `openbor_libretro.info` para `RetroArch/info/`.
3. No RetroArch, carregue o core **OpenBOR** e abra diretamente um `.pak`.

Não precisa de `bor.pak`, seletor de módulos ou pasta chamada `Paks`. O caminho
completo fornecido pelo frontend é usado sem alterar o diretório de trabalho.
Espaços e acentos são aceitos. O motor ainda limita caminhos a **255 bytes UTF-8**;
caminhos de saves também precisam deixar espaço para o nome do jogo e sufixos.
Caminhos que ultrapassam esses limites são recusados, sem truncamento.

No EmuVR, use as pastas `RetroArch/cores` e `RetroArch/info` da instalação que ele
utiliza. A configuração do Game Scanner fica a seu critério.

Exemplo de execução:

```text
retroarch.exe -L "cores/openbor_libretro.dll" "D:/Meus jogos/Aventura.pak"
```

## Saves e controles

Os saves nativos, configurações e recordes ficam em `OpenBOR/` dentro do diretório
de saves informado pelo frontend. Logs ficam em `OpenBOR/Logs/` e capturas em
`OpenBOR/ScreenShots/`. Sem diretório de saves, o fallback é a pasta do próprio
PAK. A pasta escolhida precisa ser gravável. Jogos com o mesmo nome de arquivo
compartilham o mesmo nome de save.

São disponibilizadas quatro portas RetroPad. Remapeie pelo RetroArch:

| RetroPad | OpenBOR |
|---|---|
| Direcional | Movimento |
| Y | Ataque |
| B | Pulo |
| X | Especial |
| A | Ataque 2 |
| L / R | Ataque 3 / 4 |
| Start | Iniciar / pausar |
| Select | Menu / voltar |

## Estado e limitações

- Vídeo por software em XRGB8888 e áudio estéreo pelo frontend; sem janela SDL.
- Reiniciar, descarregar e trocar conteúdo recriam a sessão do motor.
- Saves nativos do jogo funcionam. **Save states, rewind e netplay libretro não
  são implementados.**
- Apenas Windows x64 nesta implementação. O binário usa o UCRT do Windows.
- PAKs padrão, não criptografados, menores que 2 GiB. O índice do PAK é validado
  antes da inicialização. Arquivos avulsos em `data` não substituem o conteúdo.
- WebM: VP8 com Vorbis mono/estéreo, ou sem áudio. VP9, AV1, Opus e vídeos
  acima de 1080p não são suportados; falhas são registradas no log e a cena retorna.
- Rumble e controles de teclado internos não estão implementados.
  Cenas normais do OpenBOR e música Ogg/ADPCM permanecem no motor.
- Validado com os PAKs X-Men Arcade e Retro Gamer Adventure 1.0.3 fornecidos. Isso não garante
  compatibilidade de todos os módulos antigos com o OpenBOR 4.0.
- Testado no RetroArch 1.7.5 x64 distribuído na instalação local do EmuVR.
  A execução dentro da sala virtual do EmuVR ainda precisa de validação.

## Compilação

Requisitos: CMake, MSYS2 UCRT64 com GCC, Ninja, pkg-config, SDL2, libpng,
libvorbis, libogg e libvpx. As bibliotecas são vinculadas estaticamente; não é
necessário distribuir DLLs de SDL, Ogg, Vorbis ou libvpx ao lado do core.

```powershell
./engine/libretro/build.ps1 -MsysRoot C:/msys64
```

Saída: `dist/openbor-libretro-win64/openbor_libretro.dll`.

O `session.c` incorpora o motor como recurso da DLL. Ao carregar um PAK, extrai
uma cópia privada para um arquivo temporário de nome único, carrega o motor e
remove esse arquivo ao descarregar. Isso isola os globais do OpenBOR entre
sessões e entre cópias do core. Se o processo for encerrado à força, um arquivo
temporário `obr*.tmp` poderá permanecer na pasta temporária do Windows.

O backend usa fibers do Windows para suspender o loop original nos pontos de
entrega de quadros. `retro_run` controla o tempo virtual e a produção de áudio.
As chamadas de ciclo de vida e execução devem ocorrer na mesma thread.

Correções no motor: tipos inteiros de 64 bits no Windows, proteção para barras
de status de tamanho zero e caminhos de arquivos/saves próprios do libretro.
O backend converte caminhos UTF-8 para as APIs Unicode do Windows.

## Teste automatizado

`tests/smoke.c` é um frontend mínimo usando a API 1.7.5. Verifica carregamento inválido,
descarregamento antes do primeiro quadro, vídeo, áudio, entrada, reset e reabertura.
Gera capturas PPM a cada 600 quadros no diretório atual.

```powershell
C:/msys64/ucrt64/bin/gcc.exe -I engine/libretro engine/libretro/tests/smoke.c -municode -O2 -o engine/libretro/build-win64/smoke.exe
# Execute de uma pasta de testes, passando os caminhos completos da DLL e PAK:
smoke.exe "caminho/openbor_libretro.dll" "outra pasta/jogo.pak" 6000 reload
```

## Licenças

O motor e este port seguem a licença da árvore OpenBOR, reproduzida no pacote.
O cabeçalho libretro tem sua própria licença permissiva. As licenças das
bibliotecas vinculadas são distribuídas na pasta `licenses`.

Referências: [API libretro](https://docs.libretro.com/development/cores/developing-cores/)
e [cabeçalho 1.7.5](https://github.com/libretro/RetroArch/blob/v1.7.5/libretro-common/include/libretro.h).
