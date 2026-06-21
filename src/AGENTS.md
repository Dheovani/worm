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
- Use `camelCase` para funções, métodos e variáveis, respeitando APIs legadas
  enquanto não houver uma migração explicitamente solicitada.
- Use `UPPER_SNAKE_CASE` apenas para macros e constantes que sigam essa forma.

## Módulos

- Headers internos devem incluir caminhos a partir de `src`, como
  `<core/expression.hpp>`.
- Um módulo não deve depender do executável definido em `main.cpp`.
- Dependências entre módulos devem ser expressas com targets `Worm::*` no CMake.
