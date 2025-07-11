#[[.rst:
BBuildEnvMingw
--------------

Functions and macros supporting MinGW development.

.. command:: bb_add_target_CopyMingwRuntimeFiles

  The ``bb_add_target_CopyMingwRuntimeFiles()`` macro adds a custom target ``CopyMingwRuntimeFiles`` 
  to copy MinGW dlls to :variable:`CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CONFIG>`. 

#]]

if( NOT CMAKE_VERSION VERSION_LESS 3.10 )
  include_guard( GLOBAL )
endif()

macro( bb_add_target_CopyMingwRuntimeFiles )
  if( MINGW AND CMAKE_CROSSCOMPILING )
    if( NOT TARGET CopyMingwRuntimeFiles )
      if( NOT DEFINED bb_MINGW_RUNTIME_FILES )
        message( FATAL_ERROR "\
  No MinGW runtime files defined, check your toolchain file and 
  define bb_MINGW_RUNTIME_FILES or don't call bb_add_target_CopyMingwRuntimeFiles()." )
      endif()
      if( NOT CMAKE_VERSION VERSION_LESS 3.8 )
        add_custom_target( CopyMingwRuntimeFiles ALL ${CMAKE_COMMAND} -E make_directory
                             $<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>
                             $<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>
                             $<$<CONFIG:RelWithDebInfo>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO}>
                             $<$<CONFIG:MinSizeRel>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL}>
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${bb_MINGW_RUNTIME_FILES} 
                             $<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>
                             $<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>
                             $<$<CONFIG:RelWithDebInfo>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO}>
                             $<$<CONFIG:MinSizeRel>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL}> 
                           VERBATIM COMMAND_EXPAND_LISTS )
      else()
        add_custom_target( CopyMingwRuntimeFiles ALL ${CMAKE_COMMAND} -E make_directory
                             $<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>
                             $<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>
                             $<$<CONFIG:RelWithDebInfo>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO}>
                             $<$<CONFIG:MinSizeRel>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL}>
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different ${bb_MINGW_RUNTIME_FILES} 
                             $<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>
                             $<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>
                             $<$<CONFIG:RelWithDebInfo>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO}>
                             $<$<CONFIG:MinSizeRel>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL}> )
      endif()
    endif()
  endif()
endmacro()

