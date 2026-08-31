# Comandos

Referência de bolso. Tudo aqui é uma linha só — cole uma de cada vez, o zsh
engasga com blocos de várias linhas.

O projeto vive em `~/harmonia`. Se você abriu o terminal agora:

```bash
cd ~/harmonia
```

---

## Atualizar

O ciclo completo, na ordem. É o que você roda sempre que eu avisar que subi
alguma coisa.

```bash
cd ~/harmonia && git pull
```

O motor e a linha de comando (segundos):

```bash
cmake --build build -j
```

Apague o plugin instalado antes de recompilar — a cópia pós-build funde
arquivos em vez de substituir a pasta, e restos de build antiga quebram a
assinatura:

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/"Nowhr Dynamics"/Progressions.vst3
```

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DHARMONIA_BUILD_PLUGIN=ON -DHARMONIA_INSTALL_PLUGIN=ON
```

```bash
cmake --build build-plugin -j
```

De 5 a 10 minutos, e fica quieto no fim durante o link — não é travamento.
Ele se instala sozinho em `Nowhr Dynamics/` no final.

Depois, no Live: **Cmd+Q** para fechar o programa inteiro (ele mantém o plugin
em memória), reabrir, **Preferences → Plug-Ins**, Option + **Rescan**.

---

## Conferir que deu certo

A assinatura. Tem que ficar **muda** — qualquer saída significa que o Live não
vai carregar:

```bash
codesign -v --strict ~/Library/Audio/Plug-Ins/VST3/"Nowhr Dynamics"/Progressions.vst3
```

O que está instalado:

```bash
find ~/Library/Audio/Plug-Ins/VST3 -maxdepth 3 -name "*.vst3"
```

Para quais Macs o binário serve:

```bash
lipo -archs ~/Library/Audio/Plug-Ins/VST3/"Nowhr Dynamics"/Progressions.vst3/Contents/MacOS/Progressions
```

Se a cópia automática não tiver acontecido, force:

```bash
ditto ~/harmonia/build-plugin/plugin/Progressions_artefacts/Release/VST3/Progressions.vst3 ~/Library/Audio/Plug-Ins/VST3/"Nowhr Dynamics"/Progressions.vst3
```

---

## Testar sem o DAW

```bash
open ~/harmonia/build-plugin/plugin/Progressions_artefacts/Release/Standalone/Progressions.app
```

---

## O cérebro, pela linha de comando

O plugin faz tudo isso sozinho pelo menu **Learn from my library…**. A linha de
comando serve para quando você quer vários cérebros separados por gênero.

Catalogar um drive sem aprender nada (mais rápido):

```bash
./build/cli/harmonia-cli scan /Volumes/KINGSTON --index ~/tudo.json --no-learn
```

Ver o que tem lá dentro:

```bash
./build/cli/harmonia-cli library --index ~/tudo.json --tags
```

Montar um cérebro focado e entregar direto ao plugin:

```bash
./build/cli/harmonia-cli learn --index ~/tudo.json --tag "melodic house" --style ~/melodic.style.json --install
```

Outro, só de baixo, numa faixa de andamento:

```bash
./build/cli/harmonia-cli learn --index ~/tudo.json --role bass --bpm 120-126 --style ~/bass.style.json
```

Ver as progressões que a sua coleção mais usa:

```bash
./build/cli/harmonia-cli library --index ~/tudo.json --progressions
```

Listar os 36 presets:

```bash
./build/cli/harmonia-cli presets
```

Gerar ideias sem abrir nada:

```bash
./build/cli/harmonia-cli --preset melodic-lift --key "F minor" --style ~/melodic.style.json --part bass,melody,pluck --out ~/ideias
```

O Reese sai com as notas encavaladas, que é o que faz o portamento de um synth
mono funcionar. Num patch **polifônico** isso soma duas notas graves ao mesmo
tempo — se for o seu caso, peça sem sobreposição:

```bash
./build/cli/harmonia-cli resources/examples/bass_loop.mid --part reese --no-glide --out ~/ideias
```

O **Pattern** inventa uma figura e a repete, com os acordes passando por baixo.
O `--follow-chords` decide o quanto ela cede à harmonia — 0 mantém a figura
intacta, 1 puxa tudo para as notas do acorde e vira arpejo:

```bash
./build/cli/harmonia-cli --progression "Am | F | C | G" --key "A minor" --part pattern --follow-chords 0.2 --out ~/ideias
```

O **Sub** é outra parte, não um Reese mais grave: fundamental do acorde, uma
oitava só, nunca duas notas juntas. O `--density` decide se ele segura ou repete:

```bash
./build/cli/harmonia-cli resources/examples/bass_loop.mid --part sub --density 0.9 --out ~/ideias
```

---

## Compartilhar

O jeito fácil: mande o link das
[Releases](https://github.com/andrebeltrame/ProgressionsVST3/releases). O zip tem duas
coisas: o plugin, que serve para Mac e Windows, e o `INSTALL.txt` dizendo onde
colocá-lo. O `install-windows.ps1` fica ao lado na mesma página, para quem
esbarrar em problema.

Se preferir empacotar o que está na sua máquina:

```bash
ditto -c -k --keepParent ~/Library/Audio/Plug-Ins/VST3/"Nowhr Dynamics" ~/Desktop/Progressions.zip
```

Mande esta frase junto, senão a pessoa não consegue abrir:

> Na primeira vez, use botão direito → Abrir (Mac) ou "Mais informações →
> Executar assim mesmo" (Windows). O plugin é meu, só não está registrado nas
> lojas da Apple e da Microsoft ainda.

Publicar uma versão com link próprio:

```bash
cd ~/harmonia && git tag v1.0.0 && git push origin v1.0.0
```

---

## Quando alguma coisa some

O plugin não aparece no Live. Na ordem:

1. Fechou o Live com **Cmd+Q** e reabriu? Ele guarda o plugin em memória.
2. Fez **Rescan** segurando **Option**? O normal reaproveita cache.
3. O `codesign -v --strict` ficou mudo? Se falou alguma coisa, é isso.
4. Está procurando em **Instruments**? Ele é um instrumento, não um efeito.

Reinstalar do zero:

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/"Nowhr Dynamics" && rm -rf ~/harmonia/build-plugin
```

E aí repita a seção **Atualizar** desde o `cmake -S . -B build-plugin`.

Apagar o cérebro e começar de novo (o botão **Forget this library** faz o
mesmo):

```bash
rm -f ~/Library/Application\ Support/Nowhr\ Dynamics/Progressions/library.style.json
```

## Montar o zip no Mac quando o CI não consegue

Os dois binários vêm do CI, mas a junção é local. Baixe da página do run os
artefatos `macos-build` e `windows-build`, e rode:

```
cd ~/Downloads
unzip -o macos-build.zip
unzip -o windows-build.zip
cd ~/harmonia
packaging/merge.sh ~/Downloads/macos-bundle.zip ~/Downloads/Progressions.vst3 ~/Desktop
```

Saem duas coisas no Desktop: `Progressions-<versão>.zip`, com o plugin e o
`INSTALL.txt` dentro, e o `install-windows.ps1` ao lado. O script confere
o que montou: Mach-O universal com as duas
arquiteturas, PE32+ do lado Windows, e `codesign -v --strict`.

A assinatura tem que vir **depois** de colocar o binário Windows dentro: mexer
num bundle já assinado quebra o selo, e um selo quebrado o macOS recusa sem
dizer nada — o plugin simplesmente não aparece na lista do DAW.

## Roteamento no Ableton Live

O Live **descarta o canal MIDI** ao rotear de um plugin para outra pista: canal
só tem significado na entrada e na saída de uma pista, não dentro dela. Os
canais por parte do Progressions servem em Cubase, Reaper e Bitwig; no Live,
use um destes dois caminhos.

**Arrastar cada parte (recomendado).** Cada parte vira um clipe independente na
pista do instrumento dela.

1. Na janela do Progressions, escolha a aba da parte (Pad).
2. Arraste **Drag MIDI to your DAW** para a pista que tem o instrumento do pad.
3. Clique na aba seguinte (Bass) e arraste para a pista do baixo. E assim por
   diante.

**Saída MIDI ao vivo, uma parte por vez.**

1. Desligue **Play all sequences** no Progressions — senão as partes chegam somadas.
2. Na pista do instrumento de destino: *MIDI From* → a pista do Progressions.
3. No seletor logo abaixo, escolha **Progressions** (o plugin), e não
   *Track In* / *Pre FX* / *Post FX*.
4. Monitor em **In**.
5. Desligue **Preview sound** no plugin, senão você ouve os dois.

## Gerar os binários no CI

Os builds de macOS e Windows **não** rodam mais a cada push: o runner do macOS
custa 10× o tempo real e o do Windows 2×, e um dia de commits normais esgotou a
cota mensal da conta. Quando a cota acaba, todo job falha em segundos sem runner
e sem log — parece exatamente com o código estar quebrado.

O que roda em todo push são os testes do motor, que são Linux e custam o que
dizem. Os binários você pede quando quer:

**Na aba Actions** → workflow **Build** → botão **Run workflow** → escolha o
branch → **Run workflow**. Sai `macos-build` e `windows-build` como artefatos.

**Ou empurrando uma tag**, que além dos binários publica o release com o zip:

```bash
git tag -a v1.1.0 -m "Progressions 1.1.0" && git push origin v1.1.0
```

Se os jobs falharem em segundos, com `runner_id: 0` e 404 nos logs, é cota —
veja Settings → Billing and licensing → Plans and usage → Actions.
