# CMakeLists.txt for the ZoneMinder database scripts

# Create files from the .in files
configure_file(zm_create.sql.in "${CMAKE_CURRENT_BINARY_DIR}/zm_create.sql" @ONLY)

# Legacy migration templates - processed and installed to legacy/ subdirectory
configure_file(legacy/zm_update-1.31.30.sql.in "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.31.30.sql" @ONLY)
configure_file(legacy/zm_update-1.35.24.sql.in "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.35.24.sql" @ONLY)
configure_file(legacy/zm_update-1.37.4.sql.in "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.37.4.sql" @ONLY)
configure_file(legacy/zm_update-1.37.69.sql.in "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.37.69.sql" @ONLY)
configure_file(legacy/zm_update-1.37.74.sql.in "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.37.74.sql" @ONLY)

# install zm_create.sql
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/zm_create.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db")

# Glob all legacy database upgrade scripts
file(GLOB legacyDbFilesList RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/legacy" "legacy/*.sql")

# Install processed legacy template files
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.31.30.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/legacy")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.35.24.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/legacy")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.37.4.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/legacy")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.37.69.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/legacy")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/legacy/zm_update-1.37.74.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/legacy")

# Install the legacy database upgrade scripts
install(DIRECTORY legacy/ DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/legacy"
        FILES_MATCHING PATTERN "*.sql"
        PATTERN "*.sql.in" EXCLUDE)

# Glob all new migration scripts
file(GLOB migrationFilesList RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/migrations" "migrations/*.sql")

# Install the new migration scripts
install(DIRECTORY migrations/ DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db/migrations"
        FILES_MATCHING PATTERN "*.sql")

# Install other database files (manufacturers, models, etc.)
file(GLOB otherDbFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.sql")
install(FILES ${otherDbFiles} DESTINATION "${CMAKE_INSTALL_DATADIR}/zoneminder/db")
