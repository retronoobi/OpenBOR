# OpenBOR

Core libretro para Windows x64, baseado no OpenBOR 4.0 build 7533, com foco no
RetroArch 1.7.5 utilizado pelo EmuVR. Carrega o PAK diretamente pelo caminho
fornecido pelo frontend, sem exigir uma pasta chamada `Paks`.

## Instalação

Copie `openbor_libretro.dll` para `RetroArch/cores` e
`openbor_libretro.info` para `RetroArch/info`. O nome exibido é **OpenBOR**.

[Controles, correções e limitações](engine/libretro/README.md) ·
[Validação](engine/libretro/TESTING.md)

## Compilar localmente

Instale MSYS2 em `C:/msys64` e execute no terminal **UCRT64**:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,SDL2,libpng,libvorbis,libogg,libvpx,zlib}
```

Depois, no PowerShell, a partir da raiz do projeto:

```powershell
./engine/libretro/build.ps1
```

Saída: `dist/openbor-libretro-win64.zip`. O script interrompe a criação do pacote
se a compilação ou os testes falharem. Para outra instalação MSYS2, passe
`-MsysRoot D:/msys64`.

## Organização

- `engine/openbor.c`, `openbor.h`, `openborscript.c`: motor e integração de scripts.
- `engine/source`: componentes usados pelo motor e cabeçalhos de compatibilidade.
- `engine/sdl`: suporte de threads e cabeçalhos necessários ao port.
- `engine/libretro`: interface libretro, sessão, arquivos UTF-8 e compilação.
- `engine/libretro/tests`: testes de regressão e frontend mínimo para testes com PAK.
- `.github/workflows/build.yml`: compilação automatizada.

`Sources.cmake` lista explicitamente os fontes compilados. Os ports de outras
plataformas, SDKs e ferramentas do projeto original foram retirados da árvore
ativa. Na cópia local, foram preservados em `.local-archive`, ignorada pelo Git.
PAKs, diagnósticos locais e saídas de compilação também são ignorados.

Esta adaptação mantém a licença e os créditos do OpenBOR. Consulte [LICENSE](LICENSE)
e as licenças das dependências em `engine/libretro/licenses`.
