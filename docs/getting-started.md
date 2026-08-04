# Primeiros passos

Este guia apresenta o menor fluxo completo atualmente suportado pelo Worm:
configurar uma build SQLite, mapear uma entidade, inserir, consultar, atualizar e
remover registros e controlar uma transação. O exemplo compilável correspondente
está em [`examples/sqlite-quick-start.cpp`](../examples/sqlite-quick-start.cpp).

## Estado da API

O Worm ainda está em desenvolvimento e não possui garantia de estabilidade
binária ou de compatibilidade entre versões. Use-o para experimentação e
contribuição, não para dados de produção.

## Compilar o quick start

São necessários CMake 3.20, um compilador C++20 e vcpkg. Com `VCPKG_ROOT`
configurado, uma build mínima usa somente a feature SQLite do manifesto:

```powershell
cmake -S . -B build/quick-start `
  -DBUILD_TESTING=OFF `
  -DWORM_BUILD_EXAMPLES=ON `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build/quick-start --config Debug
./build/quick-start/examples/Debug/WormSqliteQuickStart.exe
```

Em geradores de configuração única, como Ninja, o executável normalmente fica
diretamente em `build/quick-start/examples/`.

## Consumir o Worm com CMake

Enquanto regras de instalação e `find_package(Worm)` ainda não existem, inclua o
repositório como subdiretório:

```cmake
set(WORM_ENABLE_POSTGRESQL OFF CACHE BOOL "" FORCE)
set(WORM_ENABLE_MYSQL OFF CACHE BOOL "" FORCE)
set(WORM_ENABLE_SQLITE ON CACHE BOOL "" FORCE)
set(WORM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory(external/worm)

add_executable(my_application main.cpp)
target_compile_features(my_application PRIVATE cxx_std_20)
target_link_libraries(my_application PRIVATE Worm::Core Worm::Connection)
```

## Definir uma entidade

Uma entidade persistível precisa fornecer `table()`, `reflect()` e exatamente uma
chave primária persistente:

```cpp
struct User
{
  std::int64_t id{};
  std::string name;
  std::optional<std::string> email;

  static constexpr worm::core::Table table() noexcept
  {
    return worm::core::Table{"users"};
  }

  static constexpr auto reflect() noexcept
  {
    return std::tuple{
      worm::reflection::field("id", &User::id, {.primaryKey = true}),
      worm::reflection::field("name", &User::name),
      worm::reflection::field("email", &User::email)};
  }
};
```

O nome informado a `field()` é também o nome da coluna, salvo quando
`FieldMetadata::columnName` for definido. Campos marcados como `ignored` não são
persistidos. O fluxo portátil atual usa chaves fornecidas pela aplicação. Chaves
marcadas como `generated` exigem que o `INSERT` devolva uma linha com o valor
gerado, comportamento que ainda não está completo nos três drivers.

## Criar os objetos do ORM

A injeção explícita é o caminho recomendado porque torna conexão e lifetime
visíveis:

```cpp
const worm::connection::ConnectionConfig config{
  .dbname = "application.db",
};

const auto client = std::make_shared<worm::connection::SqliteClient>(config);
const worm::core::SqliteBuilder sqlBuilder;
const worm::core::QueryBuilder queryBuilder{sqlBuilder};
const auto registry = std::make_shared<worm::core::Registry>();
const worm::core::Repository<User> users{client, queryBuilder, registry};
```

O Worm ainda não cria tabelas. Antes de usar o repositório, o esquema precisa
existir:

```sql
CREATE TABLE users (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  email TEXT NULL
);
```

## CRUD

```cpp
const std::shared_ptr<User> created = users.insert({
  .id = 1,
  .name = "Ada",
  .email = "ada@example.com",
});

const std::shared_ptr<User> found = users.find(std::int64_t{1});

found->name = "Ada Lovelace";
const std::uint64_t affected = users.update(found->id, *found);

users.delete_(found->id);
```

`find()` retorna `nullptr` quando não encontra a linha. Chamadas repetidas a
`find(id)` no mesmo `Registry` reutilizam o `shared_ptr` registrado. Quando existe
um snapshot para a entidade, `update()` envia somente campos alterados e retorna
o número de linhas afetadas.

## Consulta parametrizada

Valores nunca devem ser concatenados ao SQL. Use os builders:

```cpp
const worm::core::Statement statement = queryBuilder.selectAll(
  {User::table().name()},
  {},
  worm::core::Filter{
    worm::core::Predicate::equal("users.name", std::string{"Ada Lovelace"})});

const std::vector<std::shared_ptr<User>> result = users.findAll(statement);
```

Os overloads que recebem `Statement` são o escape controlado para SQL manual,
mas continuam aceitando apenas a operação correspondente ao método do
repositório e parâmetros separados do texto SQL.

## Transações

Uma transação precisa ser finalizada explicitamente. Se sair do escopo ainda
ativa, seu destrutor tenta executar rollback:

```cpp
{
  auto transaction = client->beginTransaction();
  static_cast<void>(users.insert(User{.id = 2, .name = "Grace"}));
  transaction.commit();
}
```

Após rollback, descarte ou recrie o `Registry`: o banco reverte os dados, mas o
identity map ainda não reconcilia automaticamente entidades inseridas ou
alteradas durante a transação.

## Erros

Erros públicos derivam de `worm::WormException`. Capture tipos específicos
quando houver uma recuperação possível e use a base no limite da aplicação:

```cpp
try {
  const std::shared_ptr<User> user = users.find(std::int64_t{1});
} catch (const worm::QueryExecutionException& error) {
  // Falha reportada pelo banco ou pelo driver.
} catch (const worm::WormException& error) {
  // Outro erro normalizado pelo ORM.
}
```

## Contexto de persistência e lifetime

`Session` centraliza o cliente, o identity map e os repositories. O tipo
de banco é lido de `DATABASE_TYPE`, documentado em [`.env.example`](../.env.example):

```cpp
const worm::context::Session context(config);
const auto& users = context.repository<User>();
```

O contexto e os objetos associados pertencem à thread em que foram criados. O
acesso por outra thread produz `ConcurrentAccessException`; use um contexto e uma
conexão separados para cada fluxo concorrente. Uma transação ativa também precisa
ser confirmada ou revertida na thread proprietária. Se sair de escopo ainda ativa,
seu destrutor tenta executar rollback.

O `Repository` mantém ownership compartilhado do cliente e do registry recebidos.
Entidades retornadas como `shared_ptr` podem sobreviver ao contexto, mas o ponteiro
compartilhado não sincroniza modificações feitas na própria entidade.

## Limitações atuais

- Migrações e criação de esquema ainda não estão implementadas.
- Chaves geradas pelo banco ainda não têm comportamento portátil entre drivers.
- Relacionamentos, eager/lazy loading e detecção de N+1 ainda não existem.
- Não existe pool de conexões nem cache de statements preparados.
- Um mesmo `Client`, `Session`, `Repository` ou `Registry` não pode ser
  compartilhado entre threads; crie contextos independentes para trabalho paralelo.
- O contrato de instalação e `find_package(Worm)` ainda será definido.
