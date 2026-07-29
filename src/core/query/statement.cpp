#include <core/query/statement.hpp>

#include <cctype>
#include <string_view>
#include <vector>

namespace worm::core
{

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
      return equalsCaseInsensitive(token, "group")
          || equalsCaseInsensitive(token, "having")
          || equalsCaseInsensitive(token, "order")
          || equalsCaseInsensitive(token, "limit")
          || equalsCaseInsensitive(token, "offset")
          || equalsCaseInsensitive(token, "returning")
          || equalsCaseInsensitive(token, "union")
          || equalsCaseInsensitive(token, "except")
          || equalsCaseInsensitive(token, "intersect");
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

} // namespace worm::core
