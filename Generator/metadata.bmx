SuperStrict

' Type only
Const LUGI_META_EXPOSE$ = "expose"			' Sets a type to be exposed via LuGI
Const LUGI_META_HIDEFIELDS$ = "hidefields"	' Sets all fields in the type to be hidden
Const LUGI_META_NOCLASS$ = "noclass"		' Suggests to LuGI that the type's methods should not be associated with a class - requires META_STATIC
Const LUGI_META_STATIC$ = "static"			' Suggests to LuGI that the type should be made static (one instance is created as a global variable)
Const LUGI_META_CONSTRUCTOR$ = "constructor"' Sets the name of a type's constructor
Const LUGI_META_DISABLECONSTRUCTOR$ = "disablenew"' Sets whether or not the type can be instanciated in Lua

' Types & Methods
Const LUGI_META_RENAME$ = "rename"			' Renames a method or type

' Fields & Methods
Const LUGI_META_HIDDEN$ = "hidden"			' Sets whether a method or field is hidden

' Methods
Const LUGI_META_BINDING$ = "binding"		' UNUSED - Specifies the type of binding for the method (late/immediate)
Const LUGI_META_BOOL$ = "returnbool"		' Sets whether or not a method returns a boolean value (only applies for return types of Int, Short, Byte, and Long)
