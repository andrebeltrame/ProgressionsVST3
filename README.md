# Harmonia

Plugin **VST 3** (e app standalone) que recebe um MIDI qualquer — baixo, pad,
lead, arpejo — descobre a harmonia que está por trás dele e escreve ideias novas
em cima: pads, melodias, contracantos, baixos e arpejos que seguem a mesma
progressão.

A ideia central: você joga um **baixo** de 4 compassos na janela, o plugin
responde `Am | F | C | G` e já entrega um pad com esse encadeamento, um lead que
resolve nos acordes e um contracanto que não briga com a sua linha.

![Interface do Harmonia](docs/interface.png)

---

## O que ele faz

**Análise do clipe que você soltar**

| | |
|---|---|
| Tonalidade | Perfis de Temperley com viés para menor natural, ancorados nas notas que abrem e fecham o trecho. Detecta também Dórico, Mixolídio, Lídio e menor harmônica. |
| Progressão | Reconhecimento de acordes por Viterbi sobre 12 fundamentais × 10 qualidades, com bônus para acordes diatônicos, bônus para a nota mais grave do compasso e penalidade por troca de acorde. |
| Papel do clipe | Baixo, pad/acordes, lead ou arpejo — e a detecção muda o resto: um baixo monofônico ganha peso extra na fundamental, um lead ganha uma grade harmônica mais larga. |
| Ritmo | Grade de semicolcheias, densidade, sincopação, polifonia média e registro. É isso que faz a parte gerada respirar junto com a sua. |

**Geração**

| Parte | O que sai |
|---|---|
| **Pad** | Vozes sustentadas por acorde, com condução de vozes de verdade (busca por inversão que menos se movimenta). |
| **Chords** | O mesmo material em bloco rítmico, seguindo o groove do clipe. |
| **Melody** | Linha construída a partir de um motivo curto, repetido e variado em frases de 4 compassos, ancorada em notas do acorde nos tempos fortes e resolvendo no fim. |
| **Counter** | Igual à melodia, mas empurrada para movimento contrário ao do clipe e proibida de dobrar as suas notas. |
| **Bass** | Fundamental com quintas, oitavas e aproximações cromáticas para o próximo acorde, na densidade que você pedir. |
| **Arp** | Notas do acorde em Up / Down / Up-Down / Down-Up / Converge / Random. |

**Reharmonização** — um clique troca acordes por relativas, dominantes
secundárias, substituições de trítono e empréstimo modal. Cada acorde também
pode ser empurrado na mão: clique no chip para subir um grau, botão direito para
descer.

**Saída** — arraste o resultado direto para a timeline do DAW, salve como `.mid`,
ou use a saída MIDI do plugin para tocar num instrumento seu. Tem um sintetizador
de preview embutido para você ouvir sem ligar nada.

---

## Instalando

### Linux

```bash
sudo apt install build-essential cmake ninja-build \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
    libxcomposite-dev libasound2-dev libfreetype-dev libfontconfig1-dev \
    libgl1-mesa-dev

git clone <este-repo> harmonia && cd harmonia
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build
```

O `.vst3` fica em `build/plugin/Harmonia_artefacts/Release/VST3/Harmonia.vst3`.
Copie para `~/.vst3/`.

### macOS

```bash
brew install cmake ninja
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build
```

Copie o `.vst3` para `~/Library/Audio/Plug-Ins/VST3/`. Para gerar AU também,
acrescente `AU` em `FORMATS` no `plugin/CMakeLists.txt`.

### Windows

```powershell
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build --config Release
```

Copie o `.vst3` para `C:\Program Files\Common Files\VST3\`.

> Passe `-DHARMONIA_INSTALL_PLUGIN=ON` e o CMake já copia para a pasta de
> plugins do sistema no fim do build. Se você não clonar o JUCE à mão, o CMake
> baixa sozinho via `FetchContent` (só demora mais).

---

## Usando no DAW

1. Abra o Harmonia como **instrumento** numa pista.
2. Arraste um `.mid` para a janela (ou **Load MIDI…**). Tem exemplos em
   `resources/examples/`.
3. Olhe o painel de análise e a régua de acordes — é o que o plugin entendeu.
4. Escolha a parte (**Pad**, **Melody**, …) e mexa nos botões.
5. **New idea** sorteia outra versão com as mesmas configurações.
6. **Drag MIDI to your DAW** arrasta o resultado para a timeline.

Alguns detalhes que valem saber:

- **Follow groove** faz a parte nova herdar o padrão rítmico do clipe. Desligue
  se o clipe for um pad de semibreves e você quiser uma melodia mexida.
- **Stay clear** mantém a parte nova fora do registro que o clipe já ocupa —
  é o que impede um pad de subir em cima do lead.
- **Host sync** toca junto com o transporte do DAW; desligado, o botão **Play**
  roda em loop no andamento do próprio clipe.
- **Length** repete a progressão até completar 1, 2, 4, 8 ou 16 compassos.
- **Chord changes** força a grade harmônica. Em *Auto* o plugin decide sozinho;
  se ele achar um acorde só num lead que você sabe que tem quatro, force
  *1 per bar*. Mudar isso refaz a análise e descarta acordes editados à mão.
- A **semente** aparece no painel: mesma semente + mesmos controles = exatamente
  a mesma ideia, sempre. Ela é salva no projeto.

---

## Linha de comando

O motor também roda sem DAW nenhum, útil para gerar material em lote:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/cli/harmonia-cli resources/examples/bass_loop.mid --info
./build/cli/harmonia-cli meu_baixo.mid --part pad,melody,counter --variations 3 --out ideias/
```

```
Analysed resources/examples/bass_loop.mid
  Key            : A Minor  (confidence 0.53)
  Tempo          : 96.0 BPM
  Detected role  : Bass
  Progression    : Am | F | C | G
  Roman numerals : i | bVI | bIII | bVII
```

Opções principais: `--part`, `--variations`, `--bars`, `--density`,
`--complexity`, `--humanize`, `--swing`, `--octave`, `--voices`, `--seed`,
`--key`, `--reharm`, `--chords-per-bar`, `--no-follow`, `--no-avoid`, `--info`.
Rode `harmonia-cli --help` para a lista completa.

---

## Estrutura do projeto

```
core/      motor puro em C++17, sem dependência nenhuma (nem JUCE)
           leitor/escritor de SMF, teoria musical, análise, geradores
cli/       front end de linha de comando
tests/     37 testes unitários do motor
plugin/    invólucro JUCE: processador, editor, componentes de UI
           tests/ traz um smoke test headless do plugin
resources/ clipes MIDI de exemplo
docs/      como o motor funciona por dentro
```

O motor não conhece o JUCE. Isso mantém a análise e a geração testáveis em
segundos, sem abrir DAW nenhum — e permite reaproveitar o mesmo código em outro
invólucro (AU, CLAP, um app web via WASM) sem reescrever nada.

## Rodando os testes

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build && ./build/tests/harmonia_tests
```

Com `-DHARMONIA_BUILD_PLUGIN=ON` o alvo `harmonia_plugin_tests` também é
construído: ele instancia o plugin, roda 400 blocos de áudio, confere que todo
note-on tem note-off, faz round-trip do estado e renderiza a interface em PNG.

## Licença

O código deste repositório é seu. O JUCE é licenciado à parte — veja
[juce.com/juce-licence](https://juce.com/juce-licence). Compilando sob a licença
pessoal do JUCE, o plugin exibe o splash screen do JUCE ao abrir.
