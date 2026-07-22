#include <core/sql-builder.hpp>

#include <connection/client.hpp>
#include <errors/database-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <string>

namespace worm::core
{

	namespace
	{

    const std::string listSelectFields(const std::vector<worm::core::Field>& fields)
		{
      std::string list;

			for (int i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];

        list += std::string{field.source_.alias_.value_or(field.source_.name_)};
        list += ".";
        list += std::string{field.name_};

				if (i < fields.size())
          list += ",";
			}

			return list;
		}

		const std::string getJoinClause(const Join type)
		{
			using enum Join;

			switch (type) {
			case Inner:
				return "inner join";
			case Left:
				return "left join";
			case Right:
				return "right join";
			case Full:
				return "full join";
			}

			return "inner join";
		}

		const std::string buildRelations(const std::vector<Relation>& relations)
		{
      std::string list;

			for (auto& rel : relations) {
        list += getJoinClause(rel.type_) + " ";
        list += std::string{rel.left_.name_} + " ";

				if (rel.left_.alias_.has_value())
          list += std::string{rel.left_.alias_.value()};

				list += " on (";
        list += rel.expression_.sql;
        list += ")";
			}

      return list;
    }

	} // namespace

  const std::string_view PgBuilder::select(
		const std::vector<worm::core::Field>& fields,
		const Source& source,
		const std::vector<Relation>& relations) const noexcept
	{
    const std::string fieldsList = listSelectFields(fields);
    const std::string sourceName = std::string{source.name_} + " " + std::string{source.alias_.value_or("")};
    const std::string _relations = buildRelations(relations);
		const std::string sql = "select " + fieldsList + " from " + sourceName + " " + _relations;

    return std::string_view{sql};
	}

	[[nodiscard]]
  const auto getBuilder()
	{
		const auto type = worm::DependencyInjector<worm::DatabaseType>().get();

		switch (type) {
    case DatabaseType::PostgreSQL:
      return PgBuilder();
    case DatabaseType::MySQL:
    case DatabaseType::SQLite:
    default:
      throw worm::DatabaseException("Unsupported database type.");
		}
	}

}
