# library
set( LIB_NAME Utilities )

# get source files
file( GLOB SRC_FILES "*.cpp" )

# get include files
file( GLOB INC_FILES "*.h" )

# NATVIS files for Visual Studio
if( MSVC )
  file( GLOB NATVIS_FILES "../../VisualStudio/*.natvis" )
endif()

# library
add_library( ${LIB_NAME} STATIC ${SRC_FILES} ${INC_FILES} ${NATVIS_FILES} )
target_compile_definitions( ${LIB_NAME} PUBLIC )

if( HIGH_BITDEPTH )
  target_compile_definitions( ${LIB_NAME} PUBLIC RExt__HIGH_BIT_DEPTH_SUPPORT=1 )
endif()

if( EXTENSION_360_VIDEO )
  target_compile_definitions( ${LIB_NAME} PUBLIC EXTENSION_360_VIDEO=1 )
endif()

if( SET_ENABLE_TRACING )
  if( ENABLE_TRACING )
    target_compile_definitions( ${LIB_NAME} PUBLIC ENABLE_TRACING=1 )
  else()
    target_compile_definitions( ${LIB_NAME} PUBLIC ENABLE_TRACING=0 )
  endif()
endif()

target_include_directories( ${LIB_NAME} PUBLIC . .. )
target_link_libraries( ${LIB_NAME} Threads::Threads )

# example: place header files in different folders
source_group( "Natvis Files" FILES ${NATVIS_FILES} )

# set the folder where to place the projects
set_target_properties( ${LIB_NAME} PROPERTIES FOLDER lib )
