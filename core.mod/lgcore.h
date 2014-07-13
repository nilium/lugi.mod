/*
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
*/

#ifndef LUAINT_H_FE1DA7L0
#define LUAINT_H_FE1DA7L0

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <brl.mod/blitz.mod/blitz.h>
#include <pub.mod/lua.mod/lua-5.1.4/src/lua.h>
#include <pub.mod/lua.mod/lua-5.1.4/src/lauxlib.h>


/********************************************* Macros *********************************************/

/**
	\brief Macro to enable table-like behavior for BMax objects in Lua.

	\note The code enabled by this macro is experimental and \em untested, it should be used with
	extreme caution.

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
#ifndef BMX_TABLE_SUPPORT
#	define BMX_TABLE_SUPPORT 0
#endif



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

/** \brief Registers a method closure with the LuGI core.

		This method will add to LuGI's registry a method to be associated with a virtual method
		table or alternatively a global function.  To register a global function, the BBClass passed
		to p_lugi_register_method should be NULL.

	\author Noel Cower
	\date 2009-07-16
	\param fn A pointer to the function to create a closure for.
	\param name The name of the method.  E.g., \c "DoSomething".
	\param clas The BBClass to associate the method with.  If NULL, it is assumed that the method is
	static and will be pushed as a global variable.
	\sa p_lugi_register_field
**/
void p_lugi_register_method(lua_CFunction fn, BBString *name, BBClass *clas);

/** \brief Registers a field name and offset with the LuGI core.

		This method will add to LuGI's registry a field offset and name to be associated with
		object's of a specific BBClass (and its subclasses).

	\author Noel Cower
	\date 2009-07-16
	\param offset The offset of the field in an instance of the type specified by the clas
	parameter.
	\param type The field type, one of fieldtype_e.  BOOLFIELDOPT can be or'd with any integer
	field type to specify that the field should return a boolean value.
	\param name The name of the field.  E.g., \c "RootNode".
	\param clas The BBClass to associate the method with.  If NULL, an exception will be thrown.
	\sa p_lugi_register_method
**/
void p_lugi_register_field(int offset, int type, BBString *name, BBClass *clas);



/*********************************** Object handling/conversion ***********************************/

/** \brief Pushes a BlitzMax object onto the top of the Lua stack.

		Valid types of objects are strings, any instance of a custom type subclassing Object, and
		one-dimensional arrays of any type.

	\pre The state must have been initialized with lugi_init().

	\author Noel Cower
	\date 2009-06-28
	\param state The Lua state whose stack you want to push the Object onto.
	\param obj The object to push onto the stack.
	\sa lua_pushbmaxarray(lua_State *state, BBArray *arr)
**/
void lua_pushbmaxobject(lua_State *state, BBObject *obj);

/** \brief Gets a BMax object off of the Lua stack.

		Valid types of objects to get off of the stack are strings, tables (converted to arrays),
		and any instance of a custom type subclassing Object.

	\pre The state must have been initialized with lugi_init().
	\pre The index specified must be a valid Lua stack index.
	\pre The value at the index specified on the stack must be a valid BlitzMax object, a string, a
	table, or nil.

	\author Noel Cower
	\date 2009-06-28
	\param state The Lua state whose stack you want to retrieve the Object from.
	\param index The index in the stack where the Object is located.
	\return The object located at the index on the stack.  May be a String, Object, array, or Null
	depending on the type of value at the index.
	\sa lua_tobmaxarray(lua_State *state, int index), \ref arrays
**/
BBObject *lua_tobmaxobject(lua_State *state, int index);

/** \brief Returns whether or not the value at the index is a BlitzMax object.

	This currently works by checking the metatable of the object.  If the object has the generic
	metatable for BlitzMax objects, then it is an object.

	\pre The state must have been initialized for use with LuGI.
	\pre The index specified must be a valid Lua stack index.

	\author Noel Cower
	\date 2009-07-20
	\param state The Lua state whose stack you want to retrieve the Object from.
	\param index The index in the stack where the value is located.
	\return 1 if the value is a BlitzMax object, otherwise 0.
	\sa \ref arrays
**/
int32_t lua_isbmaxobject(lua_State *state, int index);



/************************************ Array handling/conversion ***********************************/

/** \brief Pushes a BlitzMax array onto the stack as a table.

	\author Noel Cower
	\date 2009-06-28
	\param state The Lua state whose stack you want to push the array onto.
	\param  description of parameter
	\return description of return value
	\sa \ref arrays
**/
void lua_pushbmaxarray(lua_State* state, BBArray* arr);

/** \brief Converts a Lua table to a BlitzMax array.

	\pre The state must have been initialized for use with LuGI.
	\pre The index specified must be a valid Lua stack index.
	\pre The value at the index specified on the stack must be a table.

	\author Noel Cower
	\date 2009-06-28
	\param state The Lua state whose stack you want to retrieve the array from.
	\param index The index in the stack where the table is located.
	\return A BBArray - the array may be empty or Null, depending on the contents of the table.
	\sa \ref arrays
**/
BBArray *lua_tobmaxarray(lua_State *state, int index);



/*************************************** Object constructor ***************************************/

/** \brief Generic constructor function to allow instanciation of BMax types in Lua. **/
int p_lugi_new_object(lua_State *state);



#ifdef __cplusplus
}
#endif


#endif /* end of include guard: LUAINT_H_FE1DA7L0 */
