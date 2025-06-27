# library
set( LIB_NAME CommonAnalyserLib )

# NATVIS files for Visual Studio
if( MSVC )
  file( GLOB NATVIS_FILES "../../VisualStudio/*.natvis" )
endif()

if (NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64") )
  # get source files
  file( GLOB BASE_SRC_FILES "../CommonLib/*.cpp" )

  # get include files
  file( GLOB BASE_INC_FILES "../CommonLib/*.h" )

  # get x86 source files
  file( GLOB X86_SRC_FILES "../CommonLib/x86/*.cpp" )

  # get x86 include files
  file( GLOB X86_INC_FILES "../CommonLib/x86/*.h" )

  # get avx source files
  file( GLOB AVX_SRC_FILES "../CommonLib/x86/avx/*.cpp" )

  # get avx2 source files
  file( GLOB AVX2_SRC_FILES "../CommonLib/x86/avx2/*.cpp" )

  # get sse4.1 source files
  file( GLOB SSE41_SRC_FILES "../CommonLib/x86/sse41/*.cpp" )

  # get sse4.2 source files
  file( GLOB SSE42_SRC_FILES "../CommonLib/x86/sse42/*.cpp" )
endif()

# get libmd5 source files
file( GLOB MD5_SRC_FILES "../libmd5/*.cpp" )

# get libmd5 include files
file( GLOB MD5_INC_FILES "../libmd5/*.h" )


# get all source files
set( SRC_FILES ${BASE_SRC_FILES} ${X86_SRC_FILES} ${SSE41_SRC_FILES} ${SSE42_SRC_FILES} ${AVX_SRC_FILES} ${AVX2_SRC_FILES} ${MD5_SRC_FILES} )

# get all include files
set( INC_FILES ${BASE_INC_FILES} ${X86_INC_FILES} ${MD5_INC_FILES} )


# library
add_library( ${LIB_NAME} STATIC ${SRC_FILES} ${INC_FILES} ${NATVIS_FILES} )
target_compile_definitions( ${LIB_NAME} PUBLIC RExt__DECODER_DEBUG_BIT_STATISTICS=1 )
target_compile_definitions( ${LIB_NAME} PUBLIC RExt__DECODER_DEBUG_TOOL_STATISTICS=1 )

if( EXTENSION_360_VIDEO )
  target_compile_definitions( ${LIB_NAME} PUBLIC EXTENSION_360_VIDEO=1 )
endif()

if( EXTENSION_HDRTOOLS )
  target_compile_definitions( ${LIB_NAME} PUBLIC EXTENSION_HDRTOOLS=1 )
endif()

if( DEFINED ENABLE_TRACING )
  if( ENABLE_TRACING )
    target_compile_definitions( ${LIB_NAME} PUBLIC ENABLE_TRACING=1 )
  else()
    target_compile_definitions( ${LIB_NAME} PUBLIC ENABLE_TRACING=0 )
  endif()
endif()

if( DEFINED ENABLE_HIGH_BITDEPTH )
  if( ENABLE_HIGH_BITDEPTH )
    target_compile_definitions( ${LIB_NAME} PUBLIC RExt__HIGH_BIT_DEPTH_SUPPORT=1 )
  else()
    target_compile_definitions( ${LIB_NAME} PUBLIC RExt__HIGH_BIT_DEPTH_SUPPORT=0 )
  endif()
endif()

if ( ENABLE_SEARCH_OPENSSL )
  find_package(OpenSSL)
endif()

if ( (NOT OPENSSL_FOUND) OR (OPENSSL_VERSION VERSION_LESS "1.1.1") )
  message ("OpenSSL not available or version less than 1.1.1. Compiling with parsing only support for Digitally Signed Content SEIs")
  target_compile_definitions( ${LIB_NAME} PUBLIC JVET_AJ0151_DSC_SEI=0 )
  target_include_directories( ${LIB_NAME} PUBLIC ../CommonLib/. ../CommonLib/.. ../CommonLib/x86 ../libmd5 )
  target_link_libraries( ${LIB_NAME} )
else()
  target_include_directories( ${LIB_NAME} PUBLIC ../CommonLib/. ../CommonLib/.. ../CommonLib/x86 ../libmd5 ${OPENSSL_INCLUDE_DIR} )
  target_link_libraries( ${LIB_NAME} OpenSSL::SSL OpenSSL::Crypto )
endif()

if (NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64") )
  # set needed compile definitions
  set_property( SOURCE ${SSE41_SRC_FILES} APPEND PROPERTY COMPILE_DEFINITIONS USE_SSE41 )
  set_property( SOURCE ${SSE42_SRC_FILES} APPEND PROPERTY COMPILE_DEFINITIONS USE_SSE42 )
  set_property( SOURCE ${AVX_SRC_FILES}   APPEND PROPERTY COMPILE_DEFINITIONS USE_AVX )
  set_property( SOURCE ${AVX2_SRC_FILES}  APPEND PROPERTY COMPILE_DEFINITIONS USE_AVX2 )
  # set needed compile flags
  if( MSVC )
    set_property( SOURCE ${AVX_SRC_FILES}   APPEND PROPERTY COMPILE_FLAGS "/arch:AVX" )
    set_property( SOURCE ${AVX2_SRC_FILES}  APPEND PROPERTY COMPILE_FLAGS "/arch:AVX2" )
  elseif( UNIX OR MINGW )
    set_property( SOURCE ${SSE41_SRC_FILES} APPEND PROPERTY COMPILE_FLAGS "-msse4.1" )
    set_property( SOURCE ${SSE42_SRC_FILES} APPEND PROPERTY COMPILE_FLAGS "-msse4.2" )
    set_property( SOURCE ${AVX_SRC_FILES}   APPEND PROPERTY COMPILE_FLAGS "-mavx" )
    set_property( SOURCE ${AVX2_SRC_FILES}  APPEND PROPERTY COMPILE_FLAGS "-mavx2" )
  endif()
endif()

# example: place header files in different folders
source_group( "Natvis Files" FILES ${NATVIS_FILES} )

# set the folder where to place the projects
set_target_properties( ${LIB_NAME} PROPERTIES FOLDER lib )
