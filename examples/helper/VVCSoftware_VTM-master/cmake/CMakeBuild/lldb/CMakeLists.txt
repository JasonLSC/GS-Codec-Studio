# CMakeBuild/CMakeBuild/lldb/CMakeLists.txt

if( NOT DEFINED LLDB_INSTALL_ROOT )
  set( LLDB_INSTALL_ROOT "$ENV{HOME}/.lldb.d" ) 
  message( STATUS "${CMAKE_CURRENT_LIST_FILE}: LLDB_INSTALL_ROOT not set, using default ${LLDB_INSTALL_ROOT}." )
endif()

if( NOT DEFINED LLDBINIT_FILE )
  set( LLDBINIT_FILE "$ENV{HOME}/.lldbinit" ) 
  message( STATUS "${CMAKE_CURRENT_LIST_FILE}: LLDBINIT_FILE not set, using default ${LLDBINIT_FILE}." )
endif()

if( NOT DEFINED ENABLE_LLDBINIT_UPDATE )
  set( ENABLE_LLDBINIT_UPDATE TRUE ) 
  message( STATUS "${CMAKE_CURRENT_LIST_FILE}: ENABLE_LLDBINIT_UPDATE not set, using default ${ENABLE_LLDBINIT_UPDATE}." )
endif()


set( LLDB_INSTALL_DIR ${LLDB_INSTALL_ROOT}/${PROJECT_NAME} )

# Get all LLDB python modules
file( GLOB LLDB_MODULES "*.py" )

configure_file( lldbinit-${PROJECT_NAME}.in lldbinit-${PROJECT_NAME} @ONLY )

if( ENABLE_LLDBINIT_UPDATE )
  # Install LLDB files and update  ~/.lldbinit

  # ---
  # Create a cmake script to update ~/.ldbinit 
  set( cmake_lldbinit_module ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/BBuildEnvLldb.cmake )
  set( input_file ${LLDBINIT_FILE} )
  set( section_marker ${PROJECT_NAME} )
  set( lldbinit_content "command source -s 0 ${LLDB_INSTALL_ROOT}/lldbinit-${PROJECT_NAME}" )
  configure_file( lldbinit_update-in.cmake lldbinit_update.cmake )
  # ---

  add_custom_target( Install${PROJECT_NAME}LldbFiles ${CMAKE_COMMAND} -E make_directory ${LLDB_INSTALL_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/lldbinit-${PROJECT_NAME} ${LLDB_INSTALL_ROOT}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLDB_MODULES} ${LLDB_INSTALL_DIR} 
                     COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/lldbinit_update.cmake
                     VERBATIM
                     SOURCES lldbinit-${PROJECT_NAME}.in ${LLDB_MODULES} )
else()
  # Install LLDB files without touching ~/.lldbinit
  add_custom_target( Install${PROJECT_NAME}LldbFiles ${CMAKE_COMMAND} -E make_directory ${LLDB_INSTALL_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/lldbinit-${PROJECT_NAME} ${LLDB_INSTALL_ROOT}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LLDB_MODULES} ${LLDB_INSTALL_DIR} 
                     VERBATIM
                     SOURCES lldbinit-${PROJECT_NAME}.in ${LLDB_MODULES} )
endif()
