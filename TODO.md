# Roadmap

O Worm pretende ser um ORM C++20 pequeno, seguro e previsível, inspirado nas
boas ideias do Doctrine sem tentar reproduzir toda a sua complexidade. O valor
para o desenvolvedor deve vir de uma API tipada, SQL parametrizado, integração
simples com CMake e comportamento explícito, sem consultas ou alocações ocultas.

## Princípios do projeto

- Segurança e correção têm prioridade sobre conveniência sintática.
- A API pública deve ser pequena, documentada e difícil de usar incorretamente.
- Recursos devem nascer de casos de uso completos e testáveis, não apenas de
  abstrações isoladas.
- SQL gerado deve ser inspecionável e permitir escape controlado para SQL manual.
- Drivers e dependências de bancos devem ser opcionais.
- Erros, limitações e custos de desempenho devem ser explícitos.
- O projeto deve acolher contribuições com respeito, documentação e decisões
  técnicas transparentes.

## Marco 0 — Fundação concluída

- [x] Organizar o código de produção em `src/`.
- [x] Configurar CMake com targets `Worm::*`.
- [x] Adicionar manifesto vcpkg reproduzível.
- [x] Integrar testes ao CTest fora de `src/`.
- [x] Padronizar estilo, nomes e formatação com `.clang-format` e `AGENTS.md`.
- [x] Cobrir erros, conexões, utilitários e injeção de dependências.

### Reflexão

- [x] Implementar descritores de campos tipados com ponteiros para membros.
- [x] Definir o conceito C++20 `Reflectable`.
- [x] Implementar visitação `for_each_field` sem alocações ou funções virtuais.
- [x] Adicionar metadados de coluna, chave primária, geração e campo ignorado.
- [x] Implementar lookup por membro e coluna com tratamento de colisão de hash.
- [x] Criar snapshots tipados e detecção de campos persistentes alterados.
- [x] Criar testes unitários separados para cada componente da reflexão.

## Marco 1 — ORM mínimo utilizável

Este marco deve terminar com um exemplo completo que cria, persiste, consulta,
altera e remove uma entidade em SQLite sem concatenar valores em SQL.

### Modelo de entidades

- [ ] Definir metadados de entidade e tabela, separados dos metadados de campo.
- [ ] Reintroduzir o núcleo de entidades sobre o subsistema de reflexão.
- [ ] Validar em compile time, quando possível, chave primária ausente ou duplicada.
- [ ] Definir uma convenção clara para entidades novas, persistidas e removidas.

### SQL e parâmetros

- [ ] Criar uma representação `Statement` contendo SQL e parâmetros vinculados.
- [ ] Implementar geração de `SELECT`, `INSERT`, `UPDATE` e `DELETE`.
- [x] Criar expressões parametrizadas e cláusulas `WHERE` e `ORDER BY`.
- [ ] Implementar operadores lógicos agrupados: `AND`, `OR` e `NOT`.
- [ ] Vincular parâmetros nos drivers sem interpolação de valores.
- [ ] Criar uma abstração de dialeto para placeholders, identificadores e recursos
  específicos de SQLite, PostgreSQL e MySQL.
- [ ] Permitir SQL manual parametrizado como escape controlado da abstração.

### Tipos e hidratação

- [ ] Definir codecs entre tipos C++ e SQL.
- [ ] Cobrir números, booleanos, texto, `std::optional`, datas e enums.
- [ ] Diferenciar corretamente `NULL`, string vazia e valor padrão.
- [ ] Hidratar resultados em entidades refletidas com diagnóstico de coluna inválida.
- [ ] Tratar conversões inválidas sem perda silenciosa de dados.

### Persistência

- [ ] Implementar repositório tipado para operações básicas.
- [ ] Implementar mapa de identidade para evitar instâncias duplicadas da mesma linha.
- [ ] Implementar unidade de trabalho usando snapshots para `UPDATE` parcial.
- [ ] Implementar transações RAII com commit e rollback explícitos.
- [ ] Criar um fluxo de erro consistente entre reflexão, SQL e drivers.

## Marco 2 — Conexões confiáveis e portáveis

- [x] Adicionar destrutor virtual à interface `Client`.
- [x] Remover `noexcept` de fábricas que podem falhar ao conectar.
- [x] Padronizar nomes e tratamento básico entre os três drivers.
- [ ] Substituir ownership manual nos drivers por RAII.
- [ ] Separar conexão, statement preparado e resultado em interfaces próprias.
- [ ] Definir comportamento de thread safety e impedir uso concorrente inseguro.
- [ ] Implementar configuração de timeout e cancelamento quando o driver suportar.
- [ ] Adicionar pool de conexões somente após medir uma necessidade real.
- [ ] Separar drivers opcionais para não exigir todos os bancos em cada build.
- [ ] Criar testes de integração descartáveis para PostgreSQL e MySQL.
- [ ] Executar o mesmo contrato de integração para todos os drivers.

## Marco 3 — Consultas e relacionamentos

- [ ] Adicionar projeções, aliases, joins, paginação e agregações.
- [ ] Criar uma API de critérios componível sem esconder o SQL resultante.
- [ ] Implementar relações um-para-um, um-para-muitos e muitos-para-muitos.
- [ ] Tornar carregamento eager ou lazy uma escolha explícita.
- [ ] Detectar e documentar consultas N+1.
- [ ] Definir cascatas e orphan removal com opções conservadoras por padrão.

## Marco 4 — Esquema e migrações

- [ ] Representar esquema, tabela, coluna, índice e chave estrangeira.
- [ ] Comparar metadados de entidades com o esquema existente.
- [ ] Gerar migrações revisáveis, sem executá-las automaticamente.
- [ ] Manter histórico, checksum, aplicação e rollback de migrações.
- [ ] Documentar diferenças e limitações de migração entre os bancos.

## Experiência do desenvolvedor

- [ ] Criar um quick start que funcione em menos de dez minutos.
- [ ] Manter exemplos completos para CRUD, transações, consultas e relações.
- [ ] Produzir mensagens de erro que indiquem entidade, campo, coluna e operação.
- [ ] Documentar claramente ownership, lifetime e garantias de thread safety.
- [ ] Criar documentação de API com Doxygen ou ferramenta equivalente.
- [ ] Publicar um guia de arquitetura e decisões técnicas relevantes.
- [ ] Adicionar changelog e guia de migração para mudanças incompatíveis.

## Qualidade, segurança e desempenho

- [ ] Executar formatação e análise estática automaticamente na CI.
- [ ] Ativar warnings rigorosos e tratar warnings do projeto como erros na CI.
- [ ] Adicionar sanitizers no Linux e ferramentas equivalentes no Windows.
- [ ] Medir cobertura e publicar lacunas relevantes, sem perseguir apenas percentual.
- [ ] Adicionar testes de propriedade e fuzzing para parser, geração e parâmetros SQL.
- [ ] Criar benchmarks para hidratação, snapshots e geração de consultas.
- [ ] Definir uma política de segurança e canal responsável para vulnerabilidades.
- [ ] Auditar logs e exceções para nunca expor senhas ou parâmetros sensíveis.

## Portabilidade e distribuição

- [ ] Configurar CI para Windows e Linux com MSVC, GCC e Clang.
- [ ] Testar versões mínimas e atuais dos compiladores suportados.
- [ ] Criar regras CMake de instalação e exportação com `find_package(Worm)`.
- [ ] Publicar pacotes reproduzíveis no vcpkg e, se houver demanda, Conan.
- [ ] Permitir builds mínimos apenas com SQLite.
- [ ] Definir e seguir versionamento semântico.
- [ ] Estabelecer critérios objetivos para versões alpha, beta e `1.0.0`.

## Comunidade e sustentabilidade

- [x] Adotar a licença MIT.
- [ ] Criar `CONTRIBUTING.md` com setup, testes e critérios de revisão.
- [ ] Adotar um código de conduta.
- [ ] Criar templates para bugs, propostas e pull requests.
- [ ] Manter uma lista pública de limitações e decisões fora de escopo.
- [ ] Reconhecer contribuidores e registrar decisões importantes de forma aberta.

## Critérios para a versão 1.0

- [ ] API pública documentada e com política de compatibilidade definida.
- [ ] CRUD, transações, parâmetros e hidratação validados nos três bancos.
- [ ] Nenhum ownership manual crítico ou vulnerabilidade SQL conhecida.
- [ ] Pacote instalável e exemplo consumido por um projeto externo limpo.
- [ ] CI, sanitizers, análise estática e testes de integração estáveis.
- [ ] Guia de início, referência, limitações e processo de contribuição publicados.
