#.rst:
# BBuildEnvLldb
# -------------
#
# Functions to update an LLDB init file.
#
# ::
#
#   bb_lldbinit_update(ADD <lldbinit_file> <marker> <content> [OUTPUT <output_file>])
#
# Add or replace the content of section labeled with ``marker`` in file ``lldbinit_file``. 
#
# ::
#
#   bb_lldbinit_update(REMOVE <lldbinit_file> <marker> [OUTPUT <output_file>])
#
# Remove the section labeled with ``marker`` from file ``lldbinit_file``.
#

if( NOT CMAKE_VERSION VERSION_LESS 3.10 )
  include_guard( GLOBAL )
endif()

function( bb_lldbinit_update action_ lldbinit_file_ marker_ )
  set( _fnc_name "bb_lldbinit_update" )
  #message( STATUS "${_fnc_name}: entering" )
  
  if( NOT ${action_} MATCHES "^(ADD|REMOVE)$" )
    message( FATAL_ERROR "Unknown update action ${action_} specified, expected ADD or REMOVE." )
  endif()
  
  if( IS_ABSOLUTE "${lldbinit_file_}" )
    set( _lldbinit_file "${lldbinit_file_}" )
  else()
    set( _lldbinit_file "${CMAKE_CURRENT_SOURCE_DIR}/${lldbinit_file_}" )
  endif()
  
  set( _lldbinit_out_file "${_lldbinit_file}" )
  unset( _lldbinit_lines )
  
  # Parse command line and handle optional arguments
  if( ARGC GREATER 3 )
    #message( STATUS "${_fnc_name}: optional arguments ${ARGN}" )
    set( _opt_args ${ARGN} )
    list( LENGTH _opt_args _opt_args_cnt )
    while( _opt_args_cnt GREATER 0 )
      list( GET _opt_args 0 _arg )
      list( REMOVE_AT _opt_args 0 )
      #message( STATUS "${_fnc_name}: processing optional argument ${_arg}" )
      if( _arg STREQUAL "OUTPUT" )
        # Pick the next argument and stop processing
        list( LENGTH _opt_args _opt_args_cnt )
        if( _opt_args_cnt EQUAL 1 )
          list( GET _opt_args 0 _lldbinit_out_file )
          if( NOT IS_ABSOLUTE "${_lldbinit_out_file}" )
            set( _lldbinit_out_file "${CMAKE_CURRENT_SOURCE_DIR}/${_lldbinit_out_file}" )
          endif()          
        else()
          message( FATAL_ERROR "${_fnc_name}: illegal number of arguments." )
        endif()
        break()
      else()
        list( APPEND _lldbinit_lines ${_arg} )
      endif()
      list( LENGTH _opt_args _opt_args_cnt )
    endwhile()
  endif()
  
  #message( STATUS "${_fnc_name}: new_content: ${_lldbinit_lines}" )
  #message( STATUS "${_fnc_name}: input_file: ${_lldbinit_file}" )
  #message( STATUS "${_fnc_name}: output_file: ${_lldbinit_out_file}" )
  
  set( _regex_start "^#[ \t]*<${marker_}>" )
  set( _regex_end "^#[ \t]*</${marker_}>" )
  
  unset( _lldbinit_section_orig )
  unset( _lldbinit_section_new )
  
  unset( _lldbinit_orig_top )
  unset( _lldbinit_orig_bottom )
  set( _start_section_cnt 0 )
  set( _end_section_cnt 0 )
  
  if( action_ STREQUAL "ADD" )
    set( _update_lldbinit_file TRUE )
  elseif( action_ STREQUAL "REMOVE" )
    set( _update_lldbinit_file FALSE )
  endif()
  
  # Create the new lldbinit section 
  foreach( _line IN LISTS _lldbinit_lines )
    string( APPEND _lldbinit_section_new "${_line}" "\n" )
  endforeach()
  
  # Create section start and section end lines 
  _bb_lldbinit_create_marker_lines( ${marker_} _lldbinit_section_start _lldbinit_section_end )
    
  if( EXISTS "${_lldbinit_file}" )
    file( STRINGS ${_lldbinit_file} _lldbinit_lines_orig )
    #message( STATUS "bb_update_lldbinit(): |${_lldbinit_lines_orig}|" )
    #message( STATUS "----" )
    #message( STATUS ${_lldbinit_lines_orig} )
    #message( STATUS "----" )
    foreach( _line IN LISTS _lldbinit_lines_orig )
      #message( STATUS "bb_update_lldbinit(): processing ${_line}" )
      if( _line MATCHES "${_regex_start}" )
        math( EXPR _start_section_cnt "${_start_section_cnt} + 1" )
        continue()
      endif()
      if( _line MATCHES "${_regex_end}" )
        math( EXPR _end_section_cnt "${_end_section_cnt} + 1" )
        continue()
      endif()
      if( ( _start_section_cnt EQUAL 1 ) AND ( _end_section_cnt EQUAL 0 ) )
        # Extract existing section
        string( APPEND _lldbinit_section_orig "${_line}" "\n" )
      elseif( ( _start_section_cnt EQUAL 0 ) AND ( _end_section_cnt EQUAL 0 ) )
        string( APPEND _lldbinit_orig_top "${_line}" "\n" )
      elseif( ( _start_section_cnt EQUAL 1 ) AND ( _end_section_cnt EQUAL 1 ) )
        string( APPEND _lldbinit_orig_bottom "${_line}" "\n" )
      endif()
    endforeach()
    # Check section counters
    if( ( _start_section_cnt EQUAL 1 ) AND ( _end_section_cnt EQUAL 1 ) )
      #message( STATUS "_lldbinit_orig_top:     ${_lldbinit_orig_top}" )
      #message( STATUS "_lldbinit_section_orig: ${_lldbinit_section_orig}" )
      #message( STATUS "_lldbinit_orig_bottom:  ${_lldbinit_orig_bottom}" )
      if( action_ STREQUAL "ADD" )
        if( "${_lldbinit_section_orig}" STREQUAL "${_lldbinit_section_new}" )
          set( _update_lldbinit_file FALSE )
        endif()       
      elseif( action_ STREQUAL "REMOVE" )
        set( _update_lldbinit_file TRUE )
      endif()
    elseif( ( _start_section_cnt EQUAL 0 ) AND ( _end_section_cnt EQUAL 0 ) )
    else()
      # Unsupported format
      message( WARNING "${_fnc_name}: unsupported lldbinit, check number of tags." )
      return()
    endif()
  endif()
  
  if( NOT EXISTS ${_lldbinit_out_file} )
    set( _update_lldbinit_file TRUE )
  endif()
  
  if( _update_lldbinit_file )
    message( STATUS "${_fnc_name}: updating ${_lldbinit_out_file}, section=${marker_}" )
    if( action_ STREQUAL "ADD" )
      file( WRITE ${_lldbinit_out_file} "${_lldbinit_orig_top}" "${_lldbinit_section_start}" "${_lldbinit_section_new}" "${_lldbinit_section_end}" "${_lldbinit_orig_bottom}" )
    elseif( action_ STREQUAL "REMOVE" )
      file( WRITE ${_lldbinit_out_file} "${_lldbinit_orig_top}" "${_lldbinit_orig_bottom}" )
    endif()
  endif() 
    
  #message( STATUS "${_fnc_name}: leaving" )  
endfunction( bb_lldbinit_update )

function( _bb_lldbinit_create_marker_lines marker_ start_marker_ end_marker_ )
  string( TIMESTAMP _time_now "%Y-%m-%d %H:%M:%S" )
  set( ${start_marker_} "#<${marker_}> - automatically inserted at ${_time_now}, don't remove.\n" PARENT_SCOPE )
  #set( ${start_marker_} "#<${marker_}>  - automatically inserted, don't remove.\n" PARENT_SCOPE )
  set( ${end_marker_}   "#</${marker_}> - automatically inserted, don't remove.\n" PARENT_SCOPE )
endfunction( _bb_lldbinit_create_marker_lines )
 