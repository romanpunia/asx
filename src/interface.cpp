#include "interface.h"
#include "app.h"

void asx_import_builtin(const char* path)
{
	VI_ASSERT(path != nullptr, "path should be set");
	auto* env = asx::environment::get();
	env->vm->import_system_addon(path).expect("import error");
}
void asx_import_native(const char* path)
{
	VI_ASSERT(path != nullptr, "path should be set");
	auto* env = asx::environment::get();
	env->vm->import_addon(path).expect("import error");
}
void asx_export_property(const char* declaration, void* property_address)
{
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	VI_ASSERT(property_address != nullptr, "property address should be set");
	auto* env = asx::environment::get();
	env->vm->set_property_address(declaration, property_address).expect("binding error");
}
void asx_export_function_address(const char* declaration, void(*function_address)())
{
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	VI_ASSERT(function_address != nullptr, "function address should be set");
	auto* env = asx::environment::get();
	env->vm->set_function_address(declaration, bridge::function_call(function_address)).expect("binding error");
}
void asx_export_namespace_begin(const char* name)
{
	VI_ASSERT(name != nullptr, "name should be set");
	auto* env = asx::environment::get();
	env->vm->begin_namespace(name).expect("binding error");
}
void asx_export_namespace_end()
{
	auto* env = asx::environment::get();
	env->vm->end_namespace().expect("binding error");
}
void asx_export_enum(const char* name)
{
	VI_ASSERT(name != nullptr, "name should be set");
	auto* env = asx::environment::get();
	env->vm->set_enum(name).expect("binding error");
}
void asx_export_enum_value(const char* name, const char* declaration, int value)
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = enumeration(env->vm, type.get_type_info(), type.get_type_id());
	base.set_value(declaration, value).expect("binding error");
}
void asx_export_class_address(const char* name, size_t size, size_t flags)
{
	VI_ASSERT(name != nullptr, "name should be set");
	auto* env = asx::environment::get();
	auto base = env->vm->set_struct_address(name, size,
		(size_t)object_behaviours::value |
		(size_t)object_behaviours::app_class |
		(flags & (1 << 0) ? (size_t)object_behaviours::app_class_constructor : 0) |
		(flags & (1 << 0) ? (size_t)object_behaviours::app_class_destructor : 0) |
		(flags & (1 << 0) ? (size_t)object_behaviours::app_class_assignment : 0) |
		(flags & (1 << 0) ? (size_t)object_behaviours::app_class_copy_constructor : 0));
}
void asx_export_class_property_address(const char* name, const char* declaration, int property_offset)
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = type_class(env->vm, type.get_type_info(), type.get_type_id());
	base.set_property_address(declaration, property_offset).expect("binding error");
}
void asx_export_class_constructor_address(const char* name, const char* declaration, void(*constructor_function_address)(void*))
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	VI_ASSERT(constructor_function_address != nullptr, "constructor function address should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = type_class(env->vm, type.get_type_info(), type.get_type_id());
	base.set_constructor_address(declaration, bridge::function_call(constructor_function_address)).expect("binding error");
}
void asx_export_class_operator_address(const char* name, const char* declaration, void(*operator_function_address)())
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	VI_ASSERT(operator_function_address != nullptr, "operator function address should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = type_class(env->vm, type.get_type_info(), type.get_type_id());
	base.set_operator_address(declaration, bridge::function_call(operator_function_address)).expect("binding error");
}
void asx_export_class_copy_operator_address(const char* name, void(*copy_operator_function_address)())
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(copy_operator_function_address != nullptr, "copy operator function address should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = type_class(env->vm, type.get_type_info(), type.get_type_id());
	base.set_operator_copy_address(bridge::function_call(copy_operator_function_address)).expect("binding error");
}
void asx_export_class_destructor_address(const char* name, void(*destructor_function_address)(void*))
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(destructor_function_address != nullptr, "destructor function address should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = type_class(env->vm, type.get_type_info(), type.get_type_id());
	base.set_destructor_address("void f()", bridge::function_call(destructor_function_address)).expect("binding error");
}
void asx_export_class_method_address(const char* name, const char* declaration, void(*method_function_address)())
{
	VI_ASSERT(name != nullptr, "name should be set");
	VI_ASSERT(declaration != nullptr, "declaration should be set");
	VI_ASSERT(method_function_address != nullptr, "method function address should be set");
	auto* env = asx::environment::get();
	auto type = env->vm->get_type_info_by_name(name);
	auto base = type_class(env->vm, type.get_type_info(), type.get_type_id());
	base.set_method_address(declaration, bridge::function_call(method_function_address)).expect("binding error");
}
