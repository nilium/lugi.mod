Strict

Import "metadata.bmx"

Private

Const METH_PREFIX$="_lugi_glue"	' An _ will always follow this

Public

Type LExposedMethod
	Field methodid:TMethod
	Field owner:TTypeId
	Field name$
	Field hidden%
	Field tag$
	
	Method New()
		Const TAGCHARS$="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"
		
		' generate a short tag to follow the method name, just to reduce the chance of conflicts
		Local tagbuff:Short[7]
		tagbuff[0] = 95 ' underscore
		For Local i:Int = 1 Until tagbuff.Length
			tagbuff[i] = TAGCHARS[Rand(0,62)]
		Next
		tag = String.FromShorts(tagbuff, tagbuff.Length)
	End Method
	
	Method InitWithMethod:LExposedMethod(meth:TMethod, owner:TTypeId)
		methodid = meth
		Self.owner = owner
		
		' get metadata
		hidden = methodid.Metadata(LUGI_META_HIDDEN).ToInt()>0
		
		name = methodid.Metadata(LUGI_META_RENAME).Trim()
		If Not name Then
			name = methodid.Name()
		EndIf
		
		Return Self
	End Method
	
	Method PreInitBlock:String()
	End Method
	
	Method PostInitBlock:String()
	End Method
	
	Method ImplementationName$()
		Return METH_PREFIX+"_"+owner.Name()+"_"+name+tag
	End Method
	
	Method Implementation:String( instanceMethod:Int )
		If hidden Then
			Return Null
		EndIf
		
		Local head$ = "Function "+ImplementationName()+"(lua_vm:Byte Ptr)"
		Local tail$ = "End Function"
		
		If instanceMethod Then
			Return head+"~n"+__instanceImp()+"~n"+tail
		Else
			Return head+"~n"+__noclassImp()+"~n"+tail
		EndIf
	End Method
	
	' Implementation for instances/static types
	Method __instanceImp:String()
	End Method
	
	' Implementation for types without classes
	Method __noclassImp:String()
		
	End Method
End Type
