Strict

Const BYTEFIELD:Int = 0
Const SHORTFIELD:Int = 1
Const INTFIELD:Int = 2
Const FLOATFIELD:Int = 3
Const LONGFIELD:Int = 4
Const DOUBLEFIELD:Int = 5
Const STRINGFIELD:Int = 6
Const OBJECTFIELD:Int = 7
Const ARRAYFIELD:Int = 8

Extern "C"
	Function p_lugi_register_method(fn:Int(state@ Ptr), name$, clas@ Ptr=Null)
	Function p_lugi_register_field(off%, typ%, name$, clas@ Ptr)
	
	Function p_lugi_init(state@ Ptr)
	
	Function lua_pushbmaxobject(state@ Ptr, obj:Object)
	Function lua_tobmaxobject:Object(state@ Ptr, index:Int)
	
	Function lua_pushbmaxarray(state@ Ptr, arr:Object)
	Function lua_tobmaxarray:Object(state@ Ptr, index:Int)
	
	' Constructor object - push with BBClass for type as upvalue
	Function p_lugi_new_object:Int(state@ Ptr)
End Extern
