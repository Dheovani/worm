#pragma once

/**
 * Index file
 * Basically includes all other files
 * from the connection namespace/dir
 */

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <connection/transaction.hpp>

#if defined(WORM_HAS_MYSQL_DRIVER)
#include <connection/drivers/mysql-client.hpp>
#endif

#if defined(WORM_HAS_POSTGRESQL_DRIVER)
#include <connection/drivers/pg-client.hpp>
#endif

#if defined(WORM_HAS_SQLITE_DRIVER)
#include <connection/drivers/sqlite-client.hpp>
#endif
