#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/output/result-set.hpp>

#include <errors/hydration-exception.hpp>
#include <reflection/lookup.hpp>

#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace worm::core
{
  namespace detail
  {
    [[nodiscard]]
    inline std::string decode_error_message(DecodeError error)
    {
      switch (error) {
      case DecodeError::IncompatibleType:
        return "incompatible column type";
      case DecodeError::OutOfRange:
        return "column value is out of range";
      case DecodeError::NullValue:
        return "column value is null";
      }

      return "unknown decode error";
    }

    [[nodiscard]]
    inline std::string hydration_error(std::string_view column, std::string_view reason)
    {
      return "Unable to hydrate column '" + std::string{column} + "': " + std::string{reason};
    }

    [[nodiscard]]
    inline bool has_column(const ResultRow& row, std::string_view columnName) noexcept
    {
      for (const ResultColumn& column : row.columns) {
        if (column.name == columnName) {
          return true;
        }
      }

      return false;
    }

    template <typename T>
    void validate_required_columns(const ResultRow& row)
    {
      std::apply(
        [&row](const auto&... field) {
          (
            [&row, &field] {
              if (field.isPersistent() && !has_column(row, field.columnName())) {
                throw HydrationException(hydration_error(field.columnName(), "required persistent column is missing"));
              }
            }(),
            ...);
        },
        fields_of<T>());
    }

    template <typename Entity, typename Field>
    void hydrate_field(Entity& entity, const Field& field, const ResultColumn& column)
    {
      using Value = typename std::remove_cvref_t<Field>::value_type;

      if constexpr (!DecodableParameter<Value>) {
        throw HydrationException(hydration_error(column.name, "field type is not decodable"));
      } else if constexpr (!std::assignable_from<Value&, Value>) {
        throw HydrationException(hydration_error(column.name, "field is not assignable"));
      } else {
        const DecodeResult<Value> decoded = decode<Value>(column.value);
        if (std::holds_alternative<DecodeError>(decoded)) {
          throw HydrationException(hydration_error(column.name, decode_error_message(std::get<DecodeError>(decoded))));
        }

        field.get(entity) = std::get<Value>(decoded);
      }
    }
  } // namespace detail

  template <PersistableEntity T>
  [[nodiscard]]
  T hydrate(const ResultRow& row)
  {
    detail::validate_required_columns<T>(row);

    T entity{};

    for (const ResultColumn& column : row.columns) {
      const bool hydrated = reflection::visit_column_descriptor<T>(column.name, [&entity, &column](const auto& field) {
        if (!field.isPersistent()) {
          throw HydrationException(detail::hydration_error(column.name, "column maps to an ignored field"));
        }

        detail::hydrate_field(entity, field, column);
      });

      if (!hydrated) {
        throw HydrationException(detail::hydration_error(column.name, "column does not exist in entity metadata"));
      }
    }

    return entity;
  }
} // namespace worm::core
