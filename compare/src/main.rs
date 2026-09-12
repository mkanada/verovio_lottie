//! Ferramentas de comparação visual entre a saída SVG e dotLottie do verovio_lottie.
//!
//! Ver `compare/README.md` para o fluxo de uso.

use std::ffi::CString;
use std::path::{Path, PathBuf};

use anyhow::{bail, Context, Result};
use clap::{Parser, Subcommand};

#[derive(Parser)]
#[command(
    name = "compare",
    about = "Renderiza SVG e Lottie/dotLottie para PNG e compara os resultados pixel a pixel"
)]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Renderiza um SVG (ex.: saída `verovio -t svg`) para PNG usando resvg.
    SvgToPng {
        input: PathBuf,
        output: PathBuf,
    },
    /// Renderiza um frame de uma animação Lottie (.json) ou dotLottie (.lottie) para PNG.
    LottieToPng {
        input: PathBuf,
        output: PathBuf,
        /// Largura do canvas de renderização, em pixels.
        #[arg(long)]
        width: u32,
        /// Altura do canvas de renderização, em pixels.
        #[arg(long)]
        height: u32,
        /// Número do frame a renderizar (0 = primeiro frame / pose "parada").
        #[arg(long, default_value_t = 0.0)]
        frame: f32,
    },
    /// Compara dois PNGs pixel a pixel e gera uma imagem de diferença.
    Diff {
        a: PathBuf,
        b: PathBuf,
        output: PathBuf,
        /// Diferença máxima por canal (0-255) tolerada antes de marcar o pixel como diferente.
        #[arg(long, default_value_t = 0)]
        tolerance: u8,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Command::SvgToPng { input, output } => svg_to_png(&input, &output),
        Command::LottieToPng {
            input,
            output,
            width,
            height,
            frame,
        } => lottie_to_png(&input, &output, width, height, frame),
        Command::Diff {
            a,
            b,
            output,
            tolerance,
        } => diff(&a, &b, &output, tolerance),
    }
}

fn svg_to_png(input: &Path, output: &Path) -> Result<()> {
    let svg_data =
        std::fs::read(input).with_context(|| format!("lendo {}", input.display()))?;

    let mut opt = usvg::Options {
        resources_dir: input
            .canonicalize()
            .ok()
            .and_then(|p| p.parent().map(|p| p.to_path_buf())),
        ..usvg::Options::default()
    };
    // O Verovio usa fontes vetorizadas em <defs>/<use>, mas carregamos as fontes
    // do sistema também para não falhar em SVGs com <text> (ex.: letra de música).
    opt.fontdb_mut().load_system_fonts();

    let tree = usvg::Tree::from_data(&svg_data, &opt)
        .with_context(|| format!("interpretando SVG {}", input.display()))?;

    let size = tree.size().to_int_size();
    let mut pixmap = tiny_skia::Pixmap::new(size.width(), size.height())
        .context("SVG com dimensões inválidas (0x0)")?;
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

fn lottie_to_png(input: &Path, output: &Path, width: u32, height: u32, frame: f32) -> Result<()> {
    if width == 0 || height == 0 {
        bail!("--width e --height precisam ser maiores que zero");
    }

    let mut player = dotlottie_rs::Player::new();

    // O destino de renderização precisa existir ANTES de carregar a animação —
    // dotlottie-rs (ver examples/simple_player.rs) inicializa o renderer contra
    // o alvo já configurado; carregar antes faz o load "ter sucesso" (retorna
    // Ok) mas sem nenhuma animação de fato ativa no renderer.
    let mut buffer: Vec<u32> = vec![0; (width as usize) * (height as usize)];
    player
        .set_sw_target(&mut buffer, width, height, dotlottie_rs::ColorSpace::ARGB8888)
        .context("configurando destino de renderização por software")?;

    match input.extension().and_then(|e| e.to_str()) {
        Some("lottie") => {
            let data =
                std::fs::read(input).with_context(|| format!("lendo {}", input.display()))?;
            player
                .load_dotlottie_data(&data)
                .with_context(|| format!("carregando dotLottie {}", input.display()))?;
        }
        Some("json") => {
            let data = std::fs::read_to_string(input)
                .with_context(|| format!("lendo {}", input.display()))?;
            let c_data = CString::new(data).context("JSON contém byte nulo")?;
            player
                .load_animation_data(&c_data)
                .with_context(|| format!("carregando Lottie JSON {}", input.display()))?;
        }
        other => bail!(
            "extensão não suportada ({:?}); use .lottie ou .json",
            other
        ),
    }

    if player.total_frames() <= 0.0 {
        bail!("nenhuma animação carregada — verifique se o arquivo é um Lottie/dotLottie válido");
    }

    // O load já deixa o buffer com o frame inicial renderizado (dotlottie-rs
    // faz isso internamente: ver `Player::load_animation_common`, que chama
    // set_frame()+render() ignorando erros de propósito). set_frame()/render()
    // aqui só têm efeito real quando `frame` difere do frame inicial; quando
    // coincide, ThorVG trata como no-op e ambos retornam erro — inofensivo,
    // pois o buffer já está correto. Por isso só avisamos, sem abortar.
    if let Err(e) = player.set_frame(frame) {
        eprintln!("aviso: set_frame({frame}) não teve efeito ({e}) — pode ser um no-op inofensivo se já era o frame corrente");
    }
    if let Err(e) = player.render() {
        eprintln!("aviso: render() não teve efeito ({e}) — pode ser um no-op inofensivo se nada mudou desde o load");
    }

    let mut img = image::RgbaImage::new(width, height);
    for (i, px) in buffer.iter().enumerate() {
        let x = (i as u32) % width;
        let y = (i as u32) / width;
        let a = ((px >> 24) & 0xFF) as u8;
        let r = ((px >> 16) & 0xFF) as u8;
        let g = ((px >> 8) & 0xFF) as u8;
        let b = (px & 0xFF) as u8;
        img.put_pixel(x, y, image::Rgba([r, g, b, a]));
    }
    img.save(output)
        .with_context(|| format!("salvando {}", output.display()))?;

    println!(
        "PNG salvo em {} ({}x{}, frame {})",
        output.display(),
        width,
        height,
        frame
    );
    Ok(())
}

fn diff(a_path: &Path, b_path: &Path, output: &Path, tolerance: u8) -> Result<()> {
    let a = image::open(a_path)
        .with_context(|| format!("abrindo {}", a_path.display()))?
        .to_rgba8();
    let b = image::open(b_path)
        .with_context(|| format!("abrindo {}", b_path.display()))?
        .to_rgba8();

    if a.dimensions() != b.dimensions() {
        bail!(
            "dimensões diferentes: {} é {}x{}, {} é {}x{} — renderize os dois no mesmo tamanho antes de comparar",
            a_path.display(),
            a.width(),
            a.height(),
            b_path.display(),
            b.width(),
            b.height(),
        );
    }

    let (width, height) = a.dimensions();
    let mut out = image::RgbaImage::new(width, height);
    let mut diff_pixels: u64 = 0;
    let mut max_diff: u8 = 0;

    for y in 0..height {
        for x in 0..width {
            let pa = a.get_pixel(x, y);
            let pb = b.get_pixel(x, y);
            let d = channel_diff(pa, pb);
            max_diff = max_diff.max(d);

            if d > tolerance {
                diff_pixels += 1;
                out.put_pixel(x, y, image::Rgba([255, 0, 0, 255]));
            } else {
                // Fundo em tons de cinza esmaecido (a partir da imagem A) para dar contexto
                // visual a quem for inspecionar a imagem de diferença.
                let l = ((pa[0] as u32 + pa[1] as u32 + pa[2] as u32) / 3) as u8;
                let dimmed = 255 - ((255 - l) / 3);
                out.put_pixel(x, y, image::Rgba([dimmed, dimmed, dimmed, 255]));
            }
        }
    }

    out.save(output)
        .with_context(|| format!("salvando {}", output.display()))?;

    let total = (width as u64) * (height as u64);
    let pct = 100.0 * diff_pixels as f64 / total as f64;
    println!("Pixels comparados: {total}");
    println!("Pixels diferentes (tolerância {tolerance}): {diff_pixels} ({pct:.4}%)");
    println!("Maior diferença de canal observada: {max_diff}");
    println!("Imagem de diferença salva em {}", output.display());

    Ok(())
}

fn channel_diff(a: &image::Rgba<u8>, b: &image::Rgba<u8>) -> u8 {
    a.0.iter()
        .zip(b.0.iter())
        .map(|(x, y)| x.abs_diff(*y))
        .max()
        .unwrap_or(0)
}

#[cfg(test)]
mod smoke_test {
    /// Lottie JSON mínimo válido: um único frame com um círculo vermelho.
    /// Serve só para provar que o pipeline nativo dotlottie-rs/ThorVG está
    /// linkado e funcionando neste ambiente (renderer de software).
    const MINIMAL_LOTTIE_JSON: &str = r#"{
        "v": "5.5.2", "fr": 30, "ip": 0, "op": 30, "w": 200, "h": 200,
        "nm": "test", "ddd": 0, "assets": [],
        "layers": [{
            "ddd": 0, "ind": 1, "ty": 4, "nm": "shape", "sr": 1,
            "ks": {
                "o": {"a": 0, "k": 100}, "r": {"a": 0, "k": 0},
                "p": {"a": 0, "k": [100, 100, 0]}, "a": {"a": 0, "k": [0, 0, 0]},
                "s": {"a": 0, "k": [100, 100, 100]}
            },
            "ao": 0,
            "shapes": [{
                "ty": "gr",
                "it": [
                    {"ty": "el", "p": {"a": 0, "k": [0, 0]}, "s": {"a": 0, "k": [80, 80]}},
                    {"ty": "fl", "c": {"a": 0, "k": [1, 0, 0, 1]}, "o": {"a": 0, "k": 100}},
                    {"ty": "tr", "p": {"a": 0, "k": [0, 0]}, "a": {"a": 0, "k": [0, 0]},
                     "s": {"a": 0, "k": [100, 100]}, "r": {"a": 0, "k": 0}, "o": {"a": 0, "k": 100}}
                ]
            }],
            "ip": 0, "op": 30, "st": 0
        }]
    }"#;

    #[test]
    fn dotlottie_rs_render_smoke_test() {
        let mut player = dotlottie_rs::Player::new();
        let mut buffer = vec![0u32; 128 * 128];
        player
            .set_sw_target(&mut buffer, 128, 128, dotlottie_rs::ColorSpace::ABGR8888)
            .unwrap();
        let data = std::ffi::CString::new(MINIMAL_LOTTIE_JSON).unwrap();
        player.load_animation_data(&data).unwrap();
        assert!(
            buffer.iter().any(|&px| px != 0),
            "nada foi desenhado no buffer (load já deveria renderizar o frame inicial)"
        );
    }
}
