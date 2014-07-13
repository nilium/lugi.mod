Rem
	LuGI - Copyright (c) 2009, 2014 Noel R. Cower

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

'Import brl.Map

Private

Rem
Inserts strings from the passed array into the string at marked insertion points.

Insertions in the format string are marked by \<index>. All indices are one-based unsigned integers.
Indices smaller than 1 and larger than the number of objects passed will fail an assertion, resulting
in an exception being thrown.

Essentially, the marker index must meet this condition: [1 <= index <= strings.Length]

To escape a marker, simply use another backslash (similar to any other system where you'd escape
a backslash or its respective escape character).

For example:
\code
	' Standard insertion points
	FormatString("\1 \003 \2 ", ["Foo", "Bar", "Baz"])		' => "Foo Baz Bar"


	' Escaped insertion points
	FormatString("\\1 \\2 \\003", ["Foo", "", "Wimbleton"])	' => "\1 \2 \003"
\endcode
EndRem
Function FormatString$(format$, strings:String[])
	Const CHAR_0% = 48
	Const CHAR_9% = 57
	Const CHAR_SLASH% = 92

	Rem
	Local strings:String[objects.Length]

	For Local index:Int = 0 Until objects.Length
		If objects[index] Then
			strings[index] = objects[index].ToString()
		Else
			strings[index] = "Null"
		EndIf
	Next
	EndRem

	Local numberIndices:Int[] = [-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1]
	Local numberLengths:Int[16]
	Local numbers:Int[16]
	Local numberIndex:Int = 0
	Local escape:Int = False
	Local length:Int = 0
	Local char:Int

	For Local index:Int = 0 Until format.Length
		char = format[index]

		If escape Then
			If CHAR_0 <= char And char <= CHAR_9 Then
				If numberLengths[numberIndex] = 0 Then
					numberIndices[numberIndex] = index-1
				EndIf

				char :- CHAR_0
				numbers[numberIndex] :* 10
				numbers[numberIndex] :+ char
				numberLengths[numberIndex] :+ 1
				Continue
			ElseIf numberIndices[numberIndex] <> -1 Then
				numbers[numberIndex] :- 1
				Assert numbers[numberIndex] >= 0 And numbers[numberIndex] < strings.Length ..
					Else "FormatString: Index out of range: index must be >= 1 and <= strings.Length"
				length :+ strings[numbers[numberIndex]].Length
				numberIndex :+ 1
				escape = False
			EndIf
		EndIf

		If char = CHAR_SLASH Then
			If Not escape Then
				If numberIndex >= numbers.Length Then
					numbers = numbers[..numbers.Length*2]
					numberLengths = numberLengths[..numbers.Length]
					numberIndices = numberIndices[..numbers.Length]
				EndIf
				escape = True
				Continue
			Else
				escape = False
			EndIf
		EndIf

		length :+ 1
	Next

	If escape And numberIndices[numberIndex] <> -1 Then
		numbers[numberIndex] :- 1
		length :+ strings[numbers[numberIndex]].Length
	EndIf

	Local buffer:Short Ptr = Short Ptr MemAlloc(length * 2)
	Local p:Short Ptr = buffer

	numberIndex = 0
	escape = False
	For Local index:Int = 0 Until format.Length
		If index = numberIndices[numberIndex] Then
			index :+ numberLengths[numberIndex]
			Local innerString$ = strings[numbers[numberIndex]]
			For Local innerStringIndex:Int = 0 Until innerString.Length
				p[0] = innerString[innerStringIndex]
				p :+ 1
			Next
			numberIndex :+ 1
			Continue
		ElseIf Not escape And format[index] = CHAR_SLASH Then
			escape = True
			Continue
		EndIf
		escape = False

		p[0] = format[index]
		p :+ 1
	Next

	Local output$ = String.FromShorts(buffer, length)
	MemFree(buffer)
	Return output
End Function

' Returns the name of the lua_push* function to be used when pushing something of the type passed to the Lua stack
Function LuaPushFunctionForTypeID$(typ:TTypeID)
	If typ._class = ArrayTypeId._class Then
		Return "lua_pushbmaxarray"
	EndIf

	Select typ
	Case Null
		Return Null
	Case ArrayTypeId
		Return "lua_pushbmaxarray"
	Case StringTypeId
		Return "lua_pushstring"
	Case ByteTypeId, ShortTypeId, IntTypeId, LongTypeId
		Return "lua_pushinteger"
	Case FloatTypeId, DoubleTypeId
		Return "lua_pushnumber"
	Default
		If typ.ExtendsType(ObjectTypeId) Then
			Return "lua_pushbmaxobject"
		EndIf
		Return Null
	End Select
End Function

' Returns the name of the lua_to* function to be used when getting something of the type passed off the Lua stack
Function LuaToFunctionForTypeID$(typ:TTypeID, vmname$="lua_vm", stackidx%=-1, bool%=False)
	If typ._class = ArrayTypeId._class Then
		Return typ.Name()+"(lua_tobmaxarray("+vmname+", "+stackidx+"))"
	EndIf

	Select typ
	Case ArrayTypeId
		Return typ.Name()+"(lua_tobmaxarray("+vmname+", "+stackidx+"))"
	Case StringTypeId
		Return "lua_tostring("+vmname+", "+stackidx+")"
	Case ByteTypeId, ShortTypeId, IntTypeId, LongTypeId
		If bool Then
			Return "lua_toboolean("+vmname+", "+stackidx+")"
		Else
			Return "lua_tointeger("+vmname+", "+stackidx+")"
		EndIf
	Case FloatTypeId, DoubleTypeId
		Return "lua_tonumber("+vmname+", "+stackidx+")"
	Default
		Return typ.Name()+"(lua_tobmaxobject("+vmname+", "+stackidx+"))"
	End Select
End Function

' Returns the default value an argument should have for a type
Function DefaultValueForTypeID$(typ:TTypeID)
	Select typ
	Case StringTypeId
		Return "~q~q"
	Case ByteTypeId, ShortTypeId, IntTypeId, LongTypeId
		Return "0"
	Case FloatTypeId
		Return "0#"
	Case DoubleTypeId
		Return "0!"
	Default
		Return "Null"
	End Select
End Function

Function GenerateUniqueTag$(length%, timeout%=-1)
	?Threaded
	Global g_tags_mutex:TMutex = New TMutex
	?
	Global g_tags:TMap = New TMap
	Const TAG_CHARS$="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

	' generate a short tag to follow the method name, just to reduce the chance of conflicts
	Local tagbuff:Short Ptr = Short Ptr(MemAlloc(length*2))
	Local tag:String
	?Threaded
	g_tags_mutex.Lock
	?
	Local ticks:Int = 0
	Repeat
		For Local i:Int = 0 Until length
			tagbuff[i] = TAG_CHARS[Rand(0,TAG_CHARS.Length-1)]
		Next

		tag = String.FromShorts(tagbuff, length)
		timeout :+ 1
	Until Not g_tags.Contains(tag) Or timeout = ticks
	g_tags.Insert(tag, tag)
	?Threaded
	g_tags_mutex.Unlock
	?
	MemFree(tagbuff)
	Return tag
End Function

Function SeedRndWithDateTime()
	Local date$ = CurrentDate()
	Local r:Int = (date[0 .. 2].ToInt()-1) * 86400
	r :+ (date[date.Length-4 ..]).ToInt() * 31536000
	r :+ Millisecs()
	SeedRnd(r)
End Function
