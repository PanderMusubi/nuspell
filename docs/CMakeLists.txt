find_program(RONN_EXECUTABLE ronn)

if (NOT RONN_EXECUTABLE)
    message(WARNING "Ronn not found, generating man-pages will be skipped")
    return()
endif()

add_custom_command(OUTPUT nuspell.1
    COMMAND
        ${RONN_EXECUTABLE} -r --pipe ${CMAKE_CURRENT_SOURCE_DIR}/nuspell.1.md > nuspell.1
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/nuspell.1.md
    COMMENT "Building manpage nuspell.1")
add_custom_target(nuspell-man-pages ALL DEPENDS nuspell.1)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nuspell.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
