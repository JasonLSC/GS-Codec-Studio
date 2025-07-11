import lldb

def Rectangle_SummaryProvider (valobj,internal_dict):
    height_val = valobj.GetChildMemberWithName('height')
    width_val = valobj.GetChildMemberWithName('width')
    height = height_val.GetValueAsUnsigned(0)
    width = width_val.GetValueAsUnsigned(0)
    area = height*width
    perimeter = 2*(height + width)
    return 'Area: ' + str(area) + ', Perimeter: ' + str(perimeter)

def __lldb_init_module( debugger, dict ):
    debugger.HandleCommand( 'type summary add -F CMakeBuildLldbExt.Rectangle_SummaryProvider -e  Rectangle' )
