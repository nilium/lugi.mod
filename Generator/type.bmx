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
		Local ot:LExposedType = LExposedType(other)
		If other = Null Then
			Return 1
		ElseIf TTypeId(other) Then
			Return typeid.Compare(other)
		ElseIf other And Not ot Then
			Throw "Invalid type to compare LExposedType against"
		Else
			Return typeid.Compare(ot.typeid)
		EndIf
	End Method
	
	' Returns a list of exposed types to iterate over
	Function EnumTypes:TList()
		Return _exposedTypes.Copy()
	End Function
	
	' Initializes an LExposedType with information for the type passed
	' The object returned may not be the one you originally sent the initialize message to
	' If there is already an instance for this type, that instance will be returned instead
	' of proceeding with intialization
	Method InitWithTypeID:LExposedType(tid:TTypeId)
		If tid = ObjectTypeId Or tid = StringTypeId Or tid._class = ArrayTypeId._class Then
			Return Null
		EndIf
		
		Local lnk:TLink = _exposedTypes.FindLink(tid)
		If lnk Then
			Return LExposedType(lnk.Value())
		EndIf
		
		typeid = tid
		
		exposed = typeid.Metadata(LUGI_META_EXPOSE).ToInt()>0
		
		If Not exposed Then
			Return Null
		EndIf
		
		DebugLog "Building type info for "+tid.Name()
		
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
		If Not exposed Then
			Return Null
		EndIf
		
		Local outs$=""
		
		DebugLog "Pre-init block for "+typeid.Name()
		
		If Not (static And noclass) Then
			DebugLog "Instance methods"
			For Local m:LExposedMethod = EachIn methods
				Local initString$ = m.PreInitBlock()
				If initString Then
					outs :+ "~t" + initString + "~n"
				EndIf
			Next
		EndIf
		
		If Not noclass Then
			DebugLog "Fields"
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
		If Not exposed Then
			Return Null
		EndIf
		
		DebugLog "Post-init block for "+typeid.Name()
		
		If Not (nonew Or (static And noclass)) Then
			Return 	"~tlua_pushlightuserdata lua_vm, Byte Ptr(TTypeId.ForName(~q" + typeid.Name() + "~q)._class)~n" + ..
					"~tlua_pushcclosure lua_vm, p_lugi_new_object, 1~n"
		ElseIf static And Not noclass Then
			Return	"~t"
		EndIf
	End Method
	
	Method MethodImplementations:String()
		If Not exposed Then
			Return Null
		EndIf
		
		DebugLog "Method implementations for "+typeid.Name()
		
		Local m:LExposedMethod
		Local outs$
		Local instMethod% = Not (static And noclass)
		Local block$
		
		For m = EachIn methods
			Local parentImp:LExposedMethod = __methodExposedInParent(m)
			
			If m <> parentImp Then
				Continue
			EndIf
			
			block = m.Implementation(instMethod)
			If block Then
				outs :+ "~n"+block+"~n"
			EndIf
		Next
		
		Return outs
	End Method
	
	Method __methodExposedInParent:LExposedMethod(current:LExposedMethod)
		Local name$ = current.methodid.Name().ToLower()
		
		Local typ:TTypeId = typeid.SuperType()
		Local parentImp:LExposedMethod
		While typ
			Local exp:LExposedType = ForType(typ)
			
			If exp Then
				For Local m:LExposedMethod = EachIn exp.methods
					If m.methodid.Name().ToLower() = name Then
						parentImp = m
						Exit	' go onto next parent
					EndIf
				Next
			EndIf
			
			typ = typ.SuperType()
		Wend
		
		If Not parentImp Then
			Return current
		Else
			Return parentImp
		EndIf
	End Method
	
	Method __initMethods()
		methods = New TList
		
		If Not exposed Then
			Return
		EndIf
		
		' Build list of exposed methods
		For Local m:TMethod = EachIn typeid.Methods()
			If "sendmessage^compare^new^delete^tostring".Find(m.Name().ToLower()) = -1 Then
				methods.AddLast(New LExposedMethod.InitWithMethod(m, typeid))
			EndIf
		Next
	End Method
	
	Method __initFields()
		fields = New TList
		
		If Not exposed Or hidefields Then
			Return
		EndIf
		
		' Build list of exposed fields
		For Local f:TField = EachIn typeid.Fields()
			fields.AddLast(New LExposedField.InitWithField(f, typeid))
		Next
	End Method
End Type

' Builds up the information for all types
Function BuildTypeInfo()
	DebugLog "Building type info..."
	TTypeID._Update()
	_buildTypeInfo(ObjectTypeId.DerivedTypes())
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
