//! Ferramentas de comparação visual entre a saída SVG e dotLottie do verovio_lottie.
//!
//! Ver `compare/README.md` para o fluxo de uso.

use std::ffi::CString;
use std::path::{Path, PathBuf};
use std::time::Instant;

use anyhow::{bail, Context, Result};
use clap::{Parser, Subcommand};
use dotlottie_rs::OpenUrlPolicy;

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
        /// Arquivo de fonte adicional a carregar (repetível). Use para as fontes do
        /// Verovio (ex.: Leipzig, Bravura) quando o SVG referencia glifos SMuFL via
        /// `@font-face` — o resvg não carrega esse `@font-face` embutido sozinho.
        #[arg(long = "font")]
        fonts: Vec<PathBuf>,
        /// Nome de família a que o genérico CSS "serif" deve resolver,
        /// independente do que o SO tem instalado (ver docs/plano/
        /// D01-2-controle-de-fonte-na-comparacao.md). Precisa bater o nome de
        /// família de uma fonte já carregada via --font.
        #[arg(long)]
        pin_serif_family: Option<String>,
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
        /// Slot de cor a sobrescrever, "id:r,g,b" (0-1 cada). Repetível. Ver
        /// `set_color_slot`/`src/renderer/slots/color.rs` no dotlottie-rs — não
        /// depende do frame atual (E6 de B01-spike-state-machine.md).
        #[arg(long = "slot")]
        slots: Vec<String>,
        /// Pixel "x,y" a amostrar e imprimir (RGBA) depois de renderizar. Repetível.
        #[arg(long = "sample")]
        samples: Vec<String>,
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
    /// Simula um host: carrega uma state machine v2 de um pacote dotLottie, dispara
    /// eventos por nome num roteiro de tempo e salva PNGs nos instantes pedidos.
    /// Ver docs/plano/B01-spike-state-machine.md.
    SmRender {
        /// Pacote .lottie de onde carregar a animação (e, com --sm, a state machine).
        input: PathBuf,
        /// Diretório onde salvar os PNGs de evidência.
        out_dir: PathBuf,
        /// Id da state machine dentro do pacote (`s/<id>.json` no manifest).
        #[arg(long, conflicts_with = "sm_file")]
        sm: Option<String>,
        /// Caminho para um JSON de state machine avulso (fora do pacote), para
        /// state machines sintéticas grandes demais para valer a pena empacotar.
        #[arg(long = "sm-file", conflicts_with = "sm")]
        sm_file: Option<PathBuf>,
        #[arg(long)]
        width: u32,
        #[arg(long)]
        height: u32,
        /// Roteiro "ms:ação;ms:ação;...". Ações: "fire nome" (state machine),
        /// "slot id:r,g,b" (Player::set_color_slot, 0-1 cada), "clearslot id"
        /// e "clearslots" (ver docs/plano/C03-slots-interativos.md sobre o
        /// handoff M2/M3). Vazio = nenhuma ação.
        #[arg(long, default_value = "")]
        script: String,
        /// Lista de instantes (ms) em que salvar um PNG, separados por vírgula.
        #[arg(long, default_value = "")]
        snap: String,
        /// Prefixo dos arquivos salvos em out_dir (<prefixo>-t<ms>.png).
        #[arg(long, default_value = "frame")]
        prefix: String,
        /// Pixel "x,y" a amostrar e imprimir (RGBA) a cada snap. Repetível.
        #[arg(long = "sample")]
        samples: Vec<String>,
        /// Só carrega a state machine e imprime o tempo de carregamento; não
        /// roda o roteiro nem salva PNGs (uso: E5, escala).
        #[arg(long)]
        measure_load: bool,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Command::SvgToPng {
            input,
            output,
            fonts,
            pin_serif_family,
        } => svg_to_png(&input, &output, &fonts, pin_serif_family.as_deref()),
        Command::LottieToPng {
            input,
            output,
            width,
            height,
            frame,
            slots,
            samples,
        } => lottie_to_png(&input, &output, width, height, frame, &slots, &samples),
        Command::Diff {
            a,
            b,
            output,
            tolerance,
        } => diff(&a, &b, &output, tolerance),
        Command::SmRender {
            input,
            out_dir,
            sm,
            sm_file,
            width,
            height,
            script,
            snap,
            prefix,
            samples,
            measure_load,
        } => sm_render(
            &input,
            &out_dir,
            sm.as_deref(),
            sm_file.as_deref(),
            width,
            height,
            &script,
            &snap,
            &prefix,
            &samples,
            measure_load,
        ),
    }
}

/// Remove todo nó `<title>` do SVG antes do usvg processar — o resvg 0.48
/// inclui erroneamente o texto de `<title>` aninhado ao medir a largura
/// para `text-anchor`, mesmo esse elemento nunca sendo desenhado (ver
/// docs/plano/D01-3-titulo-aninhado-resvg.md). Se o SVG não for XML válido,
/// devolve o texto original inalterado (deixa `usvg` reportar o erro).
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
    let svg_data =
        std::fs::read(input).with_context(|| format!("lendo {}", input.display()))?;
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
    // O Verovio usa fontes vetorizadas em <defs>/<use>, mas carregamos as fontes
    // do sistema também para não falhar em SVGs com <text> (ex.: letra de música).
    opt.fontdb_mut().load_system_fonts();
    // O SVG do Verovio referencia texto SMuFL (dinâmicas, ornamentos) via
    // font-family embutido em @font-face (woff2); o resvg não carrega esse
    // @font-face sozinho, então as fontes do Verovio precisam ser registradas
    // explicitamente para uma comparação justa contra o Lottie renderizado.
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

/// Um único "id:r,g,b" (0-1 cada) de `--slot`/da ação `slot` do roteiro de `sm-render`.
fn parse_color_slot(s: &str) -> Result<(String, [f32; 3])> {
    let (id, rgb) = s
        .split_once(':')
        .with_context(|| format!("slot mal formado (esperado id:r,g,b): {s:?}"))?;
    let parts: Vec<&str> = rgb.split(',').map(str::trim).collect();
    let [r, g, b] = parts.as_slice() else {
        bail!("slot precisa de 3 componentes r,g,b (0-1): {s:?}");
    };
    Ok((
        id.trim().to_string(),
        [
            r.parse().with_context(|| format!("componente r inválido em {s:?}"))?,
            g.parse().with_context(|| format!("componente g inválido em {s:?}"))?,
            b.parse().with_context(|| format!("componente b inválido em {s:?}"))?,
        ],
    ))
}

fn parse_color_slots(slots: &[String]) -> Result<Vec<(String, [f32; 3])>> {
    slots.iter().map(|s| parse_color_slot(s)).collect()
}

fn lottie_to_png(
    input: &Path,
    output: &Path,
    width: u32,
    height: u32,
    frame: f32,
    slots: &[String],
    samples: &[String],
) -> Result<()> {
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

    // Slots de cor (theming v2): sobrescrevem a cor de uma propriedade marcada
    // com "sid" no JSON da animação, independente do frame atual (E6 de
    // B01-spike-state-machine.md) — por isso aplicados aqui, antes do
    // set_frame/render abaixo, sem relação com o `frame` pedido.
    for (id, rgb) in parse_color_slots(slots)? {
        player
            .set_color_slot(&id, dotlottie_rs::ColorSlot::new(rgb))
            .with_context(|| format!("aplicando slot de cor {id:?}"))?;
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

    for s in samples {
        let (x, y) = s
            .split_once(',')
            .with_context(|| format!("--sample mal formado (esperado x,y): {s:?}"))?;
        let x: u32 = x.trim().parse().with_context(|| format!("coordenada x inválida: {s:?}"))?;
        let y: u32 = y.trim().parse().with_context(|| format!("coordenada y inválida: {s:?}"))?;
        match pixel_rgba(&buffer, width, x, y) {
            Some((r, g, b, a)) => println!("pixel({x},{y})=rgba({r},{g},{b},{a})"),
            None => println!("pixel({x},{y})=fora-dos-limites"),
        }
    }

    println!(
        "PNG salvo em {} ({}x{}, frame {})",
        output.display(),
        width,
        height,
        frame
    );
    Ok(())
}

/// Uma ação do roteiro de `sm-render`, no instante `ms`. Ver `docs/plano/C03-slots-interativos.md`
/// para o motivo de `Slot`/`ClearSlot`/`ClearSlots` existirem: testar o handoff M2 (state
/// machine, `Fire`) ↔ M3 (slot de cor, `Player::set_color_slot`/`clear_slot(s)`) num único
/// roteiro, sem precisar de dois processos.
enum ScriptOp {
    Fire(String),
    Slot(String, [f32; 3]),
    ClearSlot(String),
    ClearSlots,
}

struct ScriptAction {
    ms: u32,
    op: ScriptOp,
}

fn parse_script(script: &str) -> Result<Vec<ScriptAction>> {
    let mut actions = Vec::new();
    for entry in script.split(';') {
        let entry = entry.trim();
        if entry.is_empty() {
            continue;
        }
        let (ms_str, rest) = entry
            .split_once(':')
            .with_context(|| format!("ação mal formada (esperado ms:ação): {entry:?}"))?;
        let ms: u32 = ms_str
            .trim()
            .parse()
            .with_context(|| format!("instante inválido: {:?}", ms_str.trim()))?;
        let rest = rest.trim();
        let op = if let Some(name) = rest.strip_prefix("fire ") {
            ScriptOp::Fire(name.trim().to_string())
        } else if let Some(spec) = rest.strip_prefix("slot ") {
            let (id, rgb) = parse_color_slot(spec.trim())?;
            ScriptOp::Slot(id, rgb)
        } else if let Some(id) = rest.strip_prefix("clearslot ") {
            ScriptOp::ClearSlot(id.trim().to_string())
        } else if rest == "clearslots" {
            ScriptOp::ClearSlots
        } else {
            bail!(
                "ação não suportada (esperado \"fire <nome>\", \"slot <id:r,g,b>\", \"clearslot <id>\" ou \"clearslots\"): {rest:?}"
            );
        };
        actions.push(ScriptAction { ms, op });
    }
    Ok(actions)
}

fn parse_snap(snap: &str) -> Result<Vec<u32>> {
    snap.split(',')
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(|s| s.parse::<u32>().with_context(|| format!("snap inválido: {s:?}")))
        .collect()
}

fn parse_samples(samples: &[String]) -> Result<Vec<(u32, u32)>> {
    samples
        .iter()
        .map(|s| {
            let (x, y) = s
                .split_once(',')
                .with_context(|| format!("--sample mal formado (esperado x,y): {s:?}"))?;
            Ok((
                x.trim()
                    .parse()
                    .with_context(|| format!("coordenada x inválida: {:?}", x.trim()))?,
                y.trim()
                    .parse()
                    .with_context(|| format!("coordenada y inválida: {:?}", y.trim()))?,
            ))
        })
        .collect()
}

fn pixel_rgba(buffer: &[u32], width: u32, x: u32, y: u32) -> Option<(u8, u8, u8, u8)> {
    let px = *buffer.get((y as usize) * (width as usize) + (x as usize))?;
    let a = ((px >> 24) & 0xFF) as u8;
    let r = ((px >> 16) & 0xFF) as u8;
    let g = ((px >> 8) & 0xFF) as u8;
    let b = (px & 0xFF) as u8;
    Some((r, g, b, a))
}

#[allow(clippy::too_many_arguments)]
fn save_and_sample(
    buffer: &[u32],
    width: u32,
    height: u32,
    out_dir: &Path,
    prefix: &str,
    t: u32,
    state_name: &str,
    samples: &[(u32, u32)],
) -> Result<()> {
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
    let out_path = out_dir.join(format!("{prefix}-t{t}.png"));
    img.save(&out_path)
        .with_context(|| format!("salvando {}", out_path.display()))?;

    let mut line = format!("t={t}ms state={state_name} png={}", out_path.display());
    for &(x, y) in samples {
        match pixel_rgba(buffer, width, x, y) {
            Some((r, g, b, a)) => {
                line.push_str(&format!(" pixel({x},{y})=rgba({r},{g},{b},{a})"));
            }
            None => line.push_str(&format!(" pixel({x},{y})=fora-dos-limites")),
        }
    }
    println!("{line}");
    Ok(())
}

#[allow(clippy::too_many_arguments)]
fn sm_render(
    input: &Path,
    out_dir: &Path,
    sm: Option<&str>,
    sm_file: Option<&Path>,
    width: u32,
    height: u32,
    script: &str,
    snap: &str,
    prefix: &str,
    samples: &[String],
    measure_load: bool,
) -> Result<()> {
    if width == 0 || height == 0 {
        bail!("--width e --height precisam ser maiores que zero");
    }
    if sm.is_none() && sm_file.is_none() {
        bail!("informe --sm <id-no-pacote> ou --sm-file <json-avulso>");
    }

    std::fs::create_dir_all(out_dir)
        .with_context(|| format!("criando {}", out_dir.display()))?;

    let mut player = dotlottie_rs::Player::new();
    let mut buffer: Vec<u32> = vec![0; (width as usize) * (height as usize)];
    player
        .set_sw_target(&mut buffer, width, height, dotlottie_rs::ColorSpace::ARGB8888)
        .context("configurando destino de renderização por software")?;

    let data = std::fs::read(input).with_context(|| format!("lendo {}", input.display()))?;
    player
        .load_dotlottie_data(&data)
        .with_context(|| format!("carregando dotLottie {}", input.display()))?;

    let sm_file_contents;
    let load_start = Instant::now();
    let mut engine = match (sm, sm_file) {
        (Some(id), None) => {
            let id_c = CString::new(id).context("id da state machine contém byte nulo")?;
            player
                .state_machine_load(&id_c)
                .with_context(|| format!("carregando state machine {id:?} do pacote"))?
        }
        (None, Some(path)) => {
            sm_file_contents = std::fs::read_to_string(path)
                .with_context(|| format!("lendo {}", path.display()))?;
            player
                .state_machine_load_data(&sm_file_contents)
                .with_context(|| format!("carregando state machine de {}", path.display()))?
        }
        _ => unreachable!("validado acima: exatamente um de --sm/--sm-file"),
    };
    let load_elapsed = load_start.elapsed();

    let start_time = Instant::now();
    engine
        .start(&OpenUrlPolicy::default())
        .map_err(|e| anyhow::anyhow!("engine.start() falhou: {e:?}"))?;
    let start_elapsed = start_time.elapsed();

    let input_count = engine.get_inputs().len() / 2;
    println!(
        "state_machine_load: {:.3}ms — start(): {:.3}ms — {input_count} input(s) declarado(s)",
        load_elapsed.as_secs_f64() * 1000.0,
        start_elapsed.as_secs_f64() * 1000.0,
    );

    if measure_load {
        engine.release();
        return Ok(());
    }

    let actions = parse_script(script)?;
    let snaps = parse_snap(snap)?;
    let sample_points = parse_samples(samples)?;

    let max_t = actions
        .iter()
        .map(|a| a.ms)
        .chain(snaps.iter().copied())
        .max()
        .unwrap_or(0);

    let mut t: u32 = 0;
    loop {
        // Roteiro é "ms:ação" — as ações marcadas para o instante `t` (inclusive
        // t=0, ex.: E1) rodam antes do snapshot desse instante.
        for action in actions.iter().filter(|a| a.ms == t) {
            match &action.op {
                ScriptOp::Fire(event) => {
                    if let Err(e) = engine.fire(event, true) {
                        eprintln!(
                            "aviso: fire({event:?}) falhou em t={t}ms ({e:?}) — nome não declarado como Event input?"
                        );
                    }
                }
                ScriptOp::Slot(id, rgb) => {
                    if let Err(e) = engine.player.set_color_slot(id, dotlottie_rs::ColorSlot::new(*rgb)) {
                        eprintln!("aviso: slot({id:?}) falhou em t={t}ms ({e:?})");
                    }
                }
                ScriptOp::ClearSlot(id) => {
                    if let Err(e) = engine.player.clear_slot(id) {
                        eprintln!("aviso: clearslot({id:?}) falhou em t={t}ms ({e:?})");
                    }
                }
                ScriptOp::ClearSlots => {
                    if let Err(e) = engine.player.clear_slots() {
                        eprintln!("aviso: clearslots falhou em t={t}ms ({e:?})");
                    }
                }
            }
        }
        // fire() não renderiza sozinho: força o flush do frame/estado atual pro buffer
        // antes de tirar o snapshot (inofensivo quando nada mudou, ver README).
        let _ = engine.player.render();

        if snaps.contains(&t) {
            save_and_sample(
                &buffer,
                width,
                height,
                out_dir,
                prefix,
                t,
                &engine.get_current_state_name(),
                &sample_points,
            )?;
        }

        if t >= max_t {
            break;
        }
        engine
            .tick(1.0)
            .map_err(|e| anyhow::anyhow!("engine.tick() falhou em t={t}ms: {e:?}"))?;
        t += 1;
    }

    engine.release();
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
