struct Input
{
    type : string;
    value : string;
}

struct Defs
{
    testName : string;
    inputs : list<Input>;
    expectedOutput : string;
    iRepJson : string;
    previewFunctionName : string;
    previewSignature : list<string>;
    hasPreviewSignature : bool;
}