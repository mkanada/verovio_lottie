//! Renderiza um SVG (ex.: saída `verovio -t svg`) para PNG usando `resvg`.
//!
//! Extraído do `compare` original (Rust, removido no commit que reescreveu a
//! ferramenta em Flutter) porque o `flutter_svg`/Impeller mostrou limitações
//! demais como renderizador de referência: não suporta `<svg>` aninhado
//! (precisa de pré-processamento pra achatar), erra posição/tamanho de texto
//! sob um ancestral com `transform`, e (achado real, ver
//! `docs/plano/decisoes/B03-texto.md`) desenha um glifo `<use>` repetido
//! gigante e fora de lugar em pelo menos uma peça do corpus — bug do
//! `flutter_svg`/Impeller, não do SVG em si (confirmado renderizando o mesmo
//! arquivo, sem nenhum pré-processamento, no Chrome headless). O `resvg` não
//! tem nenhuma dessas limitações (suporta `<svg>` aninhado nativamente), só
//! as pegadinhas mais restritas documentadas abaixo.
//!
//! Ver `compare/README.md` para o fluxo de uso completo (este binário cobre
//! só o passo `svg-to-png`; os demais — `lottie-to-png`, `diff`, `sm-render`
//! — continuam no `compare` em Dart/Flutter, que não tem motivo pra trocar:
//! o lado Lottie usa FFI direto no `libdotlottie_rs.so`, não o Impeller).

use std::path::{Path, PathBuf};

use anyhow::{Context, Result};
use clap::Parser;

#[derive(Parser)]
#[command(
    name = "svg_render",
    about = "Renderiza um SVG (saída do Verovio) para PNG usando resvg"
)]
struct Cli {
    input: PathBuf,
    output: PathBuf,
    /// Arquivo de fonte adicional a carregar (repetível). Use para as fontes
    /// do Verovio (ex.: Liberation Serif, Leipzig, Bravura) quando o SVG
    /// referencia texto/glifos por font-family — o resvg não carrega
    /// `@font-face` embutido (woff2) sozinho.
    #[arg(long = "font")]
    fonts: Vec<PathBuf>,
    /// Nome de família a que o genérico CSS "serif" deve resolver,
    /// independente do que o SO tem instalado (ver docs/plano/
    /// D01-2-controle-de-fonte-na-comparacao.md). Precisa bater o nome de
    /// família de uma fonte já carregada via --font.
    #[arg(long)]
    pin_serif_family: Option<String>,
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    svg_to_png(
        &cli.input,
        &cli.output,
        &cli.fonts,
        cli.pin_serif_family.as_deref(),
    )
}

/// Remove todo nó `<title>` do SVG antes do usvg processar — o resvg 0.48
/// inclui erroneamente o texto de `<title>` aninhado ao medir a largura para
/// `text-anchor`, mesmo esse elemento nunca sendo desenhado (ver
/// docs/plano/D01-3-titulo-aninhado-resvg.md). Se o SVG não for XML válido,
/// devolve o texto original inalterado (deixa o `usvg` reportar o erro).
fn strip_title_elements(svg: &str) -> String {
    let Ok(doc) = roxmltree::Document::parse(svg) else {
        return svg.to_string();
    };
    let mut ranges: Vec<_> = doc
        .descendants()
        .filter(|n| n.has_tag_name("title"))
        .map(|n| n.range())
        .collect();
    ranges.sort_by_key(|r| r.start);

    let mut result = String::with_capacity(svg.len());
    let mut last_end = 0;
    for range in ranges {
        result.push_str(&svg[last_end..range.start]);
        last_end = range.end;
    }
    result.push_str(&svg[last_end..]);
    result
}

fn svg_to_png(
    input: &Path,
    output: &Path,
    fonts: &[PathBuf],
    pin_serif_family: Option<&str>,
) -> Result<()> {
    let svg_data = std::fs::read(input).with_context(|| format!("lendo {}", input.display()))?;
    let svg_text = String::from_utf8(svg_data)
        .with_context(|| format!("SVG não é UTF-8 válido: {}", input.display()))?;
    // O resvg 0.48 mede erroneamente o texto de `<title>` aninhado ao resolver
    // `text-anchor` (elemento nunca desenhado por nenhum renderizador conforme
    // a spec) — ver docs/plano/D01-3-titulo-aninhado-resvg.md.
    let svg_text = strip_title_elements(&svg_text);

    let mut opt = usvg::Options {
        resources_dir: input
            .canonicalize()
            .ok()
            .and_then(|p| p.parent().map(|p| p.to_path_buf())),
        ..usvg::Options::default()
    };
    // O Verovio usa fontes vetorizadas em <defs>/<use> pra glifos SMuFL, mas
    // carregamos as fontes do sistema também pra não falhar em SVGs com
    // <text> comum sem as fontes do projeto explicitamente passadas.
    opt.fontdb_mut().load_system_fonts();
    // O SVG do Verovio referencia texto comum via font-family; as fontes do
    // projeto (Liberation Serif) precisam ser registradas explicitamente
    // pra uma comparação justa contra o Lottie renderizado (que as embute).
    for font in fonts {
        opt.fontdb_mut()
            .load_font_file(font)
            .with_context(|| format!("carregando fonte {}", font.display()))?;
    }
    // Sem isto, o genérico CSS "serif" (usado pelo Verovio como "Times, serif")
    // resolve via fontconfig do sistema operacional, que varia por ambiente —
    // ver docs/plano/D01-2-controle-de-fonte-na-comparacao.md.
    if let Some(family) = pin_serif_family {
        opt.fontdb_mut().set_serif_family(family);
    }

    let tree = usvg::Tree::from_data(svg_text.as_bytes(), &opt)
        .with_context(|| format!("interpretando SVG {}", input.display()))?;

    let size = tree.size().to_int_size();
    let mut pixmap = tiny_skia::Pixmap::new(size.width(), size.height())
        .context("SVG com dimensões inválidas (0x0)")?;
    // Fundo branco opaco antes de desenhar por cima: um `Pixmap` novo começa
    // totalmente transparente, e um PNG assim sai ilegível em visualizadores
    // com tema escuro.
    pixmap.fill(tiny_skia::Color::WHITE);
    resvg::render(&tree, tiny_skia::Transform::default(), &mut pixmap.as_mut());

    pixmap
        .save_png(output)
        .with_context(|| format!("salvando {}", output.display()))?;

    println!(
        "PNG salvo em {} ({}x{})",
        output.display(),
        size.width(),
        size.height()
    );
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strip_title_removes_nested_title_keeps_rest() {
        let svg = r#"<svg><text><tspan x="10" text-anchor="middle">
            <title class="labelAttr">rótulo bem longo</title>
            <tspan>texto de verdade</tspan></tspan></text></svg>"#;
        let out = strip_title_elements(svg);
        assert!(!out.contains("<title"));
        assert!(!out.contains("rótulo bem longo"));
        assert!(out.contains("texto de verdade"));
    }

    #[test]
    fn strip_title_without_title_is_unchanged() {
        let svg = r#"<svg><rect width="5"/></svg>"#;
        assert_eq!(strip_title_elements(svg), svg);
    }

    #[test]
    fn strip_title_invalid_xml_returns_original() {
        let svg = "<svg><unclosed>";
        assert_eq!(strip_title_elements(svg), svg);
    }
}
