# executable
set( EXE_NAME parcat )

# get source files
file( GLOB SRC_FILES "*.cpp" )

# get include files
file( GLOB INC_FILES "*.h" )

# add executable
add_executable( ${EXE_NAME} ${SRC_FILES} ${INC_FILES} )

target_link_libraries( ${EXE_NAME} Threads::Threads ${ADDITIONAL_LIBS} )

# include the output directory, where the svnrevision.h file is generated
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  add_custom_command( TARGET ${EXE_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
                                                          $<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/parcat>
                                                          $<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/parcat>
                                                          $<$<CONFIG:RelWithDebInfo>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO}/parcat>
                                                          $<$<CONFIG:MinSizeRel>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL}/parcat>
                                                          $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/bin/parcatStaticd>
                                                          $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/bin/parcatStatic>
                                                          $<$<CONFIG:RelWithDebInfo>:${CMAKE_SOURCE_DIR}/bin/parcatStaticp>
                                                          $<$<CONFIG:MinSizeRel>:${CMAKE_SOURCE_DIR}/bin/parcatStaticm> )
endif()

# set the folder where to place the projects
set_target_properties( ${EXE_NAME}  PROPERTIES FOLDER app LINKER_LANGUAGE CXX )

