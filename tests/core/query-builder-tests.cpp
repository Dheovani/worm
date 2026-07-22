#include <core/query-builder.hpp>

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
  class RecordingBuilder final : public worm::core::Builder
  {
  public:
    const std::string_view select(const std::vector<worm::core::Field>& fields, const worm::core::Source& source,
                                  const std::vector<worm::core::Relation>& relations) const noexcept override
    {
      fieldsCount_ = fields.size();
      sourceName_ = source.name_;
      relationsCount_ = relations.size();
      return query_;
    }

    mutable std::size_t fieldsCount_{0};
    mutable std::string_view sourceName_;
    mutable std::size_t relationsCount_{0};

  private:
    static constexpr std::string_view query_{"select delegated"};
  };
} // namespace

int main()
{
  using worm::core::Expression;
  using worm::core::Field;
  using worm::core::Join;
  using worm::core::QueryBuilder;
  using worm::core::Relation;
  using worm::core::Source;

  const RecordingBuilder sqlBuilder;
  const QueryBuilder queryBuilder{sqlBuilder};

  const Source users{"users", "u"};
  const Source orders{"orders", "o"};
  const std::vector<Field> fields{
      Field{"id", users},
      Field{"total", orders},
  };
  const std::vector<Relation> relations{
      Relation{Join::Inner, users, orders, Expression{"u.id = o.user_id", {}}},
  };

  const auto query = queryBuilder.select(fields, users, relations);

  if (query != "select delegated") {
    std::cerr << "QueryBuilder did not return the SQL produced by the concrete builder.\n";
    return 1;
  }

  if (sqlBuilder.fieldsCount_ != 2 || sqlBuilder.sourceName_ != "users" || sqlBuilder.relationsCount_ != 1) {
    std::cerr << "QueryBuilder did not delegate the query envelope to the concrete builder.\n";
    return 1;
  }

  return 0;
}
