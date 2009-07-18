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

'Import "metadata.bmx"

Private

Const REGISTER_FIELD_NAME$ = "register_field"

Function TypeStringForField:String(tid:TTypeID, fid:TField)
	Local typ$
	Select tid
		Case ByteTypeId
			typ = "BYTEFIELD"
		Case ShortTypeId
			typ = "SHORTFIELD"
		Case IntTypeId
			typ = "INTFIELD"
		Case FloatTypeId
			typ = "FLOATFIELD"
		Case DoubleTypeId
			typ = "DOUBLEFIELD"
		Case LongTypeId
			typ = "LONGFIELD"
		Case ArrayTypeId
			typ = "ARRAYFIELD"
		Case StringTypeId
			typ = "STRINGFIELD"
		Default
			If ArrayTypeId._class = tid._class Then
				typ = "ARRAYFIELD"
			Else
				typ = "OBJECTFIELD"
			EndIf
	End Select
	
	' Add LUGI_ prefix
	typ = "LUGI_"+typ
	
	If fid.Metadata(LUGI_META_BOOL).ToInt()>0 Then
		Select tid
			Case ByteTypeId, ShortTypeId, IntTypeId, LongTypeId
				typ = "("+typ+"|LUGI_BOOLFIELDOPT)"
		End Select
	EndIf
	
	Return typ
End Function

Type LExposedField
	Field fieldid:TField
	Field registration$
	
	' Initializes an exposed field - returns Null if the field isn't exposed
	Method InitWithField:LExposedField(id:TField, owner:TTypeID)
		fieldid = id
		
		If Not ((owner.Metadata(LUGI_META_HIDEFIELDS).ToInt()>0) Or (fieldid.Metadata(LUGI_META_HIDDEN).ToInt()>0)) Then
			registration = REGISTER_FIELD_NAME+"( " + fieldid._index + ", " + TypeStringForField(fieldid.TypeId(), fieldid) + ", ~q" + fieldid.Name() + "~q, Byte Ptr(TTypeID.ForName(~q" + owner.Name() + "~q)._class) )"
		Else
			Return Null
		EndIf
		
		Return Self
	End Method
	
	Method PreInitBlock:String()
		Return registration
	End Method
End Type
