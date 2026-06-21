# Diretrizes para agentes de IA

Estas regras se aplicam a todo o repositório. Arquivos `AGENTS.md` em pastas
internas podem acrescentar regras específicas ao seu escopo.

## Organização

- Preserve a separação entre produção (`src/`) e testes (`tests/`).
- Cada subsistema deve possuir seu próprio target CMake com alias `Worm::*`.
- Use `index.hpp` como nome exclusivo para headers agregadores que apenas expõem
  os headers públicos de uma pasta.
- Não crie agregadores com o mesmo nome da pasta, como `core/core.hpp`.
- Atualize includes, CMake, testes e documentação ao mover ou renomear arquivos.
- Use também `lowercase-kebab-case` para módulos auxiliares em `cmake/`; apenas
  nomes impostos pelas ferramentas, como `CMakeLists.txt` e `AGENTS.md`, são
  exceções.

## Convenções gerais

- Use C++20 e o namespace raiz `worm`.
- Preserve mudanças do usuário não relacionadas à tarefa atual.
- Compile e execute os testes relevantes antes de declarar uma alteração pronta.
- Registre decisões estruturais duradouras neste arquivo ou no `AGENTS.md` de
  escopo mais específico.
