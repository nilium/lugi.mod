Rem
	LuGI - Copyright (c) 2009, 2014 Noel R. Cower

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
EndRem

'SuperStrict

Private

Global LUGI_CATEGORIES_ARR:String[]
Global LUGI_CATEGORIES_STRING:String = ""

Public

' Prefixes
Global LUGI_GLOBAL_PREFIX$ = "lugi_"                  ' Prefix applied before other prefixes (e.g., ${GLOBAL_PREFIX}${METH_PREFIX}${TYPENAME}_${METHODNAME})
Global LUGI_METH_PREFIX$ = "glue_"                    ' Prefix applied to method implementations
Global LUGI_NOCLASS_VAR_PREFIX$ = "lugi_glob_"
Global LUGI_INIT_PREFIX$ = "p_lugi_init"

' Options
Global LUGI_METH_NODEBUG:Int = False                  ' Specifies whether or not to add the 'NoDebug' to methods
Global LUGI_CATEGORIES:String = ""                    ' Specifies the categories to generate code for (each category separated by a comma).  Categories are case sensitive.

''' On Metadata:
' Metadata specified may be different between modules.  The default options are what is encourages,
' but if necessary, it's entirely possible to change the metadata to use different names for them.
' However, all metadata is treated as a boolean value - greater than 0 is true, everything else is
' false.  This can't be changed.

' Type only
Global LUGI_META_EXPOSE$ = "expose"                   ' Sets a type to be exposed via LuGI
Global LUGI_META_HIDEFIELDS$ = "hidefields"           ' Sets all fields in the type to be hidden
Global LUGI_META_NOCLASS$ = "noclass"                 ' Suggests to LuGI that the type's methods should not be associated with a class - requires META_STATIC
Global LUGI_META_STATIC$ = "static"                   ' Suggests to LuGI that the type should be made static (one instance is created as a global variable)
Global LUGI_META_CONSTRUCTOR$ = "constructor"         ' Sets the name of a type's constructor
Global LUGI_META_DISABLECONSTRUCTOR$ = "disablenew"   ' Sets whether or not the type can be instanciated in Lua
Global LUGI_META_CATEGORY$ = "category"               ' Sets the category/section/namespace of the type (used to filter types when generating code)

' Types & Methods
Global LUGI_META_RENAME$ = "rename"                   ' Renames a method or type

' Fields & Methods
Global LUGI_META_HIDDEN$ = "hidden"                   ' Sets whether a method or field is hidden
Global LUGI_META_BOOL$ = "bool"                       ' Sets whether or not a method or field returns a boolean value (only applies for return types of Int, Short, Byte, and Long)

' Methods
Global LUGI_META_BINDING$ = "binding"                 ' UNUSED - Specifies the type of binding for the method (late/immediate)
