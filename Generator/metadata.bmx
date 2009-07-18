SuperStrict

' Prefixes
Global GLOBAL_PREFIX$="lugi_"	' Prefix applied before other prefixes (e.g., ${GLOBAL_PREFIX}${METH_PREFIX}${TYPENAME}_${METHODNAME})
Global METH_PREFIX$="glue_"		' Prefix applied to method implementations
Global NOCLASS_VAR_PREFIX$ = "lugi_glob_"
Global INIT_PREFIX$="p_lugi_init"

' Options
Global METH_NODEBUG:Int = False	' Specifies whether or not to add the 'NoDebug' to methods

''' On Metadata:
' Metadata specified may be different between modules.  The default options are what is encourages,
' but if necessary, it's entirely possible to change the metadata to use different names for them.
' However, all metadata is treated as a boolean value - greater than 0 is true, everything else is
' false.  This can't be changed.

' Type only
Global LUGI_META_EXPOSE$ = "expose"			' Sets a type to be exposed via LuGI
Global LUGI_META_HIDEFIELDS$ = "hidefields"	' Sets all fields in the type to be hidden
Global LUGI_META_NOCLASS$ = "noclass"		' Suggests to LuGI that the type's methods should not be associated with a class - requires META_STATIC
Global LUGI_META_STATIC$ = "static"			' Suggests to LuGI that the type should be made static (one instance is created as a global variable)
Global LUGI_META_CONSTRUCTOR$ = "constructor"' Sets the name of a type's constructor
Global LUGI_META_DISABLECONSTRUCTOR$ = "disablenew"' Sets whether or not the type can be instanciated in Lua

' Types & Methods
Global LUGI_META_RENAME$ = "rename"			' Renames a method or type

' Fields & Methods
Global LUGI_META_HIDDEN$ = "hidden"			' Sets whether a method or field is hidden
Global LUGI_META_BOOL$ = "bool"				' Sets whether or not a method or field returns a boolean value (only applies for return types of Int, Short, Byte, and Long)

' Methods
Global LUGI_META_BINDING$ = "binding"		' UNUSED - Specifies the type of binding for the method (late/immediate)
