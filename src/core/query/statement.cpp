#include <core/query/statement.hpp>

#include <algorithm>
#include <cctype>
#include <functional>
#include <string_view>
#include <vector>

namespace worm::core
{
  std::size_t StatementHash::operator()(const Statement& statement) const noexcept
  {
    constexpr std::size_t hashConstant = 0x9e3779b9;
    std::size_t result = std::hash<std::string>{}(statement.sql);

    for (const Parameter& parameter : statement.parameters) {
      std::size_t parameterHash = parameter.index();
      std::visit(
        [&parameterHash](const auto& value) {
          using Value = std::decay_t<decltype(value)>;

          if constexpr (std::is_same_v<Value, std::nullptr_t>) {
            parameterHash = 0;
          } else {
            parameterHash ^= std::hash<Value>{}(value) + hashConstant + (parameterHash << 6U) + (parameterHash >> 2U);
          }
        },
        parameter);

      result ^= parameterHash + hashConstant + (result << 6U) + (result >> 2U);
    }

    return result;
  }

  namespace
  {
    enum class TokenKind
    {
      Identifier,
      Dot,
      LeftParenthesis,
      RightParenthesis,
      Other
    };

    struct Token
    {
      TokenKind kind;
      std::string_view text;
    };

    [[nodiscard]]
    bool isIdStart(char character)
    {
      const auto value = static_cast<unsigned char>(character);
      return std::isalpha(value) || character == '_';
    }

    [[nodiscard]]
    bool isIdContinue(char character)
    {
      const auto value = static_cast<unsigned char>(character);
      return std::isalnum(value) || character == '_' || character == '$';
    }

    [[nodiscard]]
    bool equalsCaseInsensitive(std::string_view left, std::string_view right)
    {
      if (left.size() != right.size())
        return false;

      for (std::size_t index = 0; index < left.size(); ++index) {
        const auto lhs = static_cast<unsigned char>(left[index]);
        const auto rhs = static_cast<unsigned char>(right[index]);

        if (std::tolower(lhs) != std::tolower(rhs))
          return false;
      }

      return true;
    }

    [[nodiscard]]
    std::string_view removeIdentifierQuotes(std::string_view identifier)
    {
      if (identifier.size() < 2)
        return identifier;

      const char first = identifier.front();
      const char last = identifier.back();

      if ((first == '"' && last == '"') || (first == '`' && last == '`') || (first == '[' && last == ']')) {
        return identifier.substr(1, identifier.size() - 2);
      }

      return identifier;
    }

    [[nodiscard]]
    bool isBlank(std::string_view value) noexcept
    {
      return std::ranges::all_of(value, [](unsigned char ch) { return std::isspace(ch); });
    }

    [[nodiscard]]
    std::vector<Token> tokenize(std::string_view sql)
    {
      std::vector<Token> tokens;

      for (std::size_t index = 0; index < sql.size();) {
        const char current = sql[index];

        if (std::isspace(static_cast<unsigned char>(current))) {
          ++index;
          continue;
        }

        if (current == '-' && index + 1 < sql.size() && sql[index + 1] == '-') {
          index += 2;

          while (index < sql.size() && sql[index] != '\n')
            ++index;

          continue;
        }

        if (current == '/' && index + 1 < sql.size() && sql[index + 1] == '*') {
          index += 2;

          while (index + 1 < sql.size() && !(sql[index] == '*' && sql[index + 1] == '/')) {
            ++index;
          }

          if (index + 1 < sql.size())
            index += 2;

          continue;
        }

        if (current == '\'') {
          ++index;

          while (index < sql.size()) {
            if (sql[index] != '\'') {
              ++index;
              continue;
            }

            if (index + 1 < sql.size() && sql[index + 1] == '\'') {
              index += 2;
              continue;
            }

            ++index;
            break;
          }

          continue;
        }

        if (current == '"' || current == '`' || current == '[') {
          const char closing = current == '[' ? ']' : current;
          const std::size_t begin = index++;

          while (index < sql.size()) {
            if (sql[index] != closing) {
              ++index;
              continue;
            }

            if (closing != ']' && index + 1 < sql.size() && sql[index + 1] == closing) {
              index += 2;
              continue;
            }

            ++index;
            break;
          }

          tokens.push_back({TokenKind::Identifier, sql.substr(begin, index - begin)});

          continue;
        }

        if (isIdStart(current)) {
          const std::size_t begin = index++;

          while (index < sql.size() && isIdContinue(sql[index])) {
            ++index;
          }

          tokens.push_back({TokenKind::Identifier, sql.substr(begin, index - begin)});

          continue;
        }

        switch (current) {
        case '.':
          tokens.push_back({TokenKind::Dot, sql.substr(index, 1)});
          break;

        case '(':
          tokens.push_back({TokenKind::LeftParenthesis, sql.substr(index, 1)});
          break;

        case ')':
          tokens.push_back({TokenKind::RightParenthesis, sql.substr(index, 1)});
          break;

        default:
          tokens.push_back({TokenKind::Other, sql.substr(index, 1)});
          break;
        }

        ++index;
      }

      return tokens;
    }

    [[nodiscard]]
    bool isWhereTerminator(std::string_view token)
    {
      return equalsCaseInsensitive(token, "group") || equalsCaseInsensitive(token, "having") ||
             equalsCaseInsensitive(token, "order") || equalsCaseInsensitive(token, "limit") ||
             equalsCaseInsensitive(token, "offset") || equalsCaseInsensitive(token, "returning") ||
             equalsCaseInsensitive(token, "union") || equalsCaseInsensitive(token, "except") ||
             equalsCaseInsensitive(token, "intersect");
    }
  } // namespace

  bool hasFilterWhere(std::string_view sql, std::string_view qualifier)
  {
    const auto tokens = tokenize(sql);

    std::size_t where_index = tokens.size();
    std::size_t depth = 0;

    for (std::size_t index = 0; index < tokens.size(); ++index) {
      const Token& token = tokens[index];

      if (token.kind == TokenKind::LeftParenthesis) {
        ++depth;
        continue;
      }

      if (token.kind == TokenKind::RightParenthesis) {
        if (depth > 0)
          --depth;

        continue;
      }

      if (depth == 0 && token.kind == TokenKind::Identifier && equalsCaseInsensitive(token.text, "where")) {
        where_index = index + 1;
        break;
      }
    }

    if (where_index == tokens.size())
      return false;

    depth = 0;

    for (std::size_t index = where_index; index < tokens.size(); ++index) {
      const Token& token = tokens[index];

      if (token.kind == TokenKind::LeftParenthesis) {
        ++depth;
        continue;
      }

      if (token.kind == TokenKind::RightParenthesis) {
        if (depth > 0)
          --depth;

        continue;
      }

      if (depth == 0 && token.kind == TokenKind::Identifier && isWhereTerminator(token.text)) {
        break;
      }

      if (index + 2 < tokens.size() && token.kind == TokenKind::Identifier &&
          tokens[index + 1].kind == TokenKind::Dot && tokens[index + 2].kind == TokenKind::Identifier) {
        const auto candidate = removeIdentifierQuotes(token.text);

        if (equalsCaseInsensitive(candidate, qualifier))
          return true;
      }
    }

    return false;
  }

  std::vector<std::string> splitStatementQueries(std::string_view sql)
  {
    std::vector<std::string> statements;
    std::string current;

    bool singleQuoted = false;
    bool doubleQuoted = false;
    bool backtickQuoted = false;
    bool bracketQuoted = false;
    bool lineComment = false;
    bool blockComment = false;

    for (std::size_t i = 0; i < sql.size(); ++i) {
      const char ch = sql[i];

      if (lineComment) {
        current += ch;
        if (ch == '\n')
          lineComment = false;
        continue;
      }

      if (blockComment) {
        current += ch;
        if (ch == '*' && i + 1 < sql.size() && sql[i + 1] == '/') {
          current += '/';
          ++i;
          blockComment = false;
        }
        continue;
      }

      const bool quoted = singleQuoted || doubleQuoted || backtickQuoted || bracketQuoted;
      if (!quoted && ch == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
        current += "--";
        ++i;
        lineComment = true;
        continue;
      }

      if (!quoted && ch == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
        current += "/*";
        ++i;
        blockComment = true;
        continue;
      }

      if (ch == '\'' && !doubleQuoted && !backtickQuoted && !bracketQuoted) {
        if (singleQuoted && i + 1 < sql.size() && sql[i + 1] == '\'') {
          current += "''";
          ++i;
          continue;
        }

        singleQuoted = !singleQuoted;
      }

      if (ch == '"' && !singleQuoted && !backtickQuoted && !bracketQuoted) {
        if (doubleQuoted && i + 1 < sql.size() && sql[i + 1] == '"') {
          current += "\"\"";
          ++i;
          continue;
        }
        doubleQuoted = !doubleQuoted;
      }

      if (ch == '`' && !singleQuoted && !doubleQuoted && !bracketQuoted) {
        if (backtickQuoted && i + 1 < sql.size() && sql[i + 1] == '`') {
          current += "``";
          ++i;
          continue;
        }
        backtickQuoted = !backtickQuoted;
      }

      if (ch == '[' && !singleQuoted && !doubleQuoted && !backtickQuoted && !bracketQuoted)
        bracketQuoted = true;
      else if (ch == ']' && bracketQuoted)
        bracketQuoted = false;

      if (ch == ';' && !singleQuoted && !doubleQuoted && !backtickQuoted && !bracketQuoted) {
        if (!isBlank(current)) {
          statements.emplace_back(std::move(current));
          current.clear();
        }

        continue;
      }

      current += ch;
    }

    if (!isBlank(current)) {
      statements.emplace_back(std::move(current));
    }

    return statements;
  }

} // namespace worm::core
