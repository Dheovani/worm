# Convenções do código de produção

Estas regras complementam o `AGENTS.md` da raiz para todo o conteúdo de `src/`.

## Nomes de arquivos

- Use somente letras minúsculas em nomes de arquivos próprios do projeto.
- Use `kebab-case` para nomes compostos: `where-clause.hpp`, `client-factory.cpp`.
- Use extensões `.hpp` para headers C++ e `.cpp` para implementações.
- Use `index.hpp` para o header agregador público de um subsistema.
- `CMakeLists.txt` mantém a grafia exigida pelo CMake e é uma exceção à regra.

## Símbolos C++

- Use `PascalCase` para classes, structs, enums e conceitos.
- Use `camelCase` para todas as funções, métodos e variáveis da aplicação.
- Use `snake_case` apenas para símbolos de metaprogramação, como traits,
  variable templates e funções auxiliares avaliadas em tempo de compilação.
- Use `UPPER_SNAKE_CASE` apenas para macros e constantes que sigam essa forma.
- Declare subnamespaces dentro do namespace principal, como `namespace worm`
  contendo `namespace connection`; não reabra o namespace principal apenas para
  declarar o namespace filho.
- Indente o conteúdo de cada namespace em 2 espaços, inclusive subnamespaces.
- Use membros privados com sufixo `_`, como `connection_`.
- Formate C++ com o `.clang-format` versionado na raiz.
- Use indentação de 2 espaços, sem caracteres de tabulação.
- Abra chaves na linha seguinte para namespaces, classes, structs, enums e
  funções ou métodos.
- Mantenha a chave na mesma linha em blocos de controle, como `if`, `else`,
  `switch`, `for`, `while`, `try` e `catch`.

## Módulos

- Headers internos devem incluir caminhos a partir de `src`, como
  `<core/expression.hpp>`.
- Um módulo não deve depender do executável definido em `main.cpp`.
- Dependências entre módulos devem ser expressas com targets `Worm::*` no CMake.
