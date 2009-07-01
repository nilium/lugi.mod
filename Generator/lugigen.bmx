Strict

Import "type.bmx"
Import "field.bmx"
Import "method.bmx"

Private

Function SeedRndWithDate()
	Local datetime$
	Local date$ = CurrentDate()
	Local time$ = CurrentTime()
	datetime = date[..2].Trim()+date[date.length-2..]+time[0..2]+time[3..5]+time[6..8]
	SeedRnd datetime.ToInt()
End Function

Public

Function GenerateGlueCode(url:Object = Null)
	Assert url Else "GenerateGlueCode: No output URL provided"
	
	Local output:TStream = OpenStream(url, False, True)
	Assert output Else "GenerateGlueCode: Unable to create writeable output stream"
	
	SeedRndWithDate
	
	BuildTypeInfo()
	
	output.WriteString("Private~n~n")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		Local outs$ = t.MethodImplementations()
		If outs Then
			output.WriteLine("'---- "+t.name)
			output.WriteString(outs)
		EndIf
	Next
	
	output.WriteString("Public~n~n")
	
	output.WriteLine("Function InitLuGI(lua_vm:Byte Ptr)")
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PreInitBlock()+"~n")
	Next
	
	output.WriteLine("~n~t' Initialize lua_vm with registered classes/methods/fields~n")
	output.WriteLine("~tp_lugi_init(lua_vm)~n~n")
	
	
	For Local t:LExposedType = EachIn LExposedType.EnumTypes()
		output.WriteString(t.PostInitBlock())
	Next
	
	output.WriteString("End Function~n")
	
	output.Flush
	output.Close
End Function
