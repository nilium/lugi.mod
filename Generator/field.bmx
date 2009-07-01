Strict

Import "metadata.bmx"

Private

Function TypeStringForField:String(tid:TTypeID)
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
	Return typ
End Function

Public

Type LExposedField
	Field fieldid:TField
	Field registration$
	
	Method InitWithField:LExposedField(id:TField, owner:TTypeId)
		fieldid = id
		
		If Not ((owner.Metadata(LUGI_META_HIDEFIELDS).ToInt()>0) Or (fieldid.Metadata(LUGI_META_HIDDEN).ToInt()>0)) Then
			registration = "p_lugi_register_field " + fieldid._index + ", " + TypeStringForField(fieldid.TypeId()) + ", " + fieldid.Name() + ", TTypeId.ForName(~q" + owner.Name() + "~q)"
		EndIf
		
		Return Self
	End Method
	
	Method PreInitBlock:String()
		Return registration
	End Method
End Type
