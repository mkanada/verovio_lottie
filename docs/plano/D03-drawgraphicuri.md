# D03 — `DrawGraphicUri` (imagens raster)

**Depende de:** — · **Decisão necessária:** SIM — pare e pergunte ao usuário
antes de escrever qualquer código (ver "Decisão necessária" abaixo). É a
mesma pergunta que D00 já deixou em aberto ("exigiria assets de imagem no
pacote — confirmar com o usuário se vale a pena").

## Objetivo (se aprovado)

Implementar `LottieDeviceContext::DrawGraphicUri` (hoje stub vazio,
`verovio/src/lottiedevicecontext.cpp:526`), correspondente ao elemento MEI
`<graphic target="...">` — uma referência a um arquivo de imagem raster
(não um `<svg>` embutido, que é D02).

**Nenhuma peça do corpus atual usa `<graphic>`** (confirmado:
`grep -l "<graphic" corpus/mei/*.mei` não bate nada) e o levantamento de
elementos do `README.md` também mostra `image 0` ocorrências nos SVGs
gerados. Isto é: este passo não tem nenhuma cobertura real no corpus de
teste hoje — todo o trabalho seria especulativo até aparecer uma peça real
que use isso.

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L1219-1227 (`SvgDeviceContext::DrawGraphicUri` —
  só escreve um `<image xlink:href="...">`; a resolução do `target` fica a
  cargo de quem abre o SVG depois, o Verovio nunca toca os bytes da
  imagem).
- `verovio/src/view_text.cpp` L526-545 (`View::DrawGraphic`, único
  chamador) — `graphic->GetTarget()` é o valor cru do atributo MEI
  `@target` (pode ser caminho relativo, absoluto, ou URL http(s); o Verovio
  não valida/normaliza isso hoje).
- `verovio/src/filereader.cpp` L140-143 (`ZipFileWriter::AddFile`) — se a
  decisão for embutir, é o mesmo mecanismo de D01 (fonts) e do restante do
  pacote.
- `docs/plano/README.md`, "Referência rápida: pacote dotLottie v2" — hoje só
  documenta `f/<arquivo>.ttf` (fontes); um asset de imagem dotLottie v2
  normalmente vive em `i/<arquivo>` referenciado por um layer `"ty":2`
  (`u`/`p` no asset, `origin` análogo ao de fontes) — **não verificado por
  spike nesta sessão**, só por analogia com o padrão de fontes; confirmar
  contra o `dotlottie-rs` vendorizado antes de implementar, mesmo aviso que
  D01 já faz para texto.

## Decisão necessária

Perguntar ao usuário, quando este passo for de fato pego para execução:

1. **Vale a pena implementar, dado que o corpus atual não tem nenhum caso
   de uso real?** Alternativa: deixar `DrawGraphicUri` como stub
   permanentemente (documentar como limitação conhecida) até uma peça real
   do zywny precisar disso.
2. **Se sim, que `target` resolver?**
   - **Só caminho local** (relativo ao MEI/`--resource-path` de entrada):
     ler o arquivo do disco no momento da exportação e embutir no pacote
     (`i/<nome>`), como as fontes de D01. Mais simples, sem I/O de rede,
     mas não cobre `target` que seja uma URL http(s).
   - **URL http(s) também**: exigiria buscar a imagem pela rede durante a
     exportação (`verovio -t dotlottie`), o que é uma mudança de
     comportamento maior (I/O de rede numa ferramenta de linha de comando
     que hoje é 100% local/determinística) — teria custo de tempo de
     execução, falhas de rede a tratar, e possivelmente um cache. Precisa
     de aprovação explícita antes de escrever qualquer código de rede
     (natureza da mudança listada nas instruções deste projeto).
   - **Não resolver, só referenciar**: escrever a referência do jeito que o
     SVG já faz (deixar `target` como está) e esperar que o **player** do
     zywny resolva/baixe a imagem em tempo de execução — mais parecido com
     o comportamento do SVG hoje, mas precisa confirmar se o
     `dotlottie-rs`/ThorVG (ou o runtime real do zywny, ainda não escolhido —
     ver risco 1 do `README.md`) suporta um asset de imagem "externo"
     (`origin` != embutido) apontando pra uma URL, em vez de sempre exigir
     bytes dentro do pacote.

## Fora de escopo (independente da decisão acima)

Qualquer processamento de imagem (redimensionar, recomprimir, converter
formato) — se embutido, os bytes originais vão direto pro pacote, igual ao
`.ttf` de D01.

## Critérios de aceite (se aprovado, a preencher conforme a decisão)

Ficam bloqueados até a decisão acima. Se aprovado com resolução local:

- Criar um MEI mínimo em `compare/out/` com `<graphic target="...">`
  apontando pra um PNG/JPEG pequeno de teste.
- `compare/scripts/compare-page.sh` mostra a imagem na posição/tamanho
  certos no PNG do Lottie comparado ao do SVG.
- `unzip -l` no pacote gerado confirma o asset de imagem embutido.

## Decisão (2026-09-13)

Usuário optou por **não implementar agora**: `DrawGraphicUri` continua como
stub vazio (`verovio/src/lottiedevicecontext.cpp:526`), documentado aqui como
limitação conhecida. Nenhuma peça do corpus atual usa `<graphic>`, então o
trabalho seria especulativo. Revisitar quando uma peça real do zywny
precisar de imagem raster embutida — nesse momento, refazer a pergunta de
"Decisão necessária" acima (resolução local vs. URL vs. referência não
resolvida) antes de escrever qualquer código.
