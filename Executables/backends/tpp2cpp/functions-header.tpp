template render_cpp_functions(functions: list<CppFunctionDecl>, includes: list<string>, namespaceName: optional<string>, functionPrefix: string)
#pragma once
#include <tpp/ArgType.h>
@for inc in includes@
#include "@inc@"
@end for@
#include <string>
@if namespaceName@
namespace @namespaceName@ {
@end if@
@for function in functions@
@if function.docComment@@function.docComment@@end if@std::string @functionPrefix@@function.name@(@for param in function.params | sep=", "@typename tpp::ArgType<@cpp_ir_type(param.type)@>::type @param.name@@end for@);
@end for@
@if namespaceName@
} // namespace @namespaceName@
@end if@
END
