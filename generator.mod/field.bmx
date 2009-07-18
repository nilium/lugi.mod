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
