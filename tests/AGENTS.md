# Convenções dos testes

Estas regras complementam o `AGENTS.md` da raiz para todo o conteúdo de `tests/`.

- Espelhe em `tests/` a organização dos subsistemas existentes em `src/`.
- Crie um arquivo de teste próprio para cada componente testado.
- Nomeie arquivos como `<componente>-tests.cpp`, sempre em `kebab-case`.
- Registre cada executável no CTest e aplique os labels do subsistema.
- Execute primeiro os testes do subsistema alterado e depois a suíte completa.
- Não adicione um framework de testes sem uma decisão explícita para o projeto.
