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

Module LuGI.Core

ModuleInfo "Name: LuGI Core"
ModuleInfo "Description: Core API provided by LuGI to interact with BMax objects via Lua"
ModuleInfo "Author: Noel Cower"
ModuleInfo "License: MIT"
ModuleInfo "URL: <a href=~qhttp://github.com/nilium/lugi.mod/~q>http://github.com/nilium/lugi.mod/</a>"

?Threaded
Import Brl.Threads
?
Import Brl.LinkedList
Import Pub.Lua
Import "lgcore.cpp"

Public

' LuGI field types - used by LuGI to determine how to access fields at runtime
' The only reason these are public is because the generated code uses them, otherwise I'd hide
' them away too.  But then if I hide them and use number constants in the generated code, we'll
' get the chance that changing these later will break existing glue code, and doing that is bad.
Const LUGI_BYTEFIELD:Int = $0001
Const LUGI_SHORTFIELD:Int = $0002
Const LUGI_INTFIELD:Int = $0004
Const LUGI_FLOATFIELD:Int = $0008
Const LUGI_LONGFIELD:Int = $0010
Const LUGI_DOUBLEFIELD:Int = $0020
Const LUGI_STRINGFIELD:Int = $0040
Const LUGI_OBJECTFIELD:Int = $0080
Const LUGI_ARRAYFIELD:Int = $0100
' Optional flag to specify that an integer field (Int, Short, Byte, or Long) should be treated as a
' boolean value when passed to Lua
Const LUGI_BOOLFIELDOPT:Int = $8000

Type LuGIInitFunction
	
	' Registers the callback provided as a pre-init stage routine
	Method PreInit:LuGIInitFunction(cb(vm:Byte Ptr, rfield(off%, typ%, name$, clas@ Ptr), rmethod(fn:Int(state@ Ptr), name$, clas@ Ptr)))
		_cb = cb
		?Threaded
		p_lugi_initlock.Lock()
		?
		If p_lugi_preinit_cb = Null Then
			p_lugi_preinit_cb = New TList
		EndIf
		p_lugi_preinit_cb.AddLast(Self)
		?Threaded
		p_lugi_initlock.Unlock()
		?
		Return Self
	End Method
	
	' Registers the callback provided as a post-init stage routine
	Method PostInit:LuGIInitFunction(cb(state:Byte Ptr, constructor:Int(state:Byte Ptr)))
		_cb = cb
		?Threaded
		p_lugi_initlock.Lock()
		?
		If p_lugi_postinit_cb = Null Then
			p_lugi_postinit_cb = New TList
		EndIf
		p_lugi_postinit_cb.AddLast(Self)
		?Threaded
		p_lugi_initlock.Unlock()
		?
		Return Self
	End Method
	
	' #region PRIVATE
	
	Field _cb:Byte Ptr
	
	Method Pre(vm:Byte Ptr, rfield(off%, typ%, name$, clas@ Ptr), rmethod(fn:Int(state@ Ptr), name$, clas@ Ptr))
		Local cb(vm:Byte Ptr, rfield(off%, typ%, name$, clas@ Ptr), rmethod(fn:Int(state@ Ptr), name$, clas@ Ptr)) = _cb
		cb(vm, rfield, rmethod)
	End Method
	
	Method Post(vm:Byte Ptr, constructor:Int(state:Byte Ptr))
		Local cb(vm:Byte Ptr, constructor:Int(state:Byte Ptr)) = _cb
		cb(vm, constructor)
	End Method
	
	' #endregion
	
End Type

Private

' There is really no good way to do this in a thread-safe manner, so the best I can do is to make
' all access to the initialization lists a critical section
?Threaded
Global p_lugi_initlock:TMutex = TMutex.Create()
?
Global p_lugi_preinit_cb:TList
Global p_lugi_postinit_cb:TList

' Executes init functions that are to occur prior to creation of any BMax objects, VMTs, etc.
' Doesn't lock the lists since it's only supposed to be called after the lists have been locked
Function PreInitLuGI(vm@ Ptr)
	If Not p_lugi_preinit_cb Then
		Return
	EndIf
	For Local cb:LuGIInitFunction = EachIn p_lugi_preinit_cb
		cb.Pre(vm, p_lugi_register_field, p_lugi_register_method)
	Next
End Function

' Executes init functions that are to occur following the creation of BMax VMTs, metatables, cache, etc.
' Doesn't lock the lists since it's only supposed to be called after the lists have been locked
Function PostInitLuGI(vm@ Ptr)
	If Not p_lugi_postinit_cb Then
		Return
	EndIf
	For Local cb:LuGIInitFunction = EachIn p_lugi_postinit_cb
		cb.Post(vm, p_lugi_new_object)
	Next
End Function

' Private/internal code that shouldn't be directly accessed (only provided)
Extern "C"
	Function p_lugi_register_method(fn:Int(state@ Ptr), name$, clas@ Ptr=Null)
	Function p_lugi_register_field(off%, typ%, name$, clas@ Ptr)
	
	Function p_lugi_init(state@ Ptr)
	
	' Constructor object - push with BBClass for type as upvalue
	Function p_lugi_new_object:Int(state@ Ptr)
End Extern

Public

Extern "C"
	' Pushing/getting BBObjects (BBObjects, BBStrings, and BBArrays to tables/tables to BBArrays respectively)
	Function lua_pushbmaxobject(state@ Ptr, obj:Object)
	Function lua_tobmaxobject:Object(state@ Ptr, index:Int)
	
	' Pushing/getting BBArrays (BBArrays to tables/tables to BBArrays respectively)
	' @arr has to be treated as an Object in BMax since passing an Int[] as an Object[] doesn't work
	Function lua_pushbmaxarray(state@ Ptr, arr:Object)
	Function lua_tobmaxarray:Object(state@ Ptr, index:Int)
End Extern

' Initializes the Lua state for use with BMax objects via LuGI
Function InitLuGI(vm:Byte Ptr)
	?Threaded
	p_lugi_initlock.Lock()
	?
	PreInitLuGI(vm)
	p_lugi_init(vm)
	PostInitLuGI(vm)
	?Threaded
	p_lugi_initlock.Unlock()
	?
End Function
