#include <core/model/constraint.hpp>

#include <iostream>

int main()
{
  constexpr worm::core::Table users{"users"};
  constexpr worm::core::Table posts{"posts"};
  constexpr worm::core::Column userId{"id", users};
  constexpr worm::core::Column postUserId{"user_id", posts};
  constexpr worm::core::Column userEmail{"email", users};
  constexpr worm::core::PrimaryKey emptyPrimaryKey{"pk_empty", {}};
  constexpr worm::core::PrimaryKey userPrimaryKey{"pk_users", {userId}};
  constexpr worm::core::ForeignKey postUserForeignKey{"fk_posts_users",
    {postUserId},
    users,
    {userId},
    {{worm::core::Operation::Delete, worm::core::ReferentialAction::Cascade},
      {worm::core::Operation::Update, worm::core::ReferentialAction::Restrict}}};
  constexpr worm::core::Index uniqueUserEmail{"idx_users_email", {{userEmail}}, true};
  constexpr worm::core::Index descendingUserEmail{"idx_users_email_desc",
    {{userEmail, worm::core::IndexOrder::Descending}}};

  static_assert(emptyPrimaryKey.empty());
  static_assert(userPrimaryKey.name() == "pk_users");
  static_assert(!userPrimaryKey.empty());
  static_assert(userPrimaryKey.columns().size() == 1);
  static_assert(userPrimaryKey.columns().front() == userId);
  static_assert(postUserForeignKey.name() == "fk_posts_users");
  static_assert(postUserForeignKey.columns().front() == postUserId);
  static_assert(postUserForeignKey.referencedTable() == users);
  static_assert(postUserForeignKey.referencedColumns().front() == userId);
  static_assert(
    postUserForeignKey.referentialActionFor(worm::core::Operation::Delete) == worm::core::ReferentialAction::Cascade);
  static_assert(
    postUserForeignKey.referentialActionFor(worm::core::Operation::Update) == worm::core::ReferentialAction::Restrict);
  static_assert(
    postUserForeignKey.referentialActionFor(worm::core::Operation::Insert) == worm::core::ReferentialAction::NoAction);
  static_assert(uniqueUserEmail.unique());
  static_assert(uniqueUserEmail.columns().front().column == userEmail);
  static_assert(uniqueUserEmail.columns().front().order == worm::core::IndexOrder::Ascending);
  static_assert(!descendingUserEmail.unique());
  static_assert(descendingUserEmail.columns().front().order == worm::core::IndexOrder::Descending);

  if (postUserForeignKey.referentialActions().size() != 2 || uniqueUserEmail.columns().size() != 1) {
    std::cerr << "Constraint metadata did not preserve copied column and action values.\n";
    return 1;
  }

  return 0;
}
