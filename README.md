# Launcher EBook

`Launcher EBook` é uma aplicação desktop em C++23 com Qt 6.12.0 Widgets para catalogar arquivos locais de e-books e documentos, exibir metadados básicos, reportar estatísticas e abrir cada item com aplicativos associados do sistema operacional ou com um executável escolhido manualmente pelo usuário.

O projeto foi estruturado para builds reproduzíveis no Windows 10/11 (x64) com CMake Presets, Ninja 1.13.x, MSVC e MinGW, gerando binários `Release` autossuficientes com as dependências Qt implantadas via `windeployqt` e isolamento completo entre os runtimes de cada toolchain.

## Principais funcionalidades

- Seleção de um diretório pai da biblioteca por diálogo nativo.
- Varredura recursiva de arquivos em subdiretórios.
- Filtro por extensões de e-book, documentos, texto, HTML, XML, TeX, CSV, logs e arquivos de quadrinhos.
- Listagem tabular com nome, extensão, tipo MIME, tamanho legível e caminho absoluto.
- Abertura por duplo clique com uma janela intermediária de escolha de aplicativo.
- Descoberta de aplicativos associados no Windows por associação padrão e Registro.
- Escolha manual de executável quando a associação do sistema não for suficiente.
- Botão e ação de menu para limpar os dados carregados.
- Rodapé com total de itens reconhecidos e relatórios opcionais de arquivos e pastas.
- Barra de menus com ações de arquivos, configurações, idiomas, estatísticas e informações legais.
- Persistência das preferências de relatório e idioma em arquivo JSON.
- Suporte a português e inglês por arquivos `.ts` e `.qm`.
- Geração de gráficos simples por extensão: pizza e histograma.
- Textos de Sobre carregados de `assets/about`.
- Ícone de aplicação incorporado ao executável Windows.

## Organização do projeto

```text
.
├── assets/
│   ├── about/                 # Textos carregados pelo menu Sobre
│   ├── icon/                  # Ícone .ico usado na janela e no executável
│   ├── img/                   # Imagens e recursos visuais
│   └── release/               # Notas de lançamento em português e inglês
├── build/                     # Diretórios regeneráveis de build
│   ├── Launcher_EBook_mingw/
│   ├── Launcher_EBook_msvc/
│   ├── mocks_mingw/
│   └── mocks_msvc/
├── locale/                    # Traduções Qt Linguist (.ts e .qm)
├── mocks/                     # Ferramentas auxiliares GUI para manutenção
├── repo/                      # Scripts de distribuição e instaladores Inno Setup
├── source/                    # Código-fonte principal da aplicação
├── .vscode/                   # Integração com VS Code, CMake e tasks
├── CMakeLists.txt
├── CMakePresets.json
└── main.cpp
```

O diretório `source/` concentra os módulos de domínio e interface. O diretório `build/` é regenerável e pode ser removido quando for necessário forçar uma configuração limpa.

## Arquitetura

### `main.cpp`

Inicializa `QApplication`, define o nome lógico da aplicação como `Launcher EBook`, registra a organização `LocalTools` e exibe a janela principal.

### `source/MainWindow.*`

Implementa a janela principal e coordena a maior parte do fluxo da aplicação:

- composição da UI com `QMainWindow`, `QMenuBar`, `QStatusBar`, `QTableWidget` e botões de ação;
- criação das ações de menu e atalhos;
- varredura recursiva com `QDirIterator`;
- identificação MIME com `QMimeDatabase`;
- contagem de arquivos e pastas;
- renderização de gráficos por extensão com `QPainter`;
- carregamento de textos legais em `assets/about`;
- carregamento de ícone em `assets/icon`;
- troca de idioma via `QTranslator`;
- persistência de preferências em JSON.

### `source/AssociatedApps.*`

Isola a descoberta e execução de aplicativos associados a arquivos. No Windows, consulta associações registradas e constrói uma lista de candidatos para o diálogo de escolha. Também mantém a opção de seleção manual para cenários em que o Registro ou a associação padrão não retornem um leitor adequado.

### `source/AppChooserDialog.*`

Implementa o diálogo de escolha de aplicativo. A janela exibe os candidatos encontrados, permite seleção manual de executável e retorna a escolha final para a janela principal executar a abertura do arquivo.

## Persistência de configurações

As preferências do menu `Configurações` são salvas em:

```text
%LOCALAPPDATA%\EBookLauncherQt\settings.json
```

Formato atual:

```json
{
  "version": 1,
  "settings": {
    "language": "pt_BR",
    "reportFileCount": false,
    "reportFolderCount": false
  }
}
```

Campos:

- `language`: idioma carregado na próxima inicialização. Valores suportados: `pt_BR` e `en_US`.
- `reportFileCount`: controla a exibição do contador de arquivos no rodapé.
- `reportFolderCount`: controla a exibição do contador de pastas no rodapé.
- `version`: versão do esquema de configuração.

A escrita usa `QSaveFile`, reduzindo o risco de corromper o arquivo em caso de falha durante a gravação.

## Internacionalização

Os arquivos de tradução ficam em `locale/`:

- `Launcher EBook_pt_BR.ts`
- `Launcher EBook_en_US.ts`

Durante a build, o CMake executa `lrelease` e gera os arquivos `.qm` correspondentes no diretório de build. Após a compilação, os `.qm` são copiados para `locale/` ao lado do executável, permitindo que a troca de idioma funcione no binário implantado.

O aplicativo procura os recursos em ordem:

1. diretório do executável;
2. diretório relativo acima do executável;
3. diretório de origem definido por `EBOOKLAUNCHER_SOURCE_DIR`;
4. diretório corrente do processo.

Esse comportamento permite executar tanto a partir da árvore de desenvolvimento quanto a partir da pasta de build implantada.

## Builds suportadas

O projeto usa CMake Presets com Ninja 1.13.2, padrão C++23 (`CMAKE_CXX_EXTENSIONS OFF`) e gera builds `Release` com ambientes totalmente isolados por toolchain.

Presets principais:

- `Launcher_EBook_mingw`: Qt 6.12.0 MinGW (`C:\Qt\6.12.0\mingw_64`), compilador `g++` 15.1.0 do MinGW-w64 (`C:\Qt\Tools\mingw1510_64\bin\g++.exe`), `windres.exe` e `PATH` limpo sem variáveis MSVC.
- `Launcher_EBook_msvc`: Qt 6.12.0 MSVC (`C:\Qt\6.12.0\msvc2022_64`), compilador `cl.exe` do toolset MSVC x64 `14.52.37025` (v145), Windows SDK `10.0.28000.0` (`rc.exe`, `mt.exe`, `Include` e `Lib`) e `PATH` limpo sem variáveis MinGW.

Saídas esperadas:

```text
build\Launcher_EBook_mingw\Launcher EBook.exe
build\Launcher_EBook_msvc\Launcher EBook.exe
```

Comandos equivalentes via workflow (ou configure + build separados):

```powershell
cmake --workflow --preset Launcher_EBook_mingw
```

```powershell
cmake --workflow --preset Launcher_EBook_msvc
```

Ou passo a passo:

```powershell
cmake --preset Launcher_EBook_mingw
cmake --build --preset Launcher_EBook_mingw --parallel
```

```powershell
cmake --preset Launcher_EBook_msvc
cmake --build --preset Launcher_EBook_msvc --parallel
```

No VS Code, os arquivos `.vscode/tasks.json`, `.vscode/cmake-kits.json`, `.vscode/c_cpp_properties.json` e `.vscode/settings.json` encapsulam o ambiente isolado de cada toolchain (incluindo `QTDIR`, `PATH`, Ninja 1.13.2, MinGW 15.1.0 e `vcvarsall.bat x64 10.0.28000.0 -vcvars_ver=14.52` para MSVC) tanto para a aplicação principal quanto para os mocks.

## Implantação Qt

Em builds Windows, o CMake executa `windeployqt` com perfil enxuto (`--no-compiler-runtime`, `--no-opengl-sw`, `--no-system-d3d-compiler`, `--no-system-dxc-compiler`, `--no-ffmpeg` e filtragem estrita de plugins) e implanta explicitamente apenas o runtime mínimo necessário de cada toolchain, evitando contaminação cruzada e excesso de arquivos:

- **MSVC x64**: apenas os 3 módulos Qt necessários (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`), plugins mínimos (`platforms\qwindows.dll`, `styles\qmodernwindowsstyle.dll` e `imageformats\qico.dll` no app) e as 5 DLLs app-local do CRT v145 (`msvcp140.dll`, `msvcp140_1.dll`, `msvcp140_2.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`), sem nenhuma DLL MinGW.
- **MinGW x64**: os mesmos 3 módulos Qt e plugins mínimos, acompanhados das 3 DLLs de runtime do MinGW-w64 15.1.0 (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) copiadas diretamente de `C:\Qt\Tools\mingw1510_64\bin`, sem nenhuma DLL MSVC.
- No aplicativo principal, copiam-se apenas `assets/about/`, `assets/icon/launcherebook2_256-256.ico` e os arquivos `.qm` compilados em `locale/`.

Esse passo garante execução autossuficiente com a versão exata `6.12.0.0` de `Qt6Core.dll`, mantendo a distribuição enxuta e livre de bibliotecas supérfluas.

## Distribuição externa

Depois do `windeployqt` e da cópia do runtime mínimo da toolchain, o CMake sincroniza os produtos executáveis para:

```text
D:\MISCELANEAS\VSCode\Launcher_EBook
```

As variantes principais são publicadas em:

```text
D:\MISCELANEAS\VSCode\Launcher_EBook\Launcher_EBook_mingw
D:\MISCELANEAS\VSCode\Launcher_EBook\Launcher_EBook_msvc
```

Os mocks são publicados com ambos os executáveis (`compile_translations.exe` e `find_VersionEditor.exe`) e seus módulos/plugins Qt 6.12.0 compartilhados em pastas isoladas por toolchain:

```text
D:\MISCELANEAS\VSCode\Launcher_EBook\mocks_mingw
D:\MISCELANEAS\VSCode\Launcher_EBook\mocks_msvc
```

O script `repo/sync_distribution.ps1` remove resíduos de CMake/Ninja, artefatos intermediários (`.ilk`, `.pdb`, `.exp`, `.lib`, `.obj`, `.res`, etc.), DLLs/plugins/assets supérfluos e `vc_redist*.exe`, impõe isolamento estrito entre MSVC (exigindo as 5 DLLs CRT v145 app-local) e MinGW (exigindo as 3 DLLs MinGW 15.1.0) e valida `Qt6Core.dll == 6.12.0.0` no destino. Os instaladores Inno Setup ficam em `repo/Launcher_EBook*.iss`, validam a versão do Qt e o runtime app-local de cada variante (sem embutir `vc_redist.x64.exe` redundante no instalador MSVC) e geram os executáveis finais em `D:\MISCELANEAS\VSCode\Launcher_EBook\InnoSetupOutput` via Inno Setup 7 (`C:\Program Files\Inno Setup 7\ISCC.exe`).

No VS Code, a tarefa `Gerar Distribuição Launcher EBook` recompila o aplicativo principal e ambos os mocks nas variantes MinGW e MSVC, enquanto `Gerar Instaladores Launcher EBook (Inno Setup)` recompila as variantes principais e executa o `ISCC.exe` do Inno Setup 7 sobre `repo/Launcher_EBook.iss`.

## Ferramentas auxiliares

O diretório `mocks/` contém pequenas GUIs de manutenção:

- `compile_translations`: compila arquivos `.ts` para `.qm` usando `lrelease`.
- `find_VersionEditor`: atualiza versões em textos de Sobre e no `CMakeLists.txt`.

Ambas as ferramentas suportam build dual em **MinGW x64** e **MSVC x64** com C++23 e Qt 6.12.0, utilizando diretórios de saída separados por toolchain:

```text
build\mocks_mingw
build\mocks_msvc
```

Presets em `mocks/CMakePresets.json`:

```powershell
# MinGW x64
cmake --workflow --preset mocks-Launcher_EBook_mingw
cmake --build --preset mock-version-editor --parallel
cmake --build --preset mock-compile-translations --parallel

# MSVC x64
cmake --workflow --preset mocks-Launcher_EBook_msvc
cmake --build --preset mock-version-editor-msvc --parallel
cmake --build --preset mock-compile-translations-msvc --parallel
```

## Atalhos de teclado

- `Ctrl+O`: escolher diretório.
- `Ctrl+R`: listar arquivos.
- `Ctrl+Shift+L`: limpar dados.
- `Ctrl+Q`: sair.
- `Ctrl+Alt+A`: alternar relatório de quantidade de arquivos.
- `Ctrl+Alt+P`: alternar relatório de quantidade de pastas.
- `Ctrl+Alt+1`: português.
- `Ctrl+Alt+2`: inglês.
- `Ctrl+Alt+G`: gráfico de pizza.
- `Ctrl+Alt+H`: histograma.
- `Ctrl+Alt+I`: propriedade intelectual.
- `Ctrl+Alt+D`: termo de isenção de responsabilidade.
- `Ctrl+Alt+M`: licença MIT.

## Boas práticas aplicadas

- Separação entre UI principal, descoberta de aplicativos associados e diálogo de escolha.
- Build declarativa com CMake Presets.
- Builds `Release` por padrão.
- Integração direta com VS Code por `tasks.json`, `settings.json` e `c_cpp_properties.json`.
- Uso de APIs Qt para filesystem, MIME, JSON, tradução e UI.
- Persistência atômica de configurações com `QSaveFile`.
- Recursos externos copiados para o diretório de execução no pós-build.
- Diretórios de build isolados por toolchain.
- Mocks separados da aplicação principal para manutenção sem poluir o fluxo de execução do usuário final.

## Requisitos

- Windows 10 ou 11 (x64).
- CMake 3.28 ou superior (validado com CMake 4.5.1).
- Ninja 1.13.x (validado com Ninja 1.13.2 do Visual Studio 18).
- Qt 6.12.0 (`msvc2022_64` e `mingw_64`) com módulos `Core`, `Gui`, `Widgets` e `LinguistTools`.
- MinGW-w64 15.1.0 (`C:\Qt\Tools\mingw1510_64`) para builds MinGW x64.
- Visual Studio 2026/18 Community com toolset MSVC x64 `14.52.37025` (v145), Redistributable v145 e Windows SDK `10.0.28000.0` para builds MSVC x64.
- Inno Setup 7 (`C:\Program Files\Inno Setup 7\ISCC.exe`) para geração dos instaladores.

## Limpeza de builds

Os diretórios abaixo são regeneráveis:

```text
build\Launcher_EBook_mingw
build\Launcher_EBook_msvc
build\mocks_mingw
build\mocks_msvc
install\Launcher_EBook_mingw
install\Launcher_EBook_msvc
```

Para limpeza assistida dos artefatos regeneráveis do projeto principal e dos mocks (MSVC e MinGW, além de pastas legadas suportadas), use:

```powershell
.\mocks\limpar_build_qt6_regeneravel.ps1
```

Use `-WhatIf` para simular antes de remover:

```powershell
.\mocks\limpar_build_qt6_regeneravel.ps1 -WhatIf
```

## Licença

O projeto inclui texto de Licença MIT em `assets/about`, em português e inglês, para exibição no menu `Sobre`.
