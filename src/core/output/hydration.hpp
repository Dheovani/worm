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

    template <Model T>
    [[nodiscard]]
    inline std::string hydrationError(std::string_view reason)
    {
      return "Unable to hydrate " + std::string{modelKind<T>()} + " '" + std::string{modelName<T>()} +
             "' during SELECT: " + std::string{reason};
    }

    template <Model T, typename Field>
    [[nodiscard]]
    inline std::string hydrationFieldError(const Field& field, std::string_view reason)
    {
      return hydrationError<T>(
        "field '" + std::string{field.name()} + "' mapped to column '" + std::string{field.columnName()} + "' " +
        std::string{reason});
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
                throw HydrationException(hydrationFieldError<T>(field, "is missing from the result row"));
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
        throw HydrationException(hydrationFieldError<Entity>(field, "has a type that is not decodable"));
      } else if constexpr (!std::assignable_from<Value&, Value>) {
        throw HydrationException(hydrationFieldError<Entity>(field, "is not assignable"));
      } else {
        const DecodeResult<Value> decoded = decode<Value>(column.value);
        if (std::holds_alternative<DecodeError>(decoded)) {
          throw HydrationException(
            hydrationFieldError<Entity>(
              field,
              "could not be decoded: " + decode_error_message(std::get<DecodeError>(decoded))));
        }

        field.get(entity) = std::get<Value>(decoded);
      }
    }
  } // namespace detail

  template <Model T>
  [[nodiscard]]
  T hydrate(const ResultRow& row)
  {
    detail::validate_required_columns<T>(row);

    T entity{};

    for (const ResultColumn& column : row.columns) {
      const bool hydrated = reflection::visit_column_descriptor<T>(column.name, [&entity, &column](const auto& field) {
        if (!field.isPersistent()) {
          throw HydrationException(
            detail::hydrationFieldError<T>(field, "is ignored and cannot receive a result value"));
        }

        detail::hydrate_field(entity, field, column);
      });

      if (!hydrated) {
        throw HydrationException(
          detail::hydrationError<T>("result column '" + column.name + "' is not mapped to a reflected field"));
      }
    }

    return entity;
  }
} // namespace worm::core
