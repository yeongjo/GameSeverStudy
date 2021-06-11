myid =  -1
my_init_x = -1
my_init_y = -1
agro_distance = 0
my_hp = 0
my_level = 0
my_exp = 0
my_damage = 0
my_is_initialized = false

function set_uid(x)
	myid = x
	my_init_x = API_get_x(myid)
	my_init_y = API_get_y(myid)
	reset()
end

function reset()
	m_x = my_init_x
	m_y = my_init_y
	API_set_pos(myid, m_x, m_y)
	agro_distance = 11
	my_hp = 2
	my_level = 1
	my_exp = my_level*my_level*2
	my_damage = 1
end

function distance(x1, y1, x2, y2)
	x = x1 - x2
	y = y1 - y2
	return math.sqrt(x*x+y*y)
end

function tick(actor_id_array)
	m_x = API_get_x(myid)
	m_y = API_get_y(myid)
	--API_print("CALL tick: "..m_x..","..m_y.."\n")
	min_distance = agro_distance
	min_distance_actor_x = -1
	min_distance_actor_y = -1
	for i = 1, #actor_id_array do
		local actor_id = actor_id_array[i]
		--API_print("tick loop["..i)
		--API_print("]: "..actor_id.."\n")
		actor_x = API_get_x(actor_id)
		actor_y = API_get_y(actor_id)
		cur_distance = distance(m_x, m_y, actor_x, actor_y)
		if(min_distance > cur_distance) then
			min_distance = cur_distance
			min_distance_actor_x = actor_x
			min_distance_actor_y = actor_y
		end
	end
	if(min_distance_actor_x == -1 or min_distance_actor_y == -1) then
		return
	end
	--API_print("center: "..min_distance_actor_x..","..m_x.."/"..min_distance_actor_y..","..m_y.."\n")
	if(min_distance_actor_x - m_x > 0) then
		m_x = m_x+1
	elseif(min_distance_actor_x - m_x < 0) then
		m_x = m_x-1
	elseif(min_distance_actor_y - m_y > 0) then
		m_y = m_y+1
	elseif(min_distance_actor_y - m_y < 0) then
		m_y = m_y-1
	end
	--API_print("after tick: "..m_x..","..m_y.."\n")
	API_set_pos(myid, m_x, m_y)
end

function on_near_actor_with_player_move(p_id)
end

function on_near_actor_with_self_move(p_id)
	p_x = API_get_x(p_id)
	p_y = API_get_y(p_id)
	my_x = API_get_x(myid)
	my_y = API_get_y(myid)

	if(p_x == my_x) then
		--API_print(tostring(p_y)..", "..tostring(my_y).."\n")
		if(p_y == my_y) then
			API_take_damage(p_id, myid)
		end
	end
end

function take_damage(p_id, damage)
	
	result = true
	my_hp = my_hp - damage
	if(my_hp <= 0) then
		my_hp = 0
		p_hp = API_get_hp(p_id)
		p_level = API_get_level(p_id)
		p_exp = API_get_exp(p_id)
		p_exp = p_exp + my_exp
		API_send_stat_change(p_id, p_hp, p_level, p_exp); -- °æÇèÄ¡ ½Àµæ
		result = false
	end
	API_print("take_damage("..p_id..":"..damage..","..my_hp..")\n")
	API_send_stat_change(myid, my_hp, my_level, my_exp);
	return result
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