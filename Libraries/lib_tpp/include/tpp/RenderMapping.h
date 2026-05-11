#pragma once

#include <tpp/Diagnostic.h>
#include <string>
#include <vector>

namespace tpp
{
    // Maps a source-location range in a template to a character span in the rendered output.
    // Populated by IR::renderTracked() for output-producing template regions,
    // including literal text, interpolations, call sites, and structural scopes.
    struct RenderMapping
    {
        std::string sourceUri; // URI of the template file that produced this output span
        Range sourceRange; // the source range in the template that produced this output span
        int outStart;      // start character offset in the rendered output string
        int outEnd;        // end character offset (exclusive) in the rendered output string
    };
}
