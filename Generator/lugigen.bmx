Strict

Import "type.bmx"
Import "field.bmx"
Import "method.bmx"
Import "metadata.bmx"

Public

Function GenerateGlueCode(url:Object = Null)
	Assert url Else "GenerateGlueCode: No output URL provided"
	
	' Clear existing types (in case the code is generating multiple wrappers)
	ClearExposedTypes()
	
	Local output:TStream = OpenStream(url, False, True)
	Assert output Else "GenerateGlueCode: Unable to create writeable output stream"
	
	SeedRndWithDateTime
	
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
	output.WriteLine("Function "+GLOBAL_PREFIX+INIT_PREFIX+"pre_"+pretag+"(lua_vm:Byte Ptr, register_field(off%, typ%, name$, class@ Ptr), register_method(luafn:Int(state:Byte Ptr), name$, class@ Ptr))")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PreInitBlock()+"~n")
	Next
	
	output.WriteLine("End Function")
	output.WriteString("New LuGIInitFunction.PreInit("+GLOBAL_PREFIX+INIT_PREFIX+"pre_"+pretag+")~n~n~n")
	
	output.WriteLine("Function "+GLOBAL_PREFIX+INIT_PREFIX+"post_"+posttag+"(lua_vm:Byte Ptr, constructor:Int(state:Byte Ptr))")
	
	' Any initialization code following the exposure (e.g., setting global constructors and such)
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PostInitBlock())
	Next
	
	output.WriteLine("End Function")
	output.WriteString("New LuGIInitFunction.PostInit("+GLOBAL_PREFIX+INIT_PREFIX+"post_"+posttag+")~n~n~n")
	
	output.WriteString("~nPublic~n~n")
	
	output.Flush
	output.Close
End Function
