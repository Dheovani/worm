#include <core/model/migration-artifact.hpp>

#include <errors/migration-exception.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace worm::core
{
  namespace
  {
    class Sha256
    {
    public:
      void update(std::string_view value)
      {
        for (const unsigned char byte : value) {
          buffer_[bufferSize_++] = byte;
          bitCount_ += 8;
          if (bufferSize_ == buffer_.size()) {
            transform();
            bufferSize_ = 0;
          }
        }
      }

      [[nodiscard]]
      std::array<std::uint8_t, 32> finish()
      {
        buffer_[bufferSize_++] = 0x80U;
        if (bufferSize_ > 56) {
          while (bufferSize_ < buffer_.size()) {
            buffer_[bufferSize_++] = 0;
          }
          transform();
          bufferSize_ = 0;
        }

        while (bufferSize_ < 56) {
          buffer_[bufferSize_++] = 0;
        }

        for (std::size_t index = 0; index < 8; ++index) {
          buffer_[63 - index] = static_cast<std::uint8_t>(bitCount_ >> (index * 8U));
        }
        transform();

        std::array<std::uint8_t, 32> digest{};
        for (std::size_t index = 0; index < state_.size(); ++index) {
          digest[index * 4] = static_cast<std::uint8_t>(state_[index] >> 24U);
          digest[index * 4 + 1] = static_cast<std::uint8_t>(state_[index] >> 16U);
          digest[index * 4 + 2] = static_cast<std::uint8_t>(state_[index] >> 8U);
          digest[index * 4 + 3] = static_cast<std::uint8_t>(state_[index]);
        }
        return digest;
      }

    private:
      [[nodiscard]]
      static constexpr std::uint32_t choose(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
      {
        return (x & y) ^ (~x & z);
      }

      [[nodiscard]]
      static constexpr std::uint32_t majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
      {
        return (x & y) ^ (x & z) ^ (y & z);
      }

      [[nodiscard]]
      static constexpr std::uint32_t upperSigma0(std::uint32_t value) noexcept
      {
        return std::rotr(value, 2) ^ std::rotr(value, 13) ^ std::rotr(value, 22);
      }

      [[nodiscard]]
      static constexpr std::uint32_t upperSigma1(std::uint32_t value) noexcept
      {
        return std::rotr(value, 6) ^ std::rotr(value, 11) ^ std::rotr(value, 25);
      }

      [[nodiscard]]
      static constexpr std::uint32_t lowerSigma0(std::uint32_t value) noexcept
      {
        return std::rotr(value, 7) ^ std::rotr(value, 18) ^ (value >> 3U);
      }

      [[nodiscard]]
      static constexpr std::uint32_t lowerSigma1(std::uint32_t value) noexcept
      {
        return std::rotr(value, 17) ^ std::rotr(value, 19) ^ (value >> 10U);
      }

      void transform() noexcept
      {
        constexpr std::array<std::uint32_t, 64> constants{
          0x428a2f98U,
          0x71374491U,
          0xb5c0fbcfU,
          0xe9b5dba5U,
          0x3956c25bU,
          0x59f111f1U,
          0x923f82a4U,
          0xab1c5ed5U,
          0xd807aa98U,
          0x12835b01U,
          0x243185beU,
          0x550c7dc3U,
          0x72be5d74U,
          0x80deb1feU,
          0x9bdc06a7U,
          0xc19bf174U,
          0xe49b69c1U,
          0xefbe4786U,
          0x0fc19dc6U,
          0x240ca1ccU,
          0x2de92c6fU,
          0x4a7484aaU,
          0x5cb0a9dcU,
          0x76f988daU,
          0x983e5152U,
          0xa831c66dU,
          0xb00327c8U,
          0xbf597fc7U,
          0xc6e00bf3U,
          0xd5a79147U,
          0x06ca6351U,
          0x14292967U,
          0x27b70a85U,
          0x2e1b2138U,
          0x4d2c6dfcU,
          0x53380d13U,
          0x650a7354U,
          0x766a0abbU,
          0x81c2c92eU,
          0x92722c85U,
          0xa2bfe8a1U,
          0xa81a664bU,
          0xc24b8b70U,
          0xc76c51a3U,
          0xd192e819U,
          0xd6990624U,
          0xf40e3585U,
          0x106aa070U,
          0x19a4c116U,
          0x1e376c08U,
          0x2748774cU,
          0x34b0bcb5U,
          0x391c0cb3U,
          0x4ed8aa4aU,
          0x5b9cca4fU,
          0x682e6ff3U,
          0x748f82eeU,
          0x78a5636fU,
          0x84c87814U,
          0x8cc70208U,
          0x90befffaU,
          0xa4506cebU,
          0xbef9a3f7U,
          0xc67178f2U,
        };

        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
          const std::size_t offset = index * 4;
          words[index] = (static_cast<std::uint32_t>(buffer_[offset]) << 24U) |
                         (static_cast<std::uint32_t>(buffer_[offset + 1]) << 16U) |
                         (static_cast<std::uint32_t>(buffer_[offset + 2]) << 8U) |
                         static_cast<std::uint32_t>(buffer_[offset + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
          words[index] =
            lowerSigma1(words[index - 2]) + words[index - 7] + lowerSigma0(words[index - 15]) + words[index - 16];
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (std::size_t index = 0; index < words.size(); ++index) {
          const std::uint32_t first = h + upperSigma1(e) + choose(e, f, g) + constants[index] + words[index];
          const std::uint32_t second = upperSigma0(a) + majority(a, b, c);
          h = g;
          g = f;
          f = e;
          e = d + first;
          d = c;
          c = b;
          b = a;
          a = first + second;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
      }

      std::array<std::uint32_t, 8> state_{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U,
      };
      std::array<std::uint8_t, 64> buffer_{};
      std::size_t bufferSize_{0};
      std::uint64_t bitCount_{0};
    };

    void addChecksumValue(Sha256& hash, std::string_view value)
    {
      hash.update(std::to_string(value.size()));
      hash.update(":");
      hash.update(value);
      hash.update(";");
    }

    void addChecksumStatements(Sha256& hash, const std::vector<MigrationStatement>& statements)
    {
      addChecksumValue(hash, std::to_string(statements.size()));
      for (const MigrationStatement& statement : statements) {
        addChecksumValue(hash, statement.description);
        addChecksumValue(hash, statement.sql);
        addChecksumValue(hash, std::to_string(static_cast<unsigned int>(statement.risk)));
      }
    }

    [[nodiscard]]
    std::string digestString(const std::array<std::uint8_t, 32>& digest)
    {
      std::ostringstream result;
      result << "sha256:" << std::hex << std::setfill('0');
      for (const std::uint8_t byte : digest) {
        result << std::setw(2) << static_cast<unsigned int>(byte);
      }
      return result.str();
    }

    [[nodiscard]]
    bool validId(std::string_view id) noexcept
    {
      if (id.size() != 14) {
        return false;
      }
      for (const unsigned char character : id) {
        if (std::isdigit(character) == 0) {
          return false;
        }
      }
      return true;
    }

    void validateStatements(const std::vector<MigrationStatement>& statements, std::string_view direction)
    {
      for (const MigrationStatement& statement : statements) {
        if (statement.description.empty()) {
          throw MigrationException("Migration {} statement requires a description.", direction);
        }
        if (statement.sql.empty()) {
          throw MigrationException("Migration {} statement requires SQL.", direction);
        }
        if (statement.sql.find('\0') != std::string::npos) {
          throw MigrationException("Migration {} statement SQL cannot contain a null byte.", direction);
        }
      }
    }

    void validateMigrationArtifactContents(const MigrationArtifact& artifact)
    {
      if (artifact.formatVersion() != MigrationArtifact::currentFormatVersion) {
        throw MigrationException(
          "Unsupported migration artifact version '{}'. Expected version '{}'.",
          artifact.formatVersion(),
          MigrationArtifact::currentFormatVersion);
      }
      if (!validId(artifact.id())) {
        throw MigrationException("Migration id '{}' must contain exactly 14 decimal digits.", artifact.id());
      }
      if (artifact.name().empty()) {
        throw MigrationException("Migration '{}' requires a non-empty name.", artifact.id());
      }
      if (artifact.database().empty()) {
        throw MigrationException("Migration '{}' requires a target database.", artifact.id());
      }
      if (artifact.forward().empty()) {
        throw MigrationException("Migration '{}' requires at least one forward statement.", artifact.id());
      }
      if (artifact.rollback().has_value() && artifact.rollback()->empty()) {
        throw MigrationException(
          "Migration '{}' must omit rollback or provide at least one rollback statement.",
          artifact.id());
      }

      validateStatements(artifact.forward(), "forward");
      if (artifact.rollback().has_value()) {
        validateStatements(*artifact.rollback(), "rollback");
      }
    }
  } // namespace

  MigrationArtifact::MigrationArtifact(
    std::uint32_t formatVersion,
    std::string id,
    std::string name,
    std::string database,
    std::string checksum,
    std::vector<MigrationStatement> forward,
    std::optional<std::vector<MigrationStatement>> rollback)
    : formatVersion_(formatVersion),
      id_(std::move(id)),
      name_(std::move(name)),
      database_(std::move(database)),
      checksum_(std::move(checksum)),
      forward_(std::move(forward)),
      rollback_(std::move(rollback))
  {}

  std::uint32_t MigrationArtifact::formatVersion() const noexcept
  {
    return formatVersion_;
  }

  const std::string& MigrationArtifact::id() const noexcept
  {
    return id_;
  }

  const std::string& MigrationArtifact::name() const noexcept
  {
    return name_;
  }

  const std::string& MigrationArtifact::database() const noexcept
  {
    return database_;
  }

  const std::string& MigrationArtifact::checksum() const noexcept
  {
    return checksum_;
  }

  const std::vector<MigrationStatement>& MigrationArtifact::forward() const noexcept
  {
    return forward_;
  }

  const std::optional<std::vector<MigrationStatement>>& MigrationArtifact::rollback() const noexcept
  {
    return rollback_;
  }

  MigrationArtifact makeMigrationArtifact(
    std::string id,
    std::string name,
    std::string database,
    std::vector<MigrationStatement> forward,
    std::optional<std::vector<MigrationStatement>> rollback)
  {
    MigrationArtifact draft{
      MigrationArtifact::currentFormatVersion,
      std::move(id),
      std::move(name),
      std::move(database),
      {},
      std::move(forward),
      std::move(rollback),
    };
    validateMigrationArtifactContents(draft);

    return {
      draft.formatVersion(),
      draft.id(),
      draft.name(),
      draft.database(),
      calculateMigrationArtifactChecksum(draft),
      draft.forward(),
      draft.rollback(),
    };
  }

  std::string calculateMigrationArtifactChecksum(const MigrationArtifact& artifact)
  {
    Sha256 hash;
    addChecksumValue(hash, "worm-migration-artifact");
    addChecksumValue(hash, std::to_string(artifact.formatVersion()));
    addChecksumValue(hash, artifact.id());
    addChecksumValue(hash, artifact.name());
    addChecksumValue(hash, artifact.database());
    addChecksumStatements(hash, artifact.forward());
    addChecksumValue(hash, artifact.rollback().has_value() ? "rollback-present" : "rollback-absent");
    if (artifact.rollback().has_value()) {
      addChecksumStatements(hash, *artifact.rollback());
    }
    return digestString(hash.finish());
  }

  bool isMigrationArtifactChecksum(std::string_view checksum) noexcept
  {
    constexpr std::string_view prefix = "sha256:";
    if (checksum.size() != prefix.size() + 64 || !checksum.starts_with(prefix)) {
      return false;
    }
    return std::ranges::all_of(checksum.substr(prefix.size()), [](unsigned char character) {
      return std::isdigit(character) != 0 || (character >= 'a' && character <= 'f');
    });
  }

  bool hasValidMigrationArtifactChecksum(const MigrationArtifact& artifact)
  {
    return artifact.checksum() == calculateMigrationArtifactChecksum(artifact);
  }

  void validateMigrationArtifact(const MigrationArtifact& artifact)
  {
    validateMigrationArtifactContents(artifact);
    if (!hasValidMigrationArtifactChecksum(artifact)) {
      throw MigrationException("Migration '{}' checksum does not match its contents.", artifact.id());
    }
  }
} // namespace worm::core
