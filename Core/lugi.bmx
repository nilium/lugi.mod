Strict

?Threaded
Import Brl.Threads
?
Import Pub.Lua
Import "lgcore.c"

Public

Const LUGI_BYTEFIELD:Int = $0001
Const LUGI_SHORTFIELD:Int = $0002
Const LUGI_INTFIELD:Int = $0004
Const LUGI_FLOATFIELD:Int = $0008
Const LUGI_LONGFIELD:Int = $0010
Const LUGI_DOUBLEFIELD:Int = $0020
Const LUGI_STRINGFIELD:Int = $0040
Const LUGI_OBJECTFIELD:Int = $0080
Const LUGI_ARRAYFIELD:Int = $0100
Const LUGI_BOOLFIELDOPT:Int = $8000

Type LuGIInitFunction
	Field _cb:Byte Ptr
	
	Method PreInit(cb(vm:Byte Ptr, rfield(off%, typ%, name$, clas@ Ptr), rmethod(fn:Int(state@ Ptr), name$, clas@ Ptr)))
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
	End Method
	
	Method PostInit(cb(state:Byte Ptr, constructor:Int(state:Byte Ptr)))
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
	End Method
	
	Method Pre(vm:Byte Ptr, rfield(off%, typ%, name$, clas@ Ptr), rmethod(fn:Int(state@ Ptr), name$, clas@ Ptr))
		Local cb(vm:Byte Ptr, rfield(off%, typ%, name$, clas@ Ptr), rmethod(fn:Int(state@ Ptr), name$, clas@ Ptr)) = _cb
		cb(vm, rfield, rmethod)
	End Method
	
	Method Post(vm:Byte Ptr, constructor:Int(state:Byte Ptr))
		Local cb(vm:Byte Ptr, constructor:Int(state:Byte Ptr)) = _cb
		cb(vm, constructor)
	End Method
End Type

Private

?Threaded
Global p_lugi_initlock:TMutex = TMutex.Create()
?
Global p_lugi_preinit_cb:TList
Global p_lugi_postinit_cb:TList

' Executes init functions that are to occur prior to creation of any BMax objects, VMTs, etc.
Function PreInitLuGI(vm@ Ptr)
	If Not p_lugi_preinit_cb Then
		Return
	EndIf
	For Local cb:LuGIInitFunction = EachIn p_lugi_preinit_cb
		cb.Pre(vm, p_lugi_register_field, p_lugi_register_method)
	Next
End Function

' Executes init functions that are to occur following the creation of BMax VMTs
Function PostInitLuGI(vm@ Ptr)
	If Not p_lugi_postinit_cb Then
		Return
	EndIf
	For Local cb:LuGIInitFunction = EachIn p_lugi_postinit_cb
		cb.Post(vm, p_lugi_new_object)
	Next
End Function

Extern "C"
	Function p_lugi_register_method(fn:Int(state@ Ptr), name$, clas@ Ptr=Null)
	Function p_lugi_register_field(off%, typ%, name$, clas@ Ptr)
	
	Function p_lugi_init(state@ Ptr)
	
	' Constructor object - push with BBClass for type as upvalue
	Function p_lugi_new_object:Int(state@ Ptr)
End Extern

Public

Extern "C"
	Function lua_pushbmaxobject(state@ Ptr, obj:Object)
	Function lua_tobmaxobject:Object(state@ Ptr, index:Int)
	
	Function lua_pushbmaxarray(state@ Ptr, arr:Object)
	Function lua_tobmaxarray:Object(state@ Ptr, index:Int)
End Extern

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
