# Dolphin Finder (teste local)

Base: Dolphin 26.08.2, tag `v26.08.2` do KDE.

## Abrir

Use **Dolphin Finder (Teste)** no menu de aplicativos ou execute:

```sh
~/.local/bin/dolphin-finder
```

O executável fica em `~/dolphin-finder/build/bin/dolphin`. Mantenha esta pasta
enquanto usar a versão de teste. O Dolphin instalado pelo pacman continua disponível.

## Interface

- Painel Locais à esquerda, ocupando toda a altura, com fundo translúcido.
- Fechar, minimizar e maximizar/restaurar no próprio painel.
- Arraste o espaço vazio ao lado dos botões para mover a janela.
- Clique duas vezes nesse espaço para maximizar/restaurar.
- Arraste a margem externa de 6 pixels para redimensionar; o atalho Meta +
  botão direito do Plasma também pode ser usado conforme a configuração do KWin.
- Navegação e arquivos ficam na coluna à direita.
- O painel Locais permanece visível e fixo, pois contém os controles da janela.

É um protótipo de interface; a disposição dos controles do Dolphin não é uma
cópia completa do Nautilus. Os efeitos de vidro dependem do compositor KWin.

## Configurações separadas

O lançador usa `~/.local/share/dolphin-finder-test/` para configuração, dados,
cache e estado. Ícones e fontes continuam disponíveis a partir dos diretórios
do usuário e do sistema. O tema Kvantum e a paleta inicial foram copiados para
o perfil de teste. Alterações posteriores no tema global não são copiadas
automaticamente para esse perfil.

O serviço D-Bus do protótipo não registra `org.freedesktop.FileManager1`.
O atalho também não declara associações MIME padrão.

## Recompilar e verificar

```sh
cmake -S ~/dolphin-finder -B ~/dolphin-finder/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local/opt/dolphin-finder"
cmake --build ~/dolphin-finder/build -j 2
DOLPHIN_FINDER_PROFILE="$(mktemp -d /tmp/opencode/finder-check.XXXXXX)" \
  QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 \
  ~/.local/bin/dolphin-finder --finder-self-test /tmp/opencode
```

O autoteste verifica o layout, a divisão de visualização e os controles de
janela. Não verifica o blur do compositor nem gestos Wayland.

O modo de ícones do perfil usa ícones de 48 px, rótulos compactos e largura
mínima para manter pelo menos cinco colunas na janela normal.

Validação local: compilação concluída, autoteste aprovado em Wayland e
conferência visual do painel integrado e translúcido com o compositor ativo.

Não é necessário executar `cmake --install`: o lançador usa a árvore de build.
Após atualizações incompatíveis de Qt/KDE, pode ser necessário recompilar.
Uma atualização do Dolphin do pacman não atualiza este código personalizado.

## Remover

Feche a versão de teste e remova os seguintes caminhos, caso não queira mais usá-la:

- `~/.local/bin/dolphin-finder`
- `~/.local/share/applications/org.kde.dolphin-finder.desktop`
- `~/.local/share/dolphin-finder-test/`
- `~/dolphin-finder/`

O tema global e o executável `/usr/bin/dolphin` não são alterados por este protótipo.
