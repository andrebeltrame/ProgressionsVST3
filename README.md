# Progressions

**Nowhr Dynamics** · Plugin **VST 3** (e app standalone) para gerar ideias
musicais a partir da sua própria harmonia. Três caminhos, todos levando ao mesmo lugar:

1. **Solta um MIDI** — baixo, pad, lead, pluck — e o plugin descobre a
   progressão que está por trás dele.
2. **Digita a progressão** que você quer: `Am | F | C | G` ou `i VI III VII`.
3. **Escolhe um preset** — 36 progressões em 12 gêneros: House, Deep House,
   Melodic House, Afro House, Progressive House, Melodic Techno, Techno, Trance,
   Drum & Bass, UK Garage, Downtempo e os clássicos.

Com a harmonia definida, ele escreve pads, melodias, contracantos, baixos,
arpejos e plucks em cima dela — e você arrasta o resultado direto para o DAW.

![Interface do Progressions](docs/interface.png)

---

## Índice

- [O que ele faz](#o-que-ele-faz)
- [Escrevendo a progressão](#escrevendo-a-progressão)
- [Sua biblioteca de MIDIs](#sua-biblioteca-de-midis)
- [O cérebro: escrever no seu estilo](#o-cérebro-escrever-no-seu-estilo)
- [Instalando](#instalando)
- [Rodando na sua máquina](#rodando-na-sua-máquina)
- [Usando no DAW](#usando-no-daw)
- [Linha de comando](#linha-de-comando)
- [Estrutura do projeto](#estrutura-do-projeto)

Com pressa? [`docs/COMANDOS.md`](docs/COMANDOS.md) é a folha de comandos:
atualizar, conferir, aprender a coleção, compartilhar.

---

## O que ele faz

**Análise do clipe que você soltar**

| | |
|---|---|
| Tonalidade | Perfis de Temperley com viés para menor natural, ancorados nas notas que abrem e fecham o trecho. Detecta também Dórico, Mixolídio, Lídio e menor harmônica. |
| Progressão | Reconhecimento de acordes por Viterbi sobre 12 fundamentais × 10 qualidades, com bônus diatônico, bônus para a nota mais grave e penalidade por troca de acorde. |
| Papel do clipe | Baixo, pad/acordes, lead ou arpejo — e isso muda o resto da análise: um baixo monofônico ganha peso na fundamental, um lead ganha uma grade harmônica mais larga. |
| Ritmo | Grade de semicolcheias, densidade, sincopação, polifonia média e registro. É isso que faz a parte gerada respirar junto com a sua. |

**Geração**

| Parte | O que sai |
|---|---|
| **Pad** | Vozes sustentadas por acorde, com condução de vozes de verdade (busca pela inversão que menos se movimenta). |
| **Chords** | O mesmo material em bloco rítmico, seguindo o groove. |
| **Melody** | Linha construída sobre um motivo curto e **trabalhado** ao longo da frase: inversão, retrógrado, fragmentação e notas de passagem, com arco de altura (segura, sobe, pico, assenta). O botão **Craft** dosa o quanto. |
| **Counter** | Igual à melodia, mas empurrada para movimento contrário ao do clipe e proibida de dobrar as suas notas. |
| **Bass** | Fundamental com quintas, oitavas e aproximações cromáticas para o próximo acorde. Sincopado, seguindo o groove. |
| **Reese** | Uma voz grave sustentada em vez de uma linha tocada. As durações variam: um acorde pode ficar numa nota longa só ou ser dividido em duas ou três mais curtas — nunca abaixo de um tempo, senão vira pluck e não Reese. As notas que se movem vão para a quinta, para a oitava ou para um passo em direção ao próximo acorde. O interruptor **Glide** decide o resto: ligado, cada nota se sobrepõe à seguinte, que é a única forma de um synth mono fazer portamento — sem essa sobreposição no MIDI, nenhum ajuste de glide no sintetizador funciona. Desligado, cada nota termina exatamente onde a próxima começa: nada se sobrepõe, nada soma no grave, que é o que um patch **polifônico** precisa. Fica uma oitava abaixo do Bass. |
| **Sub** | A fundamental e mais nada. Não é um Reese uma oitava abaixo: o Reese *se movimenta* — quinta, oitava, um passo para o próximo acorde — e um sub que se movimenta deixou de ser sub. Toca a fundamental do acorde que estiver soando, numa oitava só (C1–C2, e o botão **Octave** move o conjunto). Duas regras que ele não quebra, porque quebrar qualquer uma das duas é o que arruína o grave: **nunca duas notas ao mesmo tempo** — um sub é uma voz por definição, e dois somados viram uma lama que mixagem nenhuma desfaz — e **nunca mais curto que um tempo**, senão vira bumbo, e bumbo já tem. O **Density** é o único botão que faz diferença aqui: embaixo é uma nota segurada por acorde, no meio ele repete a fundamental na metade do compasso, em cima repete a cada tempo. A decisão é por compasso inteiro, não por batida — sortear batidas soltas não soa como um compasso que respira, soa como um sub com defeito. |
| **Arp** | Notas do acorde em Up / Down / Up-Down / Down-Up / Converge / Random. A corrida em si continua regular — uma corrida embaralhada deixa de ser arpejo — mas **New idea** troca a figura por cima dela: onde a corrida começa, até onde ela alcança, se os ciclos sobem uma oitava e onde ela respira. Os acentos seguem o groove que estiver embaixo. |
| **Pluck** | Notas curtas do acorde em cima do groove, com saltos e oitavas dobradas nos acentos — o pluck de house. |

As partes são escritas na ordem em que você pede, e **cada uma escuta a
anterior**: peça `--part bass,melody` e a melodia herda o groove do baixo e
evita bater a nona menor contra ele nos tempos fortes. No plugin, o clipe que
você carregou faz esse papel quando é um baixo.

Com um *style model* carregado, todas essas partes passam a usar os compassos,
os movimentos e os espaçamentos aprendidos da sua coleção — veja
[O cérebro](#o-cérebro-escrever-no-seu-estilo).

**Reharmonização** — um clique troca acordes por relativas, dominantes
secundárias, substituições de trítono e empréstimo modal. Cada acorde também
pode ser empurrado na mão: clique no chip para subir um grau, botão direito para
descer.

**Acordes travados** — clique no cadeado no canto do chip e aquele acorde para
de mudar: **Reharmonise** e **Surprise me** escrevem em volta dele e o clique
que sobe um grau também passa direto. É como segurar os dois acordes que já
estão bons e girar o resto até o resto acompanhar.

**Surprise me** — ao lado do *Load MIDI*, e responde à mesma pergunta pelo outro
lado: um tira a harmonia de um clipe que você tem, o outro inventa uma quando
você não tem nada. Escolhe a tonalidade, o modo e uma progressão que funciona
nela, rola uma semente nova e escreve a parte. Com uma biblioteca aprendida ele
sorteia entre os *loops de graus* que a sua própria coleção toca, ponderados
pela frequência — são numerais romanos e uma contagem, nada do material
original vem junto. Sem biblioteca nenhuma ele caminha pela harmonia funcional
da tonalidade, que é o caminho que precisa funcionar numa instalação nova.

**Partes empilhadas** — o plugin guarda todas as partes que você escreveu e toca
todas juntas, **cada uma no seu próprio canal MIDI**: pad no 1, chords no 2,
melody no 3, e assim por diante. Desligue **Play all sequences** para voltar a
ouvir só a parte selecionada.

Uma ressalva que importa se você usa **Ableton Live**: o Live descarta o canal
MIDI ao rotear de um plugin para outra pista — canal só significa alguma coisa
na entrada e na saída de uma pista, não dentro dela. Os canais separados servem
em Cubase, Reaper, Bitwig e afins; no Live, o caminho é arrastar cada parte para
a sua pista, que é o que o botão de arrastar faz e o que os clipes com loop
correto agora garantem.

**O pad é a âncora** — as demais partes são escritas por cima dele: entram onde
ele se movimenta e evitam dobrar a voz que ele está segurando. Se você refizer o
pad, as partes já escritas **não** são refeitas — perder uma melodia que acabou
de acertar seria pior — mas ganham um `*` na aba avisando que pertencem a um pad
que não toca mais.

**Desfazer** — um passo atrás em qualquer coisa da tela: um botão, os acordes
que você digitou, um preset, uma reharmonização, uma parte removida.

**Favoritos** — quando a combinação ficou boa, **My favourites... → Keep this
one**. Ela vai para a pasta do próprio plugin, não para o projeto, então aparece
em qualquer set que você abrir depois. Como a geração é determinística, o que
fica guardado é a semente mais os ajustes — alguns bytes — e voltar num favorito
devolve as partes vivas e ainda ajustáveis, não um clipe renderizado. Junto vai
uma pasta com **um MIDI por parte**, em `~/Music/Progressions/`, para quando o
que você quer é o arquivo na mão e não o plugin.

**Saída** — arraste o resultado direto para a timeline do DAW, salve como `.mid`,
ou use a saída MIDI do plugin para tocar num instrumento seu. O arquivo escrito
declara o **loop**, não a última nota: um loop de 4 compassos entra como um clipe
de 4 compassos mesmo que o pad estivesse segurando uma nota na virada. Tem um
sintetizador de preview embutido para ouvir sem ligar nada, com um timbre
diferente por parte — o alto-falante ao lado do volume mostra se ele está
ligado, e riscado quando o som está saindo só pelos seus instrumentos.

---

## Escrevendo a progressão

O campo de texto embaixo da régua de acordes aceita as duas notações, misturadas
se você quiser:

```
Am | F | C | G                   cifras
i - VI - III - VII               graus (na tonalidade atual)
Cm7 Fm7 Bb7 Ebmaj7               espaço também separa
i9 | IV9                         graus com extensão
F#m7b5 | B7 | Em                 acidentes e qualidades completas
```

Reconhece `m`, `maj7`, `m7`, `7`, `9`, `maj9`, `m9`, `6`, `m6`, `dim`, `dim7`,
`m7b5`, `aug`, `sus2`, `sus4`, `7sus4`, `add9`, `5` e inversões com `/`
(`Cmaj7/G`). Nos graus, **maiúscula é acorde maior e minúscula é menor** — `V`
em Lá menor é Mi maior, `v` é Mi menor.

Os graus são contados **na escala da própria tonalidade**: em Lá menor,
`i VI III VII` é `Am F C G`. É a notação que produtor usa, não a do
conservatório (que escreveria `i bVI bIII bVII`).

**Com um clipe carregado**, escrever uma progressão troca só os acordes: andamento,
compasso, tamanho e groove continuam sendo os do clipe. Ou seja, dá para pegar
um baixo que você gosta e experimentar outra harmonia por cima dele.

**Sem clipe nenhum**, a progressão vira o material inteiro — um compasso por
acorde, no andamento do DAW.

O combo **Style presets** traz 20 progressões prontas, escritas em graus e por
isso transportáveis para qualquer tom. O preset mantém a tônica em que você já
está e só troca o modo (não adianta aplicar `vi IV I V` numa tonalidade menor).

```
$ harmonia-cli presets --style melodic

Melodic House
  melodic-lift          Melodic lift          i | III | VII | VI
                        Minor - The Anjuna-style four - lands well under long arpeggios.
  melodic-drive         Driving minor         i | VII | VI | VII
                        Minor - Keeps moving without resolving; good under a rolling bass.
  ...
```

O combo **Key** fixa a tonalidade em vez de deixar o detector escolher — é ela
que decide como os graus que você digitar são lidos.

---

## Sua biblioteca de MIDIs

O `scan` percorre uma pasta inteira, analisa cada `.mid` e grava um índice JSON
com tonalidade, andamento, progressão, papel (bass/lead/pad/pluck/arp) e o perfil
rítmico de cada clipe. As pastas viram etiquetas, então a organização que você já
tem no HD passa a ser pesquisável.

O papel sai do caminho do arquivo, do indício mais próximo para o mais distante:
primeiro o nome do arquivo (incluindo os prefixos que os packs usam — `BS`, `LD`,
`CH`, `PD`, `ARP`, `PL`), depois a pasta onde ele está, depois a pasta acima.
Títulos de pack e nomes de gênero são ignorados de propósito: `Melodic House &
Techno` não faz de todo clipe do pack um lead, e `Deep House` não é um baixo.

```bash
harmonia-cli scan "/Volumes/HD Externo/MIDI" --index ~/harmonia-library.json
```

```
Walked 412 folders, 3184 files
  MIDI files found : 2841
  Indexed          : 2790
  Unreadable       : 51
  System files     : 2790   (macOS "._" twins e ocultos, ignorados)
  Other files      : .wav(210) .als(41) .zip(9)

Indexed 2790 clips into /Users/você/harmonia-library.json
  Percussion     : 412
  By role        : arp=233 bass=486 chords=390 drums=412 lead=507 pad=381 pluck=432
  Top folders    : deep house(612) melodic house(548) bass(486) leads(507) ...
```

O cabeçalho existe justamente para você conferir se ele pegou tudo. Se o número
de MIDIs encontrados for menor do que você esperava:

```bash
# conta sem ler nada, é instantâneo
harmonia-cli scan "/Volumes/HD Externo/MIDI" --dry-run

# compare com o que o próprio macOS enxerga
find "/Volumes/HD Externo/MIDI" -iname "*.mid" -o -iname "*.midi" | wc -l
```

Se os dois números baterem, ele achou tudo. Se o `find` achar mais, o motivo
costuma estar na linha `Other files` (coleção ainda dentro de `.zip`, ou em
formato que não é MIDI solto) ou em pastas que são *alias*/symlink — nesse caso
use `--follow-symlinks`. `Folders refused` aponta problema de permissão.

Em pendrive ou HD formatado em **exFAT/FAT32**, o macOS cria um gêmeo `._nome.mid`
para cada arquivo copiado. Eles casam com a extensão mas são lixo de resource
fork — o scan os ignora e reporta na linha `System files`, junto com `__MACOSX`,
`.Trashes` e outras pastas de sistema. Por isso o `find` costuma contar o dobro
do que o Progressions indexa; para comparar de verdade, filtre:

```bash
find "/Volumes/KINGSTON" \( -iname "*.mid" -o -iname "*.midi" \) \
     ! -name "._*" ! -path "*/__MACOSX/*" | wc -l
```

Depois:

```bash
# tudo que é baixo de deep house em Fá menor entre 118 e 124 BPM
harmonia-cli library --index ~/harmonia-library.json \
    --tag "deep house" --role bass --key "F minor" --bpm 118-124

# que progressões aparecem na minha coleção?
harmonia-cli library --index ~/harmonia-library.json --contains "VI | VII"

# um mapa das pastas
harmonia-cli library --index ~/harmonia-library.json --tags
```

E o principal: **usar um clipe da biblioteca como doador de groove**. A harmonia
vem da progressão, o ritmo vem de um clipe seu:

```bash
harmonia-cli --preset deep-rhodes --key "F minor" \
    --index ~/harmonia-library.json \
    --groove "dh_bass_01" \
    --part pluck,pad --out ideias/
```

O `--groove` aceita tanto um caminho `.mid` quanto um pedaço de nome/pasta que é
procurado no índice.

---

## O cérebro: escrever no seu estilo

Catalogar é uma coisa; **aprender** é outra. O mesmo `scan` que monta o índice
também constrói um *style model*: uma destilação estatística de como a sua
coleção se comporta.

```bash
harmonia-cli scan "/Volumes/HD Externo/MIDI" --index ~/harmonia-library.json
```

```
Learned a style model into /Users/você/harmonia-library.style.json
  2841 clips, 11204 bars, 312 melody bars, 198 bass bars, 274 pluck bars,
  156 chords bars, 141 voicings, 200 progressions
  Your most common progressions:
    i | VI | III | VII                      184 clips
    i | VII | VI | VII                      121 clips
```

### O que ele aprende

| | |
|---|---|
| **Compassos** | Cada compasso da sua coleção vira uma máscara de 16 semicolcheias, com velocity e duração por posição — separado por papel (bass, lead, pluck, chords, arp). É por isso que um baixo gerado cai nas mesmas semicolcheias que os seus. |
| **Movimento melódico** | Cadeia de Markov sobre **passos de escala**, não sobre notas: `passo anterior → próximo passo`. Como é medido em graus, transporta para qualquer tom. |
| **Intervalos sobre a fundamental** | O que você toca sobre a raiz, separado pela força métrica da semicolcheia (cabeça de compasso / tempo / contratempo de colcheia / de semicolcheia). É o que faz um baixo gerado ter o *seu* vocabulário de oitavas e quintas. |
| **Espaçamento de acordes** | As distâncias entre as vozes dos seus pads. |
| **Progressões** | Quais encadeamentos aparecem na sua coleção, e com que frequência. |

### O que ele **não** faz

Ele **não guarda frases suas**. O que fica no arquivo são contagens — quantas
vezes um compasso tinha ataque na semicolcheia 7, quantas vezes um passo de +2
foi seguido de −1. Nada é reproduzido literalmente, e o `.style.json` não contém
MIDI: dá para versionar e compartilhar sem carregar material de ninguém junto.

Os espaçamentos de acorde também não trazem harmonia errada: eles são aplicados
como *molde* e depois encaixados nas notas do acorde que está tocando. Um
espaçamento tirado de um acorde maior, usado sobre um menor, mantém a abertura e
o registro — mas a terça continua sendo a do acorde certo.

### Usando

No **CLI**:

```bash
harmonia-cli --preset melodic-lift --key "F minor"     --style ~/harmonia-library.style.json     --part bass,melody,pluck --out ideias/
```

`--style-amount 0.5` mistura meio a meio com o feel interno; `0` ignora o
modelo. E para ver o que a sua coleção anda tocando:

```bash
harmonia-cli library --index ~/harmonia-library.json --progressions
```

### O plugin escaneia sozinho

Você não precisa da linha de comando para nada disso. Em **Learn from my
library…** o menu abre com:

- **Scan a MIDI folder…** — escolha a pasta e pronto.
- **Scan a folder I already have** — os lugares onde uma coleção costuma estar
  (a User Library do Ableton, `~/Music`, e cada drive montado em `/Volumes`),
  já filtrados pelos que existem na sua máquina.
- **Load a .style.json…** — para um modelo que você montou pelo `harmonia-cli`.

O scan roda em segundo plano, numa thread de prioridade baixa: o áudio não
engasga e a interface não trava. O painel mostra `1234 / 227335`, e o botão vira
**Stop scanning** — uma coleção grande leva minutos e você tem que poder
desistir. O que já foi aprendido até ali continua valendo.

Nada sai da sua máquina. O plugin lê os arquivos, conta estatísticas e joga o
índice fora: só o modelo fica.

### O cérebro dentro do plugin

O plugin não guarda um caminho para o seu HD — ele guarda o cérebro. Tanto o
scan acima quanto um `.style.json` carregado à mão são copiados para dentro do
plugin:

```
~/Library/Application Support/Nowhr Dynamics/Progressions/library.style.json
```

A partir daí **toda instância nova, em qualquer projeto, já abre com ele
carregado**. Você não aponta o arquivo de novo, e o projeto continua abrindo
certo mesmo que você mova, renomeie ou apague o `.style.json` original — ou
abra a sessão em outra máquina.

O `learn` entrega o modelo direto, sem passar pelo plugin:

```bash
harmonia-cli learn --index ~/tudo.json --tag "melodic house"     --style ~/melodic.style.json --install
```

Clicando no botão de novo (**Change my library…**) você troca por outro cérebro
ou usa **Forget this library** para voltar ao que veio de fábrica.

**Dentro do binário.** Para um `.vst3` realmente autossuficiente — levar para
outra máquina, ou não depender de pasta nenhuma — compile o modelo junto:

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release   -DHARMONIA_BUILD_PLUGIN=ON -DHARMONIA_STYLE_MODEL=~/melodic.style.json
```

O modelo vira bytes dentro do plugin. O `prune` mantém o arquivo pequeno por
mais gigante que seja a coleção, então isso custa umas poucas centenas de KB no
binário, não os 227 mil arquivos.

A ordem de precedência é: modelo carregado nesta instância → modelo instalado
na pasta → modelo compilado no binário.

O toggle **Write in my style** liga o cérebro e o knob **My style** dosa o
quanto ele manda.

### Um cérebro por gênero

O modelo é global — ele mistura tudo que você mandou escanear. Se você quiser
que Deep House e Melodic House não se contaminem, escaneie separado:

```bash
harmonia-cli scan "/Volumes/HD/MIDI/Deep House"    --index ~/deep.json
harmonia-cli scan "/Volumes/HD/MIDI/Melodic House" --index ~/melodic.json
```

Cada scan produz o seu próprio `.style.json`, e você escolhe qual carregar.

### Uma ressalva honesta

Testei isso contra corpora sintéticos que eu mesmo construí — sei que o modelo
aprende e reproduz o que foi ensinado (há testes que provam isso: um corpus que
só toca nas semicolcheias 0 e 6 gera baixos só nessas posições). O que eu **não**
pude testar é como soa com MIDI de house de verdade, porque o seu HD não está
aqui. É bem possível que a dosagem precise de ajuste depois do primeiro contato
com material real — me diga o que sair.

### Coleções muito grandes

Um arquivo de MIDIs baixado da internet pode ter centenas de milhares de
arquivos. Duas coisas para saber antes de apontar o scan para um desses:

- **Tempo.** Loops curtos custam menos de 1 ms; arranjos completos, uns 30 ms.
  Dividido pelos núcleos da máquina, 200 mil arquivos ficam na casa dos minutos
  — a não ser que o gargalo seja o próprio drive USB. Use `--dry-run` antes para
  saber com o que você está lidando.
- **Foco.** Um cérebro treinado com 200 mil MIDIs aleatórios da internet soa
  como a média da internet, não como você. Para o *style model*, 250 clipes
  curados de Melodic House valem mais que 200 mil arquivos genéricos.

O `--dry-run` mostra quantos MIDIs há em cada pasta de primeiro nível, que é
como você decide o que vale escanear:

```
MIDI by top-level folder:
    88104  Sample Packs
    61220  Construction Kits
    12903  Melodic House
      250  Meus Packs
```

Aí escaneie só o que representa o seu som:

```bash
harmonia-cli scan "/Volumes/HD/Packs/Melodic House" --index ~/melodic.json
```

Para catalogar tudo mas aprender só com uma parte, o fluxo é: **um scan, vários
cérebros**.

```bash
# 1. indexa o drive inteiro, sem aprender nada (mais rápido)
harmonia-cli scan /Volumes/KINGSTON --index ~/tudo.json --no-learn

# 2. monta cérebros focados a partir do índice, sem reler o drive
harmonia-cli learn --index ~/tudo.json --tag "melodic house" --style ~/melodic.style.json
harmonia-cli learn --index ~/tudo.json --role bass --bpm 120-126 --style ~/bass.style.json

# 3. escolhe qual usar
harmonia-cli --preset melodic-lift --style ~/melodic.style.json --part bass,melody
```

O `learn` aceita os mesmos filtros do `library` (`--tag`, `--role`, `--key`,
`--bpm`, `--contains`, `--min-bars`), então dá para ter um cérebro por gênero,
por papel, por faixa de andamento — e trocar no plugin conforme a faixa.

O scan usa todos os núcleos da máquina (`--threads` para controlar). O resultado
é idêntico independente de quantas threads: os arquivos são divididos em blocos
fixos, não numa fila compartilhada.

### Onde guardar os MIDIs

Recomendo **deixar os arquivos no HD e versionar só o índice**: ele é um JSON de
alguns MB que descreve a coleção inteira e não carrega áudio nem MIDI de
terceiros junto. A pasta `library/` já está no `.gitignore` caso você prefira
copiar uma seleção para dentro do projeto.

Uma ressalva prática: MIDI pack comprado quase sempre vem com licença que proíbe
redistribuição. Os seus MIDIs, feitos por você, são seus — esses pode subir sem
problema. Os de pack, melhor manter fora de um repositório público.

---

## Instalando

### Linux

```bash
sudo apt install build-essential cmake ninja-build \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
    libxcomposite-dev libasound2-dev libfreetype-dev libfontconfig1-dev \
    libgl1-mesa-dev

git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build
```

O `.vst3` fica em `build/plugin/Progressions_artefacts/Release/VST3/Progressions.vst3`.
Copie para `~/.vst3/Nowhr Dynamics/`.

### macOS

```bash
brew install cmake ninja
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build
```

Copie o `.vst3` para `~/Library/Audio/Plug-Ins/VST3/Nowhr Dynamics/`. Para gerar
AU também, acrescente `AU` em `FORMATS` no `plugin/CMakeLists.txt`.

### Windows

```powershell
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git external/JUCE
cmake -S . -B build -DHARMONIA_BUILD_PLUGIN=ON
cmake --build build --config Release
```

Copie o `.vst3` para `C:\Program Files\Common Files\VST3\Nowhr Dynamics\`.

### Instaladores prontos

Não é preciso compilar nada. Cada push dispara um build no GitHub Actions que
monta as duas plataformas nas próprias plataformas e publica:

**Um download serve os dois sistemas.** O `Progressions-1.0.0-VST3.zip` traz um
único `Progressions.vst3` com os binários de macOS e Windows lado a lado — é
para isso que o formato de bundle do VST3 existe. Descompacte e copie para:

| Sistema | Pasta |
|---|---|
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Windows | `C:\Program Files\Common Files\VST3\` |

O binário de macOS é universal (Intel e Apple Silicon) e o de Windows não
depende do Visual C++ Redistributable. O CI falha se qualquer uma dessas
coisas deixar de ser verdade, ou se o selo do bundle estiver quebrado.

Dentro do zip vai também um `INSTALL.txt` com as instruções para os dois
sistemas — quem baixa não precisa de mais nada.

Não há instalador. Sem certificado de assinatura, um `.exe` ou um `.pkg`
dispara aviso de segurança e vira uma primeira impressão pior do que uma pasta
para arrastar.

O build de macOS é **universal** (Intel e Apple Silicon) e o CI falha se não
for, ou se o selo do bundle estiver quebrado.

Baixe nas [Releases](https://github.com/andrebeltrame/ProgressionsVST3/releases), ou
em **Actions → o build mais recente → Artifacts** para uma versão de
desenvolvimento.

**Enquanto não houver certificado**, o bundle leva assinatura ad-hoc. No macOS
isso basta para o plugin carregar quando você mesmo o copia; num Mac que
recebeu o arquivo pela internet, some a quarentena uma vez com
`xattr -dr com.apple.quarantine` na pasta. No Windows, desbloqueie o `.zip`
nas Propriedades antes de extrair. O gancho de assinatura já está no lugar —
basta o segredo `MACOS_SIGN_IDENTITY` no repositório.

> Passe `-DHARMONIA_INSTALL_PLUGIN=ON` e o CMake copia sozinho para
> `.../VST3/Nowhr Dynamics/` no fim do build, criando a pasta se precisar. O
> DAW varre a pasta VST3 recursivamente, então a subpasta do fabricante só
> organiza — não é preciso configurar caminho nenhum. Se você não clonar o
> JUCE à mão, o CMake baixa via `FetchContent` (só demora mais).

---

## Rodando na sua máquina

O projeto está no GitHub; o container onde ele foi escrito é descartável.
Para trazer tudo para o seu computador:

```bash
git clone https://github.com/andrebeltrame/ProgressionsVST3.git harmonia
cd harmonia
git checkout claude/vst3-music-idea-generator-vqgacc
```

Daí siga a seção [Instalando](#instalando). Só o motor + CLI, sem o plugin, é
questão de segundos e não precisa de nenhuma dependência:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tests/harmonia_tests
```

---

## Usando no DAW

1. Abra o Progressions como **instrumento** numa pista.
2. Arraste um `.mid` para a janela, ou digite uma progressão, ou escolha um preset.
3. Confira o painel de análise e a régua de acordes.
4. Escolha a parte (**Pad**, **Melody**, **Pluck**, …) e mexa nos botões.
5. **New idea** sorteia outra versão com as mesmas configurações.
6. **Drag MIDI to your DAW** arrasta o resultado para a timeline.

Detalhes que valem saber:

- **Follow groove** faz a parte nova herdar o padrão rítmico do clipe. Desligue
  se o clipe for um pad de semibreves e você quiser algo mexido.
- **Stay clear** mantém a parte nova fora do registro que o clipe já ocupa —
  é o que impede um pad de subir em cima do lead.
- **Host sync** toca junto com o transporte do DAW; desligado, o botão **Play**
  roda em loop no andamento do próprio clipe.
- **Length** repete a progressão até completar 1, 2, 4, 8 ou 16 compassos.
- **Craft** decide o quanto a melodia trabalha o motivo. Em 0 ela só repete e
  transpõe; subindo, o motivo é invertido, tocado ao contrário, fragmentado, e
  os saltos de terça ganham a nota de passagem no meio. Só vale para Melody e
  Counter.
- **Chord changes** força a grade harmônica. Em *Auto* o plugin decide; se ele
  achar um acorde só num lead que você sabe que tem quatro, force *1 per bar*.
- **Play all sequences** toca todas as partes escritas ao mesmo tempo, cada uma no seu
  canal MIDI. No Live, para mandar a saída MIDI para outro instrumento: na pista
  de destino, *MIDI From* → a pista do Progressions, e no seletor de baixo
  escolha **Progressions** (o plugin), com Monitor em **In**. Como o Live junta
  os canais, faça isso com **Play all sequences** desligado, uma parte por vez.
- **Remove part** para de guardar a parte atual; as outras continuam tocando.
- **Undo** volta um passo em qualquer ajuste da tela.
- **Spelling** decide se as notas e os acordes aparecem com sustenido ou bemol.
  Em *Key* ele usa os acidentes que a tonalidade pede. Você pode digitar das duas
  formas em qualquer ajuste — `Db` e `C#` entram igual.
- **Time** é a fórmula de compasso. **Nada detecta métrica**: ela é lida do
  arquivo que você carregou, e é 4/4 quando não há arquivo. Se um clipe declara
  3/4 mas toca 4/4, é aqui que você discorda dele.
- **Reharm** dosa o quanto o botão **Reharmonise** reescreve da progressão.
- **O cadeado** no canto de cada acorde o congela: **Reharmonise** e **Surprise
  me** passam por cima dele sem tocar, e o clique que sobe um grau também. Serve
  para segurar o que já está bom e girar só o resto.
- **Surprise me** inventa tudo do zero — tonalidade, modo, progressão e semente
  — e escreve a parte selecionada. Com biblioteca aprendida, sorteia entre os
  loops de graus que a sua coleção realmente toca.
- **Colour** não faz nada no **Sub**, e por isso aparece apagado ali: a parte
  toca fundamentais, e extensão e cromatismo não têm onde agir.
- **Glide** só existe para o **Reese**. Ligado, as notas se encavalam, que é a
  única forma de um synth **mono** fazer portamento entre elas. Desligado, cada
  nota termina onde a próxima começa — nada soma no grave, que é o que um patch
  **polifônico** precisa.
- **O alto-falante** ao lado do volume liga e desliga o som interno do plugin, e
  mostra qual dos dois está valendo. Riscado quem toca são os seus instrumentos;
  o MIDI continua saindo dos dois jeitos.
- **My favourites…** guarda a combinação atual na pasta do plugin — global, vale
  em qualquer projeto — e escreve um MIDI por parte em `~/Music/Progressions/`.
  *Forget one* tira da lista e **deixa os MIDIs onde estão**.
- A **semente** aparece no painel: mesma semente + mesmos controles = exatamente
  a mesma ideia, sempre. Ela é salva no projeto, junto com o clipe e a progressão.

---

## Linha de comando

```bash
harmonia-cli <arquivo.mid> [opções]      analisa um clipe e escreve partes
harmonia-cli --progression "Am F C G"    escreve sobre acordes digitados
harmonia-cli --preset deep-warm          escreve sobre um preset
harmonia-cli presets [--style X]         lista as progressões prontas
harmonia-cli scan <pasta> [opções]       indexa uma pasta de MIDIs
harmonia-cli library [opções]            pesquisa o índice
```

```
$ harmonia-cli resources/examples/bass_loop.mid --info
Analysed resources/examples/bass_loop.mid
  Key            : A Minor  (confidence 0.53)
  Tempo          : 96.0 BPM
  Detected role  : Bass
  Progression    : Am | F | C | G
  Roman numerals : i | VI | III | VII
```

Principais opções de geração: `--part`, `--progression`, `--preset`, `--groove`,
`--style`, `--style-amount`,
`--variations`, `--bars`, `--bpm`, `--density`, `--complexity`, `--humanize`,
`--swing`, `--octave`, `--voices`, `--seed`, `--key`, `--reharm`,
`--chords-per-bar`, `--no-follow`, `--no-avoid`, `--info`.
`harmonia-cli --help` tem a lista completa.

---

## Estrutura do projeto

```
core/      motor em C++17, sem dependência nenhuma (nem JUCE)
           MIDI, teoria, análise, geradores, progressões, presets,
           biblioteca e o modelo de estilo
cli/       front end de linha de comando
tests/     80 testes unitários do motor
plugin/    invólucro JUCE: processador, editor, componentes de UI
           tests/ traz um smoke test headless do plugin
resources/ clipes MIDI de exemplo
docs/      como o motor funciona por dentro, e COMANDOS.md
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
note-on tem note-off, testa progressões digitadas e presets, faz round-trip do
estado e renderiza a interface em PNG.

## Licença

O código deste repositório é seu. O JUCE é licenciado à parte — veja
[juce.com/juce-licence](https://juce.com/juce-licence). Compilando sob a licença
pessoal do JUCE, o plugin exibe o splash screen do JUCE ao abrir.
