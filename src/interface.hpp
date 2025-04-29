#ifndef ASX_INTERFACE_HPP
#define ASX_INTERFACE_HPP
#ifdef _WIN32
#include <windows.h>
#define INTERFACE_OPEN() GetModuleHandle(nullptr)
#define INTERFACE_LOAD(handle, name) (void*)GetProcAddress(handle, #name)
#define INTERFACE_CLOSE(handle) (void)0
#define INTERFACE_EXPORT __declspec(dllexport)
#else
#include <dlfcn.h>
#define INTERFACE_OPEN() dlopen(nullptr, RTLD_LAZY)
#define INTERFACE_LOAD(handle, name) dlsym(handle, #name)
#define INTERFACE_CLOSE(handle) dlclose(handle);
#define INTERFACE_EXPORT
#endif
namespace
{
	void(*asx_import_builtin)(const char* path) = nullptr;
	void(*asx_import_native)(const char* path) = nullptr;
	void(*asx_export_property)(const char* declaration, void* property_address) = nullptr;
	void(*asx_export_function_address)(const char* declaration, void(*function_address)()) = nullptr;
	void(*asx_export_namespace_begin)(const char* name) = nullptr;
	void(*asx_export_namespace_end)() = nullptr;
	void(*asx_export_enum)(const char* name) = nullptr;
	void(*asx_export_enum_value)(const char* name, const char* declaration, int value) = nullptr;
	void(*asx_export_class_address)(const char* name, size_t size, size_t flags) = nullptr;
	void(*asx_export_class_property_address)(const char* name, const char* declaration, int property_offset) = nullptr;
	void(*asx_export_class_constructor_address)(const char* name, const char* declaration, void(*constructor_function_address)(void*)) = nullptr;
	void(*asx_export_class_operator_address)(const char* name, const char* declaration, void(*operator_function_address)()) = nullptr;
	void(*asx_export_class_copy_operator_address)(const char* name, void(*copy_operator_function_address)()) = nullptr;
	void(*asx_export_class_destructor_address)(const char* name, void(*destructor_function_address)(void*)) = nullptr;
	void(*asx_export_class_method_address)(const char* name, const char* declaration, void(*method_function_address)()) = nullptr;
}

template <typename t, typename r, typename... args>
static auto asx_operator(r(t::*value)(args...))
{
    return value;
}
template <typename t, typename r, typename... args>
static auto asx_operator(r(t::*value)(args...) const)
{
    return value;
}
template <typename t, typename... args>
static void asx_constructor(void* memory, args... data)
{
	new(memory) t(data...);
}
template <typename t>
static void asx_destructor(void* memory)
{
	((t*)memory)->~t();
}
template <typename t>
void asx_export_function(const char* declaration, t function)
{
	void(*function_address)() = reinterpret_cast<void(*)()>(size_t(function));
	asx_export_function_address(declaration, function_address);
}
template <typename t>
void asx_export_class(const char* name)
{
	asx_export_class_address(name, sizeof(t),
		(std::is_default_constructible<t>::value ? 1 << 0 : 0) |
		(std::is_destructible<t>::value ? 1 << 1 : 0) |
		(std::is_copy_assignable<t>::value ? 1 << 2 : 0) |
		(std::is_copy_constructible<t>::value ? 1 << 4 : 0));
}
template <typename t, typename r>
void asx_export_class_property(const char* name, const char* declaration, r t::* value)
{
	asx_export_class_property_address(name, declaration, (int)reinterpret_cast<size_t>(&(((t*)0)->*value)));
}
template <typename t, typename... args>
void asx_export_class_constructor(const char* name)
{
	void(*constructor_address)(void*) = reinterpret_cast<void(*)(void*)>(&asx_constructor<t, args...>);
	asx_export_class_constructor_address(name, "void f()", constructor_address);
}
template <typename t, typename... args>
void asx_export_class_constructor(const char* name, const char* declaration)
{
	void(*constructor_address)(void*) = reinterpret_cast<void(*)(void*)>(&asx_constructor<t, args...>);
	asx_export_class_constructor_address(name, declaration, constructor_address);
}
template <typename t, typename r, typename... args>
void asx_export_class_operator(const char* name, const char* declaration, r(t::* value)(args...))
{
    void(*operator_address)() = reinterpret_cast<void(*)()>(value);
    asx_export_class_operator_address(name, declaration, operator_address);
}
template <typename t, typename r, typename... args>
void asx_export_class_operator(const char* name, const char* declaration, r(t::* value)(args...) const)
{
    void(*operator_address)() = reinterpret_cast<void(*)()>(value);
	asx_export_class_operator_address(name, declaration, operator_address);
}
template <typename t, typename r, typename... args>
void asx_export_class_copy_operator(const char* name)
{
	void(*operator_address)() = reinterpret_cast<void(*)()>(asx_operator<t, r, args...>(&t::operator =));
	asx_export_class_copy_operator_address(name, operator_address);
}
template <typename t, typename... args>
void asx_export_class_destructor(const char* name)
{
	void(*destructor_address)(void*) = reinterpret_cast<void(*)(void*)>(&asx_destructor<t, args...>);
	asx_export_class_destructor_address(name, destructor_address);
}
template <typename t, typename r, typename... args>
void asx_export_class_method(const char* name, const char* declaration, r(t::* value)(args...))
{
	void(*method_function_address)() = reinterpret_cast<void(*)()>(value);
	asx_export_class_method_address(name, declaration, method_function_address);
}
template <typename t, typename r, typename... args>
void asx_export_class_method(const char* name, const char* declaration, r(t::* value)(args...) const)
{
	void(*method_function_address)() = reinterpret_cast<void(*)()>(value);
	asx_export_class_method_address(name, declaration, method_function_address);
}
void asx_import_interface()
{
    auto handle = INTERFACE_OPEN();
    asx_import_builtin = (decltype(asx_import_builtin))INTERFACE_LOAD(handle, asx_import_builtin);
    asx_import_native = (decltype(asx_import_native))INTERFACE_LOAD(handle, asx_import_native);
    asx_export_property = (decltype(asx_export_property))INTERFACE_LOAD(handle, asx_export_property);
    asx_export_function_address = (decltype(asx_export_function_address))INTERFACE_LOAD(handle, asx_export_function_address);
    asx_export_namespace_begin = (decltype(asx_export_namespace_begin))INTERFACE_LOAD(handle, asx_export_namespace_begin);
    asx_export_namespace_end = (decltype(asx_export_namespace_end))INTERFACE_LOAD(handle, asx_export_namespace_end);
    asx_export_enum = (decltype(asx_export_enum))INTERFACE_LOAD(handle, asx_export_enum);
    asx_export_enum_value = (decltype(asx_export_enum_value))INTERFACE_LOAD(handle, asx_export_enum_value);
    asx_export_class_address = (decltype(asx_export_class_address))INTERFACE_LOAD(handle, asx_export_class_address);
    asx_export_class_property_address = (decltype(asx_export_class_property_address))INTERFACE_LOAD(handle, asx_export_class_property_address);
    asx_export_class_constructor_address = (decltype(asx_export_class_constructor_address))INTERFACE_LOAD(handle, asx_export_class_constructor_address);
    asx_export_class_operator_address = (decltype(asx_export_class_operator_address))INTERFACE_LOAD(handle, asx_export_class_operator_address);
    asx_export_class_copy_operator_address = (decltype(asx_export_class_copy_operator_address))INTERFACE_LOAD(handle, asx_export_class_copy_operator_address);
    asx_export_class_destructor_address = (decltype(asx_export_class_destructor_address))INTERFACE_LOAD(handle, asx_export_class_destructor_address);
    asx_export_class_method_address = (decltype(asx_export_class_method_address))INTERFACE_LOAD(handle, asx_export_class_method_address);
	INTERFACE_CLOSE(handle);
}
#endif
