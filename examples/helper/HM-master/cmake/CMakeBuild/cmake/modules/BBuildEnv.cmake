#[===[.rst:
BBuildEnv
---------

The module BBuildEnv provides a CMake build environment with functions and macros 
to access other external SDKs including CodeMeter, CUDASamples, Intel Performance Libraries
not yet supported by CMake natively. 

Other macros ensure consistent build settings for multithreading and provide 
an abstract CMake generator independent way to configure compiler warnings 
depending on compiler family and version. 

This module also provides some system information not yet available by CMake.  

Unless explicitly disabled by configuration option ``BBuildEnv_EXCLUDE_MODULES`` 
module ``BBuildEnv`` loads the following submodules to provide additional support 
for Boost, Qt5, OpenCV, file downloads, MinGW and CPack:

==============================   ===========================================================
Module                           Description
==============================   ===========================================================
:module:`BBuildEnvAddProject`    Macros and functions to add standard
                                 subproject like console applications, libraries, samples, 
                                 UTF tests and Qt applications to a standard workspace
:module:`BBuildEnvGitSvn`        Utility functions to support Git to SVN interoperability.                                   
:module:`BBuildEnvDownload`      Supports HTTPS downloads of single files at build time
:module:`BBuildEnvVersionUtil`   Functions to parse version header files
:module:`BBuildEnvCPack`         Functions helping to create binary distribution packages
:module:`BBuildEnvBoost`         Macros and functions helping to use locally built Boost 
                                 libraries. 
:module:`BBuildEnvOpenCV`        Helper functions to copy OpenCV runtime DLLs
:module:`BBuildEnvQt5`           Helper functions to copy Qt5 runtime DLLs
:module:`BBuildEnvMingw`         Helper functions to copy MinGW runtime DLLs on Ubuntu
==============================   ===========================================================

The following modules are not loaded by default as they provide functionality not needed
by all main projects.

- :module:`BBuildEnvGit` provides macros and functions to checkout Git repositories at 
  configuration time to aggregate them into a single build tree.  Similar functionality 
  is provided by module :module:`FetchContent` with slightly different Git clone and
  update behavior.

Configuration Options
^^^^^^^^^^^^^^^^^^^^^

This module evaluates the following variables at load time allowing users to 
customize its behavior:

``BBuildEnv_EXCLUDE_MODULES``
  List of submodules to be excluded from loading.  Use ``ALL`` to disable
  loading any submodule.

``BBuildEnv_DEBUG``
  Enable debugging messages.

``BBuildEnv_USE_LIBRARY_NAME_POSTFIX``
  A boolean variable to enable a configuration specific library name postfix which
  allows to install all library or executable variants in the same directory. It's unset/off by
  default to provide backward compatibility with earlier releases. If enabled executable 
  targets will need a configuration postfix as well. 


How to Use
^^^^^^^^^^

Include the following statement(s) in your top-level CMakeLists.txt to load this module 
and its submodules.  The modification of :variable:`CMAKE_MODULE_PATH` makes a set of new 
or patched find modules available as well and is therefore the recommended way 
to include ``BBuildEnv``.  The logic below assumes assumes CMakeBuild is included
as an svn:external or as a versioned Git subtree:

.. code-block:: cmake

  # Set module path to subversion or git layout based on the existence of 
  # ${CMAKE_SOURCE_DIR}/CMakeBuild/CMakeBuild/cmake/modules.
  if( EXISTS "${CMAKE_SOURCE_DIR}/CMakeBuild/CMakeBuild/cmake/modules" )
    set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeBuild/CMakeBuild/cmake/modules" )
    set( USE_GIT_SUBPROJECTS OFF )
  else()
    file( GLOB CMAKEBUILD_TAGS RELATIVE ${CMAKE_SOURCE_DIR}/CMakeBuild ${CMAKE_SOURCE_DIR}/CMakeBuild/*/CMakeBuild/cmake/modules/BBuildEnv.cmake )
    list( LENGTH CMAKEBUILD_TAGS NUM_OF_TAGS )
    if( ${NUM_OF_TAGS} EQUAL 1 )
      if( CMAKEBUILD_TAGS MATCHES "^([^/]+)/" )
        set( CMAKEBUILD_TAG ${CMAKE_MATCH_1} )
      endif()
      if( EXISTS "${CMAKE_SOURCE_DIR}/CMakeBuild/${CMAKEBUILD_TAG}/CMakeBuild/cmake/modules" )
        set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeBuild/${CMAKEBUILD_TAG}/CMakeBuild/cmake/modules" )
        set( USE_GIT_SUBPROJECTS ON )
      else()
        message( FATAL_ERROR "ERROR: CMakeBuild directory not found: ${CMAKE_SOURCE_DIR}/CMakeBuild/${CMAKEBUILD_TAG}/CMakeBuild/cmake/modules" )
      endif()
    else()
      message( FATAL_ERROR "ERROR: ${NUM_OF_TAGS} CMakeBuild directories found, exactly one is expected. Directories found: ${CMAKEBUILD_TAGS}" )
    endif()
  endif()
  
  # Include a utility module providing functions, macros, and settings to customize the build environment.
  include( BBuildEnv )


Provided Variables
^^^^^^^^^^^^^^^^^^

Module ``BBuildEnv`` provides the following output variables 
which are supposed to be treated readonly:

``BBuildEnv_VERSION``
  Module's version in decimal dotted format with a maximum of four components.
  
``BBuildEnv_MSYS``
  Set to true when using MSYS.

``BBuildEnv_GENERATOR_ALIAS``
  CMake generator specific build directory. It's a plain name without any path separators.
  
  ``umake`` 
    Unix Makefiles
 
  ``vs16``
    Microsoft Visual Studio 2019
    
  ``vs15``
    Microsoft Visual Studio 2017
    
  ``vs14``
    Microsoft Visual Studio 2015
    
  ``xcode``
    Xcode generator, switching between different Xcode versions is currently not supported within a single build tree.
    
  ``ninja``
    Ninja generator

``BBuildEnv_<CONFIG>_POSTFIX``
  Configuration specific postfix strings to support side-by-side installation in the same 
  directory. 

``BBuildEnv_SHARED_DIR_POSTFIX``
  A string specific to the shared library configuration to allow for single 
  output directories or installation directories.  

``BBuildEnv_OUTPUT_DIR_SUFFIX``
  A generator specific relative path to be used in installation rules to support multiple 
  generators or compiler versions in combination with the same installation prefix. 

``BBuildEnv_ROOT_DIR``
  Optional root directory of CMakeBuild customization files. 

``BBuildEnv_SCRIPT_DIR``
  Optional path to non-cmake scripts. 


Provided Functions and Macros
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. command:: bb_multithreading

  The ``bb_multithreading()`` macro adds an imported target ``Threads::Threads`` 
  to enable multithreaded code generation.  All multithreaded targets 
  shall have an explicit or implicit dependency on ``Threads::Threads``::
  
    bb_multithreading()
  

.. command:: bb_enable_warnings

  The ``bb_enable_warnings()`` macro enables CXX compiler warnings in case the 
  selected CMake generator does not by default 
  and accepts parameters to configure warnings as errors and disable warnings given a 
  specific compiler version or compiler family::

    bb_enable_warnings([<compiler>[-<compiler_version>]] [warnings-as-errors] [<warning_flag>...])

  **Parameters:**

    ``compiler``
      The macro supports the following compiler families 
      ``gcc``, ``clang``, ``msvc`` and ``intel``.
  
    ``compiler_version``
      Compiler version specified as ``major_version`` or 
      ``major_version.minor_version`` to indicate which compiler version
      the macro should configure. 
      
    ``warnings-as-errors``
      Treat warnings as errors.
      
    ``warning_flag``
      Compiler specific flag to enable or disable a warning.


.. command:: bb_add_subdirectory

  The ``bb_add_subdirectory()`` macro adds an external in-tree Git subproject 
  provided variable ``USE_GIT_SUBPROJECTS`` is ON. 
  The macro silently assumes the subproject is checked out to
  ``${CMAKE_SOURCE_DIR}/ext/<subproject>``.  If variable ``USE_GIT_SUBPROJECTS`` 
  is OFF, the macro will invoke :command:`add_subdirectory` for backward compatibility 
  with SVN repositories and subproject aggregation via SVN externals::
  
    bb_add_subdirectory(<subproject>)
    
  **Parameters:**
  
    ``subproject``
      A relative path to an in-tree subproject; e.g. ``BoostAddon/src/lib/LoggerLib``


.. command:: bb_set_target_output_name

  The ``bb_set_target_output_name`` macro appends a configuration specific postfix to 
  the output name of executable targets if variable ``BBuildEnv_USE_LIBRARY_NAME_POSTFIX`` 
  is ON. If applied to library targets, it will change :prop_tgt:`COMPILE_PDB_NAME_<CONFIG>` 
  for static libraries to align the PDB filename with the library filename. 
  CMake's postfix machinery does it for linker generated PDB files but not for compiler
  generated PDB files::

    bb_set_target_output_name( <target> )

  **Parameters:**
  
    ``target``
      An existing target to be modified.


.. command:: bb_set_external_dir

  The ``bb_set_external_dir()`` function searches for a directory given a 
  relative path using a fixed set of root paths.  It's intended use is to find 
  a shared folder holding an external project
  without searching any system paths or cross compiler specific paths::
  
    bb_set_external_dir(<abs_path> <dir> [<OPTIONAL>])

  **Parameters:**

    ``abs_path``
      Absolute path to ``relative_path`` found in one of the default locations.

    ``dir``
      Directory to search for in one of the default locations.  An absolute path 
      will be returned as-is.
          
    ``OPTIONAL``
      Search failure is not treated as an fatal error. 

  **Search Path:**

  Search path in decreasing order of preference.  All paths consisting of 
  undefined environment variables are silently ignored.

    ``$ENV{PROJ_HOME}``

    ``${CMAKE_SOURCE_DIR}/..``
     
    ``${CMAKE_SOURCE_DIR}/../..``
     
    ``$ENV{HOME}/projects``
      Ignored on native windows host systems. It is searched when MSYS has been detected 
      or any other non-windows platform.
     
    ``$ENV{USERPROFILE}/projects``
      Ignored on non-windows host systems.


Reserved Identifiers
^^^^^^^^^^^^^^^^^^^^

Avoiding name clashes in CMakeLists.txt or project specific CMake files all 
projects including module ``BBuildEnv``, or any of its submodules, are advised 
not to use CMake variables, functions or macros starting with::

  BBuildEnv, _BBuildEnv, _bb_, bb_, BB_, _BB_

Users may use variables starting with ``BBuildEnv_<var>`` only to configure the 
behavior of ``BuildEnv`` modules or submodules or evaluate properties of loaded 
``BuildEnv`` modules or submodules exposed through documented variables 
``BBuildEnv_<var>``. 

#]===]


if( NOT CMAKE_VERSION VERSION_LESS 3.10 )
  include_guard( GLOBAL )
endif()

include( "${CMAKE_CURRENT_LIST_DIR}/BBuildEnvVersion.cmake" )

# List of submodules to load by default.
set( _BBuildEnvSubmoduleList
     BBuildEnvAddProject
     # BBuildEnvDebug
     BBuildEnvDownload
     BBuildEnvMingw
     BBuildEnvVersionUtil
     BBuildEnvCPack
     BBuildEnvBoost 
     BBuildEnvQt5
     BBuildEnvOpenCV 
     BBuildEnvGitSvn
   )

foreach( _cmod IN LISTS _BBuildEnvSubmoduleList )
  if( DEFINED BBuildEnv_EXCLUDE_MODULES )
    if( "${BBuildEnv_EXCLUDE_MODULES}" STREQUAL "ALL" )
      break()
    endif()
    if( NOT _cmod IN_LIST BBuildEnv_EXCLUDE_MODULES )
      include( "${CMAKE_CURRENT_LIST_DIR}/${_cmod}.cmake" OPTIONAL )
    endif()
  else()
    include( "${CMAKE_CURRENT_LIST_DIR}/${_cmod}.cmake" OPTIONAL )
  endif()
endforeach()     


# this macro switches between subversion externals and git dependencies
macro( bb_add_subdirectory subdirectory_ )
  if( USE_GIT_SUBPROJECTS )
    string( REGEX REPLACE "([^/]+).*" "\\1" BASE_DIRECTORY ${subdirectory_} )
    add_subdirectory( ${CMAKE_SOURCE_DIR}/ext/${BASE_DIRECTORY}/${subdirectory_} ${CMAKE_BINARY_DIR}/${subdirectory_} )
  else()
    add_subdirectory( ${subdirectory_} )  
  endif()
endmacro()


macro( bb_set_target_output_name target_ )
  if( BBuildEnv_USE_LIBRARY_NAME_POSTFIX ) 
    get_target_property( _bb_tmp_target_type ${target_} TYPE )
    if( _bb_tmp_target_type STREQUAL "EXECUTABLE" )
      set_target_properties( ${target_} PROPERTIES OUTPUT_NAME_DEBUG ${target_}${CMAKE_DEBUG_POSTFIX} 
                                        OUTPUT_NAME_RELWITHDEBINFO   ${target_}${CMAKE_RELWITHDEBINFO_POSTFIX} 
                                        OUTPUT_NAME_MINSIZEREL       ${target_}${CMAKE_MINSIZEREL_POSTFIX} )
    elseif( MSVC AND (_bb_tmp_target_type STREQUAL "STATIC_LIBRARY" ) )
      # message( STATUS "${target_} is static, setting COMPILE_PDB_NAME_DEBUG ..." )
      set_target_properties( ${target_} PROPERTIES COMPILE_PDB_NAME_DEBUG ${target_}${CMAKE_DEBUG_POSTFIX} COMPILE_PDB_NAME_RELWITHDEBINFO ${target_}${CMAKE_RELWITHDEBINFO_POSTFIX} )
    endif()  
  endif()
endmacro()


macro( bb_save_find_context fnd_ctx )
  if( CMAKE_CROSSCOMPILING )
    # find_package must be told not to expect the BOOST libraries inside "CMAKE_FIND_ROOT_PATH".
    foreach( _v  INCLUDE LIBRARY PACKAGE )
      if( DEFINED CMAKE_FIND_ROOT_PATH_MODE_${_v} )
        # message( STATUS "bb_save_find_context(): CMAKE_FIND_ROOT_PATH_MODE_${_v}=${CMAKE_FIND_ROOT_PATH_MODE_${_v}}" )
        
        set( ${fnd_ctx}_CMAKE_FIND_ROOT_PATH_MODE_${_v} ${CMAKE_FIND_ROOT_PATH_MODE_${_v}} )
        set( CMAKE_FIND_ROOT_PATH_MODE_${_v} NEVER )
      else()
        unset( ${fnd_ctx}_CMAKE_FIND_ROOT_PATH_MODE_${_v} )
      endif()
    endforeach()
  endif()
endmacro()


macro( bb_restore_find_context fnd_ctx )
  if( CMAKE_CROSSCOMPILING )
    # Restore CMAKE_FIND_ROOT_PATH settings    
    foreach( _v  INCLUDE LIBRARY PACKAGE )
      if( DEFINED ${fnd_ctx}_CMAKE_FIND_ROOT_PATH_MODE_${_v} )
        set( CMAKE_FIND_ROOT_PATH_MODE_${_v} ${${fnd_ctx}_CMAKE_FIND_ROOT_PATH_MODE_${_v}} )
        unset( ${fnd_ctx}_CMAKE_FIND_ROOT_PATH_MODE_${_v} )
        
        # message( STATUS "bb_restore_find_context(): CMAKE_FIND_ROOT_PATH_MODE_${_v}=${CMAKE_FIND_ROOT_PATH_MODE_${_v}}" )
      endif()
    endforeach()
  endif()
endmacro()



#
# Internal macro to extract the CXX compiler's <major>.<minor> version.
#
macro( _bb_get_cxx_compiler_version_major_minor version_major_minor_ )
  string( REGEX REPLACE "([0-9]+)\\.([0-9]+)([0-9.]+)?" "\\1.\\2" ${version_major_minor_} ${CMAKE_CXX_COMPILER_VERSION} )
endmacro()


function( bb_get_home_dir home_dir_ )
  set( _native FALSE )
  
  if( ARGC EQUAL 2 )
    if( ${ARGV1} STREQUAL "NATIVE" )
      set( _native TRUE )
    else()
      message( FATAL_ERROR "bb_get_home_dir: argument ${ARGV1} not understood." )
    endif()
  elseif( ARGC GREATER 2 )
    message( FATAL_ERROR "bb_get_home_dir: too many arguments specified, expected <home_dir> [NATIVE]." )
  endif()
    
  if( CMAKE_HOST_WIN32 )
    # Force forward slashes on Windows
    if( BBuildEnv_MSYS )
      if( _native )
        file( TO_CMAKE_PATH "$ENV{USERPROFILE}" _home_dir )
      else()
        file( TO_CMAKE_PATH "$ENV{HOME}" _home_dir )
      endif()
    else()
      file( TO_CMAKE_PATH "$ENV{USERPROFILE}" _home_dir )
    endif()
  else()
    set( _home_dir "$ENV{HOME}" )
  endif()
  
  set( ${home_dir_} "${_home_dir}" PARENT_SCOPE )
  
endfunction()


macro( bb_set_home_dir home_dir_ )
  # backward compatibility
  bb_get_home_dir( ${home_dir_} )
endmacro()



function( bb_get_binary_tool_dir tool_dir_ )
  # Location to store scripts and programs generated by CMake to support the build.
  set( _tool_dir "${CMAKE_BINARY_DIR}/tools" )
  if( NOT EXISTS "${_tool_dir}" )
    file( MAKE_DIRECTORY "${_tool_dir}" )
  endif()
  set( ${tool_dir_} "${_tool_dir}" PARENT_SCOPE )
endfunction()


macro( bb_check_build_type )
  # message( STATUS "bb_check_build_type(): entering" )
  if( CMAKE_VERSION VERSION_LESS 3.9 )
    # Define property GENERATOR_IS_MULTI_CONFIG if it does not exist.
    get_property( _bb_have_generator_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG DEFINED )
    if( NOT _bb_have_generator_is_multi_config )
      # message( STATUS "bb_check_build_type(): global property GENERATOR_IS_MULTI_CONFIG is undefined." )
      # Property GENERATOR_IS_MULTI_CONFIG is not available
      define_property( GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG
                       BRIEF_DOCS "True when using a multi-configuration generator."
                       FULL_DOCS "True when using a multi-configuration generator. Builtin property of CMake 3.9.0 or higher." )
      if( MSVC OR XCODE )
        set_property( GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG 1 )
      else()
        set_property( GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG 0 )
      endif()
    endif()
    unset( _bb_have_generator_is_multi_config )
  endif()
  get_property( _bb_generator_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG )
  # message( STATUS "bb_check_build_type(): GENERATOR_IS_MULTI_CONFIG=${_bb_generator_is_multi_config}" )
  if( NOT _bb_generator_is_multi_config )
    # set default CMAKE_BUILD_TYPE to Release if not set
    if( NOT CMAKE_BUILD_TYPE )
      set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE )
    endif()
  endif()
  unset( _bb_generator_is_multi_config )
  # message( STATUS "bb_check_build_type(): leaving" )
endmacro( bb_check_build_type )


macro( bb_multithreading )
  if( TARGET Threads::Threads )
    message( FATAL_ERROR "bb_multithreading(): target Threads::Threads already exists. Please contact technical support." )
  endif()
  unset( _bb_mt_prop_value )
  if( MINGW )
    # MinGW may be a native compiler or a cross compiler.
    find_package( Threads REQUIRED )
    #add_compile_options( -mthreads )
    #message( STATUS "bb_multithreading(): mingw detected, add_compile_options: -mthreads" )
    get_target_property( _bb_mt_prop_value Threads::Threads INTERFACE_COMPILE_OPTIONS )
    if( _bb_mt_prop_value )
      if( NOT _bb_mt_prop_value STREQUAL "-mthreads" ) 
        message( FATAL_ERROR "bb_multithreading(): target Threads::Threads has unexpected INTERFACE_COMPILE_OPTIONS: ${_bb_mt_prop_value}" )
      endif()
    else()
      set_target_properties( Threads::Threads PROPERTIES INTERFACE_COMPILE_OPTIONS "-mthreads" )
    endif()
  elseif( CMAKE_CROSSCOMPILING )
    find_package( Threads REQUIRED )
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Cray" )
    find_package( Threads REQUIRED ) 
  else()
    if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
      # Add interface compile/link option -pthread
      set( THREADS_PREFER_PTHREAD_FLAG ON )
    endif()
    find_package( Threads REQUIRED )
    if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
      # A compile option -pthread bound to all enabled languages won't work if CUDA is enabled as a programming language (tested with cmake 3.9.1).
      # Since NVCC does not understand -pthread, the compile option must be bound to C and CXX.
      get_target_property( _bb_mt_prop_value Threads::Threads INTERFACE_COMPILE_OPTIONS )
      if( _bb_mt_prop_value )
        if( _bb_mt_prop_value STREQUAL "-pthread" ) 
          set_target_properties( Threads::Threads PROPERTIES INTERFACE_COMPILE_OPTIONS $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:-pthread> )
        endif()
      endif()
    endif()
  endif()
endmacro( bb_multithreading )


macro( bb_enable_warnings )
  unset( _bb_warning_options )        # in case the calling context has defined it.

  _bb_enable_warnings_helper( _bb_warning_options ${ARGN} )

  if( DEFINED _bb_warning_options )
    message( STATUS "bb_enable_warnings: ${toolset} -> updating warnings flags: ${_bb_warning_options}" )
    foreach( _bb_warning_opt IN LISTS _bb_warning_options )
      if( MSVC )
        # CMake 3.11.0 introduces support for generator expression COMPILE_LANGUAGE.
        # MSVC generator does not support the generator expression COMPILE_LANGUAGE yet.
        string( APPEND CMAKE_CXX_FLAGS " ${_bb_warning_opt}" )
        string( APPEND CMAKE_C_FLAGS " ${_bb_warning_opt}" )
      else()
        add_compile_options( $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:${_bb_warning_opt}> )
      endif()
    endforeach()      
    unset( _bb_warning_options )
  endif()    
endmacro()

function( _bb_enable_warnings_helper warning_options_ )
  #message( STATUS "_bb_enable_warnings_helper(): ${warning_options_} ARGC=${ARGC} ${ARGN}" )

  # Translate CMAKE_CXX_COMPILER_ID to a string which compares easily with toolset -> gcc, msvc, clang.
  if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    set( _bb_warning_compiler_id "gcc" )
  elseif( CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang)$" )
    if( MSVC )
      # No support for clang as MSVC toolset.
      # message( STATUS "_bb_enable_warnings_helper(): no support for MSVC/clang toolsets yet." )
      return()
    else()
      set( _bb_warning_compiler_id "clang" )
    endif()
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    set( _bb_warning_compiler_id "msvc" )
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
    set( _bb_warning_compiler_id "intel" )
    if( APPLE )
      # No support for intel yet.
      return() 
    endif()
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Cray" )
    set( _bb_warning_compiler_id "cray" )
    return()    
  else()
    # Not a compiler ID we have support for.
    return()
  endif()
 
  # Unset a few variables the logic below depends on. The calling context may have defined them.
  unset( _bb_warning_options )        
  unset( _bb_tmp_list_var )
  
  if( ${ARGC} GREATER 1 )
    set( _bb_tmp_list_var "${ARGN}" )
    # Analyze the first argument it may be a toolset, warnings-as-errors or a warning option.
    list( GET _bb_tmp_list_var 0 _arg1 )
    
    if( "${_arg1}" MATCHES "^(gcc|clang|msvc|intel)" )
      #message( STATUS "bb_enable_warnings(): found toolset argument ${ARGV0}" )
      if( "${_arg1}" MATCHES "^([a-z]+)-([0-9]+)$" )
        # Strip version suffix; e.g. gcc-8 -> gcc
        set( _bb_warning_toolset         "${CMAKE_MATCH_1}" )
        # Save major version
        set( _bb_warning_toolset_version "${CMAKE_MATCH_2}" )
        # Get compiler's minor version
        if( CMAKE_CXX_COMPILER_VERSION MATCHES "^[0-9]+\\.([0-9]+)" )
          string( APPEND _bb_warning_toolset_version ".${CMAKE_MATCH_1}" )
        endif()
      elseif( "${_arg1}" MATCHES "^([a-z]+)-([0-9]+\\.[0-9]+)" )
        # Strip version suffix; e.g. gcc-4.8 -> gcc
        set( _bb_warning_toolset         "${CMAKE_MATCH_1}" )
        set( _bb_warning_toolset_version "${CMAKE_MATCH_2}" )
      else()
        set( _bb_warning_toolset         "${_arg1}" )
        # Fake a version matching the current compiler version which simplifies the logic below.
        if( DEFINED bb_compiler_version_major_minor )
          set( _bb_warning_toolset_version "${bb_compiler_version_major_minor}" )
        else()
          _bb_get_cxx_compiler_version_major_minor( _bb_warning_toolset_version )
        endif()
      endif()
      
      if( ( NOT _bb_warning_toolset STREQUAL _bb_warning_compiler_id ) OR ( NOT _bb_warning_toolset_version VERSION_EQUAL bb_compiler_version_major_minor ) )
        # No match for current CXX compiler
        # message( STATUS "_bb_enable_warnings_helper(): no match for current compiler" )
        return()
      endif()
      
      # Drop the toolset from the list of arguments to be processed later on
      list( REMOVE_AT _bb_tmp_list_var 0 )
    endif()
  endif()
  
  if( CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang)$" )
    if( NOT XCODE )
      set( _bb_warning_options -Wall )
    endif()
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    set( _bb_warning_options -Wall -fdiagnostics-show-option )
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    # Do we have to override the warning level?
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
    # Do we have to override the warning level on Windows/MSVC?
    if( ( NOT MSVC ) AND ( NOT XCODE ) )
      set( _bb_warning_options -Wall )
    endif()
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Cray" )
    # Do we have to override the warning level?    
  endif()
  
  #message( STATUS "bb_enable_warnings(): processing additional warning options ${_bb_tmp_list_var}" )
  foreach( _bb_v IN LISTS _bb_tmp_list_var )
    #message( STATUS "processing ${_bb_v}" )
    if( ${_bb_v} STREQUAL "warnings-as-errors" )
      if( CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|AppleClang|Clang)$" )
        list( APPEND _bb_warning_options "-Werror" )
      elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
        if( MSVC )
          list( APPEND _bb_warning_options "/WX" )
        else()
          list( APPEND _bb_warning_options "-Werror" )
        endif()          
      elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
        list( APPEND _bb_warning_options "/WX" )
      endif()
    else()
      list( APPEND _bb_warning_options "${_bb_v}" )
    endif()
  endforeach()
  
  if( DEFINED _bb_warning_options )
    if( MSVC )
      if( CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
        # Replace -XXX with /XXX avoiding the mix of both option specifications. 
        string( REPLACE "-Q" "/Q" _bb_warning_options "${_bb_warning_options}" )
      endif()
    endif()
    set( ${warning_options_} ${_bb_warning_options} PARENT_SCOPE )
  endif()
endfunction( _bb_enable_warnings_helper )


macro( bb_get_program_files_x86 program_files_x86_ )
  bb_get_env_icase( ${program_files_x86_} "ProgramFiles(x86)" TO_CMAKE_PATH )
endmacro()


macro( bb_get_program_files program_files_ )
  bb_get_env_icase( ${program_files_} "ProgramFiles" TO_CMAKE_PATH )
endmacro()


macro( bb_get_program_data program_data_ )
  bb_get_env_icase( ${program_data_} "ProgramData" TO_CMAKE_PATH )
endmacro()


function( bb_get_env_icase env_var_value_ env_var_ )
  unset( _path_conversion )
  if( ${ARGC} EQUAL 3 )
    set( _path_conversion ${ARGV2} )
    if( NOT ${_path_conversion} MATCHES "^(TO_CMAKE_PATH|TO_NATIVE_PATH)$" )
      message( FATAL_ERROR "illegal conversion ${_path_conversion} specified, use TO_CMAKE_PATH or TO_NATIVE_PATH" )
    endif()
  elseif( ${ARGC} GREATER 3 )
     message( FATAL_ERROR "too many arguments specified: ${ARGV}" )
  endif()
  unset( _env_var_value )
  if( DEFINED ENV{${env_var_}} )
    set( _env_var_value "$ENV{${env_var_}}" )
  else()
    # Mixed-case environment variable?
    string( TOUPPER "${env_var_}" _env_var_uc )
    if( NOT ${_env_var_uc} STREQUAL ${env_var_} )
      # Try uppercase environment variable to suit buildbot and possibly other python frameworks.
      if( DEFINED ENV{${_env_var_uc}} )
        set( _env_var_value "$ENV{${_env_var_uc}}" )
      endif()
    endif()
  endif()
  if( DEFINED _env_var_value )
    if( DEFINED _path_conversion )
      file( ${_path_conversion} "${_env_var_value}" _env_var_value )
    endif()
    set( ${env_var_value_} "${_env_var_value}" PARENT_SCOPE )
  else()
    # Should we assign -NOTFOUND instead?
    if( DEFINED ${env_var_value_} )
      unset( ${env_var_value_} PARENT_SCOPE )
    endif()
  endif()
endfunction()


function( _bb_find_proj_home proj_home_ home_dir_ )
  unset( _proj_home )
  if( DEFINED ENV{PROJ_HOME} )
    # message( STATUS "BBuildEnv: using environment variable PROJ_HOME=$ENV{PROJ_HOME}" )
    # Force forward slashes on Windows
    file( TO_CMAKE_PATH "$ENV{PROJ_HOME}" _proj_home )
    if( NOT EXISTS ${_proj_home} )
      message( FATAL_ERROR "\
  Environment variable PROJ_HOME=${_proj_home}
  points to a non-existing directory.
      " )
    endif()
  elseif( EXISTS "${home_dir_}/projects" )
    set( _proj_home "${home_dir_}/projects" )
  elseif( BBuildEnv_MSYS )
    # Check for %USERPROFILE%/projects as a fallback when using MSYS.
    bb_get_home_dir( _home_dir NATIVE )
    if( EXISTS "${_home_dir}/projects" )
      set( _proj_home "${_home_dir}/projects" )
    endif()
  endif()
  if( DEFINED _proj_home )
    set( ${proj_home_} "${_proj_home}" PARENT_SCOPE )
  endif()
endfunction()

#
# search path: $ENV{PROJ_HOME} ${CMAKE_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR}/../.. ${bb_home_dir}/projects
#
function( bb_set_external_dir dir_var_ dir_ )
  unset( _dir )
  set( _dir_required TRUE )
  
  if( ARGC GREATER 2 )
    # Process optional arguments
    foreach( _arg IN LISTS ARGN )
      if( _arg STREQUAL "OPTIONAL" )
        set( _dir_required FALSE )
      endif()
    endforeach()
  endif()
  
  if( IS_ABSOLUTE "${dir_}" )
    if( NOT EXISTS "${dir_}" )
      if( _dir_required )
        message( FATAL_ERROR "bb_set_external_dir(): ${dir_} does not exist." )
      endif()
    else()
      set( _dir "${dir_}" )
    endif()
  else()
    unset( _search_path ) 
    if( DEFINED ENV{PROJ_HOME} )
      file( TO_CMAKE_PATH "$ENV{PROJ_HOME}" _proj_home )
      list( APPEND _search_path "${_proj_home}" )
    endif()
    foreach( _dir_tmp "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}/../.." )
      if( EXISTS "${_dir_tmp}" )
        get_filename_component( _dir_norm "${_dir_tmp}" REALPATH )
        list( APPEND _search_path "${_dir_norm}" )
      endif()
    endforeach()
    bb_get_home_dir( _home_dir )
    if( EXISTS "${_home_dir}/projects" )
      list( APPEND _search_path "${_home_dir}/projects" )
    endif()
    if( BBuildEnv_MSYS )
      bb_get_home_dir( _home_dir NATIVE )
      if( EXISTS "${_home_dir}/projects" )
        list( APPEND _search_path "${_home_dir}/projects" )
      endif()
    endif()
    list( REMOVE_DUPLICATES _search_path )
    foreach( _path IN LISTS _search_path )
      if( EXISTS "${_path}/${dir_}" )
        set( _dir "${_path}/${dir_}" )
        break()
      endif()
    endforeach()
    if( NOT DEFINED _dir )
      if( _dir_required )
        message( FATAL_ERROR "bb_set_external_dir(): cannot find ${dir_} in ${_search_path}. You may use the environment variable PROJ_HOME to supplement the default search path." )
      endif()
    else()
      get_filename_component( _dir "${_dir}" REALPATH )
    endif()
  endif()
  if( DEFINED _dir )
    set( ${dir_var_} "${_dir}" PARENT_SCOPE )
  endif()
endfunction()


# 
# Additional system information
# 
macro( bb_get_python_script_path script_path_ script_dir_ script_name_ )
  if( PYTHONINTERP_FOUND )
    if( ${PYTHON_VERSION_STRING} VERSION_LESS 3.0 )
      set( ${script_path_} "${script_dir_}/${script_name_}.py" )
    else()
      set( ${script_path_} "${script_dir_}/${script_name_}3.py" )
    endif()
  else()
    message( FATAL_ERROR "bb_get_python_script_path(): configuration error: python interpreter not found, please contact technical support." )
  endif()
endmacro()
 

function( _bb_query_linux_pkg_arch pkg_arch_ )
  if( bb_dpkg_cmd )
    execute_process( COMMAND ${bb_dpkg_cmd} --print-architecture
                     RESULT_VARIABLE _retv_child
                     OUTPUT_VARIABLE _pkg_arch
                     OUTPUT_STRIP_TRAILING_WHITESPACE )
    if( NOT _retv_child EQUAL 0 )
      message( FATAL_ERROR "${bb_dpkg_cmd} --print-architecture failed, please contact technical support." )
    endif()
    set( ${pkg_arch_} "${_pkg_arch}" PARENT_SCOPE )
  elseif( bb_rpm_cmd )
    execute_process( COMMAND ${bb_rpm_cmd} --eval "%_arch"
                     RESULT_VARIABLE _retv_child
                     OUTPUT_VARIABLE _pkg_arch
                     OUTPUT_STRIP_TRAILING_WHITESPACE )
    if( NOT _retv_child EQUAL 0 )
      message( FATAL_ERROR "${bb_rpm_cmd} --eval %_arch failed, please contact technical support." )
    endif()
  endif()
  if( DEFINED _pkg_arch )
    set( ${pkg_arch_} "${_pkg_arch}" PARENT_SCOPE )    
  endif()  
endfunction()

function( _bb_find_macosx_isysroot isysroot_ )
  if( CMAKE_HOST_APPLE )
    execute_process( COMMAND xcrun --sdk macosx --show-sdk-path
                     RESULT_VARIABLE _retv_child
                     OUTPUT_VARIABLE _isysroot
                     OUTPUT_STRIP_TRAILING_WHITESPACE )
    # message( STATUS "macosx isysroot discovery: retv_child=${_retv_child} ${_isysroot}" )
    if( _retv_child EQUAL 0 )
      set( ${isysroot_} ${_isysroot} PARENT_SCOPE )
    endif()
  endif()
endfunction()

function( _bb_query_system_info system_info_ )

  #message( STATUS "_bb_query_system_info: starting  system_info=${${system_info_}}" )
  if( DEFINED ${system_info_} )
    return()
  endif()

  #message( STATUS "BBuildEnv: collecting additional system information ..." )
  
  foreach( v _lsb_distro_name _lsb_distro_codename _lsb_distro_version _distro_pkg_fmt _distro_pkg_arch _isysroot )
    # message( STATUS "v = ${v}" )
    set( ${v} "none" )
  endforeach()
  
  set( _os_arch "x86_64" )
  
  if( CMAKE_HOST_WIN32 )
    # cmake host system: Windows-6.1.7601
    set( _system_info "windows" )
    if( CMAKE_HOST_SYSTEM MATCHES "[wW][A-Za-z]+-([0-9.]+)" )
      set( _lsb_distro_version "${CMAKE_MATCH_1}" )
    endif()
  elseif( CMAKE_HOST_APPLE )
    set( _system_info "macosx" )
    if( NOT CMAKE_VERSION VERSION_LESS 3.10.0 )
      # sw_vers -productVersion
      cmake_host_system_information( RESULT _lsb_distro_version QUERY OS_RELEASE )
      # sw_vers -buildVersion
      # cmake_host_system_information( RESULT _lsb_distro_version QUERY OS_VERSION )
    else()
      execute_process( COMMAND sw_vers -productVersion
                       OUTPUT_VARIABLE _lsb_distro_version
                       RESULT_VARIABLE _retv_child
                       OUTPUT_STRIP_TRAILING_WHITESPACE )    
      if( NOT _retv_child EQUAL 0 )
        message( WARNING "sw_vers -productVersion failed, please contact technical support." )
      endif()      
    endif()

    if( NOT CMAKE_CROSSCOMPILING )
      _bb_find_macosx_isysroot( _isysroot )
    endif()  
  elseif( CMAKE_HOST_UNIX )
    set( _system_info "linux" )
    if( CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(i686|x86)$" )
      set( _os_arch "x86" )
    endif()
    
    mark_as_advanced( bb_dpkg_cmd bb_bb_rpm_cmd bb_lsb_release_cmd )
    
    find_program( bb_dpkg_cmd "dpkg" )
    if( bb_dpkg_cmd )
      set( _distro_pkg_fmt "deb" )
    else()
      find_program( bb_rpm_cmd "rpm" )
      if( bb_rpm_cmd )
        set( _distro_pkg_fmt "rpm" )
      endif()
    endif()
    find_program( bb_lsb_release_cmd "lsb_release" )
    
    if( DEFINED ENV{CRAYOS_VERSION} )
      set( _lsb_distro_version $ENV{CRAYOS_VERSION} )
      set( _lsb_distro_name     "cray" )
      set( _lsb_distro_codename "none" )
    else()
      if( NOT bb_lsb_release_cmd )
        message( WARNING "BBuildEnv: lsb_release not found, no way to retrieve additional linux system information." )
        #return()
      else()
        execute_process( COMMAND ${bb_lsb_release_cmd} -is
                         RESULT_VARIABLE _retv_child
                         OUTPUT_VARIABLE _lsb_distro_name
                         OUTPUT_STRIP_TRAILING_WHITESPACE )
        if( NOT _retv_child EQUAL 0 )
          message( WARNING "${bb_lsb_release_cmd} -is failed, please contact technical support." )
        endif()
        execute_process( COMMAND ${bb_lsb_release_cmd} -cs
                         RESULT_VARIABLE _retv_child
                         OUTPUT_VARIABLE _lsb_distro_codename
                         OUTPUT_STRIP_TRAILING_WHITESPACE )
        if( NOT _retv_child EQUAL 0 )
          message( WARNING "${bb_lsb_release_cmd} -cs failed, please contact technical support." )
        endif()    
        execute_process( COMMAND ${bb_lsb_release_cmd} -rs
                         RESULT_VARIABLE _retv_child
                         OUTPUT_VARIABLE _lsb_distro_version
                         OUTPUT_STRIP_TRAILING_WHITESPACE )
        if( NOT _retv_child EQUAL 0 )
          message( WARNING "${bb_lsb_release_cmd} -rs failed, please contact technical support." )
        endif()
      endif()
    endif()
    
    _bb_query_linux_pkg_arch( _distro_pkg_arch )

    # convert distro name to lowercase 
    string( TOLOWER "${_lsb_distro_name}" _lsb_distro_name )
    # convert distro codename to lowercase
    string( TOLOWER "${_lsb_distro_codename}" _lsb_distro_codename )
    # suse does not seem to provide a reasonable codename via lsb_release -cs and issues "n/a".  Such
    # a string is likely to cause problems if used as filename component or evaluated in a similar context.
    # Hence, it's replace by none.
    if( _lsb_distro_codename STREQUAL "n/a" )
      set( _lsb_distro_codename "none" )
    endif()
  endif()
  if( _lsb_distro_version STREQUAL "none" )
    set( _lsb_distro_version "0.0"  )
  endif()
  list( APPEND _system_info ${_os_arch} ${_lsb_distro_name} ${_lsb_distro_codename} ${_lsb_distro_version} ${_distro_pkg_fmt} ${_distro_pkg_arch} ${_isysroot} )
  set( ${system_info_} "${_system_info}" CACHE INTERNAL "additional system information" )
endfunction( _bb_query_system_info )

function( _bb_get_platform_dir platform_dir_ )
  if( CMAKE_CROSSCOMPILING )
    if( CMAKE_SYSTEM_PROCESSOR MATCHES "^(i686|x86)$" )
      set( _platform_dir "x86" )
    elseif( CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$" )
      set( _platform_dir "x86_64" )
    else()
      set( _platform_dir "${CMAKE_SYSTEM_PROCESSOR}" )
    endif()  
  else()
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( _platform_dir "x86_64" )
    else()
      set( _platform_dir "x86" )
    endif()
  endif()
  set( ${platform_dir_} "${_platform_dir}" PARENT_SCOPE )
endfunction()

function( _bb_get_toolset_subdir toolset_subdir_ compiler_version_ )
  unset( _toolset_subdir )
  # Assume cmake is able to figure out CMAKE_CXX_COMPILER_ID for all compilers including cross compilers.
  if( CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang)$" )
    set( _toolset_subdir "clang-${compiler_version_}" )
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    set( _toolset_subdir "gcc-${compiler_version_}" )
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    set( _toolset_subdir "msvc-${compiler_version_}" )
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Intel" )
    set( _toolset_subdir "intel-${compiler_version_}" )
    if( MSVC )
      if( NOT DEFINED bb_generator_alias )
        message( FATAL_ERROR "MSVC defined but bb_generator_alias is undefined, looks like an internal bug. Please contact technical support." )
      else()
        set( _toolset_subdir "intel-${compiler_version_}-${bb_generator_alias}" )
      endif()
    endif()
  elseif( CMAKE_CXX_COMPILER_ID STREQUAL "Cray" )
    set( _toolset_subdir "cray-${compiler_version_}" )
  else()
    message( FATAL_ERROR "unsupported CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}, please contact technical support." )
  endif()
  if( MINGW )
    # MinGW is either a cross compiler or a native compiler.
    set( _toolset_subdir "gcc-mingw-${compiler_version_}" )
  elseif( CMAKE_CROSSCOMPILING )
    if( BBuildEnv_DEBUG )
      message( STATUS "[ ${CMAKE_CURRENT_LIST_FILE}:${CMAKE_CURRENT_LIST_LINE} ] "
                      "cross compiling for ${CMAKE_SYSTEM_PROCESSOR} CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID} cxx_version=${compiler_version_}" )
    endif()
    # message( FATAL_ERROR "no support for this type of cross compiler, please contact technical support." )
  endif()
  if( DEFINED _toolset_subdir )
    set( ${toolset_subdir_} "${_toolset_subdir}" PARENT_SCOPE )
  endif()
endfunction()

function( bb_get_os_arch os_arch_ )
  list( GET bb_system_info 1 _os_arch )
  set( ${os_arch_} "${_os_arch}" PARENT_SCOPE )
endfunction()

function( bb_get_target_arch target_arch_ )
  list( GET bb_system_info 1 _target_arch )
  if( MSVC )
    if( CMAKE_SIZEOF_VOID_P EQUAL 4 )
      set( _target_arch "x86" )
    endif()
  elseif( MINGW )
    if( CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" )
      set( _target_arch "x86_64" )
    else()
      set( _target_arch "x86" )
    endif()
  elseif( CMAKE_CROSSCOMPILING )
    # tbd
    set( _target_arch "${CMAKE_SYSTEM_PROCESSOR}" )
  endif()
  set( ${target_arch_} "${_target_arch}" PARENT_SCOPE )
endfunction()

function( bb_get_linux_distro_name distro_name_ )
  list( GET bb_system_info 2 _distro_name )
  set( ${distro_name_} "${_distro_name}" PARENT_SCOPE )
endfunction()

function( bb_get_os_codename os_codename_ )
  list( GET bb_system_info 3 _os_codename )
  set( ${os_codename_} "${_os_codename}" PARENT_SCOPE )
endfunction()

function( bb_get_os_version os_version_ )
  list( GET bb_system_info 4 _os_version )
  set( ${os_version_} "${_os_version}" PARENT_SCOPE )
endfunction()

function( bb_get_linux_pkg_fmt pkg_fmt_ )
  list( GET bb_system_info 5 _pkg_fmt )
  set( ${pkg_fmt_} "${_pkg_fmt}" PARENT_SCOPE )
endfunction()

function( bb_get_linux_pkg_arch pkg_arch_ )
  list( GET bb_system_info 6 _pkg_arch )
  set( ${pkg_arch_} "${_pkg_arch}" PARENT_SCOPE )
endfunction()

function( bb_get_isysroot isysroot_ )
  list( GET bb_system_info 7 _isysroot )
  set( ${isysroot_} "${_isysroot}" PARENT_SCOPE )
endfunction()



function( bb_get_imp_targets_from_components target_list_ target_prefix_ config_ comp1_ )
  set( _comp_list ${comp1_} ${ARGN} )
  unset( _target_list )
  #message( STATUS "bb_get_imp_targets_from_components(): ${_comp_list}" )
  string( TOUPPER "${config_}" _config_uc )
  set( _target_prop_list INTERFACE_LINK_LIBRARIES_${_config_uc} INTERFACE_LINK_LIBRARIES IMPORTED_LINK_INTERFACE_LIBRARIES_${_config_uc} IMPORTED_LINK_INTERFACE_LIBRARIES )
  foreach( _comp IN LISTS _comp_list )
    set( _target_name ${target_prefix_}${_comp} )
    list( APPEND _target_list ${_target_name} )
    foreach( _target_prop IN LISTS _target_prop_list )
      get_target_property( _prop_value ${_target_name} ${_target_prop} )
      #message( STATUS "${_target_name} ${_target_prop} -> _prop_value=${_prop_value}" )
      if( _prop_value )
        #message( STATUS "_bb_get_qt5_dlls: Qt5::${_comp}: INTERFACE_LINK_LIBRARIES=${_prop_value}" )
        foreach( _lnk_lib ${_prop_value} )
          if( _lnk_lib MATCHES "^${target_prefix_}[a-zA-Z]" )
            list( APPEND _target_list ${_lnk_lib} )
          endif()
        endforeach()
        break()
      endif()
    endforeach()
  endforeach()
  if( DEFINED _target_list )
    list( REMOVE_DUPLICATES _target_list )
    set( ${target_list_} ${_target_list} PARENT_SCOPE )
  endif()
endfunction()

# Optional argument: NO_PDB_FILES
function( bb_get_dsos_from_imp_targets dso_list_ config_ target1_ )
  string( TOUPPER "${config_}" _config_uc )
  set( _target_list ${target1_} ${ARGN} )
  if( NO_PDB_FILES IN_LIST _target_list )
    set( _no_pdb_files TRUE )
    list( REMOVE_ITEM _target_list NO_PDB_FILES )
  else()
    set( _no_pdb_files FALSE )
  endif()
  unset( _bin_dir )
  unset( _dso_list )
  # Walk _target_list and collect paths to dlls.
  foreach( _target ${_target_list} )
    # "IMPORTED_LOCATION" "IMPORTED_LOCATION_RELEASE" "IMPORTED_LOCATION_DEBUG"
    get_target_property( _prop_value ${_target} IMPORTED_LOCATION_${_config_uc} )
    if( NOT _prop_value )
      message( WARNING "bb_get_dsos_from_imp_targets(): no IMPORTED_LOCATION_${_config_uc} for target ${_target}" )
      get_target_property( _prop_value ${_target} IMPORTED_LOCATION )
    endif()
    if( _prop_value )
      # Ignore any static libraries
      get_filename_component( _lib_ext ${_prop_value} EXT )
      if( WIN32 )
        if( NOT _lib_ext MATCHES "^\\.(dll|DLL)$" )
          continue()
        endif()
      elseif( APPLE )
        if( _lib_ext STREQUAL ".a" )
          continue()
        endif()      
      else()
        # Linux: extension may have a trailing version number.
        if( NOT _lib_ext MATCHES "^\\.so" )
          continue()
        endif()      
      endif()
      # message( STATUS "${_target}: IMPORTED_LOCATION_RELEASE=${_prop_value}" )
      if( NOT DEFINED _bin_dir )
        get_filename_component( _bin_dir ${_prop_value} DIRECTORY )
      endif()
      list( APPEND _dso_list ${_prop_value} )
      if( _config_uc STREQUAL "DEBUG" )
        if( WIN32 AND NOT ${_no_pdb_files} )
          # Add PDB file if available
          get_filename_component( _filenm ${_prop_value} NAME_WE )
          if( EXISTS "${_bin_dir}/${_filenm}.pdb" )
            list( APPEND _dso_list "${_bin_dir}/${_filenm}.pdb" )
          endif()
        endif()
      endif()
    endif()
  endforeach()
  if( DEFINED _dso_list )
    set( ${dso_list_} ${_dso_list} PARENT_SCOPE )
  endif()
endfunction()


function( bb_file action_ input_path_ output_path_ )

  if( "${action_}" STREQUAL "TO_SHORT_PATH" )
    if( NOT CMAKE_HOST_WIN32 )
      message( FATAL_ERROR "Short path conversion is only supported on Windows host systems." )
    endif()
  endif()
  
  if( "${action_}" MATCHES "^(TO_NATIVE_PATH|TO_SHORT_PATH)$" )
    if( CMAKE_HOST_WIN32 )      
      if( CMAKE_GENERATOR STREQUAL "MinGW Makefiles" )
        # Painful hack to get backslashes as the CMake generator mgwmake seems to enable Linux path conversion 
        # rather than Windows path conversion.
        # example: C:/Program Files -> C:/Program\ Files
        string( REPLACE "/" "\\" _native_path "${input_path_}" )
      else()
        file( TO_NATIVE_PATH "${input_path_}" _native_path )
      endif()
    else()
      file( TO_NATIVE_PATH "${input_path_}" _native_path )
    endif()
    # message( STATUS "input_path='${input_path_}' native_path='${_native_path}'" )
    if( "${action_}" STREQUAL "TO_NATIVE_PATH" )
      set( ${output_path_} "${_native_path}" PARENT_SCOPE )
    endif()
  endif()
  
  if( "${action_}" STREQUAL "TO_SHORT_PATH" )
    find_program( BB_SHELL_PROG "cmd.exe" )
    bb_file( TO_NATIVE_PATH "${BB_SHELL_PROG}" script_launcher )
    set( script_cmd_line "/c" )
  
    bb_get_binary_tool_dir( _tool_dir )
  
    if( NOT EXISTS "${_tool_dir}/get_short_path.cmd" )
      file( WRITE "${_tool_dir}/get_short_path.cmd" "@echo off\n" "echo %~s1\n" )
    endif()
    
    bb_file( TO_NATIVE_PATH "${_tool_dir}/get_short_path.cmd" cmd_script )
    string( APPEND script_cmd_line " ${cmd_script}" )
    string( APPEND script_cmd_line " \"${_native_path}\"" )
    #
    separate_arguments( script_args NATIVE_COMMAND "${script_cmd_line}" )
    # message( STATUS "Launching: ${script_launcher} ${script_args}" )
    execute_process( COMMAND "${script_launcher}" ${script_args} WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" RESULT_VARIABLE retv OUTPUT_VARIABLE short_path OUTPUT_STRIP_TRAILING_WHITESPACE )

    if( retv EQUAL 0 )
      #message( STATUS "python interpreter (short path): ${short_path}" )
      set( ${output_path_} "${short_path}" PARENT_SCOPE )
    else()
      message( FATAL_ERROR "short path conversion failed: ${retv}, please contact technical support." )
    endif()
  endif()
endfunction()



#
# Internal macro to setup the build environment. 
#
macro( bb_build_env_setup )

  message( STATUS "Setting up BBuildEnv ${BBuildEnv_VERSION}" )

  unset( BBuildEnv_ROOT_DIR )
  unset( BBuildEnv_SCRIPT_DIR )
  set( BBuildEnv_MSYS FALSE )
  
  if( CMAKE_HOST_WIN32 )
    if( DEFINED ENV{MSYSTEM} )
      message( STATUS "BBuildEnv: MSYS[2] platform detected." )
      set( BBuildEnv_MSYS TRUE )
    endif()
  endif()
  
  bb_check_build_type()

  if( EXISTS "${CMAKE_CURRENT_LIST_DIR}/../.." )
    get_filename_component( BBuildEnv_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH )
    if( EXISTS "${BBuildEnv_ROOT_DIR}/bin" )
      set( BBuildEnv_SCRIPT_DIR "${BBuildEnv_ROOT_DIR}/bin" )
    endif()
  endif()

  
  _bb_query_system_info( bb_system_info )
  # message( STATUS "bb_system_info: ${bb_system_info}" )
    
  bb_get_home_dir( bb_home_dir )
  
  _bb_find_proj_home( bb_proj_home "${bb_home_dir}" )

  # Add a cmake generator alias
  unset( bb_generator_alias )
  if( CMAKE_GENERATOR STREQUAL "Unix Makefiles" )
    set( bb_generator_alias "umake" )
  elseif( CMAKE_GENERATOR STREQUAL "Xcode" )
    set( bb_generator_alias "xcode" )
  elseif( CMAKE_GENERATOR MATCHES "Visual Studio ([0-9][0-9])" )
    set( bb_generator_alias "vs${CMAKE_MATCH_1}" )
  elseif( CMAKE_GENERATOR STREQUAL "Ninja" )
    set( bb_generator_alias "ninja" )
  elseif( CMAKE_GENERATOR STREQUAL "MinGW Makefiles" )
    set( bb_generator_alias "mgwmake" )
  else()
    message( WARNING "BBuildEnv.cmake: generator '${CMAKE_GENERATOR}' is not fully supported yet, please contact technical support for further information." ) 
    #return()
    string( TOLOWER "${CMAKE_GENERATOR}" _generator_uc )
    string( REPLACE " " "_" bb_generator_alias "${_generator_uc}" )
    unset( _generator_uc )
  endif()
  #message( STATUS "bb_generator_alias: ${bb_generator_alias}" ) 
  _bb_get_cxx_compiler_version_major_minor( bb_compiler_version_major_minor )
  _bb_get_toolset_subdir( bb_toolset_subdir ${bb_compiler_version_major_minor} )
  _bb_get_platform_dir( bb_platform_dir )
  
  # set standard output directories: gcc-5.4/x86_64
  if( DEFINED bb_generator_alias )
    set( BBuildEnv_GENERATOR_ALIAS "${bb_generator_alias}" )
    set( bb_default_output_dir "${bb_generator_alias}/${bb_toolset_subdir}/${bb_platform_dir}" )
  else()
    set( bb_default_output_dir "${bb_toolset_subdir}/${bb_platform_dir}" )
  endif()
  
  # BBuildEnv_OUTPUT_DIR_SUFFIX could be a cache variable to make it customizable. 
  set( BBuildEnv_OUTPUT_DIR_SUFFIX           "${bb_default_output_dir}" )
    
  # the deploy folder may be used to save installer packages.
  set( bb_deploy_dir "${CMAKE_SOURCE_DIR}/deploy" )
    
  if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    set( CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Standard install prefix" FORCE )
  endif()
  
  if( BUILD_SHARED_LIBS )
    set( BBuildEnv_SHARED_DIR_POSTFIX "-shared" )
  else()
    unset( BBuildEnv_SHARED_DIR_POSTFIX )
  endif()

  if( NOT DEFINED BBuildEnv_USE_LIBRARY_NAME_POSTFIX )
    set( BBuildEnv_USE_LIBRARY_NAME_POSTFIX OFF CACHE BOOL "Enable library name postfix" )
  endif()
    
  #set( BBuildEnv_RELEASE_POSTFIX "" )
  set( BBuildEnv_DEBUG_POSTFIX          "-d" )
  set( BBuildEnv_RELWITHDEBINFO_POSTFIX "-rd" )
  set( BBuildEnv_MINSIZEREL_POSTFIX     "-mr" )
  
  
  if( BBuildEnv_USE_LIBRARY_NAME_POSTFIX ) 
    set( CMAKE_DEBUG_POSTFIX                           ${BBuildEnv_DEBUG_POSTFIX} )
    set( CMAKE_RELWITHDEBINFO_POSTFIX                  ${BBuildEnv_RELWITHDEBINFO_POSTFIX} )
    set( CMAKE_MINSIZEREL_POSTFIX                      ${BBuildEnv_MINSIZEREL_POSTFIX} )

    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY                "${CMAKE_SOURCE_DIR}/lib${BBuildEnv_SHARED_DIR_POSTFIX}/${BBuildEnv_OUTPUT_DIR_SUFFIX}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" )
    
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL}" )    
    
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_SOURCE_DIR}/bin${BBuildEnv_SHARED_DIR_POSTFIX}/${BBuildEnv_OUTPUT_DIR_SUFFIX}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" )
    
  else()
    # Using CMake's default library name convention which is the same for all configurations.
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}/lib/${BBuildEnv_OUTPUT_DIR_SUFFIX}/debug${BBuildEnv_SHARED_DIR_POSTFIX}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/lib/${BBuildEnv_OUTPUT_DIR_SUFFIX}/release${BBuildEnv_SHARED_DIR_POSTFIX}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/lib/${BBuildEnv_OUTPUT_DIR_SUFFIX}/relwithdebinfo${BBuildEnv_SHARED_DIR_POSTFIX}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_SOURCE_DIR}/lib/${BBuildEnv_OUTPUT_DIR_SUFFIX}/minsizerel${BBuildEnv_SHARED_DIR_POSTFIX}" )
    
    
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL}" )    
    
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}/bin/${BBuildEnv_OUTPUT_DIR_SUFFIX}/debug${BBuildEnv_SHARED_DIR_POSTFIX}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/bin/${BBuildEnv_OUTPUT_DIR_SUFFIX}/release${BBuildEnv_SHARED_DIR_POSTFIX}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin/${BBuildEnv_OUTPUT_DIR_SUFFIX}/relwithdebinfo${BBuildEnv_SHARED_DIR_POSTFIX}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_SOURCE_DIR}/bin/${BBuildEnv_OUTPUT_DIR_SUFFIX}/minsizerel${BBuildEnv_SHARED_DIR_POSTFIX}" )    
  endif()
    
  
endmacro( bb_build_env_setup )

#message( STATUS "BBuildEnv.cmake: starting: ${CMAKE_GENERATOR}" )
# Setup the build environment
bb_build_env_setup()
#message( STATUS "BBuildEnv.cmake: leaving" )

