Strict

Import "type.bmx"
Import "field.bmx"
Import "method.bmx"

Public

Function GenerateGlueCode(url:Object = Null)	
	Assert url Else "GenerateGlueCode: No output URL provided"
	
	Local output:TStream = OpenStream(url, False, True)
	Assert output Else "GenerateGlueCode: Unable to create writeable output stream"
	
	BuildTypeInfo()
	
	output.WriteLine("Private")
	output.WriteLine("")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		Local outs$ = t.MethodImplementations()
		If outs Then
			output.WriteLine("'---- "+t.name)
			output.WriteString(outs)
		EndIf
	Next
	
	output.WriteLine("Public")
	
	output.WriteLine("Function InitLuGI(lua_vm:Byte Ptr)")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PreInitBlock())
	Next
	
	output.WriteLine("~tp_lugi_init(lua_vm)")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PostInitBlock())
	Next
	
	output.WriteLine("End Function")
	
	output.Flush
	output.Close
End Function
