#include <utils/n-plus-one-detector.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>
#include <string>

int main()
{
  worm::utils::NPlusOneDetector detector;
  detector.record({"select * from posts where user_id = ?", {std::int64_t{1}}});
  detector.record({"select * from posts where user_id = ?", {std::int64_t{2}}});

  const auto warnings = detector.warnings();
  if (warnings.size() != 1 || warnings[0].sql != "select * from posts where user_id = ?" ||
      warnings[0].executions != 2 || warnings[0].distinctParameterSets != 2) {
    std::cerr << "N+1 detector did not report repeated parameterized SELECT statements.\n";
    return 1;
  }

  detector.record({"select * from posts where user_id = ?", {std::int64_t{2}}});
  const auto repeatedParameterWarnings = detector.warnings();
  if (repeatedParameterWarnings.size() != 1 || repeatedParameterWarnings[0].executions != 3 ||
      repeatedParameterWarnings[0].distinctParameterSets != 2) {
    std::cerr << "N+1 detector did not separate executions from distinct parameter sets.\n";
    return 1;
  }

  worm::utils::NPlusOneDetector strictDetector{3};
  strictDetector.record({"select * from posts where user_id = ?", {std::int64_t{1}}});
  strictDetector.record({"select * from posts where user_id = ?", {std::int64_t{2}}});
  if (!strictDetector.warnings().empty()) {
    std::cerr << "N+1 detector ignored its configured minimum execution threshold.\n";
    return 1;
  }

  strictDetector.record({"select * from posts where user_id = ?", {std::int64_t{3}}});
  if (strictDetector.warnings().size() != 1) {
    std::cerr << "N+1 detector did not report after reaching its threshold.\n";
    return 1;
  }

  worm::utils::NPlusOneDetector ignoredStatements;
  ignoredStatements.record({"update posts set title = ? where id = ?", {std::string{"Ada"}, std::int64_t{1}}});
  ignoredStatements.record({"select * from posts"});
  ignoredStatements.record({"select * from posts where user_id = ?", {std::int64_t{1}}});
  ignoredStatements.record({"select * from profiles where user_id = ?", {std::int64_t{1}}});
  if (!ignoredStatements.warnings().empty() || ignoredStatements.executionCount() != 4) {
    std::cerr << "N+1 detector reported unrelated or non-parameterized statements.\n";
    return 1;
  }

  ignoredStatements.clear();
  if (!ignoredStatements.warnings().empty() || ignoredStatements.executionCount() != 0) {
    std::cerr << "N+1 detector did not clear its state.\n";
    return 1;
  }

  try {
    static_cast<void>(worm::utils::NPlusOneDetector{1});
    std::cerr << "N+1 detector accepted an invalid threshold.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  return 0;
}
