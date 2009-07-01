SuperStrict

Import "field.bmx"
Import "method.bmx"
Import "metadata.bmx"

Import Brl.LinkedList

Private

Global _exposedTypes:TList = New TList

Public

Type LExposedType
	Field exposed%
	Field static%
	Field noclass%
	Field hidefields%
	Field nonew%
	
	Field typeid:TTypeId
	Field methods:TList			' TList<LExposedMethod>
	Field fields:TList			' TList<LExposedField>
	Field constructor$
	Field name$
	
	' Returns an LExposedType for a typeid - recommended to use this instead of creating a new LExposedType and initializing it
	Function ForType:LExposedType(typeid:TTypeId)
		If typeid = ObjectTypeId Or typeid = StringTypeId Or typeid._class = ArrayTypeId._class Then
			Return Null
		EndIf
		
		Local link:TLink = _exposedTypes.FindLink(typeid)
		If link Then
			Return LExposedType(link.Value())
		EndIf
		Return New LExposedType.InitWithTypeID(typeid)
	End Function
	
	' Compares LExposedTypes based on the TTypeId associated with each LExposedType
	Method Compare:Int(other:Object)
		Return typeid.Compare(LExposedType(other).typeid)
	End Method
	
	' Returns a list of exposed types to iterate over
	Function EnumTypes:TList()
		Return _exposedTypes.Copy()
	End Function
	
	' Initializes an LExposedType with information for the type passed
	' Will throw an exception if an LExposedType for the typeid has already been created and initialized
	Method InitWithTypeID:LExposedType(tid:TTypeId)
		typeid = tid
		
		If _exposedTypes.Contains(_exposedTypes) Then
			Throw "Attempt to initialize duplicate LExposedType for type "+tid.Name()
		EndIf
		
		exposed = typeid.Metadata(LUGI_META_EXPOSE).ToInt()>0
		static = typeid.Metadata(LUGI_META_STATIC).ToInt()>0
		noclass = typeid.Metadata(LUGI_META_NOCLASS).ToInt()>0
		hidefields = typeid.Metadata(LUGI_META_HIDEFIELDS).ToInt()>0
		nonew = typeid.Metadata(LUGI_META_DISABLECONSTRUCTOR).ToInt()>0
		
		name = typeid.Metadata(LUGI_META_RENAME).Trim()
		If Not name Then
			name = typeid.Name()
		EndIf
		
		If Not (nonew Or (static and noclass)) Then
			constructor = typeid.Metadata(LUGI_META_CONSTRUCTOR).Trim()
			If Not constructor Then
				' Force New[A-Z_].* as default
				constructor = "New"+name[0..1].ToUpper()+name[1..]
			EndIf
		EndIf
		
		__initMethods
		__initFields
		
		_exposedTypes.AddLast(Self)
		
		Return Self
	End Method
	
	Method PreInitBlock:String()
		Local outs$=""
		
		If Not (static And noclass) Then
			For Local m:LExposedMethod = EachIn methods
				Local initString$ = m.PreInitBlock()
				If initString Then
					outs :+ "~t" + initString + "~n"
				EndIf
			Next
		EndIf
		
		If Not noclass Then
			For Local f:LExposedField = EachIn fields
				Local initString$ = f.PreInitBlock()
				If initString Then
					outs :+ "~t" + initString + "~n"
				EndIf
			Next
		EndIf
		
		Return outs
	End Method
	
	Method PostInitBlock:String()
		If Not (nonew Or (static And noclass)) Then
			Return 	"~tlua_pushlightuserdata lua_vm, Byte Ptr(TTypeId.ForName(~q" + typeid.Name() + "~q)._class)" + ..
					"~tlua_pushcclosure lua_vm, p_lugi_new_object, 1"
		ElseIf static And Not noclass Then
			Return	"~t"
		EndIf
	End Method
	
	Method MethodImplementations:String()
		Local m:LExposedMethod
		Local outs$
		Local instMethod% = Not (static And noclass)
		Local block$
		
		For m = EachIn methods
			block = m.Implementation(instMethod)
			If block Then
				outs :+ "~n~n"+block
			EndIf
		Next
		
		Return outs
	End Method
	
	Method _exposedInParent:Int(m:TMethod)
		Local name$ = m.Name().ToLower()
		
		Local typ:TTypeId = typeid.SuperType()
		While typ
			
		Wend
	End Method
	
	Method __initMethods()
		methods = New TList
		
		If Not exposed Then
			Return
		EndIf
		
		' Build list of exposed methods
		For Local m:TMethod = EachIn typeid.Methods()
			
		Next
	End Method
	
	Method __initFields()
		fields = New TList
		
		If Not exposed Or hidefields Then
			Return
		EndIf
		
		For Local f:TField = EachIn typeid.Fields()
			
		Next
	End Method
End Type

' Builds up the information for all types
Function BuildTypeInfo()
End Function

Private

' Recursively creates the information for types
Function _buildTypeInfo(types:TList)
	' Iterate over types and load data for them
	For Local tid:TTypeId = EachIn types
		New LExposedType.InitWithTypeID(tid)
		_buildTypeInfo(tid.DerivedTypes())
	Next
End Function
