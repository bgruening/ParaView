file(MAKE_DIRECTORY ${PARAVIEW_GENERATED_DOCUMENTATION_OUTPUT_DIRECTORY})

#---------------------------------------------------------------------------------
# add a txt file to indicate which source version the documentation was
# generated for.
file(GENERATE
  OUTPUT "${PARAVIEW_GENERATED_DOCUMENTATION_OUTPUT_DIRECTORY}/version.txt"
  CONTENT "${PARAVIEW_VERSION_FULL}\n")

#---------------------------------------------------------------------------------
# add a custom command to package the documentations into a tarball that's
# easy to move around.
add_custom_target(ParaViewDoc-TGZ
  COMMAND
    ${CMAKE_COMMAND} -E tar cfz
    ${CMAKE_CURRENT_BINARY_DIR}/ParaViewGeneratedDocs.tar.gz
    ${PARAVIEW_GENERATED_DOCUMENTATION_OUTPUT_DIRECTORY}
  WORKING_DIRECTORY ${PARAVIEW_GENERATED_DOCUMENTATION_OUTPUT_DIRECTORY}/..
  COMMENT "Generate ParaViewGeneratedDocs.tar.gz")

set(doc_targets
  ParaViewDoxygenDoc)
if (TARGET ParaViewPythonDoc)
  list(APPEND doc_targets
    ParaViewPythonDoc)
endif ()

add_dependencies(ParaViewDoc-TGZ ${doc_targets})
