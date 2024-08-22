function(create_po_target domain_name lang po_file)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${domain_name}_${lang}.gmo"
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/${domain_name}_${lang}.gmo" "${po_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDS "${po_file}"
     )
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${domain_name}_${lang}.gmo" PROPERTIES GENERATED true)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${domain_name}_${lang}.gmo"
    DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES"
    RENAME ${domain_name}.mo)
endfunction()
