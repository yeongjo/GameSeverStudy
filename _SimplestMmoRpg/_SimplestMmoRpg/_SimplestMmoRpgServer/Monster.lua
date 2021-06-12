myid =  -1
my_init_x = -1
my_init_y = -1
agro_distance = 0
my_hp = 0
my_level = 0
my_exp = 0
my_damage = 0
my_is_initialized = false

function SetId(x)
	myid = x
	my_init_x = LuaGetX(myid)
	my_init_y = LuaGetY(myid)
	InitStat()
end

function Reset()
	m_x = my_init_x
	m_y = my_init_y
	LuaSetPos(myid, m_x, m_y)
	InitStat()
end

function InitStat()
	agro_distance = 11
	my_hp = 2
	my_level = 1
	my_exp = my_level*my_level*2
	my_damage = 1
end

function Distance(x1, y1, x2, y2)
	x = x1 - x2
	y = y1 - y2
	return math.sqrt(x*x+y*y)
end

function Tick(actor_id_array)
	m_x = LuaGetX(myid)
	m_y = LuaGetY(myid)
	--LuaPrint("CALL tick: "..m_x..","..m_y.."\n")
	min_distance = agro_distance
	min_distance_actor_x = -1
	min_distance_actor_y = -1
	for i = 1, #actor_id_array do
		local actor_id = actor_id_array[i]
		--LuaPrint("tick loop["..i)
		--LuaPrint("]: "..actor_id.."\n")
		actor_x = LuaGetX(actor_id)
		actor_y = LuaGetY(actor_id)
		cur_distance = Distance(m_x, m_y, actor_x, actor_y)
		if(min_distance > cur_distance) then
			min_distance = cur_distance
			min_distance_actor_x = actor_x
			min_distance_actor_y = actor_y
		end
	end
	if(min_distance_actor_x == -1 or min_distance_actor_y == -1) then
		return
	end
	--LuaPrint("center: "..min_distance_actor_x..","..m_x.."/"..min_distance_actor_y..","..m_y.."\n")
	if(min_distance_actor_x - m_x > 0) then
		m_x = m_x+1
	elseif(min_distance_actor_x - m_x < 0) then
		m_x = m_x-1
	elseif(min_distance_actor_y - m_y > 0) then
		m_y = m_y+1
	elseif(min_distance_actor_y - m_y < 0) then
		m_y = m_y-1
	end
	--LuaPrint("after tick: "..m_x..","..m_y.."\n")
	LuaSetPos(myid, m_x, m_y)
end

function OnNearActorWithPlayerMove(p_id)
end

function OnNearActorWithSelfMove(p_id)
	p_x = LuaGetX(p_id)
	p_y = LuaGetY(p_id)
	my_x = LuaGetX(myid)
	my_y = LuaGetY(myid)

	if(p_x == my_x) then
		--LuaPrint(tostring(p_y)..", "..tostring(my_y).."\n")
		if(p_y == my_y) then
			LuaTakeDamage(p_id, myid)
		end
	end
end

function TakeDamage(p_id, damage)
	
	result = true
	my_hp = my_hp - damage
	if(my_hp <= 0) then
		my_hp = 0
		p_hp = LauGetHp(p_id)
		p_level = LuaGetLevel(p_id)
		p_exp = LuaGetExp(p_id)
		p_exp = p_exp + my_exp
		LuaSendStatChange(p_id, myid, p_hp, p_level, p_exp); -- °æÇèÄ¡ ½Àµæ
		result = false
	end
	LuaPrint("take_damage("..p_id..":"..damage..","..my_hp..")\n")
	LuaSendStatChange(myid, myid, my_hp, my_level, my_exp);
	return result
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