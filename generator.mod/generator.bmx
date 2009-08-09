Rem
	LuGI - Copyright (c) 2009 Noel R. Cower

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

SuperStrict

Module LuGI.Generator

ModuleInfo "Name: LuGI Code Generator"
ModuleInfo "Description: Default code generator for Lua glue code used by LuGI"
ModuleInfo "Author: Noel Cower"
ModuleInfo "License: MIT"
ModuleInfo "URL: <a href=~qhttp://github.com/nilium/lugi.mod/~q>http://github.com/nilium/lugi.mod/</a>"

Import brl.LinkedList
Import brl.Map
Import brl.Random
Import brl.Reflection
Import brl.Stream
Import Brl.System
?Threaded
Import brl.Threads
?

Private

Include "utility.bmx"

' The actually meat of the code here...
Include "metadata.bmx"
Include "method.bmx"
Include "field.bmx"
Include "type.bmx"

Public

Function GenerateGlueCode(url:Object = Null)
	Assert url Else "GenerateGlueCode: No output URL provided"
	
	' Clear existing types (in case the code is generating multiple wrappers)
	ClearExposedTypes()
	
	Local output:TStream = OpenStream(url, False, True)
	Assert output Else "GenerateGlueCode: Unable to create writeable output stream"
	
	SeedRndWithDateTime
	
	' Set up the categories array
	LUGI_CATEGORIES_STRING = LUGI_CATEGORIES
	If LUGI_CATEGORIES_STRING.Trim().Length Then
		LUGI_CATEGORIES_ARR = LUGI_CATEGORIES_STRING.Split(",")
		For Local idx:Int = 0 Until LUGI_CATEGORIES_ARR.Length
			LUGI_CATEGORIES_ARR[idx] = LUGI_CATEGORIES_ARR[idx].Trim()
		Next
	Else
		LUGI_CATEGORIES_ARR = New String[0]
	EndIf
	
	BuildTypeInfo()
	
	output.WriteString("Private~n~n")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		Local outs$ = t.Implementation()
		If outs Then
			output.WriteLine("'---- "+t.name)
			output.WriteString(outs)
		EndIf
	Next
	
	Local pretag$ = GenerateUniqueTag(16)
	Local posttag$ = GenerateUniqueTag(16)
	
	' Generate the initialization function
	output.WriteLine("Function "+LUGI_GLOBAL_PREFIX+LUGI_INIT_PREFIX+"pre_"+pretag+"(lua_vm:Byte Ptr, register_field(off%, typ%, name$, class@ Ptr), register_method(luafn:Int(state:Byte Ptr), name$, class@ Ptr))")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PreInitBlock()+"~n")
	Next
	
	output.WriteLine("End Function")
	output.WriteString("New LuGIInitFunction.PreInit("+LUGI_GLOBAL_PREFIX+LUGI_INIT_PREFIX+"pre_"+pretag+", False)~n~n~n")
	
	output.WriteLine("Function "+LUGI_GLOBAL_PREFIX+LUGI_INIT_PREFIX+"post_"+posttag+"(lua_vm:Byte Ptr, constructor:Int(state:Byte Ptr))")
	
	' Any initialization code following the exposure (e.g., setting global constructors and such)
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PostInitBlock())
	Next
	
	output.WriteLine("End Function")
	output.WriteString("New LuGIInitFunction.PostInit("+LUGI_GLOBAL_PREFIX+LUGI_INIT_PREFIX+"post_"+posttag+")~n~n~n")
	
	output.WriteString("~nPublic~n~n")
	
	output.Flush
	output.Close
End Function
