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

#include "lgcore.h"


#ifdef __cplusplus
extern "C" {
#endif

/*************************************  Internal metamethods  *************************************/

static int lugi_le_object(lua_State *state);			/* lua: a <= b */
static int lugi_lt_object(lua_State *state);			/* lua: a < b */
static int lugi_eq_object(lua_State *state);			/* lua: a == b */
static int lugi_index_object(lua_State *state);			/* lua: object[key], object.key, object:key */
static int lugi_newindex_object(lua_State *state);		/* lua: object[key] = value, object.key = value */
static int lugi_gc_object(lua_State *state);			/* BBObject userdata collected by Lua */

static int lugi_concat_array(lua_State *state);			/* array..array */
static int lugi_length_array(lua_State *state);			/* #array */
static int lugi_index_array(lua_State *state);			/* lua: array[index] */
static int lugi_newindex_array(lua_State *state);			/* lua: array[index] = value */



/*******************************  Internal BBObject implementation  *******************************/

static int lugi_sendmessage_object(lua_State *state);
static int lugi_compare_object(lua_State *state);
static int lugi_tostring_object(lua_State *state);



/*******************************  Internal BBArray implementation  ********************************/

static int lugi_sort_array(lua_State *state);
static int lugi_slice_array(lua_State *state);
static int lugi_type_array(lua_State *state);



/***************************************  Debugging macros  ***************************************/

/*********** Error string utilities */

#define _LINESTR(S) #S
#define LINESTR(S) _LINESTR(S)
#define FILELOC __FILE__ ":" LINESTR(__LINE__)
#define ERRORSTR(S) FILELOC S



/******************************************* Type access ******************************************/

/*********** Field access */

/* union for accessing field values */
typedef union {
	uint8_t byte_value;
	uint16_t short_value;
	int32_t int_value;
	float float_value;
	int64_t long_value;
	double double_value;
	BBString *string_value;
	BBObject *obj_value;
	BBArray *arr_value;
} bmx_field;



/************* Data structures for the internal registry of methods/fields for classes ************/

/** Type of registry item (field or instance method) **/
typedef enum {
	FIELDTYPE=0,
	METHODTYPE=1
} infotype_e;


/** Structure containing information pertaining to the glue code - like reflection for the blind **/
typedef struct s_glueinfo glueinfo_t;

typedef struct s_fieldinfo
{
	fieldtype_e type;
	ptrdiff_t offset;
} fieldinfo_t;

struct s_glueinfo {
	infotype_e type;
	union {
		lua_CFunction fn;			/* glue method */
		fieldinfo_t field;
	} data;
	char *name;						/* method name */
	BBClass *clas;					/* NULL if static */
	glueinfo_t *next;				/* next function */
};

/* list of functions to create in Lua */
static glueinfo_t *lugi_g_infohead = NULL;



/**************************** Light userdata keys into the Lua registry ***************************/

/* the values of these variables is completely irrelevant, as the addresses of the variables are used */
/* as keys into the registry table */
static void* const LUGI_METATABLE_KEY = (void* const)&LUGI_METATABLE_KEY;		/* key for the generic metatable */
static void* const LUGI_METATABLE_ARRAY_KEY = (void* const)&LUGI_METATABLE_ARRAY_KEY; /* key for the array metatable */
static void* const LUGI_OBJECT_CACHE_KEY = (void* const)&LUGI_OBJECT_CACHE_KEY;	/* key for the LuGI object cache */



/********************* Initialization API (considered private during runtime) *********************/

/*
  Initializes the glue code for use in the lua_State provided
  In short, the function will initialize the generic BMax object metatable (handles comparisons,
  garbage collection, and field/method access [provided by a hidden virtual method table in the
  registry]) and creates the virtual method table for all classes as well as making global methods
  accessible.
*/
void p_lugi_init(lua_State *state) {
	/* check for object cache */
	lua_pushlightuserdata(state, LUGI_OBJECT_CACHE_KEY);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	if ( lua_type(state, -1) != LUA_TTABLE ) {
		/* create object cache if it doesn't exist */
		lua_pushlightuserdata(state, LUGI_OBJECT_CACHE_KEY);
	
		lua_newtable(state);
		
		lua_createtable(state, 0, 1);		/* create metatable for object cache */
		lua_pushliteral(state, "v");		/* references to values are weak so that as long as the object */
							/* persists in Lua, the object can be accessed from this cache */
		lua_setfield(state, -2, "__mode");
		lua_setmetatable(state, -2);
		
		lua_settable(state, LUA_REGISTRYINDEX);
	}
	lua_pop(state, 1); /* pop result of checking for the cache */
	
	/* check for BBObject class VMT */
	lua_pushlightuserdata(state, &bbObjectClass);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	if ( lua_type(state, -1) != LUA_TTABLE ) {
		/* create BBObject VMT */
		lua_pushlightuserdata(state, &bbObjectClass);
		
		lua_createtable(state, 0, 3);
		
		lua_pushcclosure(state, lugi_sendmessage_object, 0);
		lua_setfield(state, -2, "SendMessage");
		
		lua_pushcclosure(state, lugi_tostring_object, 0);
		lua_setfield(state, -2, "ToString");
		
		lua_pushcclosure(state, lugi_compare_object, 0);
		lua_setfield(state, -2, "Compare");
		
		lua_settable(state, LUA_REGISTRYINDEX);
	}
	lua_pop(state, 1); /* pop result of checking for the  objectVMT */
	
	/* check for BBArray class VMT */
	lua_pushlightuserdata(state, &bbArrayClass);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	if ( lua_type(state, -1) != LUA_TTABLE ) {
		/* create BBArray VMT */
		lua_pushlightuserdata(state, &bbArrayClass);
		
		lua_createtable(state, 0, 3);
		
		lua_pushcclosure(state, lugi_sort_array, 0);
		lua_setfield(state, -2, "Sort");
		
		lua_pushcclosure(state, lugi_slice_array, 0);
		lua_setfield(state, -2, "Slice");
		
		lua_pushcclosure(state, lugi_type_array, 0);
		lua_setfield(state, -2, "Type");
		
		lua_settable(state, LUA_REGISTRYINDEX);
	}
	lua_pop(state, 1); /* pop result of checking for the array VMT */
	
	/* put closures in the VMTs for classes */
	/* these do not check to see if the methods have already been registered, so they'll be overwritten at times */
	glueinfo_t *info = lugi_g_infohead;
	while (info) {
		if ( info->clas == NULL ) {
			/* no class = global function */
			lua_pushcclosure(state, info->data.fn, 0);
			lua_setfield(state, LUA_GLOBALSINDEX, info->name);
		}
		else {
			/* retrieve the virtual method table for class */
			lua_pushlightuserdata(state, info->clas);
			lua_gettable(state, LUA_REGISTRYINDEX);
			
			if ( lua_type(state, -1) != LUA_TTABLE ) {
				/* if the VMT doesn't exist, create it */
				lua_pop(state, 1);
				
				lua_newtable(state);
				lua_pushlightuserdata(state, info->clas);
				/* push another reference to the new table to the top */
				lua_pushvalue(state, -2);
				/* afterward, a reference to the table remains after inserting it into the registry */
				lua_settable(state, LUA_REGISTRYINDEX);
			}
			
			switch(info->type) {
				case METHODTYPE:
				lua_pushcclosure(state, info->data.fn, 0);
				break;
				
				case FIELDTYPE:
                (*(fieldinfo_t*)lua_newuserdata(state, sizeof(fieldinfo_t))) = info->data.field;
				break;
				
				default:
				luaL_error(state, ERRORSTR("@p_lugi_init: Unrecognized glue info type"));
				return;
				break;
			}
			
			lua_setfield(state, -2, info->name); /* insert the closure into the class table */
			lua_pop(state, 1); /* pop the class table */
		}
		
		info = info->next;
	}
	
	
	/* check to see if the generic object metatable exists */
	lua_pushlightuserdata(state, LUGI_METATABLE_KEY);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	if ( lua_type(state, -1) != LUA_TTABLE ) {
		/* create the metatable if it doesn't exist */
		lua_pushlightuserdata(state, LUGI_METATABLE_KEY);
		
		lua_createtable(state, 0, 6); /* 6 metamethods for object instances.. */
		
		/* comparison */
		lua_pushcclosure(state, lugi_le_object, 0); /* a < b */
		lua_setfield(state, -2, "__le");
		lua_pushcclosure(state, lugi_lt_object, 0); /* a <= b */
		lua_setfield(state, -2, "__lt");
		lua_pushcclosure(state, lugi_eq_object, 0); /* a == b */
		lua_setfield(state, -2, "__eq");
		/* index */
		lua_pushcclosure(state, lugi_index_object, 0); /* a[b] */
		lua_setfield(state, -2, "__index");
		/* newindex */
		lua_pushcclosure(state, lugi_newindex_object, 0); /* a[b] = c */
		lua_setfield(state, -2, "__newindex");
		
		/* t.__gc */
		lua_pushcclosure(state, lugi_gc_object, 0);
		lua_setfield(state, -2, "__gc");
		
		lua_settable(state, LUA_REGISTRYINDEX);
	}
	lua_pop(state, 1); /* remove the result of checking for the metatable */
	
	/* check to see if the array object metatable exists */
	lua_pushlightuserdata(state, LUGI_METATABLE_ARRAY_KEY);
	
	if ( lua_type(state, -1) != LUA_TTABLE ) {
		/* create the metatable if it doesn't exist */
		lua_pushlightuserdata(state, LUGI_METATABLE_ARRAY_KEY);
		
		lua_createtable(state, 0, 8); /* 8 metamethods for array instances. */
		
		/* comparison */
		lua_pushcclosure(state, lugi_le_object, 0); /* a < b */
		lua_setfield(state, -2, "__le");
		lua_pushcclosure(state, lugi_lt_object, 0); /* a <= b */
		lua_setfield(state, -2, "__lt");
		lua_pushcclosure(state, lugi_eq_object, 0); /* a == b */
		lua_setfield(state, -2, "__eq");
		/* index */
		lua_pushcclosure(state, lugi_index_array, 0); /* a[b] */
		lua_setfield(state, -2, "__index");
		/* newindex */
		lua_pushcclosure(state, lugi_newindex_array, 0); /* a[b] = c */
		lua_setfield(state, -2, "__newindex");
		
		/* length */
		lua_pushcclosure(state, lugi_length_array, 0);
		lua_setfield(state, -2, "__len");
		
		/* concat */
		lua_pushcclosure(state, lugi_concat_array, 0);
		lua_setfield(state, -2, "__concat");
		
		/* t.__gc */
		lua_pushcclosure(state, lugi_gc_object, 0);
		lua_setfield(state, -2, "__gc");
		
		lua_settable(state, LUA_REGISTRYINDEX);
	}
	lua_pop(state, 1);
}


/*******
	This code could be replaced with something similar to contexts at a later date if you wanted
	to have more than one kind of thing you wanted to initialize, I suppose
*******/

/* Internal
  Places a glue function with the name specified into the global list of glue info
  If it's a method of a class, pass a pointer to the object's BBClass to insert it into
  the class's Lua method table
  If you're registering a global function (e.g., just a regular glue function), pass a NULL pointer
  for the class
*/
void p_lugi_register_method(lua_CFunction fn, BBString *name, BBClass *clas) {
    glueinfo_t *info;
    char *cname = bbStringToCString(name);
	
    info = lugi_g_infohead;
    while (info != NULL) {
        if ( info->type == METHODTYPE && info->clas == clas &&
            strcmp(cname, info->name) == 0 && info->data.fn == fn ) {
            // then
            bbMemFree(cname);
            return;
        }
        
        info = info->next;
    }
    
	info = (glueinfo_t*)bbMemAlloc(sizeof(glueinfo_t));
	info->type = METHODTYPE;
	info->data.fn = fn;
	info->clas = clas;
	info->name = cname;
	info->next = lugi_g_infohead;
	lugi_g_infohead = info;
}


/* Internal
  Places the offset of an instance variable for a class into the global list of glue info
  An exception is thrown if you pass a NULL pointer for the class
*/
void p_lugi_register_field(int offset, int type, BBString *name, BBClass *clas) {
	if ( clas == NULL )
		bbExThrowCString(ERRORSTR("@lugi_register_field: Cannot pass Null BBClass for field."));
    glueinfo_t *info;
    char *cname = bbStringToCString(name);
	
    info = lugi_g_infohead;
    while (info != NULL) {
        if ( info->type == FIELDTYPE && info->clas == clas &&
            strcmp(cname, info->name) == 0 && info->data.field.offset == offset &&
            info->data.field.type == type ) {
            // then
            bbMemFree(cname);
            return;
        }
        
        info = info->next;
    }
	
	
	info = (glueinfo_t*)bbMemAlloc(sizeof(glueinfo_t));
	info->type = FIELDTYPE;
	info->data.field.offset = static_cast<ptrdiff_t>(offset);
	info->data.field.type = static_cast<fieldtype_e>(type);
	info->clas = clas;
	info->name = cname;
	info->next = lugi_g_infohead;
	lugi_g_infohead = info;
}


/* Deallocates the internal list of closures */
void lugi_free_info() {
	glueinfo_t *info;
	while (lugi_g_infohead != NULL)
	{
		info = lugi_g_infohead;
		lugi_g_infohead = info->next;
		bbMemFree(info->name);	/* note: Use bbMemFree because ToCString uses bbMemAlloc, and its blocks have a small header attached to them */
		bbMemFree(info);
	}
}



/*********************************** Object handling/conversion ***********************************/

/* precondition: lua interface must be initialized for this state, or you will experience a crash. */
void lua_pushbmaxobject(lua_State *state, BBObject *obj) {
	int top = lua_gettop(state);
	
	if (obj == &bbNullObject || (BBArray*)obj == &bbEmptyArray) {
		lua_pushnil(state);
		return;
	}
	else if ((BBString*)obj == &bbEmptyString) {
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
	
	/* check object cache for existing object */
	lua_pushlightuserdata(state, LUGI_OBJECT_CACHE_KEY);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	/* cache[object] */
	lua_pushlightuserdata(state, obj);
	lua_gettable(state, -2);
	
	/* if cache[object] == object */
	if (lua_type(state, -1) == LUA_TUSERDATA) {
		lua_remove(state, -2);
		/* object exists, remove table and return */
		return;
	}
	
	lua_pop(state, 1);
	
	/* increment reference count for the object since BMax's default GC relies on ref counting */
	BBRETAIN(obj);
	
	/* store object */
	BBObject **data = (BBObject**)lua_newuserdata(state, sizeof(BBObject*));
	*data = obj;
	
#if BMX_TABLE_SUPPORT > 0
	/*
	  this is part of an unsupported feature to enable table-like behavior for BMax objects
	  decided not to include it (by default, anyway) because, frankly, it doesn't fit in when
	  you might be trying to set a field for some reason and you'll never receive an error
	  that the field doesn't exist because you're just setting some value for a table
	*/
	lua_newtable(state);
	if ( lua_setfenv(state, -2) == 0 )
	{
		lua_settop(state, top); /* clean up stack.. */
		luaL_error(state, ERRORSTR("@lua_pushbmaxobject: Failed to set environment table for BMax object."));
		return;
	}
#endif
	
	/* set the metatable */
	if ( obj->clas == &bbArrayClass )
		lua_pushlightuserdata(state, LUGI_METATABLE_ARRAY_KEY);
	else
		lua_pushlightuserdata(state, LUGI_METATABLE_KEY);
	
	lua_gettable(state, LUA_REGISTRYINDEX);
	lua_setmetatable(state, -2);
	
	/* cache the object and remove the cache table */
	lua_pushlightuserdata(state, obj);
	lua_pushvalue(state, -2);
	lua_settable(state, -4);
	lua_remove(state, -2);
	
	/* done */
}


/* Converts a Lua value to a BMax object (Userdata -> BBObject*, String -> BBString*, Table -> lua_tobmaxarray(..)) */
BBObject *lua_tobmaxobject(lua_State *state, int index) {
	/* check the type at the index */
	switch(lua_type(state, index))
	{
		case LUA_TNONE:
			luaL_error(state, ERRORSTR("@lua_tobmaxobject: Invalid index (%d)"), index);
			return &bbNullObject;
		
		case LUA_TNIL:				/* return null for nil */
			return &bbNullObject;
		
		case LUA_TSTRING:			/* if it's a string, return a string (since a BBString is an object) */
		{
			size_t length;
			const char *sbuf = lua_tolstring(state, -1, &length);
			return (BBObject*)bbStringFromBytes(sbuf, length);
		}
		
		case LUA_TTABLE: /* pass off to lua_tobmaxarray */
			return (BBObject*)lua_tobmaxarray(state, index);
		
		/* get an object out of the userdata */
		case LUA_TUSERDATA:
			if ( lua_isbmaxobject(state, index) != 0 )
				return *(BBObject**)lua_touserdata(state, index);
		
		/* and the best default ever: an error */
		default:
			luaL_error(state, ERRORSTR("@lua_tobmaxobject: Value at index (%d) is not an object."), index);
			return &bbNullObject;
	}
}


int32_t lua_isbmaxobject(lua_State *state, int index) {
	int result;
	/* first check the type of the value to make sure it's correct */
	if ( lua_type(state, index) != LUA_TUSERDATA )
		return 0;
	
	/* get the metatable for the value at the index */
	lua_getmetatable(state, index);
	
	/* get the generic metatable */
	lua_pushlightuserdata(state, LUGI_METATABLE_KEY);
	lua_gettable(state, LUA_REGISTRYINDEX);
	
	/* compare the two */
	result = lua_rawequal(state, -1, -2);
	
	if ( result == 0 ) {	/* check if it has the array metatable */
		lua_pop(state, 1);
		lua_pushlightuserdata(state, LUGI_METATABLE_ARRAY_KEY);
		lua_gettable(state, LUA_REGISTRYINDEX);
		
		result = lua_rawequal(state, -1, -2);
	}
	
	/* pop both metatables */
	lua_pop(state, 2);
	
	return result;
}



/************************************ Array handling/conversion ***********************************/

void lua_pushbmaxarray(lua_State *state, BBArray *arr) {
	lua_pushbmaxobject(state, (BBObject*)arr);
}


/* Converts a Lua table to a BMax array - the type of the array is determined by the first value in the table (at index 1) */
BBArray *lua_tobmaxarray(lua_State *state, int index) {
	switch (lua_type(state, index))
	{
		case LUA_TNONE:
			luaL_error(state, ERRORSTR("@lua_tobmaxarray: Invalid index (%d)"), index);
			return &bbEmptyArray;
		
		case LUA_TNIL:
			return &bbEmptyArray;
		
		case LUA_TUSERDATA:
			if ( lua_isbmaxobject(state, index) ) {
				BBObject *obj = lua_tobmaxobject(state, index);
				if ( obj->clas == &bbArrayClass ) {
					return (BBArray*)obj;
				}
				else {
					luaL_error(state, ERRORSTR("@lua_tobmaxarray: Value at index (%d) is not an array."), index);
					return &bbEmptyArray;
				}
			}
		
		case LUA_TTABLE: /* code below */
		break;
		
		default:
			if (lua_type(state, index) != LUA_TTABLE) {
				luaL_error(state, ERRORSTR("@lua_tobmaxarray: Value at index (%d) is not a table."), index);
				return &bbEmptyArray;
			}
	}
	
	/* make the index absolute, since we're now dealing with more than one value on the stack */
	if (index < 0 && index > LUA_REGISTRYINDEX)
		index = lua_gettop(state)+(index+1);
	
	/* the index into the array */
	size_t table_index;
	BBArray *arr = NULL;
	size_t len = lua_objlen(state, index);
	
	if ( len == 0 )
		return &bbEmptyArray;
	
	/* get the first item of the table */
	lua_pushinteger(state, 1);
	lua_gettable(state, index);
	
	/* starting at index 2 when iterating */
	table_index = 2;
	
	/* determine what type of array to create based on the first value in the table (at index 0) */
	switch (lua_type(state, -1)) {
		case LUA_TNUMBER: {		/* array of doubles */
			double *p;
			
			arr = bbArrayNew1D("d", len);
			p = (double*)BBARRAYDATA(arr, arr->dims);
			
			*p++ = lua_tonumber(state, -1);
			lua_pop(state, 1);
			
			for (; table_index <= len; ++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p++ = lua_tonumber(state, -1);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		case LUA_TBOOLEAN: {		/* array of integers */
			int *p;
			
			arr = bbArrayNew1D("i", len);
			p = (int*)BBARRAYDATA(arr, arr->dims);
			
			*p++ = lua_toboolean(state, -1);
			lua_pop(state, 1);
			
			for (; table_index <= len; ++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p++ = lua_toboolean(state, -1);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		case LUA_TSTRING: {		/* array of strings */
			BBString **p;
			
			arr = bbArrayNew1D("$", len);
			p = (BBString**)BBARRAYDATA(arr, arr->dims);
			
			*p = bbStringFromCString(lua_tostring(state, -1));
			BBRETAIN((BBObject*)*p++);
			lua_pop(state, 1);
			
			for (; table_index <= len; ++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p = bbStringFromCString(lua_tostring(state, -1));
				BBRETAIN((BBObject*)*p++);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		case LUA_TTABLE:		/* array of arrays (arrays inside of arrays do not have to be the same type) */
		case LUA_TUSERDATA:
		{
			BBObject **p;
			
			arr = bbArrayNew1D(":Object", len);
			p = (BBObject**)BBARRAYDATA(arr, arr->dims);
			
			*p = lua_tobmaxobject(state, -1);
			BBRETAIN(*p++);
			lua_pop(state, 1);
			
			for (; table_index <= len; ++table_index) {
				lua_pushinteger(state, table_index);
				lua_gettable(state, index);
				
				*p = lua_tobmaxobject(state, -1);
				BBRETAIN(*p++);
				
				lua_pop(state, 1);
			}
			
			return arr;
		}
		
		default:
			luaL_error(state, ERRORSTR("@lua_tobmaxarray: Arrays of type %s are not unsupported"), lua_typename(state, lua_type(state, -1)));
			return &bbEmptyArray;
	} /* switch lua type */
	
	return arr;
} /* lua_tobmaxarray */



/******************************* Internal metamethod implementations ******************************/

/*********** BBObject metatable */

/* a <= b */
static int lugi_le_object(lua_State *state) {
	BBObject *a = lua_tobmaxobject(state, 1);
	BBObject *b = lua_tobmaxobject(state, 2);
	lua_pushboolean(state, (a->clas->Compare(a, b) <= 0));
	return 1;
} /* lugi_le_object */


/* a < b */
static int lugi_lt_object(lua_State *state) {
	BBObject *a = lua_tobmaxobject(state, 1);
	BBObject *b = lua_tobmaxobject(state, 2);
	lua_pushboolean(state, (a->clas->Compare(a, b) < 0));
	return 1;
} /* lugi_lt_object */


/* a == b */
static int lugi_eq_object(lua_State *state) {
	BBObject *a = lua_tobmaxobject(state, 1);
	BBObject *b = lua_tobmaxobject(state, 2);
	lua_pushboolean(state, (a->clas->Compare(a, b) == 0));
	return 1;
} /* lugi_eq_object */


/* table[key]  OR  table.key */
static int lugi_index_object(lua_State *state) {
	
#ifdef BMX_TABLE_SUPPORT > 1	
	/* prior table behavior for BMax objects */
	lua_getfenv(state, 1);
	lua_pushvalue(state, 2);
	lua_gettable(state, -2);
	if ( lua_isnil(state, -1) == 0 ) {
		return 1;
	}
	
	lua_settop(state, 2);
#endif
	
	if (lua_type(state, 2) == LUA_TSTRING) {
		
		BBObject *obj = lua_tobmaxobject(state, 1);
		BBClass *clas = obj->clas;
		
		/* while class */
		do {
		
			/* get class VMT */
			lua_pushlightuserdata(state, clas);
			lua_gettable(state, LUA_REGISTRYINDEX);
		
			if ( lua_type(state, -1) == LUA_TTABLE ) /* there's a lookup table for the class.. */
			{
			
				lua_pushvalue(state, 2);
				lua_rawget(state, -2);
						
				int type = lua_type(state, -1);
			
				if (type == LUA_TUSERDATA) {		/* getting the value of a field */
					fieldinfo_t *info = (fieldinfo_t*)lua_touserdata(state, -1);
					bmx_field *field = (bmx_field*)((char*)obj+info->offset);
					
					switch (info->type&(~BOOLFIELDOPT)) {
						case BYTEFIELD:
						if (info->type&BOOLFIELDOPT) {
							lua_pushboolean(state, field->byte_value);
						}
						else {
							lua_pushinteger(state, field->byte_value);
						}
						break;
				
						case SHORTFIELD:
						if (info->type&BOOLFIELDOPT) {
							lua_pushboolean(state, field->short_value);
						}
						else {
							lua_pushinteger(state, field->short_value);
						}
						break;
				
						case INTFIELD:
						if (info->type&BOOLFIELDOPT) {
							lua_pushboolean(state, field->int_value);
						}
						else {
							lua_pushinteger(state, field->int_value);
						}
						break;
				
						case FLOATFIELD:
						lua_pushnumber(state, field->float_value);
						break;
				
						case DOUBLEFIELD:
						lua_pushnumber(state, field->double_value);
						break;
				
						case LONGFIELD:
						if (info->type&BOOLFIELDOPT) {
							lua_pushboolean(state, field->long_value);
						}
						else {
							lua_pushinteger(state, field->long_value);
						}
						break;
				
						case STRINGFIELD: {
							char *strbuf = bbStringToCString(field->string_value);
							lua_pushlstring(state, strbuf, field->string_value->length);
							bbMemFree(strbuf);
						} break;
				
						case ARRAYFIELD:
						lua_pushbmaxarray(state, field->arr_value);
						break;
				
						case OBJECTFIELD:
						lua_pushbmaxobject(state, field->obj_value);
						break;
				
						default:
						return luaL_error(state, ERRORSTR("@lugi_index_object: Unrecognized field type (%d)."), info->type);
						break;
					}
					return 1;
					
					/** method **/
				}
				else if (type ==  LUA_TFUNCTION) {
					return 1;
				}
				else {
					lua_pop(state, 1);
				}
			} /* VMT found */
			
			/* pop previous VMT/nil */
			lua_pop(state, 1);
			
			/* iterate to the superclass */
			clas = clas->super;
			
		} while (clas != NULL);
		
	} /* key is string */
	
#if BMX_TABLE_SUPPORT < 1
	/* notify that there was an error accessing a field or method that does not exist */
	const char *objtype="invalid object";
	if (lua_isbmaxobject(state, 1))
		objtype = lua_tobmaxobject(state, 1)->clas->debug_scope->name;
	
	if ( (LUA_TNUMBER|LUA_TSTRING)&lua_type(state, 2) )
		return luaL_error(state, ERRORSTR("@lugi_index_object: Index (%s) for object<%s> is not a valid field or method."), lua_tostring(state, 2), objtype);
	else
		return luaL_error(state, ERRORSTR("@lugi_index_object: Index for object<%s> is not a valid field or method."), objtype);
#else
	lua_pushnil(state);
	return 1;
#endif
} /* lugi_index_object */


/* table[key] = value OR table.key = value */
static int lugi_newindex_object(lua_State *state) {
	if (lua_type(state, 2) == LUA_TSTRING) {
		
		BBObject *obj = lua_tobmaxobject(state, 1);
		BBClass *clas = obj->clas;
		
		/* while class != NULL */
		do {
			/* get class VMT */
			lua_pushlightuserdata(state, clas);
			lua_gettable(state, LUA_REGISTRYINDEX);
			
			if ( lua_type(state, -1) == LUA_TTABLE ) /* there's a lookup table for the class.. */
			{
				lua_pushvalue(state, 2);
				lua_rawget(state, -2);
				
				if (lua_type(state, -1) == LUA_TUSERDATA) {		/* getting the value of a field */
                    fieldinfo_t *info = (fieldinfo_t*)lua_touserdata(state, -1);;
					bmx_field *field = (bmx_field*)((char*)obj+info->offset);
					
					clas = NULL;
					
					switch (info->type) /* set the value of a field */
					{
						case BYTEFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN) {
							field->byte_value = (unsigned char)lua_toboolean(state, 3);
						}
						else {
							field->byte_value = (unsigned char)lua_tointeger(state, 3);
						}
						break;
					
						case SHORTFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN) {
							field->short_value = (unsigned short)lua_toboolean(state, 3);
						}
						else {
							field->short_value = (unsigned short)lua_tointeger(state, 3);
						}
						break;
					
						case INTFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN) {
							field->int_value = lua_toboolean(state, 3);
						}
						else {
							field->int_value = lua_tointeger(state, 3);
						}
						break;
					
						case FLOATFIELD:
						field->float_value = (float)lua_tonumber(state, 3);
						break;
					
						case DOUBLEFIELD:
						field->double_value = lua_tonumber(state, 3);
						break;
					
						case LONGFIELD:
						if (lua_type(state, 3) == LUA_TBOOLEAN) {
							field->long_value = lua_toboolean(state, 3);
						}
						else {
							field->long_value = lua_tointeger(state, 3);
						} break;
					
						case STRINGFIELD: {
							const char *strbuf = lua_tostring(state, 3);
							BBString *last = field->string_value;
							BBRETAIN((BBObject*)(field->string_value = bbStringFromCString(strbuf)));
							BBRELEASE((BBObject*)last);
						} break;
					
						case ARRAYFIELD: {
							BBArray *last = field->arr_value;
							BBRETAIN((BBObject*)(field->arr_value = lua_tobmaxarray(state, 3)));
							BBRELEASE((BBObject*)last);
						} break;
					
						case OBJECTFIELD: {
						BBObject *last = field->obj_value;
						BBRETAIN(field->obj_value = lua_tobmaxobject(state, 3));
						BBRELEASE(last);
						} break;
					
						default:
						return luaL_error(state, ERRORSTR("@lugi_newindex_object: Unrecognized field type (%d)."), info->type);
						break;
					} /* set value based on type */
					return 0;
				}
				else {
					lua_pop(state, 1);
				}
			} /* VMT found */
			
			lua_pop(state, 1);
			
			clas = clas->super; /* iterate to a superclass if nothing is found in the class */
			
		} while (clas != NULL);
	} /* key is string */
	
#if BMX_TABLE_SUPPORT < 1
	const char *objtype="null";
	if (lua_isbmaxobject(state, 1))
		objtype = lua_tobmaxobject(state, 1)->clas->debug_scope->name;
	
	if ((LUA_TNUMBER|LUA_TSTRING)&lua_type(state,2))
		return luaL_error(state, ERRORSTR("@lugi_newindex_object: Index (%s) for object<%s> is not a valid field or method."), lua_tostring(state, 2), objtype);
	else
		return luaL_error(state, ERRORSTR("@lugi_newindex_object: Index for object<%s> is not a valid field or method."), objtype);
#else
	/* prior table behavior for BMax objects - disabled by default */
	lua_settop(state, 3);
	
	lua_getfenv(state, 1);
	lua_insert(state, 2);
	lua_settable(state, 2);
	
	return 0;
#endif
}


/* Userdata collection routine - only used in unthreaded builds where refcount has to be decremented */
static int lugi_gc_object(lua_State *state) {
	BBObject **obj = (BBObject**)lua_touserdata(state,1);
	BBRELEASE(*obj);
	*obj = NULL;
	return 0;
}


/*********** BBArray metatable */

/* #table */
static int lugi_length_array(lua_State *state) {
	BBArray *arr = lua_tobmaxarray(state, 1);
	lua_pushinteger(state, arr->scales[0]);
	return 1;
}


/* table[key] or table.key */
/* specifically applies to BBArray objects */
static int lugi_index_array(lua_State *state) {
	if (lua_isnumber(state, 2) == 1) {
		BBArray *arr = lua_tobmaxarray(state, 1);
		lua_Integer index = lua_tointeger(state, 2);
		if ( index < 0 || arr->scales[0] <= index ) {
			return luaL_error(state, ERRORSTR("@lugi_index_array: Attempt to index array failed: index (%d) out of bounds"), index);
		}
		
		#define LGINDEXARRAY(LUA_PUSHX, TYPE) ((LUA_PUSHX)(state, ((TYPE*)BBARRAYDATA(arr, arr->dims))[index]))

		switch (arr->type[0]) {
			case 'b':	/* byte */
			LGINDEXARRAY(lua_pushinteger, BBBYTE);
			break;

			case 's':	/* short */
			LGINDEXARRAY(lua_pushinteger, BBSHORT);
			break;

			case 'i':	/* int */
			LGINDEXARRAY(lua_pushinteger, BBINT);
			break;

			case 'l':	/* long */
			LGINDEXARRAY(lua_pushinteger, BBLONG);
			break;

			case 'f':	/* float */
			LGINDEXARRAY(lua_pushnumber, BBFLOAT);
			break;

			case 'd':	/* double */
			LGINDEXARRAY(lua_pushnumber, BBDOUBLE);
			break;

			case '$':	/* string */
			case ':':	/* any type of object */
			case '[':
			LGINDEXARRAY(lua_pushbmaxobject, BBObject*);
			break;
			
			default:
			return luaL_error(state, ERRORSTR("@lugi_index_array: Unsupported array element type: %s"), arr->type);
		}
		
		#undef LGINDEXARRAY
		
		return 1;
	} /* key is a valid type of index for an array */
	else if ( lua_isstring(state, 2) ) {
		return lugi_index_object(state); // pass off the event to the object metamethod so it can check for fields/methods
	}
	
	return luaL_error(state, ERRORSTR("@lugi_index_array: Invalid type for an array index (%s), must be an integer or a string convertible to an integer"), lua_typename(state, lua_type(state, 2)));
}


static int lugi_newindex_array(lua_State *state) {
	if (lua_isnumber(state, 2) == 1) {
		BBArray *arr = lua_tobmaxarray(state, 1);
		
		lua_Integer index = lua_tointeger(state, 2);
		if ( index < 0 || arr->scales[0] <= index ) {
			return luaL_error(state, ERRORSTR("@lugi_newindex_array: Attempt to index array failed: index (%d) out of bounds"), index);
		}

		switch (arr->type[0]) {
			case 'b':	/* byte */
				if ( lua_isnumber(state, 3) ) {
					((BBBYTE*)BBARRAYDATA(arr, arr->dims))[index] = (BBBYTE)lua_tointeger(state, 3);
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of byte array"), lua_typename(state, lua_type(state, 3)));
				}
			break;

			case 's':	/* short */
				if ( lua_isnumber(state, 3) ) {
					((BBSHORT*)BBARRAYDATA(arr, arr->dims))[index] = (BBSHORT)lua_tointeger(state, 3);
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of short array"), lua_typename(state, lua_type(state, 3)));
				}
			break;

			case 'i':	/* int */
				if ( lua_isnumber(state, 3) ) {
					((BBINT*)BBARRAYDATA(arr, arr->dims))[index] = (BBINT)lua_tointeger(state, 3);
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of integer array"), lua_typename(state, lua_type(state, 3)));
				}
			break;

			case 'l':	/* long */
				if ( lua_isnumber(state, 3) ) {
					((BBLONG*)BBARRAYDATA(arr, arr->dims))[index] = (BBLONG)lua_tointeger(state, 3);
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of long array"), lua_typename(state, lua_type(state, 3)));
				}
			break;

			case 'f':	/* float */
				if ( lua_isnumber(state, 3) ) {
					((BBFLOAT*)BBARRAYDATA(arr, arr->dims))[index] = (BBFLOAT)lua_tonumber(state, 3);
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of short array"), lua_typename(state, lua_type(state, 3)));
				}
			break;

			case 'd':	/* double */
				if ( lua_isnumber(state, 3) ) {
					((BBDOUBLE*)BBARRAYDATA(arr, arr->dims))[index] = (BBDOUBLE)lua_tonumber(state, 3);
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of short array"), lua_typename(state, lua_type(state, 3)));
				}
			break;

			case '$':	/* string */
			{
				BBString **data = ((BBString**)BBARRAYDATA(arr, arr->dims));
				if ( lua_isstring(state, 3) ) {
					BBString *newstring = bbStringFromCString(lua_tostring(state, 3));
					BBRETAIN((BBObject*)newstring);
					BBRELEASE((BBObject*)data[index]);
					data[index] = newstring;
				}
				else if ( lua_isnil(state, 3 ) ) {
					BBRELEASE((BBObject*)data[index]);
					data[index] = &bbEmptyString;
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign value with type %s to element of string array"), lua_typename(state, lua_type(state, 3)));
				}
			} break;
			
			case ':':	/* any type of object */
			{
				BBObject *value = lua_tobmaxobject(state, 3);
				BBClass *arrclas = NULL;
				
				{	/* search for the array element type's class */
					const char *arrtypename = arr->type+1;
					int numTypes = 0;
					int regidx = 0;
					BBClass **regtypes = bbObjectRegisteredTypes(&numTypes);
					
					for (; regidx < numTypes; ++regidx) {
						if ( strcmp(regtypes[regidx]->debug_scope->name, arrtypename) == 0 ) {
							arrclas = regtypes[regidx];
							break;
						}
					}
				}
				
				if ( arrclas == NULL ) {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Undefined array type encountered: %s"), arr->type);
				}
				
				value = bbObjectDowncast(value, arrclas);
				
				BBObject **data = (BBObject**)BBARRAYDATA(arr, arr->dims);
				BBRETAIN(value);
				BBRELEASE(data[index]);
				data[index] = value;
			} break;
			
			case '[': {
				BBArray *value = lua_tobmaxarray(state, 3);
				if ( (BBObject*)value == &bbNullObject ) {
					value = &bbEmptyArray;
				}
				
				if ( value == &bbEmptyArray || strcmp(arr->type+2, value->type) == 0 ) {
					BBArray **data = (BBArray**)BBARRAYDATA(arr, arr->dims);
					BBRETAIN((BBObject*)value);
					BBRELEASE((BBObject*)data[index]);
					data[index] = value;
				}
				else {
					return luaL_error(state, ERRORSTR("@lugi_newindex_array: Cannot assign array value to an element of an array of a differing type"));
				}
			} break;
			
			default:
			return luaL_error(state, ERRORSTR("@lugi_newindex_array: Unsupported array element type: %s"), arr->type);
		}
		
		return 0;
	} /* key is a valid type of index for an array */
	
	return luaL_error(state, ERRORSTR("@lugi_newindex_array: Invalid type for an array index (%s), must be an integer or a string convertible to an integer"), lua_typename(state, lua_type(state, 2)));
}


static int lugi_concat_array(lua_State *state) {
	BBArray *l = lua_tobmaxarray(state, 1);
	BBArray *r = lua_tobmaxarray(state, 2);
	
	if ( (BBObject*)r == &bbNullObject ) {
		r = &bbEmptyArray;
	}
	
	if ( l->clas != &bbArrayClass || r->clas != &bbArrayClass ) {
		luaL_error(state, ERRORSTR("@lugi_concat_array: Both objects are not arrays."));
	}
	
	if ( r == &bbEmptyArray || strcmp(l->type, r->type) == 0 ) {
		lua_pushbmaxobject(state, (BBObject*)bbArrayConcat(l->type, l, r));
		return 1;
	}
	
	luaL_error(state, ERRORSTR("@lugi_concat_array: Invalid array types."));
	return 0;
}



/*************************************** Object constructor ***************************************/

int p_lugi_new_object(lua_State *state) {
	BBClass *clas = (BBClass*)lua_touserdata(state, lua_upvalueindex(1));
	lua_pushbmaxobject(state, bbObjectNew(clas));
	return 1;
}



/************************* Object methods (SendMessage, Compare, ToString) ************************/

/*
  receiver:SendMessage(
    message : LUA_TNIL or LUA_TSTRING or LUA_TTABLE or Object,
    [context = nil : LUA_TNIL or LUA_TSTRING or LUA_TTABLE or Object]
  ) => LUA_TSTRING, LUA_TTABLE, Object
*/
int lugi_sendmessage_object(lua_State *state) {
	int top = lua_gettop(state);
	
	if (2 < top || 3 < top) {
		/* try to provide somewhat useful error messages */
		if (top < 1) {
			return luaL_error(state, ERRORSTR("@p_lugi_sendmessage_object: No instance for call to Object#SendMessage"));
		}
		else if (top == 1) {
			return luaL_error(state, ERRORSTR("@p_lugi_sendmessage_object: Too few arguments (%d for 1) for Object#SendMessage"), (top-1));
		}
		else {
			return luaL_error(state, ERRORSTR("@p_lugi_sendmessage_object: Too many arguments (%d for 2) for Object#SendMessage"), (top-1));
		}
	}
	
	BBObject *receiver = lua_tobmaxobject(state, 1);
	BBObject *message = lua_tobmaxobject(state, 2);
	BBObject *context = &bbNullObject;
	
	if (top == 3)
		context = lua_tobmaxobject(state, 3);
	
	return 1;
}


/* receiver:ToString() => LUA_TSTRING */
int lugi_tostring_object(lua_State *state) {
	int top = lua_gettop(state);
	if (top != 1) {
		if (top < 1) {
			return luaL_error(state, ERRORSTR("@p_lugi_tostring_object: No instance for call to Object#ToString"));
		}
		else {
			return luaL_error(state, ERRORSTR("@p_lugi_tostring_object: Too many arguments (%d for 0) to Object#ToString"), (top-1));
		}
	}
	
	BBObject *receiver = lua_tobmaxobject(state, 1);
	lua_pushbmaxobject(state, (BBObject*)receiver->clas->ToString(receiver));
	return 1;
}


/* left:Compare(Object|LUA_TSTRING|LUA_TTABLE) => LUA_TNUMBER */
int lugi_compare_object(lua_State *state) {
	int top = lua_gettop(state);
	if (top != 2) {
		if (top < 1) {
			return luaL_error(state, ERRORSTR("@p_lugi_compare_object: No instance for call to Object#Compare"));
		}
		else if (top == 1) {
			return luaL_error(state, ERRORSTR("@p_lugi_compare_object: Too few arguments (%d for 1) for Object#Compare"), (top-1));
		}
		else {
			return luaL_error(state, ERRORSTR("@p_lugi_compare_object: Too many arguments (%d for 1) for Object#Compare"), (top-1));
		}
	}
	
	BBObject *receiver = lua_tobmaxobject(state, 1);
	lua_pushinteger(state, receiver->clas->Compare(receiver, lua_tobmaxobject(state, 2)));
	
	return 1;
}



/********************************** Array methods (Slice, Sort) ***********************************/

static int lugi_sort_array(lua_State *state) {
	BBArray *arr = lua_tobmaxarray(state, 1);
	int ascending = 0;
	if ( lua_isnone(state, 2) == 0 ) {
		ascending = lua_toboolean(state, 2);
	}
	bbArraySort(arr, ascending);
	return 0;
}


/*
	array:Slice(
	from : LUA_TNUMBER or LUA_TSTRING(number),
	to : LUA_TNUMBER or LUA_TSTRING(number)
	) => Object(array) or LUA_TNIL
*/
static int lugi_slice_array(lua_State *state) {
	int top = lua_gettop(state);
	if ( top < 3 ) {
		return luaL_error(state, ERRORSTR("@lugi_slice_array: Too few arguments to Slice(from, to)"));
	}
	else if ( top > 3 ) {
		return luaL_error(state, ERRORSTR("@lugi_slice_array: Too many arguments to Slice(from, to)"));
	}
	
	BBArray *arr = lua_tobmaxarray(state, 1);
	
	lua_Integer beg, end;
	beg = lua_tointeger(state, 2);
	end = lua_tointeger(state, 3);
	BBArray *slice = bbArraySlice(arr->type, arr, beg, end);
	lua_pushbmaxobject(state, (BBObject*)slice);
	return 1;
}


static int lugi_type_array(lua_State *state) {
	BBArray *arr = lua_tobmaxarray(state, 1);
	
	switch (arr->type[0]) {
		case 'b':	/* byte */
		lua_pushliteral(state, "byte");
		break;
		case 's':
		lua_pushliteral(state, "short");
		break;
		case 'i':
		lua_pushliteral(state, "integer");
		break;
		case 'l':
		lua_pushliteral(state, "long");
		case 'f':
		lua_pushliteral(state, "float");
		case 'd':
		lua_pushliteral(state, "double");
		break;
		
		case '$':	/* string */
		lua_pushliteral(state, "string");
		break;
		
		default:	/* some type of object */
		lua_pushstring(state, arr->type);
		break;
	}
	
	return 1;
}



#ifdef __cplusplus
}
#endif
