# Como o Progressions funciona por dentro

Documento para quem for mexer no código. Todo o miolo está em `core/`, sem
nenhuma dependência externa — dá para compilar e testar em segundos.

## O caminho de um clipe

```
arquivo .mid                          progressão digitada / preset
   │                                          │
   ├─ midi::readFromMemory                    ├─ parseProgression()
   │     core/src/MidiFile.cpp                │     core/src/Progression.cpp
   │     junta as tracks numa                 │     cifras e graus, misturados
   │     NoteSequence a 960 PPQ               │
   │                                          │
   ├─ analyze()  core/src/Analysis.cpp        ├─ com clipe:  applyProgressionTo()
   │     ├─ pitchClassHistogram               │      troca só os acordes
   │     ├─ detectKey                         │
   │     ├─ buildRhythmProfile                └─ sem clipe: analysisFromProgression()
   │     ├─ detectRole                               monta uma Analysis do zero
   │     └─ Viterbi -> progressão                          │
   │                     │                                 │
   └─────────────────────┴─────────────┬───────────────────┘
                                       │
                                  generate()   core/src/Generators.cpp
                                  escreve a parte sobre Analysis::progression
```

## Detecção de tonalidade

`detectKey()` correlaciona o histograma de classes de altura com 24 perfis
(12 tônicas × maior/menor). Três detalhes importam mais do que o algoritmo em si:

1. **Perfis de Temperley, não os de Krumhansl-Kessler.** Os originais erram
   feio em loops de 2 a 4 compassos, que é exatamente o que se joga aqui.
2. **O perfil menor foi puxado para o menor natural** — a sétima maior foi
   rebaixada e a menor promovida. Os perfis clássicos esperam uma sensível que
   pop, rock e eletrônica quase nunca têm, e sem esse ajuste `Am F C G` vira
   Fá maior.
3. **Bônus de borda.** Correlação nenhuma distingue Dó maior de Lá menor: são
   as mesmas notas. Quem desempata é a classe de altura mais grave que abre o
   trecho (+0,14) e a que o fecha (+0,06).

Depois vem um passo modal: se a sexta maior aparece com força num contexto
menor, vira Dórico; se a sétima menor domina num contexto maior, vira
Mixolídio, e assim por diante. O teste exige que a nota modal esteja de fato
presente, não apenas que a rival esteja ausente.

## Reconhecimento de acordes

O tempo é fatiado em quadros — um por tempo, ou um por meio compasso quando o
clipe é uma melodia, porque uma linha solta implica harmonia de forma bem mais
frouxa. Cada quadro vira um vetor de 12 pesos (sobreposição × velocidade, com
ataque valendo mais que sustentação).

Para cada quadro e cada um dos 120 candidatos (12 fundamentais × maior, menor,
dim, aug, sus2, sus4, maj7, m7, 7, m7b5) calcula-se:

```
+ peso das notas que pertencem ao acorde   (fundamental +0,35, terça +0,18, sétima +0,08)
− peso das notas de fora                   (×0,85)
+ bônus se a nota mais grave é a fundamental
    0,45 para baixo · 0,25 para acordes · 0,12 para melodia
+ 0,22 se o acorde é diatônico à tonalidade detectada
− custo por tamanho e por qualidade        (sus e aug precisam ganhar de verdade)
```

Um Viterbi encadeia os quadros com penalidade por troca de acorde — menor na
barra de compasso, maior no meio dele, e 1,7× maior quando a fonte é uma
melodia. Isso é o que faz um baixo de semínimas em `C A F G` sair como quatro
acordes de um compasso, e não dezesseis picotados.

### Peso da evidência

Um detalhe que muda tudo em cima de linhas de baixo: **quadro com uma nota só
vale menos**. Uma classe de altura sozinha serve igualmente bem para dezenas de
acordes, então a emissão inteira é multiplicada por

```
evidência = 0,35 + 0,35 × (classes de altura distintas − 1)   (limitado a 1,0)
```

Uma nota → 0,35. Duas → 0,70. Três ou mais → 1,0. Sem isso, a nota cromática de
aproximação que um baixo toca no último tempo do compasso ganha do acorde
inteiro e vira um acorde próprio. Com isso, `C C C E | F F F A | Bb Bb Bb D`
sai como `C | F | Bb` em vez de seis acordes picotados.

O bônus da nota grave também é escalado pela força métrica do quadro (1,0 na
cabeça do compasso, 0,7 no meio, 0,45 no resto): baixo em tempo forte é
fundamental, a mesma nota no contratempo é passagem.

No fim, quadros iguais viram `ChordSegment`, e um segmento curto espremido
entre dois acordes idênticos é absorvido (nota de passagem).

## Graus e progressões escritas

`Chord::romanNumeral()` conta os graus **na escala da tonalidade**, não na escala
maior. Em Lá menor, Fá é `VI`, não `bVI`. Acordes de fora da escala ganham `b` ou
`#`. Acordes sem terça (sus, power) herdam a caixa do acorde diatônico daquele
grau, para que `isus2` volte a ser `isus2` depois de escrito e lido de novo.

`parseProgression()` faz o caminho inverso e aceita os dois mundos no mesmo
texto. Cada token é tentado primeiro como cifra (`Am7`, `F#m7b5`, `Cmaj7/G`) e
depois como grau (`i`, `bVII7`, `V7`). Não há ambiguidade real entre os dois:
nenhum nome de nota começa com `i` ou `v`, e `bVII` falha como cifra (`B` seguido
de `VII`, que não é qualidade nenhuma) antes de ser tentado como grau.

Duas formas de aplicar o resultado:

- **`applyProgressionTo(analysis, chords)`** — com um clipe carregado. Mantém
  andamento, compasso, tamanho, groove e papel detectado; divide o comprimento
  igualmente entre os acordes e encaixa as trocas na grade de tempos.
- **`analysisFromProgression(...)`** — sem clipe. Monta uma `Analysis` completa
  com um compasso por acorde e um `RhythmProfile` vazio, o que faz os geradores
  caírem no próprio feel em vez de tentar copiar um groove que não existe.

Os presets (`core/src/Presets.cpp`) são guardados em graus justamente para poder
ser transportados. Aplicar um preset mantém a tônica atual e adota o modo do
preset — `vi IV I V` não significa nada numa tonalidade menor.

## Condução de vozes

`voiceChord()` não empilha o acorde na posição fundamental. Ele gera **todas** as
inversões em **todas** as oitavas que cabem no registro e escolhe a de menor
custo:

```
custo = Σ distância de cada voz até a nota mais próxima do acorde anterior
      + 0,6 × deslocamento do centro do acorde
      + 0,25 × excesso de abertura acima de duas oitavas
```

Na prática: `Am7 → Fmaj7 → Cmaj7 → G7` sai como
`E G A C → E F A C → E G B C → F G B D`. Uma ou duas vozes se mexem por acorde.

Extensões passam por `extendChord()`, que só usa a sétima **que já existe na
tonalidade** (via `diatonicSevenths`), e só põe nona se a nona for diatônica.
Sem isso, um Sol maior em Lá menor viraria Gmaj7 e traria um Fá# que não existe
na música.

## Geradores

Todos partem de `Analysis::progression` e de uma grade de semicolcheias.
`buildOnsets()` sorteia ataques combinando o peso posicional (tempo forte pesa
1,0; contratempo, 0,28) com a grade rítmica do clipe original. A influência do
clipe é proporcional a quanta informação ele tem: um pad com um ataque por
compasso quase não influencia; um baixo sincopado influencia muito.

A melodia é construída sobre um motivo curto de graus da escala, repetido em
frases de quatro compassos no formato A A' B A''. Em tempo forte a nota é
puxada para nota do acorde; nos tempos fracos, para nota da escala. Saltos
maiores que uma oitava são cortados, repetições de três notas iguais são
quebradas, e a última nota resolve na fundamental ou na terça.

O contracanto é a mesma engrenagem com duas restrições: prefere movimento
contrário ao do clipe e nunca toca a mesma nota que ele — inclusive depois do
ajuste da cadência final, que é onde a checagem ingênua escapava.

Tudo é determinístico: mesma semente (`Rng`, um xorshift de 32 bits) e mesmos
parâmetros produzem exatamente o mesmo MIDI. É por isso que basta salvar a
semente no estado do plugin em vez do resultado inteiro.

## A biblioteca de MIDIs

`core/src/Library.cpp` percorre uma árvore de pastas com `std::filesystem`,
analisa cada `.mid` e monta um `LibraryIndex`. Cada entrada guarda tonalidade,
andamento, compassos, progressão, papel detectado, papel deduzido do nome da
pasta (`bass`, `pluck`, `arp`, `drums`, …), as pastas como etiquetas, e o
`RhythmProfile` completo — inclusive a grade de 16 fatias.

Guardar a grade é o que permite o **doador de groove**: `GenerateOptions` aceita
um `RhythmProfile` vindo de outro clipe, e os geradores escrevem contra ele em
vez do groove da fonte. Como o pedido é explícito, a influência do doador sobe
para 0,9 (contra 0,6 do "seguir a fonte"), então o resultado realmente soa como
o clipe doado.

Percussão é detectada por canal 10 e pelo nome da pasta, e fica marcada para
poder ser filtrada — não tem harmonia para contribuir.

O índice é JSON, escrito e lido por `core/src/Json.cpp` (um parser mínimo,
~300 linhas, para não trazer dependência). Dá para abrir no editor, versionar e
processar com `jq`.

## O modelo de estilo

`core/src/StyleModel.cpp` é a camada que aprende. O `scan` já lê cada arquivo
uma vez, então o aprendizado acontece na mesma passada (`scanDirectory` aceita
um `StyleModel*`) — reler um HD inteiro seria a parte cara.

De cada clipe saem cinco coisas:

1. **Um `RhythmPattern` por compasso** — máscara de 16 bits com os ataques, mais
   velocity e duração por posição. Vai para o banco do papel do clipe (o nome da
   pasta ganha da detecção: `.../Plucks/` vira `pluck` mesmo que a análise diga
   `lead`). Compassos iguais viram o mesmo registro com o contador somado.
2. **Passos de escala** — `absoluteDegree()` converte a altura em grau contando
   as oitavas (uma oitava = 7 passos), e o que se guarda são as *diferenças*.
   Por serem graus, transportam para qualquer tom; notas cromáticas quebram a
   cadeia em vez de virar ruído. Guarda-se a marginal e a transição de ordem 1.
3. **Intervalos sobre a fundamental**, separados pela força métrica da
   semicolcheia (`slotClassFor`: 0 = cabeça do compasso, 1 = tempo, 2 =
   contratempo de colcheia, 3 = de semicolcheia).
4. **Espaçamentos de acorde** — as vozes soando no início de cada segmento,
   medidas a partir da fundamental abaixo da voz mais grave, para que inversões
   mantenham o formato.
5. **A progressão** em graus.

Nada disso é uma frase. São contagens, e por isso o modelo é pequeno,
transportável e não carrega material de ninguém.

### Como os geradores consultam o modelo

- `buildOnsets()` sorteia um compasso do banco do papel em vez de sortear
  semicolcheia por semicolcheia. A escolha é ponderada por `sqrt(frequência)`
  vezes uma gaussiana sobre a distância entre a quantidade de ataques do
  compasso e a que a knob **Density** pediu — assim o controle continua
  significando alguma coisa.
- A melodia troca a tabela fixa de passos por `sampleStep(modelo, passoAnterior)`.
- O baixo ganha um caminho próprio (`writeLearnedBass`): compassos do banco e
  intervalo sobre a fundamental sorteado pela classe métrica da posição. Um
  guarda-corpo puxa de volta para a fundamental a nota que não pertence nem ao
  acorde nem à tonalidade, em 70% dos casos.
- Pads e acordes usam `learnedVoicing()`: o espaçamento vem do corpus, é
  deslocado por oitavas até caber no registro e então **cada voz é encaixada na
  nota do acorde mais próxima**. É isso que impede um molde de acorde maior de
  arrastar uma terça maior para cima de um acorde menor.

Tudo tem plano B: sem corpus, ou com `styleAmount` baixo, cada gerador volta
para a heurística embutida. E `useStyle()` consulta o RNG, então a mesma semente
com e sem modelo produz resultados diferentes — o que é o esperado.

### Onde o modelo mora

Um plugin que guarda o caminho `/Volumes/KINGSTON/...` dentro do projeto quebra
na primeira vez que o HD não está montado. Então o plugin guarda o **modelo**,
não o caminho. `plugin/Source/StyleStore.cpp` resolve três origens, nesta ordem:

1. **Sessão** — um `.style.json` carregado só nesta instância. É a única origem
   que vai para o estado do projeto, porque é a única que o projeto conhece.
2. **Instalado** — `~/Library/Application Support/Nowhr Dynamics/Progressions/`
   `library.style.json` (`~/.config/nowhr-dynamics/progressions` no Linux,
   `%APPDATA%\Nowhr Dynamics\Progressions` no Windows). Escolher um modelo
   no editor copia para lá, e toda instância nova nasce com ele.
3. **Compilado** — `-DHARMONIA_STYLE_MODEL=<arquivo>` transforma o JSON em bytes
   dentro do binário via `juce_add_binary_data`, e `parseStyleModel()` lê direto
   da memória, sem passar por disco. É o que torna o `.vst3` autossuficiente.

`install()` faz o parse antes de copiar: um modelo inválido instalado seria um
erro em toda instância futura, não só nesta. E abrir um projeto nunca reescreve
o modelo instalado — carregar do estado passa `install = false`.

O tamanho não é um problema porque `prune()` limita o modelo (256 padrões por
papel, 128 voicings, 200 progressões), então 227 mil arquivos e 200 arquivos
produzem um JSON da mesma ordem de grandeza.

### Limites

O modelo é global: ele mistura tudo que foi escaneado numa passada. Para separar
Deep House de Melodic House, escaneie as pastas separadamente e carregue o
`.style.json` de cada uma. Ordem 1 na cadeia de passos é uma escolha de robustez
— ordem 2 precisaria de um corpus bem maior para não decorar.

## A camada do plugin

`ProgressionsProcessor` guarda o `harmonia::Engine`, os parâmetros (APVTS) e o
resultado. Gerar é caro e acontece na thread de mensagens; o resultado vira um
`RenderedPart` (eventos em semínimas, `ReferenceCountedObject`) publicado sob
`SpinLock`. A thread de áudio faz `tryEnter` e, no pior caso, deixa passar um
bloco — nunca bloqueia.

O `processBlock` posiciona os eventos por PPQ, então funciona igual seguindo o
transporte do host ou o transporte interno, e o loop é feito por módulo sobre
`lengthPPQ` (com o bloco quebrado em dois pedaços quando cruza a volta do loop).

`plugin/tests/PluginSmokeTest.cpp` roda tudo isso sem DAW e sem servidor
gráfico: instancia o processador, toca 400 blocos, confere que todo note-on tem
note-off, faz round-trip do estado e renderiza o editor num PNG.
