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

---

## Compartilhar

O jeito fácil: **Actions → o build mais recente → Artifacts** no GitHub, e mande
o `.pkg` (Mac) ou o `.exe` (Windows). Eles instalam sozinhos.

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
