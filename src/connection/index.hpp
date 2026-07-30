#pragma once

/**
 * Index file
 * Basically includes all other files
 * from the connection namespace/dir
 */

#include <connection/configuration.hpp>
#include <connection/client.hpp>
#include <connection/transaction.hpp>
#include <connection/drivers/mysql-client.hpp>
#include <connection/drivers/pg-client.hpp>
#include <connection/drivers/sqlite-client.hpp>
