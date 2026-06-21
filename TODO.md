# Roadmap

## Infraestrutura concluída

- [x] Organizar o código de produção em `src/`.
- [x] Configurar CMake com targets `Worm::*`.
- [x] Adicionar manifesto vcpkg reproduzível.
- [x] Integrar testes ao CTest fora de `src/`.
- [x] Cobrir os tipos de erro e os contratos básicos de conexão.
- [x] Cobrir helpers, hash, logger, factory e injeção de dependências.

## Reflexão

- [x] Implementar descritores de campos tipados com ponteiros para membros.
- [x] Definir o conceito C++20 `Reflectable`.
- [x] Implementar visitação `ForEachField` sem alocações ou funções virtuais.
- [ ] Adicionar metadados de ORM: coluna, chave primária, geração automática e campo ignorado.
- [ ] Implementar lookup opcional por nome com tratamento de colisão de hash.
- [ ] Criar snapshots de entidade e detecção de campos alterados.
- [x] Criar testes unitários separados para a fundação do subsistema de reflexão.

## Persistência e ORM

- [x] Criar a fundação parametrizada para expressões, cláusulas `WHERE` e ordenação.
- [ ] Reintroduzir o núcleo de entidades sobre o novo subsistema de reflexão.
- [ ] Implementar geração de `SELECT`, `INSERT`, `UPDATE` e `DELETE`.
- [ ] Introduzir parâmetros vinculados para evitar interpolação insegura de SQL.
- [ ] Definir mapeamento entre tipos C++ e tipos SQL.
- [ ] Implementar identidade, hidratação e rastreamento de entidades.
- [ ] Definir transações e uma unidade de trabalho.

## Conexões

- [ ] Adicionar destrutor virtual à interface `Client`.
- [ ] Remover `noexcept` de fábricas que podem falhar ao conectar.
- [ ] Padronizar nomes e tratamento de erros entre os três drivers.
- [ ] Criar testes de integração descartáveis para MySQL e PostgreSQL.
- [ ] Separar drivers opcionais para não exigir todos os bancos em cada build.

## Qualidade e distribuição

- [ ] Adicionar análise estática e formatação automática.
- [ ] Adicionar cobertura de testes.
- [ ] Configurar CI para Windows e Linux.
- [ ] Criar regras CMake de instalação e exportação do pacote.
- [ ] Definir versionamento, licença e política de contribuição.
- [ ] Criar documentação de API e exemplos de uso.
