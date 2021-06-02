myid =  -1
function set_uid(x)
	myid = x
end

function player_is_near(p_id)
	p_x = API_get_x(p_id)
	p_y = API_get_y(p_id)
	my_x = API_get_x(myid)
	my_y = API_get_y(myid)
	API_print(p_x, my_x)

	if(p_x == my_x) then
		API_print(p_y, my_y)
		if(p_y == my_y) then
			API_send_mess(p_id, myid, "HELLO")
		end
	end
end