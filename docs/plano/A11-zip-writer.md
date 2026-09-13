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

- `ZipFileWriter` declarada em `filereader.h` logo após `ZipFileReader`,
  reaproveitando a mesma forward declaration de `miniz_cpp::zip_file`.
  Implementação em `filereader.cpp`, único lugar que inclui `zip_file.hpp`.
- Construtor cria `m_file` diretamente com `new miniz_cpp::zip_file()`
  (construtor vazio da lib, sem `Reset()` — diferente do reader, que carrega
  de bytes/arquivo existente).
- `AddFile` chama `writestr(archivePath, content)` sem tratar exceção
  (conforme o plano — só `Save`/`GetBytes` capturam).
- `Save`/`GetBytes` envolvem a chamada em `try/catch (const std::exception &)`,
  com `LogError` e retorno `false`/`{}`. Note que
  `zip_file::save(std::vector<unsigned char> &)` recebe o vetor por
  referência e o preenche (não retorna um novo vetor).
- Build verificado com `cd verovio/tools && make -j4` (build já existente,
  gerado por passo anterior) — compilou e linkou sem erros.
