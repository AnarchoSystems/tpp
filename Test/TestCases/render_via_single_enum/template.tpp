template render_item(i: int)
int:@i@
END

template render_item()
none
END

template main(item: Item)
@render item via render_item@
END