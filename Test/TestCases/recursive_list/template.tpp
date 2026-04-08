template render_node(node: IntListNode)
@node.value@@if node.next@, @render_node(node.next)@@end if@
END

template main(root: IntListNode)
@render_node(root)@
END