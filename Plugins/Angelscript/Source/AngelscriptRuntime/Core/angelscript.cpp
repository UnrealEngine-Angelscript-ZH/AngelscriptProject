#include "angelscript.h"
#include "ClassGenerator/ASClass.h"

asITypeInfo* asIScriptObject::GetObjectType() const
{
	//return (asITypeInfo*)((UObject*)this)->GetClass()->ScriptTypePtr;
	UASClass* asClass = UASClass::GetFirstASClass((UObject*)this);
	if (asClass)
		return (asITypeInfo*)asClass->ScriptTypePtr;
	else
		return nullptr;
}
