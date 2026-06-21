# Worm

Worm é um ORM em C++20 inspirado no Doctrine. O projeto está em fase inicial e
atualmente concentra-se na infraestrutura de conexão com bancos de dados, injeção
de dependências e no desenho de um subsistema de reflexão tipado.

## Estado atual

- clientes para MySQL, PostgreSQL e SQLite;
- dependências gerenciadas por vcpkg;
- build organizado em targets CMake namespaced (`Worm::*`);
- testes unitários para erros, conexões e utilitários;
- teste de integração SQLite executado inteiramente em memória.
- reflexão C++20 tipada com descritores `constexpr`, conceito `Reflectable` e
  visitação de campos.

O projeto ainda não deve ser considerado pronto para produção. Consulte
[TODO.md](TODO.md) para acompanhar as próximas etapas.

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

O manifesto [vcpkg.json](vcpkg.json) instala automaticamente JsonCpp, libmysql,
libpqxx e SQLite durante a configuração.

## Testes

```powershell
ctest --preset debug
```

Para executar apenas um domínio:

```powershell
ctest --test-dir build -C Debug -L errors --output-on-failure
ctest --test-dir build -C Debug -L connection --output-on-failure
```

Os testes ficam fora do código de produção:

```text
tests/
├── connection/
└── errors/
```

MySQL e PostgreSQL possuem atualmente testes de contrato que não abrem conexões
externas. Testes de integração para esses bancos serão adicionados com instâncias
descartáveis.

## Estrutura

```text
worm/
├── cmake/          # descoberta e normalização de dependências
├── src/
│   ├── connection/ # clientes de banco de dados
│   ├── errors/     # tipos de erro públicos
│   ├── reflection/ # descritores e visitação tipada de campos
│   └── utils/      # helpers, factory e injeção de dependências
├── tests/          # testes organizados por subsistema
├── CMakeLists.txt
└── vcpkg.json
```

## Licença

Uma licença ainda não foi definida para o projeto.
