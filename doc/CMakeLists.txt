find_package(Doxygen)
option(BUILD_DOC "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})
if(BUILD_DOC)
    if(NOT DOXYGEN_FOUND)
         message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()
    configure_file(doxyfile.in doxyfile @ONLY)
    add_custom_target(doc ALL
                      COMMAND ${DOXYGEN_EXECUTABLE} doxyfile
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
                      COMMENT "Generating API documentation with Doxygen ${DOXYGEN_VERSION}"
                      VERBATIM
    )
endif(BUILD_DOC)