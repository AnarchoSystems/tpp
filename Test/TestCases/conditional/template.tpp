template main(user: User)
User: @user.name@
@if user.email@
Contact: @user.email@
@else@
Contact: N/A
@endif@
END