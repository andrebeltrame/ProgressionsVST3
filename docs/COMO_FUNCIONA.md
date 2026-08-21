# Como o Harmonia funciona por dentro

Documento para quem for mexer no código. Todo o miolo está em `core/`, sem
nenhuma dependência externa — dá para compilar e testar em segundos.

## O caminho de um clipe

```
arquivo .mid
   │
   ├─ midi::readFromMemory       core/src/MidiFile.cpp
   │     junta todas as tracks numa NoteSequence e reescala para 960 PPQ
   │
   ├─ analyze()                  core/src/Analysis.cpp
   │     ├─ pitchClassHistogram  peso = duração × velocidade
   │     ├─ detectKey            perfis de Temperley + âncora nas bordas
   │     ├─ buildRhythmProfile   grade de 16 fatias, densidade, polifonia
   │     ├─ detectRole           Bass / Chords / Lead / Arp
   │     └─ Viterbi              acorde por quadro, depois junta em segmentos
   │
   └─ generate()                 core/src/Generators.cpp
         escreve a parte pedida em cima de Analysis::progression
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

No fim, quadros iguais viram `ChordSegment`, e um segmento curto espremido
entre dois acordes idênticos é absorvido (nota de passagem).

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

## A camada do plugin

`HarmoniaProcessor` guarda o `harmonia::Engine`, os parâmetros (APVTS) e o
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
