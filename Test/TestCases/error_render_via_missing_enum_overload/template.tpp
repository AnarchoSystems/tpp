template render_item(i: int)
int:@i@
END
template main(items: list<Item>)
@render items via render_item@
END