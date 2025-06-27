import lldb


# Synthetic view for static_vector objects
class static_vector_SynthChildProvider:
  def __init__( self, valobj, dict ):
    self.valobj = valobj

  def num_children( self ):
    size = self.valobj.GetChildMemberWithName( '_size' ).GetValueAsUnsigned()
    return size + 2

  def has_children( self ):
    return self.num_children() != 0

  def get_child_at_index( self, index ):
    if index < 0:
      return None
    if index >= self.num_children():
      return None
    if index == self.num_children() - 2:
      return self.valobj.GetChildMemberWithName( '_arr' )
    if index == self.num_children() - 1:
      return self.valobj.GetChildMemberWithName( '_size' )
    arr = self.valobj.GetChildMemberWithName( '_arr' )
    return arr.GetChildAtIndex(index)

  def update( self ):
    pass


# Summary for UnitArea objects
def static_vector_SummaryProvider( valobj, dict ):
  size = valobj.GetNumChildren() - 2
  return 'size=' + str( size )

# Synthetic view for UnitArea objects
class UnitArea_SynthChildProvider:
  def __init__( self, valobj, dict ):
    self.valobj = valobj

  def num_children( self ):
    blocks = self.valobj.GetChildMemberWithName( 'blocks' )
    size = blocks.GetChildMemberWithName( '_size' ).GetValueAsUnsigned()
    return size + 3

  def has_children( self ):
    return True

  def get_child_at_index( self, index ):
    if index < 0:
      return None
    if index >= self.num_children():
      return None
    if index == 0:
      return self.valobj.GetChildMemberWithName( 'chromaFormat' )
    if index == self.num_children()-2:
      return self.valobj.GetChildMemberWithName( 'blocks' ).GetChildMemberWithName( '_arr' )
    if index == self.num_children()-1:
      return self.valobj.GetChildMemberWithName( 'blocks' ).GetChildMemberWithName( '_size' )
    if index > 0:
      arr = self.valobj.GetChildMemberWithName( 'blocks' ).GetChildMemberWithName( '_arr' )
      return arr.GetChildAtIndex( index - 1 )

  def update( self ):
    pass

# Summary for Area objects
def Area_SummaryProvider( valobj, dict ):
  x      = valobj.GetChildMemberWithName( 'x' ).GetValueAsUnsigned()
  y      = valobj.GetChildMemberWithName( 'y' ).GetValueAsUnsigned()
  width  = valobj.GetChildMemberWithName( 'width' ).GetValueAsUnsigned()
  height = valobj.GetChildMemberWithName( 'height' ).GetValueAsUnsigned()
  return 'x=' + str( x ) + ', y=' + str( y ) + ', width=' + str( width ) + ', height=' + str( height )

# Summary for CompArea objects
def CompArea_SummaryProvider( valobj, dict ):
  compid = valobj.GetChildMemberWithName( 'compID' ).GetValueAsUnsigned()
  if compid == 0:
    comp = 'Y '
  elif compid == 1:
    comp = 'Cb '
  elif compid == 2:
    comp = 'Cr '
  elif compid == 3:
    comp = 'Cb2 '
  elif compid == 4:
    comp = 'Cr2 '
  else:
    comp = 'invalid '
  return comp + Area_SummaryProvider( valobj, dict)

# Summary for UnitArea objects
def UnitArea_SummaryProvider( valobj, dict ):
  area = valobj.GetChildAtIndex( 1 )
  return Area_SummaryProvider( area, dict)

# Summary for CodingUnit objects
def CodingUnit_SummaryProvider( valobj, dict ):
  area = valobj.GetChildMemberWithName( 'blocks' ).GetChildAtIndex( 0 )
  return 'CU: ' + Area_SummaryProvider( area, dict)

# Summary for PredictionUnit objects
def PredictionUnit_SummaryProvider( valobj, dict ):
  area = valobj.GetChildMemberWithName( 'blocks' ).GetChildAtIndex( 0 )
  return 'PU: ' + Area_SummaryProvider( area, dict)

# Summary for TransformUnit objects
def TransformUnit_SummaryProvider( valobj, dict ):
  area = valobj.GetChildMemberWithName( 'blocks' ).GetChildAtIndex( 0 )
  return 'TU: ' + Area_SummaryProvider( area, dict)

def __lldb_init_module( debugger, dict ):
  debugger.HandleCommand( 'type synthetic add -l NextSoftware.static_vector_SynthChildProvider -x "static_vector<.*>$"' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.static_vector_SummaryProvider -e -x "static_vector<.*>$"' )
#  debugger.HandleCommand( 'type summary add --summary-string size=${svar%#} -e -x "static_vector<.*>$"' )
  debugger.HandleCommand( 'type synthetic add -l NextSoftware.UnitArea_SynthChildProvider UnitArea' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.UnitArea_SummaryProvider -e  UnitArea' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.Area_SummaryProvider -e Area' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.CompArea_SummaryProvider -e CompArea' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.CodingUnit_SummaryProvider -e CodingUnit' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.PredictionUnit_SummaryProvider -e PredictionUnit' )
  debugger.HandleCommand( 'type summary add -F NextSoftware.TransformUnit_SummaryProvider -e TransformUnit' )
