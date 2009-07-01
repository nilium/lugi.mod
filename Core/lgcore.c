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

#include "lgcore.h"

#include <stdio.h> // needed for asprintf


#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************************************
**********/// Internal metamethods

static int lugi_le_object(lua_State *state);			// lua: a <= b
static int lugi_lt_object(lua_State *state);			// lua: a < b
static int lugi_eq_object(lua_State *state);			// lua: a == b
static int lugi_index_object(lua_State *state);			// lua: object[key], object.key, object:key
static int lugi_newindex_object(lua_State *state);		// lua: object[key] = value, object.key = value
#ifndef THREADED                                                
static int lugi_gc_object(lua_State *state);			// BBObject userdata collected by Lua
#endif



/**************************************************************************************************
**********/// Debugging macros

//////// Error string utilities

#define _LINESTR(S) #S
#define LINESTR(S) _LINESTR(S)
#define FILELOC __FILE__ ":" LINESTR(__LINE__)

#define ERRORSTR(S) FILELOC S



/**************************************************************************************************
**********/// Type access

//////// Field access

// union for accessing field values
typedef union {
	unsigned char byte_value;
	unsigned short short_value;
	int int_value;
	float float_value;
	long long long_value;
	double double_value;
	BBString *string_value;
	BBObject *obj_value;
	BBArray *arr_value;
} bmx_field;



/**************************************************************************************************
**********/// Data structures for the internal registry of methods/fields for classes


/// Type of registry item (field or instance method)
typedef enum {
	FIELDTYPE,
	METHODTYPE
} infotype_e;

/// Structure containing information pertaining to the glue code - like reflection for the blind
typedef struct s_glueinfo glueinfo_t;

struct s_glueinfo
{
	infotype_e type;
	union {
		lua_CFunction fn; // method
		struct {
			unsigned short field_type;		// the type of field
			unsigned short field_offset;	// the offset of the field in the object (char*)obj+field_offset
		};
	};
	char *name;	// method name
	BBClass *clas; // NULL if static
	glueinfo_t *next; // next function
};

// list of functions to create in Lua
static glueinfo_t *lugi_g_infohead = NULL;



/**************************************************************************************************
**********/// Light userdata keys into the Lua registry

// the values of these variables is completely irrelevant, as the addresses of the variables are used
// as keys into the registry table
static void *bmx_metatable = NULL;
static void *bmx_object_key = NULL;
static void *bmx_table_key = NULL;



/**************************************************************************************************
**********/// Initialization API (considered private during runtime)

// Initializes the glue code for use in the lua_State provided
// In short, the function will initialize the generic BMax object metatable (handles comparisons,
// garbage collection, and field/method access [provided by a hidden virtual method table in the
// registry]) and creates the virtual method table for all classes as well as making global methods
// accessible.
void p_lugi_init(lua_State *state) {
	bmx_metatable = &bmx_metatable;
	bmx_object_key = &bmx_object_key;
	
	int top = lua_gettop(state);
	
	// create object cache
	lua_pushlightuserdata(state, bmx_object_key);
	lua_newtable(state);
	
	lua_createtable(state, 0, 1); // create metatable for object cache
	lua_pushlstring(state, "v", 1); // references to values are weak so that as long as the object persists in Lua, the object can be accessed from this cache
	lua_setfield(state, -2, "__mode");
	lua_setmetatable(state, -2);
	
	lua_settable(state, LUA_REGISTRYINDEX);
	
	// put closures in the VMTs for classes
	glueinfo_t *info = lugi_g_infohead;
	while (info) {
		if ( info->clas == NULL ) {
			// no class = global function
			lua_pushcclosure(state, info->fn, 0);
			lua_setfield(state, LUA_GLOBALSINDEX, info->name);
		}
		else
		{
			// retrieve the virtual method table for class
			lua_pushlightuserdata(state, info->clas);
			lua_gettable(state, LUA_REGISTRYINDEX);
			
			if ( lua_type(state, -1) != LUA_TTABLE ) {
				// if the VMT doesn't exist, create it
				lua_pop(state, 1);
				
				lua_newtable(state);
				lua_pushlightuserdata(state, info->clas);
				// push another reference to the new table to the top
				lua_pushvalue(state, -2);
				// afterward, a reference to the table remains after inserting it into the registry
				lua_settable(state, LUA_REGISTRYINDEX);
			}
			
			switch(info->type) {
				case METHODTYPE:
				lua_pushcclosure(state, info->fn, 0);
				break;
				
				case FIELDTYPE:
				lua_pushinteger(state, ((int)info->field_offset & 0xFF) | ((int)info->field_type << 16));
				break;
				
				default:
				luaL_error(state, ERRORSTR("@p_lugi_init: Unrecognized glue info type"));
				return;
				break;
			}
			
			lua_setfield(state, -2, info->name); // insert the closure into the class table
			lua_pop(state, 1); // pop the class table
		}
		
		info = info->next;
	}
	
	// create the generic object metatable
	lua_pushlightuserdata(state, bmx_metatable);
#ifdef THREADED
	lua_createtable(state, 0, 5);
#else
	lua_createtable(state, 0, 6);	// extra record for __gc
	
	// t.__gc
	lua_pushcclosure(state, lugi_gc_object, 0);
	lua_setfield(state, -2, "__gc");
#endif
	
	lua_pushcclosure(state, lugi_le_object, 0); // a < b
	lua_setfield(state, -2, "__le");
	lua_pushcclosure(state, lugi_lt_object, 0); // a <= b
	lua_setfield(state, -2, "__lt");
	lua_pushcclosure(state, lugi_eq_object, 0); // a == b
	lua_setfield(state, -2, "__lq");
	// index
	lua_pushcclosure(state, lugi_index_object, 0); // a[b]
	lua_setfield(state, -2, "__index");
	// newindex
	lua_pushcclosure(state, lugi_newindex_object, 0); // a[b] = c
	lua_setfield(state, -2, "__newindex");
	
#ifndef THREADED
	// t.__gc
	lua_pushcclosure(state, lugi_gc_object, 0);
	lua_setfield(state, -2, "__gc");
#endif
	
	lua_settable(state, LUA_REGISTRYINDEX);
	
	
	// reset the top of the stack so it's clean (shouldn't be necessary, but just in case)
	lua_settop(state, top);
}


/*******
	This code could be replaced with something similar to contexts at a later date if you wanted
	to have more than one kind of thing you wanted to initialize, I suppose
*******/

// Internal
// Places a glue function with the name specified into the global list of glue info
// If it's a method of a class, pass a pointer to the object's BBClass to insert it into
// the class's Lua method table
// If you're registering a global function (e.g., just a regular glue function), pass a NULL pointer
// for the class
void p_lugi_register_method(lua_CFunction fn, BBString *name, BBClass *clas) {
	glueinfo_t *info = (glueinfo_t*)bbMemAlloc(sizeof(glueinfo_t));
	info->type = METHODTYPE;
	info->fn = fn;
	info->clas = clas;
	info->name = bbStringToCString(name);
	info->next = lugi_g_infohead;
	lugi_g_infohead = info;
}


// Internal
// Places the offset of an instance variable for a class into the global list of glue info
// An exception is thrown if you pass a NULL pointer for the class
void p_lugi_register_field(int offset, int type, BBString *name, BBClass *clas) {
	if ( clas == NULL )
		bbExThrowCString(ERRORSTR("@lugi_register_field: Cannot pass Null BBClass for field."));
	
	glueinfo_t *info = (glueinfo_t*)bbMemAlloc(sizeof(glueinfo_t));
	info->type = FIELDTYPE;
	info->field_offset = (unsigned short)offset;
	info->field_type = (unsigned short)type;
	info->clas = clas;
	info->name = bbStringToCString(name);
	info->next = lugi_g_infohead;
	lugi_g_infohead = info;
}


// Deallocates the internal list of closures
void lugi_free_info() {
	glueinfo_t *info;
	while (lugi_g_infohead != NULL)
	{
		info = lugi_g_infohead;
		lugi_g_infohead = info->next;
		bbMemFree(info->name);	// note: Use bbMemFree because ToCString uses bbMemAlloc, and its blocks have a small header attached to them
		bbMemFree(info);
	}
}



/**************************************************************************************************
**********/// Object handling/conversion

// precondition: lua interface must be initialized for this state, or you will experience a crash.
void lua_pushbmaxobject(lua_State *state, BBObject *obj) {
	int top = lua_gettop(state);
	
	if (obj == &bbNullObject) {
		lua_pushnil(state);
		return;
	}
	else if ((BBString*)obj == &bbEmptyString)
	{
		lua_pushlstring(state, "", 0);
		return;
	}
	
	if (obj->clas == &bbStringClass) {
		BBString *str = (BBString*)obj;
		char *buf = bbStringToCString(str);
		lua_pushlstring(state, buf, str->length);
		bbMemFree(buf);
		return;
	}
	else if (obj->clas == &bbArrayClass) {
		lua_pushbmaxarray(state, (BBArray*)obj);
		return;
	}
	
	// check object cache for existing object
	lua_pushlightuserdata(state, bmx_object_key);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	// cache[object]
	lua_pushlightuserdata(state, obj);
	lua_gettable(state, -2);
	
#ifdef THREADED
	// in the case of threaded builds, we use light userdata instead of regular userdata, since
	// the garbage collection metatable is not necessary
	// if cache[object] == object
	if (lua_type(state, -1) == LUA_TLIGHTUSERDATA) {
		lua_remove(state, -2);
		// object exists, remove table and return
		return;
	}
	
	lua_pop(state, 1);
	
	lua_pushlightuserdata(state, obj);
#else
	// if cache[object] == object
	if (lua_type(state, -1) == LUA_TUSERDATA) {
		lua_remove(state, -2);
		// object exists, remove table and return
		return;
	}
	
	lua_pop(state, 1);
	
	// increment reference count for the object since BMax's default GC relies on ref counting
	BBRETAIN(obj);
	
	// store object
	BBObject **data = (BBObject**)lua_newuserdata(state, sizeof(BBObject*));
	*data = obj;
#endif
	
#if BMX_TABLE_SUPPORT == 1
	// this is part of an unsupported feature to enable table-like behavior for BMax objects
	// decided not to include it (by default, anyway) because, frankly, it doesn't fit in when
	// you might be trying to set a field for some reason and you'll never receive an error
	// that the field doesn't exist because you're just setting some value for a table
	lua_newtable(state);
	if ( lua_setfenv(state, -2) == 0 )
	{
		lua_settop(state, top); // clean up stack..
		luaL_error(state, ERRORSTR("@lua_pushbmaxobject: Failed to set environment table for BMax object."));
		return;
	}
#endif
	
	// set the metatable
	lua_pushlightuserdata(state, bmx_metatable);
	lua_gettable(state, LUA_REGISTRYINDEX);
	lua_setmetatable(state, -2);
	
	// cache the object and remove the cache table
	lua_pushlightuserdata(state, obj);
	lua_pushvalue(state, -2);
	lua_settable(state, -4);
	lua_remove(state, -2);
	
	// done
}


// Converts a Lua value to a BMax object (Userdata -> BBObject*, String -> BBString*, Table -> lua_tobmaxarray(..))
BBObject *lua_tobmaxobject(lua_State *state, int index) {
	// check the type at the index
	switch(lua_type(state, index))
	{
		case LUA_TNIL:				// return null for nil
			return &bbNullObject;
		
		case LUA_TSTRING:			// if it's a string, return a string (since a BBString is an object)
		{
			int length;
			const char *sbuf = lua_tolstring(state, -1, &length);
			return bbStringFromBytes(sbuf, length);
		}
		
		case LUA_TTABLE: // pass off to lua_tobmaxarray
			return (BBObject*)lua_tobmaxarray(state, index);
		
		// get an object out of the userdata
#ifdef THREADED
		case LUA_TLIGHTUSERDATA:
			return (BBObject*)lua_touserdata(state, index);
#else
		case LUA_TUSERDATA:
			return *(BBObject**)lua_touserdata(state, index);
#endif
		
		// and the best default ever: an error
		default:
			luaL_error(state, ERRORSTR("@lua_tobmaxobject: Value at index (%d) is not an object."), index);
			return &bbNullObject;
	}
}



/**************************************************************************************************
**********/// Array handling/conversion

void lua_pushbmaxarray(lua_State *state, BBArray *arr) {
	if ( arr == &bbEmptyArray )
	{
		// create an empty table for an empty array..
		lua_createtable(state, 0, 0);
		return;
	}
	
	if ( arr->dims != 1 )
	{
		luaL_error(state, ERRORSTR("@lua_pushbmaxarray: UNSUPPORTED: Attempt to push multidimension array"));
		return;
	}
	
	int table_index = 1;
	int array_len = arr->scales[0];
	
	lua_createtable(state, array_len, 0);
	
	// macro to simplify the code for pushing the contents of arrays
	#define BUILDTABLE(PUSHER, TYPE)                                   \
		{                                                              \
			TYPE* p = (TYPE*)BBARRAYDATA(arr, arr->dims);              \
			for(;table_index <= array_len; ++table_index) {            \
				printf(#TYPE "  " #PUSHER "\n");\
				lua_pushnumber(state, table_index);                   \
				PUSHER(state, *p++);                                   \
				lua_settable(state, -3);                               \
			}                                                          \
		}
	// END MACRO
	
	switch(arr->type[0])
	{
		case 'b':
		BUILDTABLE(lua_pushinteger, unsigned char)
		break;
		
		case 's':
		BUILDTABLE(lua_pushinteger, unsigned short)
		break;
		
		case 'i':
		BUILDTABLE(lua_pushinteger, int)
		break;
		
		case 'l':
		BUILDTABLE(lua_pushinteger, long long)
		break;
		
		case 'f':
		BUILDTABLE(lua_pushnumber, float)
		break;
		
		case 'd':
		BUILDTABLE(lua_pushnumber, double)
		break;
		
		case '$':
		case ':':
		BUILDTABLE(lua_pushbmaxobject, BBObject*)
		break;
		
		case '[':
		BUILDTABLE(lua_pushbmaxarray, BBArray*)
		break;
	}
	
	#undef BUILDTABLE
}


// Converts a Lua table to a BMax array - the type of the array is determined by the first value in the table (at index 1)
BBArray *lua_tobmaxarray(lua_State *state, int index) {
	switch (lua_type(state, index))
	{
		case LUA_TNIL:
			return &bbEmptyArray;
		
		case LUA_TTABLE: // code below
		break;
		
		default:
			if (lua_type(state, index) != LUA_TTABLE) {
				luaL_error(state, ERRORSTR("@lua_tobmaxarray: Value at index (%d) is not a table."), index);
				return &bbEmptyArray;
			}
	}
	
	// make the index absolute, since we're now dealing with more than one value on the stack
	if (index < 0 && index > LUA_REGISTRYINDEX)
		index = lua_gettop(state)+(index+1);
	
	// the index into the array
	int table_index;
	BBArray *arr = NULL;
	size_t len = lua_objlen(state, index);
	
	if ( len == 0 )
		return &bbEmptyArray;
	
	// get the first item of the table
	lua_pushinteger(state, 1);
	lua_gettable(state, index);
	
	// starting at index 2 when iterating
	table_index = 2;
	
	// determine what type of array to create based on the first value in the table (at index 0)
	switch (lua_type(state, -1))
	{
		case LUA_TNUMBER:		// array of doubles
		{
			double *p;
			
			arr = bbArrayNew1D("d", len);
			p = (double*)BBARRAYDATA(arr, arr->dims);
			
			*p++ = lua_tonumber(state, -1);
			lua_pop(state, 1);
			
			for (;table_index<=len;++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p++ = lua_tonumber(state, -1);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		case LUA_TBOOLEAN:		// array of integers
		{
			int *p;
			
			arr = bbArrayNew1D("i", len);
			p = (int*)BBARRAYDATA(arr, arr->dims);
			
			*p++ = lua_toboolean(state, -1);
			lua_pop(state, 1);
			
			for (;table_index<=len;++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p++ = lua_toboolean(state, -1);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		case LUA_TSTRING:		// array of strings
		{
			BBString **p;
			
			arr = bbArrayNew1D("$", len);
			p = (BBString**)BBARRAYDATA(arr, arr->dims);
			
			*p = bbStringFromCString(lua_tostring(state, -1));
			BBRETAIN(*p++);
			lua_pop(state, 1);
			
			for (;table_index<=len;++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p = bbStringFromCString(lua_tostring(state, -1));
				BBRETAIN(*p++);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		case LUA_TTABLE:		// array of arrays (these arrays do not have to be the same type)
#ifdef THREADED
		case LUA_TUSERDATA:		// array of objects
#else
		case LUA_TLIGHTUSERDATA:
#endif
		{
			BBObject **p;
			
			arr = bbArrayNew1D(":Object", len);
			p = (BBObject**)BBARRAYDATA(arr, arr->dims);
			
			*p = lua_tobmaxobject(state, -1);
			BBRETAIN(*p++);
			lua_pop(state, 1);
			
			for (;table_index<=len;++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p = lua_tobmaxobject(state, -1);
				BBRETAIN(*p++);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		// if you're wondering where LUA_TNIL is, just know that it is impossible for this to be the first index in a table - if nil were the first value, it would be length 0
		
		default:
			luaL_error(state, ERRORSTR("@lua_tobmaxarray: Arrays of type %s are not unsupported"), lua_typename(state, lua_type(state, -1)));
			return &bbEmptyArray;
	}
	
	return arr;
}



/**************************************************************************************************
**********/// Internal metamethod implementations

//////// BBObject metatable

// a <= b
static int lugi_le_object(lua_State *state) {
	BBObject *a = lua_tobmaxobject(state, 1);
	BBObject *b = lua_tobmaxobject(state, 2);
	lua_pushboolean(state, (a->clas->Compare(a, b) <= 0));
	return 1;
}


// a < b
static int lugi_lt_object(lua_State *state) {
	BBObject *a = lua_tobmaxobject(state, 1);
	BBObject *b = lua_tobmaxobject(state, 2);
	lua_pushboolean(state, (a->clas->Compare(a, b) < 0));
	return 1;
}


// a == b
static int lugi_eq_object(lua_State *state) {
	BBObject *a = lua_tobmaxobject(state, 1);
	BBObject *b = lua_tobmaxobject(state, 2);
	lua_pushboolean(state, (a->clas->Compare(a, b) == 0));
	return 1;
}


// table[key]  OR  table.key
static int lugi_index_object(lua_State *state) {
	if (lua_type(state, 2) == LUA_TSTRING) {
		
		BBObject *obj = lua_tobmaxobject(state, 1);
		BBClass *clas = obj->clas;
		
		while (clas != NULL) {
		
			// get class VMT
			lua_pushlightuserdata(state, clas);
			lua_gettable(state, LUA_REGISTRYINDEX);
		
			if ( lua_type(state, -1) == LUA_TTABLE ) // there's a lookup table for the class..
			{
			
				lua_pushvalue(state, 2);
				lua_rawget(state, -2);
						
				int type = lua_type(state, -1);
			
				if (type == LUA_TNUMBER) {		// getting the value of a field
					int fieldopt = lua_tointeger(state, -1);
					unsigned short *opts = &fieldopt;
					unsigned short offset = opts[0];
					bmx_field *field = (bmx_field*)((char*)obj+offset);
				
					switch (opts[1])
					{
						case BYTEFIELD:
						lua_pushinteger(state, field->byte_value);
						break;
					
						case SHORTFIELD:
						lua_pushinteger(state, field->short_value);
						break;
					
						case INTFIELD:
						lua_pushinteger(state, field->int_value);
						break;
					
						case FLOATFIELD:
						lua_pushnumber(state, field->float_value);
						break;
					
						case DOUBLEFIELD:
						lua_pushnumber(state, field->double_value);
						break;
					
						case LONGFIELD:
						lua_pushinteger(state, field->long_value);
						break;
					
						case STRINGFIELD:
						{
							char *strbuf = bbStringToCString(field->string_value);
							lua_pushlstring(state, strbuf, field->string_value->length);
							bbMemFree(strbuf);
						}
						break;
					
						case ARRAYFIELD:
						lua_pushbmaxarray(state, field->arr_value);
						break;
					
						case OBJECTFIELD:
						lua_pushbmaxobject(state, field->obj_value);
						break;
					
						default:
						return luaL_error(state, ERRORSTR("@lugi_index_object: Unrecognized field type (%d)."), opts[1]);
						break;
					}
					return 1;
				}
				else if (type ==  LUA_TFUNCTION)		// method
				{
					return 1;
				} // 
			} // table found
			
			// iterate to the superclass
			clas = clas->super;
		} // while class
	} // key is string
	
#if BMX_TABLE_SUPPORT != 1
	// notify that there was an error accessing a field or method that does not exist
	return luaL_error(state, ERRORSTR("@lugi_index_object: Index for object is not a valid field or method."));
#else
	lua_settop(state, 2);
	
	// prior table behavior for BMax objects
	lua_getfenv(state, 1);
	lua_insert(state, 2);
	lua_gettable(state, 2); // this is funky
	
	return 1;
#endif
}


// table[key] = value OR table.key = value
static int lugi_newindex_object(lua_State *state) {
	if (lua_type(state, 2) == LUA_TSTRING) {
		
		BBObject *obj = lua_tobmaxobject(state, 1);
		BBClass *clas = obj->clas;
		
		while (clas != NULL)
		{
			// get class VMT
			lua_pushlightuserdata(state, obj->clas);
			lua_gettable(state, LUA_REGISTRYINDEX);
		
			if ( lua_type(state, -1) == LUA_TTABLE ) // there's a lookup table for the class..
			{
				lua_pushvalue(state, 2);
				lua_rawget(state, -2);
				
				if (lua_type(state, -1) == LUA_TNUMBER) {		// getting the value of a field
					clas = NULL;
					
					int fieldopt = lua_tointeger(state, -1);
					unsigned short *opts = &fieldopt;
					bmx_field *field = (bmx_field*)((char*)obj+opts[0]);
					
					switch (opts[1]) // set the value of a field
					{
						case BYTEFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN)
							field->byte_value = (unsigned char)lua_toboolean(state, 3);
						else
							field->byte_value = (unsigned char)lua_tointeger(state, 3);
						break;
					
						case SHORTFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN)
							field->short_value = (unsigned short)lua_toboolean(state, 3);
						else
							field->short_value = (unsigned short)lua_tointeger(state, 3);
						break;
					
						case INTFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN)
							field->int_value = lua_toboolean(state, 3);
						else
							field->int_value = lua_tointeger(state, 3);
						break;
					
						case FLOATFIELD:
						field->float_value = (float)lua_tonumber(state, 3);
						break;
					
						case DOUBLEFIELD:
						field->double_value = lua_tonumber(state, 3);
						break;
					
						case LONGFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN)
							field->long_value = lua_toboolean(state, 3);
						else
							field->long_value = lua_tointeger(state, 3);
						break;
					
						case STRINGFIELD:
						{
							int length = 0;
							char *strbuf = lua_tolstring(state, 3, &length);
						
							BBRELEASE(field->string_value);
							
							field->string_value = bbStringFromBytes(strbuf,length);
							BBRETAIN(field->string_value);
						}
						break;
					
						case ARRAYFIELD:
						if ( field->arr_value != NULL )
							BBRELEASE(field->arr_value);
						
						field->arr_value = lua_tobmaxarray(state, 3);
						BBRETAIN(field->arr_value);
						break;
					
						case OBJECTFIELD:
						#ifndef THREADED
						if ( field->obj_value != NULL )
							BBRELEASE(field->obj_value);
						#endif
					
						field->obj_value = lua_tobmaxobject(state, 3);
						BBRETAIN(field->obj_value);
						break;
					
						default:
						return luaL_error(state, ERRORSTR("@lugi_newindex_object: Unrecognized field type (%d)."), opts[1]);
						break;
					} // set value based on type
					return 0;
				} // field found
			} // VMT found
			
			clas = clas->super; // iterate to a superclass if nothing is found in the class
		} // while class
	} // key is string
	
#if BMX_TABLE_SUPPORT != 0
	return luaL_error(state, ERRORSTR("@lugi_newindex_object: Index for object is not a valid field or method."));
#else
	// prior table behavior for BMax objects - disabled by default
	lua_settop(state, 3);
	
	lua_getfenv(state, 1);
	lua_insert(state, 2);
	lua_settable(state, 2);
	
	return 0;
#endif
}


// Userdata collection routine - only used in unthreaded builds where refcount has to be decremented
// This is only compiled if not threaded, of course
#ifndef THREADED
static int lugi_gc_object(lua_State *state) {
	BBObject **obj = (BBObject**)lua_touserdata(state,1);
	BBRELEASE(*obj);
	*obj = NULL;
}
#endif



/**************************************************************************************************
**********/// Object constructor

int p_lugi_new_object(lua_State *state) {
	BBClass *clas = lua_touserdata(state, lua_upvalueindex(1));
	lua_pushbmaxobject(state, bbObjectNew(clas));
	return 1;
}

#ifdef __cplusplus
}
#endif
