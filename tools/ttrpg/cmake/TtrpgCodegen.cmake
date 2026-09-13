include_guard(GLOBAL)

function(ttrpg_generate)
    cmake_parse_arguments(PARSE_ARGV 0 TTRPG "" "SCHEMA;HEADER;SOURCE;DEPFILE;TARGET" "")
    if(TTRPG_UNPARSED_ARGUMENTS OR TTRPG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid ttrpg_generate arguments")
    endif()
    foreach(REQUIRED_ARGUMENT IN ITEMS SCHEMA HEADER SOURCE)
        if(NOT TTRPG_${REQUIRED_ARGUMENT})
            message(FATAL_ERROR "ttrpg_generate requires ${REQUIRED_ARGUMENT}")
        endif()
    endforeach()

    get_filename_component(TTRPG_SCHEMA "${TTRPG_SCHEMA}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(OUTPUT_ARGUMENT IN ITEMS HEADER SOURCE)
        get_filename_component(TTRPG_${OUTPUT_ARGUMENT} "${TTRPG_${OUTPUT_ARGUMENT}}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endforeach()
    if(NOT TTRPG_DEPFILE)
        set(TTRPG_DEPFILE "${TTRPG_SOURCE}.d")
    endif()
    get_filename_component(TTRPG_DEPFILE "${TTRPG_DEPFILE}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    add_custom_command(
        OUTPUT "${TTRPG_HEADER}" "${TTRPG_SOURCE}"
        COMMAND $<TARGET_FILE:ttrpg_codegen>
            --schema "${TTRPG_SCHEMA}"
            --out-h "${TTRPG_HEADER}"
            --out-cpp "${TTRPG_SOURCE}"
            --depfile "${TTRPG_DEPFILE}"
        DEPENDS ttrpg_codegen "${TTRPG_SCHEMA}"
        DEPFILE "${TTRPG_DEPFILE}"
        VERBATIM
    )

    set(TTRPG_PROPERTY_SCOPE)
    if(TTRPG_TARGET)
        if(NOT TARGET "${TTRPG_TARGET}")
            message(FATAL_ERROR "ttrpg_generate TARGET must already exist: ${TTRPG_TARGET}")
        endif()
        set(TTRPG_PROPERTY_SCOPE TARGET_DIRECTORY "${TTRPG_TARGET}")
    endif()
    set_source_files_properties("${TTRPG_HEADER}" ${TTRPG_PROPERTY_SCOPE}
        PROPERTIES GENERATED ON)
    set_source_files_properties("${TTRPG_SOURCE}" ${TTRPG_PROPERTY_SCOPE}
        PROPERTIES GENERATED ON SKIP_LINTING ON)
endfunction()
