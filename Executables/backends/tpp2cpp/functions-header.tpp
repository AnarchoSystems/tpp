template cpp_arg_type(t: TypeKind)
@switch t@@case Str@const std::string&@end case@@case Int@const int&@end case@@case Bool@bool@end case@@case Named(n)@const @n@&@end case@@case List(e)@const std::vector<@cpp_ir_type(e)@>&@end case@@case Optional(inner)@const std::optional<@cpp_ir_type(inner)@>&@end case@@end switch@
END

template render_cpp_functions(functions: list<CppFunctionDecl>, includes: list<string>, namespaceName: optional<string>, functionPrefix: string)
#pragma once
#include <string>
#include <vector>
#include <optional>
@for inc in includes@
#include "@inc@"
@end for@
@if namespaceName@
namespace @namespaceName@ {
@end if@
@for function in functions@
@if function.docComment@@function.docComment@@end if@std::string @functionPrefix@@function.name@(@for param in function.params | sep=", "@cpp_arg_type(param.type)@ @param.name@@end for@);
@end for@
@if namespaceName@
} // namespace @namespaceName@
@end if@
END
