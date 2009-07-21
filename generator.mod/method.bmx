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

'Strict

'Import brl.Map

'Import "metadata.bmx"		' Metadata names
'Import "utility.bmx"		' FormatString

Private

Const REGISTER_METHOD_NAME$ = "register_method"

Global g_CanReturnBoolTypes:TList = New TList
g_CanReturnBoolTypes.AddLast(IntTypeID) ' Trying to do this in order of which would be most likely..
g_CanReturnBoolTypes.AddLast(ShortTypeID)
g_CanReturnBoolTypes.AddLast(ByteTypeID)
g_CanReturnBoolTypes.AddLast(LongTypeID)

Type LExposedMethod
	Field methodid:TMethod
	Field owner:TTypeID
	Field name$
	Field hidden%
	Field booltype%
	Field tag$
	
	Method New()
		tag = "_"+GenerateUniqueTag(6)
	End Method
	
	' Initializes an exposed method - returns Null if the method is hidden/not exposed
	Method InitWithMethod:LExposedMethod(meth:TMethod, owner:TTypeID)
		methodid = meth
		Self.owner = owner
		
		' get metadata
		hidden = methodid.Metadata(LUGI_META_HIDDEN).ToInt()>0
		
		If hidden Then
			Return Null
		EndIf
		
		name = methodid.Metadata(LUGI_META_RENAME).Trim()
		If Not name Then
			name = methodid.Name()
		EndIf
		
		Return Self
	End Method
	
	Method PreInitBlock:String(fortype:TTypeID=Null)
		If hidden Then
			Return Null
		EndIf
		
		If fortype Then
			' Registers the method with LuGI's core
			Return REGISTER_METHOD_NAME+"( " + ImplementationName() + ", ~q" + name + "~q, Byte Ptr(TTypeID.ForName(~q" + fortype.Name() + "~q)._class) )"
		Else
			' If no type is provided, then it's assumed that the method is global (e.g., a noclass method)
			Return REGISTER_METHOD_NAME+"( " + ImplementationName() + ", ~q" + name + "~q, Null )"
		EndIf
	End Method
	
	Method PostInitBlock:String()
		' noop - methods have no post-init block
	End Method
	
	Method ImplementationName$()
		' This should never actually be necessary, since hidden methods shouldn't even be
		' created
		If hidden Then
			Return Null
		EndIf
		
		Return LUGI_GLOBAL_PREFIX+LUGI_METH_PREFIX+owner.Name()+"_"+name+tag
	End Method
	
	' Returns the implementation of the method
	Method Implementation:String( instanceMethod:Int, objectName$=Null )
		If hidden Then
			Return Null
		EndIf
		
		Local head$ = "Function "+ImplementationName()+":Int( lua_vm:Byte Ptr )"
		
		' Whether or not to add the NoDebug flag to method glue functions
		If LUGI_METH_NODEBUG Then
			head :+ " NoDebug"
		EndIf
		
		head :+ "~n"
		
		' Tail is same for all methods: return 1 and end function
		Local tail$ = "~nEnd Function~n"
		
		' output string always starts with the head
		Local outs$ = head
		
		' Choose the implementation type depending on whether this is an instance method or
		' a noclass method
		If instanceMethod Then
			outs :+ __instanceImp()
		Else
			' Pass the name of the global instance for the noclass method
			outs :+ __noclassImp(objectName)
		EndIf
		
		Local retType:TTypeID = methodid.TypeID()
		Local pushfn$ = LuaPushFunctionForTypeID(retType)
		' select the push function needed depending on whether or not the method is to return
		' a bool and if it matches a type that can be used to return a bool
		If retType And pushfn Then
			If methodid.Metadata(LUGI_META_BOOL).ToInt()>0 And g_CanReturnBoolTypes.Contains(retType) Then
				outs :+ "~tlua_pushboolean"
			Else
				outs :+ "~t"+pushfn
			EndIf
		
			' method parameters
			outs :+ "( lua_vm, obj."+methodid.Name()+"("+(", ".Join(__argNames()))+") )~n~n~tReturn 1~n"
		Else
			' Method doesn't have a return type (or a supported return type), returns nothing
			outs :+ "~tobj."+methodid.Name()+"("+(", ".Join(__argNames()))+")~n~n~tReturn 0~n"
		EndIf
		
		outs :+ tail
		
		Return outs
	End Method
	
	' Returns a formatted block of code for the arguments (with tabs/line-endings, as opposed to
	' something like PreInitBlock which returns just the line)
	' stackstart designates the first stack index of the argument
	Method __argsBlock:String(stackstart%)
		Const argLocalFormat$ = "Local \1:\2 = \3"
		Const argGetterFormat$ = "\1 = \2"
		
		Local args:TTypeID[] = methodid.ArgTypes()
		
		' If there are no arguments, we have nothing to show
		If args.Length = 0 Then
			Return Null
		EndIf
		
		Local argNames$[] = __argNames()
		Local argGetters$[args.Length]
		Local argLocals$[args.Length]
		
		For Local idx:Int = 0 Until args.Length
			' generate the Local declaration
			argLocals[idx] = FormatString(argLocalFormat, [argNames[idx], args[idx].Name(), DefaultValueForTypeID(args[idx])])
			' and the getter - many '~t's follow
			Select args[idx]
				Case IntTypeId, ShortTypeId, ByteTypeId, LongTypeId
					argGetters[idx] = ..
						"Select lua_type(lua_vm, "+(stackstart+idx)+")" + ..
							"~n~t~t~t~tCase LUA_TBOOLEAN~n" + ..
								"~t~t~t~t~t" + argNames[idx] + " = " + LuaToFunctionForTypeID(args[idx], "lua_vm", stackstart+idx, True) + ..
							"~n~t~t~t~tDefault~n" + ..		 ' attempt to convert whatever is on the stack to an integer
								"~t~t~t~t~t" + argNames[idx] + " = " + LuaToFunctionForTypeID(args[idx], "lua_vm", stackstart+idx, False) + ..
						"~n~t~t~tEnd Select"
				
				Default
					argGetters[idx] = argNames[idx] + " = " + LuaToFunctionForTypeID(args[idx], "lua_vm", stackstart+idx, False)
			End Select	
		Next
		
		Local outs$ = "~t"+("~n~t".Join(argLocals))+"~n"
		
		If args.Length > 1 Then
			' multiple arguments get a select/case block
			
			outs :+ "~t' Get arguments off stack~n"
			outs :+ "~tSelect lua_gettop(lua_vm)~n"
			If stackstart > 1 Then
				outs :+ "~t~tCase "+(stackstart-1)+"~n~t~t~t' no arguments provided~n~n"
			EndIf
			For Local idx:Int = 0 Until args.Length
				If idx < args.Length - 1 Then
					outs :+ "~t~tCase "+(idx+stackstart)+"~n"
				Else
					outs :+ "~t~tDefault~n"
				EndIf
				outs :+ "~t~t~t"+("~n~t~t~t".Join(argGetters[..idx+1]))+"~n"
				If idx < args.Length-1 Then
					outs :+ "~n"
				EndIf
			Next
			outs :+ "~tEnd Select ' Arguments retrieved from stack~n"
		ElseIf args.Length = 1
			' If there's only one argument, simplify it to use a single conditional
			outs :+ ..
				"~tIf "+(stackstart)+" <= lua_gettop(lua_vm) Then~n" + ..
					"~t~t"+argGetters[0].Replace("~n~t", "~n") + ..
				"~n~tEndIf~n"
		EndIf
		
		Return outs
	End Method
	
	Method __argNames$[]()
		Local argNames:String[methodid.ArgTypes().Length]
		For Local idx:Int = 0 Until argNames.Length
			argNames[idx] = "_arg_"+(idx+1)
		Next
		Return argNames
	End Method
	
	' Implementation for instances/static types
	Method __instanceImp:String()
		Local outs$
		outs :+ "~tLocal obj:"+owner.Name()+" = "+LuaToFunctionForTypeID(owner, "lua_vm", 1, False)+"~n"
		outs :+ __argsBlock(2)+"~n"
		
		Return outs
	End Method
	
	' Implementation for types without classes (has an upvalue object associated with the closure)
	Method __noclassImp:String(objname$)
		Local outs$
		
		outs :+ "~tLocal obj:"+owner.Name()+" = "+objname+"~n"
		outs :+ __argsBlock(1)+"~n"
		
		Return outs
	End Method
End Type
