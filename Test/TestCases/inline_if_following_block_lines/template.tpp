template main(data: Data)
@data.prefix@ @if data.enabled@
alpha
beta
@else@
gamma
delta
@end if@
END