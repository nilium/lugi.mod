/*
	Copyright (c) 2009 Noel R. Cower

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
*/

#ifndef LUAINT_H_FE1DA7L0
#define LUAINT_H_FE1DA7L0


#include <stdint.h>
#include <brl.mod/blitz.mod/blitz.h>
#include <pub.mod/lua.mod/lua-5.1.4/src/lua.h>
#include <pub.mod/lua.mod/lua-5.1.4/src/lauxlib.h>


#ifdef __cplusplus
extern "C" {
#endif



/********************************************* Macros *********************************************/

/**
	\brief Macro to enable table-like behavior for BMax objects in Lua.
	
	\note This code is experimental and should be used with caution.
	
	When set to 1, LuGI will allow the indexing of BMax objects regardless of whether or not a key
	is defined for the BMax type.  For example, given the type Account:
	\code
	Type Account
		Field Name:String
		Field ID:Int
	End Type
	\endcode
	
	Setting the value of key \c Age in Lua will create a new key-value pair in the object's
	environment table.
	\code
	local account = NewAccount()
	
	account.Age = 32 -- adds a key-value pair {"Age", 32} to the object's environment table
	\endcode
**/
#define BMX_TABLE_SUPPORT 0



/******************************************* Field types ******************************************/

/** \brief Field type enum

	Used to identify the type of a BMax object's field from within Lua.
	
	\sa lugi_register_field()
**/
typedef enum {
	BYTEFIELD = 0x001,
	SHORTFIELD = 0x002,
	INTFIELD = 0x004,
	FLOATFIELD = 0x008,
	LONGFIELD = 0x010,
	DOUBLEFIELD = 0x020,
	STRINGFIELD = 0x040,
	OBJECTFIELD = 0x080,
	ARRAYFIELD = 0x100,
	
	/** Optional flag to specify that an integer field should be returned as a Lua boolean. **/
	BOOLFIELDOPT = 0x8000
} fieldtype_e;



/*************************************** Initialization API ***************************************/

/** \brief Initialize LuGI for use with the specified Lua state.
	
		Creates the generic BBObject metatable and metamethods, the object cache table, and the type
		method tables.
	
	\precondition The Lua state must be created.
	\postcondition The Lua state will be prepared to work with BlitzMax objects based on which
		methods and fields are exposed via register_method and register_field.
	
	\author Noel Cower
	\date 2009-06-28
	\param state The Lua state to initialize the glue code in.
**/
void p_lugi_init(lua_State *state);

/** \brief undocumented function
	
		longer description
	
	\author Noel Cower __MyCompanyName__
	\date 2009-06-28
	\param  description of parameter
	\param  description of parameter
	\return description of return value
	\sa
**/
void p_lugi_register_method(lua_CFunction fn, BBString *name, BBClass *clas);

/** \brief undocumented function
	
		longer description
	
	\author Noel Cower __MyCompanyName__
	\date 2009-06-28
	\param  description of parameter
	\param  description of parameter
	\return description of return value
	\sa
**/
void p_lugi_register_field(int offset, int type, BBString *name, BBClass *clas);



/*********************************** Object handling/conversion ***********************************/

/** \brief Pushes a BlitzMax object onto the top of the Lua stack.
	
		Valid types of objects are strings, any instance of a custom type subclassing Object, and
		one-dimensional arrays of any type.
		
	\pre The state must have been initialized with lugi_init().
	
	\author Noel Cower
	\date 2009-06-28
	\param state The Lua state whose stack you want to push the object onto.
	\param obj The object to push onto the stack.
	\sa lua_pushbmaxarray(lua_State *state, BBArray *arr)
**/
void lua_pushbmaxobject(lua_State *state, BBObject *obj);

/** \brief undocumented function
	
		longer description
	
	\pre The state must have been initialized with lugi_init().
	
	\author Noel Cower __MyCompanyName__
	\date 2009-06-28
	\param state The Lua state whose stack you want to retrieve the Object from.
	\param index The index in the stack where the Object is located.
	\return The object located at the index on the stack.  May be a String, Object, array, or Null
	depending on the type of value at the index.
	\sa lua_tobmaxarray(lua_State *state, int index), \ref arrays
**/
BBObject *lua_tobmaxobject(lua_State *state, int index);



/************************************ Array handling/conversion ***********************************/

/** \brief undocumented function
	
		longer description
	
	\author Noel Cower __MyCompanyName__
	\date 2009-06-28
	\param  description of parameter
	\param  description of parameter
	\return description of return value
	\sa
**/
void lua_pushbmaxarray(lua_State* state, BBArray* arr);

/** \brief undocumented function
	
		longer description
	
	\author Noel Cower __MyCompanyName__
	\date 2009-06-28
	\param  description of parameter
	\param  description of parameter
	\return description of return value
	\sa
**/
BBArray *lua_tobmaxarray(lua_State *state, int index);



/*************************************** Object constructor ***************************************/

/** \brief Generic constructor function to allow instanciation of BMax types in Lua. **/
int p_lugi_new_object(lua_State *state);



#ifdef __cplusplus
}
#endif


#endif /* end of include guard: LUAINT_H_FE1DA7L0 */
