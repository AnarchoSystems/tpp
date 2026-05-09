enum Item
{
    String(string),
    Bool(bool)
};

struct Input
{
    prefix : string;
    middle : string;
    item : Item;
}