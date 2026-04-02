struct Table
{
     name : string;
     rows : list<list<string>>;
};

enum StepArgument
{
    None, DocString(string), DataTable(Table)
};

struct Step
{
    name : string;
    regexArgs : list<string>;
    argument : StepArgument;
};

struct Scenario
{
    name : string;
    steps : list<Step>;
};

struct Feature
{
    name : string;
    scenarios : list<Scenario>;
};