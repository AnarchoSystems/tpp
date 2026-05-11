template main(items: list<Item>)
@for item in items@
  @render_item(item)@
@end for@
END