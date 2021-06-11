myid =  -1
function set_uid(x)
	myid = x
end

function on_near_actor_with_player_move(p_id)
	p_x = API_get_x(p_id)
	p_y = API_get_y(p_id)
	my_x = API_get_x(myid)
	my_y = API_get_y(myid)
	--API_print(tostring(p_x)..", "..tostring(my_x).."\n")

	if(p_x == my_x) then
		--API_print(tostring(p_y)..", "..tostring(my_y).."\n")
		if(p_y == my_y) then
			API_send_mess(p_id, myid, "HELLO")
			API_add_event_npc_random_move(myid, 0000);
			API_add_event_npc_random_move(myid, 1000);
			API_add_event_npc_random_move(myid, 2000);
			API_add_event_send_mess(p_id, myid, "BYE", 2000);
		end
	end
end
function on_near_actor_with_self_move(p_id)
	
end
function get_hp()
	return my_hp
end
function get_level()
	return my_level
end
function get_exp()
	return my_exp
end
function get_damage()
	return my_damage
end