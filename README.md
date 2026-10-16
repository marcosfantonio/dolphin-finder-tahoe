# Dolphin Finder

Uma experiência estilo Finder/macOS para o Dolphin do KDE: painel de locais
integrado aos controles da janela, cabeçalho sem decoração separada e fundo
translúcido com blur no KWin.

Este é um **fork de teste**, baseado no Dolphin 26.08.2 do KDE. Não substitui
nem altera o Dolphin do sistema, não registra o serviço de gerenciador de
arquivos padrão e mantém configurações próprias.

## O que muda

- Painel **Locais** ocupa toda a coluna esquerda; os botões vermelho, amarelo e
  verde ficam no topo dela.
- Ícones de fechar, minimizar e maximizar/restaurar aparecem no hover de cada
  botão, individualmente.
- A barra de navegação e os arquivos ficam na coluna direita, sem uma barra de
  título KWin separada.
- Blur e transparência dependem do compositor KWin ativo. O tema e os ícones
  MacTahoe são opcionais e não estão incluídos neste repositório.
- O marcador de seleção “+/−” do Dolphin é desativado no perfil novo.
- O painel Locais permanece fixo/visível, já que também abriga os controles.

Arraste a área livre do cabeçalho para mover a janela; dê dois cliques nela
para maximizar/restaurar. A janela pode ser redimensionada pelas bordas.

## Requisitos e instalação no CachyOS

No CachyOS, com sessão KDE Plasma/Wayland ou X11, clone este repositório e rode:

```sh
git clone <URL_DO_REPOSITORIO> ~/Documentos/github/dolphin-finder
cd ~/Documentos/github/dolphin-finder
./scripts/build-cachyos.sh
```

Durante a instalação, escolha entre o ícone temático Dolphin e o ícone macOS
Finder. Também é possível selecionar diretamente:

```sh
./scripts/build-cachyos.sh --theme-icon
./scripts/build-cachyos.sh --macos-icon
```

O script verifica as dependências e compila/instala **apenas no seu usuário**.
Se faltar pacote, mostra a linha `pacman` apropriada. Para pedir ao script que
instale automaticamente os pacotes ausentes, use:

```sh
./scripts/build-cachyos.sh --install-deps
```

Isso requer `sudo` e instala dependências de desenvolvimento do KDE/Qt pelo
pacman. O build usa dois jobs por padrão; ajuste com `JOBS=4` se houver memória
suficiente.

Depois, abra **Dolphin Finder** pelo menu de aplicativos ou execute:

```sh
dolphin-finder
```

O script cria o executável em `~/.local/opt/dolphin-finder`, um lançador em
`~/.local/bin/dolphin-finder` e o atalho **Dolphin Finder** no menu.
O perfil fica em `~/.local/share/dolphin-finder`. Na primeira instalação,
o `kdeglobals` e a configuração do Kvantum do usuário são copiados quando
existem; não são sobrescritos em builds seguintes.

Para personalizar o visual no outro computador, instale nele o tema Kvantum e
os ícones MacTahoe desejados. A aparência exata do tema não é distribuída por
este projeto.

## Desenvolver e testar

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$HOME/.local/opt/dolphin-finder"
cmake --build build --parallel 2
cmake --install build
```

O teste de fumaça automatizado da interface, incluindo split view e controles
da janela, pode ser rodado em uma sessão gráfica com:

```sh
QT_FORCE_STDERR_LOGGING=1 ~/.local/bin/dolphin-finder --finder-self-test
```

O modo `offscreen` do Qt também verifica os controles, mas não consegue validar
blur, movimentação ou redimensionamento do compositor.

## Limitações

Esta alteração é uma adaptação da interface do Dolphin, não uma implementação
completa do Finder. Algumas preferências e atualizações do Dolphin podem
precisar de ajustes no fork. O repositório não contém os temas externos do KDE.

## Remover

Feche as janelas do Dolphin Finder e remova, se desejar, os caminhos criados
pelo script:

- `~/.local/bin/dolphin-finder`
- `~/.local/share/applications/org.kde.dolphin-finder.desktop`
- `~/.local/opt/dolphin-finder`
- `~/.local/share/dolphin-finder`

O Dolphin original (`/usr/bin/dolphin`) e as configurações globais permanecem
intactos.

## Licença

O código original do Dolphin e este fork são distribuídos sob as licenças
indicadas nos arquivos `COPYING`, `COPYING.DOC` e `LICENSES/`. Os avisos de
copyright dos contribuidores KDE foram mantidos.
