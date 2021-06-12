myid =  -1
function SetId(x)
	myid = x
end

function OnNearActorWithPlayerMove(p_id)
	p_x = LuaGetX(p_id)
	p_y = LuaGetY(p_id)
	my_x = LuaGetX(myid)
	my_y = LuaGetY(myid)
	--API_print(tostring(p_x)..", "..tostring(my_x).."\n")

	if(p_x == my_x) then
		--API_print(tostring(p_y)..", "..tostring(my_y).."\n")
		if(p_y == my_y) then
			LuaSendMess(p_id, myid, "HELLO")
			LuaAddEventNpcRandomMove(myid, 0000);
			LuaAddEventNpcRandomMove(myid, 1000);
			LuaAddEventNpcRandomMove(myid, 2000);
			LuaAddEventSendMess(p_id, myid, "BYE", 2000);
		end
	end
end
function OnNearActorWithSelfMove(p_id)
	
end
function GetHp()
	return my_hp
end
function GetLevel()
	return my_level
end
function GetExp()
	return my_exp
end
function GetDamage()
	return my_damage
end