#
# lldbinit_update-in.cmake
#

include( @cmake_lldbinit_module@ )

# in-place add
bb_lldbinit_update( ADD "@input_file@" @section_marker@ "@lldbinit_content@" )

# in-place remove
# bb_lldbinit_update( REMOVE "@input_file@" @section_marker@ )
