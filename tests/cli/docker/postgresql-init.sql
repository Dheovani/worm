CREATE TABLE users (
  id BIGSERIAL PRIMARY KEY,
  email TEXT NOT NULL UNIQUE
);

CREATE TABLE schema_contract (
  id BIGSERIAL PRIMARY KEY,
  email TEXT UNIQUE,
  tenant TEXT,
  external_id TEXT,
  UNIQUE (tenant, external_id)
);
