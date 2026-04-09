#pragma once

#include <tpp/Diagnostic.h>
#include <vector>

namespace tpp
{
    // Maps a source-location range in a template to a character span in the rendered output.
    // Populated by IR::renderTracked() for every ForNode, IfNode, and SwitchNode
    // encountered during rendering.
    struct RenderMapping
    {
        Range sourceRange; // the @for@/@if@/@switch@ directive range in the template source
        int outStart;      // start character offset in the rendered output string
        int outEnd;        // end character offset (exclusive) in the rendered output string
    };
}
