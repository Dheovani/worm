#include <core/query-builder.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  class RecordingBuilder final : public worm::core::SqlBuilder
  {
  public:
    std::string select(
      const std::vector<worm::core::Field>& fields,
      const worm::core::Source& source,
      const std::vector<worm::core::Relation>& relations,
      const std::optional<worm::core::Filter>& filter = std::nullopt,
      const std::vector<worm::core::Ordering>& ordering = {}) const override
    {
      fieldsCount_ = fields.size();
      sourceName_ = source.name;
      relationsCount_ = relations.size();
      hasFilter_ = filter.has_value();
      orderingCount_ = ordering.size();
      return std::string{query_};
    }

    mutable std::size_t fieldsCount_{0};
    mutable std::string_view sourceName_;
    mutable std::size_t relationsCount_{0};
    mutable bool hasFilter_{false};
    mutable std::size_t orderingCount_{0};

  private:
    static constexpr std::string_view query_{"select delegated"};
  };
} // namespace

int main()
{
  using worm::core::Expression;
  using worm::core::Field;
  using worm::core::Filter;
  using worm::core::Join;
  using worm::core::OrderDirection;
  using worm::core::Ordering;
  using worm::core::Predicate;
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

  const auto query = queryBuilder.select(
    fields,
    users,
    relations,
    Filter{Predicate::equal("u.active", true)},
    {Ordering{"o.total", OrderDirection::Descending}});

  if (query != "select delegated") {
    std::cerr << "QueryBuilder did not return the SQL produced by the concrete builder.\n";
    return 1;
  }

  if (sqlBuilder.fieldsCount_ != 2 || sqlBuilder.sourceName_ != "users" ||
      sqlBuilder.relationsCount_ != 1 || !sqlBuilder.hasFilter_ || sqlBuilder.orderingCount_ != 1) {
    std::cerr << "QueryBuilder did not delegate the query envelope to the concrete builder.\n";
    return 1;
  }

  return 0;
}
