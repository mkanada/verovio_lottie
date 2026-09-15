// thorvg-render: renderiza SVG e Lottie/dotLottie via ThorVG puro (mesma
// dependência de renderização por trás do `.lottie` do projeto, mas com o
// loader de SVG também habilitado - ausente do build vendorizado por
// compare/, que só precisa do loader de Lottie). Ver thorvg-cli/README.md.
//
// Objetivo: comparar como o MESMO motor (ThorVG) renderiza o SVG gerado pelo
// Verovio (via loader de SVG nativo do ThorVG) contra o .lottie gerado pelo
// mesmo Verovio (via loader de Lottie nativo do ThorVG) - isolando se uma
// divergência (ex.: espaçamento de texto itálico grudado) vem do layer de
// texto do Lottie (ty:5) ou é uma característica do próprio motor de texto
// do ThorVG (`Text`/`tvgText.cpp`), compartilhada pelos dois loaders (ambos
// chamam `Text::gen()` - `tvgSvgBuilder.cpp`/`tvgLottieBuilder.cpp`).
//
// Uso:
//   thorvg-render svg <in.svg> <out.png> [--width W --height H] [--font arquivo.ttf]...
//   thorvg-render lottie <in.lottie|in.json> <out.png> --width W --height H [--frame N]
//
// Fundo sempre branco opaco no PNG de saída (mesma convenção de
// compare/src/main.rs - ver docs/matriz-layout/README.md).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <unistd.h>
#include <thorvg.h>
#include "lodepng.h"

namespace {

struct Args {
    std::string mode;
    std::string input;
    std::string output;
    uint32_t width = 0;
    uint32_t height = 0;
    float frame = 0.0f;
    std::vector<std::string> fonts;
};

[[noreturn]] void Usage(const char *prog)
{
    std::fprintf(stderr,
        "Uso:\n"
        "  %s svg <in.svg> <out.png> [--width W --height H] [--font arquivo.ttf]...\n"
        "  %s lottie <in.lottie|in.json> <out.png> --width W --height H [--frame N]\n",
        prog, prog);
    std::exit(1);
}

Args ParseArgs(int argc, char **argv)
{
    if (argc < 4) Usage(argv[0]);
    Args args;
    args.mode = argv[1];
    args.input = argv[2];
    args.output = argv[3];
    for (int i = 4; i < argc; i++) {
        std::string flag = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) Usage(argv[0]);
            return argv[++i];
        };
        if (flag == "--width") {
            args.width = static_cast<uint32_t>(std::stoul(next()));
        } else if (flag == "--height") {
            args.height = static_cast<uint32_t>(std::stoul(next()));
        } else if (flag == "--frame") {
            args.frame = std::stof(next());
        } else if (flag == "--font") {
            args.fonts.push_back(next());
        } else {
            std::fprintf(stderr, "Flag desconhecida: %s\n", flag.c_str());
            Usage(argv[0]);
        }
    }
    if (args.mode != "svg" && args.mode != "lottie") Usage(argv[0]);
    return args;
}

// Compõe o buffer ARGB8888S (alpha reto, como no ARGB8888S de
// compare/src/main.rs) sobre fundo branco opaco e devolve RGBA pronto pro
// lodepng - mesma lógica/motivo de compare/src/main.rs's buffer_to_white_png.
std::vector<unsigned char> CompositeOverWhite(const std::vector<uint32_t> &buffer, uint32_t width, uint32_t height)
{
    std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
    auto blend = [](uint32_t channel, uint32_t alpha) -> unsigned char {
        return static_cast<unsigned char>((channel * alpha + 255 * (255 - alpha) + 127) / 255);
    };
    for (uint32_t i = 0; i < width * height; i++) {
        uint32_t px = buffer[i];
        uint32_t a = (px >> 24) & 0xFF;
        uint32_t r = (px >> 16) & 0xFF;
        uint32_t g = (px >> 8) & 0xFF;
        uint32_t b = px & 0xFF;
        rgba[i * 4 + 0] = blend(r, a);
        rgba[i * 4 + 1] = blend(g, a);
        rgba[i * 4 + 2] = blend(b, a);
        rgba[i * 4 + 3] = 255;
    }
    return rgba;
}

void SavePng(const std::string &path, const std::vector<unsigned char> &rgba, uint32_t width, uint32_t height)
{
    unsigned error = lodepng::encode(path, rgba, width, height);
    if (error) {
        std::fprintf(stderr, "Erro ao salvar PNG (%u): %s\n", error, lodepng_error_text(error));
        std::exit(1);
    }
    std::printf("PNG salvo em %s (%ux%u)\n", path.c_str(), width, height);
}

void Check(tvg::Result result, const char *what)
{
    if (result != tvg::Result::Success) {
        std::fprintf(stderr, "Erro em %s (código %d)\n", what, static_cast<int>(result));
        std::exit(1);
    }
}

// Extrai um único arquivo de um .lottie (zip) para stdout via `unzip -p`,
// mesmo mecanismo já usado em compare/scripts/*.sh (ver compare-page.sh) -
// evita depender de uma lib de zip em C++ só pra isto.
std::string UnzipToString(const std::string &archive, const std::string &member)
{
    std::string cmd = "unzip -p " + archive + " " + member;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::fprintf(stderr, "Erro ao rodar unzip para %s\n", member.c_str());
        std::exit(1);
    }
    std::string out;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), pipe)) > 0) {
        out.append(buf, n);
    }
    pclose(pipe);
    return out;
}

// Lista os nomes de arquivo dentro do .lottie (zip), um por linha, via
// `unzip -Z1` - usado para achar as fontes embutidas (f/*.ttf) sem parsear
// o manifest.json de verdade (formato fixo e conhecido do exportador deste
// projeto, ver verovio/src/toolkit.cpp EmbedCommonTextFonts).
std::vector<std::string> ListZipEntries(const std::string &archive)
{
    std::string cmd = "unzip -Z1 " + archive;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::fprintf(stderr, "Erro ao listar entradas de %s\n", archive.c_str());
        std::exit(1);
    }
    std::vector<std::string> entries;
    char line[1024];
    while (std::fgets(line, sizeof(line), pipe)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty()) entries.push_back(s);
    }
    pclose(pipe);
    return entries;
}

// Acha o id da animação principal em manifest.json (procura só o primeiro
// "animations":[{"id":"..." - suficiente para o formato fixo gerado por
// este projeto, não é um parser JSON de verdade).
std::string FindAnimationId(const std::string &manifestJson)
{
    auto pos = manifestJson.find("\"animations\"");
    if (pos == std::string::npos) return "score";
    pos = manifestJson.find("\"id\"", pos);
    if (pos == std::string::npos) return "score";
    pos = manifestJson.find(':', pos);
    pos = manifestJson.find('"', pos + 1);
    auto end = manifestJson.find('"', pos + 1);
    if (pos == std::string::npos || end == std::string::npos) return "score";
    return manifestJson.substr(pos + 1, end - pos - 1);
}

void LoadEmbeddedFonts(const std::string &archive)
{
    for (const auto &entry : ListZipEntries(archive)) {
        if (entry.rfind("f/", 0) != 0) continue;
        std::string data = UnzipToString(archive, entry);
        if (data.empty()) continue;
        // Nome de registro = nome do arquivo sem extensão (ex.
        // "LiberationSerif-Italic"), o mesmo valor usado em "fName"/"f" no
        // JSON do layer de texto (LottieWriter::WriteFontsList).
        auto slash = entry.find_last_of('/');
        auto dot = entry.find_last_of('.');
        std::string name = entry.substr(slash + 1, dot - slash - 1);
        Check(tvg::Text::load(name.c_str(), data.data(), static_cast<uint32_t>(data.size()), "ttf", true),
            ("Text::load (fonte embutida) " + entry).c_str());
    }
}

// O SVG do Verovio aninha um <svg class="definition-scale" viewBox="0 0 VW VH" ...>
// dentro do <svg> raiz pra escalar as "unidades de definição" (DEFINITION_FACTOR=10,
// ver docs/plano/README.md "Unidades") pro tamanho final de página em px. O loader de
// SVG do ThorVG não suporta <svg> aninhado - "Nested <svg> element is not supported" -
// e dropa TODO o conteúdo em silêncio (achado testando este mesmo caminho: PNG saía
// 100% branco, sem erro). Substitui o <svg> aninhado por um <g transform="scale(...)">
// matematicamente equivalente antes de carregar, preservando os demais atributos da
// tag (ex. font-family, color) que só estavam ali por herança CSS.
std::string FlattenNestedSvg(const std::string &svg)
{
    auto firstSvg = svg.find("<svg");
    if (firstSvg == std::string::npos) return svg;
    auto secondSvg = svg.find("<svg", firstSvg + 4);
    if (secondSvg == std::string::npos) return svg; // sem aninhamento, nada a fazer

    auto ExtractAttr = [](const std::string &tag, const std::string &name) -> std::string {
        auto pos = tag.find(name + "=\"");
        if (pos == std::string::npos) return "";
        pos += name.size() + 2;
        auto end = tag.find('"', pos);
        return tag.substr(pos, end - pos);
    };

    std::string outerTag = svg.substr(firstSvg, svg.find('>', firstSvg) - firstSvg + 1);
    double outerW = std::stod(ExtractAttr(outerTag, "width"));
    double outerH = std::stod(ExtractAttr(outerTag, "height"));

    auto innerTagEnd = svg.find('>', secondSvg);
    std::string innerTag = svg.substr(secondSvg, innerTagEnd - secondSvg + 1);
    double vx, vy, vw, vh;
    std::sscanf(ExtractAttr(innerTag, "viewBox").c_str(), "%lf %lf %lf %lf", &vx, &vy, &vw, &vh);

    // "<svg ...>" -> "<g ...>" (troca só o nome da tag, mantém todos os outros atributos)
    std::string gTag = innerTag;
    gTag.replace(1, 3, "g");
    auto vbPos = gTag.find("viewBox=\"");
    auto vbEnd = gTag.find('"', vbPos + 9);
    char transformAttr[128];
    std::snprintf(transformAttr, sizeof(transformAttr), "transform=\"scale(%.10g,%.10g)\"", outerW / vw, outerH / vh);
    gTag.replace(vbPos, vbEnd - vbPos + 1, transformAttr);

    // </svg> que fecha o aninhado = primeiro </svg> depois dele (Verovio nunca aninha
    // um terceiro <svg> dentro deste, então é inequívoco).
    auto innerClose = svg.find("</svg>", innerTagEnd);

    return svg.substr(0, secondSvg) + gTag + svg.substr(innerTagEnd + 1, innerClose - innerTagEnd - 1) + "</g>"
        + svg.substr(innerClose + 6);
}

std::string ReadFile(const std::string &path)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "Erro ao abrir %s\n", path.c_str());
        std::exit(1);
    }
    std::string data;
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
    std::fclose(f);
    return data;
}

void RunSvg(const Args &args)
{
    for (const auto &font : args.fonts) {
        Check(tvg::Text::load(font.c_str()), ("Text::load (--font) " + font).c_str());
    }

    std::string svgData = FlattenNestedSvg(ReadFile(args.input));
    std::string rpath = ".";
    if (auto slash = args.input.find_last_of('/'); slash != std::string::npos) rpath = args.input.substr(0, slash);

    auto picture = tvg::Picture::gen();
    Check(picture->load(svgData.data(), static_cast<uint32_t>(svgData.size()), "svg", rpath.c_str(), false),
        "Picture::load (SVG)");

    uint32_t width = args.width, height = args.height;
    if (width == 0 || height == 0) {
        float fw, fh;
        picture->size(&fw, &fh);
        width = static_cast<uint32_t>(fw);
        height = static_cast<uint32_t>(fh);
    } else {
        Check(picture->size(static_cast<float>(width), static_cast<float>(height)), "Picture::size");
    }

    auto canvas = tvg::SwCanvas::gen();
    std::vector<uint32_t> buffer(static_cast<size_t>(width) * height, 0);
    Check(canvas->target(buffer.data(), width, width, height, tvg::ColorSpace::ARGB8888S), "SwCanvas::target");
    Check(canvas->add(picture), "Canvas::add");
    Check(canvas->draw(true), "Canvas::draw");
    canvas->sync();

    SavePng(args.output, CompositeOverWhite(buffer, width, height), width, height);
}

void RunLottie(const Args &args)
{
    if (args.width == 0 || args.height == 0) {
        std::fprintf(stderr, "--width e --height são obrigatórios pro modo lottie\n");
        std::exit(1);
    }

    std::string jsonPath = args.input;
    std::string tmpJsonPath;
    if (args.input.size() > 7 && args.input.compare(args.input.size() - 7, 7, ".lottie") == 0) {
        LoadEmbeddedFonts(args.input);
        std::string manifest = UnzipToString(args.input, "manifest.json");
        std::string id = FindAnimationId(manifest);
        std::string member = "a/" + id + ".json";
        std::string json = UnzipToString(args.input, member);
        if (json.empty()) {
            std::fprintf(stderr, "Não achei %s dentro de %s\n", member.c_str(), args.input.c_str());
            std::exit(1);
        }
        tmpJsonPath = "/tmp/thorvg-render-" + std::to_string(getpid()) + ".json";
        FILE *f = std::fopen(tmpJsonPath.c_str(), "wb");
        std::fwrite(json.data(), 1, json.size(), f);
        std::fclose(f);
        jsonPath = tmpJsonPath;
    }

    auto animation = tvg::Animation::gen();
    Check(animation->picture()->load(jsonPath.c_str()), "Picture::load (Lottie)");
    Check(animation->picture()->size(static_cast<float>(args.width), static_cast<float>(args.height)), "Picture::size");
    Check(animation->frame(args.frame), "Animation::frame");

    auto canvas = tvg::SwCanvas::gen();
    std::vector<uint32_t> buffer(static_cast<size_t>(args.width) * args.height, 0);
    Check(canvas->target(buffer.data(), args.width, args.width, args.height, tvg::ColorSpace::ARGB8888S), "SwCanvas::target");
    Check(canvas->add(animation->picture()), "Canvas::add");
    Check(canvas->draw(true), "Canvas::draw");
    canvas->sync();

    if (!tmpJsonPath.empty()) std::remove(tmpJsonPath.c_str());

    SavePng(args.output, CompositeOverWhite(buffer, args.width, args.height), args.width, args.height);
}

} // namespace

int main(int argc, char **argv)
{
    Args args = ParseArgs(argc, argv);

    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;
    Check(tvg::Initializer::init(threads), "Initializer::init");

    if (args.mode == "svg") {
        RunSvg(args);
    } else {
        RunLottie(args);
    }

    tvg::Initializer::term();
    return 0;
}
