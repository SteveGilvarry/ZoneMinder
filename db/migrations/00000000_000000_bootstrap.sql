-- Migration: 00000000_000000_bootstrap
-- Description: Bootstrap the new migration system from legacy version tracking
-- This is a special migration handled by zmupdate.pl
--
-- When applied to an existing system:
-- 1. Creates the Schema_Migrations table if it doesn't exist
-- 2. zmupdate.pl reads the current ZM_DYN_DB_VERSION from Config
-- 3. zmupdate.pl inserts records for all legacy migrations <= that version
-- 4. This bootstrap migration is recorded as applied
--
-- For fresh installs:
-- The Schema_Migrations table is created by zm_create.sql.in with all
-- legacy migrations pre-populated as applied.

-- Create Schema_Migrations table if it doesn't exist
-- (For upgrades from legacy systems that don't have it yet)
CREATE TABLE IF NOT EXISTS `Schema_Migrations` (
  `Id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `Migration` varchar(128) NOT NULL,
  `AppliedAt` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `Batch` int(10) unsigned NOT NULL DEFAULT 1,
  `ExecutionTimeMs` int(10) unsigned DEFAULT NULL,
  `Checksum` char(32) DEFAULT NULL,
  PRIMARY KEY (`Id`),
  UNIQUE KEY `Migration_UNIQUE` (`Migration`)
) ENGINE=InnoDB;

-- The rest of the bootstrap process (populating legacy migrations)
-- is handled by zmupdate.pl Perl logic, not SQL.
-- This ensures the correct legacy version is read from ZM_DYN_DB_VERSION
-- and appropriate legacy migration records are inserted.
