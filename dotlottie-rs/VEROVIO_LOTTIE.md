# dotlottie-rs local (verovio_lottie)

Cópia vendorizada do crate [`dotlottie-rs`](https://github.com/LottieFiles/dotlottie-rs),
usada por `compare/` (`dotlottie-rs = { path = "../dotlottie-rs", ... }`).

Existe só para compilar o [ThorVG local](../thorvg/VEROVIO_LOTTIE.md) em vez
do submódulo original: o `build.rs` do dotlottie-rs compila o ThorVG a partir
do caminho fixo `deps/thorvg` (relativo ao crate), sem variável de ambiente
para trocar a origem, e o Cargo não permite substituir um submódulo de uma
dependência git.

## Origem

- Commit `eb44c991e5e2bc08daa5081caf750d1324f31d62` (versão 0.1.58) — o mesmo
  que o `compare/Cargo.lock` fixava.
- Copiados de `dotlottie-rs/` (subpasta do repositório original): `Cargo.toml`,
  `build.rs`, `cbindgen.toml`, `src/`, `cpp/`, `examples/` e `benches/` (o
  `Cargo.toml` declara `[[example]]`/`[[bench]]` explicitamente). Da raiz do
  repositório original: `LICENSE` (MIT) e `README.md`.
- Omitidos: `assets/` e `tests/` (usados só pelos testes do próprio crate —
  `cargo test` dentro desta pasta não funciona), `deps/` (substituído, ver
  abaixo) e `Cargo.lock` (quem manda é o `compare/Cargo.lock`).

## Modificações locais

- Nenhuma no código Rust.
- `deps/thorvg` é um symlink para `../../thorvg`.

## Atualizando

1. Trocar os arquivos listados em "Origem" pelos de um commit novo.
2. Anotar o commit do submódulo `deps/thorvg` desse commit e atualizar
   `thorvg/` para ele (ver `thorvg/VEROVIO_LOTTIE.md`, que também lista as
   correções locais a reaplicar).
3. Conferir se o `build.rs` novo ainda compila de `deps/thorvg` (senão, ajustar
   o symlink).
