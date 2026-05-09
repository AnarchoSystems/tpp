template render_cpp_functions(functions: list<FunctionDef>, includes: list<string>, namespaceName: optional<string>, functionPrefix: string)
#pragma once
#include <tpp/ArgType.h>
@for inc in includes@
#include "@inc@"
@end for@
#include <string>
@if namespaceName@
namespace @namespaceName@ {
@end if@
@for function in functions | enumerator=functionIndex@
@cpp_doc_function_marker(functionIndex)@
std::string @functionPrefix@@function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@cpp_ir_type(param.type)@>::type @param.name@@end for@);
@end for@
@if namespaceName@
} // namespace @namespaceName@
@end if@
END
