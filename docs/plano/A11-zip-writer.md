# A11 — `ZipFileWriter`

**Depende de:** A01 (só do build) · **Decisão:** nenhuma ·
Independente das formas; pode ser feito a qualquer momento depois de A01.

## Objetivo

Oferecer escrita de arquivos zip ao Verovio, para empacotar o `.lottie`.

## Ler antes (só isto)

- `verovio/include/vrv/filereader.h` L15-L82 — `ZipFileReader` e a forward
  declaration de `miniz_cpp::zip_file` (L17-L19, membro em L80).
- `verovio/src/filereader.cpp` L20-L125 — implementação do leitor.
- `verovio/include/zip/zip_file.hpp` L9486-L9575 (construtores, `save`) e
  L9849-L9870 (`writestr`).

## Arquivos

- Modificar: `verovio/include/vrv/filereader.h`, `verovio/src/filereader.cpp`.

## O que fazer

1. Declarar `class ZipFileWriter` em `filereader.h`, ao lado do leitor,
   reaproveitando a forward declaration: construtor, destrutor,
   `void AddFile(const std::string &archivePath, const std::string &content)`,
   `bool Save(const std::string &filename)`,
   `std::vector<unsigned char> GetBytes()`.
2. Implementar em **`src/filereader.cpp`**, a única unidade de tradução que
   inclui `zip_file.hpp`: `miniz_cpp::zip_file *m_file` criado no construtor e
   destruído no destrutor, como no leitor. `AddFile` → `writestr`;
   `Save` → `save(filename)`; `GetBytes` → `save(std::vector<unsigned char> &)`.
   A biblioteca lança exceções: capturar em `Save`/`GetBytes`, `LogError` e
   retornar `false`/vetor vazio.
3. Não incluir `zip_file.hpp` em nenhum outro arquivo (header-only com a
   implementação do miniz → risco de símbolos duplicados no link).

## Fora de escopo

Manifest e conteúdo do `.lottie` (A12).

## Critérios de aceite

- Compila e linka. O uso real (e `unzip -t`) é verificado em A12.

## Notas de execução

_(preencher ao executar)_
