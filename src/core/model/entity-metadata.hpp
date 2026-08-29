#pragma once

#include <core/model/entity.hpp>
#include <core/model/schema-metadata.hpp>
#include <core/query/parameter-value.hpp>
#include <errors/mapping-exception.hpp>
#include <reflection/visit.hpp>
#include <utils/helpers.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace worm::core
{
  namespace detail
  {
    struct PrimaryKeyFieldSelector
    {
      template <typename T, typename Field>
      static constexpr bool matches(Field field)
      {
        if (!field.isPersistent()) {
          return false;
        }

        constexpr auto primaryKey = std::remove_cvref_t<T>::primaryKey();
        for (const auto& column : primaryKey.columns()) {
          if (column.columnName == field.columnName()) {
            return true;
          }
        }

        return false;
      }
    };

    struct PersistentFieldSelector
    {
      template <typename T, typename Field>
      static constexpr bool matches(Field field)
      {
        return field.isPersistent();
      }
    };

    template <typename Selector, typename T, std::size_t Index>
    constexpr auto selected_field_tuple()
    {
      constexpr auto fields = std::remove_cvref_t<T>::reflect();
      constexpr auto field = std::get<Index>(fields);

      if constexpr (Selector::template matches<T>(field)) {
        return std::tuple{field};
      } else {
        return std::tuple{};
      }
    }

    template <typename Selector, typename T, std::size_t... Index>
    constexpr auto selected_fields_impl(std::index_sequence<Index...>)
    {
      return std::tuple_cat(selected_field_tuple<Selector, T, Index>()...);
    }

    template <typename Selector, typename T>
    constexpr auto selected_fields_of()
    {
      using EntityType = std::remove_cvref_t<T>;
      using Fields = std::remove_cvref_t<decltype(EntityType::reflect())>;

      return selected_fields_impl<Selector, EntityType>(std::make_index_sequence<std::tuple_size_v<Fields>>{});
    }

    template <typename T>
    struct column_value
    {
      using type = std::remove_cvref_t<T>;
    };

    template <typename T>
    struct column_value<std::optional<T>>
    {
      using type = T;
    };

    template <typename T>
    using column_value_t = typename column_value<std::remove_cvref_t<T>>::type;

    template <typename T>
    consteval bool is_column_type_mappable()
    {
      using Value = column_value_t<T>;
      constexpr bool losslessInteger =
        std::integral<Value> && (std::is_signed_v<Value> || sizeof(Value) < sizeof(std::uint64_t));
      constexpr bool supportedFloat = std::same_as<Value, float> || std::same_as<Value, double>;
      constexpr bool supportedEnum = [] {
        if constexpr (std::is_enum_v<Value>) {
          return is_column_type_mappable<std::underlying_type_t<Value>>();
        }
        return false;
      }();
      return std::same_as<Value, bool> || losslessInteger || supportedFloat || supportedEnum ||
             utils::is_string_like<Value> || utils::is_date_type<Value> || std::same_as<Value, Decimal> ||
             std::same_as<Value, Binary>;
    }

    template <typename T>
    [[nodiscard]]
    ColumnType inferredColumnType()
    {
      using Value = column_value_t<T>;
      static_assert(is_column_type_mappable<Value>(), "Worm cannot infer a SQL column type for this C++ field type.");

      if constexpr (std::same_as<Value, bool>) {
        return {.kind = ColumnTypeKind::Boolean};
      } else if constexpr (std::is_enum_v<Value>) {
        return inferredColumnType<std::underlying_type_t<Value>>();
      } else if constexpr (std::integral<Value>) {
        constexpr ColumnTypeKind kind = sizeof(Value) <= sizeof(std::int16_t)
          ? ColumnTypeKind::Int16 : sizeof(Value) <= sizeof(std::int32_t)
            ? ColumnTypeKind::Int32 : ColumnTypeKind::Int64;
        return {.kind = kind, .unsignedValue = std::is_unsigned_v<Value>};
      } else if constexpr (std::same_as<Value, float> || std::same_as<Value, double>) {
        return {.kind = sizeof(Value) <= sizeof(float) ? ColumnTypeKind::Float32 : ColumnTypeKind::Float64};
      } else if constexpr (std::same_as<Value, Decimal>) {
        return {.kind = ColumnTypeKind::Decimal};
      } else if constexpr (std::same_as<Value, Binary>) {
        return {.kind = ColumnTypeKind::Binary};
      } else if constexpr (utils::is_date_type<Value>) {
        return {.kind = ColumnTypeKind::Date};
      } else {
        return {.kind = ColumnTypeKind::String};
      }
    }

    template <typename T>
    concept HasColumnTypeMapping = requires(std::string_view column) {
      { std::remove_cvref_t<T>::columnType(column) } -> std::same_as<ColumnType>;
    };

    template <typename T>
    concept HasIndexes = requires { std::remove_cvref_t<T>::indexes(); };

    template <typename T>
    concept HasForeignKeys = requires { std::remove_cvref_t<T>::foreignKeys(); };

    template <typename Value, typename Tuple>
    void appendTuple(std::vector<Value>& target, Tuple&& tuple)
    {
      std::apply(
        [&](const auto&... values) {
          static_assert((std::same_as<std::remove_cvref_t<decltype(values)>, Value> && ...));
          (target.push_back(values), ...);
        },
        std::forward<Tuple>(tuple));
    }
  } // namespace detail

  template <Entity T>
  [[nodiscard]]
  constexpr Table table_of()
  {
    return std::remove_cvref_t<T>::table();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr bool has_valid_table()
  {
    return !table_of<T>().empty();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr auto fields_of()
  {
    return std::remove_cvref_t<T>::reflect();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr auto persistent_fields_of()
  {
    return detail::selected_fields_of<detail::PersistentFieldSelector, T>();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr auto primary_key_fields_of()
  {
    return detail::selected_fields_of<detail::PrimaryKeyFieldSelector, T>();
  }

  template <Entity T>
  inline constexpr std::size_t persistent_field_count =
    std::tuple_size_v<std::remove_cvref_t<decltype(persistent_fields_of<T>())>>;

  template <Entity T>
  inline constexpr std::size_t primary_key_count =
    std::tuple_size_v<std::remove_cvref_t<decltype(primary_key_fields_of<T>())>>;

  template <Entity T>
  inline constexpr bool has_single_primary_key = primary_key_count<T> == 1;

  template <Entity T>
  [[nodiscard]]
  constexpr bool has_primary_key()
  {
    return primary_key_count<T> > 0;
  }

  template <Entity T>
  [[nodiscard]]
  constexpr bool is_valid_entity()
  {
    return has_valid_table<T>() && persistent_field_count<T> > 0 && has_single_primary_key<T>;
  }

  template <typename T>
  concept PersistableEntity = Entity<T> && is_valid_entity<std::remove_cvref_t<T>>();

  template <PersistableEntity T>
  [[nodiscard]]
  constexpr auto primary_key_field_of()
  {
    return std::get<0>(primary_key_fields_of<T>());
  }

  template <typename T>
  concept MappableColumn = detail::is_column_type_mappable<T>();

  template <MappableColumn T>
  [[nodiscard]]
  ColumnType column_type_of()
  {
    return detail::inferredColumnType<T>();
  }

  template <PersistableEntity T>
  [[nodiscard]]
  TableMetadata table_metadata_of()
  {
    const Table table = table_of<T>();
    std::vector<ColumnMetadata> columns;
    columns.reserve(persistent_field_count<T>);

    std::apply(
      [&](const auto&... fields) {
        (
          [&] {
            using Field = std::remove_cvref_t<decltype(fields)>;
            using Value = typename Field::value_type;
            ColumnType type;
            if constexpr (detail::HasColumnTypeMapping<T>) {
              type = std::remove_cvref_t<T>::columnType(fields.columnName());
              if (type.kind == ColumnTypeKind::Unknown) {
                if constexpr (MappableColumn<Value>) {
                  type = column_type_of<Value>();
                } else {
                  throw MappingException(
                    "Entity column '{}.{}' has no SQL type mapping.",
                    table.name(),
                    fields.columnName());
                }
              }
            } else {
              type = column_type_of<Value>();
            }
            reflection::FieldMetadata metadata = fields.metadata();
            metadata.columnName = fields.columnName();
            columns.emplace_back(Column{metadata, table}, std::move(type));
          }(),
          ...);
      },
      persistent_fields_of<T>());

    std::vector<Index> indexes;
    if constexpr (detail::HasIndexes<T>) {
      detail::appendTuple(indexes, std::remove_cvref_t<T>::indexes());
    }

    std::vector<ForeignKey> foreignKeys;
    if constexpr (detail::HasForeignKeys<T>) {
      detail::appendTuple(foreignKeys, std::remove_cvref_t<T>::foreignKeys());
    }

    return TableMetadata{table,
      std::move(columns),
      std::remove_cvref_t<T>::primaryKey(),
      std::move(indexes),
      std::move(foreignKeys)};
  }

  template <PersistableEntity... T>
    requires(sizeof...(T) > 0)
  [[nodiscard]]
  SchemaMetadata schema_metadata_of()
  {
    std::vector<TableMetadata> tables;
    tables.reserve(sizeof...(T));
    (tables.push_back(table_metadata_of<T>()), ...);

    const Schema schema = tables.front().table().schema();
    return SchemaMetadata{schema, std::move(tables)};
  }

  template <Viewable T>
  [[nodiscard]]
  constexpr View view_of()
  {
    return std::remove_cvref_t<T>::view();
  }

  template <Viewable T>
  [[nodiscard]]
  constexpr bool has_valid_view()
  {
    return !view_of<T>().empty();
  }

  template <Viewable T>
  [[nodiscard]]
  constexpr auto fields_of()
  {
    return std::remove_cvref_t<T>::reflect();
  }

  template <Viewable T>
  inline constexpr std::size_t view_field_count = reflection::field_count<T>;

  template <Viewable T>
  [[nodiscard]]
  constexpr bool is_valid_view()
  {
    return has_valid_view<T>() && view_field_count<T> > 0;
  }

  template <typename T>
  concept QueryableView = Viewable<T> && is_valid_view<std::remove_cvref_t<T>>();

  template <typename T>
  concept Model = PersistableEntity<T> || QueryableView<T>;
} // namespace worm::core
