#ifndef ASX_INTERFACE_H
#define ASX_INTERFACE_H
#include <vitex/config.hpp>
#ifdef VI_MICROSOFT
#define VI_EXPORT __declspec(dllexport)
#else
#define VI_EXPORT
#endif
extern "C"
{
	VI_EXPORT void asx_import_builtin(const char* path);
	VI_EXPORT void asx_import_native(const char* path);
	VI_EXPORT void asx_export_property(const char* declaration, void* property_address);
	VI_EXPORT void asx_export_function_address(const char* declaration, void(*function_address)());
	VI_EXPORT void asx_export_namespace_begin(const char* name);
	VI_EXPORT void asx_export_namespace_end();
	VI_EXPORT void asx_export_enum(const char* name);
	VI_EXPORT void asx_export_enum_value(const char* name, const char* declaration, int value);
	VI_EXPORT void asx_export_class_address(const char* name, size_t size, size_t flags);
	VI_EXPORT void asx_export_class_property_address(const char* name, const char* declaration, int property_offset);
	VI_EXPORT void asx_export_class_constructor_address(const char* name, const char* declaration, void(*constructor_function_address)(void*));
	VI_EXPORT void asx_export_class_operator_address(const char* name, const char* declaration, void(*operator_function_address)(void*));
	VI_EXPORT void asx_export_class_copy_operator_address(const char* name, void(*copy_operator_function_address)(void*));
	VI_EXPORT void asx_export_class_destructor_address(const char* name, void(*destructor_function_address)(void*));
	VI_EXPORT void asx_export_class_method_address(const char* name, const char* declaration, void(*method_function_address)());
}
#endif