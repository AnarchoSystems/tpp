template main(feature: Feature)
@for scenario in feature.scenarios@
TEST(@feature.name@, @scenario.name@) 
{
    @for step in scenario.steps@
    @switch step.argument@
    @case None@
    @step.name@(@for arg in step.regexArgs | sep=", "@@arg@@@end for@);
    @end case@
    @case DocString(string)@
    @step.name@(@for arg in step.regexArgs | sep=", " followedBy=", "@@arg@@@end for@ @string@);
    @end case@
    @case DataTable(Table)@
    {
        std::vector<TableRow> table = {
            @for row in step.argument.rows | sep=",\n"@
            {@for cell in row | sep=", "@@cell@@@end for@},
            @end for@
        };
        @step.name@(@for arg in step.regexArgs | sep=", " followedBy=", "@@arg@@@end for@ table);
    }
    @end case@
    @end switch@
    @end for@
}
@end for@
END