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

'SuperStrict

'Import "field.bmx"
'Import "method.bmx"
'Import "metadata.bmx"

'Import Brl.LinkedList

Private

Global _exposedTypes:TList = New TList

Const CONSTRUCTOR_NAME$="constructor"

Function ClearExposedTypes()
	_exposedTypes.Clear()
End Function

Type LExposedType
	Field exposed%
	Field static%
	Field noclass%
	Field hidefields%
	Field nonew%
	
	Field typeid:TTypeID
	Field methods:TList			' TList<LExposedMethod>
	Field fields:TList			' TList<LExposedField>
	Field constructor$
	Field name$
	
	Field globalname$			' Only set if noclass and static are set
	
	' Returns an LExposedType for a typeid - recommended to use this instead of creating a new LExposedType and initializing it
	Function ForType:LExposedType(typeid:TTypeID)
		If typeid = ObjectTypeId Or typeid = StringTypeId Or typeid._class = ArrayTypeId._class Then
			Return Null
		EndIf
		
		Local link:TLink = _exposedTypes.FindLink(typeid)
		If link Then
			Return LExposedType(link.Value())
		EndIf
		Return New LExposedType.InitWithTypeID(typeid)
	End Function
	
	' Compares LExposedTypes based on the TTypeID associated with each LExposedType
	Method Compare:Int(other:Object)
		Local ot:LExposedType = LExposedType(other)
		If other = Null Then
			Return 1
		ElseIf TTypeID(other) Then
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
	Method InitWithTypeID:LExposedType(tid:TTypeID)
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
		
		If Not (nonew Or static) Then
			constructor = typeid.Metadata(LUGI_META_CONSTRUCTOR).Trim()
			If Not constructor Then
				' Force New[A-Z_].* as default
				constructor = "New"+name[0..1].ToUpper()+name[1..]
			EndIf
		EndIf
		
		If static And noclass Then
			globalname = LUGI_GLOBAL_PREFIX+LUGI_NOCLASS_VAR_PREFIX+typeid.Name()+"_"+GenerateUniqueTag(6)
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
				outs :+ "~t' Register instance method "+typeid.Name()+"#"+m.methodid.Name()+"~n"
				m = __methodExposedInParent(m)
				Local initString$ = m.PreInitBlock(typeid)
				If initString Then
					outs :+ "~t" + initString + "~n"
				EndIf
			Next
		Else
			DebugLog "Instance methods"
			For Local m:LExposedMethod = EachIn methods
				outs :+ "~t' Register global method "+typeid.Name()+"#"+m.methodid.Name()+"~n"
				'm = __methodExposedInParent(m)
				Local initString$ = m.PreInitBlock(Null)
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
		
		If Not static And Not noclass Then
			Return 	"~t' Register constructor for "+typeid.Name()+"~n" + ..
				"~tlua_pushlightuserdata( lua_vm, Byte Ptr(TTypeID.ForName(~q" + typeid.Name() + "~q)._class) )~n" + ..
				"~tlua_pushcclosure( lua_vm, "+CONSTRUCTOR_NAME+", 1 )~n" + ..
				"~tlua_setfield( lua_vm, LUA_GLOBALSINDEX, ~q"+constructor+"~q )~n"
		ElseIf static And Not noclass Then
			Return "~t' Create object for static type "+typeid.Name()+"~n" + ..
				"~tlua_pushbmaxobject( lua_vm, New "+typeid.Name()+" )~n" + ..
				"~tlua_setfield( lua_vm, LUA_GLOBALSINDEX, ~q"+name+"~q )~n"
		EndIf
	End Method
	
	Method Implementation:String()
		' The method to be implemented
		Local m:LExposedMethod
		' Output string
		Local outs$
		' Whether or not the method is an instance method
		Local instMethod% = Not (static And noclass)
		
		' Return nothing if the type has no method implementations
		If Not exposed Then
			Return Null
		EndIf
		
		' Implementation of a noclass type requires a global instance of the type
		If static And noclass Then
			outs :+ "Global "+globalname+":"+typeid.Name()+" = New "+typeid.Name()+"~n"
		EndIf
		
		For m = EachIn methods
			' Stores the implementation of the method (under some conditions, it's
			' possible for a method to not return an implementation)
			Local block$
			
			' Check to see if this method is implemented in a super type, and if it is,
			' skip reimplementing it- the supertype's implementation will be used by the
			' VMT for this type as well (for speed reasons)
			Local parentImp:LExposedMethod = __methodExposedInParent(m)
			
			If m <> parentImp And instMethod Then
				Continue
			EndIf
			
			' Return the method implementation
			block = m.Implementation(instMethod, globalname)
			If block Then
				outs :+ "~n"+block+"~n"
			EndIf
		Next
		
		Return outs
	End Method
	
	Method __methodExposedInParent:LExposedMethod(current:LExposedMethod)
		Local name$ = current.methodid.Name().ToLower()
		
		Local typ:TTypeID = typeid.SuperType()
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
				Local meth:LExposedMethod = New LExposedMethod.InitWithMethod(m, typeid)
				If meth Then
					methods.AddLast(meth)
				EndIf
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
			Local exf:LExposedField = New LExposedField.InitWithField(f, typeid)
			If exf Then
				fields.AddLast(exf)
			EndIf
		Next
	End Method
End Type

' Caches relevant information for all types
Function BuildTypeInfo()
	DebugLog "Building type info..."
	TTypeID._Update()
	_buildTypeInfo(ObjectTypeId.DerivedTypes())
End Function

Private

' Recursively creates the information for types
Function _buildTypeInfo(types:TList)
	' Iterate over types and load data for them
	For Local tid:TTypeID = EachIn types
		New LExposedType.InitWithTypeID(tid)
		_buildTypeInfo(tid.DerivedTypes())
	Next
End Function
