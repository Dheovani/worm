# Worm

Worm é um ORM em C++20 inspirado no Doctrine. O projeto está em fase inicial,
mas já oferece um fluxo tipado de persistência sobre reflexão estática, SQL
parametrizado e drivers para SQLite, PostgreSQL e MySQL.

## Estado atual

- drivers opcionais para MySQL, PostgreSQL e SQLite;
- CRUD tipado com hidratação, identity map e updates parciais por snapshot;
- parâmetros vinculados e transações RAII;
- dependências opcionais gerenciadas por features do vcpkg;
- build organizado em targets CMake namespaced (`Worm::*`);
- testes unitários e um contrato de integração compartilhado entre drivers;
- reflexão C++20 tipada com descritores `constexpr`, conceito `Reflectable` e
  visitação de campos.
- consultas com expressões parametrizadas e composição de
  cláusulas `WHERE` e `ORDER BY`.

O projeto ainda não deve ser considerado pronto para produção. Consulte
[TODO.md](TODO.md) para acompanhar as próximas etapas.

## Uso real

O [guia de primeiros passos](docs/getting-started.md) mostra um fluxo completo
com SQLite: integração CMake, entidade refletida, CRUD, consulta parametrizada,
transação, erros, ownership e limitações atuais. O mesmo fluxo está disponível
como exemplo compilável em
[`examples/sqlite-quick-start.cpp`](examples/sqlite-quick-start.cpp).

## Requisitos

- CMake 3.20 ou superior;
- compilador com suporte a C++20;
- Git;
- vcpkg;
- no Windows, Visual Studio 2022 Build Tools com a carga de trabalho C++.

Defina `VCPKG_ROOT` apontando para a instalação do vcpkg:

```powershell
$env:VCPKG_ROOT = "C:\Users\seu-usuario\vcpkg"
```

Copie [.env.example](.env.example) para `.env` e ajuste o driver e as
credenciais. O valor `database_type` aceita `sqlite`, `postgresql` ou `mysql`;
para SQLite, `dbname` também pode ser `:memory:`.

## Configuração e build

No Windows com MSVC, os presets versionados são a forma recomendada:

```powershell
cmake --preset windows-msvc
cmake --build --preset debug
```

Para gerar uma build Release:

```powershell
cmake --build --preset release
```

Por padrão, o manifesto [vcpkg.json](vcpkg.json) instala libmysql, libpqxx e
SQLite. Cada driver pode ser desativado durante a configuração:

```powershell
cmake -S . -B build/sqlite `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

As opções `WORM_ENABLE_POSTGRESQL`, `WORM_ENABLE_MYSQL` e
`WORM_ENABLE_SQLITE` são independentes e vêm habilitadas por padrão. Drivers
desativados não têm seus fontes, testes ou dependências adicionados à build.

## Testes

```powershell
ctest --preset debug
```

Para executar apenas um domínio:

```powershell
ctest --test-dir build -C Debug -L errors --output-on-failure
ctest --test-dir build -C Debug -L connection --output-on-failure
ctest --test-dir build -C Debug -L core --output-on-failure
```

Os testes ficam fora do código de produção:

```text
tests/
├── connection/
├── core/
├── errors/
├── reflection/
└── utils/
```

SQLite, MySQL e PostgreSQL compartilham o mesmo contrato de integração. SQLite
roda localmente; os outros bancos usam instâncias descartáveis na CI e podem ser
executados localmente com as variáveis descritas em `.env.example`.

## Estrutura

```text
worm/
├── cmake/          # descoberta e normalização de dependências
├── docs/           # guias de uso e limitações
├── examples/       # exemplos compiláveis opcionais
├── src/
│   ├── connection/ # clientes de banco de dados
│   ├── core/       # expressões e cláusulas SQL parametrizadas
│   ├── errors/     # tipos de erro públicos
│   ├── reflection/ # descritores e visitação tipada de campos
│   └── utils/      # helpers, factory e injeção de dependências
├── tests/          # testes organizados por subsistema
├── CMakeLists.txt
└── vcpkg.json
```

## Licença

Este projeto é distribuído sob a [licença MIT](LICENSE).
